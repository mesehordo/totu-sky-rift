# TOTU: Sky Rift – fejlesztési alapdokumentum

Kelt: 2026-11-11 · kódállapot: helyi Git, `d986749`.
**Ez az egyetlen irányadó fejlesztési dokumentum.** A 67 korábbi tanulmányból azt tartja meg, ami a valódi játékra érvényes, a kóddal összevetve. A tanulmányok háttéranyagok, státuszukat a [TANULMANY_LELTAR.md](TANULMANY_LELTAR.md) adja. A nyitott kérdések a [DONTESEK.md](DONTESEK.md) fájlban vannak.

## 1. Dokumentumrend

| Elsőbbség | Forrás | Mire irányadó |
|---|---|---|
| 1 | A tulajdonos döntései (`DONTESEK.md`, ELDÖNTVE sorok) | termék, irány, kánon, licenc |
| 2 | Kánon: `docs/tortenet/FOFAJOK.md`, `SZOVETSEGES_NEPEK.md`, `docs/tortenet/story/`, `novellak/` | világ, fajok, egységek, történet |
| 3 | Ez a dokumentum | követelmények, sorrend, tesztek |
| 4 | A kód és a mérések (`docs/fejlesztes/meresek/`) | mi működik ténylegesen |
| 5 | A tanulmányok | háttér, ötletek, módszertan |

Ha a kód eltér ettől a dokumentumtól, a kód mutatja a valóságot, ez pedig a célt. Az eltérés feladat lesz, nem tévedés. A `README.md` és a `DESIGN.md` részben elavult (pl. „kő” nyersanyag); ezeket egy későbbi feladat frissíti.

## 2. Mi a TOTU – rögzített tények

