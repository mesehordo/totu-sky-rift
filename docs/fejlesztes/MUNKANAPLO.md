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

## 2026-11-12 – A Python-játék lekerült a GitHubról; CodeQL

**Döntés (tulajdonos):** a régi Python-játék és a hozzá tartozó anyagok ne legyenek a GitHubon; helyben, referenciaként megmaradnak. Az IPTV-projekt botjai közül csak a CodeQL kerül be (a CodeRabbit látható gépi átnéző lenne, és PR-ok nélkül nem is futna; a Dependabotot a tulajdonos nem kérte).
**Változás:** 222 fájl kikerült a követésből (a gépen megmaradt, a `.gitignore` tartja távol): `main.py`, `rts/`, `mods/`, `data/`, `campaigns/`, `tests/`, a Python-eszközök (`tools/`, kivéve `tools/ab/`, `gen_fixed_tables.py`, `fejlesztoi_kornyezet.ps1`), `DESIGN.md`, `docs/architecture_audit/`, `docs/MODDING.md`, `docs/MULTIPLAYER.md`, a `referencia` munkafolyamat. A tartalom az új formátumban az `engine/content/` alatt van; a konverter és a leltáreszköz helyben fut. README újraírva (csak az új motor), `LICENSE.md` igazítva, új `.github/workflows/codeql.yml` (C++ – csak a saját kód – és a munkafolyamatok).

## 2026-11-12 – D28: a Micélia csak kisnép és mod-faj

**Változás:** a konverter a Micélia főnépet a `micelia` mod rétegébe teszi (`engine/content/mods/micelia/`: `mod.json` függőséggel, egységek, épületek, kutatások, faj); az alapban 21 faj marad (a kisnép-Micéliával). Új teszt: az alap egyedül (206 egység, 142 épület, 164 kutatás, 21 faj) és a mod réteggel (220 / 152 / 168 / 22) is érvényes; 19 teszt zöld. A Micélia- és novella-küldetések kikerülnek, a küldetések az M11-ben nulláról készülnek.

## 2026-11-12 – A tulajdonos friss dokumentációjának összevetése; Aurethi Birodalom

**Összevetés:** a friss dokumentáció hat felső szintű fájlja (2026-10-31) négy ponton régebbi volt a játéknál (cím, Csápszolga ára, Quetzlaru, Lélekreaktor) – a tulajdonos döntése szerint a játék marad, a dokumentáció frissítve (az eredetik ott megőrizve). Új anyag: világépítés (2026-11-07–12), hangterv, grafikai cél és stílus.
**Átvezetve:** CF-001 – a megjelenített név mindenhol „Aurethi Birodalom” (275 csere 48 fájlban, 55 névelő „az”-ra javítva; az azonosítók és fájlnevek változatlanok; a munkanapló mint történeti feljegyzés érintetlen). CF-007 nem ütközés (`stone` = Arany).
**Terv:** `docs/alap/VILAGEPITES_INTEGRACIO.md` – rendezett pontok, a játékra váró kreátori döntések (kitsu-mata most, oroszlános gárda, címer, wer-ursanok, Ghoblin-név, Tendüür, Ooni), nyitott kérdések (nyersanyag), és hogy a világépítési anyag mely mérföldkőben kerül a motorba. Az `engine/content` mostantól az egyetlen tartalomforrás (`engine/content/README.md`).

## 2026-11-12 – D29: kitsu-mata

**Döntés (tulajdonos):** a javasolt számok jóváhagyva. **Változás:** `engine/content` – `oon_kitsune`: Kitsu-mata, lövész (`archer` alak, `lovesz` osztály, `ranged`/`organic`), 80 életerő, 12 sebzés, 5,5 lőtáv, 1,4 s, 2,8 sebesség, 70 fa + 30 arany + 10 meteorit, 20 s, energia és képességek nélkül; új magyar és angol név és leírás. A régi Illúzió képesség és az illúzió-másolat egység a tartalomban marad (most nem használja semmi). Dokumentumok igazítva (egységtípusok, fejlesztés-katalógus, új fajok terve, főfajok, döntések, világépítési terv). A sprite még a régi rókamaszkos – a grafika a tulajdonosé. 19 teszt zöld.

## 2026-11-12 – A tanulmányok forrásainak feldolgozása

**Kimenet:** `docs/alap/FORRASOK_ES_TANULSAGOK.md`. A tanulmánycsomag 354 egyedi hivatkozásából kb. 150 szakmai; a motor M2–M12 mérföldköveit közvetlenül érintőket ellenőriztem: AoE-hálózat (*1500 Archers*), *Fix Your Timestep*, Recoil-teljesítményleírás, Pottinger-féle csoportmozgás, Red Blob útkeresés, ORCA/RVO2 (Apache-2.0, lebegőpontos), OpenRA traitek és Lua-API, 0 A.D. komponensek, Luau-sandbox, StarCraft-triggerek, ink, microRTS, Glicko-2; a HPA*-cikk most nem elérhető (404). Tíz kiegészítés a motortervhez (rendszerenkénti hash, mindig bekapcsolt mérés, fő ciklus, mozgásrétegek, Lua-sandbox, feltételrendszer, lockstep-paraméterek, trigger-API, előléptetés, épületfoglalás), ebből kettő tulajdonosi döntés; olvasási lista mérföldkövenként.

## 2026-11-12 – D30: épületelfoglalás, megszállás, előléptetés

**Döntés (tulajdonos):** két épületre ható képesség – elfoglalás (tulajdonosváltás, az épület saját listája képezhető, a képességei nem) és megszállás (a tulajdonos ott nem képezhet és nem kutathat, a távolharcos megszálló kilő; férőhely méret szerint; kiűzés 50% alatt vagy visszafoglalással) –, valamint előléptetés (Veterán, Elit). Kiosztás: népenként 2–3 egység (az én javaslatom, a népek jelleméhez), Ghoblin és Daresh kisnépnél minden egység. Terv: `docs/alap/spec/ELFOGLALAS_ES_ELOLEPTETES.md`; a számok az M7 balanszmunkája.

**D30 pontosítás (tulajdonos):** főnépenként 3 egység kap épületképességet, népenként variált kombinációban (2 E + 1 M, 1 E + 2 M, mindkettőre képes egységekkel) – a kiosztás a specifikációban átírva.

## 2026-11-12 – A küldetések terve az új motorra (D31); a novella-küldetések pályái lekerültek

**Felismerés:** a repó `novellak/` mappájában a novella-küldetések pályái voltak (61 fájl), nem a novellák szövege (az a `docs/tortenet/story/` alatt és a `stories/` könyvtárban van) – a D28 szerint lekerültek a GitHubról (helyben megvannak); a hivatkozások javítva.
**Kimenet:** `docs/alap/KULDETESEK_TERV.md` – elvek (a történet vezet, valódi helyszínek, változatos küldetéstípusok, tétes döntések, három nehézség, modolható eszközök), a küldetésrendszer elemei (triggerek, események, célok, régiók, átvezetők, párbeszéd, erősítés, küldetés-AI, pályánkénti objektumadat, hadjáratállapot, mentés, eligazítás), a hadjárat váza a világépítés helyeivel (11 rész, kb. 41 küldetés), munkamenet (fejezetenkénti tervlapok tulajdonosi jóváhagyással).
**Döntések (D31):** Mór szövetséges hősként más fejezetekben, a gombanép-szál a VII. fejezetbe olvad; az Ooni- és a Huk’ucha-hadjárat marad.

## 2026-11-12 – M2 kész: a golden visszajátszás mind az öt rendszeren egyezik

