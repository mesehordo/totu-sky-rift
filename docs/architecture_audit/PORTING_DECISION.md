# TOTU Sky Rift – C++ / C# / Python döntés-előkészítés

Kelt: 2026-11-11 · commit `320d42e` · kapcsolódó: `00_REPO_AUDIT.md`, `MODDING_GAP_AUDIT.md`, `CROSS_PLATFORM_GAP_AUDIT.md`.
Ebben a dokumentumban a **mért** adat mellett mindig ott a forrása. Minden más **becslés**, és ennek is van jelölve.

## 1. Vezetői összefoglaló

**Javaslat: most ne induljon teljes újraírás, és ne hirdessünk győztest.** A döntési munkalap szabálya szerint győztest csak mért adatra lehet hirdetni (az 5 százalékpontos szabály). C++-os vagy C#-os TOTU-mérés viszont még nincs, ezért ma egyik nyelv mellett sem lehet ténybeli érvet felhozni.

Ami a kódból biztosan kiderült, és a döntést meghatározza:

1. **A szimuláció már most grafikamentes** (~8 400 sor, pygame-import nélkül). Ennek a határnak a mentén lehet szétválasztani a szervert és a klienst: Python marad (E), hibrid (D) és teljes port (A–C) esetén egyaránt.
2. **A tartalom nagyrészt adatvezérelt** (22 fajfájl, 39 adatból vezérelt fajszabály, 58 kampány-JSON, 20 pálya). Ez nyelvfüggetlen, és bármely motorba átvihető. Ami nem vihető át: a ~209 metódusos `game.py` szabálylogikája, a pickle-mentés és a Python modszkriptek.
3. **A mai Python szimuláció kis seregnél is túllépi a keretet.** 72 egységnél a p99 43 ms, a max 115 ms (30 Hz-en a keret 33,3 ms). Az idő fele útkeresés. 8 játékos, 300 egység mellett a medián tick már 25 ms, a p95 217 ms, és a játék a valós időnél lassabban (×0,54) fut. Mindkét mérésben az útkeresés dominál (51% → 76%). Ezért az 1600 egység 20 Hz-en a mai formában **nem érhető el** (mért, nem becsült).
4. **A követelmény szerinti hálózati modell (szerverautoritatív, FOW-szűrt) sehol nincs meg.** A mai LAN lockstep, így bármely nyelvű motorhoz új hálózati réteg kell. Vegyes OS-ű lockstep a lebegőpontos determinizmus miatt kockázatos.
5. **A licencindoklás pontatlan.** A kód ma MIT; a GPLv2+ a 02-es kézikönyvben csak javasolt cél volt, a C++/C# csomag mégis „korábban választott” licencként kezeli. Amíg az MIT marad (D8), a MonoGame kizárásának ez az indoka nem áll fenn.

**Javasolt sorrend (mindegyik kis, mérhető lépés):**

1. **Python kontroll (E):** az útkeresés kimért forró pontjának javítása (a S06-ba illeszkedik). Utána mérés 250/500/800 egységen. Ez mutatja meg, hogy a Python plafon „rossz algoritmus” vagy „Python-overhead”.
2. **Nyelvfüggetlen szerződések:** egy referencia-szimuláció parancsnapló- és replayformátummal; állapot-snapshot séma a pickle helyett; a fajadatok JSON-sémája. Ezek mindhárom úthoz kellenek, tehát nem vesznek kárba.
3. **A/B stresszprototípus:** ugyanaz a headless mag C#-ban és C++-ban (mozgás, A*, ütközés, egyszerű harc, FOW), ugyanazon a pályán és seeden, 250 → 2400 entitással, 20 Hz-en. Ugyanez Pythonban mint kontroll. **Ezen múlik a nyelvválasztás.**
4. A mért eredmény alapján ADR, és csak utána fokozatos port: először a szimulációs mag, a Python kliens ideiglenesen megmarad (D).

**Előzetes, mérés nélküli hajlás (becslés):** C# (.NET + Silk.NET/raylib-cs, Lua a modokhoz). Indok: memóriabiztonság, gyorsabb fejlesztési és hibajavítási ciklus, egyszerű keresztplatformos build. Az 1600 egység 20 Hz-en nagyságrendileg mindkét natív nyelvnek elérhető. A C++ akkor nyer, ha a C# prototípus a GC-szünetek vagy a p99-tick miatt elbukik a H4 kapun. Ez **hipotézis**, nem döntés.

## 2. Hard gate-ek (`04_DONTESI_MUNKALAP.md` 1.)

