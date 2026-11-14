# A szimuláció viselkedési szerződése (F2)

Kelt: 2026-11-11. Nyelvfüggetlen leírás: bármely motor (a mai Python, egy C#/C++ prototípus vagy egy dedikált
szerver) ehhez igazodik. A kód a referencia: `rts/net.py` (parancsok), `rts/snapshot.py` (állapot),
`rts/replay.py` (felvétel), `rts/savegame.py` (mentés).

## 1. Idő

- Fix szimulációs lépés: `sim_hz` = 30 (D6), `dt` = 1/30 s. A szabályok csak a ticktől függnek, a képkockától nem.
- `Game.tick` egész szám, minden `update` 1-gyel növeli. `Game.time` = tick × dt (lebegőpontos összeg).
- Egy parancs a `t` tickhez tartozik: a `Game.tick == t` állapotban, a következő `update` **előtt** hajtódik végre.
- Logikai tick nem hagyható ki; a lassulás valós időben látszik, a szabályokban nem.

## 2. Determinizmus

- Azonos kezdőállapot + azonos parancssor → bitre azonos állapothash-sorozat. Igazolva: egy folyamaton belül,
  új folyamatban és eltérő `PYTHONHASHSEED` mellett (`tools/golden_replays.py`). Más gépen és más OS-en még nem.
- A szimuláció véletlenje: a globális `random` (a `Game` létrehozásakor `random.seed(seed)`), az állapot része.
  A kliens (rajzolás, hang) saját `random.Random` példányokat használ, a szimuláció véletlenjét nem fogyaszthatja.
- Állapothash: `rts.net.state_hash` – CRC32 a tickről, játékosokról, épületekről, egységekről, nyersanyagokról és a
  blokkrácsról. Szerződésbeli ellenőrzőösszeg, nem kriptográfiai.

## 3. Parancsok

Forma: `[név, [arg, ...], {kw}]` JSON-ban; a `kw` csak `queue`, `attack`, `target` lehet.
Engedélyezett nevek: `rts.net.ALLOWED` (40 parancs: `cmd_move`, `cmd_attack`, `cmd_smart`, `cmd_stop`, `cmd_hold`,
`cmd_build`, `cmd_train`, `cmd_research`, `cmd_rally`, `cmd_cast` …).
Paraméterkódolás:

| Kód | Jelentés |
|---|---|
| JSON-primitív, lista | önmaga (nem véges szám elutasítva) |
| `{"E": id}` | egység vagy épület az azonosítójával |
| `{"P": index}` | játékos |
| `{"R": [tx, ty]}` | nyersanyag-csempe |
| `{"I": id}` | földön fekvő tárgy |
| `{"D": id}` | egység-/épületdefiníció |
| `{"M": {...}}` | egyéb dict |

Érvényesítés (`rts.net.execute`): ismeretlen név, hibás paraméter vagy nem létező entitás → a parancs elvész; a
parancs csak a kiadó játékos saját egységeire/épületeire hathat. A szerverautoritatív modell (D4) ugyanezt a
sémát és ellenőrzést használja.

## 4. Állapot-snapshot (`totu-snapshot`, séma 1)

A teljes szimulációs objektumgráf JSON-ban; leírás: `rts/snapshot.py` fejléce. Lényeg:
- objektumtábla (`objects`), az objektumokra index mutat (`$o`); megosztott tárolók egyszer (`$i`/`$c`);
- a játékadatok nem kerülnek bele, útvonallal hivatkozunk rájuk (`$data`); a betöltő a futó adatkészlethez köt;
- betöltéskor csak a szimulációs modulok osztályai jönnek létre, kódfuttatás nélkül.

Ez a mai Python-objektumok belső szerkezetét tükrözi: **belső formátum**, nem a hálózati kliensnézet. A FOW-szűrt
kliensnézet és a szerver-folytatáshoz szükséges állapot külön séma lesz (F5).

## 5. Mentés (`.tsav`)

gzip(JSON snapshot) a következő gyökérrel: `version` (2), `game`, `cam`, `seen`, `extra`, `rng`, `mod_ids`.
Mellette `.json` fejléc és `.png` bélyegkép. A modlista eltérése betöltési hiba. A régi pickle `.sav` csak
`tools/convert_saves.py`-jal alakítható át (saját mentésre).

## 6. Felvétel (`.trep`)

gzip JSON: `format` = `totu-replay`, `schema` 1, `start` (snapshot a felvétel kezdetén), `mod_ids`,
`commands` (`{"t", "p", "c"}`), `hashes` (`[tick, hash]` 300 tickenként), `end_tick`, `complete`.
Egyéni csatában a játék automatikusan felvesz: `saves/profiles/<profil>/replays/`, a legutóbbi 20.
Korlát: kampányban (döntések, átvezetők) és LAN-on még nincs felvétel.

## 7. Ellenőrző eszközök

| Eszköz | Mit igazol |
|---|---|
| `tools/golden_replays.py` | a rögzített `tests/golden/*.trep` felvételek változatlanul játszódnak le |
| `tools/snapshot_check.py` | mentés → betöltés → folytatás = megszakítás nélküli futás (skirmish, 8P, 46 küldetés) |
| `tools/test_snapshot.py` | oda-vissza és hamisított mentés elutasítása |
| `tools/replay_check.py` | felvétel szkriptelt parancsokkal és visszajátszás |
| `tools/golden_hash.py` | állapothash-sorozat parancsok nélkül (gép–gép) |

## 8. Még hiányzik a szerződésből (következő F2-feladatok)

- ~~A szabályok szöveges leírása~~ – kész: [SZABALYOK.md](SZABALYOK.md) (alapszabályok + az F3 közös feladata).
- Kampány-felvétel (döntés-ablak és átvezető mint parancs).
- Más gépen / más OS-en futtatott determinizmus-próba.
