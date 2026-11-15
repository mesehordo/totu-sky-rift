-- SPDX-License-Identifier: GPL-3.0-or-later
-- Gazdaság-menedzser: munkásképzés (a nép terve × személyiség × nehézség, telepenként több), a tétlen munkások
-- a nyersanyagarány szerinti hiányhoz (a régi `manage_workers`), a telepekre küldött munkások, létszámegység.
local K = require("kozos")
local G = {}

function G.init()
  local plan = K.plan()
  local gather = {}
  for _, g in ipairs(plan.gather or {}) do
    if g[2] > 0 then gather[#gather + 1] = {g[1], g[2]} end
  end
  if #gather == 0 then gather = {{"wood", 55}, {"gold", 32}, {"meteor", 13}} end
  S.gather = gather
  S.exp_sites = S.exp_sites or {}
end

local function worker_target(plan)
  local P, D = K.pers(), K.diff()
  local base = plan.workers or 14
  return (base * P.workers * D.workers) // 10000 + #(S.exp_sites or {}) * 3
end

function G.run()
  local v = K.view()
  local plan = K.plan()
  if v.hq and plan.worker and #v.workers < worker_target(plan) and v.free > 0 then AI.train(plan.worker) end
  local have = {}
  for _, w in ipairs(v.workers) do
    if w.gathering then have[w.gathering] = (have[w.gathering] or 0) + 1 end
  end
  local total = math.max(1, #v.workers)
  S.nogather = S.nogather or {}
  for _, w in ipairs(v.idle_workers) do
    -- amely munkásfajta lelőhelyről nem gyűjt (pl. a Croftian szolgái), azt percenként egyszer próbálja újra
    local ng = S.nogather[w.kind]
    local sent = ng ~= nil and ng > v.tick
    -- előbb a telepek (a terjeszkedés lelőhelyei), telepenként néhány munkás
    for _, site in ipairs(S.exp_sites) do
      if site.ready and (site.sent or 0) < 4 then
        if AI.gather_at(w.id, site.node) then
          site.sent = (site.sent or 0) + 1
          sent = true
        else
          site.ready = false   -- kimerült vagy elveszett
        end
        break
      end
    end
    if not sent then
      -- a legnagyobb hiányú nyersanyag (az arány szerint); ami nem gyűjthető, a következő
      local order = {}
      for i, g in ipairs(S.gather) do
        order[i] = {g[1], total * g[2] / 100 - (have[g[1]] or 0)}
      end
      table.sort(order, function(a, b) if a[2] ~= b[2] then return a[2] > b[2] end return a[1] < b[1] end)
      local any = false
      for _, o in ipairs(order) do
        if AI.gather(w.id, o[1]) then
          have[o[1]] = (have[o[1]] or 0) + 1
          any = true
          break
        end
        if DEBUG_AI then log("nem gyűjt", w.kind, o[1]) end
      end
      S.nogather[w.kind] = (not any) and v.tick + 60 * 30 or nil
    end
  end
  -- létszámegység (a régi `manage_supply`: emu, szemhordozó)
  if plan.supply_unit and v.free <= 6 and v.me.supply_cap < 200 and (S.supply_t or 0) <= v.tick then
    if AI.train(plan.supply_unit) then
      S.supply_t = v.tick + 10 * 30
      S.supply_full = false
    else
      S.supply_full = true
    end
  end
end

return G
