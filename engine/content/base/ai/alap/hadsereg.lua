-- SPDX-License-Identifier: GPL-3.0-or-later
-- Hadsereg-menedzser: kiképzés az egységmix-súlyok szerint (a nép hadrendje × személyiség × tanult eltolás),
-- a régi `manage_army` állapotgépe (gyülekezés → támadás → visszavonulás), portyázás, mikro és képességhasználat.
local K = require("kozos")
local H = {}

function H.init()
  local plan, P = K.plan(), K.pers()
  S.army = {}
  for _, a in ipairs(plan.army or {}) do S.army[#S.army + 1] = {a[1], a[2]} end
  S.trained = {}
  S.state, S.state_t = "build", AI.tick()
  S.wave = math.max(4, ((plan.attack_supply or 16) * P.wave) // 300)
  S.raid_ids = {}
end

-- a következő képzendő egység: a súlyához képest a legkevesebbszer képzett
local function train(v, P)
  local cands = {}
  for _, a in ipairs(S.army) do
    local t = K.tmpl(a[1])
    local w = a[2] * (P.mix[t.class] or 100) * (100 + K.shift(t.class)) // 100
    if w > 0 then cands[#cands + 1] = {a[1], ((S.trained[a[1]] or 0) * 10000) // w} end
  end
  table.sort(cands, function(a, b) if a[2] ~= b[2] then return a[2] < b[2] end return a[1] < b[1] end)
  -- ami nem képezhető (nincs hozzá épület), átlépi; ha csak a nyersanyag hiányzik, arra gyűjt
  for i = 1, #cands do
    local ok, why = AI.train(cands[i][1])
    if ok then
      S.trained[cands[i][1]] = (S.trained[cands[i][1]] or 0) + 1
      return true
    end
    if DEBUG_AI then log("nem képez", cands[i][1], why) end
    if why == "nincs elég nyersanyag" then return false end
  end
  return false
end

local function main_army(v)
  local out = {}
  for _, u in ipairs(v.army) do
    if not S.raid_ids[u.id] and u.id ~= S.scout_id then out[#out + 1] = u end
  end
  return out
end

local function set_state(s, t)
  S.state, S.state_t = s, t
end

-- portyázás (a régi gerillaharc): a gyors egységekből kis csapat a látott munkásokra, távoli raktárakra
local function raid(v, army, P, D, t)
  if not D.raid or P.raid <= 0 or t < P.raid_time * 30 then return end
  local group = {}
  for _, u in ipairs(v.army) do
    if S.raid_ids[u.id] then group[#group + 1] = u end
  end
  if #group == 0 then
    S.raid_ids = {}
    if (S.raid_next or 0) > t or #army < P.raid_size + 2 then return end
    if (t // 30) % 100 >= P.raid then   -- a hajlam szerinti gyakoriság
      S.raid_next = t + 60 * 30
      return
    end
    local tx, ty
    for _, u in pairs(S.intel.u) do
      if u.worker and u.owner ~= 0 then tx, ty = u.x, u.y break end
    end
    if not tx then
      for _, b in pairs(S.intel.b) do
        if b.owner ~= 0 and not b.tower then tx, ty = b.x, b.y break end
      end
    end
    if not tx then return end
    table.sort(army, function(a, b)
      local fa = (a.class == "lovas" or a.class == "repulo") and 0 or 1
      local fb = (b.class == "lovas" or b.class == "repulo") and 0 or 1
      if fa ~= fb then return fa < fb end
      return a.id < b.id
    end)
    local ids = {}
    for i = 1, P.raid_size do
      ids[i] = army[i].id
      S.raid_ids[army[i].id] = true
    end
    AI.attack(ids, tx, ty)
    S.raid_next = t + 90 * 30
    S.raids = (S.raids or 0) + 1
    return
  end
  local cx, cy = K.center(group)
  if K.enemy_strength(cx, cy, 10) > K.own_strength(group, cx, cy, 10) * 1.2 + 50 then   -- kerüli a nyílt csatát
    AI.move(K.ids(group), S.home[1] + 0.5, S.home[2] + 0.5)
    S.raid_ids = {}
  end
end

local function use_abilities(army, P, D)
  if not D.abilities or P.ability <= 0 then return end
  local n = 0
  for _, u in ipairs(army) do
    if n >= D.tasks then return end
    local t = K.tmpl(u.kind)
    if #t.abilities > 0 and (u.hero or (u.id + AI.tick() // 30) % 100 < P.ability) then
      local best, bd
      for _, e in ipairs(K.enemies()) do
        if e.owner ~= 0 and not e.building then
          local d = K.d2(e.x, e.y, u.x, u.y)
          if d < 64 and (not bd or d < bd) then best, bd = e, d end
        end
      end
      if best then
        for _, a in ipairs(t.abilities) do
          if a.type ~= "other" then
            n = n + 1
            if AI.ability({u.id}, a.id, best.x, best.y, best.id) then break end
          end
        end
      end
    end
  end
end

function H.run()
  local v = K.view()
  local P, D, plan = K.pers(), K.diff(), K.plan()
  local t = v.tick
  -- kiképzés (a hadsereg aránya szerint; az épülő létszám vagy a gyűjtés idején minden második alkalommal szünetel)
  local hold = (S.saving or S.hero_wait) and (t // D.think) % 2 == 0
  if #S.army > 0 and v.free >= 2 and not hold and K.gate("army", 30) then train(v, P) end
  local army = main_army(v)
  raid(v, army, P, D, t)
  if S.threat_t and t - S.threat_t < 10 * 30 and S.state ~= "attack" then return end   -- a védelem vezényel
  local rx, ry = K.rally()
  local value, supply = 0, 0
  for _, u in ipairs(army) do
    value = value + K.value(u)
    supply = supply + (u.supply or 1)
  end
  if S.state == "build" then
    local idle = {}
    for _, u in ipairs(army) do
      if not u.moving and K.d2(u.x, u.y, rx, ry) > 49 then idle[#idle + 1] = u.id end
    end
    if #idle > 0 then AI.attack(idle, rx + 0.5, ry + 0.5) end
    local est = math.max(S.intel and S.intel.value or 0, 110 * t / 1800 - 150)
    local adv = P.advantage
    if P.night > 0 and AI.night() >= 600 then adv = adv * (100 - P.night // 2) // 100 end   -- éjjel bátrabb
    if P.weather > 0 and AI.weather() ~= "" then adv = adv * (100 - P.weather // 3) // 100 end
    local strong = value * 100 >= adv * est + adv * 150 / 100 * 100 or v.me.supply_used >= 190
    local first = P.first_attack * 30 * D.attack_delay // 100
    if t >= first and supply >= S.wave and strong then
      set_state("gather", t)
      AI.attack(K.ids(army), rx + 0.5, ry + 0.5)
    end
  elseif S.state == "gather" then
    local near = 0
    for _, u in ipairs(army) do
      if K.d2(u.x, u.y, rx, ry) < 81 then near = near + 1 end
    end
    if near * 4 >= #army * 3 or t - S.state_t > 25 * 30 then
      set_state("attack", t)
      S.attack_t = 0
      S.wave = math.min(100, S.wave + P.wave_growth * 2)
      if not S.first_attack then
        S.first_attack = t
        log("elso_tamadas", t)
      end
      S.attacks = (S.attacks or 0) + 1
    end
  elseif S.state == "attack" then
    if #army < 3 then
      AI.move(K.ids(army), rx + 0.5, ry + 0.5)
      set_state("retreat", t)
      return
    end
    local cx, cy = K.center(army)
    local mine = K.own_strength(army, cx, cy, 12)
    local theirs = K.enemy_strength(cx, cy, 14)
    if theirs * 100 > mine * P.retreat + 10000 then
      AI.move(K.ids(army), rx + 0.5, ry + 0.5)
      set_state("retreat", t)
      return
    end
    -- mikro: a sebesültek vissza (Normáltól), fókuszlövés (Nehéztől)
    local front = {}
    for _, u in ipairs(army) do
      if D.micro >= 1 and not u.hero and u.hp * 100 < u.max * 30 then
        AI.move({u.id}, rx + 0.5, ry + 0.5)
      else
        front[#front + 1] = u
      end
    end
    if D.micro >= 2 then
      local weak, wh
      for _, e in ipairs(K.enemies()) do
        if e.owner ~= 0 and not e.building and K.d2(e.x, e.y, cx, cy) < 64 and (not wh or e.hp < wh) then weak, wh = e, e.hp end
      end
      if weak then
        local shooters = {}
        for _, u in ipairs(front) do
          if u.ranged then shooters[#shooters + 1] = u.id end
        end
        if #shooters > 0 then AI.target(shooters, weak.id) end
      end
    end
    use_abilities(front, P, D)
    if t >= (S.attack_t or 0) then
      S.attack_t = t + 5 * 30
      local tx, ty = K.attack_target(cx, cy)
      if not tx then
        tx, ty = S.map_w / 2, S.map_h / 2
      end
      local far, go = {}, {}
      for _, u in ipairs(front) do
        if K.d2(u.x, u.y, cx, cy) > 196 then far[#far + 1] = u.id elseif not u.moving then go[#go + 1] = u.id end
      end
      if #far > 0 then AI.attack(far, cx, cy) end
      if #go > 0 then AI.attack(go, tx, ty) end
      -- a kezdőhely kitisztázva: felderítve, és nincs ott ismert ellenséges épület
      for _, st in ipairs(S.starts) do
        if not st.cleared and AI.visible(st[1], st[2]) then
          local any = false
          for _, b in pairs(S.intel.b) do
            if K.d2(b.x, b.y, st[1], st[2]) < 100 then any = true break end
          end
          if not any then st.cleared = true end
        end
      end
    end
  elseif S.state == "retreat" then
    if t - S.state_t > 12 * 30 then set_state("build", t) end
  end
end

return H
