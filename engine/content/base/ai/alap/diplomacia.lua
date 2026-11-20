-- SPDX-License-Identifier: GPL-3.0-or-later
-- Diplomácia-menedzser (KIS-1): kész Diplomácia csarnokával szövetség egy kisnéppel – a nép `ally_pref` listájából
-- 80%-ban az első kettő közül (a gép saját, determinisztikus véletlenjével) –, utána a kisnép terve a
-- feladatlistákba. A csapatjátékos a kisnép seregéből többet képez.
local K = require("kozos")
local A = {}

local function has_diplomacy(v)
  for _, b in ipairs(v.buildings) do
    if b.complete and K.tmpl(b.kind).diplomacy then return true end
  end
  return false
end

function A.run()
  local v = K.view()
  local plan = K.plan()
  if v.me.ally == "" then
    if not has_diplomacy(v) then return end
    local pref = plan.ally_pref or {}
    if #pref == 0 then return end
    if not S.ally_pick then
      local i = (math.random(100) <= 80) and math.random(math.min(2, #pref)) or math.random(#pref)
      S.ally_pick = pref[i]
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
