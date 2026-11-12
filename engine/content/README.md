# A játéktartalom (ADR-005)

Ez a mappa a játék **egyetlen tartalomforrása** (2026-11-12 óta). Az első változat a régi játék adataiból készült
egyszeri átalakítással; a módosítások ide, közvetlenül kerülnek.

| Mappa | Tartalom |
|---|---|
| `base/` | az alaptartalom: `mod.json`, `templates/` (fajtánkénti névterek: egységek és épületek, kutatások, képességek, tárgyak, állapotok, fajok), `game.json`, `locale/` (hu, en) |
| `mods/micelia/` | a Micéliák mint játszható főnép (mod, D28) |
| `schema/` | a sablonok JSON-sémái |

A `KONVERZIO.md` az átalakítás jelentése: mi melyik komponensbe került, és mi maradt `Legacy`-ben (ezeket az M6/M7
dolgozza fel).