**Kód:** `core/bytes.hpp` (little-endian bájtfolyam); `sim/world.hpp` – komponensenkénti hash, mentés, betöltés (C++20 concept-ekkel, a műveletek nélküli komponenseket kihagyja), világhash, komponenshash-lista; `sim/sim` – szimulációs mag: fix tick, parancssor (tick szerint, azonos ticken a beérkezés sorrendjében), Position/Movement/Health/Attack, mozgás- és harcrendszer (fixpontos; a legközelebbi ellenség egyenlőségnél a kisebb azonosítóval; sebzés a harc saját véletlenszám-folyamából), teljes mentés–betöltés a függő parancsokkal; `sim/replay` – visszajátszás (mag, parancsok, 30 tickenkénti ellenőrzőpont teljes és komponensenkénti hash-sel), ellenőrzés és eltéréskereső (az első eltérő tick és a komponensek); `tools/totu_sim` – ablak nélküli futtató (`scenario`, `verify`).
**Golden:** `engine/tests/data/golden_m2.trep` – 200 egység, 260 parancs, 1800 tick, 60 ellenőrzőpont, végső hash `0xfe41b6013b53a52c`; a csatában 115 egység esik el (a próba valódi).
**Tesztek:** 22 (determinizmus, mentés–betöltés–folytatás bájtra azonos mentéssel, csonka mentés elutasítása, visszajátszás, szándékos eltérés megtalálása a 30. tickben a Position komponensben, sérült fájl); a tesztek egyike megmutatta, hogy az ellenőrzés kezdetben csak a teljes hash-t nézte – most a komponenshash-eket is.
**M2 kapu:** a CI mind a 6 feladatában (Windows MSVC, Linux GCC, macOS arm64 és Intel, FreeBSD, OpenBSD) a `golden_replay` teszt **egyezik**.

## 2026-11-12 – M3: Lua-réteg

**Lua:** a hivatalos Lua 5.4.8 forrás (MIT) változatlanul az `engine/third_party/lua/` alatt (a lua.org csomag, egyezik a GitHub-tükör `v5.4.8` címkéjével), egyetlen C++ fordítási egységként (`src/script/lua_build.cpp`; a hibák C++-kivételek, így a motor destruktorai lefutnak). Determinizmus makrókkal: rögzített hash-mag, rögzített `table.sort`-pivot, saját hatványozás (egész kitevő ismételt szorzással, 0,5 = sqrt, más kitevő hiba), bájtsorrendű szöveg-összehasonlítás; `-ffp-contract=off` / `/fp:precise`.
**Kód:** `script/host` – modonként saját Lua-állapot (`ScriptHost`, `sim::SimHooks`-ként a szimulációhoz kötve); homokozó (nincs io/os/debug/package/load/dofile/collectgarbage/print/next/string.dump, csak szöveges forrás, a `require` csak a mod modulbetöltőjén át és útvonal-kitörés nélkül); kulcssorrendű `pairs`; címet nem író `tostring` és `string.format`; `__gc`/`__mode` tiltva; saját `math` (a `math.random` a mod saját PCG32-folyama); utasításkeret tickenként (számlálóhorog), memóriakorlát (saját foglaló); csak olvasható adattáblák JSON-ból; Game felület (tick, spawn, move, kill, unit, units, count, after, cancel); események (`on_start`, `on_tick`, `on_death`, megnevezett időzítők); mentés és hash: az `S` tábla, az időzítők és a véletlenszám-folyam. `sim/sim` – `SimHooks`, közvetlen műveletek (spawn, order_move, kill), halál-események, a csatlakozók a hashben és a mentésben (séma 2), az eltéréskereső „SzkriptN” néven jelzi őket; csatlakozó nélkül a hash változatlan (a `golden_m2` egyezik). `sim/replay` – 2-es formátum a szkriptek forrásával. `totu_sim scripted`.
**Golden:** `engine/tests/data/golden_m3.trep` (forrás: `golden_m3.lua`, 8 hullámos védekező küldetés időzítőkkel, halál-eseménnyel, véletlennel, rendezéssel, bejárással, korutinnal) – 3600 tick, 120 ellenőrzőpont, végső hash `0xc07b15c50bafd4ab`.
**Tesztek:** 31 (9 új: homokozó, require, rendezett bejárás és címmentes szövegek, math és hatványozás, utasításkeret – a szkript saját pcall-jával sem kerülheti meg – és memóriakorlát, csak olvasható adat, Game API és események, mentés–betöltés–folytatás bájtra azonos mentéssel, szkriptes visszajátszás és az eltérő szkript megtalálása).
**Leírás:** `docs/alap/spec/SZKRIPT_API.md`. `LICENSE.md` – a Lua a külső összetevők között.
**M3 kapu:** a CI mind a 6 feladatában (Windows MSVC, Linux GCC, macOS arm64 és Intel, FreeBSD, OpenBSD) a `golden_replay_m3` **egyezik**, és a `golden_replay` (M2) is; a CodeQL-elemzés hibátlan, nyitott figyelmeztetés nélkül.

## 2026-11-12 – M4 indul: D32 és az ADR-006 javaslata

**Döntés (tulajdonos, D32):** sík pálya sokféle terepfajtával és StarCraft 1-szerű „kamu” magaslatokkal; csoportmozgás alapból WC3-szerűen, választható AoE-alakzatokkal.
**Kimenet:** `docs/alap/dontesek/ADR-006-terkep-es-mozgas.md` – fél csempés navrács, terepfajták és mozgásosztályok adatban, méretosztályonkénti szabadtér-térkép, hierarchikus útkeresés O(1) elérhetőséggel és azonnali legközelebbi ponttal, A* + JPS-finomítás, flow field mért igény esetén, Pottinger-féle helyi réteg, formáció, híd/kapu/rámpa, `map.json`, hét tesztpálya, kapu (8 × 200 egység, p99 ≤ 40 ms), öt lépés.

## 2026-11-12 – D33 (magaslat, mélység), D34 (pályaméretek); ADR-006 elfogadva; M4 1. lépés: terep, pálya, navrács

**Döntések (tulajdonos):** D33 – a pályaszerkesztőben a terep magaslattá vagy mélységgé nyilvánítható, extra szabályokkal (javaslatom az ADR-006 3. pontjában: látótáv, ráláthatás, lőtáv, felfelé lövés hibája, sebzés, rámpa, mélység mint fedezék, repülők, kivételek; a számok adatban, `game.json` → `elevation_rules`); D34 – kicsi (96), közepes (160) és nagy (256) pálya, egyedi legfeljebb 512 (`map_sizes`). Az ADR-006 elfogadva.
**Tartalom:** `templates/terrain.json` – 24 terepfajta (7 járhatósági osztály: land, shallow, deep, blocked, cliff, ramp, bridge; sebességszorzó, építhetőség, rejtés, jel); `templates/movement.json` – szárazföldi, kétéltű, vízi, repülő; `schema/world.schema.json`; `Movement.class` az entitássémában; `game.json` → `navigation`, `elevation_rules`, `map_sizes`.
**Kód:** `map/terrain` (terepfajták és mozgásosztályok a tartalomból, determinisztikus sorrend), `map/map` (`map.json`: sorok jelmagyarázattal vagy futáshossz-kódolás, szintek −1…2, automatikus szirtperem, rámpa), `map/navgrid` (fél csempés navcellák mozgásosztályonként, Csebisev-féle szabad tér a pálya szélével, méretosztály a sugárból, számlálós statikus akadályok, helyi újraszámolás a változás 2 × határ sugarú környékén).
**Tesztek:** 38 (7 új): terepadat, pályaformátum és hibák, magaslat/mélység/rámpa elérhetősége, 1/2/3 csempés rés a kicsi/közepes/nagy egységnek, akadályok (a helyi frissítés 60 véletlen lépésben is bitre egyezik a teljes újraszámolással), mozgásosztályok és terepváltozás, a `game.json` beállításai, 256-os pálya navrácsa.

## 2026-11-12 – M4 2. lépés: hierarchikus útkeresés

