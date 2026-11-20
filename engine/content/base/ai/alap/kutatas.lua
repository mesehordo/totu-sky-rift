-- SPDX-License-Identifier: GPL-3.0-or-later
-- Kutatás-menedzser: a nép kutatási sorrendje (a kutatási arány szerinti gyakorisággal), felemelkedés után a kedvelt
-- fejlődési irány, és az irány épületei, serege a feladatlistákba (a régi `on_path`).
local K = require("kozos")
local R = {}

function R.init()
  S.research = {}
  for _, r in ipairs(K.plan().research_order or {}) do S.research[#S.research + 1] = r end
end

function R.run()
  local v = K.view()
  local plan = K.plan()
  if K.gate("research", 30) then
    for k = 1, math.min(3, #S.research) do
      if AI.research(S.research[k]) then
        table.remove(S.research, k)
        break
      end
    end
  end
  if v.me.ascended and v.me.path == "" and plan.path_pref and plan.path_pref[1] then
    local P = K.pers()
    -- a technológus és az ostromló az utolsó (rendszerint a nehézgépes) irányt, a többiek az elsőt kedvelik
    local pick = plan.path_pref[1]
    if (S.pers == "technologus" or S.pers == "ostromlo") and #plan.path_pref > 1 then pick = plan.path_pref[#plan.path_pref] end
    local forced = AI.forced().path   -- a beállításban előre megadott irány (a régi `forced_path`)
    if forced ~= "" then pick = forced end
    if AI.choose_path(pick) then
      local pp = plan.paths and plan.paths[pick]
      if pp then
        for _, b in ipairs(pp.build or {}) do S.build[#S.build + 1] = b end
        for _, a in ipairs(pp.army or {}) do S.army[#S.army + 1] = {a[1], a[2] * (P.hero >= 100 and 2 or 1)} end
      end
    end
  end
end

return R
