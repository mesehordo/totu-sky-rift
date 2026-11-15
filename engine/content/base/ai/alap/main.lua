-- SPDX-License-Identifier: GPL-3.0-or-later
-- Az alap gépi ellenfél belépési pontja (M9; GEPI_ELLENFEL.md 1–2. és 5. pont; a régi `rts/ai.py` mintájára).
--
-- Menedzserek (a 0 A.D. Petra felépítése szerint): gazdaság, építés, hadsereg, védelem, felderítés, kutatás, hős,
-- diplomácia/szövetség és tanulás. Ütemezett, eltolt futás: minden menedzsernek saját periódusa van (a nehézség
-- `think` értékének többszöröse), a kezdő fázis a menedzser és a játékos sorszámából eltolva; egy tickben legfeljebb
-- a nehézség `tasks` számú menedzsere fut, a többi a következő tickre marad (feladatlista).
local K = require("kozos")

local MANAGERS = {
  {"vedelem", require("vedelem"), 1},
  {"gazdasag", require("gazdasag"), 1},
  {"epites", require("epites"), 1},
  {"hadsereg", require("hadsereg"), 1},
  {"felderites", require("felderites"), 2},
  {"kutatas", require("kutatas"), 2},
  {"hos", require("hos"), 2},
  {"diplomacia", require("diplomacia"), 4},
  {"tanulas", require("tanulas"), 4},
}

function on_start()
  local me = AI.player()
  K.resolve_personality(me.faction)
  K.init_geometry()
  for _, m in ipairs(MANAGERS) do
    if m[2].init then m[2].init() end
  end
  local D = K.diff()
  S.sched = {}
  local p = AI.me()
  for i, m in ipairs(MANAGERS) do
    S.sched[m[1]] = AI.tick() + 1 + (i * 7 + p * 5) % D.think
  end
end

function on_tick(t)
  if S.done or not S.sched then return end
  local D = K.diff()
  local budget = D.tasks
  for _, m in ipairs(MANAGERS) do
    if budget <= 0 then break end
    local due = S.sched[m[1]]
    if due and due <= t then
      S.sched[m[1]] = t + m[3] * D.think
      budget = budget - 1
      if K.view().me.defeated then
        S.done = true
        return
      end
      m[2].run()
    end
  end
end