**Kód:** `map/pathfinder` – (mozgásosztály, méretosztály) páronként 16 × 16 navcellás klaszterek, klaszteren belüli régiók (4-szomszédság), régiógráf a klaszterhatárokon, összefüggő komponensek → elérhetőség O(1); elérhetetlen cél helyett a legközelebbi azonos komponensű navcella (gyűrűs keresés, euklideszi távolság, rögzített döntetlen); A* a régiógráfon, majd finom A* a folyosó és szomszédai navcelláin, a terep sebességével súlyozva, 8 irányban sarokvágás nélkül; útsimítás „supercover” látóvonallal; indulás akadályból a legközelebbi szabad helyről; repülő egyenesen; akadály- és terepváltozáskor csak az érintett klaszterek számolódnak újra. A nyílt lista teljes rendezésű (f, index), így minden szabványkönyvtárban ugyanaz a sorrend. A JPS kimaradt: a súlyozott (terepsebességes) rácson nem érvényes, és a mérés szerint most nem kell.
**Tesztpályák:** `engine/tests/data/maps/` – NAV-DIAG (három átlós fal résekkel), NAV-UNREACH (sziget mély tóban, sziklatömb), NAV-HIGH (magaslat rámpával, rámpa nélküli mélység).
**Tesztek:** 44 (6 új): NAV-DIAG mindkét irányban optimális költséggel (1644 = a teljes rácsos keresésé), NAV-UNREACH (a legközelebbi parti pont 24 navcella kibontásával; a kétéltű eléri a szigetet), NAV-HIGH (a rámpán át, a nagy egység is; a rámpa eltávolításakor elérhetetlen, visszatéve újra elérhető), a hierarchikus elérhetőség 3600 véletlen próbában egyezik a teljes bejárással akadályváltozások közben is, és a frissített hierarchia azonos a nulláról építettel; rögzített út-hash (`0x4804f5ff69098eae`) minden rendszeren.
**Mérés (256 × 256, 400 sziklatömb és tó, fejlesztői gép):** navrács 8,7 ms, hierarchia 36 ms (betöltéskor), hosszú út átlag 3,5 ms / legrosszabb 7 ms, épület lerakása 3,7 ms (a határélek ismétlődésének kiszűrése előtt 15 ms). Az útkérések tickenkénti kerete a 3. lépésben.

## 2026-11-12 – M4 3. lépés: navigációs mozgás (ütközés, kitérés, útkérés-sor)

**Kód:** `sim/navmove.cpp` és a `Sim` bővítése – a navigáció csak csatolt pályán kapcsol be (`attach_navigation`), így pálya nélkül a szimuláció és a hash az M2/M3-as marad (a golden visszajátszások változatlanul egyeznek). Új komponensek: `Body` (sugár, navrács, méretosztály), `Path` (cél, útpontok, várakozás, beragadás, újratervezés). Útkérés-sor tickenkénti kerettel (régebbi kérés előbb, azon belül azonosító szerint), látóvonalnál keresés nélküli közvetlen út; útpontkövetés a terep sebességével; közbülső útpont 1 csempén belül teljesül, ha a következő látóvonalban van; kemény sugár (a sugár 0,7-szerese) – soha nem lapolódik át, a távolodás megengedett; ugyanarra haladó saját egység mögött sorban vár (nem ingadozik), álló, idegen vagy szemből jövő akadálynál ±30/60/90°-os kitérés rögzített sorrendben; az álló saját egységet félretolja (merőlegesen, arra az oldalra, ahol már áll); beragadásnál újratervezés (legfeljebb 3), a hosszú várakozás (4 mp) is beragadás; tömeg a célnál: a tényleges végponttól (elérhetetlen célnál a legközelebbi elérhető pont) 3 csempén belül megérkezik; akadályváltozáskor (`navigation_changed`) minden mozgó egység újratervez.
**Hibakeresés:** az első változatban a sorban várók feladták (a várakozás elfogyasztotta az újratervezéseket), a „mindkettő zárva” esetben pedig köröztek (az újratervezés-számláló a közeledéskor nullázódott, és a célt az elérhetetlen eredetihez mérték). A tickenkénti nyomkövetés megmutatta a fő okot: mindenki pontosan a közbülső útpontot akarta elérni, és a sorban ide-oda oldalazott. Javítás: közbülső útpont 1 csempén belül, követés a sorban, külön várakozásszámláló, a tényleges végponthoz mért érkezés.
**Tesztpályák:** NAV-MAMMOTH (2 és 3 csempés rés), NAV-CHOKE (két folyosó).
**Tesztek:** 49 (5 új): NAV-MAMMOTH (a nagy egység a széles résen, a 475. tickben; a kicsi a közelin), NAV-CHOKE (a bal folyosó lezárásakor a jobbon át mind a 12 megérkezik, 24 útkereséssel; mindkettő zárva: megállnak, az utolsó 200 tickben senki sem mozdul), 20 egység egy 2 csempés résen át (mind átér, a kemény sugarak egyetlen tickben sem lapolódnak át), félretolás (a saját félreáll, az idegent kikerüli), útkérés-keret (8/tick, a sor kiürül), két futás azonos, mentés–betöltés–folytatás bájtra azonos, rögzített navigációs állapothash (`0x0cee3ee6afee1873`).

## 2026-11-12 – D35: napszakok és időjárás (kérés rögzítve, javaslat)

**Kérés (tulajdonos):** napszakok (mozgó árnyékok, esteledés, virradat) és időjárás a motorban – egyelőre díszlet, de WC3-szerű játékhatással is; az időjárás a pályát is változtathatja (kiáradó folyó, pocsolyák, visszanövő fák …); mindkettő a meccs indításakor kapcsolható; minden népre hasson a napszak és az időjárás is.
**Kimenet:** `docs/alap/spec/NAPSZAK_ES_IDOJARAS.md` – meccsbeállítás (napszak: ki / hangulat / hatással; időjárás: ki / hangulat / hatással / + pályaváltozás; ciklushossz, gyakoriság), determinisztikus környezeti rendszer (napciklus a tickből, időjárás saját véletlenszám-folyamból, éghajlat szerinti átmenetek, környezeti feltételek a D22/D24 modellhez, Lua-események, felület), általános hatások, fajonkénti napszakos és időjárásos hatás mind a 21 népnek, pályaváltozás (ártér, pocsolya, visszanövő fa, villámtűz, befagyó tó, hótorlasz, vándorló dűne, kiszáradó gázló) biztonsági szabályokkal, ütemezés (M5 látvány, M6 rendszer, M7 faji hatások, M8 felület, M11 szerkesztő). A terv a motortervbe (M5, M6) is bekerült. A számok és néhány részlet tulajdonosi jóváhagyásra vár.
## 2026-11-12 – M4 4. lépés: alakzatok és csoportos mozgás

**Kód:** `sim/formation` – alakzatok (D32): WC3-szerű automatikus (széles, kb. 2 : 1; elöl a közelharcosok), sor, doboz, szétszórt, szárnyas, „nincs” (a helyek szétosztása, mindenki a saját sebességével); a paraméterek adatban (`game.json` → `formations`), a helytávolság a legnagyobb egységhez igazodik. `Sim::order_group_move` – a helyek a célpont köré, a haladási irány szerint forgatva; soronként oldalirány szerint rendezett kiosztás (nincs keresztezés); a nem járható hely helyett a legközelebbi elérhető pont; közös sebesség (a leglassabb tag); **közös út** azonos méret- és mozgásosztályonként (egy útkeresés a csoportnak, a tagok látóvonalas csatlakozással kapják).
**A helyi réteg fejlődése** (a tesztek és a nyomkövetés alapján): kemény sugár 0,45 (a puha – a teljes sugár – tömegben megsérthető, ADR-006); szemből jövő egységnél „jobbra tarts” kitérési sorrend és a szemből jövő saját egység kis oldalirányú lökése (sávok alakulnak ki); falmenti csúszás saroknál; a várakozás csak akkor várakozás, ha az akadály az utolsó 3 tickben ténylegesen haladt; mozgó egység miatt soha nem ad fel; az álló saját egységet előbb félretolja, csak utána tekinti tömegnek a célt (alakzattagnál 1 csempén belül); megállt dugóban nem adja fel végleg, hanem legfeljebb 3-szor újrakezd (a zsákutca végpontjánál továbbra is megáll – NAV-CHOKE).
**Tesztpályák:** NAV-BRIDGE (folyó 4 csempés híddal), NAV-FORM (4 csempés rés).
**Tesztek:** 53 (4 új): alakzatok és az adat; NAV-FORM (24 egység a résen át, a 432. tickre összeáll, elöl a közelharcosok, 5 útkeresés + 20 közös út); sor alakzat; NAV-BRIDGE (két 50 fős szövetséges sereg szemből: mind a 100 átkel a 604. tickre, átfedés nélkül, 102 útkeresés). A rögzített navigációs állapothash újra rögzítve (`0xe28045f47c51f3c1`; a `Path` új mezői miatt változott).
**Mérés – kell-e flow field?** Nem: 100 egység szemből egy hídon tickenként átlag 0,05 ms, legrosszabb 3,1 ms; a csoportos parancs osztályonként egyetlen útkeresés. A flow field az ADR-006 szerint csak mért igény esetén jön – az 5. lépés 8 × 200-as mérése dönt véglegesen.

