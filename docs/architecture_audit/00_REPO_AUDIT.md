# TOTU Sky Rift – repó- és funkcióaudit a C++/C# döntéshez

Kelt: 2026-11-11 · commit: `320d42e` (helyi Git) · vizsgáló környezet: Windows 10, i7-6700K, Radeon RX 550, 32 GB, Python 3.13.14, pygame-ce 2.5.8, numpy 2.5.4.
Állapotjelek: **IMPLEMENTÁLT** (kód + futtatott ellenőrzés) · **IMPLEMENTÁLT, NEM FUTTATOTT** (kód van, itt nem próbáltam) · **RÉSZLEGES** · **NINCS** · **ISMERETLEN**.

## 1. Méret és szerkezet

| Réteg | Fájlok | Sor | pygame-függés |
|---|---|---:|---|
| Szimuláció: `game.py` 3696, `entities.py` 1867, `ai.py` 886, `heroes.py` 460, `mapgen.py` 520, `mission.py` 351, `world.py` 266, `pathfinding.py` 175, `points.py` 183 | 9 | ~8 400 | **nincs** (csak `math`, `random`, `collections`) |
| Adat/mod/hálózat/mentés: `data.py`, `modmgr.py`, `net.py`, `savegame.py`, `campaign.py`, `profiles.py`, `i18n.py` | 7 | ~2 400 | csak `savegame.py` (bélyegkép) |
| Kliens: `ui.py` 4561, `menu.py` 2386, `hud.py` 1613, `setup_screen.py`, `vfx.py`, `scenery.py`, `render.py`, `sprites.py`, `faction_ui.py`, `buildfx.py`, `shadows.py`, `canvas.py`, `audio.py`, `calm.py`, `fonts.py`, `tips.py` + `main.py` | 17 | ~13 300 | igen |
| Adat | `data/` (22 fajfájl, abilities, items, camps, decor, elements, 20 pálya), `campaigns/` 58 JSON | – | – |
| Asset | `assets/` 2734 PNG, 1279 hang, ~1,2 GB | – | – |
| Eszközök | `tools/` ~70 szkript (generátorok, importálók, balansz, HUD-teszt, `perf_probe.py`) | – | – |

**Fontos tény:** a szimuláció már most grafikamentes (`python main.py --simulate` ablak nélkül fut). Ez minden migrációs útnál a legértékesebb meglévő határ.

## 2. Függőségek és licenc

| Elem | Tény | Bizonyíték |
|---|---|---|
| Kódlicenc | **MIT** („Copyright (c) 2026 Mesehordo”), **nem GPLv2+** | `LICENSE.md` 2. pont |
| Világ, történet, szövegek | © Mesehordo, minden jog fenntartva | `LICENSE.md` 1. pont |
| Grafika | CC BY 4.0 (README) | `README.md` fejléc |
| pygame-ce | LGPL-2.1, dinamikusan linkelve | pip csomag |
| numpy | BSD-3 | pip csomag |

**Ellentmondás a C++/C# tanulmánnyal:** a v1.1 csomag „GPLv2+ TOTU-kódot” feltételez, és erre építi a MonoGame (Ms-PL) kizárását. A repó tényleges licence MIT, így ez az indok nem áll fenn. Döntés kell: marad-e az MIT (→ a MonoGame licencakadálya megszűnik), vagy a tulajdonos GPL-re akar váltani.

## 3. Funkciómegőrzési mátrix

