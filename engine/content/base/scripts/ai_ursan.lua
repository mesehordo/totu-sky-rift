-- SPDX-License-Identifier: GPL-3.0-or-later
-- Szkriptelt ursan ellenfél (M6, ADR-007 9. lépés; D37): munkások, építési sorrend, kiképzés, kutatás, fejlődési irány
-- és támadóhullámok – a nép `ai` adatai szerint. A valódi gépi ellenfél az M9-ben. Az AI_PLAYERS a csatolás előtt
-- állítható (alapérték: a 2. játékos); az AI_ALLY[p] előírja a kisnépet (egyébként a nép `ai.ally_pref` listájából).
AI_PLAYERS = AI_PLAYERS or {2}
AI_ALLY = AI_ALLY or {}

local BUILD = {"urs_hosszuhaz", "urs_harcosok_haza", "urs_tanya", "urs_hosszuhaz", "urs_eskufa", "urs_harcosok_haza",
               "urs_runakovacs", "urs_ortorony", "urs_tanya", "urs_tanya", "urs_diplomacia"}
local ARMY = {"urs_pajzshordozo", "urs_szigonyvezo", "urs_pajzshordozo", "urs_dardaor", "urs_szigonyvezo", "urs_runapap",
              "urs_pajzshordozo", "urs_dardaor", "urs_szigonyvezo"}
local RESEARCH = {"urs_runapengek", "urs_medvebunda", "urs_vasszigony", "urs_telegi_edzes", "urs_runapajzs",
                  "urs_kiados_tanya", "urs_gyors_helyorseg"}
local WORKERS = 14
local GATHER = {"wood", "gold", "wood", "gold", "wood", "meteor", "wood", "gold"}   -- ~ fa 0,5, arany 0,375, meteorit 0,125
local THINK = 30                -- tick (1 s)
local FIRST_WAVE = 4 * 60 * 30  -- az első hullám legkorábban 4 perc után

function on_start()
  for _, p in ipairs(AI_PLAYERS) do
    S[p] = {army_i = 1, research_i = 1, wave_size = 6, next_wave = Game.tick() + FIRST_WAVE, assigned = {}, n = 0,
            build = {table.unpack(BUILD)}, army = {table.unpack(ARMY)}, ally_on = false}
  end
end

local function inventory(p)
  local inv = {count = {}, workers = {}, army = {}, building = 0, hq = nil}
  for _, id in ipairs(Game.units(p)) do
    local kind, complete = Game.kind(id)
    if kind then
      inv.count[kind] = (inv.count[kind] or 0) + 1
      if kind == "urs_telepes" then
        inv.workers[#inv.workers + 1] = id
      elseif kind == "urs_nagyterem" then
        inv.hq = Game.unit(id)
      elseif complete == false then
        inv.building = inv.building + 1
      elseif not string.find(kind, "^urs_hos") and Game.unit(id) and not Game.unit(id).moving then
        inv.army[#inv.army + 1] = id
      end
    end
  end
  return inv
end

-- az ellenség célpontja: a főépülete, ha nincs, bármely épülete vagy egysége
local function enemy_target(p)
  for q = 1, 8 do
    if q ~= p and Game.count(q) > 0 then
      local fallback = nil
      for _, id in ipairs(Game.units(q)) do
        local kind = Game.kind(id)
        local u = Game.unit(id)
        if kind == "urs_nagyterem" and u then return u.x, u.y end
        if u and not fallback then fallback = u end
      end
      if fallback then return fallback.x, fallback.y end
    end
  end
  return nil
end

local function think(p, tick)
  local me = Game.player(p)
  if me.defeated then return end
  local st = S[p]
  local inv = inventory(p)
  local free = me.supply_cap - me.supply_used
  if inv.hq and #inv.workers < WORKERS and free > 0 then Game.train(p, "urs_telepes") end
  -- az új munkások a nyersanyagarány szerint (egyszer kiosztva; az építők utána visszamennek)
  for _, w in ipairs(inv.workers) do
    if not st.assigned[w] and Game.unit(w) and not Game.unit(w).moving then
      st.n = st.n + 1
      if Game.gather(w, GATHER[(st.n - 1) % #GATHER + 1]) then st.assigned[w] = true end
    end
  end
  -- építés: egyszerre egy; a létszám előbb
  if inv.building == 0 and inv.hq then
    local hx, hy = math.floor(inv.hq.x), math.floor(inv.hq.y)
    local want
    if free < 3 and me.supply_cap < 200 then want = "urs_tanya" end
    if not want then
      local need = {}
      for _, b in ipairs(st.build) do
        need[b] = (need[b] or 0) + 1
        if (inv.count[b] or 0) < need[b] then want = b break end
      end
    end
    if want then Game.build(p, want, hx + 3, hy + 3) end
  end
  -- kiképzés és kutatás
  if (inv.count["urs_harcosok_haza"] or 0) > 0 and free >= 2 then
    if Game.train(p, st.army[st.army_i]) then st.army_i = st.army_i % #st.army + 1 end
  end
  if st.research_i <= #RESEARCH and Game.research(p, RESEARCH[st.research_i]) then st.research_i = st.research_i + 1 end
  if me.ascended and me.path == "" then Game.choose_path(p, "runa") end
  -- szövetség (KIS-1): kész Diplomácia csarnokával; utána a kisnép épületei és egységei a tervbe
  if me.ally == "" and (inv.count["urs_diplomacia"] or 0) > 0 then Game.ai_ally(p, AI_ALLY[p]) end
  if me.ally ~= "" and not st.ally_on then
    st.ally_on = true
    local plan = Game.faction_ai(me.ally)
    if plan then
      for _, b in ipairs(plan.build) do st.build[#st.build + 1] = b end
      for _, u in ipairs(plan.army) do
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