| ID | Feltétel | C++ raylib | C++ SDL3 | C# OpenTK/raylib-cs | Python (kontroll) | Bizonyíték |
|---|---|---|---|---|---|---|
| H1 | 4 platform build | NEM ISMERT | NEM ISMERT | NEM ISMERT | NEM ISMERT (csak Windows PASS) | `CROSS_PLATFORM_GAP_AUDIT.md` |
| H2 | HD 4600-on indul | NEM ISMERT | NEM ISMERT | NEM ISMERT | NEM ISMERT | nincs ilyen gép |
| H3 | 720p 30 FPS 1600 látható egységgel | NEM ISMERT | NEM ISMERT | NEM ISMERT | NEM ISMERT (render nincs mérve) | – |
| H4 | 20 Hz headless 1600 egység, p99 ≤ 40 ms | NEM ISMERT | NEM ISMERT | NEM ISMERT | **NEM** (72 egységnél p99 43 ms; 300 egységnél p99 365 ms) | `meresek/` 2P és 8P JSON |
| H5 | FOW-szűrés, szerveroldali validáció | NEM ISMERT | NEM ISMERT | NEM ISMERT | NEM (lockstep) | `net.py` |
| H6 | 7+13 faj megőrizhető | adatból IGEN, a logikát portolni kell | ua. | ua. | IGEN | `00_REPO_AUDIT.md` 3. |
| H7 | Mentés/replay/pálya kompatibilitás | NEM (a pickle nem vihető át) | NEM | NEM | IGEN (pickle), replay nincs | `savegame.py` |
| H8 | Licenc | IGEN (raylib zlib, MIT kód) | IGEN (SDL zlib) | IGEN (OpenTK/Silk.NET MIT; MonoGame Ms-PL MIT-kódnál nem akadály) | IGEN | `LICENSE.md` |
| H9 | A fejlesztői gépen fordítható, tesztelhető | NEM ISMERT | NEM ISMERT | NEM ISMERT | IGEN | fejlesztői gép |
| H10 | Nincs viselkedési regresszió | NEM ISMERT (golden teszt kell) | ua. | ua. | – | – |
| H11–H13 | Vegyes OS LAN, online, macOS aláírás | NOT RUN | NOT RUN | NOT RUN | NOT RUN | – |

## 3. Mért adatok (Python baseline)

| Próba | Egység (csúcs) | Tick p50 / p95 / p99 / max (ms) | Keret túllépés | Forrás |
|---|---|---|---|---|
| 2P generált pálya, croftian–ooni, seed 7, 30 Hz | 72 | 1,5 / 23,9 / 43,1 / 115 | 196 / 10920 | `meresek/20261111_192521_S02-SKIRMISH-2P_s7.json` |
| 8P `nyolc_tuz` (144×144), 8 közepes AI, seed 1, 12 perc, 30 Hz | 300 | 25,0 / 217 / 365 / 935 | 7804 / 21600 (36%) – a játék a valós idő 0,54-szeresén futott | `meresek/20261111_195152_S02-nyolc_tuz-8P_s1.json` |

8P bontás: útkeresés **76%** (p95 198 ms, max 911 ms), egységfrissítés 13%, szétválasztás 5%, FOW 1,3%, AI 0,8%. Csúcs-munkamemória 50 MiB.

Gép: i7-6700K, RX 550, 32 GB, Windows 10, Python 3.13.14. Egyszeri futások, nem a „10 perc × 3 ismétlés” szabvány szerint, tehát tájékoztató jellegűek.

**Mit jelent ez a nyelvválasztásra?** A szűk keresztmetszet elsősorban **algoritmikus**: minden útkérés egy teljes, legfeljebb 9000 csomópontos A* Pythonban, gyorsítótár, hierarchia és flow field nélkül. Torlódáskor a sorozatos újratervezés ezt megsokszorozza. Egy natív port ugyanazzal az algoritmussal nagyságrendileg gyorsabb lenne (becslés), de az algoritmust bármelyik nyelven ki kell cserélni. Ezért az útkeresés javítása Pythonban az első lépés: ez megmutatja, mennyi marad a „Python-overheadre”, és a tanult megoldás egy az egyben átvihető.

## 4. Pontozás (`04_DONTESI_MUNKALAP.md` 2.)

Mivel C++/C# TOTU-mérés nincs, a súlyozott pontszám **nem számolható**. A szimulációs (25%), multiplayer (15%), tesztelhetőségi (15%) és hardveres (10%) dimenzió minden natív jelöltnél `N/A`. A licenc (5%) mindháromnál teljesül. A fenntarthatóság (20%) és az UI/editor (10%) pontozása mérés nélkül csak vélemény lenne. A munkalap szabálya szerint itt nem hirdetek győztest.

