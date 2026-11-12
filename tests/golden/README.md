# Golden replay-készlet

Rögzített felvételek a játékmenet változatlanságának ellenőrzésére: `python tools/golden_replays.py`.
Mind a `pelda_mod` és a `micelia` nélkül, alapadatokkal készült (`--no-mods` szerű betöltés, üres modlista).

| Fájl | Tartalom | Újrarögzítés |
|---|---|---|
| `skirmish_croftian_ooni_s7.trep` | 2 játékos, generált pálya, 5 perc, szkriptelt parancsok | `python tools/replay_check.py --seed 7 --minutes 5 --no-mods --out tests/golden/skirmish_croftian_ooni_s7.trep` |
| `skirmish_ursan_elves_s11.trep` | 2 játékos, 4 perc, más fajpár | `python tools/replay_check.py --seed 11 --faction ursan --enemy elves --minutes 4 --no-mods --out tests/golden/skirmish_ursan_elves_s11.trep` |
| `harmas_folyo_3p_s2.trep` | 3 játékos, csatapálya, 3 perc | `python tools/replay_check.py --map harmas_folyo --players 3 --seed 2 --minutes 3 --no-mods --out tests/golden/harmas_folyo_3p_s2.trep` |

Újrarögzíteni csak szándékos szabály- vagy adatváltozás után szabad, a munkanaplóban indokolva.

Utolsó újrarögzítés: 2026-11-12, a determinisztikus matematika (`rts/dmath.py`, C#-port P0) miatt.
`dmath_vectors.json`: a `dmath` függvényeinek bit-pontos referenciakimenetei (`python tools/test_dmath.py --vectors tests/golden/dmath_vectors.json --n 5000`) a C#-átirat ellenőrzéséhez.

2026-11-12, D21: a `skirmish_croftian_ooni_s7.trep` újrarögzítve – a mélységi rothadás új sorrendje miatt tért el
(900. tick). A régi rothadás-függvénnyel visszajátszva mind a 30 hash egyezett, tehát a többi változtatás
(fajnév → adatkulcs, hős halála) ebben a felvételben nem okozott eltérést.