## 2026-11-12 – D36: többféle gépi ellenfél, modolható AI (kérés rögzítve, javaslat)

**Kérés (tulajdonos):** a nehézségi szinteken túl többféle személyiségű AI (védekező, támadó, kiegyenlített és más logikus stílusok, a küldetésekhez is), és AI modként is hozzáadható legyen.
**Kimenet:** `docs/alap/spec/GEPI_ELLENFEL.md` – nehézség (6 szint, csalás csak a nyíltan jelölt legfelsőn) és személyiség (11 stílus, adatként) két tengelyen; Lua-AI keretrendszer menedzserekkel (0 A.D. Petra mintájára), csak a látott információval, ütemezve; küldetés-AI WC3-szerűen; AI-modok három szinten; próbák; nyitott kérdések. A motorterv M9-e kiegészítve.
## 2026-11-12 – M4 5. lépés: kapuk, 3-as visszajátszás, 8 × 200 mérés, golden M4

**Kapuk (ADR-006 8. pont):** `NavGrid::set_gate` (csapatmaszk navcellánként; a szabad tér nyitott kapuval számolódik); az útkereső a kapucellákat külön régióként címkézi (kapumaszkonként), a kapurégiók klaszteren belüli szomszédságát is élként veszi fel, és a komponenseket kilenc nézetben számolja (minden kapu nyitva + 8 csapat) – az elérhetőség csapatonként O(1); a keresés, a legközelebbi pont és a látóvonal a csapattal hívható (`kNoTeam` = a kapu nem számít, így a korábbi hívások változatlanok). A szimulációban a csapat a tulajdonosból jön (1–8. játékos → 0–7. csapat); `Sim::nav_gate`, `Sim::nav_block`. Teszt: NAV-GATE (a saját csapat átmegy, az ellenfél a kapunál megáll; a kapu megnyitásakor átjut; eltávolítás), és a csapatonkénti elérhetőség 25 véletlen kapu- és falváltozás alatt egyezik a teljes bejárással.
**Visszajátszás 3-as formátum:** a `map.json` szövege a fájlban; M4-es parancsok (`GroupMove` alakzattal, `Block` építés/bontás, `Gate`), a `Spawn` sugárral és mozgásosztállyal; a mentés sémája 3. A régi (1-es, 2-es) visszajátszások változatlanul olvashatók; a golden M2 és M3 egyezik. `totu_sim navgolden`, `bench`, `verify --content`.
**Golden M4:** `engine/tests/data/golden_m4.trep` – a NAV-BRIDGE pályán két 60 fős, vegyes méretű (kicsi, távolsági, nagy) ellenséges sereg: WC3-szerű és sor alakzat, csata a hídon, kapu a hídon (előbb csak a 2. játékosnak, később mindkettőnek), építés és bontás a déli mezőn, szárnyas, doboz, szétszórt alakzat; 2400 tick, 130 parancs, 80 ellenőrzőpont, végső hash `0xb4786d076441405e`; új CTest: `golden_replay_m4`.
**Mérés (M4 kapu, fejlesztői gép):** 8 × 200 egység folyamatos csoportos mozgása egy 256 × 256-os, ~10%-ban sziklával és tóval borított pályán, 10 másodpercenként új cél váltakozó alakzatban: **tick p50 17,3 ms, p99 24,8 ms (kapu: ≤ 40 ms – teljesül), legrosszabb 37,3 ms**.
**Hogyan lett meg (mérés → ok → javítás):** az első mérés p99 217 ms volt. A bontás szerint 25 másodpercből 22 az útkeresésé (11 145 keresés, átlag 5000 kibontott cella), de a tick zömét valójában az M2-es harcrendszer vitte (minden támadó mind az 1600 egységet vizsgálta). Javítások: (1) navigációval a harc a mozgás térbeli rácsából keres célpontot (ugyanaz a szabály: a legközelebbi, egyenlőségnél a kisebb azonosító; pálya nélkül a régi kód – a golden M2 egyezik); (2) az A* azonos f-nél a nagyobb g-t veszi előbb (a nyílt rácson nem bont ki széles sávot – a NAV-DIAG kibontása 3851-ről 1914-re esett); (3) a finom keresés előbb csak a régiófolyosón (szükség esetén a szomszédokkal); (4) tickenként 40 000 kibontott cella a keret; (5) a csoporttag a közös út későbbi, látható pontjához is csatlakozhat. Egy kipróbált változtatást (egység okozta beragadásnál ne legyen újratervezés) a tesztek visszadobtak (híd 91/100, NAV-FORM szétesett), kikerült.
**Tesztek:** 55 (2 új), mind zöld; a rögzített út-hash (`0x0f4f54236284369a`) és navigációs állapothash (`0x6daebed53e7984a7`) a szándékos keresési változás miatt újra rögzítve.
**Flow field:** a mérés szerint nem kell (ADR-006: csak mért igény esetén) – a 8 × 200-as terhelés így is a kapun belül van; ha a megjelenítővel együtt szűk lesz, az M5-ben újramérjük.
**M4 kapu:** a CI mind a 6 feladatában (Windows MSVC, Linux GCC, macOS arm64 és Intel, FreeBSD, OpenBSD) a `golden_replay_m4` **egyezik** (a golden M2 és M3 is), a rögzített út- és navigációs hash minden rendszeren ugyanaz; a 8 × 200-as mérés a fejlesztői gépen p99 24,8 ms. **Az M4 (térkép és mozgás) kész.**

## 2026-11-12 – D35 és D36 eldöntve

**Döntések (tulajdonos):** D35 – alapbeállítás napszak *hatással*, időjárás *hangulat*; éjszaka a Wullof vadászai és felderítői láthatatlanok; a számok az M7 balanszában. D36 – 6 nehézségi szint (csalás csak a jelölt „Őrült” szinten), a hadjáratban népe szerinti személyiség, és tanuló AI is: a meccsen belül alkalmazkodik (egységmix, védelem helye, támadás iránya, időzítés; a személyiség korlátai között), a meccs végén felejt – így determinisztikus marad. A két terv (`spec/NAPSZAK_ES_IDOJARAS.md`, `spec/GEPI_ELLENFEL.md`) elfogadottra állítva.

## 2026-11-12 – M5 1. lépés: grafikus réteg (OpenGL 3.3), sprite-köteg, csapatszín

**Felmérés:** a grafika sprite-lapokból áll JSON-leírással (8 irány soronként, animációk oszloptartományként, horgony, külön csapatszín-maszk, HD = 2× méret), a terep 64 px-es csempekészletekből (nyári, téli, erdei) és átmenet-maszkokból; a meta-adatokban nincs gépi nyom. Az M5 lépései az ADR-003 végén.
**stb:** `engine/third_party/stb/` – stb_image v2.30 és stb_image_write v1.16 a `nothings/stb` 2026-10-14-i commitjából (`2a34860`; a tároló friss változata mai keltezésű volt, ezért egy két hétnél régebbit választottam), változatlanul, MIT vagy közkincs; külön fordítási egységben (`render/stb_impl.cpp`), csak PNG. `LICENSE.md` kiegészítve.
**Kód:** `render/image` (PNG olvasás–írás memóriából, felezés előszorzott alfával, kivágás, képeltérés – grafikus kártya nélkül is, külön könyvtár), `render/gl` (OpenGL 3.3 core függvénytábla saját betöltővel, Windows-hívási konvencióval), `render/rhi` (textúra mipmappal, shader-program hibaüzenettel, render-célpont, képkiolvasás), `render/sprite_batch` (példányos rajzolás: egy négyzet egy 40 bájtos példány, a csúcsokat a shader képzi; előszorzott alfa; csapatszín a régi játék „shade” képlete szerint: a maszk alatti szürke rész a csapatszínnel szorzódik), `render/sprite_sheet` (a meglévő sprite-leírás betöltése; a kártya korlátja szerint felez). Kliens: `--demo N`, `--bench`, `--screenshot`, `--check`, `--hidden`, `--size`, `--assets`.
**Mérés (RX 550, 1280 × 720, függőleges szinkron nélkül, a GPU bevárásával):** 1600 egység 8 néptől valódi HD sprite-tal, animációval és csapatszínnel – 8 rajzoló hívás, **p50 1,9 ms, p99 3,8 ms** (a kapu 16,7 ms; a raylib-próba ugyanitt 10,5 ms volt).
**Hiba közben:** az első képen nem látszott a csapatszín – a bemutató minden típust egyetlen színnel rajzolt (a csapat és a típus ugyanabból a sorszámból jött), és a shader fényesség alapján színezett; a régi képlet (szorzás) lett az irányadó, a „mostani kinézet” szabálya szerint.
**CI:** a natív feladatok egyetlen sprite-lapot is kicsekkolnak (cro_agyfalo); Linuxon megjelenítőpróba Mesa szoftveres OpenGL-lel virtuális kijelzőn (`--check`: a kép legalább 2%-a rajzolt), a kép műtermékként letölthető.
**Tesztek:** 58 (3 új: PNG bitre, felezés–kivágás–eltérés, valódi sprite-lap és maszk).

