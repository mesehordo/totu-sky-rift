# Munkanapló

## 2026-11-11 – S00 állapotfelmérés

**Vizsgálva:** a tanulmánycsomag belépő dokumentumai (munkajegyzék, közös követelmények, megvalósítási utasítás, ZIP-tartalomjegyzékek), `README.md`, `main.py`, `rts/` szerkezete, `data/`, `tools/test_hud.py`.

**Ténylegesen futtatott ellenőrzés:**
- `python tools/test_hud.py` → „Minden HUD-teszt rendben.”
- `python main.py --simulate 8 --seed 7 --faction croftian --enemy ooni` kétszer → azonos kimenet (győztes Croftianok, 6,0 perc, 41 mp valós idő).
- `--simulate 15 --seed 7` faj nélkül: Huk’ucha–Micéliák, 7,3 perc játékidő 162 mp alatt (nyers megfigyelés, nem benchmark).

**Nem ellenőrzött:** ablakos játék, menük, mentés/betöltés, kampány, LAN, hang.

## 2026-11-11 – H-001 javítás

**Változás:** `main.py` – `--simulate` módban megadott `--seed` esetén a fajválasztás is a seedből származó `random.Random`-ot használja.
**Ellenőrzés:** `--simulate 2 --seed 7` kétszer → azonos fajpár (Huk’ucha–Croftianok) és azonos kimenet.
**Hatás:** mentés/hálózat/balansz nem érintett; seed nélkül a viselkedés változatlan. **Visszaállítás:** a két sor visszacserélése `random.choice`-ra.

## 2026-11-11 – F-001 kiindulópont

**Változás:** helyi Git-tároló, `.gitignore`, kiinduló commit `ffe4893` (432 fájl, ~3 MB). Kimarad: `assets/`, `backup_grafika/`, `dist/`, a `docs/` képmappái, `saves/`, `__pycache__`, `p.out`.
**Identitás:** csak ebben a tárolóban, `Mesehordo <mesehordo@localhost>`; nincs távoli tároló, feltöltés nem történt.
**Ismert korlát:** a grafika és a mentések nincsenek verziókezelve, ezekről külön mentés kell.
**Visszaállítás:** `git checkout -- <fájl>` vagy `git reset --hard ffe4893`; a tároló eltávolítása: a `.git` mappa törlése.

## 2026-11-11 – H-003b: fekete tesztkép a `--screenshot` módban

**Tünet:** `python main.py --screenshot kép.png --seed 3 --faction elves --enemy ursan` → a játéktér fekete, a minitérkép-keret a bal felső sarokban.
**Ok:** az SDL dummy meghajtóban az egér a (0,0)-n áll, és a pygame „fókuszban lévőnek” jelzi → a szélgörgetés képkockánként a térkép bal felső sarkába vitte a kamerát (nyomkövetés: 1865,2041 → 1061,1237 → 895,1071). A valódi játékot nem érinti.
**Változás:** `main.py` – a képernyőképes ágban a szélgörgetés ki van kapcsolva.
**Ellenőrzés:** az új kép helyes (`docs/fejlesztes/s01_kepernyokep.png`); `tools/test_hud.py` OK.
**Visszaállítás:** `git revert` erre a commitra.

## 2026-11-11 – S02 elkezdve, majd felhasználói kérésre szüneteltetve

**Új eszköz:** `tools/perf_probe.py` – gép–gép meccs, tickidő-percentilisek, részrendszer-bontás (útkeresés, egységek, szétválasztás, FOW, AI …), JSON a `meresek/` mappába. `--render` a kliensrajzolást, `--profile N` a cProfile-t is méri.
**Első mérés** (`--seed 7 --faction croftian --enemy ooni --minutes 8`, i7-6700K, Radeon RX 550, 32 GB RAM, Windows 10, Python 3.13): 6,07 perc játék 52,7 mp alatt; tick p50 1,5 ms, p95 23,9 ms, p99 43,1 ms, max 115 ms; 196/10920 tick lépte túl a 33,3 ms-os keretet, mindössze 72 egységnél. **Az útkeresés a tickidő ~51%-a** (p95 19,8 ms, max 108 ms), az egységfrissítés ~25%, a szétválasztás ~12%.
**Ismert hiba az eszközben:** a csúcsmemória Windows-on `None` (a ctypes-hívás javítandó).
**Szüneteltetés oka:** a felhasználó a C++/C# vizsgálatot kérte előre (2026-11-11).

## 2026-11-11 – C++/C# vizsgálat (soron kívül, felhasználói kérésre)

**Kimenetek:** `docs/architecture_audit/` – `00_REPO_AUDIT.md`, `MODDING_GAP_AUDIT.md`, `CROSS_PLATFORM_GAP_AUDIT.md`, `PORTING_DECISION.md`. Kódváltoztatás a vizsgálat miatt nem történt (első kör: csak felmérés).
**Mérés:** 8P `nyolc_tuz`, seed 1, 12 perc: 300 egység, tick p50 25 / p95 217 / p99 365 ms, ×0,54 valós idő, útkeresés 76%.
**Mellékesen talált és javított összeomlás (H-004):** `Unit.wait_t` hiányzott a parancs nélküli útkérésnél (`rts/entities.py` osztályszintű alapérték). Ellenőrzés: ugyanaz a 8P futás végigment. `tools/test_hud.py` OK.
**perf_probe:** a Windows-os csúcsmemória-mérés javítva (`K32GetProcessMemoryInfo`).
**Javaslat:** nincs győztes; előbb Python-útkeresés javítása, nyelvfüggetlen szerződések, majd C#/C++/Python A/B stresszprototípus. Tulajdonosi döntések: `PORTING_DECISION.md` 7. pont.

## 2026-11-11 – P-001: sugaranként gyorsítótárazott csempe-járhatóság az A*-ban

**Ok (profil, 8P 4 perc):** a `fits()` 12,4 milliószor futott; minden `find_path` hívás újraszámolta ugyanazokat a csempéket.
**Változás:** `rts/pathfinding.py` – `_clearance_grid()`: lustán töltött, sugaranként külön rács; a térkép blokkrácsának bármely változásakor (bájtra pontos összevetés) érvénytelenül. Az `occupied` (egységek) ellenőrzése változatlanul hívásonként fut.
**Új eszköz:** `tools/golden_hash.py` – állapothash-sorozat a viselkedési azonosság igazolására.
**Helyesség:** 8P `nyolc_tuz` seed 1, 3 perc és 2P croftian–ooni seed 7, 6 perc: a 10 mp-enkénti állapothash előtte és utána **bitre azonos**.
**Mérés (8P `nyolc_tuz`, seed 1, 12 perc, ugyanaz a gép):**