## 5. Válaszok a 20 kérdésre (`03_AI_KODAUDIT…` 6.)

| # | Kérdés | Válasz | Jelleg |
|---|---|---|---|
| 1 | Hány faj van implementálva? | 7 főfaj + Micéliák modként + 13 kisfaj szövetségi csomagként (5–6 egység, 3 épület) + `vadon` semleges | forrásból ismert |
| 2 | Mekkora a játék? | ~24 000 sor Pythonban, 33 modul, ~70 eszközszkript; a „helyesen működő arány” nem mérhető, csak a szimuláció és a HUD futott le | forrásból ismert |
| 3 | Melyik nyelv tartható karban könnyebben? | A Pythont és a C#-ot a gyorsabb ciklus és a memóriabiztonság miatt; ezt mérés nem támasztja alá | javaslat |
| 4 | Van multiplayer-protokoll? | LAN-lockstep, TCP-n soronkénti JSON, 100 ms-os körök, CRC32-hash; dedikált szerver nincs | forrásból ismert |
| 5 | Mi korlátoz most? | Az útkeresés: 2P-ben 51%, 8P-ben 76% a tickidőből; utána az egységfrissítés és a szétválasztás. A render nincs mérve | mért (2P, 8P) |
| 6 | Mennyire adatvezérelt? | JSON-fajok, 39 szabálykulcs; a különleges mechanikák egy része `cmd_*` metódus a `game.py`-ban | forrásból ismert |
| 7 | Milyen küldetés-triggerek működnek? | JSON-célok: reach, survive, destroy, guard, build_at, escort, kill, gather, build, ponds, train, destroy_all | forrásból ismert |
| 8 | Fajhoz kötött kivételes szabályok? | 39 `rules` kulcs (pl. `pack_bonus`, `moon`, `larva_mult`, `network_supply_add`, `raise_from_kills`) + fajspecifikus parancsok | forrásból ismert |
| 9 | Milyen mentést kell megőrizni? | Ma pickle; nyelvváltáskor nem vihető át, ezért egy új, verziózott séma kell | döntés kell |
| 10 | Mennyi asset van használatban? | 2734 PNG, 1279 hangfájl van az `assets/` alatt; a ténylegesen használt hányadot nem mértem | részben ismert |
| 11 | Elég-e a Python renderer egy ideig? | Valószínűleg igen, a szimuláció már le van választva | javaslat |
| 12 | Minimumgép, OS, driver? | Nincs HD 4600-as tesztgép; a vizsgálat i7-6700K + RX 550 gépen futott | döntés kell |
| 13 | Kötelező-e a dedikált szerver online? | A követelmény szerint autoritatív szerver kell; a mai kód lockstep | döntés kell |
| 14 | Külön kliens- és szerverminimum? | A közös követelmények már külön kezelik | forrásból ismert |
| 15 | 8×200 pop hány valódi entitás? | Az egységek eltérő létszámot foglalnak (pl. 72 egység = 42 pop a 2P mérésben) | részben mért |
| 16 | Ranked, csalásvédelem? | Nincs a kódban | döntés kell |
| 17 | Licencek? | Kód: MIT; függőségek: LGPL-2.1, BSD; GPLv3-függőség nincs | forrásból ismert |
| 18 | Futtathatnak-e szkriptet a modok? | Ma korlátlan Pythont futtatnak | döntés kell |
| 19 | Mely teljesítménycélok lazíthatók? | – | döntés kell |
| 20 | Dokumentáció, hibajegyek? | – | döntés kell |

## 6. Hiányzó bemenetek

- HD 4600-as (vagy más minimum) tesztgép; Linux-gép; Intel- és Apple Silicon Mac.
- A tulajdonos válaszai a 7. pont kérdéseire.

## 7. Tulajdonosi döntések, amelyek nélkül a port nem indulhat

1. **Licenc:** marad-e az MIT (ekkor a MonoGame is jelölt lehet)?
2. **Hálózati modell:** elfogadott-e, hogy a mai lockstep LAN-t szerverautoritatív modell váltja (bármely nyelven)?
3. **Régi mentések:** elfogadható-e, hogy a pickle-mentések nem vihetők át az új motorba?
4. **Modszkriptek:** elfogadható-e, hogy a Python `script.py` helyett sandboxolt Lua lesz (a `pelda_mod` átírandó)?
5. **A/B prototípus:** jóváhagyod-e a 1. pont 3. lépését (C# + C++ + Python headless stresszmag, kb. 1000–1500 sor nyelvenként)? Ehhez új eszközök kellenek a gépre (.NET SDK, C++ fordító, CMake).
