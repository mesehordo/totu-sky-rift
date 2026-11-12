# F3 A/B mérés eredménye – Python / C# / C++

Kelt: 2026-11-11 · kód: `5db373e` (`tools/ab/`) · nyers adatok: `meresek/ab/*.json`.
Gép: Intel i7-6700K (4 GHz, 4 mag), 32 GB RAM, Windows 10. Python 3.13.14 · .NET 10.0.15 (SDK 10.0.404, Release,
TieredPGO) · GCC 16.2 (`-O3 -ffp-contract=off`, statikus futásidő).

## Mit mértünk

Ugyanaz az egyszerűsített szimulációs mag mindhárom nyelven (`tools/ab/SPEC.md`): a valódi `nyolc_tuz` pálya
(144×144), 8 csapat, a 7 főfaj 84 földi harci egységtípusa, attack-move a pálya közepére, 30 Hz.
**A három megvalósítás bitre azonos állapothash-sorozatot adott minden méretben és minden ismétlésben**
(45 futás). Tehát pontosan ugyanazt a munkát végezték, és a különbség csak a nyelvé és a futtatókörnyezeté.

Futások: C# és C++ 5400 tick (3 perc játékidő), 3 ismétlés, a táblázatban a medián. Python csak 900 tick
(30 mp), 1 ismétlés, mert ugyanez a munka ott 60–70× lassabb. Az összevetés ezért a 900 tickes ablakon történt.

## Eredmények

**Tickidő, 5400 tick, 3 ismétlés mediánja (ms; a keret 30 Hz-en 33,3 ms):**

| Egység | Nyelv | p50 | p95 | p99 | legrosszabb tick | csúcsmemória MiB | C# GC-szünet összesen |
|---|---|---|---|---|---|---|---|
| 248 | C++ | 1,92 | 3,71 | 5,01 | 9,6 | 5,3 | – |
| 248 | C# | 2,04 | 4,00 | 5,46 | 56,5 | 35,9 | 10,8 ms |
| 496 | C++ | 1,98 | 3,81 | 5,05 | 8,5 | 5,8 | – |
| 496 | C# | 2,07 | 4,02 | 5,55 | 27,4 | 34,0 | 22,9 ms |
| 1000 | C++ | 2,08 | 4,01 | 5,65 | 8,3 | 6,7 | – |
| 1000 | C# | 2,20 | 4,25 | 6,74 | 28,8 | 35,2 | 39,9 ms |
| 1600 | C++ | 2,30 | 4,66 | 6,31 | 9,0 | 7,7 | – |
| 1600 | C# | 2,47 | 4,97 | 8,25 | 28,5 | 35,5 | 67,4 ms |
| 2400 | C++ | 2,62 | 5,39 | 7,88 | 20,9 | 8,9 | – |
| 2400 | C# | 2,85 | 5,88 | 8,87 | 128,1 | 36,1 | 108,7 ms |

**Pythonnal együtt, 900 tickes ablak (medián tickidő, ms):**

| Egység | C++ | C# | Python | Python / C++ | C# / C++ |
|---|---|---|---|---|---|
| 248 | 1,91 | 2,03 | 112,2 | 59× | 1,06× |
| 496 | 1,94 | 2,08 | 116,7 | 60× | 1,07× |
| 1000 | 2,11 | 2,26 | 127,4 | 61× | 1,07× |
| 1600 | 2,56 | 2,65 | 179,7 | 70× | 1,04× |
| 2400 | 3,23 | 3,44 | 222,2 | 69× | 1,06× |

## Harc-uralt forgatókönyv (`battle`)

Akadálymentes 144×144-es csatatér, a 8 sereg körben a közép körül, egymástól 4 csempére; mind a középre tart, így a
harc azonnal kezdődik, az útkeresés egyenes vonalú. Mindhárom nyelv 1800 tick (60 mp), 3 ismétlés, medián.
Bitre azonos állapothash minden futásban (45 futás). Az A*-munka a `center` forgatókönyv ~1/40-e.

