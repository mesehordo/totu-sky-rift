-- SPDX-License-Identifier: GPL-3.0-or-later
-- Építés-menedzser: feladatlista (létszám, a nép építési sorrendje, védőépületek, falak, telepek) – egyszerre
-- legfeljebb a nehézség `builds` számú épület; ami nem építhető (feltétel, hely, ár), átlépi.
local K = require("kozos")
local E = {}

function E.init()
  local plan = K.plan()
  S.build = {}
  for _, b in ipairs(plan.build_order or {}) do S.build[#S.build + 1] = b end
  -- a nép épületei szerepük szerint (a sablonokból)
  S.towers_k, S.walls_k, S.dropoff_k = {}, {}, {}
  for _, b in ipairs(AI.roster().buildings) do
    local t = K.tmpl(b)
    if t.tower and b ~= plan.hq then S.towers_k[#S.towers_k + 1] = b end
    if t.wall then S.walls_k[#S.walls_k + 1] = b end
    if t.dropoff and b ~= plan.hq and t.supply_provides == 0 then S.dropoff_k[#S.dropoff_k + 1] = b end
  end
  S.expansions = 0
  S.walls_built = 0
end

local function count_where(v, pred)
  local n = 0
  for _, b in ipairs(v.buildings) do
    if pred(b) then n = n + 1 end
  end
  return n
end

-- terjeszkedés: a legközelebbi felderített aranylelőhely, amely messze van a saját és az ismert ellenséges épületektől
local function expansion_site(v)
  local hx, hy = S.home[1], S.home[2]
  local best, bd
  for _, n in ipairs(AI.resources("gold")) do
    local ok = true
    for _, b in ipairs(v.buildings) do
      if b.dropoff and K.d2(b.x, b.y, n.x, n.y) < 100 then ok = false break end
    end
    for _, s in ipairs(S.exp_sites) do
      if s.node == n.id then ok = false break end
    end
    if ok then
      for _, e in pairs(S.intel and S.intel.b or {}) do
        if K.d2(e.x, e.y, n.x, n.y) < 196 then ok = false break end
      end
    end
    if ok then
      local d = K.d2(n.x, n.y, hx, hy)
      if not bd or d < bd then best, bd = n, d end
    end
  end
  return best
end

local function expansion_kind()
  local plan = K.plan()
  for _, b in ipairs(S.dropoff_k) do   -- előbb az aranyat fogadó raktár
    if K.tmpl(b).dropoff_mask & 2 ~= 0 then return b end
  end
  return S.dropoff_k[1] or plan.hq
end

function E.run()
  local v = K.view()
  if not v.hq then return end
  local P, D, plan = K.pers(), K.diff(), K.plan()
  S.saving = false
  if v.constructing >= D.builds then return end
  local hx, hy = math.floor(v.hq.x), math.floor(v.hq.y)
  local wants = {}
  -- 1. létszám (ha nem épül éppen; a létszámegységes népnél csak, ha az egység nem képezhető)
  local reserve = 3 + v.me.supply_used // 12
  if v.free < reserve and v.me.supply_cap < 200 and plan.supply_building and (v.incomplete[plan.supply_building] or 0) == 0
     and (not plan.supply_unit or S.supply_full) then
    wants[#wants + 1] = {plan.supply_building, hx + 3, hy + 3}
  end
  -- 2. a nép építési sorrendjének hiányzó elemei
  local need = {}
  for _, b in ipairs(S.build) do
    need[b] = (need[b] or 0) + 1
    if (v.count[b] or 0) < need[b] and need[b] == (v.count[b] or 0) + 1 then wants[#wants + 1] = {b, hx + 3, hy + 3} end
  end
  local core = #wants
  -- 3. védelem: tornyok és falak az ellenség felé (a védelmi arány szerinti gyakorisággal)
  if K.gate("defense", 40) then
    local learned = (S.learn and S.learn.towers or 0)
    local towers = count_where(v, function(b) return b.tower end)
    if towers < P.towers + learned and #S.towers_k > 0 then
      local tx, ty = K.toward_enemy(6)
      if S.learn and S.learn.att_x and learned > 0 then   -- a tanult támadási irányba
        local dx, dy = S.learn.att_x - hx, S.learn.att_y - hy
        local len = math.max(1, math.sqrt(dx * dx + dy * dy))
        tx, ty = math.floor(hx + dx * 6 / len), math.floor(hy + dy * 6 / len)
      end
      for _, k in ipairs(S.towers_k) do wants[#wants + 1] = {k, tx, ty} end
    end
    if S.walls_built < P.walls and #S.walls_k > 0 then
      local wx, wy = K.toward_enemy(8)
      local off = (S.walls_built % 2 == 0) and S.walls_built or -S.walls_built
      wants[#wants + 1] = {S.walls_k[1], wx + off, wy - off, wall = true}
    end
  end
  -- 4. terjeszkedés (telepek a távoli lelőhelyeknél)
  if S.expansions < P.expand and v.tick >= P.expand_time * 30 and K.gate("economy", 60) then
    local site = expansion_site(v)
    if site then
      wants[#wants + 1] = {expansion_kind(), math.floor(site.x), math.floor(site.y), site = site}
    end
  end
  S.saving = core > 0
  for i, w in ipairs(wants) do
    if AI.build(w[1], w[2], w[3]) then
      if i <= core then S.saving = false end
      if w.wall then S.walls_built = S.walls_built + 1 end
      if w.site then
        S.expansions = S.expansions + 1
        S.exp_sites[#S.exp_sites + 1] = {node = w.site.id, x = w.site.x, y = w.site.y, ready = true, sent = 0}
      end
      break
    end
    if DEBUG_AI then log("nem épít", w[1]) end
  end
end

return E
