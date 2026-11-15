-- SPDX-License-Identifier: GPL-3.0-or-later
-- Tanulás a meccsen belül (GEPI_ELLENFEL.md 5. pont): a felderített ellenséges összetétel alapján az egységmix
-- súlyai ellencsapat felé tolódnak (korlátok között: a személyiség `learn_max`-a), az ismétlődő támadások után
-- több torony a támadás irányába; az Alkalmazkodó a felderítés alapján stílust is vált. Kezdő és Könnyű nem tanul.
-- Az állapot az S.learn-ben (a szimuláció része); a meccs végén elveszik.
local K = require("kozos")
local T = {}

-- ellencsapat: ami ellen → mi (súly)
local COUNTER = {
  lovas = {landzsas = 3},
  lovesz = {lovas = 2, repulo = 1},
  kozelharcos = {lovesz = 2, orias = 1},
  landzsas = {lovesz = 2, varazslo = 1},
  orias = {landzsas = 1, ostrom = 1, varazslo = 1},
  repulo = {lovesz = 3},
  varazslo = {lovas = 2, kozelharcos = 1},
  ostrom = {lovas = 2, kozelharcos = 1},
}

function T.init()
  S.learn = S.learn or {shift = {}}
end

function T.run()
  local D, P = K.diff(), K.pers()
  local rate = D.learn * P.learn // 100
  if rate <= 0 then return end
  local comp, total = {}, 0
  for _, u in pairs(S.intel.u) do
    if not u.worker and u.class then
      comp[u.class] = (comp[u.class] or 0) + 1
      total = total + 1
    end
  end
  if total >= 3 then
    local target = {}
    for cls, n in pairs(comp) do
      for c, w in pairs(COUNTER[cls] or {}) do
        target[c] = (target[c] or 0) + n * w * 40 // total
      end
    end
    local maxs = P.learn_max
    for _, c in ipairs(K.CLASSES) do
      local tg = math.min(maxs, target[c] or 0)
      local cur = S.learn.shift[c] or 0
      local step = (tg - cur) * rate // 400
      if step == 0 and tg ~= cur then step = tg > cur and 1 or -1 end
      S.learn.shift[c] = math.max(-40, math.min(maxs, cur + step))
    end
  end
  -- ismétlődő támadások: egy-egy további torony a támadás irányába (legfeljebb kettő)
  S.learn.towers = math.min(2, math.max(0, (S.learn.attacks or 0) - 1))
  -- alkalmazkodó: stílusváltás a felderítés alapján (ellenstratégia)
  if P.adaptive > 0 then
    local v = K.view()
    local mine = 0
    for _, u in ipairs(v.army) do mine = mine + K.value(u) end
    local towers = 0
    for _, b in pairs(S.intel.b) do
      if b.tower then towers = towers + 1 end
    end
    local style = "kiegyenlitett"
    if towers >= 2 then
      style = "ostromlo"
    elseif S.intel.value > mine * 1.3 + 200 then
      style = "vedekezo"
    elseif v.tick > 240 * 30 and S.intel.value < mine * 0.6 then
      style = "tamado"
    end
    S.adapt = style
  end
end

return T