## 2026-11-12 – M5 2. lépés: atlaszkészítő és tömbtextúra

**Kód:** `render/atlas_build` (grafikus kártya nélkül): képkockánkénti vágás a tényleges tartalomra (eltolással), determinisztikus polcos elrendezés 4096²-es lapokra 2 képpontos térközzel, a csapatmaszk ugyanabban az elrendezésben párhuzamos lapokon, sprite-laponként egymás után (egy típushoz csak a saját lapjai kellenek), egyszerre csak egy lap a memóriában, fél minőség (felezés); a leírás (`atlas.json`) oda-vissza alakítható. `tools/totu_atlas` (gyorsítótárba épít, a repóba nem kerül). `render/atlas` (futásidő): a meccshez szükséges lapok **egyetlen tömbtextúrában** (a réteg a sprite-példányban), a maszk egycsatornás (R8) tömb; ha menet közben új típus kell, a tömb kiegészül. A sprite-köteg ezért mindig tömbtextúrát használ (egy kép = egyrétegű tömb), a példány 44 bájt.
**A valódi grafikából:** 393 sprite-lap, 26 866 képkocka → 23 lap (teljes HD, 74% kitöltés, 92 s) vagy 7 lap (fél minőség, 61%, 43 s); a vágás egy egységlapon az eredeti terület 78%-át spórolja meg.
**Mérés (RX 550, 1280 × 720, a GPU bevárásával), 1600 egység 8 néptől, Y szerint rendezve, keverve:** lapokként külön textúrával 1386 rajzoló hívás, p99 19,7 ms (a kapu fölött) → tömbtextúrával **1 rajzoló hívás, p50 0,71 ms, p99 2,0 ms** (teljes HD, 10 lap); fél minőségben p99 2,4 ms, 5 lap.
**Videómemória (megjegyzés):** teljes HD-ban egy lap 64 MB (+ mipmap, + 16 MB maszk); egy meccs 2–4 népe kb. 4–10 lap. A minőségi lépcső (teljes / fél) a beállításokba kerül (M8); a tömörített textúra (S3TC/BC) később mérlegelendő.
**Tesztek:** 61 (3 új): polcos elrendezés (lapon belül, térközzel átfedés nélkül, determinisztikus), mesterséges lapok több lapra bontva – minden képkocka és maszkja pixelre egyezik az eredetivel az eltolással, az üres képkocka nem foglal helyet, a leírás oda-vissza azonos, fél minőség; valódi sprite-lap (224 képkocka, egy lapon). CI: Linuxon atlasz is épül (a kicsekkolt egy lapból), és abból is képpróba fut.

## 2026-11-12 – M5 3. lépés: világnézet (kamera, terep, egységek a szimulációból)

**Terep (`render/terrain_view`), a régi játék módszerével:** a 24 terepfajta `art` leírást kapott (`terrain.json`: táj – tileset, winter, forest –, csempesor, rang, árnyalat, part); a három tájkészlet egy tömbtextúra, az átmenet-maszkok egycsatornásak. Képenként csak a látható csempék: alapcsempe a helyéből számolt változattal, a magasabb rangú szomszéd fajták rang szerint ráfolynak (oldal- és belsősarok-maszkok, a régi sorrenddel), a föld és a homok szélén festett peremsáv (a shader a maszk alfájából számolja, a régi képlettel), a víz előtt széles homokos part, a víz animált. A szint (D33) egyelőre árnyalással: magaslat világosabb, mélység sötétebb, szirtperem sötét sziklasáv – a festett szirtgrafika és a díszletek (sziklakövek, matricák) később.
**Világnézet (`client/world`):** munkamenet (pálya, navigáció, szimuláció), egységek a szimulációból: irány a megtett útból (8 irány a képernyő szerint), járó / álló animáció, simítás két tick között, Y-rendezés, takarás, a népek sprite-jai a tulajdonos szerint, tartalék alakzat (csapatszínű kör) a sprite nélküli egységeknek; kijelölés (bal gombos keret, az 1. játékos egységei), csoportos mozgatás alakzatban a parancssoron át (jobb gomb, 1–6: alakzat), kamera (nyilak / WASD, görgő a kurzor körül). A kamera alapnagyítása a régi játéké (2 × 0,75 = 1,5; tartomány 1,0–2,0), így az egységek és a csempék aránya is a régi. A résztvevő népek atlaszlapjai a világ betöltésekor egyszerre töltődnek (először lustán: a tömb újraépítése menet közben 5,8 mp-es akadást okozott – javítva). `--world pálya.json`, `--content`. Bemutatópálya: `engine/tests/data/maps/DEMO-VILAG.json`.
**Mérés (RX 550, 1280 × 720):** 2 × 60 egység: képkocka p99 3,4 ms. 2 × 800 egység (1600, csatában): a rajzolás p50 6,4 ms, **p99 13,9 ms** (440 látható egység, 857 terep-négyzet); a szimulációs tick p50 23,8 ms, p99 30,2 ms – egy szálon a kettő együtt nem fér el 60 FPS-ben, ezért a szimuláció külön szálra kerül (motorterv, M6).
**CI:** a csempekészletek is kicsekkolva; Linuxon világnézet-képpróba is (Mesa). **Tesztek:** 61 (a terep `art` adatainak beolvasása a terep-tesztben).

## 2026-11-12 – M5 4. lépés: fények, árnyék, köd, víz (napszak a képen)

**Napszak (`render/daylight`, D35, grafikus kártya nélkül):** a nap ideje (0–1) → környezeti fényszín (hajnal rózsás, dél fehér, naplemente meleg, éjjel sötétkék), a fáklyák ereje (éjjel 1, nappal 0) és az árnyék iránya, hossza, erőssége (a nap keletről nyugatra halad: reggel balra, este jobbra dől, délben rövid). A görbe folytonos, és a nap végén körbeér.
**Fény (`render/lighting`):** a jelenet külön render-célpontba rajzolódik, mellé egy fényréteg (környezeti szín + additív, puha szélű fénykörök); az összetétel a kettőt szorozza, majd rárakja a ködöt. A kliensben ideiglenesen minden hatodik egység fáklyát visz (a valódi fényforrások – épületek, varázslatok – a tartalomból jönnek).
**Árnyék:** külön árnyékmenet a sprite-kötegben: a sprite-ot a talpvonalához képest a nap irányába vetíti (a példány új mezője a talp; 48 bájt), elmosott mipmap-szinttel puha a széle, a vége halványabb. Éjjel szinte eltűnik.
**Köd (ideiglenes):** csempénként ismeretlen / felderített / látható, az 1. játékos egységei körül 8 csempe; az összetételben puha szélű. A valódi látótér (magaslat, napszak, időjárás) az M6-ban.
**Víz:** a terep-shaderben enyhe hullámzás és vándorló csillanás.
**Kliens:** `--daytime T` (rögzített napszak), `--nofog`; különben 4 perces napciklus.
**Hiba közben:** OpenGL 1282 – a köteg az árnyékmenetben a fő program uniformjait állította (rossz program volt kötve), és az árnyék napiránya vec3-ként volt deklarálva, de vec4-ként állítva; mindkettő javítva.
**Mérés (RX 550, 1280 × 720, éjjel, 2 × 800 egység csatában):** rajzolás p50 6,0 ms, **p99 17,4 ms** (a 16,7 ms-os kapu fölött – az optimalizálás a 6. lépésben); szimulációs tick p99 32,6 ms (külön szálra kerül, M6).
**Tesztek:** 63 (2 új: napszak értékei és folytonossága).

