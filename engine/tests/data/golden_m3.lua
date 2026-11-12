-- SPDX-License-Identifier: GPL-3.0-or-later
-- M3 golden küldetés: védekezés hullámok ellen. Szándékosan sok mindent megmozgat (időzítők, halál-esemény,
-- véletlen, rendezés, bejárás, lebegőpontos számolás, szövegformázás, korutin), hogy a visszajátszás minden
-- rendszeren ugyanazt a hasht adja, ha a szkriptréteg determinisztikus.

local JATEKOS, TAMADO = 1, 2

local function korulotte(n, cx, cy, sugar)
  -- n pont egy kör mentén, gyökvonással és hatványozással (mindkettő determinisztikus)
  local pontok = {}
  for i = 1, n do
    local dx = (i % 5) - 2
    local dy = math.sqrt(math.max(0, sugar ^ 2 - dx ^ 2)) * ((i % 2 == 0) and 1 or -1)
    pontok[i] = {x = cx + dx, y = cy + dy}
  end
  return pontok
end

-- generátor korutinnal: a hullámok összetétele
local function osszetetel(hullam)
  return coroutine.wrap(function()
    for i = 1, 3 + hullam * 2 do
      local tipus = (i % 3 == 0) and "ijasz" or "harcos"
      coroutine.yield(tipus, i)
    end
  end)
end

local TIPUSOK = {
  harcos = {hp = 90, speed = 0.07, damage = 11, spread = 5, cooldown = 28, range = 1.25},
  ijasz = {hp = 60, speed = 0.06, damage = 8, spread = 3, cooldown = 40, range = 4.5},
}

function on_start()
  S.hullam = 0
  S.halottak = {[JATEKOS] = 0, [TAMADO] = 0}
  S.statisztika = {}
  S.vedok = {}
  for i, p in ipairs(korulotte(8, 40, 40, 3.5)) do
    S.vedok[i] = Game.spawn{owner = JATEKOS, x = p.x, y = p.y, hp = 160, speed = 0.05, damage = 14, spread = 4,
                            cooldown = 30, range = 3.75}
  end
  Game.after(60, "uj_hullam", {erosites = 1})
  log(string.format("indul: %d védő, %.2f átlagos távolság", #S.vedok, 3.5))
end

function uj_hullam(adat, idozito)
  S.hullam = S.hullam + 1
  local db = 0
  for tipus, i in osszetetel(S.hullam) do
    local t = TIPUSOK[tipus]
    local oldal = math.random(1, 4)
    local x = (oldal == 1) and 2 or (oldal == 2) and 78 or math.random(2, 78) + math.random()
    local y = (oldal == 3) and 2 or (oldal == 4) and 78 or math.random(2, 78) + math.random()
    local id = Game.spawn{owner = TAMADO, x = x, y = y, hp = t.hp + adat.erosites * 5, speed = t.speed,
                          damage = t.damage, spread = t.spread, cooldown = t.cooldown, range = t.range}
    Game.move(id, 40 + (i % 3) - 1, 40 + (i % 2))
    db = db + 1
  end
  S.statisztika[#S.statisztika + 1] = {hullam = S.hullam, db = db, tick = Game.tick(), idozito = idozito}
  if S.hullam < 8 then
    Game.after(300 + math.random(0, 120), "uj_hullam", {erosites = adat.erosites + 1})
  end
end

function on_death(id, owner)
  S.halottak[owner] = S.halottak[owner] + 1
  if owner == JATEKOS and Game.count(JATEKOS) <= 3 and not S.erosites_jott then
    -- utolsó erőfeszítés: erősítés érkezik
    S.erosites_jott = Game.tick()
    for i = 1, 4 do
      local v = Game.spawn{owner = JATEKOS, x = 40 + i - 2.5, y = 44, hp = 200, damage = 16, range = 3.75}
      S.vedok[#S.vedok + 1] = v
    end
  end
end

function on_tick(t)
  -- a védők időnként visszaállnak a helyükre; a túlélők rendezett listája
  if t % 150 == 75 then
    local elok = Game.units(JATEKOS)
    local adatok = {}
    for _, id in ipairs(elok) do
      local u = Game.unit(id)
      adatok[#adatok + 1] = {id = id, hp = u.hp, tav = (u.x - 40) ^ 2 + (u.y - 40) ^ 2}
    end
    table.sort(adatok, function(a, b)
      if a.hp ~= b.hp then return a.hp < b.hp end
      return a.id < b.id
    end)
    if #adatok > 0 then
      Game.move(adatok[1].id, 40, 40)   -- a legsebesültebb a középre húzódik
    end
  end
  -- nagy rendezés és szöveges kulcsok bejárása (determinisztikus sorrend), néhány tickenként
  if t % 600 == 0 then
    local szamok = {}
    for i = 1, 300 do szamok[i] = math.random(1, 100000) end
    table.sort(szamok)
    local cimkek = {}
    for k, v in pairs({gyalog = 1, lovas = 2, ijasz = 3, ostromgep = 4, hos = 5}) do
      cimkek[#cimkek + 1] = k .. "=" .. v
    end
    S.minta = string.format("%d|%d|%s|%d", szamok[1], szamok[300], table.concat(cimkek, ","), utf8.len("Wullof-ősök"))
  end
  if t == 3599 then
    local o = {}
    for owner, n in pairs(S.halottak) do o[#o + 1] = owner .. ":" .. n end
    S.eredmeny = string.format("hullám %d, halottak %s, élő védő %d, támadó %d, átlag %.3f", S.hullam,
                               table.concat(o, " "), Game.count(JATEKOS), Game.count(TAMADO),
                               S.halottak[TAMADO] / math.max(1, S.hullam))
    log(S.eredmeny)
    log(S.minta)
  end
end
