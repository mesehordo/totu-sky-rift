# A TOTU C#-portjának terve (F4b)

> **FELÜLÍRVA 2026-11-12** – az új, nulláról tervezett C++-motor terve: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) (D18–D20).
> A P0 (Python `dmath`, rendezett bejárás) elkészült, és a Python-referenciában marad; a P1–P9 nem indul.

Kelt: 2026-11-12 · alap: [ADR-001](dontesek/ADR-001-motor.md) (C# .NET 10), D2 (raylib-cs, OpenGL 3.3),
[SZERZODES.md](SZERZODES.md), [SZABALYOK.md](SZABALYOK.md).

## 1. Elvek

1. **Port, nem újratervezés.** A C#-mag ugyanazt csinálja, mint a mai Python-játék. A javítás és az új mechanika
   csak a port után jön, vagy külön, mindkét oldalon elvégezve és dokumentálva.
2. **A Python-játék a referencia,** és a port végéig játszható marad.
3. **A kapu a bitre azonos viselkedés:** ugyanabból az állapotból, ugyanazokkal a parancsokkal a C#-mag ugyanazt az
   állapothash-sorozatot adja, mint a Python. Az A/B mérés megmutatta, hogy ez megvalósítható (`tools/ab/`, 90 futás,
   mindig bitre azonos). Tűréshatáros összevetés RTS-ben nem működik: egy apró eltérés néhány tick alatt más
   célpontválasztáshoz, és onnan teljesen más meccshez vezet.
4. **Modulonként haladunk,** és minden modul a saját paritástesztjén megy át, mielőtt a következő jön.
5. **A paritás a viselkedésre vonatkozik, nem a kódszerkezetre.** A C#-mag külön rendszerekből áll (mozgás, harc,
   gazdaság, látótér, AI …), nem a `game.py` fájlonkénti átirata. A rendszerek hívási sorrendje a Python tick-sorrendjét követi.
6. **A port a minőségi szakasz (Q1–Q6) alapja** ([MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md), D17): a port után a
   szabálybefagyasztás feloldódik, és a minőségi célok mérhető kapukkal következnek.

## 2. Mi akadályozza ma a bitre azonosságot – és a megoldás (P0)

| Akadály | Előfordulás | Megoldás |
|---|---|---|
| `math.hypot`, `math.dist` – a Python saját, pontosított algoritmusa; a lebegőpontos `x ** 2` (Windows-on a CRT `pow`-ja minden ~500. esetben eltér `x * x`-től) | 141 + 43 hely | Közös `dmath.dist(dx, dy) = sqrt(dx*dx + dy*dy)` mindkét oldalon (az IEEE `sqrt` mindenhol bitre azonos) |
| `math.sin`, `cos`, `atan2` – platformfüggő könyvtár | ~90 hely | Közös, csak `+ − × ÷ sqrt`-ből álló determinisztikus `dmath.sin/cos/atan2` mindkét oldalon; a modul-szintű állandók (pl. `STEER_TURNS`) számértékként átvéve |
| A Python `random` modulja (Mersenne Twister + a `random()`, `uniform()`, `choice()`, `randint()` saját képletei) | 18 hely + a mentett állapot | C#-ban `PyRandom`: a Python algoritmusának pontos átirata, a mentett 625 elemű állapotból is indítható |
| `dict` / `set` bejárási sorrend | sok hely | C#-ban beszúrási sorrendet tartó szótár; ahol a Python `set`-et jár be döntéshez, ott rendezett bejárás mindkét oldalon (a P0 felméri, hol számít) |
| Egész osztás, kerekítés, `int()` csonkolás | sok hely | Átírási szabályok: `//` → `Math.Floor`, `round()` → bankári kerekítés, `int()` → nulla felé csonkolás |

A P0 a **Python-játékot is módosítja** (a `dmath` bevezetése). Ez a játékmenetet csak az utolsó biten
változtatja, de a golden felvételeket újra kell rögzíteni (szándékos, naplózott változás).

## 3. Projektstruktúra

Ugyanabban a repóban, közös `data/`, `assets/` és `campaigns/` mappával:

```
engine/
  Totu.sln
  src/Totu.Core/       szimuláció: világ, útkeresés, egységek, harc, gazdaság, AI, küldetés – grafika nélkül
  src/Totu.Content/    JSON-adatok, modbetöltés, sémaellenőrzés (később Lua-szkriptek, D9)
  src/Totu.Server/     fej nélküli futtató, később hálózat (D4/D5)
  src/Totu.Client/     raylib-cs megjelenítő, HUD, menük, hang
  tests/Totu.Core.Tests/   xUnit egységtesztek és modulonkénti paritás
  tests/Totu.Parity/       golden replay- és snapshot-összevetés a Python-referenciával
```

Szabály: a `Totu.Core` nem hivatkozhat a raylib-re, a hálózatra vagy a fájlrendszerre (csak a `Totu.Content`-en át).
Minden forrásfájl SPDX-fejléccel indul: `GPL-3.0-or-later` (D8).

## 4. Paritás-eszközök

1. **Snapshot-betöltés C#-ban:** a C#-mag beolvassa a Python `.tsav`/snapshot JSON-t (az objektumtábla osztály- és
   mezőneveit C#-típusokra képezi), így **bármely tickből** indítható összevetés.
2. **Hívásfelvétel (modulparitás):** a Python-oldali eszköz egy golden meccs alatt felveszi egy modul hívásait
   (bemenet + kimenet, pl. `find_path`, `apply_damage`), a C# egységteszt ugyanezekre ugyanazt kell adja. Így egy
   modul paritása a teljes játék nélkül is igazolható.
3. **Tickenkénti eltéréskereső:** ha a teljes összevetés eltér, a két oldal tickenként kiírja az entitásonkénti
   állapotot, és az eszköz megmutatja az első eltérő ticket és mezőt.
4. **Teljes golden replay:** a P6 végén mind a három golden felvétel és a kampányküldetések snapshot-próbái bitre
   azonosak.

## 5. Fázisok és kapuk