- **Műfaj:** klasszikus, 2D sprite-os, 3/4 nézetű fantasy RTS; bázisépítés, hősök, tárgyak, kampány.
- **Fajok:** 7 játszható főfaj (Szürke elfek, Ursanok, Croftianok, Wullof, Aurethi Birodalom, Huk’ucha, Ooni) és 13 szövetséges kisfaj (Micéliák, Pinguu, Ghoblin, Mhonyo, Daresh, Tokla, Galóca, Niil-elf, Tűz-elf, Tendüür, Nila-uchana, Zhan-zhama, Shrissh-matah). A kisfajok nem önálló felek: a főfaj egyet választhat közülük, és annak épületei nyílnak meg (D11).
- **Nyersanyagok:** Fa, Arany (kódban `stone`), Meteorit, Áhítat, valamint létszám (populáció, max. 200/játékos).
- **Játékosszám:** 1–8; jelenleg egyjátékos, gép elleni és LAN.
- **Tartalom:** 46 kampányküldetés 12 kampányban, 20 csatapálya (2–8 játékos), magyar és angol nyelv.
- **Technika ma:** Python 3.13 + pygame-ce; a szimuláció grafikamentes; 30 Hz; adatvezérelt JSON-tartalom; JSON- és Python-modok.
- **Irány (D18–D20, 2026-11-12):** **új motor nulláról, C++20, Lua-szkripttel**, 0 A.D. + OpenRA szintű modolással, WC3-szintű küldetésekkel, győzelmi feltételekkel és hadjárattérképpel – **terv és ütemterv: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md)** (M0–M14). A Python-játék futtatható referencia marad. (A korábbi C#-irány – ADR-001, PORT_TERV – felülírva.)
- **Platform (D3, módosítva):** Windows, Linux, macOS (Intel, Apple Silicon), FreeBSD, OpenBSD – mint a 0 A.D.
- **Hálózat (D4, D5, D15):** szerverautoritatív LAN, utána internetes dedikált szerver, rangsorolt versennyel.
- **Licenc (D8):** kód GPL-3.0-or-later (`COPYING`); világ, történet és grafika © Mesehordo, minden jog fenntartva (a TOTU-modok a játékon belül megjeleníthetik a grafikát); szabályadatok és modding-dokumentáció CC BY 4.0.

## 3. Követelményregiszter

Jelek: **✔** megvan és ellenőrzött · **◐** megvan, de javítandó vagy nem ellenőrzött · **✖** hiányzik, tervezett · **?** döntés kell · **⏳** később · **✗** elvetve vagy téves.

### 3.1 Szimuláció és teljesítmény

| Követelmény | Állapot | Bizonyíték / teendő |
|---|---|---|
| A szimuláció nem importál grafikát | ✔ | `rts/game.py`, `entities.py`, `ai.py` … pygame nélkül; `--simulate` ablak nélkül fut |
| Fix időlépés, FPS-független szabályok | ✔ | `sim_hz` 30, `Game.update(dt)` fix lépéssel |
| Azonos seed + parancsok → azonos állapot | ✔ (egy gépen, folyamatok között is) | `tools/golden_replays.py`, eltérő `PYTHONHASHSEED`-del is; más gépen nem ellenőrzött |
| Nincs globális `MAX_UNITS` | ✔ | grep: nincs |
| 20 Hz-es szimuláció | ✗ | D6: a 30 Hz marad |
| Tickidő a keretben (p95 ≤ 25 ms, p99 ≤ 40 ms az 50 ms-os keretből; 30 Hz-en ennek megfelelően a 33,3 ms-os keret tartása) | ◐ | 2P/72 egység: p99 43 ms; 8P/300 egység: p95 166, p99 243 ms → **F2 fázis** |
| Logikai tick nem dobható el; a lassulás mérendő | ✔ | a kód nem dob el ticket; a hátralékot még nem méri |
| Lépcsőzetes terhelési próbák 250 → 500 → 800 → 1600 | ◐ | `tools/perf_probe.py` megvan; egységszám szerinti forgatókönyv még nincs |
| Külön mért POP- és ENTITÁS-terhelés | ✖ | a `perf_probe` egységet számol; a pop-összeg és az entitástípusok külön mezőt kapnak |

### 3.2 Navigáció

| Követelmény | Állapot | Bizonyíték / teendő |
|---|---|---|
| Átlós lépés csak akkor, ha mindkét oldalcella szabad a lábnyomnak | ✔ | `pathfinding.py` `find_path`: `valid(nx, y)` és `valid(x, ny)` |
| Méretfüggő járhatóság (clearance) | ✔ | `fits(radius)`; P-001 óta sugaranként gyorsítótárazva |
| Ütemezett útkeresés (tickenkénti keret) | ✔ | `_process_paths(budget=6)` |
| Dinamikus térbeli index | ✔ | `_rebuild_hash`, `collision_candidates` |
| Helyi kitérés, torlódáskezelés, formációs megállás | ◐ | `local_detour`, `_settle_formation`; minőségét nem mértük |
| Elérhetetlen cél gyors felismerése | ✖ | ma 9000 csomópontig keres → **P-002** |
| HPA* / flow field / ORCA | ⏳ | csak mért szükség esetén, ADR-rel |
| NAV-BRIDGE, NAV-MAMMOTH, NAV-CHOKE tesztpálya | ✖ | F3 fázis |

### 3.3 Játékmenet

| Követelmény | Állapot | Bizonyíték / teendő |
|---|---|---|
| Alapparancsok: mozgás, támadás, attack-move, állj, tart, építés, sorba állítás (shift), rally, javítás, képesség, kutatás | ✔ | `game.py` `cmd_*`, HUD-teszt |
| Patrol | ✖ | D13: bekerül |
| Stance (agresszív / védekező / helytartás) | ✖ | D13: bekerül |
| Vezérlőcsoportok (mentés, visszahívás) | ✔ | `ui.py` `groups` |
| A játékos parancsát automatika nem írja felül | ◐ | HUD-teszt részben; külön próba kell (S03) |
| Hősök, tárgyak, szörnytáborok, drop | ◐ | `heroes.py`; nincs rá teszt |
| Kisfaj-szövetség (1 nép, 3 épület – D11) | ◐ | `cmd_choose_ally`; nincs rá teszt |
| Gépi ellenfél (3 nehézség) | ◐ | `ai.py`; megfigyelés: az Ooni megrekedhet (H-003) |
| Az AI csak felderített információt használ | ◐ | nem vizsgált (F7: AI-FOG) |
| Fog of war (ismeretlen / felderített / látható, lopakodás) | ✔ | `_update_vis`, `_update_stealth` |

### 3.4 Kezelőfelület

| Követelmény | Állapot | Bizonyíték |
|---|---|---|
| HUD sorrend, 2×3 tárgy, 4×3 kijelölés, 4×3 parancs (WERT/SDFG/XCVB) | ✔ | `tools/test_hud.py` |
| Egyetlen háromfigurás Szövetségesek-gomb, fogaskerék-menü | ◐ | a felső sávban van; teszt nincs rá |
| Billentyű = kattintás; szövegbevitel és szünet alatt nincs parancs | ✔ | `test_hud.py` |
| 720p / 1080p / 1440p elrendezés | ✔ | `test_hud.py` |
| Színtévesztő-barát jelölés | ⏳ | – |

### 3.5 Hálózat

| Követelmény | Állapot | Bizonyíték / teendő |
|---|---|---|
| LAN 2–8 játékos, lobbi, csevegés | ◐ | `net.py` lockstep; két gépen nem próbáltuk |
| Szinkronhiba-észlelés | ✔ (kódban) | CRC32 állapothash |
| Szerverautoritatív modell, FOW-szűrt kliensnézet | ✖ | D4: a LAN is erre áll át |
| Internetes játék, dedikált szerver, rejoin | ✖ | D5: közvetlenül a LAN után |
| Vegyes OS lockstep | ✗ | szerverautoritatív modellben nem szükséges |

### 3.6 Mentés, modok, biztonság

| Követelmény | Állapot | Bizonyíték / teendő |
|---|---|---|
| Mentés/betöltés a teljes szimulációval | ✔ | `.tsav` JSON; oda-vissza azonos: skirmish, 8P, 46/46 küldetés; ablakban még nem próbáltuk |
| Verziózott, biztonságos mentésformátum | ✔ | D10: `rts/snapshot.py`, séma 1; 9 régi mentés átalakítva; hamisítás elutasítva (`tools/test_snapshot.py`) |
| Replay (parancsnapló) | ◐ | `.trep`, egyéni csatában automatikus; kampányban és LAN-on még nincs |
| Adatmodok (faj, egység, pálya) | ✔ | `mods/`, `data.py` |
| 0 A.D.-szintű modolás: sandboxolt szkript, modkezelő, függőségek, modlock | ✖ | D9; ma korlátlan Python |
| Tartalomhash a LAN-lobbiban | ✔ | `net.py` 40 |

### 3.7 Platform és kiadás

| Követelmény | Állapot | Teendő |
|---|---|---|
| Windows | ✔ | – |
| Linux | ✖ | D3: cél; a kód hordozható, de nem próbáltuk |
| macOS (Intel, Apple Silicon) | ⏳ | D3: ha lesz tesztgép |
| Telepítőcsomag (PyInstaller vagy hasonló) | ✖ | F8 |
| Minimum gép igazolása | ✗ | D14: nincs tesztgép; „ajánlott” gépigény mért adatból |

### 3.8 Tartalom

| Követelmény | Állapot | Megjegyzés |
|---|---|---|
| Kampány a kánon szerint | ◐ | 46 küldetés; végigjátszás-ellenőrzés nincs |
| Küldetések JSON-ból, nem Pythonból | ✔ | `campaigns/`, `mission.py` |
| Pályavalidátor (kezdőhelyek, elérhetőség) | ✖ | F7 |
| Tiled-import, pályaszerkesztő | ⏳ | – |
| Rangsorolt verseny | ✖ | D15: cél, az internetes mód után |

## 4. Tesztek és mérések

Meglévő eszközök:

| Eszköz | Mit igazol |
|---|---|
| `python tools/test_hud.py` | HUD-elrendezés, billentyűkezelés, kijelölés |
| `python main.py --simulate N --seed S [--faction --enemy]` | gép–gép meccs ablak nélkül, reprodukálható |
| `python tools/golden_hash.py …` | állapothash-sorozat; az optimalizálás bitre azonos viselkedését igazolja |
| `python tools/perf_probe.py …` | tickidő p50/p95/p99, alrendszer-bontás, memória, JSON |
| `python tools/balance.py --seeds N` | fajpárok győzelmi aránya |
| `python tools/golden_replays.py` | a rögzített felvételek változatlanul játszódnak le (minden szimulációs változtatás után) |
| `python tools/test_snapshot.py`, `tools/snapshot_check.py` | mentés oda-vissza, hamisítás elutasítása |

A tanulmányok kb. 60 tesztazonosítójából ezek a tervezett, a játékra érvényes tesztek, fázisonként:

| Fázis | Teszt |
|---|---|
| F2 | PERF-250/500/800 egységszámmal; MP-4x200-POP, MP-8x200-POP; tickhátralék mérése |
| F3 | NAV-DIAG (átló, mindkét oldal), NAV-BRIDGE (két sereg egy hídon), NAV-MAMMOTH (nagy egység szűk kapuban), NAV-CHOKE (útlezárás építéssel), munkások erőforrásnál |
| F4 | AI-ORDER (a Tart parancs nem sérül), AI-FOG (az AI nem lát a ködön át), ECON-BUILD (nincs elérhetetlen technológia) |
| F5 | SAVE-ROUNDTRIP (mentés → betöltés → azonos hash), REPLAY (parancsnaplóból azonos hash) |
| F6 | LAN-2 két gépen, LAN-DESYNC (szándékos eltérés jelzése), kilépő játékos AI-átvétele |
| F7 | MISSION-REACHABILITY (minden cél elérhető), MAP-TOPOLOGY (minden kezdőhelyről van út) |

Mérési szabály: minden eredményhez rögzítsük a commitot, a seedet, a pályát, a hardvert és a p50/p95/p99 értéket. Becslést tilos mért adatként beírni.

## 5. Ütemterv

> **2026-11-12:** az ütemtervet az [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) M0–M14 mérföldkövei váltják fel. Az alábbi F-fázisok közül az F1–F3 teljesült; az F4b/F4c helyére az M-mérföldkövek léptek, az F5–F9 tartalma az M10–M14-be került.

Minden fázis végén működő, kipróbálható játék legyen. Egyszerre egy feladat fut, és mindegyik a munkanaplóba kerül (`docs/fejlesztes/`). A döntések: [DONTESEK.md](DONTESEK.md).

| Fázis | Cél | Állapot | Kapu |
|---|---|---|---|
| **F1 Biztos alap** | verziókezelés, mérőeszközök, indulási és összeomlási hibák | nagyrészt kész (Git, `perf_probe`, `golden_hash`, H-001, H-004) | ablakos kézi próba: menü → csata → mentés → betöltés → kilépés |
| **F2 Viselkedési szerződés** | nyelvfüggetlen leírás: parancsséma, állapot-snapshot (JSON), parancsnapló/replay, golden replay-készlet, a mozgás/harc/gazdaság szabályai | kész: snapshot, mentés, replay, golden készlet ([SZERZODES.md](SZERZODES.md)), alapszabályok és az F3 közös feladata ([SZABALYOK.md](SZABALYOK.md)); nyitott: kampány-/LAN-felvétel, más gépen futtatott determinizmus | a Python-játék a snapshotból és a parancsnaplóból azonos hash-t ad; ez a D10 JSON-mentés alapja is |
| **F3 A/B mérés (D1)** | ugyanaz a szimulációs feladat (mozgás, A*, ütközés, egyszerű harc, FOW, 250 → 2400 entitás, 30 Hz) Pythonban, C#-ban és C++-ban; GC/alloc, p50/p95/p99 | kész: [eredmény](../fejlesztes/meresek/AB_EREDMENY.md); **[ADR-001](dontesek/ADR-001-motor.md) elfogadva: C# (.NET 10)** → az F4b ág fut | mért összehasonlítás és **ADR a motorirányról**. *Előfeltétel: .NET SDK, C++ fordító és CMake telepítése (engedélyedre)* |
| ~~F4a Ha a Python marad~~ | elvetve (ADR-001) | – | 8P/300 egység a keretben → 500 → 800 → 1600 pop |
| **F4b C#-port** | fokozatos port: szimulációs mag → szerver → kliens; a Python-játék a port végéig működik; minden modul a golden replay-ekkel azonos viselkedést mutat | terv kész: [PORT_TERV.md](PORT_TERV.md) (P0–P9); rajzolási próba kész; **P0 kész** (2026-11-12); következik: P1 | golden replay-egyezés, funkcióparitás modulonként; a kliens interpolál (P8) |
| **F4c Minőségi szakasz (D17)** | Q1 mozgás, Q2 nagy léptékű útkeresés, Q3 megjelenés (60 FPS), Q4 gépi ellenfél, Q5 szerkesztő; a Q6 hálózat = F5/F6 – [MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md) | a P6 után; a szabálybefagyasztás feloldva | [PORT_TERV.md](PORT_TERV.md) 5/A kapui |
| **F5 Szerverautoritatív LAN (D4; az indulás előtt újranézendő, D17)** | headless szerver, parancsvalidáció, FOW-szűrt kliensnézet, rejoin | – | LAN-2 → 4 → 8 kliens; FOW-LEAK = 0 |
| **F6 Internet (D5) + rangsorolt alapok (D15)** | dedikált szerver, lobbi, hálózati hibatűrés; később MMR, ranglista | – | NET-LOSS, NET-REJOIN, NET-SEC |
| **F7 Játékmenet** | patrol és stance (D13), AI-hibák (H-003), parancsprioritás | a Q4-be olvad (D17) | AI-ORDER, AI-FOG, kézi próba |
| **F8 Modolás (D9)** | sandboxolt szkript, modkezelő, függőségek, modlock a lobbiban | – | MOD-HASH, MOD-SANDBOX, egy példamod (új kisfaj + épület + képesség + 3 trigger) |
| **F9 Tartalom-QA és kiadás** | pálya- és küldetésvalidátor, kampány végigjátszása, Windows + Linux csomag | – | tiszta gépen telepíthető; MISSION- és MAP-tesztek |

A sorrend logikája: az F2 szerződés mindkét irányhoz kell. Az F3 dönti el, hogy az F4a vagy az F4b ág fut. A hálózatot (F5) és a modolást (F8) az ADR után a választott motoron építjük, hogy ne kelljen kétszer megírni. A kisebb Python-javítások (pl. összeomlás) közben is mehetnek.

## 6. Munkamódszer

A korábbi munkajegyzékből érvényben marad:
- egy feladat egy megfigyelhető eredményt adjon; először reprodukálni, aztán javítani;
- optimalizálás csak mérés után, előtte/utána azonos terheléssel; a viselkedés azonosságát a `golden_hash` igazolja;
- kánonhoz, balanszhoz és licenchez döntés nélkül nem nyúlunk;
- a nem ellenőrzött dolgot nem jelöljük késznek.

## 7. „NE” lista (a tanulmányokból, változatlanul érvényes)

1. Ne legyen globális `MAX_UNITS`.
2. Ne fusson minden egységre teljes A* minden tickben.
3. Ne vessen össze minden egység minden másikkal.
4. A teljesítményt ne a játékszabály titkos ritkításával „javítsuk” (tick, sebzés, látótér).
5. Ne legyen a fog of war csak fekete réteg a kliensen, ha egyszer online játék lesz.
6. A pálya- és küldetésfájl ne futtasson Pythont.
7. Ne hirdessünk minimumgépet vagy egységszámot mérés nélkül.
8. A natív kód nem segít, ha az algoritmus rossz.
9. Ne fordítsuk le a kódot fájlonként más nyelvre.
10. Ne gyártsunk tömegesen új grafikát, amíg a konvenció és a konkrét hiány nem ismert.