| Egység | Nyelv | p50 | p95 | p99 | legrosszabb | memória MiB | ölés 60 mp alatt | C# GC-szünet |
|---|---|---|---|---|---|---|---|---|
| 248 | C++ | 0,08 | 0,24 | 0,33 | 0,6 | 5,0 | 171 | – |
| 248 | C# | 0,13 | 0,54 | 0,89 | 6,8 | 32,4 | 171 | 3,6 ms |
| 248 | Python | 4,00 | 30,39 | 38,67 | 118,9 | 18,6 | 171 | – |
| 496 | C++ | 0,15 | 0,41 | 0,48 | 0,6 | 5,4 | 275 | – |
| 496 | C# | 0,22 | 0,57 | 0,89 | 10,2 | 35,6 | 275 | 5,2 ms |
| 496 | Python | 8,64 | 51,08 | 64,87 | 157,1 | 19,0 | 275 | – |
| 1000 | C++ | 0,36 | 0,89 | 1,02 | 1,6 | 6,3 | 337 | – |
| 1000 | C# | 0,41 | 1,04 | 1,39 | 7,8 | 35,4 | 337 | 13,3 ms |
| 1000 | Python | 21,64 | 100,57 | 121,20 | 139,5 | 19,7 | 337 | – |
| 1600 | C++ | 0,65 | 1,48 | 1,64 | 5,2 | 7,1 | 320 | – |
| 1600 | C# | 0,67 | 1,53 | 2,03 | 9,9 | 38,1 | 320 | 37,3 ms |
| 1600 | Python | 40,63 | 167,22 | 188,58 | 212,7 | 20,6 | 320 | – |
| 2400 | C++ | 1,07 | 2,30 | 2,50 | 3,4 | 8,6 | 391 | – |
| 2400 | C# | 1,11 | 2,39 | 2,91 | 10,1 | 37,3 | 391 | 72,4 ms |
| 2400 | Python | 63,66 | 245,36 | 274,32 | 330,1 | 21,1 | 391 | – |

Átlagos tickidő aránya: Python / C++ = 72–80×; C# / C++ = 2,1× (248) → 1,6× (496) → 1,2× (1000) → 1,1× (1600–2400).
Kis létszámnál a C# fix többletköltsége (JIT, futtatókörnyezet) számít, de abszolút értékben 0,1–0,2 ms.

## Értelmezés

1. **Python:** ugyanaz az algoritmus 59–70×-szer lassabb, mint natívan. Már 248 egységnél is 112 ms a medián tick,
   a keret 3,4-szerese. A szimulációs célt (8 × 200 pop, p99 ≤ 40 ms) Pythonban ezzel a nagyságrendi különbséggel
   nem lehet elérni. Algoritmikus javítás mindhárom nyelvben ugyanúgy gyorsít, ezért az arány megmarad. **Mért.**
2. **C# és C++:** mindkettő bőven a keretben van: 2400 egységnél is p99 < 9 ms (a 33,3 ms-os keret negyede).
   A C++ 4–7%-kal gyorsabb (p50), és 4× kevesebb memóriát használ (9 vs. 36 MiB, a .NET futtatókörnyezet
   alapköltsége). A C# legrosszabb tickje egy futásban 128 ms volt, a többiben 27–57 ms (JIT-bemelegedés és
   szemétgyűjtés). A GC-szünetek összesen 3 perc alatt is csak 11–109 ms-ot tettek ki. **Mért.**
3. **Harcban** ugyanez a kép: a Python 72–80× lassabb (2400 egységnél p99 274 ms, a keret 8-szorosa; már 248
   egységnél is p99 39 ms), a C# és a C++ 2400 egységnél is p99 ≤ 3 ms. Nagy létszámnál a C++ ~10%-kal gyorsabb.
4. A döntési munkalap szabálya szerint a C# és a C++ közti különbség a célterhelésen (1600–2400 egység)
   **4–12%**, és mindkettő a keret töredékét használja: **teljesítmény alapján nincs egyértelmű győztes**.
   A választást a többi szempont dönti el.

## Korlátok – ezt a mérés NEM igazolja

- A `center` forgatókönyv útkeresés-uralt (6-os útkérés-keret, 3 perc alatt 59 ölés); a harcot a `battle`
  forgatókönyv méri (60 mp alatt 171–391 ölés). A kettő együtt fedi le a két fő költséget.
- **Egyszerűsített mag:** gazdaság, képességek, elemek, faji szabályok, repülők és épületek nélkül. A teljes TOTU
  tickje több munka lesz. A nyelvek közti arányra ez várhatóan kevés hatással van, de nem mértük.
- **Egyetlen gép,** és az erősebb asztali CPU. A célzott régi laptop (i5-4300M) egyszálas teljesítménye becslés
  szerint 1,5–2× kisebb; natívan ez is bőven belefér, Pythonban nem.
- Windows; a Linux és a macOS még nem mért. A C++ Windows-on nem kapott sanitizeres (ASan/UBSan) futást.
- A Python helyett a valódi játék ugyanezen a gépen 8 játékos / 300 egység mellett p50 25 ms, p95 166 ms
  (`meresek/20261111_204122_*`); ez összhangban van a Python-mag eredményével.