| Terület | Állapot | Bizonyíték / megjegyzés |
|---|---|---|
| 7 játszható főfaj (elfek, ursanok, croftianok, wullof, Vasbirodalom, Huk’ucha, Ooni) | IMPLEMENTÁLT | `data/factions/*.json`, `data.faction_list()`; gép–gép meccsek lefutnak |
| Micéliák mint teljes nép | IMPLEMENTÁLT: rejtett alapfaj (csak a Gombanép-hadjáratban); a `micelia` példa-mod egyéni csatára is feloldja | `data/factions/mycel.json` (`hidden: true`), `mods/micelia/factions/mycel.json` |
| 13 kisfaj | IMPLEMENTÁLT, NEM FUTTATOTT – szövetségi csomagként, nem önálló főfajként: 5–6 egység, 3 épület fajonként | `data/factions/ally_*.json`, `game.py` `cmd_choose_ally` (689), `ally_ready` (686) |
| Fajszabályok | adatvezérelt: 39 `rules` kulcs; a fajazonosító csak ~25 helyen van beégetve | `grep rules.get` a `rts/` alatt |
| Gazdaság: fa/kő/meteorit, létszám, felemelkedés, 3 fejlődési irány | IMPLEMENTÁLT | README, `game.py` `_check_ascension`, `cmd_choose_path` |
| Parancsok: move, attack-move, attack, smart, stop, hold, build, sorba állítás (`queue`), 20+ fajspecifikus (`cmd_garrison`, `cmd_morph`, `cmd_merge`, `cmd_pack` …) | IMPLEMENTÁLT | `game.py` 3298–3430 és `cmd_*` |
| Patrol | NINCS (nem találtam) | – |
| HUD 4×3 parancsrács, billentyű = kattintás, vezérlőcsoportok | IMPLEMENTÁLT | `tools/test_hud.py` – minden pont OK |
| Hősök (3/faj, újraélesztés, tárgyak, aurák), szörnytáborok, drop | IMPLEMENTÁLT, NEM FUTTATOTT | `heroes.py` |
| Útkeresés | A* csempén + simítás + lokális kitérés; méretfüggő `fits(radius)`; útkérés-sor tickenkénti kerettel | `pathfinding.py`, `game.py` `_process_paths` (427) |
| Flow field, HPA*, útvonal-gyorsítótár | NINCS | – |
| Fog of war | IMPLEMENTÁLT: csempénkénti látható/felderített bytearray, 6 tickenként; lopakodás | `game.py` `_update_vis` (3033), `_update_stealth` |
| Gépi ellenfél | IMPLEMENTÁLT, egyszintű szabályalapú; könnyű/közepes/nehéz | `ai.py` |
| Mentés/betöltés | IMPLEMENTÁLT, **pickle** a teljes `Game` objektumról, RNG-állapottal | `savegame.py` 66, 180 |
| Replay | NINCS | `grep replay` → 0 találat |
| LAN | IMPLEMENTÁLT, NEM FUTTATOTT: **lockstep**, TCP-n soronkénti JSON, 100 ms-os körök, CRC32 állapothash, szinkronhiba-jelzés, kilépőt az AI veszi át | `net.py` 142–154, 574–640 |
| Internetes játék, dedikált szerver, FOW-szűrt kliensnézet | NINCS (lockstepnél minden kliens a teljes állapotot ismeri) | `net.py` |
| Kampány: 12 kampány, 46 küldetés; célok: reach 34, survive 17, destroy 16, guard 14, build_at 11, escort 10, kill 7, gather 6 … | IMPLEMENTÁLT, NEM FUTTATOTT – JSON-alapú, nem Python | `campaigns/`, `campaign.py`, `mission.py` |
| Modok | JSON-felülírás + **korlátlan Python `script.py`** hookokkal (`on_load`, `on_game_start`, `on_tick`) | `data.py` 204–209 |
| Determinizmus | ugyanazon a gépen igazolva (két 8 perces futás azonos); a szimuláció a globális `random`-ot használja, a kliens saját `random.Random` példányokat | `game.py` 124, `ui.py` 129 |
| Automata tesztek | csak `tools/test_hud.py`; nincs CI | – |
| Platformfüggő kód | gyakorlatilag nincs (nincs `windll`, `winreg`, `startfile` a játékkódban) | grep |

## 4. Mért teljesítmény (Python baseline)

| Próba | Eredmény | Forrás |
|---|---|---|
| 2 játékos, generált pálya, croftian–ooni, seed 7, 30 Hz | 72 egység csúcs; tick p50 1,5 / p95 23,9 / p99 43,1 / max 115 ms; 196/10920 tick > 33,3 ms. Útkeresés 51%, egységfrissítés 25%, szétválasztás 12%, FOW 3%, AI 1% | `docs/fejlesztes/meresek/20261111_192521_S02-SKIRMISH-2P_s7.json` |
| 8 játékos – lásd `PORTING_DECISION.md` 3. pont | | |

A render-idő még nincs mérve.
