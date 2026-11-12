# Moddolás

*A motor, az eszközök és a grafika megjelöléssel szabadon felhasználható; a világ elemei (szereplők, történet, népek leírása) © 2026 Mesehordo, csak engedéllyel. Lásd: [LICENSE.md](../LICENSE.md).*

## Mit lehet moddolni?

| Terület | Hogyan |
|---|---|
| Egységek, épületek, fejlesztések, költségek, gépi ellenfél | `factions/<faj>.json` mélyösszefésüléssel (`null` = törlés) |
| Új faj | teljes `factions/<új>.json` – a menü automatikusan felveszi |
| Képességek | `abilities.json` – típusok: `toggle`, `area` (`heal` = gyógyító terület), `autoheal` (`heal_tags`), `self_buff`, `summon`, `raise`, `merge`, `bolt` (célzott: `damage`, `slow`, 0 = bénítás, `projectile`), `aura` (`radius`, `modifiers`: `*_mult`, `armor_add`, `regen_add`) |
| Hősök | egység-mezők: `hero: true`, `hero_abilities` (4 képesség), `growth` (szintenkénti `hp`, `damage`, `armor`, `energy`); a képesség `levels` listája szintenként felülírja a mezőket, `ultimate: true` = 6. szinttől |
| Tárgyak | `items.json`: `kind` (`consumable` / `equip`), `rarity` (`common`, `rare`, `artifact`), `heal`, `energy`, `xp`, `perm`, `bonus` (`hp`, `damage`, `armor`, `speed_pct`, `regen`, `energy`, `energy_regen`), `icon` |
| Szörnytáborok | a `neutral: true` faj (`factions/vadon.json`) `camps` mezője: szintenként egységek és zsákmány-ritkaság; az egységek `xp` mezője a tapasztalat |
| Fajmechanikák | faj-mezők: `mechanic` (`standard`, `power`, `creep`, `network`), `regen`, `bounty`, `pack_bonus`, `resource_names` (nyersanyag átnevezése), `supply_unit` (létszámot hordozó egység), `network_supply`, `infest`, `bounty_res`; épület-mezők: `garrison`, `altar`, `regen_aura`; egység-mezők: `unit_supply`, `start_resources`; egység-mezők: `packable`, `death_burst`, `lifetime`, `tags` (`mech` = javítható) |
| Térkép | `game.json` → `map` (méret), nyersanyag-paraméterek |
| Kampány | `campaigns/<id>/campaign.json` + küldetések, döntésekkel és jelzőkkel (lásd STORY_CAMPAIGN.md) |
| Logika | `script.py`: `on_load(data)`, `on_game_start(game)`, `on_tick(game, dt)` |
| Grafika | `python tools/make_sprites.py --mods <mod>` a mod egységeihez is sprite-ot renderel; saját PNG is megadható a manifestben |

## A Modok ablak (főmenü → Modok)

A 0 A.D. mod-választójához hasonlóan működik:

- **Fent az elérhető, lent az engedélyezett modok** listája. A sorszám a betöltési sorrend: a később betöltött mod felülírja a korábbit, ha ugyanazt az adatot módosítják.
- **Engedélyezés / Letiltás:** gombbal, Szóközzel vagy dupla kattintással mozgatod a modot a két lista között. A **Fel / Le** a sorrendet állítja, a **Rendezés** gomb a függőségek elé teszi azokat a modokat, amelyektől mások függnek.
- **Oldalt a kijelölt mod adatai:** név, címke, verzió, szerző, tartalom, függőségek és állapotuk, leírás. Ha a mod Python-szkriptet futtat, figyelmeztetés jelzi.
- **Mentés és alkalmazás:** a lista a `saves/mods.json`-ba kerül, és a játék azonnal újratölti az adatokat (nem kell újraindítani). Hiányzó vagy rossz sorrendű függőség, illetve összeférhetetlen modok mellett a gomb nem él; a hiba a gomb mellett látszik.
- Parancssorból felülírható: `python main.py --mods a,b` (ebben a sorrendben), vagy `--no-mods`.

### mod.json

```json
{
  "name": "Micéliák – játszható nép",
  "label": "Új nép",
  "version": "1.0",
  "author": "…",
  "description": "…",
  "dependencies": ["masik_mod"],
  "incompatible": ["harmadik_mod"]
}
```

A `dependencies` és az `incompatible` a többi mod **mappanevét** sorolja. A régi `enabled` és `priority` mező csak addig számít, amíg a játékos először el nem menti a Modok ablakban a választását.

### Mellékelt modok

| Mappa | Mit csinál |
|---|---|
| `mods/micelia` | A Micéliák (gombanép) játszható népként: egyéni csatában, többjátékos módban és gépi ellenfélként is. |
| `mods/pelda_mod` | Példa: új Ursan egység, olcsóbb mamut, több kezdő nyersanyag, Python-hook. |

## Új egység lépésről lépésre
1. `mods/sajat/mod.json`: `{"name": "Saját", "version": "1.0", "description": "…"}`, majd engedélyezd a Modok ablakban.
2. `mods/sajat/factions/ursan.json`: új egység az `units` alá + a termelő épület `produces` listája.
3. Sprite: `python tools/make_sprites.py --mods sajat --only <egység_id>` – ha a `shape` ismert (worker,
   warrior, archer, rider, beast, flyer, bug, …), automatikusan modell készül; sprite nélkül a játék kódból rajzol.
4. Ellenőrzés: `python tools/check_sprites.py`, balansz: `python tools/balance.py --mods sajat`.

## Tervezett bővítések
* Térképszerkesztő (JSON térképformátum, kézi elhelyezés).
* Saját sprite-csomagok (pl. kézzel rajzolt PNG-k a generált helyett, azonos metaadat-formátummal).
* Lua-szerű, homokozóban futó küldetés-scriptek a Python-hookok mellé (biztonságos megosztáshoz).
