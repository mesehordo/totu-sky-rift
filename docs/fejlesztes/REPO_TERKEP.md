# Repótérkép (S00, 2026-11-11)

**Aktív játékgyökér:** a repó gyökere (`main.py`).
**Belépési pont:** `python main.py` (főmenü). Kapcsolók: `--faction`, `--enemy`, `--seed`, `--size`, `--fullscreen`,
`--autostart`, `--mods`/`--no-mods`, `--simulate N` (ablak nélküli gép–gép), `--spectate`, rejtett `--screenshot`.
**Verziókezelés:** nincs (nem Git-tároló).

## Játékkód (`rts/`, ~24 ezer sor a `main.py`-jal)

| Fájl | Szerep |
|---|---|
| `main.py` | indítás, fő ciklus, `simulate()`, `skirmish_game()`, `custom_game()` |
| `game.py` (3696) | szimuláció és parancs-API (UI és AI közös) |
| `entities.py` (1867) | egység/épület állapotgépek, lövedékek, területi hatások |
| `ai.py` (886) | gépi ellenfél |
| `pathfinding.py` (175) | A* + simítás |
| `world.py`, `mapgen.py` | térkép, pályagenerálás (`data/maps/*.json`) |
| `heroes.py` | hősök, tárgyak, aurák, szörnytáborok |
| `net.py` (651) | LAN: TCP 47810 / UDP 47811, lockstep 100 ms-os körök, szinkronellenőrzés |
| `savegame.py` | mentés/betöltés (**pickle**) |
| `campaign.py`, `mission.py` | kampány, küldetés-célok, triggerek |
| `ui.py` (4561), `hud.py` (1613), `faction_ui.py`, `setup_screen.py`, `menu.py` (2386) | kezelőfelület, menük |
| `render.py`, `sprites.py`, `vfx.py`, `buildfx.py`, `shadows.py`, `scenery.py`, `canvas.py` | megjelenítés |
| `audio.py`, `i18n.py`, `fonts.py`, `profiles.py`, `modmgr.py`, `data.py`, `points.py`, `tips.py`, `calm.py` | kiszolgálók |

## Adat és tartalom

`data/` (game.json – `sim_hz: 30`; 22 fajfájl: 7 játszható főfaj + 13 `ally_*` kisfaj + `mycel` + `vadon`; abilities, items, maps),
`campaigns/` (12 kampánymappa), `mods/` (`micelia`, `pelda_mod`), `saves/` (profilok).

## Nem futtatott játékkód

`tools/` – adatgenerátorok (`factions_v2.py` lánc, `economy_v3.py` …), grafikai és dokumentációs szkriptek, egyszeri
importálók; teszt: `tools/test_hud.py`, balansz: `tools/balance.py`. Régi patch-/generátorszkriptet ellenőrzés nélkül nem futtatok.
Nagy, nem kód mappák: `assets/` 1,2 GB, `backup_grafika/` 409 MB, `dist/` 157 MB, `docs/` 199 MB (képek). `p.out` = régi profilozó kimenet.

## Funkcióleltár

| Funkció | Állapot |
|---|---|
| Ablak nélküli gép–gép szimuláció | működik (ellenőrizve) |
| Determinizmus azonos seed + fajpár mellett | működik (8 perces futás kétszer, azonos kimenet) |
| HUD-elrendezés, billentyű = kattintás, bemenet-védelem | működik (`test_hud.py`) |
| Főmenü, egyéni csata (20 pálya, 2–8 játékos), kampány (46 küldetés), mentés/betöltés, profilok | nem ellenőrzött (ablakos próba kell) |
| LAN-többjátékos (lockstep) | nem ellenőrzött |
| Modok, angol nyelv, hang | nem ellenőrzött |