## 2026-11-12 – M5 5. lépés: részecskék, időjárás-látvány, utófeldolgozás

**Részecskék (`render/particles`, grafikus kártya nélkül):** tisztán látvány, a kliens futtatja képkockánként (a szimulációt nem érinti); hely a talajon + magasság, gravitáció, közegellenállás, szél, élettartam, méret- és színváltozás; rögzített kapacitás (8192 – betelt készletnél az új elmarad, a memória nem nő); saját véletlenszám-folyam (egy gépen megismételhető kép). Effektek: találat (szikra), halál (porfelhő), parázs (fáklya, éjjel), esőcsepp a talajon, füst. A világnézet a szimuláció változásaiból ad eseményt (az életerő csökkent → találat, az egység eltűnt → halál).
**Rajzolás (`render/effects`):** egyetlen, kódból előállított négyrétegű textúra (izzó pont, foltos füstfelhő, gyűrű, pehely); a sima részecskék előszorzott alfával, az izzók összeadódva – és a fényrétegbe is, így éjjel világítanak.
**Időjárás látványa (`render/weather_look`, D35, grafikus kártya nélkül):** a spec 10 állapota (derült, felhős, eső, zivatar, köd, hó, hóvihar, homokvihar, hőség, hamueső) erősséggel: környezeti fény szorzója, árnyékgyengülés, fényforrások sötét viharban, telítettség, kontraszt, foltos pára, vándorló felhőárnyék, hőremegés, csapadék (fajta, sűrűség, sebesség, irány, csíkhossz, szín), villám, cseppek, szél. Fokozatos átmenet két időjárás között (a csapadék fajtaváltáskor a nullán megy át); a villám az időből számolt, megismételhető kettős felvillanás.
**Utófeldolgozás:** a fényréteg összetételi menetében (egy teljes képernyős háromszög, CPU-költség nélkül): hőremegés, felhőárnyék, pára (a fény színezi), köd, csapadék három, kissé eltérő mélységű rétegben (a kamerához rögzítve), villám, színkorrekció, enyhe vignetta. `--nopost`: a 4. lépés képe.
**Kliens:** `--weather NÉV[:ERŐSSÉG]`, a V billentyű a következő időjárásra vált (4 mp-es átmenettel); a mérés kiírja a részecskék számát.
**Mérés (RX 550, 1280 × 720, éjjel, 2 × 800 egység csatában):** zivatarban a rajzolás p50 6,7 ms, p99 9,5 ms; utófeldolgozás nélkül p99 8,8 ms – az utófeldolgozás ~0,7 ms. Esőben egy 900 képkockás futás alatt 13 132 részecske keletkezett, egy sem maradt el.
**Hiba közben:** a villámteszt a kettős villanást kétszer számolta (küszöb az utóvillanás csúcsa fölé); a kliensben egy változó elfedett egy külső nevet (`-Wshadow`).
**CI:** Linuxon éjszakai zivatar képpróba is. **Tesztek:** 68 (5 új: időjárás-nevek és semleges alap, a fajták jellege, folytonos átmenet, villám, részecskék).

## 2026-11-12 – M5 6. lépés: kapu, képösszevetés a CI-ben – az M5 kész

**Gyorsítás:** a világnézet a képkocka egységlistáját (takarás, Y-rendezés) egyszer építi fel, és az árnyék- és a fő menet közösen használja (eddig kétszer épült); az egység animációi a lapról egyszer oldódnak fel (eddig egységenként, képkockánként szöveges kulcsú keresés). A rajzolás p99 9,5 ms → 7,3 ms (azonos jelenet).
**Kapumérés (RX 550, 1280 × 720, a GPU bevárásával; 2 × 800 egység csatában, éjjel, zivatarban, részecskékkel és utófeldolgozással; háromszor 1800 képkocka):** a rajzolás p50 5,9–6,0 ms, **p99 7,3 ms**; a legkisebb nagyításnál (1,0, `--zoom`): p50 3,6 ms, p99 4,6 ms – **a 16,7 ms-os kapu teljesül**. A szimulációs tick (p99 ~29 ms) még egy szálon van – az M6-ban külön szálra kerül, így a képkockát nem terheli. (A 4. lépésnél mért 17,4 ms-os p99-et a mostani, ismételt mérések nem hozták – azt a futtatás közbeni egyéb terhelés okozhatta.)
**Képösszevetés:** `totu_imgdiff` (képpontonként a legnagyobb csatornaeltérés; alap: 12-es küszöb, legfeljebb 2‰ eltérő képpont; eltéréskép pirossal; 3-as kilépési kód: nincs még referencia). A CI Linux-feladata a négy képpróbát (bemutató, atlasz, világ nappal köd nélkül, világ éjjel zivatarban – mindkettő 300 képkocka, a csatával) a tárolt referenciaképekkel (`engine/tests/data/kepek/`, a Mesa-futás képei, meta-adat nélkül) veti össze. Helyben két egymás utáni futás képe 0‰-re egyezik, az eső és a zivatar éjjel 229‰-re tér el – az összevetés érzékeny, de nem ingadozik.
**CI:** mind az öt rendszeren (hat feladat) fordul és tesztel. **Az M5 (megjelenítő) kész.**

## 2026-11-13 – M6 terv (ADR-007), D37

**Felmérés:** az ursan tartalom adatként kész (18 egység, 15 épület, 16 kutatás, 16 képesség); a viselkedés forrása a régi Python-játék (~24 000 sor) és a specifikációk. **Döntések (D37):** ideiglenes HUD; a régi játék szabályai; hősök egységként; ursan tükörmeccs szkriptelt ellenféllel. **Terv:** `docs/alap/dontesek/ADR-007-jatekmag.md` – kilenc lépés (szimulációs szál és pillanatkép, gazdaság, építés, termelés, harc, látótér és lopakodás, kutatás–módosítók–környezet, az Ursanok teljesen, játszható szelet), kapu: golden M6 mind az öt rendszeren, végigjátszható tükörmeccs, képkocka p99 ≤ 16,7 ms és tick p99 ≤ 33 ms 1600 egységnél.

## 2026-11-13 – M6 1. lépés: a szimuláció külön szálon, pillanatkép

**ADR-007 elfogadva** (a tulajdonos: „Elfogadom, mehet”).
**Kód:** `client/session` – a munkamenet (pálya, navigáció, szimuláció) tickenként megváltoztathatatlan pillanatképet ad (egységek azonosító szerint rendezve: hely, életerő, tulajdonos; köd megosztott pufferrel, csak változáskor új), és a tick látványeseményeit (eltűnt egység → halál az előző helyén, csökkent életerő → találat; azonosító szerinti sorrendben). A `SimRunner` futtatja: külön szálon fix 30 Hz-en (lemaradáskor nem pótol: a szimuláció lassul, a kép nem akad), vagy szinkron módban a hívó szálán; a parancsok szálbiztos soron mennek, a következő tickben érvényesülnek. A világnézet (`client/world`) már csak a két legutóbbi pillanatképből rajzol (simítás a közzététel óta eltelt idő szerint; az irány, az animáció és a sprite egy azonosító szerint rendezett, lineárisan összefésült tömbben). Kliens: képernyőképnél mindig szinkron (a kép képkockára megismételhető), különben szálon; `--sync` a szinkron méréshez.
**Regresszió:** a változás előtti képekkel (nappali csata köd nélkül, éjszakai zivatar) a szinkron kép **bitre egyezik**.
**Mérés (RX 550, 1280 × 720, 2 × 800 egység csatában, éjjel, zivatarban):** külön szálon a teljes képkocka **p50 5,2 ms, p99 7,1 ms** (9000 képkocka, 47 s, 1416 tick – a szimuláció tartotta a 30 Hz-et, tick p50 14 ms, p99 31,5 ms); szinkron módban ugyanez p99 33 ms volt. **A képkocka-kapu (≤ 16,7 ms) a szimulációval együtt teljesül.**
**Tesztek:** 70 (2 új): a szálon futó szimuláció parancs után ugyanazt a hasht adja, mint a szinkron; a pillanatkép rendezett, a simításhoz a két legutóbbi, és a csatában találat- és halálesemény is keletkezik.

