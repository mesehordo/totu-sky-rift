# TOTU: Sky Rift – A kölcsönkapott öröklét

Kreátor: **Mesehordo** · A világ, a történet, a grafika, a zene és a hangok © 2026 Mesehordo, minden jog fenntartva;
a kód GNU GPL 3 vagy későbbi – részletek: [LICENSE.md](LICENSE.md).

Klasszikus, 2D sprite-os, 3/4 nézetű fantasy valós idejű stratégia: bázisépítés, hősök és tárgyak, aszimmetrikus
népek, hadjárat. Mérce: a 0 A.D., az Age of Empires, a Warcraft III és a StarCraft minősége.

## Állapot

| Rész | Állapot |
|---|---|
| **Python-referenciajáték** (`main.py`, `rts/`) | játszható: egyéni csata 2–8 játékosra, 46 küldetéses hadjárat, gépi ellenfél, LAN, mentés és visszajátszás. Ez a szabályok futtatható specifikációja; a szabályai az új motor elkészültéig be vannak fagyasztva (csak hibajavítás és adatbővítés). |
| **Új motor** (`engine/`, C++20 + Lua) | fejlesztés alatt – a terv: [docs/alap/UJ_MOTOR_TERV.md](docs/alap/UJ_MOTOR_TERV.md). Kész: platform-váz (SDL3, OpenGL 3.3), CI mind az öt célrendszeren. |

Célrendszerek: Windows, Linux, macOS (Intel és Apple Silicon), FreeBSD, OpenBSD.

## A játék

* **Hét főnép:** Szürke elfek, Ursanok, Croftianok, Wullof, Vasbirodalom, Huk’ucha, Ooni – mindegyik saját
  mechanikával (energiamező, belső építés, mélység és lárvák, falkaszellem és nomád tábor, alapozás és
  lélekhorgony, tavacskák, lámpásfény és harci kedv). A **Micéliák** rejtett nép, csak a Gombanép-hadjáratban játszható.
* **13 szövetséges kisnép:** a Diplomácia csarnokában egy választható közülük, és annak három épülete nyílik meg.
* **Nyersanyagok:** Fa, Arany, Meteorit, Áhítat (az elfeknél; a Huk’uchánál Tóének); létszám legfeljebb 200.
* **Felemelkedés:** három különböző kész épület után a nép választ a három fejlődési iránya közül.
* **Hősök** szintekkel, megtanulható képességekkel és tárgyakkal; **semleges szörnytáborok** (Vadon).
* **Tartalom:** 46 küldetés 12 hadjáratban, 20 csatapálya, magyar és angol nyelv.

Fajok és egységek: [docs/FACTIONS.md](docs/FACTIONS.md) · a szabályok pontos leírása: [docs/alap/spec/](docs/alap/spec/) ·
a tartalom teljes leltára: [docs/alap/TARTALOM_LELTAR.md](docs/alap/TARTALOM_LELTAR.md).

## A Python-referenciajáték indítása

Python 3.13, valamint:

```bash
python -m pip install pygame-ce numpy pillow
```

```bash
python main.py
```

| Kapcsoló | Jelentés |
|---|---|
| `--faction <nép>`, `--enemy <nép>\|random` | saját és ellenfél népe (`elves`, `ursan`, `croftian`, `wullof`, `imperium`, `hukucha`, `ooni`) |
| `--autostart` | a menü kihagyása |
| `--seed N` | térkép- és szimulációs mag |
| `--size 1600x900`, `--fullscreen` | ablakméret |
| `--spectate` | a saját oldalt is gép irányítja |
| `--simulate N` | gép a gép ellen N percig, ablak nélkül |
| `--mods a,b` / `--no-mods` | modok kiválasztása |

Játék közben az **F1** mutatja az irányítást. Mentés: Esc menü vagy **F5** / **F9**; a mentés verziózott JSON
(`.tsav`), az egyéni csaták visszajátszása automatikusan rögzül (`.trep`).

## Az új motor fordítása

```bash
cmake -S engine -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --parallel
```

```bash
ctest --test-dir build --output-on-failure
```

Részletek: [engine/README.md](engine/README.md).

## Tesztek (Python-referencia)

```bash
python tools/test_hud.py
```

```bash
python tools/test_snapshot.py
```

```bash
python tools/golden_replays.py
```

```bash
python tools/snapshot_check.py --all-missions
```

A golden visszajátszások a szabályok változatlanságát igazolják (`tests/golden/`).

## Modolás

Ma: a `mods/<név>/` mappa a `data/` fájljait mélyen összefésülve felülírja (`mod.json`: név, bekapcsolás,
prioritás), és Python-hookokat is hozhat. Leírás: [docs/MODDING.md](docs/MODDING.md). Az új motorban a modolás a
0 A.D. és az OpenRA szintjére bővül: sablonok öröklődéssel, rétegezett fájlrendszer, Lua-szkriptek, modkezelő,
többjátékos modegyeztetés.

## Dokumentáció

| Dokumentum | Tartalom |
|---|---|
| [docs/alap/TOTU_ALAPDOKUMENTUM.md](docs/alap/TOTU_ALAPDOKUMENTUM.md) | az irányadó fejlesztési dokumentum |
| [docs/alap/UJ_MOTOR_TERV.md](docs/alap/UJ_MOTOR_TERV.md) | az új motor terve és ütemterve (M0–M14) |
| [docs/alap/DONTESEK.md](docs/alap/DONTESEK.md) | a tulajdonosi döntések |
| [docs/alap/spec/](docs/alap/spec/) | a szabályok viselkedési specifikációja |
| [docs/fejlesztes/](docs/fejlesztes/) | állapot, munkanapló, hibák, mérések |
| [docs/STORY_CAMPAIGN.md](docs/STORY_CAMPAIGN.md) | történet és hadjárat |

A [DESIGN.md](DESIGN.md) a játék korai tervdokumentuma; történeti értékű, sok része már nem érvényes.
