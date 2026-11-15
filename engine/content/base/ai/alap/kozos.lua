-- SPDX-License-Identifier: GPL-3.0-or-later
-- Közös segédek az alap gépi ellenfélnek (M9): beállítások (személyiség, nehézség), a tick nézete a saját
-- egységekről, sablon- és értékbecslés, hely- és célpontsegédek.
--
-- A megmaradó állapot mind az S táblában van (mentés, hash, visszajátszás); a modul helyi gyorsítótárai csak
-- a szimuláció adatából újraszámolhatók (betöltés után üresen indulnak, és ugyanazt adják).
local K = {}

-- a hiányzó személyiség-mezők alapértéke
local DEFAULTS = {
  ratio = {economy = 35, army = 35, defense = 12, research = 18},
  workers = 100, first_attack = 420, advantage = 130, wave = 100, wave_growth = 2,
  mix = {}, retreat = 160, raid = 0, raid_time = 240, raid_size = 3, expand = 0, expand_time = 480,
  towers = 0, walls = 0, garrison = 0, hero = 50, ability = 50, night = 0, weather = 0,
  learn = 100, learn_max = 80, adaptive = 0, assist = 0,
}
K.CLASSES = {"kozelharcos", "landzsas", "lovesz", "lovas", "orias", "varazslo", "repulo", "ostrom"}

function K.copy(t)
  local ty = type(t)
  if ty ~= "table" and ty ~= "userdata" then return t end
  local o = {}
  for k, v in pairs(t) do o[k] = K.copy(v) end
  return o
end

local function merge(base, over)
  local o = K.copy(base)
  for k, v0 in pairs(over or {}) do
    local v = K.copy(v0)
    if type(v) == "table" and type(o[k]) == "table" then
      for kk, vv in pairs(v) do o[k][kk] = vv end
    else
      o[k] = v
    end
  end
  return o
end

-- a nehézség paraméterei
local dcache
function K.diff()
  if not dcache then dcache = merge({think = 30, tasks = 5, builds = 2, abilities = true, micro = 1, raid = true,
                                     learn = 40, attack_delay = 100, workers = 100, scout = 90},
                                    DIFFICULTIES and DIFFICULTIES[AI.difficulty()] or {}) end
  return dcache
end

-- a személyiség feloldása a meccs elején (a „népe szerinti” a nép alapján)
function K.resolve_personality(faction)
  local id = AI.personality()
  local p = PERSONALITIES and PERSONALITIES[id]
  S.pers, S.pers_over = id, nil
  if p and p.base then
    local mapped = p.factions and p.factions[faction]
    S.pers = mapped or p.base
    S.pers_over = p.overrides and K.copy(p.overrides[faction]) or nil
  end
end

-- a személyiség paraméterei (az alkalmazkodó a választott stílusét veszi, a saját tanulási súlyaival)
local pcache, pkey
function K.pers()
  local id = S.pers or AI.personality()
  local key = id .. "/" .. (S.adapt or "")
  if pkey ~= key then
    local own = merge(DEFAULTS, PERSONALITIES and PERSONALITIES[id] or {})
    if S.pers_over then own = merge(own, S.pers_over) end
    if S.adapt and S.adapt ~= id then
      local st = merge(DEFAULTS, PERSONALITIES[S.adapt] or {})
      st.adaptive, st.learn, st.learn_max = own.adaptive, own.learn, own.learn_max
      own = st
    end
    pcache, pkey = own, key
  end
  return pcache
end

-- a tanult eltolás egy egységosztály súlyán (%)
function K.shift(class)
  return S.learn and S.learn.shift and S.learn.shift[class] or 0
end

-- arányos kapu: a kategória az aránya szerinti gyakorisággal léphet (pl. védelem 10% → minden 5. alkalommal)
function K.gate(cat, threshold)
  S.acc = S.acc or {}
  local r = K.pers().ratio[cat] or 0
  S.acc[cat] = (S.acc[cat] or 0) + r
  if S.acc[cat] >= threshold then
    S.acc[cat] = S.acc[cat] - threshold
    return true
  end
  return false
end

local plan_c
function K.plan()
  if not plan_c then plan_c = AI.plan() or {} end
  return plan_c
end

local tmpl_c = {}
function K.tmpl(id)
  local t = tmpl_c[id]
  if not t then
    t = AI.template(id)
    tmpl_c[id] = t
  end
  return t
end

-- egy egység vagy épület becsült értéke (a régi `value`: költség, a sérüléssel arányosan kevesebb)
function K.value(u)
  if not u.kind then return 50 end
  local t = K.tmpl(u.kind)
  local c = t.cost
  local v = (c.wood + c.gold + c.meteor * 1.5) / math.max(1, t.count)
  local frac = u.hp / math.max(1, u.max)
  return v * (0.4 + 0.6 * frac)
end

function K.d2(ax, ay, bx, by)
  local dx, dy = ax - bx, ay - by
  return dx * dx + dy * dy
end