| | előtte | utána |
|---|---|---|
| valós idő | 1343 mp (×0,54) | 1032 mp (×0,70) |
| tick p50 / p95 / p99 / max (ms) | 25,0 / 217 / 365 / 935 | 24,6 / 166 / 243 / 642 |
| útkeresés összesen | 1022 mp | 708 mp (−31%) |

**Ismert korlát:** a keret (33,3 ms) továbbra is a tickek 35%-ában túllépve; az útkeresés még mindig 69%. Következő jelölt: az elérhetetlen célra induló, 9000 csomópontig futó keresések és a torlódáskori újratervezés.
**Visszaállítás:** `git revert` erre a commitra.

## 2026-11-11 – A tanulmányok újraértelmezése

**Kérés:** a tanulmányok rendezése egyetlen, nulláról újraépített tervvé.
**Elvégezve:** 78 fájl (67 egyedi) leltára; a 3 legnagyobb fájlról gépi összevetéssel kiderült, hogy tiszta ismétlés (~450 KB); a normatív állítások kigyűjtése; összevetés a kóddal és a kánonnal (`docs/tortenet/`).
**Kimenet:** `docs/alap/TOTU_ALAPDOKUMENTUM.md` (irányadó: dokumentumrend, tények, követelményregiszter állapottal, tesztek, F1–F8 ütemterv, munkamódszer, NE-lista), `docs/alap/DONTESEK.md` (D1–D15), `docs/alap/TANULMANY_LELTAR.md` (fájlonkénti státusz, visszatérő hibák).
**Fontosabb új megállapítások:** az Áhítat negyedik nyersanyag, egyik tanulmány sem ismeri; a README/DESIGN „kő”-je elavult (a kódban `stone` = Arany); a Micéliák a kánonban szövetséges nép, a `mods/micelia` csak feloldja a rejtett `mycel` alapfajt (helyesbítve: nem maradvány); a „3 szövetséges slot” ütközik az 1 népes Diplomácia-csarnok tervvel; a C++/C# csomagban visszatért a „3×4” és a „három allies ikon” hiba; a patrol, a stance és a replay hiányzik.

## 2026-11-11 – Tulajdonosi döntések (D1–D15)

**Rögzítve:** `docs/alap/DONTESEK.md` (összefoglaló tábla + S1 sorrend), az alapdokumentum ütemterve F1–F9-re átírva (F2 szerződés → F3 A/B mérés → ADR → F4a/F4b ág → LAN → internet → játékmenet → modolás → kiadás).
**Végrehajtva:** D8 – `LICENSE.md`: a grafika minden joga fenntartva, modkivétellel (játékon belüli megjelenítés); szabályadatok és modding-doksi CC BY 4.0 marad; `README.md` fejléce igazítva.
**D12 (helyesbítve):** először kikapcsoltam, majd tévesen visszavontam, mert azt hittem, a Gombanép-kampány a modra épül. A `mycel` valójában rejtett alapfaj, a mod csak feloldja; a végleges állapotot lásd a következő bejegyzésben.
**Megfigyelés:** a `saves/mods.json` a `pelda_mod`-ot is engedélyezi (ursan +10% életerő, olcsóbb mamut, több kezdőnyersanyag, Python-hook) – ez a normál játék balanszát is módosítja; a korábbi mérések ezzel futottak.

## 2026-11-11 – D12 végrehajtva: Micéliák rejtett faj

**Tény:** a `mycel` faj az alapadatokban rejtett (`data/factions/mycel.json`, `hidden: true`); a `mods/micelia` csak a `hidden` mezőt oldja fel.
**Változás:** `mods/micelia/mod.json` – „Példa” címke, magyarázó leírás, `enabled: false`; `saves/mods.json` – a `micelia` kikerült az engedélyezett listából (a `pelda_mod` marad).
**Ellenőrzés:** mod nélkül mind az 5 Gombanép-küldetés (`egsz_4_gombanep/g1–g5`) felépül és 20 mp-ig fut, a játékos faja `mycel`; egyéni csatában a faj nem választható (`faction_list`: 7 főfaj).
**Visszaállítás:** a mod a főmenü Modok ablakában bekapcsolható.

## 2026-11-11 – F2/1: biztonságos JSON-mentés (D10)

**Változás:** új `rts/snapshot.py` – a teljes szimulációs objektumgráf JSON-ba (megosztott hivatkozásokkal, objektumtáblával); betöltéskor csak a szimulációs modulok saját osztályai jönnek létre `__new__`-val, kódfuttatás nélkül; a játékadatokra útvonallal hivatkozik (`$data`), így betöltés után a futó adat ugyanazon objektumai kötődnek vissza. `rts/savegame.py`: új formátum `.tsav` (gzip JSON, `SAVE_VERSION` 2); a régi `.sav` csak átalakítással (`tools/convert_saves.py`), a játék közvetlenül nem tölti be.
**Új eszközök:** `tools/snapshot_check.py` (mentés → betöltés → folytatás hash-összevetése), `tools/test_snapshot.py` (oda-vissza + 7 hamisított-mentés eset).
**Ellenőrzés:** oda-vissza bitre azonos: 2P skirmish (42 KiB, 37/25 ms), 8P `nyolc_tuz` 180 s-nál (242 KiB, 213/163 ms), **46/46 kampányküldetés**; játékbeli mentés/lista/betöltés útja; `test_snapshot.py` 12/12; `test_hud.py` OK.
**Átalakítás:** a profil 9 régi mentése átalakítva, mindegyiknél egyező állapothash; a régi fájlok `.sav.regi` néven megmaradtak, a teljes `saves/` mappáról másolat: `%TEMP%\saves_mentes_2026-11-11`.
**Ismert korlát:** a „Wullof 5p” mentés micelia + pelda_mod modokkal készült, csak így tölthető be (szándékos modlista-ellenőrzés).
**Mellékesen talált:** `f3_a_nevtelen_hajnal` küldetésben a `cro_kaptar` (A Kórus magja) nem helyezhető el (-4, 46) – figyelmeztetés a küldetés felépítésekor (H-005).

