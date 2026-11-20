-- SPDX-License-Identifier: GPL-3.0-or-later
-- Diplomácia-menedzser (KIS-1): kész Diplomácia csarnokával szövetség egy kisnéppel – a nép `ally_pref` listájából az
-- a kisnép, amelyik a seregből hiányzó szerepet (gyógyító, lövő, ostrom, légvédelem) tölti be (a tanulmány szerint; a
-- preferencia csak döntetlennél dönt) –, utána a kisnép terve a feladatlistákba. A csapatjátékos a kisnép seregéből
-- többet képez.
local K = require("kozos")
local A = {}

local function has_diplomacy(v)
  for _, b in ipairs(v.buildings) do
    if b.complete and K.tmpl(b.kind).diplomacy then return true end
  end
  return false
end

-- a kisnép szerepei a seregéből (az épületei által képzett egységek és maguk az épületek)
local function roles_of(fid)
  local roles = {}
  local function look(id)
    local t = AI.template(id)
    if not t then return end
    if t.heals then roles.heal = true end
    if t.ranged and not t.building then roles.ranged = true end
    if t.class == "ostrom" then roles.siege = true end
    if t.anti_air then roles.antiair = true end
    if t.tower then roles.defense = true end
    if t.flying then roles.air = true end
    for _, p in ipairs(t.produces or {}) do look(p) end
  end
  local r = AI.roster(fid)
  for _, id in ipairs(r.units or {}) do look(id) end
  for _, id in ipairs(r.buildings or {}) do look(id) end
  return roles
end

-- a tanulmány szerint: a kisnép egy stratégiai rést töltsön be, ne véletlenül (a régi: sorsolás a preferenciából)
-- – a saját seregből hiányzó szerep és a felderített ellenség szerint pontoz; döntetlennél a népi preferencia dönt
function A.pick_by_need(v, pref)
  local have = {heal = false, ranged = 0, siege = false, antiair = false, total = 0}
  for _, u in ipairs(v.army) do
    local t = AI.template(u.kind)
    if t then
      have.total = have.total + 1
      if t.heals then have.heal = true end
      if t.ranged then have.ranged = have.ranged + 1 end
      if t.class == "ostrom" then have.siege = true end
      if t.anti_air then have.antiair = true end
    end
  end
  local towers, flyers = 0, 0
  for _, b in pairs(S.intel and S.intel.b or {}) do if b.tower then towers = towers + 1 end end
  for _, u in pairs(S.intel and S.intel.u or {}) do if u.class == "repulo" then flyers = flyers + 1 end end
  local need = {
    heal = have.heal and 0 or 3,
    ranged = (have.total > 0 and have.ranged * 10 < have.total * 3) and 2 or 0,   -- kevesebb mint 30% lövő
    siege = (towers >= 2 and not have.siege) and 3 or 0,
    antiair = (flyers >= 1 and not have.antiair) and 3 or 0,
    defense = 0,
    air = 0,
  }
  if S.pers == "ostromlo" then need.siege = need.siege + 2 end
  if S.pers == "vedekezo" then need.defense = need.defense + 2 end
  local best, best_score = pref[1], -1
  for i, fid in ipairs(pref) do
    local roles, score = roles_of(fid), (#pref - i) * 0.5   -- a preferencia csak döntetlenhez
    for role, w in pairs(need) do if roles[role] then score = score + w end end
    if score > best_score then best, best_score = fid, score end
  end
  return best
end

function A.run()
  local v = K.view()
  local plan = K.plan()
  if v.me.ally == "" then
    if not has_diplomacy(v) then return end
    local pref = plan.ally_pref or {}
    if #pref == 0 then return end
    if not S.ally_pick then
      S.ally_pick = A.pick_by_need(v, pref)
      local forced = AI.forced().ally   -- a beállításban előre megadott kisnép (a régi `forced_ally`)
      if forced ~= "" then S.ally_pick = forced end
    end
    AI.choose_ally(S.ally_pick)
    return
  end
  if not S.ally_on then
    S.ally_on = true
    local P = K.pers()
    local ap = AI.plan(v.me.ally)
    if ap then
      for _, b in ipairs(ap.build or {}) do S.build[#S.build + 1] = b end
      for _, u in ipairs(ap.army or {}) do
        S.army[#S.army + 1] = {u[1], math.min(u[2], 2) * (P.assist > 0 and 2 or 1)}
      end
    end
  end
end

return A