-- a tick nézete a saját egységekről (tickenként egyszer)
local view
function K.view()
  local t = AI.tick()
  if view and view.tick == t then return view end
  local plan = K.plan()
  local v = {tick = t, me = AI.player(), workers = {}, idle_workers = {}, army = {}, buildings = {}, count = {},
             incomplete = {}, constructing = 0, hq = nil, by_id = {}}
  for _, u in ipairs(AI.my()) do
    v.by_id[u.id] = u
    if u.kind then v.count[u.kind] = (v.count[u.kind] or 0) + 1 end
    if u.building then
      v.buildings[#v.buildings + 1] = u
      if not u.complete then
        v.constructing = v.constructing + 1
        v.incomplete[u.kind] = (v.incomplete[u.kind] or 0) + 1
      end
      if u.kind == plan.hq and (not v.hq or (u.complete and not v.hq.complete)) then v.hq = u end
    elseif u.worker then
      v.workers[#v.workers + 1] = u
      if u.idle and not u.moving and not u.sheltered then v.idle_workers[#v.idle_workers + 1] = u end
    elseif u.kind and (u.attack or u.hero) and not u.sheltered then
      v.army[#v.army + 1] = u
    end
  end
  if not v.hq then v.hq = v.buildings[1] end
  v.free = v.me.supply_cap - v.me.supply_used
  view = v
  return v
end

local enemies_c
function K.enemies()
  local t = AI.tick()
  if not enemies_c or enemies_c.tick ~= t then enemies_c = {tick = t, list = AI.enemies()} end
  return enemies_c.list
end

-- a bázis és az ellenség iránya: a saját kezdőhely (a főépülethez legközelebbi) és a többi kezdőhely
function K.init_geometry()
  local v = K.view()
  local m = AI.map()
  S.map_w, S.map_h = m.w or 64, m.h or 64
  local hx, hy = v.hq and v.hq.x or S.map_w / 2, v.hq and v.hq.y or S.map_h / 2
  S.home = {math.floor(hx), math.floor(hy)}
  S.starts = {}
  local best, bd
  for i, st in ipairs(m.starts or {}) do
    local d = K.d2(st[1], st[2], hx, hy)
    if not bd or d < bd then best, bd = i, d end
  end
  for i, st in ipairs(m.starts or {}) do
    if i ~= best then S.starts[#S.starts + 1] = {st[1], st[2], cleared = false} end
  end
end

-- a legközelebbi még nem kitisztázott ellenséges kezdőhely (vagy nil)
function K.enemy_start()
  local hx, hy = S.home[1], S.home[2]
  local best, bd
  for _, st in ipairs(S.starts) do
    if not st.cleared then
      local d = K.d2(st[1], st[2], hx, hy)
      if not bd or d < bd then best, bd = st, d end
    end
  end
  return best
end

-- az ellenség felé mutató pont a bázistól `dist` csempére
function K.toward_enemy(dist)
  local hx, hy = S.home[1], S.home[2]
  local st = K.enemy_start() or S.starts[1]
  if not st then return hx, hy end
  local dx, dy = st[1] - hx, st[2] - hy
  local len = math.max(1, math.sqrt(dx * dx + dy * dy))
  return math.floor(hx + dx * dist / len + 0.5), math.floor(hy + dy * dist / len + 0.5)
end

function K.rally()
  return K.toward_enemy(5)
end

-- a támadás célpontja: a legközelebbi ismert ellenséges épület, aztán egység, aztán a kitisztázatlan kezdőhely
function K.attack_target(cx, cy)
  local best, bd
  for _, b in pairs(S.intel and S.intel.b or {}) do
    if b.owner ~= 0 then
      local d = K.d2(b.x, b.y, cx, cy)
      if not bd or d < bd then best, bd = b, d end
    end
  end
  if best then return best.x, best.y end
  for _, u in pairs(S.intel and S.intel.u or {}) do
    if u.owner ~= 0 then
      local d = K.d2(u.x, u.y, cx, cy)
      if not bd or d < bd then best, bd = u, d end
    end
  end
  if best then return best.x, best.y end
  local st = K.enemy_start()
  if st then return st[1] + 0.5, st[2] + 0.5 end
  return nil
end

function K.ids(list)
  local o = {}
  for i, u in ipairs(list) do o[i] = u.id end
  return o
end

function K.center(list)
  local x, y = 0, 0
  for _, u in ipairs(list) do x, y = x + u.x, y + u.y end
  local n = math.max(1, #list)
  return x / n, y / n
end

-- a látott ellenséges erő egy kör belsejében (a tornyok másfélszeresen)
function K.enemy_strength(x, y, r)
  local r2, tot = r * r, 0
  for _, e in ipairs(K.enemies()) do
    if e.owner ~= 0 or e.attack then
      if K.d2(e.x, e.y, x, y) <= r2 and not e.worker then
        if e.building then
          if e.tower and e.complete then tot = tot + K.value(e) * 1.5 end
        else
          tot = tot + K.value(e)
        end
      end
    end
  end
  return tot
end

function K.own_strength(list, x, y, r)
  local r2, tot = r * r, 0
  for _, u in ipairs(list) do
    if K.d2(u.x, u.y, x, y) <= r2 then tot = tot + K.value(u) end
  end
  return tot
end

return K
