-- SPDX-License-Identifier: GPL-3.0-or-later
-- Védelem-menedzser (a régi `manage_army` 1. pontja): a bázis közelében látott ellenség ellen a sereg (támadás
-- közben csak, ha a fenyegetés nagy), kis betörés ellen a munkások, veszélyben a helyőrség (Védekező); a támadások
-- ideje és iránya a tanulásnak.
local K = require("kozos")
local V = {}

local RADIUS2 = 14 * 14

function V.run()
  local v = K.view()
  local P = K.pers()
  local t = v.tick
  local threats = {}
  for _, e in ipairs(K.enemies()) do
    if not e.building and (e.owner ~= 0 or e.moving) then
      for _, b in ipairs(v.buildings) do
        if K.d2(e.x, e.y, b.x, b.y) < RADIUS2 then
          threats[#threats + 1] = e
          break
        end
      end
    end
  end
  if #threats == 0 then
    if S.garrisoned and t - (S.threat_t or 0) > 10 * 30 then
      for _, b in ipairs(v.buildings) do AI.unload(b.id) end
      S.garrisoned = false
    end
    return
  end
  -- a támadás feljegyzése (tanulás: ideje és iránya)
  local tx, ty = K.center(threats)
  S.learn = S.learn or {shift = {}}
  if t - (S.threat_t or -100000) > 60 * 30 then
    S.learn.attacks = (S.learn.attacks or 0) + 1
    local n = S.learn.attacks
    S.learn.att_x = ((S.learn.att_x or tx) * (n - 1) + tx) / n
    S.learn.att_y = ((S.learn.att_y or ty) * (n - 1) + ty) / n
    S.learn.att_tick = t
  end
  S.threat_t = t
  local tval, my_value = 0, 0
  for _, e in ipairs(threats) do
    if not e.worker then tval = tval + K.value(e) end
  end
  for _, u in ipairs(v.army) do my_value = my_value + K.value(u) end
  local recall = S.state ~= "attack" or tval > 0.35 * my_value
  local ids, near_army = {}, 0
  for _, u in ipairs(v.army) do
    if recall or K.d2(u.x, u.y, tx, ty) < 900 then ids[#ids + 1] = u.id end
    if K.d2(u.x, u.y, tx, ty) < 144 then near_army = near_army + 1 end
  end
  if #ids > 0 then AI.attack(ids, tx, ty) end
  if recall and S.state == "attack" then S.state, S.state_t = "build", t end
  if recall then S.raid_ids = {} end
  -- veszélyben a munkások be a főépületbe (Védekező), kis betörés ellen fegyvert fognak
  if P.garrison > 0 and tval > my_value + 100 and v.hq and v.hq.complete and K.tmpl(v.hq.kind).garrison > 0 then
    local near = {}
    for _, w in ipairs(v.workers) do
      if K.d2(w.x, w.y, v.hq.x, v.hq.y) < 144 then near[#near + 1] = w.id end
    end
    if #near > 0 and AI.garrison(near, v.hq.id) then S.garrisoned = true end
  elseif near_army == 0 and tval > 0 and tval <= 250 then
    local fighters = {}
    for _, w in ipairs(v.workers) do
      if K.d2(w.x, w.y, tx, ty) < 81 then fighters[#fighters + 1] = w.id end
    end
    if #fighters > 0 then AI.attack(fighters, tx, ty) end
  end
end

return V
