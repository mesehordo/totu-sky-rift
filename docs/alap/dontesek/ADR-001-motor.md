# ADR-001: A TOTU szimulációs motorjának nyelve

**Státusz:** FELÜLÍRVA 2026-11-12 – [ADR-002](ADR-002-uj-motor.md) (C++20, mert a C# nem fut OpenBSD-n). Korábban: ELFOGADOTT 2026-11-11
**Kelte:** 2026-11-11
**Érintett:** a szimulációs mag (`rts/game.py`, `entities.py`, `ai.py`, `heroes.py`, `pathfinding.py`, `world.py`,
`mapgen.py`, `mission.py`, `points.py`), később a szerver, a kliens és a modrendszer.

## Probléma és cél

A cél 8 játékos × 200 pop, 30 Hz-es szimuláció (D6, D7), szerverautoritatív LAN és internet (D4, D5),
0 A.D.-szintű modolás (D9), Windows + Linux (D3). A mai Python-játék 8 játékos / 300 egység mellett már a ticket
35%-ában túllépi a keretet (`meresek/20261111_204122_*`).

## Megvizsgált alternatívák és bizonyíték

Közös, bitre azonos eredményű szimulációs mag három nyelven (`tools/ab/`), részletek: `meresek/AB_EREDMENY.md`.

| Szempont | Python | C# (.NET 10) | C++20 (GCC 16) |
|---|---|---|---|
| 2400 egység p99 tick, útkeresés-uralt (keret 33,3 ms) | – (248 egységnél p50 112 ms) | 8,9 ms | 7,9 ms |
| 2400 egység p99 tick, harc-uralt | 274 ms | 2,9 ms | 2,5 ms |
| Relatív sebesség | 59–80× lassabb | 1,04–1,12× (nagy létszámnál) | 1× |
| Csúcsmemória (mag) | 20–22 MiB | ~36 MiB | 5–9 MiB |
| Legrosszabb tick (2400) | 916 ms | 31–128 ms | 10–21 ms |
| Memóriabiztonság | igen | igen (GC) | nem (fegyelem, sanitizerek) |
| Fejlesztési/hibajavítási ciklus | leggyorsabb | gyors (fordítás ~3 s) | lassabb, több hibalehetőség |
| Windows/Linux/macOS build | igen | egy eszközlánc (`dotnet`) | platformonként fordító + CMake |
| Beágyazott szkript (D9) | maga a Python (sandbox nehéz) | Lua (MoonSharp / NLua) | Lua C API |
| Meglévő kód újrahasznosítása | 100% | port | port |

## Döntés

1. **A Python nem marad a szimuláció nyelve.** Mért, 59–80×-os különbség (útkeresésben és harcban is); a célterhelés Pythonban nem érhető el.
   (A Python a port idejére működő referencia és golden-replay forrás marad, az eszközök pedig Pythonban maradhatnak.)
2. **C# (.NET 10 LTS) a javasolt célnyelv.** A teljesítmény a célterhelésen a C++-éval 4–12%-on belül van
   (útkeresés- és harc-uralt forgatókönyvben is), mindkettő bőven a
   keretben, így a döntést a nem-teljesítmény szempontok viszik: memóriabiztonság, gyorsabb és biztonságosabb
   fejlesztés és hibajavítás, egyetlen keresztplatformos eszközlánc, kiforrott Lua-beágyazás.
3. **Elfogadott kompromisszum:** ~30 MiB-tal több memória, és ritka GC/JIT-csúcsok (mért legrosszabb 128 ms egy
   futásban). A szerverbuildben a GC-módot (`SustainedLowLatency` / szerver-GC) és az előmelegítést (ReadyToRun)
   be kell állítani és mérni.

## Felülvizsgálatot kiváltó esemény

Ha a teljes TOTU-szabályokkal a C# szerver 8 × 200 pop mellett p99 > 40 ms-ot vagy rendszeres > 100 ms-os
GC-szünetet mutat, és ezt beállítással nem lehet orvosolni, akkor a forró részek (útkeresés, ütközés) C++-ban
készülnek, natív könyvtárként.

## Visszavonási terv

A Python-játék a port végéig működőképes marad (golden replay-ek: `tests/golden/`). A port modulonként halad, és
minden modul a Python-referenciával azonos viselkedést kell mutasson (golden replay / állapothash), mielőtt a
következő jön.

## Nyitott pontok a jóváhagyáskor

- ~~Harc-uralt forgatókönyv mérése~~ – kész (2026-11-11): ugyanaz a kép, lásd `meresek/AB_EREDMENY.md`.
- ~~D2 újranyitása~~ – eldöntve (2026-11-12): a kliens raylib-cs, OpenGL 3.3 alappal.