## 2026-11-13 – M6 2. lépés: gazdaság

**Szabályadatok (`sim/rules`):** a `game.json` (nyersanyagok: rakodási idő, rakomány, lelőhely-mennyiség; kezdőkészlet; létszámkorlát; Hadtáp-sávok) és a sablonok (egység, épület: életerő, páncél, látótáv, költség, létszám, idő tickben, sebesség csempe/tickben, sugár, munkás, gyűjthető nyersanyagok, rakomány; lábnyom, létszámadás, leadóhely; nép: főépület, munkás, kezdő egységek, építési stílus) tömör, fixpontos alakban; a szimuláció csak ezt olvassa.
**Pálya:** a `map.json` új `objects` szakasza: kezdőhelyek, fák (`T` jelű sorok), lelőhelyek (fajta, hely, méret, mennyiség). Új próbapálya: `engine/tests/data/maps/ECO-ALAP.json` (két tükrös bázis, erdők, arany és meteorit).
**Szimuláció:** új komponensek (egység, épület, lelőhely, munkás) – csak `attach_rules` után regisztrálva, ezért a golden M2–M4 hashe **változatlan**; játékosállapot (nyersanyag ezredegységben a Hadtáp tört bevétele miatt, gyűjtött mennyiség, létszám) a hashben és a mentésben. `setup_match`: lelőhelyek és fák akadályként a navrácsban, játékosonként a főépület a kezdőhelyen, a kezdő egységek a régi képlettel (a pálya közepe felé, 0,35 rad-os ívben), a munkások azonnal gyűjteni indulnak (a nép `start_gather`-je szerint, 16 csempén belül). A munkás állapotgépe a régi `_o_gather` szerint: odamegy (22 képpontos „odaért” határ a test széle és a lábnyom között), rakodik (`gather_time × rakomány / alap rakomány`), a legközelebbi saját, kész leadóhelyre hord (az elérhetetlent 2 s-ig kihagyja), a kimerült lelőhely után 12 csempén belül újat keres; háromszori sikertelen odajutás után 14 csempén belül másikat. A kivágott fa helye járható lesz (a meglévő utak érvényesek maradnak, ezért nincs általános újratervezés). Bevétel a Hadtáppal (csak az arany: ≤ 80 létszám 100%, ≤ 140 70%, különben 40%). Létszám tickenként (kész épületek + egységek). Új parancsok (4-es formátum): `Gather`, `Stop`; a mozgásparancs abbahagyatja a munkát (a rakomány marad). Mentés: 4-es séma (a 3-as is betölthető); `restore_obstacles` a betöltés utáni navrácshoz.
**Kliens:** ha a pályán vannak kezdőhelyek, ursan–ursan meccs indul; a pillanatkép az épületeket, a lelőhelyeket és a játékosok nyersanyagait is viszi; a világnézet az épületeket és a lelőhelyeket a saját sprite-jukkal rajzolja (a régi játék szerint a lábnyom bal felső sarkához igazítva – az atlasz a `footprint_offset`-et horgonynak veszi, ha nincs `anchor`; az épület árnyéka a lábnyom aljától vetül; a fa változata a csempéből számolt); ideiglenesen a címsorban az 1. játékos nyersanyagai és létszáma (a HUD a 9. lépésben).
**Számok (ECO-ALAP, 5 telepes):** 90 s alatt ~770 fa (egy forduló ~4,7 s: 2,2 s rakodás + út), 60 s alatt ~350 arany; 330 s alatt 33 fa kivágva.
**Tesztek:** 75 (5 új): szabályadatok a régi számokkal; meccsindítás (főépület, 5 telepes a pálya közepe felé, létszám 5/12, a fák akadályok); gyűjtés – megmaradás (kivett = bevétel + rakomány), kimerült fa helye járható, a munkások tovább dolgoznak; arany a Hadtáppal és nélküle, `Gather`, `Stop`, mozgásparancs; determinizmus, mentésből folytatva ugyanaz a hash, rögzített gazdasági állapothash (`0x033068f03cde2b46`) minden rendszeren. A demóképek a változás után is bitre egyeznek. CI: új gazdasági képpróba (külön atlasszal, a régi atlasz-próba változatlan).
**Utóbb (CI):** a GCC 13 (Linux) téves `array-bounds` figyelmeztetést adott egy egyelemű listát tartalmazó parancs másolásakor – a tesztben a parancsok áthelyezve. A bemutató-képpróba referenciája frissítve: a CI most a telepes, a főépület és a nyersanyagok lapjait is kicsekkolja, és a bemutató a mappa minden egységlapját használja (a kép a bemenet miatt változott, nem a kód miatt); a gazdasági képpróba referenciája felvéve.

## 2026-11-13 – Árnyékok a régi játék szerint (hely, alak, belső árnyék)

**A tulajdonos kérése:** figyelni az árnyékok helyére és alakjára, és arra, hogy van belső árnyék is. A minta a régi `rts/shadows.py`.
**Irány és erő:** a festett grafikán a fény bal felülről jön, ezért az árnyék jobbra lefelé dől – délben pontosan a régi vetület (magasság-képpontonként 0,62 jobbra, 0,44 lefelé, 150/255 erős, a végén 0,55-re halványul). A napszak (D35) csak e körül mozgat: délelőtt laposabb, délután meredekebb (±22°), reggel és este legfeljebb 1,6-szor hosszabb; éjjel halvány holdárnyék ugyanebben az irányban. (Eddig délben egyenesen lefelé, reggel balra mutatott – ez ellentmondott a festett fénynek.)
**Épület és lelőhely:** oszloponkénti talp, mint a régiben: minden 3 texeles képcsík a saját legalsó pontján áll a földön (a lap betöltésekor számolva), így a tornyok árnyéka a tövüknél kezdődik, a lapos részek alig vetnek árnyékot; 0,72-szer rövidebb.
**Fa:** a törzs tövétől vetül (a 160/255-nél erősebb rész alja), és a festett, áttetsző talajfolt nem vet árnyékot.
**Árnyékmaszk:** az árnyékok fél felbontású külön maszkba kerülnek maximum-keveréssel – az egymást fedő árnyékok (az oszlopcsíkok, a tömeg) nem sötétednek egymásra, a kicsinyítés puhít –, majd egy menetben sötétítik a terepet.
**Belső árnyék** minden figurán (egység, épület, fa, lelőhely), a sprite-shaderben: a fénytől elforduló (jobb alsó) belső szélek mentén sötétedés (a maszk 22%-kal bal-fel tolva, erősen elmosva), a talp felé erősebb, plusz az alsó rész lágy sötétedése; legsötétebb 190/255 (a régi számok).
**Kliens:** `--look X,Y` (a kamera középpontja csempében, a képpróbákhoz).
**Mérés (RX 550, 1280 × 720, 2 × 800 egység csatában, nappal, zivatarban):** a képkocka p50 6,9 ms, p99 11,6 ms (eddig p99 ~7 ms) – a belső árnyék és a maszk ~3–4 ms, a 16,7 ms-os kapun belül.
**Tesztek:** 75 (a napszak-teszt az új iránnyal: délben a régi vetület, mindig jobbra le, reggel laposabb, este meredekebb). A világ-képpróbák referenciaképei szándékosan frissülnek.
**Kiegészítés (a tulajdonos):** „nem kell, hogy az árnyék mindig ugyanott legyen (napszak szerint vándorolhat), csak a logikát tartsd meg” – az irány ezért tágabban vándorol: reggel balra le (150°), délben pontosan a régi irány, este jobbra (10°); a rajzolás logikája (talpvonal, oszloponkénti talp, fák, maszk, belső árnyék) a régi.

## 2026-11-13 – „Nyugodt + belső átlag” réteg (a régi `rts/calm.py` szerint)

