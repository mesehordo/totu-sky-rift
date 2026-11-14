-- SPDX-License-Identifier: GPL-3.0-or-later
-- Adatvezérelt gépi ellenfél bármely főnépnek (M7, ADR-008; D37): a nép `Faction.ai` terve szerint – munkások a
-- nyersanyagarány szerint, építési sorrend (egyszerre egy, előbb a létszám), kiképzés a hadrend szerint, kutatási
-- sorrend, felemelkedés után a kedvelt irány, Diplomácia csarnokával szövetség (és a kisnép terve), támadóhullámok.
-- A valódi gépi ellenfél az M9-ben. Az AI_PLAYERS és az AI_ALLY a csatolás előtt állítható.
AI_PLAYERS = AI_PLAYERS or {2}
AI_ALLY = AI_ALLY or {}

local THINK = 30                -- tick (1 s)
local FIRST_WAVE = 4 * 60 * 30  -- az első hullám legkorábban 4 perc után

local function expand(army)
  local out = {}
  for _, u in ipairs(army) do
    for _ = 1, u[2] do out[#out + 1] = u[1] end
  end
  return out
end

function on_start()
  for _, p in ipairs(AI_PLAYERS) do
    local plan = Game.faction_ai(Game.player(p).faction) or {}
    local gather = {}   -- 8 hely a nyersanyagarány szerint (a legnagyobb maradék elve nélkül, egyszerűen)
    for _, g in ipairs(plan.gather or {}) do
      for _ = 1, math.max(1, math.floor(g[2] * 8 / 100 + 0.5)) do gather[#gather + 1] = g[1] end
    end
    if #gather == 0 then gather = {"wood", "gold", "wood", "meteor"} end
    local build = {}
    for _, b in ipairs(plan.build_order or {}) do build[#build + 1] = b end
    S[p] = {plan = plan, build = build, army = expand(plan.army or {}), gather = gather, army_i = 1, research_i = 1,
            wave_size = math.max(4, math.floor((plan.attack_supply or 16) / 3)), next_wave = Game.tick() + FIRST_WAVE,
            assigned = {}, n = 0, ally_on = false}
  end
end

local function inventory(p, st)
  local inv = {count = {}, workers = {}, army = {}, building = 0, hq = nil}
  for _, id in ipairs(Game.units(p)) do
    local kind, complete = Game.kind(id)
    if kind then
      inv.count[kind] = (inv.count[kind] or 0) + 1
      local u = Game.unit(id)
      if kind == st.plan.worker then
        inv.workers[#inv.workers + 1] = id
      elseif kind == st.plan.hq then
        inv.hq = u
      elseif complete == false then
        inv.building = inv.building + 1
      elseif u and not u.moving and not u.building then
        inv.army[#inv.army + 1] = id
      end
    end
  end
  return inv
end

-- az ellenség célpontja: bármely épülete vagy egysége (az első)
local function enemy_target(p)
  for q = 1, 8 do
    if q ~= p and Game.count(q) > 0 then
      for _, id in ipairs(Game.units(q)) do
        local u = Game.unit(id)
        if u then return u.x, u.y end
      end
    end
  end
  return nil
end

local function think(p, tick)
  local me = Game.player(p)
  if me.defeated then return end
  local st = S[p]
  local plan = st.plan
  local inv = inventory(p, st)
  local free = me.supply_cap - me.supply_used
  if inv.hq and plan.worker and #inv.workers < (plan.workers or 14) and free > 0 then Game.train(p, plan.worker) end
  for _, w in ipairs(inv.workers) do   -- a tétlen munkás (az új és az építését befejezett) gyűjteni megy
    local u = Game.unit(w)
    if u and u.idle and not u.moving then
      st.n = st.n + 1
      local res = st.gather[(st.n - 1) % #st.gather + 1]
      if not Game.gather(w, res) and DEBUG_AI then log(me.faction .. " nem gyűjt: " .. res) end
    end
  end
  -- építés: egyszerre egy; a létszám előbb (ha nem sikerült, a kör gyűjt: a sereg vár)
  local saving = false
  if inv.building == 0 and inv.hq then
    local hx, hy = math.floor(inv.hq.x), math.floor(inv.hq.y)
    local want
    -- létszám, de nem kétszer egymás után (különben a folytonos kiképzés mellett csak létszámépület épülne)
    if free < 3 and me.supply_cap < 200 and plan.supply_building and not st.last_supply then want = plan.supply_building end
    local wants = {}
    if want then wants[1] = want end
    local need = {}
    for _, b in ipairs(st.build) do   -- a sorrend hiányzó elemei; ami nem építhető (feltétel, hely), átlépi
      need[b] = (need[b] or 0) + 1
      if (inv.count[b] or 0) < need[b] and need[b] == (inv.count[b] or 0) + 1 then wants[#wants + 1] = b end
    end
    saving = #wants > 0
    for _, w in ipairs(wants) do
      if Game.build(p, w, hx + 3, hy + 3) then
        st.last_supply = w == plan.supply_building and free < 3
        saving = false
        break
      elseif DEBUG_AI then
        log(me.faction .. " nem épít: " .. w .. " fa " .. me.wood .. " kő " .. me.stone .. " met " .. me.meteor .. " munkás " .. #inv.workers)
      end
    end
  end
  -- kutatás: a sorrend következő három eleméből az első indítható
  local order = plan.research_order or {}
  local busy = false
  for k = st.research_i, math.min(#order, st.research_i + 2) do
    local ok, why = Game.research(p, order[k])
    if not ok and DEBUG_AI then log(me.faction .. " nem kutat: " .. order[k] .. " – " .. tostring(why)) end
    if ok then
      table.remove(order, k)
      break
    end
    if why == "nincs megfelelő épület" then busy = true end
  end
  -- ha a kutató épület a kiképzéssel foglalt, minden második körben a kiképzés szünetel (felszabadul a kutatásnak)
  local hold = (busy and (tick // THINK) % 2 == 0) or saving
  -- kiképzés: a hadrend körben (ami nem megy, átlépi)
  if #st.army > 0 and free >= 2 and not hold then
    for _ = 1, #st.army do
      local ok = Game.train(p, st.army[st.army_i])
      st.army_i = st.army_i % #st.army + 1
      if ok then break end
    end
  end
  if me.ascended and me.path == "" and plan.path_pref and plan.path_pref[1] then Game.choose_path(p, plan.path_pref[1]) end
  -- szövetség (KIS-1)
  if me.ally == "" then Game.ai_ally(p, AI_ALLY[p]) end
  if me.ally ~= "" and not st.ally_on then
    st.ally_on = true
    local ap = Game.faction_ai(me.ally)
    if ap then
      for _, b in ipairs(ap.build) do st.build[#st.build + 1] = b end
      for _, u in ipairs(ap.army) do
        for _ = 1, math.min(u[2], 2) do st.army[#st.army + 1] = u[1] end
      end
    end
  end
  -- támadóhullám
  if tick >= st.next_wave and #inv.army >= st.wave_size then
    local x, y = enemy_target(p)
    if x then
      Game.attack(inv.army, x, y)
      st.wave_size = st.wave_size + 2
      st.next_wave = tick + 90 * 30
    end
  end
end

function on_tick(tick)
  if tick % THINK ~= 0 then return end
  for _, p in ipairs(AI_PLAYERS) do think(p, tick) end
end
