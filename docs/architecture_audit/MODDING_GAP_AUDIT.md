# Modolhatósági hiányaudit (0 A.D.-szintű cél)

Kelt: 2026-11-11 · commit `320d42e`. Követelményforrás: `08_0AD_SZINTU_MODOLHATOSAG_RESZLETES_TANULMANY.md`, `04_DONTESI_MUNKALAP.md` X3–X10.

| Kapu | Követelmény | Jelenlegi állapot | Bizonyíték | Hiány |
|---|---|---|---|---|
| X3 | Azonos mod mindhárom OS-en, újrafordítás nélkül | **Részben teljesül**: a mod JSON + Python, nincs fordítás; más OS-en nem futtatva | `data.py` 6–7, 204–209 | Win/Linux/macOS próba |
| X4 | Új faj, kisfaj, egység, épület, tech, mágia | **Nagyrészt adatból**: a fajfájl 26 mezős (units, buildings, researches, paths, ai …), a különleges szabályok 39 `rules` kulcsból; a `micelia` mod egyetlen mezővel (`hidden`) játszhatóvá tesz egy rejtett fajt | `mods/micelia/`, `data/factions/ooni.json` | A teljesen új mechanikához (pl. új `cmd_*`) ma Python kell; nincs komponensrendszer |
| X5 | Kampány-, pálya-, trigger-, UI-mod | Pálya: JSON (`data/maps`, `mods/<mod>/maps`). Kampány: JSON-célok és -triggerek. UI: nem modolható | `docs/MULTIPLAYER.md`, `mission.py` fejléce | UI-mod, pályaszerkesztő |
| X6 | Verziózott mod API, függőség, felülírás/ütközés, manifest | **Részleges**: `mod.json` (name, description), betöltési sorrend, tartalomhash (`net.py` 40) | `data.py` 191, `net.py` 40 | API-verzió, függőségek, ütközéslista |
| X7 | Lobbi-modlock, tartalomhash, mentés/replay-kompatibilitás | **Részleges**: a LAN-lobbi tartalomhasht vet össze; a mentés `mod_ids`-t tárol | `net.py` 40, `savegame.py` 68 | Replay nincs; a pickle-mentés kódváltozásra törékeny |
| X8 | Nem megbízható modszkript korlátozott jogokkal | **NEM teljesül**: a `script.py` korlátlan Python, teljes fájl- és hálózati hozzáféréssel, a betöltéskor lefut | `data.py` 206–208 | Sandbox (pl. Lua) vagy a szkriptmod letiltása megbízhatatlan forrásból |
| X9 | Modkezelő, profil, biztonságos indítás | **Részleges**: főmenü → Modok ablak, `saves/mods.json` | `modmgr.py` | Hibás mod utáni helyreállítás, letöltés |
| X10 | 1600–2400 egység szkriptes modokkal mérve | **NINCS mérve** | – | – |

**Következmény a motorválasztásra:** a jelenlegi modmodell (JSON + Python hook) C++/C# motor alatt sem vihető át változatlanul, mert a `script.py` Python. Bármely natív úton kell egy beágyazott szkriptnyelv (Lua a tanulmány javaslata), és a meglévő `pelda_mod/script.py` átírása. A JSON-adatmodell viszont nyelvfüggetlen, és a portolás alapja lehet.

**Biztonsági megjegyzés a mai játékhoz:** a pickle-mentés (`savegame.py` 180) és a mod `script.py` egyaránt tetszőleges kódot futtathat. Amíg ez így van, idegen mentést vagy modot csak megbízható forrásból szabad betölteni.
