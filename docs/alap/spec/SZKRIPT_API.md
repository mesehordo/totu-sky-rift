# Szkriptréteg (M3) – Lua-felület küldetésekhez és modokhoz

> Állapot: az M3 első változata (2026-11-12). Az M3-as Game API a kis M2-es szimulációra épül (egység: hely,
> mozgás, életerő, támadás); a teljes felület (épületek, erőforrások, régiók, célok, párbeszéd, kamera) az M6–M11-ben
> bővül, a [küldetések terve](../KULDETESEK_TERV.md) és a [FORRASOK_ES_TANULSAGOK](../FORRASOK_ES_TANULSAGOK.md)
> 2.4–2.7 pontja szerint.
> Kód: `engine/src/script/host.*`, Lua 5.4.8: `engine/third_party/lua` (MIT, változatlan).

## 1. Alapelvek

- **Determinizmus:** a szkript a szimuláció része, ezért minden gépen bitre ugyanazt kell tennie. Ezt a motor
  kényszeríti ki, a szkriptírónak nem kell rá figyelnie. Részletek: 3. pont.
- **Elszigetelés:** minden mod saját Lua-állapotot kap, a modok nem látják egymás változóit. Az adat csak olvasható.
- **Homokozó:** a szkript nem fér a fájlrendszerhez, a hálózathoz, az operációs rendszerhez és a bájtkódhoz.
- **Korlátok:** tickenként utasításkeret (alapból 5 millió), modonként memóriakorlát (alapból 64 MiB). A túllépés
  hiba: a szkript abban a tickben leáll, a következő tickben újra fut. A játék nem fagy le.
- **Hiba:** a szkript hibája nem állítja meg a játékot. A hiba a naplóba kerül, nyomkövetéssel.

## 2. Elérhető globálisok

| Név | Mi ez |
|---|---|
| `assert error ipairs pairs select tonumber tostring type rawequal rawlen rawget rawset setmetatable getmetatable pcall xpcall _G _VERSION` | alapfüggvények (a `pairs`, a `tostring` és a `setmetatable` determinisztikus változat, lásd a 3. pontot) |
| `string table coroutine utf8` | a Lua könyvtárai; `string.dump` nincs, `string.format` `%p` és `%a` nélkül |
| `math` | saját: `abs ceil floor fmod max min sqrt tointeger type ult random pi huge maxinteger mininteger` |
| `log(...)` | sor a mod naplójába |
| `require("a.b")` | modul a mod saját fájljaiból (csak szöveges forrás; betű, szám, `_` és pont a névben); egyszer töltődik be |
| `S` | a mentett adat táblája (lásd 5. pont) |
| `Game` | a játék felülete (lásd 4. pont) |
| adattáblák | a motor által megosztott, csak olvasható adatok (pl. sablonok) |

Nincs: `io`, `os`, `debug`, `package`, `load`, `loadfile`, `dofile`, `collectgarbage`, `print`, `next`, `warn`,
`string.dump`, valamint a `math.sin`/`cos`/`exp`/`log` és a többi libm-függvény.

## 3. Mitől determinisztikus

| Veszély | Megoldás |
|---|---|
| a táblák belső sorrendje a véletlen hash-magtól függ | rögzített hash-mag; a `pairs` ráadásul **kulcssorrendben** jár (szám < szöveg < logikai; szám szerint, szöveg bájtsorrendben); tábla- vagy függvénykulcsú táblát a `pairs` nem jár be |
| a `table.sort` pivotja órából jön | rögzített érték |
| a `pow`, `sin`, `exp` … gépenként eltérhet | `^` egész kitevővel ismételt szorzás, `x^0.5` = `math.sqrt` (az IEEE szerint pontos), más kitevő hiba; a libm-függvények nincsenek |
| véletlen | `math.random` a mod saját folyama a játszma magjából (PCG32), mentődik |
| memóriacím a szövegben (`tostring(t)`, `%p`) | a `tostring` csak a típust írja (`table`), a `%p` és a `%a` tiltva |
| a szemétgyűjtés időzítése | `__gc` és `__mode` nem adható meg a `setmetatable`-ben |
| területi beállítás | a szövegek összehasonlítása bájtsorrendű |
| lebegőpontos összevonás (FMA) | a Lua is `-ffp-contract=off` / `/fp:precise` beállítással fordul |

A szimulációs érték (hely, sebesség, lőtáv) a motorban fixpontos. A Game felület a szám pontos alakját adja vissza
(pl. `2.5`), és a kapott számot determinisztikusan kerekíti fixpontosra.

## 4. Game felület

| Hívás | Eredmény |
|---|---|
| `Game.tick()` | az aktuális tick (30 tick = 1 s) |
| `Game.spawn{owner=, x=, y=, hp=100, speed=0.05, damage=10, spread=4, cooldown=30, range=1.5}` | új egység azonosítója; azonnal létrejön |
| `Game.move(id vagy {id, …}, x, y)` | hány egység kapta meg a parancsot |
| `Game.kill(id)` | `true`, ha meghalt (halál-esemény is jön) |
| `Game.unit(id)` | `{id, owner, x, y, hp, max, moving}` vagy `nil` |
| `Game.units(owner?)` | az élő egységek azonosítói növekvő sorrendben |
| `Game.count(owner?)` | az élő egységek száma |
| `Game.after(tick, "kezelő", adat?)` | időzítő; a megadott tick múlva a `kezelő(adat, azonosító)` globális függvény fut |
| `Game.cancel(azonosító)` | időzítő törlése |

A `Game` csak eseménykezelőből hívható, a szkript felső szintjéről nem. A tábla csak olvasható.

## 5. Események, mentés

- **Események** (ha a szkript megadja): `on_start()` új játszma kezdetén; `on_tick(tick)` minden tickben, a parancsok
  után és a rendszerek előtt (előtte a lejárt időzítők futnak, sorrendben); `on_death(id, owner)` a halál után,
  ugyanabban a tickben.
- **Mentés:** csak az `S` tábla (nil, logikai, szám, szöveg és ezekből álló tábla; legfeljebb 32 szint mély), az
  időzítők és a véletlenszám-folyam mentődik. Betöltéskor a szkriptfájlok felső szintje újra lefut, az `on_start`
  nem. Ezért a játszma során változó adat helye az `S`, a felső szint csak függvényeket és állandókat adjon meg.
  Az időzítő adata is mentődik, ezért ugyanezek a típusok lehetnek. A korutin és a függvény nem menthető.
- **Állapothash:** az `S`, az időzítők és a véletlenszám-folyam része a szimuláció hashének. Az eltéréskereső
  „Szkript1”, „Szkript2” … néven jelzi, ha egy mod szkriptállapota tért el.

## 6. Visszajátszás

A szkriptes visszajátszás (2-es formátum) magában hordozza a szkriptek forrását, így az ellenőrzés önálló:

    totu_sim scripted ki.trep kuldetes.lua [--seed S] [--ticks T]
    totu_sim verify ki.trep

Az M3 kapuja az `engine/tests/data/golden_m3.trep` (forrása: `golden_m3.lua`, egy 8 hullámos védekező küldetés).
Mind az öt rendszeren ugyanazt a hasht adja.

## 7. Még hátravan (későbbi mérföldkövek)

- Újratöltés futás közben (fejlesztői mód, M12 szerkesztő).
- A teljes API: épület, erőforrás, régió, cél, párbeszéd, kamera, AI-utasítás (M6–M11).
- Szkriptenkénti profilozás (M8, mérőpanel).
- A mod VFS-éből jövő `require` bekötése a tartalombetöltésbe (M4).