## 2026-11-11 – F2/2: parancsnapló, visszajátszás, golden készlet

**Változás:** új `rts/replay.py` (Recorder / Player / verify; a parancskódolás a LAN-é: `net.enc` / `net.execute`); `rts/ui.py` – egyjátékos módban a `cmd()` naplóz; `main.py` – `run_game` burkoló: egyéni csatában automatikus felvétel a profil `replays/` mappájába (utolsó 20, azonos másodpercben is egyedi név).
**Új eszközök:** `tools/replay_check.py` (szkriptelt „emberi” parancsokkal felvesz és ellenőriz), `tools/golden_replays.py` + `tests/golden/` (3 felvétel: 2P croftian–ooni s7, 2P ursan–elves s11, 3P hármas folyó s2; modok nélkül).
**Ellenőrzés:** 2P 5 perc 93 parancs 30/30 hash; 8P 3 perc 18/18; UI-úton (`UI.cmd`) kiképzés + gyűjtés 125 parancs 30/30; golden készlet 3/3 új folyamatban és `PYTHONHASHSEED=12345`-tel is; forgatás: 23 mentésből 20 marad; `test_hud.py`, `--screenshot`, `--simulate` rendben.
**Dokumentáció:** `docs/alap/SZERZODES.md` – idő, determinizmus, parancsséma, snapshot, mentés, felvétel, eszközök, hiányzó részek.
**Ismert korlát:** kampányban és LAN-on nincs felvétel; más gépen nem ellenőrzött determinizmus; a szabályok szöveges leírása még hátravan.

## 2026-11-11 – F2/3: szabályleírás

**Kimenet:** `docs/alap/SZABALYOK.md` – a tick sorrendje, mozgás (sebesség, útkérés, A*, útkövetés, kitérés, szétválasztás), célkeresés és harc (sebzési sorrend: alap → elem → típusszorzó → faji szorzók → pajzs → páncél, területi 50%, lövedék), gazdaság (gyűjtés, kiképzési ellenőrzések, termelési sor, létszám), látótér; mi maradt ki szándékosan; az F3 A/B közös feladat javaslata (bemenet, egyszerűsített szabályok, forgatókönyvek 250–2400 egységgel, mérés, döntési szabály).
**Ellenőrzés:** minden állítás a kódból olvasva, fájl:sor hivatkozással; a kiképzés költséglevonása (`p.pay`) és a gyűjtés visszavitele (`nearest_dropoff`) külön ellenőrizve. Kód nem változott.

## 2026-11-11 – F3 előkészítés: fejlesztőeszközök telepítve (a felhasználó engedélyével)

**Telepítve winget-tel (hivatalos forrás, csendes telepítés):** .NET SDK 10.0.404 (`Microsoft.DotNet.SDK.10`, LTS, MIT); CMake 4.4.4 (`Kitware.CMake`); WinLibs GCC 16.2.0 MinGW-w64 UCRT + mingw32-make + ninja (`BrechtSanders.WinLibs.POSIX.UCRT`).
**Megjegyzés:** a winget parancsaliasokat nem tudta létrehozni (Windowson ehhez fejlesztői mód kellene), ezért a CMake és a GCC a csomagmappából fut: `tools/fejlesztoi_kornyezet.ps1` beállítja a PATH-t, és kikapcsolja a .NET telemetriát.
**Ellenőrzés:** C# konzolprogram `dotnet run -c Release`, C++20 program `cmake -G "MinGW Makefiles"` + build – mindkettő lefordult és lefutott. Mérési értékük nincs (csak lánc-próba).
**Ismert korlát:** a MinGW GCC Windowson nem ad AddressSanitizert; a tanulmány által kért sanitizeres C++-próbához Linux vagy MSVC/clang kell (F3-ban jelezni).

## 2026-11-11 – F3: A/B mérés Python / C# / C++

**Kód:** `tools/ab/` – exportáló (valódi `nyolc_tuz` pálya, 84 egységtípus, 248–2400 egység), Python referencia-mag, C# (.NET 10) és C++20 (GCC 16) átirat, mérésvezérlő, `SPEC.md`.
**Helyesség:** a három mag **bitre azonos állapothash-sorozatot** adott minden méretben és ismétlésben (45 futás) – azonos munkát végeztek.
**Hiba közben:** a C++ program hozzáférési hibával állt le, ha a GCC mappája nem volt elöl a PATH-ban (a Git régebbi `libstdc++` DLL-je töltődött be) → statikus futásidő (`5db373e`).
**Eredmény** (`meresek/AB_EREDMENY.md`): Python 59–70× lassabb (248 egységnél p50 112 ms); C# és C++ 2400 egységnél is p99 < 9 ms, a C++ 4–7%-kal gyorsabb és 4× kevesebb memóriát használ; C# legrosszabb tick egy futásban 128 ms (JIT/GC).
**Korlát:** útkeresés-uralt terhelés (6-os útkérés-keret, 3 perc alatt 59 ölés); egyszerűsített mag; egy gép.
**Javaslat:** `docs/alap/dontesek/ADR-001-motor.md` – C# (.NET 10) mint célnyelv, C++ tartalék a forró részekre; jóváhagyásra vár.

## 2026-11-11 – F3: harc-uralt mérés

**Változás:** `export_scenario.py --scenario battle` (akadálymentes csatatér, 8 sereg körben, 4 csempe távolságra, mind a középre tart); `run_ab.py --scenario`.
**Eredmény:** 1800 tick, 3 ismétlés × 3 nyelv × 5 méret, bitre azonos hash (45 futás). Python 72–80× lassabb (2400: p99 274 ms); C# és C++ 2400 egységnél p99 2,9 / 2,5 ms; nagy létszámnál a C++ ~10%-kal gyorsabb, kis létszámnál a C# fix többletköltsége látszik (0,1–0,2 ms). Az ADR-001 javaslata változatlan, a táblája kiegészítve.

## 2026-11-11 – ADR-001 elfogadva: C# (.NET 10)

