# Alapállapot és kockázatok (anti-slop csomag, P0)

Kelt: 2026-11-18 · alap: `main`, HEAD `c840c29` · munkamód: a tulajdonos döntése szerint (D46) a `main`-en, kis
commitokban, a tiltott-szó kapuval; minden lépés a [MUNKANAPLO](../fejlesztes/MUNKANAPLO.md)-ban és az
[IMPLEMENTATION_LOG](IMPLEMENTATION_LOG.md)-ban.

## 1. Környezet

Windows 10, i7-6700K, Radeon RX 550 (OpenGL 3.3 core), 32 GB; GCC 16.2 (WinLibs UCRT), CMake 4.4, SDL 3.4.
Két fordítási mappa: a mag és a tesztek (`TOTU_CLIENT=OFF`), illetve a kliens (`totu`).

## 2. Build és tesztek

| Cél | Eredmény |
|---|---|
| `totu_core`, `totu_tests`, `totu`, `totu_relay`, `totu_netbench` | fordul, hiba és új figyelmeztetés nélkül |
| CTest (9): `core`, `net_loss_rovid`, `golden_replay` (M2), `golden_replay_m3`, `_m4`, `_m6`, `_m7`, `ai_batch_rovid`, `ai_batch_alap` | **9/9 PASS** (≈ 360 s) |
| `totu_tests` (a `core` része) | PASS; 364+ próba |
| Golden visszajátszások: `engine/tests/data/golden_m2/m3/m4/m6/m7.trep` | bitre azonos hash (a CTest ellenőrzi) |
| CI: Windows, Linux, macOS Intel és Apple Silicon | a GitHub-munkafolyamat szerint; **helyben NOT_RUN** |
| FreeBSD, OpenBSD | **NOT_RUN** (a CI-ben még nincs – nyitott feladat) |
| `PREEXISTING_FAILURE` | nincs |

## 3. Képek (`kepek/`)

`alap_hud_ursan_1280.jpg`, `_1920x1080`, `_2560x1440`: meccs a `csatamezo` pályán (`--world … --build-demo`);
`alap_menu_fo`, `_skirmish`, `_settings`, `_sound`, `_keys` (1280×720). A HUD mindhárom felbontásban arányosan nő,
nincs levágás. **A hét faj HUD-képe parancssorból nem készíthető**: a `--faction` csak a hálózati meccsre hat, a
`--world` meccs mindig ursan–ursan (a fajok HUD-ját a `test_hud` geometriai próbái fedik) – lásd AS-08.

## 4. Teljesítmény

Képkockaidő és ticktartam erre az állapotra: **NOT_MEASURED** (a `--bench` és a `totu_netbench` létezik; a mérés a
diagnosztikai csomaggal együtt, azonos gépen, előtte–utána készül). Korábbi mérések: `docs/fejlesztes/meresek/`.

## 5. Ténylegesen meglévő részek

Főmenü (Egyjátékos → Csata, Beállítások, Hang, Billentyűk; Többjátékos/Történetek/Modok/Készítők „hamarosan”), Csata
beállító képernyő, játékbeli HUD (kötött panelsor, 4×3 parancsrács WERT/SDFG/XCVB, 2×3 tárgyrács, 4×3 kijelölés),
küldetésnapló, párbeszéd, átvezető, szünetmenü, győzelmi/HUD-módkijelzés, küldetéssor-választó, világszerkesztő
(F4 trigger-, F5 adatszerkesztő), hálózat konzolos lobbival (`--host`/`--join`), relé. **Nincs:** menübeli lobbi,
modkezelő képernyő, eredményképernyő statisztikával, diagnosztikai export, naplófájl (a hibák a `stderr`-re mennek).

## 6. Elavult dokumentumok

| Dokumentum | Mit állít | Valóság |
|---|---|---|
| `docs/fejlesztes/ALLAPOT.md` | „M2 kész, következik az M3”; Python-tesztek | az M0–M12 kész (M12 a hadjárat nélkül) |
| `docs/fejlesztes/HIBAK_ES_FELADATOK.md` | a Python-játék hibái (H-003, P-002, H-005 …) | a régi kódra vonatkoznak; az új motor hibái a naplóban |
| `README.md` „Állapot” táblázat | „M3–M14 tervezett” | M3–M12 kész |
| `docs/fejlesztes/REPO_TERKEP.md` | a régi mappák (`campaigns/`, `mods/`, `saves/`) | a tartalom az `engine/content/` alatt |

## 7. Kockázatok és védett invariánsok

- A szimuláció, a hash, a mentés (19), a visszajátszás (8), a hálózati protokoll, a tartalomkulcsok, a HUD-elrendezés és
  a gyorsbillentyűk **nem változnak** ebben a csomagban; minden javítás a kliens megjelenítő/bemeneti rétegében marad.
- Kapu minden commit előtt: a 9 CTest zöld, a goldenek bitre azonosak, a változott képernyőről előtte–utána kép.
- Visszaállítás: minden javítás külön commit, `git revert <sha>` elég.
