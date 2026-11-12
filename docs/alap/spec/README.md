# Viselkedési specifikáció (M0)

Kelt: 2026-11-12 · forrás: a Python-referenciajáték (`rts/`), a [tartalmi leltár](../TARTALOM_LELTAR.md) és a
[SZABALYOK.md](../SZABALYOK.md) (a közös alapszabályok: tick-sorrend, mozgás, harc, gazdaság, látótér).
Cél: az új motor ([UJ_MOTOR_TERV.md](../UJ_MOTOR_TERV.md)) minden fajszabályt **ugyanúgy** valósítson meg, és ezt
**próbákkal** lehessen igazolni. Bitre azonos egyezés nem cél (új számábrázolás), a viselkedés és a számok igen.

## Fájlok és állapot

| Fájl | Tartalom | Állapot |
|---|---|---|
| [FAJMECHANIKAK.md](FAJMECHANIKAK.md) | a 6 fajmechanika és a faji rendszerek (főfajonként), valamint a közös faji rendszerek (felemelkedés, Égkő-fény, Áhítat, bányák, építési stílusok, létszám, Hadtáp, szövetség) | **kész** (első változat) |
| `KEPESSEGEK.md` | a 23 képességtípus (area, nova, self_buff, bolt, summon, aura, toggle, autoheal, passive, chant, gpower, blink, bpower, raise, merge, …) | tervezett |
| `KUTATAS_ES_SZABALYOK.md` | kutatáshatások (`effects` statnevek), a 38 szabálymódosító (`rules`), fejlődési ágak | tervezett |
| `HOSOK_TARGYAK_TABOROK.md` | hősök (tapasztalat, szint, képességtanulás, újraélesztés), tárgyak, semleges táborok, elemek | tervezett |
| `KISFAJOK.md` | a 13 szövetséges nép (`ally_pack`) és a Vadon | tervezett |

## Egy szabály leírása

Minden szabály azonosítót kap (`MECH-…`, `KEP-…`, `KUT-…`), és ezekből a részekből áll:

- **Adat:** a vezérlő adatkulcsok és a mai értékük (a `data/` alapján; a mod ezeket felülírhatja).
- **Szabály:** számozott lépések, képletekkel. Az időzítés tickben és másodpercben is (30 tick = 1 s).
- **Kód:** a Python-referencia helye (`G` = `rts/game.py`, `E` = `rts/entities.py`, `H` = `rts/heroes.py`).
- **Próba:** egy-egy forgatókönyv mérhető elvárással. Az új motor próbája ugyanazt a forgatókönyvet futtatja,
  és a Python-referencia eredményével vet össze (a megadott tűréssel).
- **Új motorban:** hova kerül (C++-rendszer vagy Lua-modul), és mit kell általánosítani.
- **Megjegyzés:** furcsaság, valószínű hiba, név szerinti elágazás – ezekről a tulajdonos dönt, a port addig
  a mai viselkedést követi.

## Közös jelölések

- `TILE` = 32 px; a sugarak az adatban csempében, a kódban px-ben (× TILE).
- „15 tickenként” = 0,5 s-onként; a `dt` ilyenkor 0,5 s. A rendszer a tick-sorrend (SZABALYOK 2.1) adott pontján fut.
- Csempe-lefedés körrel: a `(x, y)` csempe benne van, ha `(x + 0,5 − cx)² + (y + 0,5 − cy)² ≤ r²` (csempe-egységben).
- A játékos azonosítója a rácsokon `p.id + 1` (0 = senkié).

## A név szerinti elágazások (általánosítandók)

A leltár 16. pontján felül a fajazonosítóra is ágazik a kód; az új motorban mind adatkulcs lesz:

| Hely | Mai feltétel | Javasolt adatkulcs |
|---|---|---|
| G 795 `pond_speed` | `defn.faction == "hukucha"` → saját tó gyorsít | faj: `pond.own_speed: true` |
| G 2565, 2546 tüskék | `faction == "croftian"` → nem hat rá / nem mérgeződik | tüske-adat: `immune_factions` vagy a tulajdonos faja |
| E 17, 398, 1503 idézések | croftian idézés nem jár le, a hős idézése állandó (max. 4) | faj: `summons_permanent: true` |
| G 2717 lámpásfény-gyógyulás | csak `faction == "ooni"` egységek | épület: `light_heal_factions` (alapból a saját faj) |
| G 2724 Kőkutya-hűség | `defn.id == "oon_kokutya"` | egység: `guard_armor` (+ szabály-szorzó) |
| G 2730 Kapuőrzés | `"oon_kapuorzes" in toggles` | képesség: `guard_armor: 3` |
| G 1185 Lélekhorgony | `"lelekhorgony" in abilities` | képesség: `soul_anchor: true` |
| G 2499 tüskelerakás | `defs["cro_merges_tuskek"]` | faj: `spikes: {building: …}` |
| G 1610 Áhítat-effekt színe | `faction.id == "elves"` | faj: `faith.color` (csak megjelenítés) |