**Döntés:** a tulajdonos jóváhagyta az ADR-001-et (C# .NET 10 a motor nyelve, C++ tartalék a forró részekre). Rögzítve: `docs/alap/dontesek/ADR-001-motor.md` (ELFOGADOTT), `DONTESEK.md` (D1 lezárva, D2 újranyitva a C#-os megjelenítő-lehetőségekkel), alapdokumentum ütemterve (F4a elvetve, F4b C#-port következik).

## 2026-11-12 – D8 módosítva: a kód GPL-3.0-or-later

**Kérés:** licenc „mint a Stratagus / OpenRA” (módosítható, megnevezéssel; grafika és történet a tulajdonosé).
**Változás:** `COPYING` – a GNU GPL 3 hivatalos szövege (gnu.org, 674 sor); `LICENSE.md` – a kód GPL-3.0-or-later, magyar összefoglalóval és azzal a tájékoztatással, hogy a GPL az eladást nem tiltja; a korábbi MIT-változatok MIT-ek maradnak; a grafika, a világ és a történet fenntartva; szabályadatok és modding-doksi CC BY 4.0; külső összetevők közé a .NET és a raylib. `README.md` fejléce, `DONTESEK.md` D8, alapdokumentum frissítve.
**Nyitott:** SPDX-fejlécek a forrásfájlokban (később, a C#-port fájljaiban kezdve).

## 2026-11-12 – D2 eldöntve: raylib-cs, OpenGL 3.3 alap

**Döntés:** a C#-os megjelenítő raylib-cs (zlib); a MonoGame/FNA/KNI Ms-PL licence miatt kiesett. OpenGL 3.3 az alap (macOS legfeljebb 4.1, régi Intel GPU-k 3.3–4.0); 4.x legfeljebb opcionális magas grafikai profil, játékszabályt nem érinthet. Tartalék: Silk.NET. Véglegesítés előtt rajzolási próba.

## 2026-11-12 – Rajzolási próba (raylib-cs)

**Kód:** `tools/ab/render/build_atlas.py` (valódi HD sprite-okból, szélvágással, 4096² atlasz; a kimenet nem kerül a repóba), `tools/ab/render/probe/` (Raylib-cs 8.1.0, projektszintű `nuget.config` a nuget.org-gal – a felhasználói NuGet-beállításban nem volt forrás, azt nem módosítottam).
**Eredmény** (`meresek/RENDER_PROBA.md`, RX 550, 1280×720, vsync nélkül): 1600 egység mind látható p99 10,5 ms; tipikus nézet (549 látható) p99 4,2 ms; 2400 mind látható p99 15,8 ms. A D2 (raylib-cs, OpenGL 3.3) megerősítve.
**Korlát:** HD 4600-on nem mért; a HD sprite-okból csak 16 típus fér egy 4096²-es lapra → atlasz-stratégia kell a portban; csapatszín, köd, épület, effekt még nincs benne.

## 2026-11-12 – C#-port terve

**Kimenet:** `docs/alap/PORT_TERV.md` – elvek (port, nem újratervezés; bitre azonos paritás mint kapu), a bitazonosság akadályai a Python-kódban (163 `hypot`/`dist`, ~90 trigonometrikus hívás, 18 `random`-hívás, bejárási sorrend) és megoldásuk (közös `dmath`, C# `PyRandom`), `engine/` projektstruktúra, paritás-eszközök (snapshot-betöltés C#-ban, hívásfelvétel, tickenkénti eltéréskereső, golden replay), P0–P9 fázisok kapukkal, atlasz-stratégia, kockázatok, nyitott kérdés (szabálybefagyasztás).

## 2026-11-12 – D16 szabálybefagyasztás; P0 (Python-előkészítés) kész

**Döntés:** a port idejére csak hibajavítás és adatban felvehető tartalom (D16).
**Változás:** `rts/dmath.py` – determinisztikus `hypot`, `dist`, `sq`, `sin`, `cos`, `atan2` csak `+ − × ÷ sqrt floor` műveletekkel (a sin/cos/atan az fdlibm kerneleit követi; legnagyobb eltérés a platformkönyvtártól 2 ULP). A szimulációs modulokban (game, entities, ai, heroes, campaign, mission, world, points, pathfinding) 136 `hypot`, 5 `dist`, 26 `sin`, 26 `cos`, 17 `atan2` és 43 lebegőpontos `(…) ** 2` cserélve. A `mapgen` és a `scenery` kimarad: csak a játék indulásakor fut, eredménye a kezdő snapshotban van.
**Új mért tény:** Windows-on a Python `x ** 2` 2 millió próbából 3892-szer nem egyezik bitre `x * x`-szel (a CRT `pow`-ja) – ezért a négyzetre emelés is a cserébe került.
**Bejárási sorrend (felmérés):** a szimuláció 19 `set`-je közül három bejárás számított: (1) `Entity.mod` a bekapcsolt képességek (`toggles`, szöveges set) módosítóit szorozta/adta össze – a szöveg-hash folyamatonként véletlen, így 3+ bekapcsolt képességnél a Python-játék sem volt teljesen determinisztikus (H-006); (2) a küldetéspályák erdőtelepítése a `_line_cells`/`_area_cells` set sorrendjében húzott véletlenszámot – C#-ban utánozhatatlan; (3) a kizáró kutatás keresése. Mindhárom rendezett bejárásra javítva. Következmény: a küldetéspályák fáinak helye a sűrűség megtartásával kissé átrendeződött. A többi set csak tagságvizsgálatra vagy sorrendfüggetlen műveletre szolgál; a `dict`-ek beszúrási sorrendjét a C# oldal megtartja (PORT_TERV).
**Golden:** a `dmath` miatt mind a 3 felvétel eltért (első eltérés 1200–6000. tick) – szándékos, újrarögzítve; a rendezett bejárás után változatlan. Új: `tests/golden/dmath_vectors.json` (22 548 bemenet–kimenet pár bit-pontosan, a P1 C#-paritásához), `tools/test_dmath.py`.
**Teljesítmény:** 8P / 10 perc (`meresek/20261112_063754_*`): p50 25,3 ms (korábban 24,6), az egységfrissítés átlaga 9,0 ms (korábban 8,2) – a Python-függvényhívás ára ~10% ebben a részben; más meccs, ezért csak tájékoztató.
**Áthelyezve:** a hívásfelvevő eszköz a P2 elejére kerül, mert formátumát a C#-oldali fogyasztóval együtt érdemes kialakítani.
**Ellenőrzés:** `test_dmath.py`, `test_hud.py`, `test_snapshot.py`, `golden_replays.py` 3/3, `snapshot_check.py --all-missions` 46/46.

## 2026-11-12 – Minőségi elemzés: a C#-motor és a referenciajátékok

**Kérés:** mennyire felel meg a C#-motor a tanulmányoknak; cél a 0 A.D. / AoE / WC3 / SC1 minősége.
**Kimenet:** `docs/alap/MINOSEG_ELEMZES.md`. Az alapok (teljesítmény, determinizmus, mentés, replay, licenc, megjelenítő) mérten megfelelnek. A port terve viszont a mai játékot másolja, és hiányzik belőle, amit a tanulmányok előírnak: interpolált megjelenítés (a Python-kliens nem interpolál), csoportmozgás/formáció, HPA*/flow field (mért igény esetén), AI-minőség és AI-FOG, pálya- és küldetésszerkesztő; felvetés: a D4 újranézése (mind a 4 referenciajáték lockstepet használ, a P0 után a mag platformfüggetlenül determinisztikus). Javaslat: nem nulláról újraírás, hanem port + Q1–Q6 minőségi szakasz mérhető kapukkal; tulajdonosi döntésre vár.

## 2026-11-12 – D17: minőségi szakasz a port tervében

**Döntés:** a tulajdonos jóváhagyta a `MINOSEG_ELEMZES.md` javaslatát. **Változás:** `PORT_TERV.md` – két új elv (a paritás viselkedésre vonatkozik, nem kódszerkezetre; a port a Q-szakasz alapja), a P8-ban `RenderSnapshot` + interpoláció, új 5/A pont: Q1–Q6 kapukkal; `DONTESEK.md` D17; alapdokumentum: F4c sor, F5 megjegyzés (D4 újranézése), F7 a Q4-be olvad.

## 2026-11-12 – Újratervezés nulláról: C++20 + Lua (D18–D20)

**Kérés:** motor 0 A.D. + OpenRA szintű modolással, WC3-szintű küldetésekkel, kétféle Conquest-móddal, minden 0 A.D.-rendszeren, a 7 + 13 faj teljes tartalmával és a mostani grafikával; ütemterv, részcélok, megoldási javaslatok; az időigény nem szempont.
**Tények:** a 0 A.D. Windows, Linux, macOS (Intel, Apple Silicon), FreeBSD, OpenBSD alatt fut; a .NET OpenBSD-n nem, FreeBSD-n közösségi portként. A gitea.wildfiregames.com oldalát botvédelem miatt nem lehetett olvasni; az OpenRA GitHub-oldala szerint Windows, Linux, BSD, macOS, SDL + OpenGL, Lua-küldetések.
**Döntések (tulajdonos):** C++ (D18), Lua (D19), mindkét Conquest-mód saját játékbeli névvel (D20).
**Kimenet:** `docs/alap/UJ_MOTOR_TERV.md` (követelmények K1–K7, a két minta összevetése, architektúra, területenkénti megoldási javaslatok, modolási szintek, küldetések és játékmódok, a fajok átvitele, M0–M14 ütemterv kapukkal, nyitott kérdések, kockázatok); `docs/alap/dontesek/ADR-002-uj-motor.md`; `DONTESEK.md`: D1 felülírva, D2 és D4 újranyitva, D3 bővítve, D18–D20; ADR-001 és PORT_TERV felülírva; alapdokumentum: irány és ütemterv.
**M0 első részcél – tartalmi leltár:** `tools/tartalom_leltar.py` → `docs/alap/TARTALOM_LELTAR.md` + `tartalom_leltar.json`: 22 faj, 220 egység, 152 épület, 168 kutatás, 269 képesség (23 típus), 15 tárgy, 6 mechanika; a teljes szabályszókincs a Python-kódhelyekkel; 6 kódhivatkozás nélküli kulcs (`balance_v6`, `detect_units`, `shield_slot`, `no_sprite`, `extra_buildings`, `hotkey` – ellenőrizendő); 12 név szerinti elágazás a kódban. Grafika: 109 egység-sprite-lap csapatszín-maszkkal, 111 egység sprite nélkül (ma alakzatból rajzolva), 7 épület sprite nélkül.

## 2026-11-12 – Nyilvános tárhely, tisztítás, M1 platform-váz és CI

**Döntések (tulajdonos):** távoli tárhely CI-vel; egyetlen nyilvános GitHub-repó; előtte tisztítás és új, egyetlen kezdő commitból induló történet. A zene, a grafika és a történet egy része generált lehet, de nyom nélkül; a meglévő grafika, zene és történet is felkerül.
**Tisztítás:** a generáló folyamat anyagai (34 fájl: eszközök, listák, írói munkafolyamatok) a helyi, verziókezelésen kívüli `_privat/` mappába kerültek; a naplómappa `docs/fejlesztes` lett; a dokumentumok semleges megfogalmazást kaptak. A régi git-történet helyi mentése: a repó melletti `rts_regi_tortenet_2026-11-12.bundle` (ellenőrizve).
**Grafika:** 27 PNG-ből eltávolítva a C2PA-tanúsítvány (`caBX`) és a szöveges metaadat (a képi adat változatlan); 403 sprite-leíró `generator` mezője semleges. Az `assets/` felkerül (kb. 1 GB, a `_old` másolatok és a `sprites.zip` nélkül). A betűkészletek saját készítésűek. A `LICENSE.md` 2/A pontja a zenére, a hangokra és a betűkészletekre is kiterjed; a 3. pont az új motor könyvtáraira frissítve.
**Motor (M1):** `engine/` – CMake, C++20, szigorú figyelmeztetések, `-ffp-contract=off`; `totu_core` (platformazonosítás, platformfüggetlen állapothash), saját tesztkeret (4 teszt, rögzített referencia-hash), `totu` kliens: SDL3-ablak OpenGL 3.3 core környezettel, `--version`, `--frames N`. Helyben (Windows, GCC 16.2, SDL 3.4.20 forrásból) lefordul, a tesztek zöldek.
**CI:** `.github/workflows/motor.yml` – Windows (MSVC), Linux (GCC), macOS arm64 és Intel, FreeBSD és OpenBSD (virtuális gépen); csak az `engine/` mappát tölti le. `.github/workflows/referencia.yml` – a Python-referencia tesztjei.
**Feltöltés:** https://github.com/mesehordo/totu-sky-rift (nyilvános, `main`, egyetlen kezdő commit).
**CI-eredmény (M1 kapu):** az első futáson a Windows, a Linux, a macOS arm64/Intel és a Python-referencia zöld; a FreeBSD-n hiányoztak az X11-fejlécek, az OpenBSD-n a forrásból fordított SDL3-mal linkelt kliens nem indult (a kernel nem futtatható programként kezelte – valószínűleg az SDL saját ELF-jegyzetei miatt hiányzott/elcsúszott az OpenBSD-azonosító jegyzet; nem bizonyított). Javítás: mindkét BSD-n a rendszer `sdl3` csomagja (ahogy a terv írja). Ezután **mind a 6 feladat zöld** (Windows MSVC, Linux GCC, macOS arm64, macOS Intel, FreeBSD clang, OpenBSD clang). A CI ablak nélkül csak a `--version` próbát futtatja; az ablak és az OpenGL 3.3 core környezet helyben, Windows-on ellenőrizve (`--frames 120`, kilépési kód 0). A többi rendszeren az ablakos próba kijelzős gépet kíván.

## 2026-11-12 – M0: viselkedési specifikáció, 1. rész (fajmechanikák)

**Kimenet:** `docs/alap/spec/README.md` (formátum: azonosító, adat, szabály, kód, próba, új motorban, megjegyzés; a fájlok állapota; a fajazonosítóra ágazó kódhelyek és a javasolt adatkulcsok), `docs/alap/spec/FAJMECHANIKAK.md`: 8 közös faji rendszer (felemelkedés, Égkő-fény, Áhítat, bányák, építési stílusok, létszám, Hadtáp, szövetség) és a 6 mechanika a fajrendszerekkel (Mélység 8 szabály, Tó 5, Energiamező 3, Hálózat 2, Lámpásfény/Harci kedv 3, Ursanok, Wullof 4, Vasbirodalom 3), mindegyik a Python-kódhellyel, számokkal és próbákkal. Gépi ellenőrzés: a leltár minden faj-szintű kulcsa szerepel.
**Talált furcsaságok (tulajdonosi megerősítésre):** a Harci kedv sérülésből egyedül nem tör meg (`hurt` 60 < 100); a tüskék és a tó-sebesség fajnévhez kötöttek; a croftian idézések nem járnak le (fajnév); a rothadás kerete a lelőhelylista sorrendjében fogy; a `detect_units` és a `balance_v6` halott adat; az elfek `self_build` kulcsát csak a gépi ellenfél használja.
**Következik:** `KEPESSEGEK.md` (23 képességtípus), utána kutatás/szabálymódosítók, hősök/tárgyak/táborok, kisnépek.

## 2026-11-12 – D21: a specifikáció furcsaságainak rendezése

**Döntések (tulajdonos):** a közeli hős eleste azonnal megtöri a morált (a sérülés egyedül továbbra sem); a fajnévhez kötött szabályok adatkulcsok; a mélységi rothadás a főépülethez közelebbi fáknál kezdődik; a „felfedés bevezetése” – kiderült, hogy a Szemhordozó már felfed („A Kórus szeme”, 9 csempe), így a `detect_units` felesleges volt; törölve a `balance_v6`-tal együtt.
**Változás (Python-referencia):** `game.kill` – `morale.hero_death_breaks` (Ooni: true); `pond_speed` → faj `pond_affinity` (Huk’ucha); tüskék → faj `spike_immune` (Croftian); idézések → faj `summons_permanent` (Croftian, `entities.py` 3 helyen); `_update_rot` – a fák a legközelebbi saját főépülettől mért távolság szerint; adat: `ooni.json`, `hukucha.json`, `croftian.json`, `wullof.json`, `mycel.json`.
**Ellenőrzés:** HUD- és mentéstesztek, 46/46 küldetés; golden: a croftian–ooni felvétel a 900. ticktől eltért; a régi rothadás-függvénnyel 30/30 hash egyezett (a többi átírás viselkedése azonos) – újrarögzítve, 3/3. Specifikáció, döntések (D21), leltár frissítve.

## 2026-11-12 – M0: képességek specifikációja és átvizsgálása; README

**Kimenet:** `docs/alap/spec/KEPESSEGEK.md` – A: a 23 típus mai működése (közös keret, célzott, azonnali, kapcsoló/állandó, egyedi típusok, elemek) alapértékekkel és kódhelyekkel; B: átvizsgálás – a fajok varázserő-identitása nem érvényesül (pl. Ursanok `cooldown` mellett 8 energiás képesség, Croftian `hp` mellett 9, Wullof `song` mellett 10, Micélia `spores` mellett 8), típusonként eltérő alapértékek (az idézés alap-újratöltése 1 s), eltérő varázssebzés-szabály, öt külön módosítórendszer, 9 egyképességes típus kézzel írt parancsokkal, hiányzó célpontszűrés / ellenséges aura / dispel / varázslási idő / autocast; C: átvehető megoldások (0 A.D. Modifications, Auras, StatusEffects, Identity classes; WC3 képességkeret, buff-objektum, cast point, autocast, hős-időtartam); D: javasolt modell (módosító, állapot, képesség összerakható hatásokkal) és a 23 típus megfeleltetése; 4 tulajdonosi döntési pont.
**README:** a GitHub-oldal leírása újraírva a valós állapotra (elavult volt: „kő”, a Micéliák mint játszható faj, `.sav` pickle, grafikagenerátor, 6×2 parancskártya, Python-motor); a `DESIGN.md` történeti dokumentumként jelölve.

## 2026-11-12 – D22: a képességmodell döntései

**Döntések (tulajdonos):** faji varázserő-forrás, a hősök és a varázslók energiát használnak; egységes alapértékek, a varázssebzés mindenhol a szorzóval; bekerül az ellenséges aura, az eloszlatás és immunitás, a varázslási idő és csatornázás, az automatikus használat; élőt gyógyítás, gépet javítás.
**Felmérés:** a négy érintett faj 35 energiás képessége mind hősé vagy varázslóé – a mai adat megfelel, balanszváltozás nem kell; új ellenőrzés: `tools/tartalom_leltar.py` 17. pont (sértés: nincs). Gyógyítás: 25 gyógyító képesség, ebből egy gépjavító; nyitott javaslat két képességre (Második lélegzet, Foltozás).
**Kimenet:** `DONTESEK.md` D22, `spec/KEPESSEGEK.md` E pont, `spec/README.md`, leltár 17. pont.

## 2026-11-12 – D22 kiegészítés és D23: mod.io

**Döntések (tulajdonos):** a Második lélegzet gyógyít és javít, a Foltozás gépjavító (D22); a modrendszerbe bekerül a mod.io-támogatás (D23).
**Ellenőrzés:** a mod.io C++ SDK licence MIT/BSL (GPL 3-mal összefér); hivatalos platformjai Windows, Linux, macOS – FreeBSD-n és OpenBSD-n a REST API közvetlenül. A tulajdonos anyaga (`TOTU_Sky_Rift_mod_io_integracio.md`) GPLv2+-t írt; a projekt licence GPL-3.0-or-later, ez a tervbe így került.
**Kimenet:** `UJ_MOTOR_TERV.md` 3.2/6. pont (felépítés, megvalósítás, többjátékos modegyeztetés, jogi és üzleti teendők) és M13; `DONTESEK.md` D23 és D22-kiegészítés; `spec/KEPESSEGEK.md` E pont.

## 2026-11-12 – M0: kutatások, szabálymódosítók és ágak specifikációja

**Kimenet:** `docs/alap/spec/KUTATAS_ES_SZABALYOK.md` – a kutatás menete (feltételek, sor, egyszeri érvényesülés, a meglévő egységek életerő-növelése), a 10 statnév forrásokkal és értékekkel, a 39 szabálykulcs csoportosítva (25 paraméterhangoló, 14 viselkedés-szabály) kódhelyekkel, az összefésülés szabálya (felülírás; `chance`-nél a nagyobb), az ágak. Átvizsgálás: két párhuzamos rendszer (összeadó `effects` és felülíró `rules`), kulcsonként külön kód, rejtett sorrendfüggés, csak összeadó statnövelés, a viselkedés-szabályok valójában passzív képességek. Javaslat: 0 A.D. Modifications és Requirements, WC3 többszintű fejlesztések, a viselkedés-szabályok a D22 képességmodell passzív képességeiként; halmozási szabály. Gépi ellenőrzés: a leltár minden szabály-, stat-, kutatás- és ágkulcsa szerepel. 3 tulajdonosi döntési pont.

## 2026-11-12 – D24: kutatások és szabálymódosítók

**Döntés (tulajdonos):** egységes módosítórendszer, többszintű kutatások, a viselkedés-szabályok passzív képességek – rögzítve: `DONTESEK.md` D24, `spec/KUTATAS_ES_SZABALYOK.md`, `spec/README.md`.

## 2026-11-12 – M0: hősök, tárgyak, táborok és elemek specifikációja

**Kimenet:** `docs/alap/spec/HOSOK_TARGYAK_TABOROK.md` – hősök (3-as korlát, újraélesztés `0,6 + 0,12 × szint` idővel, szinthatárok, tapasztalat-képlet és -elosztás 12 csempén, szintlépés, képességtanulás 1/3/5. és végső 6. szinttől, statisztika-összesítés), 15 tárgy, táborok (tükrözött elhelyezés, 5 szint, kisnép-táborok, felriadás, visszatérés, zsákmány), a 13 elem hatásai és kölcsönhatásai. Átvizsgálás: kódba égetett állandók, csak statot adó tárgyak és bolt hiánya, az elemek két helyen és kódba égetett kölcsönhatással, eltérő halmozás; új név szerinti szabály (Nian-őr Kapuőrzése lovas ellen) a README táblázatába. Javaslat: adatszakasz az állandóknak, tárgy mint képességhordozó (WC3), bolt, zsákmánytáblák, elem mint állapot adatban leírt kölcsönhatással. 4 döntési pont.

## 2026-11-12 – D25: hősök, tárgyak, táborok, elemek

**Döntés (tulajdonos):** állandók adatba, elemek állapotként; tárgyak képességhordozók; a bolt (semleges kereskedő, faji bolt) pálya- és lobbibeállítás – a kettős jelölést a tulajdonos „beállítás legyen” válasszal pontosította; a táborok újraéledése pályabeállítás. Rögzítve: `DONTESEK.md` D25, `spec/HOSOK_TARGYAK_TABOROK.md`, `spec/README.md`.

## 2026-11-12 – M0: kisnépek és a Vadon specifikációja (az M0 specifikáció utolsó része)

**Kimenet:** `docs/alap/spec/KISFAJOK.md` – a szövetség megkötése és tartalma (3 épület, 5–6 egység, 3 kutatás), a beilleszkedés a gazda népbe (építési hely, építési stílus, ellátás, töltés, morál, falka, Égkő-fény a kisnépre is érvényes; a tó sebessége nem), a 13 kisnép áttekintése, a kisnép-táborok és a Vadon. Átvizsgálás: 11 kisnépnél van a gazda egész seregére ható kutatás (egyenetlenül); a Huk’ucha tava lassítja a saját szövetségeseit; a kisnép-Micéliák se gépi előnylistán, se táborként nem szerepelnek. Javaslat: kimondott szövetségi bónusz (0 A.D.), zsoldosbérlés a kisnép-táborokból (WC3, 0 A.D.), a gazdaszabály általános elve. 3 döntési pont.

## 2026-11-12 – D26: kisnépek; az M0 specifikáció kész

**Döntések (tulajdonos):** egy szövetségi bónusz kisnépenként, a többi kutatás csak saját; a tó a gazda népét nézi; zsoldosbérlés beállítással.
**Változás (Python-referencia, hibajavítás):** `pond_speed` a gazda népének `pond_affinity` kulcsát nézi. Ellenőrzés: HUD-teszt, golden 3/3 (változatlan).
**Állapot:** a viselkedési specifikáció mind az öt része kész (`docs/alap/spec/`: fajmechanikák, képességek, kutatások és szabályok, hősök/tárgyak/táborok/elemek, kisnépek), a döntések D21–D26.

## 2026-11-12 – M0: három műszaki ADR (javaslat)

**ADR-003 Megjelenítő:** saját vékony RHI, első háttér OpenGL 3.3 core, második SDL3 GPU (Vulkan/Metal/D3D12); a sokol_gfx GL-minimuma a forrásaink szerint 4.1/4.3-ra emelkedhetett, az SDL3 GPU-nak nincs OpenGL-háttere – ezért kettős háttér.
**ADR-004 Számábrázolás:** fixpontos Q16.16 (int32, int64 köztes), csempe-egységű koordináták, egész tickidő, saját PCG32, Lua csak egész/Fixed. Mérés (`tools/ab/fixpont/bench.cpp`, `meresek/fixpont_bench.jsonl`, 2400 egység, 2000 tick): double 0,013 ms/tick; fixpontos bitenkénti gyökkel 0,111; hardveres gyök + egész korrekcióval 0,057 ms/tick (4,4×, a keret 0,17%-a), bitre azonos a pontos gyökkel.
**ADR-005 Adatformátum:** JSON, `inherits` öröklődés, komponensek, a D22/D24/D25 modellek, előfeltétel-kifejezés, JSON Schema, modfoltok (`null`, `+`, `-`), determinisztikus számbetöltés, gettext PO, konverter a mai adatokból.
Mindhárom tulajdonosi jóváhagyásra vár.

## 2026-11-12 – D27: ADR-003/004/005 elfogadva; az M0 lezárva

**Döntés (tulajdonos):** mindhárom műszaki ADR elfogadva. Az M0 (leltár, viselkedési specifikáció, döntések D21–D27, ADR-002–005) kész; következik az M2 determinisztikus mag.

## 2026-11-12 – M2, 1. lépés: determinisztikus matematika

**Kód (`engine/src/core/`):** `fixed` – Q16.16 (`int32`, `int64` köztes), telítés, `from_ratio`, pontos egész gyök (hardveres kezdőérték + egész korrekció), `sqrt`, `length`; `angle` – bináris szög (65 536 = kör), szinusz/koszinusz a rögzített negyedhullám-táblázatból interpolációval, atan2 egész CORDIC-kal, 8 irány; `angle_tables.inc` – generált (`tools/gen_fixed_tables.py`); `rng.hpp` – PCG32 (a referencia-implementáció sorozatát adja), torzítatlan `below`/`range`/`chance`; `decimal` – tizedes szöveg → `Fixed` csak egész aritmetikával (12 tizedesjegyig, kitevővel, tartományellenőrzéssel).
**Tesztek (`engine/tests/test_math.cpp`):** műveletek, gyök (20 000 véletlen érték), szögfüggvények pontossága (sin/cos ≤ 2 egység, atan2 ≤ 1 egység), PCG32 referenciasorozat és egyenletesség, tizedes beolvasás, és egy rögzített **determinizmus-hash** (50 000 lépés minden műveletből): `0xf77842dd87d0cb56`.
**CI:** mind a 6 feladat zöld – a hash Windows MSVC, Linux GCC, macOS arm64 és Intel (Clang), FreeBSD és OpenBSD (Clang) alatt is ugyanaz. Ez az M2 kapujának első része.

## 2026-11-12 – M2, 2. lépés: tartalomréteg és entitás–komponens tár

**Kód:** `engine/src/content/json` – saját JSON-elemző (a számok szövegként, a `parse_fixed` alakítja őket; kulcssorrend megtartva; hibák sor- és oszlopszámmal; UTF-8, `\u`-párok, BOM; ismétlődő kulcs hiba; determinisztikus kiírás); `vfs` – rétegezett fájlrendszer (a felső réteg takar, a lista unió és rendezett, `..` és abszolút út tiltva), `mod.json` (azonosító, verzió, függőségek, ütközések), betöltési sorrend függőségekkel, hiba hiányzó függőségre, ütközésre és körre; `templates` – sablontár: `inherits` (egy vagy több szülő), foltok későbbi rétegekből (`null` törlés, `+`/`-` listaműveletek), `_` kezdetű megjegyzések, körkörös és hiányzó szülő, kétszeres definíció egy rétegben; `engine/src/sim/world.hpp` – entitás–komponens tár: soha újra nem használt azonosító, azonosító szerint rendezett tárolók (determinisztikus bejárás), késleltetett törlés.
**A tesztek közben talált két tervezési hiba (javítva):** egy mod `+`/`-` listafoltja és `null` törlése elveszett, ha a kulcs nem a sablonban, hanem a szülőjében volt – a foltrétegben most foltként/törlésjelzőként megmarad, és az öröklődés feloldásakor érvényesül; két egymás utáni listafolt sorrendhelyesen vonódik össze. A „kétszer ugyanabban a rétegben” ellenőrzés most minden réteget nyilvántart.
**Tesztek:** 16 (ebből 6 új), mind zöld; CI mind a 6 feladat zöld.
**Még hátra az M2-ből:** JSON-séma ellenőrzés, a mai adatok konvertere (ADR-005/9), állapothash a világra, snapshot, replay, fejléc nélküli futtató, és a kapu: egy replay mind az öt rendszeren ugyanazt a hash-sorozatot adja.

## 2026-11-12 – M2, 3. lépés: konverter, séma, a valódi tartalom betöltése

**Konverter** (`tools/konverter_motor.py`, `--check` móddal): a mai adatokból 74 fájl az `engine/content/base/` alá – alapsablonok, 22 faj, egységek, épületek, kutatások (a statnövelők D24-módosítókká), képességek, tárgyak (bónusz → módosító), 13 állapot, fajok (névsor, ágak, faji paraméterek), `game.json`, fordítások (`hu.po`, `en.po`, 1812 kulcs), jelentés (`KONVERZIO.md`). 4499 kulcs komponensbe, 7 kulcs (14 előfordulás) `Legacy`-be; a képességek (`Spec`), a faji paraméterek és a kutatási szabályok mai formában – a D22/D24 modellre az M7 alakítja őket.
**Séma:** `engine/content/schema/entity.schema.json`, `other.schema.json`; C++ ellenőrző (`content/schema`: type, properties, required, additionalProperties, items, enum, minimum/maximum fixpontos összevetéssel, `$ref`).
**Betöltő** (`content/loader`): VFS → sablontárak → séma. A valódi tartalom betöltése két azonosító-ütközést talált (épület és kutatás, kutatás és képesség azonos néven) – megoldás: fajtánkénti névtér (ADR-005 kiegészítés).
**Eredmény:** 220 egység, 152 épület, 168 kutatás, 269 képesség, 15 tárgy, 13 állapot, 22 faj – mind érvényes; 18 teszt zöld.