**A tulajdonos kérése:** „mindenen volt egy »nyugodt belső átlag« extra réteg”; és hogy ne rajzolódjon újra nagyításkor, csak betöltéskor. **Megoldás:** az atlaszkészítő a játéktér figuráinak (egység, épület, nyersanyag, tereptárgy; a csempék, a matricák és a felület nem) képkockáit a csomagolás előtt egyszer szűri (`render/calm`); a játék csak a kész atlaszt tölti be, futás közben nincs költség. A recept a régi: (1) a belső részen (5 × 5-ös alfa-minimum > 220) 3 × 3-as medián, élvédelemmel; (2) belső tinta: a simított világosság helyi maximumánál sötétebb pontok halk, lilásfekete (34, 23, 36) tintát kapnak; (3) 1 képpontos külső körvonal a sziluett köré, a helyi belső színből (0,40 × átlag + (8, 6, 8)). Eltérés: a régi a kijelzett méretben számolt (a körvonal mindig 1 képernyő-képpont), itt 1 HD-texel (nagyítástól függően ~0,5–1 képernyő-képpont). `totu_atlas --no-calm`: szűrő nélkül. Az atlasz építése 95 s → 175 s (egyszeri, gyorsítótárazott).
**Hiba közben:** a körvonal a már elkészült körvonalpontokat is tömörnek vette (a háttér fekete színét keverte be) – a régi kód szerint a tömörség a változtatás előtti alfából számol; a próba ezt fogta meg.
**Tesztek:** 76 (1 új: a körvonal színe pontosan a régi képlet szerinti, a halk belső zaj simul, az erős folt – él – megmarad, a háttér átlátszó marad; a szintetikus atlaszpróba szűrő nélkül pixelre egyezik).
**Referenciaképek:** az atlasz-, a nappali világ- és a gazdasági képpróba referenciája a szűrő és az új árnyékok miatt frissítve (a CI Mesa-képeiből, átnézve).

## 2026-11-13 – M6 3. lépés: építés

**A régi játék szerint** (G 1464–1491, 2078–2098, 2223–2330, 3439–3466; E 1060–1150, 1660–1705): a lerakás feltételei – a pályán belül, építhető terep, szabad hely (épület, lelőhely nélkül), előfeltételek (`Requirements.all`: kész épület, kutatás, fejlődési irány, felemelkedés) –, plusz a D33 szerint szirtre és szintkülönbségre nem építhető. A költség a parancskor levonódik; a munkás odamegy (0,8 csempés közelség), lerakja a félkész épületet (10% életerő; az alá került egységek a legközelebbi szabad helyre kerülnek), és a népe stílusa szerint épít (MECH-BUILD: `stay` 1 / 1,5 / 2 / 2,5 sebesség legfeljebb 4 építővel, `stay_single`, `enter`, `consume`, `leave`). **Ursan (`enter`):** a munkás bemászik az épületbe (nem látszik, nem akadály, nem célpont – a teste kikerül a navigációból), csak bentről épít, egyszerre egy; készen kijön a lábnyom alá a legközelebbi szabad helyre, és ha előtte gyűjtött, oda megy vissza. Az életerő a haladással nő (10% + 90% × haladás, pontos egész lépésekben). A félkész épület nem ad létszámot és nem leadóhely. Új parancsok: `Build` (sablon, csempe), `CancelBuild`. Az új parancs a még le nem rakott terv teljes költségét visszaadja; a bent építő nem kap parancsot.
**Eltérés a régitől (D38, a tulajdonos döntése):** a régi játékban nem lehetett épületet megszakítani vagy lebontani; most a félkész épület megszakítása a költség 75%-át adja vissza (`game.json` → `build_cancel_refund`), kész épület lebontása nincs.
**Kliens:** a félkész épület az „építési” animáció haladás szerinti fázisát mutatja; ideiglenes építési mód a HUD-ig (B: az ursan épületek léptetése áttetsző kísértetképpel, bal gomb: lerakás az első kijelölt munkással, jobb gomb: kilépés); `--build-demo` (képpróba).
**Mellékes javítás:** az `engine/CMakeLists.txt` 10 megjegyzéssorának ékezetei kétszeresen kódolva voltak (az M4 4. lépése óta) – visszaállítva.
**Tesztek:** 80 (4 új, `BUILD-ALAP` próbapálya tóval, magaslattal és erdővel): lerakhatóság (víz, szirt és szint, foglalt hely, pálya széle, előfeltétel); Ursan – fizetés, belépés, a tanya pontosan 30 s (899 tick) alatt épül, az életerő monoton 10%-ról 100%-ra nő, utána a létszám 12 → 22, a munkás kijön a lábnyomon kívülre és visszamegy gyűjteni; a fatelep elkészülte után a fát oda hordják; nincs elég nyersanyag – nincs levonás; megszakítás 75%-kal, a hely újra járható; mozgásparancs a tervezett építés teljes költségét visszaadja; determinizmus, mentésből folytatva ugyanaz, rögzített építési hash (`0x1f70ee5542492113`). A gazdasági hash a bővült állapot miatt újra rögzítve (`0x18f2f06dfb3f2a4f`); a golden M2–M4 változatlan.

## 2026-11-13 – D38 kiegészítve: lebontás egyes népeknél

**A tulajdonos:** kész épület bizonyos népeknél lebontható (a Wullof összecsomagol és odébb mehet, közben az épület nem működik), de nem mindegyiknél; a költség 70%-a járjon vissza. A választás: **Ursanok, Ooni, Wullof**. **Megvalósítás:** a nép adatában `demolish: true` (`factions.json`), `game.json` → `demolish_refund: 0.7`; új parancs: `Demolish` (csak kész épületre és csak a jelölt népnél; a hely utána járható). A Wullof összecsomagolása (MECH-WUL-4) önálló faji mechanika, az M7-ben. **Teszt:** 81 (1 új: az Ursan Hosszúház bontása 70%-ot ad vissza, a félkész nem bontható, az Aurethi Birodalom épülete nem bontható).

## 2026-11-13 – M6 4. lépés: termelés

**A régi játék szerint** (G 3486–3622, 3090–3104, 3160–3219; E 1738–1757): épületenként termelési sor (`Producer`; legfeljebb 5 elem, mindig az első halad), egység és kutatás ugyanabban a sorban. A **Train** a saját, kész, a sablont képző épületek közül a legrövidebb sorút választja (egyenlőnél a kisebb azonosítót); ellenőrzi az előfeltételeket, a költséget és a létszámot (a sorban lévő egységek is foglalnak – MECH-SUPPLY); a költség és a létszám azonnal foglalódik. **Hős:** egyszerre legfeljebb 3, és ugyanaz csak egyszer (élve vagy a sorban). **A kész egység** a lábnyom körüli helyjelöltek (oldalak csempéinek közepe és a sarkok) közül a gyülekezőponthoz – anélkül lefelé – legközelebbi szabad, járható helyen jelenik meg; ha nincs szabad hely, a következő tickben újra próbálja. **Gyülekezőpont** (`Rally`): az új egység odamegy; ha lelőhely és gyűjthet, gyűjteni kezd. **Visszavonás** (`CancelQueue`): a teljes költség vissza. **Kutatás** (`Research`): a kutatássablonokból (költség, idő, előfeltételek); kész után a játékos kész kutatásai közé kerül (a hatások a 7. lépésben). `add_resources` (küldetés, szkript, teszt). Ideiglenesen a kliensben a Q telepest képez (a HUD a 9. lépésben).
**Tesztek:** 85 (4 új): telepes – fizetés, létszám azonnal 6, pontosan 12 s (360 tick) múlva a főépület alatt; a sor legfeljebb 5, a visszavonás teljes költséget és létszámot ad vissza, a létszámkorlát megállít, előfeltétel nélkül nem képezhető; gyülekezőpont – az egység odamegy, fára küldve gyűjt, két kaszárnyánál a rövidebb sor kapja; a kutatás pontosan 60 s alatt kész, kétszer nem indítható; a hős csak egyszer. A gazdasági és az építési állapothash az új termelési komponens miatt újra rögzítve (`0xb5c68427e625c3f9`, `0xa8351b5df8af78e7`); a golden M2–M4 változatlan.
