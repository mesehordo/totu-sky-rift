-- SPDX-License-Identifier: GPL-3.0-or-later
-- Felderítés-menedzser: a látott ellenség emlékezete (a régi `update_intel`: az épületek, amíg a helyük látszik és
-- ott vannak; az egységek 90 másodpercig) és a felderítő útja a még ismeretlen kezdőhelyekre. Csak a látott
-- információ (AI.enemies) kerül ide – a köd mögötti állapot nem (AI-FOG = 0).
local K = require("kozos")
local F = {}

function F.init()
  S.intel = {b = {}, u = {}, value = 0}
end

local function update_intel(t)
  local seen = {}
  for _, e in ipairs(K.enemies()) do
    seen[e.id] = true
    if e.building then
      S.intel.b[e.id] = {x = e.x, y = e.y, owner = e.owner, kind = e.kind, tower = e.tower and e.complete,
                         v = K.value(e)}
    elseif e.owner ~= 0 then
      S.intel.u[e.id] = {x = e.x, y = e.y, owner = e.owner, class = e.class, worker = e.worker, v = K.value(e), t = t}
    end
  end
  for id, b in pairs(S.intel.b) do   -- az épület eltűnt a látott helyéről
    if not seen[id] and AI.visible(math.floor(b.x), math.floor(b.y)) then S.intel.b[id] = nil end
  end
  local value = 0
  for id, u in pairs(S.intel.u) do
    if t - u.t > 90 * 30 or (not seen[id] and AI.visible(math.floor(u.x), math.floor(u.y))) then
      S.intel.u[id] = nil
    elseif not u.worker then
      value = value + u.v
    end
  end
  for _, b in pairs(S.intel.b) do
    if b.tower then value = value + b.v * 0.9 end
  end
  S.intel.value = value
end

local function scout(v, t)
  local D = K.diff()
  if t < D.scout * 30 then return end
  local u = S.scout_id and v.by_id[S.scout_id]
  if S.scout_id and not u then S.scout_id = nil end   -- elesett
  if not S.scout_id then
    if (S.next_scout or 0) > t then return end
    -- gyors harcos, aztán bármely harcos (a hős nem); a munkás csak később (az építés elvonhatja)
    local pick
    for _, a in ipairs(v.army) do
      if (a.class == "lovas" or a.class == "repulo") and not pick then pick = a end
    end
    for _, a in ipairs(v.army) do
      if not a.hero and not pick then pick = a end
    end
    if not pick and t >= D.scout * 60 then
      for _, w in ipairs(v.workers) do
        if w.task ~= 4 and w.task ~= 5 then pick = w break end
      end
    end
    if not pick then return end
    S.scout_id, S.scout_i, S.scout_at = pick.id, 1, nil
    u = pick
  end
  -- az útvonal: előbb a bázis körüli kör (lelőhelyek a terjeszkedéshez), aztán a még nem felderített kezdőhelyek
  if not S.route then
    S.route = {}
    local hx, hy = S.home[1], S.home[2]
    local DIRS = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}}
    for _, d in ipairs(DIRS) do
      local r = (d[1] ~= 0 and d[2] ~= 0) and 13 or 18
      local x = math.max(1, math.min(S.map_w - 2, hx + d[1] * r))
      local y = math.max(1, math.min(S.map_h - 2, hy + d[2] * r))
      S.route[#S.route + 1] = {x, y}
    end
    for _, st in ipairs(S.starts) do S.route[#S.route + 1] = {st[1], st[2]} end
  end
  while S.scout_i <= #S.route and AI.explored(S.route[S.scout_i][1], S.route[S.scout_i][2]) do
    S.scout_i = S.scout_i + 1
  end
  if S.scout_i > #S.route then
    AI.move({S.scout_id}, S.home[1] + 0.5, S.home[2] + 0.5)
    if u.worker then AI.gather(S.scout_id, "wood") end
    S.scout_id = nil
    S.next_scout = t + 180 * 30
    S.scouted = true
    S.route = nil
    return
  end
  if not u.moving then
    if S.scout_at == S.scout_i then   -- odaküldve, mégsem ért oda: elérhetetlen, a következő
      S.scout_try = (S.scout_try or 0) + 1
      if S.scout_try >= 2 then
        S.scout_i, S.scout_try = S.scout_i + 1, 0
        return
      end
    else
      S.scout_at, S.scout_try = S.scout_i, 0
    end
    local st = S.route[S.scout_i]
    AI.move({S.scout_id}, st[1] + 0.5, st[2] + 0.5)
  end
end

function F.run()
  local v = K.view()
  update_intel(v.tick)
  scout(v, v.tick)
end

return F