| Fázis | Tartalom | Kapu |
|---|---|---|
| **P0 Előkészítés (Python)** – KÉSZ 2026-11-12 | `dmath` bevezetése (a lebegőpontos `** 2`-vel együtt); a `set`-bejárások felmérése és rendezése, ahol számít; golden felvételek újrarögzítése; `dmath`-referenciavektorok | a Python-tesztek és a 46 küldetés snapshot-próbája rendben; új golden készlet – **teljesült** |
| **P1 Alapok (C#)** | `engine/` szerkezet, `PyRandom`, `DMath`, állapothash, JSON-adat- és snapshot-betöltés | `PyRandom` 10⁶ számon egyezik a Pythonnal; egy betöltött snapshot hash-e egyezik |
| **P2 Térkép és útkeresés** | hívásfelvevő eszköz (a P0-ból áthelyezve); `world`, `pathfinding`, a térbeli index | hívásfelvétel-paritás: minden `find_path` / `smooth_path` / `local_detour` hívás bitre azonos |
| **P3 Egységek és harc** | `Entity`/`Unit`/`Building`/`Projectile`/`AreaEffect`, mozgás, szétválasztás, sebzés, alap parancsok | hívásfelvétel-paritás a sebzésre és a mozgásra |
| **P4 Gazdaság és rendszerek** | termelés, létszám, gyűjtés, látótér, lopakodás, energia, nyálka, hit, morál, pontok, tüskék, tavak, bányák | `--simulate` mód AI nélkül (csak parancsokkal) paritásban |
| **P5 Hősök, képességek** | `heroes`, `_o_cast`, autocast, idézés, elemek | golden felvételek AI nélküli részei |
| **P6 Gépi ellenfél** | `ai.py` | **mind a 3 golden felvétel bitre azonos** – a mag kész |
| **P7 Küldetések** | `mission`, `campaign` (rendező, triggerek) | a 46 küldetés snapshot-próbája C#-ban is azonos |
| **P8 Kliens** | raylib-cs: a magtól csak olvasható `RenderSnapshot`-ot kap (entitás-azonosító, pozíció, irány, animáció, szín, HP, láthatóság, kijelölés, effektesemények), és a két utolsó tick között **interpolál**; térkép, egységek (atlasz-stratégia), HUD (a `test_hud.py` pontjai C#-tesztként), menük, hang, mentés | a játék C#-ban indul, játszható; a HUD-tesztek C#-ban zöldek; a mozgás 60 Hz-es kijelzőn szaggatás nélküli (a megjelenítés a tick-ütemtől független) |
| **P9 Átállás** | a C#-játék lesz az elsődleges; a Python a referenciatesztekhez marad | tulajdonosi jóváhagyás egy végigjátszott meccs és küldetés után |

A szerverautoritatív hálózat (D4) és a modrendszer (D9) a P6 után, már a C#-magra épül.

**Atlasz-stratégia (P8):** fajonként külön lap(ok), a rajzolás laponként csoportosítva; a HD (2×) lapok mellé egy
1×-es változat a kicsinyített nézetekhez. A lapokat egy build-lépés készíti a `assets/` mappából (nem kerülnek a repóba).

## 5/A. Minőségi szakasz a port után (D17)

A P6 után (a mag kész) a szabálybefagyasztás (D16) feloldódik. Minden szakasz saját ADR-rel indul, és a golden
készletet a szándékos viselkedésváltozás után újra kell rögzíteni. Részletek és indoklás: [MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md).

| Szakasz | Tartalom | Mérhető kapu |
|---|---|---|
| **Q1 Mozgás** | csoportcél, formációban vonulás, híd/kapu torlódás, elérhetetlen cél gyors felismerése (P-002) | NAV-DIAG/BRIDGE/MAMMOTH/CHOKE tesztpályák; 50 egység beragadás nélkül átkel a hídon, mért idő alatt |
| **Q2 Nagy léptékű útkeresés** | HPA* és/vagy flow field, ha a Q1 mérése igazolja | 8 × 200 pop, p99 ≤ 40 ms a teljes TOTU-szabályokkal |
| **Q3 Megjelenés** | 60 FPS interpolációval, animációs átmenetek, effektek, csapatszín, kijelölési visszajelzés | 1600 egység mellett képkockaidő p99 ≤ 16,7 ms a fejlesztői gépen |
| **Q4 Gépi ellenfél** | nehézségi szintek, személyiségek, csalásmentes mód (AI-FOG), H-003; patrol és stance (D13) | AI-FOG = 0 szivárgás; győzelmi arány szintenként; 100 gép–gép meccs elakadás nélkül |
| **Q5 Szerkesztő** | pálya- és küldetésgráf-szerkesztő a meglévő JSON-formátumra | a 46 küldetés megnyitható és változatlanul visszamenthető |
| **Q6 Hálózat** | az F5/F6 a D4 újranézése után (lockstep relészerverrel vagy szerverautoritatív, mért sávszélesség alapján) | LAN-2…8, NET-LOSS, NET-REJOIN, FOW-LEAK = 0 |

A `RenderSnapshot` és az interpoláció már a P8-ban elkészül, mert a szimulációt nem érinti. A Q3 erre épít.

## 6. Kockázatok

| Kockázat | Kezelés |
|---|---|
| Rejtett nemdeterminizmus a Pythonban (bejárási sorrend, globális állapot) | P0 felmérés; az eltéréskereső tickre és mezőre mutat |
| A port alatt változik a Python-játék, kétszer kell dolgozni | a port idejére szabálybefagyasztás (D16) |
| A pontos átirat lassabb a mértnél | előbb paritás, utána optimalizálás – a golden készlettel ellenőrizve |
| A kliens (~13 ezer sor UI) a legnagyobb rész | a HUD-tesztek specifikációként; a meglévő elrendezés megtartása |

## 7. Szabálybefagyasztás (D16, eldöntve 2026-11-12)

A port idejére a Python-játékban csak hibajavítás (amit a C#-oldalon is meg kell csinálni) és adatban (JSON)
felvehető tartalom kerül be; új egység, képesség vagy mechanika (köztük a D13 patrol/stance) a port után, a C#-magban.
