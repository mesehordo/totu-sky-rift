-- Kincsvadászat (mintamod, M13): aki elsőként tart CEL aranyat a kincstárában, nyer.
-- A szkript a játék minden gépén ugyanúgy fut (determinisztikus): csak a Game és a Victory táblát használja,
-- a megmaradó adat az S táblában van (mentés, visszajátszás).

local CEL = 2000

function on_start()
  S.cel = CEL
  Victory.conquest(true)   -- a hódítás szabálya is él (alapból is be van kapcsolva)
end

local function allas()
  local best, best_p = -1, 0
  for p = 1, 8 do
    local pl = Game.player(p)
    if pl.active and not pl.defeated and pl.gold > best then
      best, best_p = pl.gold, p
    end
  end
  return best, best_p
end

function on_tick(tick)
  if tick % 30 ~= 0 or Game.winner() ~= 0 then return end   -- másodpercenként elég
  local best, best_p = allas()
  if best_p == 0 then return end
  if best >= S.cel then
    Victory.win(best_p)
    Victory.status(string.format("Kincsvadászat: a(z) %d. játékos nyert", best_p))
  else
    Victory.status(string.format("Kincs: %d / %d (vezet: %d.)", best, S.cel, best_p))
  end
end
