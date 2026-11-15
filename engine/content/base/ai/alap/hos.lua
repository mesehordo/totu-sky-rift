-- SPDX-License-Identifier: GPL-3.0-or-later
-- Hős-menedzser (a régi `manage_heroes`): az oltár hősei egyenként, ha van már sereg; a hős a képességpontjait
-- elkölti. A hőshasználat a személyiség szerint (a 30% alatti nem képez hőst).
local K = require("kozos")
local O = {}

function O.run()
  local v = K.view()
  local P, plan = K.pers(), K.plan()
  S.hero_wait = false
  if P.hero >= 30 and plan.heroes and #v.army >= (P.hero >= 100 and 2 or 3) then
    for _, h in ipairs(plan.heroes) do
      if (v.count[h] or 0) == 0 then
        if not AI.train(h) then S.hero_wait = true end
        break
      end
    end
  end
  for _, u in ipairs(v.army) do
    if u.hero then
      for _, a in ipairs(K.tmpl(u.kind).abilities) do
        if AI.learn(u.id, a.id) then break end
      end
    end
  end
end

return O
