# Hibák és feladatok

| ID | Szak. | Prior. | Leírás | Reprodukció / bizonyíték | Státusz |
|---|---|---|---|---|---|
| H-001 | S01 | mérés | `--simulate --seed N` faj megadása nélkül futásonként más fajpárt választott → a mérés nem volt megismételhető | `python main.py --simulate 2 --seed 7` kétszer | ELLENŐRZÖTT (javítva) |
| F-001 | S01 | alap | Visszaállítható kiindulópont: nincs verziókezelés | helyi Git, `ffe4893` | ELLENŐRZÖTT |
| F-002 | S01 | alap | Ablakos kézi próba: indítás → menü → pálya → parancs → kilépés | automatikus renderpróba OK (`--screenshot`); a menüs, valódi egérrel végzett próba a felhasználóra vár | FOLYAMATBAN |
| H-003b | S01 | eszköz | `--screenshot` tesztmódban fekete játéktér (szélgörgetés dummy egérrel) | MUNKANAPLO | ELLENŐRZÖTT (javítva) |

| H-003 | S08 | AI | Croftian–Ooni (seed 7) meccsben az Ooni 2–5. perc között 9–11 egységen megreked, a 6. percben elveszít mindent. Egyetlen futás, ok még ismeretlen | `--simulate 8 --seed 7 --faction croftian --enemy ooni` | TERVEZETT (vizsgálandó) |
| F-003 | S02 | mérés | Szimulációs sebesség nagyon eltér meccsenként (6 perc játék 41 mp vs. 7,3 perc 162 mp, render nélkül) – profilozandó | lásd MUNKANAPLO | TERVEZETT |
| H-004 | S01 | összeomlás | 8 játékosos gépi meccsben `AttributeError: Unit.wait_t` a `game.py` `_unit_path` (482) sorában: a `wait_t`-t csak a `set_order` állította be | `tools/perf_probe.py --map nyolc_tuz --players 8 --seed 1` | ELLENŐRZÖTT (osztályszintű alapérték; az ismételt 12 perces 8P futás hibátlan) |
| P-001 | S02 | teljesítmény | Az A* minden hívásban újraszámolta a csempék járhatóságát (`fits` 12,4 M hívás / 4 perc) | profil, MUNKANAPLO | ELLENŐRZÖTT (útkeresés −31%, hash azonos) |
| P-002 | S02 | teljesítmény | Útkeresés még a tickidő 69%-a 8P-ben: elérhetetlen célok teljes keresése, torlódási újratervezés | 8P mérés 20261111_204122 | TERVEZETT |
| H-005 | F9 | kampány | `f3_a_nevtelen_hajnal`: a `cro_kaptar` (A Kórus magja) nem helyezhető el (-4, 46) – pályán kívüli koordináta | `tools/snapshot_check.py --all-missions` kimenete | TERVEZETT |
| H-006 | F4b | determinizmus | `Entity.mod` a bekapcsolt képességek módosítóit szöveges set sorrendjében szorozta össze; a szöveg-hash folyamatonként véletlen, így 3+ képességnél a lebegőpontos eredmény futásonként eltérhetett | felmérés (P0) | ELLENŐRZÖTT (rendezett bejárás) |
| H-002 | F2 | biztonság | pickle-mentés | – | ELLENŐRZÖTT (JSON-mentés, `.tsav`) |
| F-004 | S04 | döntés | `sim_hz` 30 vs. követelmény 20 Hz | `data/game.json` | BLOKKOLT (döntés) |
