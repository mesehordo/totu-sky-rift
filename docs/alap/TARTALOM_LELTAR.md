# Tartalmi és szabályleltár (generált)

Készítette: `python tools/tartalom_leltar.py` az alapadatokból (`data/`, modok nélkül). **Kézzel ne szerkeszd** – a magyarázó részek az [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md)-ben vannak.
A „kód” oszlop a mostani Python-játék hivatkozásait mutatja (`modul` sorszámok); ez a viselkedés referenciája az új motorhoz. **nincs** = a kulcs szó szerint nem szerepel a kódban (halott adat, vagy dinamikusan képzett kulcs).

**Összesen:** 22 faj, 220 egység, 152 épület, 168 kutatás, 269 képesség (23 típus), 15 tárgy, 6 fajmechanika.

## 1. Fajok

| Faj | Név | Típus | Mechanika | Egység | Épület | Kutatás | Ág | Képesség |
|---|---|---|---|---|---|---|---|---|
| `elves` | Szürke elfek | főfaj | `power` | 17 | 13 | 16 | 3 | 18 |
| `ursan` | Ursanok | főfaj | `standard` | 18 | 15 | 16 | 3 | 17 |
| `croftian` | Croftianok | főfaj | `creep` | 17 | 16 | 16 | 3 | 12 |
| `wullof` | Wullof | főfaj | `standard` | 23 | 13 | 18 | 3 | 24 |
| `mycel` | Micéliák | rejtett főfaj | `network` | 14 | 10 | 4 | 3 | 10 |
| `imperium` | Aurethi Birodalom | főfaj | `standard` | 16 | 19 | 25 | 3 | 14 |
| `hukucha` | Huk’ucha | főfaj | `pond` | 22 | 13 | 16 | 3 | 17 |
| `ooni` | Ooni | főfaj | `lantern` | 20 | 14 | 18 | 3 | 11 |
| `vadon` | Vadon | semleges | `standard` | 5 | 0 | 0 | 0 | 4 |
| `micelia` | Micéliák | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 7 |
| `pinguu` | Pinguu | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 5 |
| `ghoblin` | Ghoblin | szövetséges kisfaj | `standard` | 6 | 3 | 3 | 0 | 3 |
| `mhonyo` | Mhonyo | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 5 |
| `daresh` | Daresh | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 3 |
| `tokla` | Tokla | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 4 |
| `galoca` | Galóca | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 3 |
| `niil_elf` | Niil-elfek | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 4 |
| `tuz_elf` | Tűz-elfek | szövetséges kisfaj | `standard` | 6 | 3 | 3 | 0 | 4 |
| `tenduur` | Tendüür | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 4 |
| `nila_uchana` | Nila-uchana | szövetséges kisfaj | `standard` | 6 | 3 | 3 | 0 | 5 |
| `zhan_zhama` | Zhan-zhama | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 3 |
| `shrissh_matah` | Shrissh-matah | szövetséges kisfaj | `standard` | 5 | 3 | 3 | 0 | 3 |

## 2. Fajmechanikák (`mechanic`)

| Mechanika | Fajok | Kód |
|---|---|---|
| `creep` | croftian | `game` 59,757,760…; `ui` 1994,1998,2008… |
| `lantern` | ooni | `ai` 217,218,291…; `game` 2295,2299 |
| `network` | mycel | `ai` 267; `game` 757,921,2269…; `ui` 4429,4533 |
| `pond` | hukucha | `ai` 235,237,241…; `campaign` 669; `entities` 161,164; `game` 150,253,257…; +3 modul |
| `power` | elves | `ai` 287; `game` 2275,3001 |
| `standard` | ursan, wullof, imperium, vadon, micelia, pinguu, ghoblin, mhonyo, daresh, tokla, galoca, niil_elf, tuz_elf, tenduur, nila_uchana, zhan_zhama, shrissh_matah | `game` 1469,3399,3410 |

## 3. Képességtípusok

| Típus | Db | Használó fajok | Adatkulcsok | Kód |
|---|---|---|---|---|
| `area` | 36 | croftian, elves, galoca, hukucha, micelia, mycel, niil_elf, nila_uchana, ooni, pinguu, tenduur, tokla, tuz_elf, wullof | range, color, radius, duration, sprite, energy, cooldown, slow, dps, levels, ally, ultimate, hp_cost, heal, spores, faith | `entities` 1605,1614,1634; `game` 1694,3631; `mission` 10,13,128…; `hud` 326; +1 modul |
| `nova` | 34 | croftian, elves, galoca, ghoblin, hukucha, imperium, mhonyo, mycel, niil_elf, nila_uchana, ooni, tenduur, tokla, tuz_elf, ursan, vadon, wullof | cooldown, color, radius, damage, stun, energy, levels, ally, slow, duration, ultimate, self_destruct, hp_cost, spores, faith, heal, building_damage | `entities` 1576,1580,1588; `game` 1657,1700,1729…; `ui` 1074,3456,3496… |
| `self_buff` | 31 | croftian, elves, galoca, hukucha, imperium, mhonyo, nila_uchana, ooni, pinguu, shrissh_matah, tenduur, tuz_elf, ursan, wullof | cooldown, duration, modifiers, levels, ally, ultimate, energy, no_weak, hp_cost, drop_work, after_slow | `entities` 1614,1646; `ui` 1087 |
| `bolt` | 28 | croftian, elves, hukucha, mycel, ooni, shrissh_matah, ursan, vadon, wullof | range, cooldown, projectile, slow, duration, energy, levels, damage, hp_cost, spores, bounces, poison, splash, ally | `entities` 1334,1594; `game` 3631; `hud` 278,326,345…; `ui` 1063,4428,4489 |
| `summon` | 27 | hukucha, nila_uchana, ooni, tuz_elf, ursan, wullof, zhan_zhama | cooldown, unit, lifetime, count, energy, levels, ultimate, ally, faith, target_placement | `entities` 1232,1614,1616…; `game` 1701,3697; `ui` 1072,4431,4527 |
| `aura` | 22 | ursan | radius, levels, modifiers | `entities` 283; `heroes` 308,343,347; `hud` 320; `ui` 1068,4432,4554 |
| `toggle` | 20 | croftian, daresh, elves, ghoblin, hukucha, imperium, mhonyo, micelia, mycel, niil_elf, ooni, pinguu, ursan, wullof | modifiers, ally, levels, burrow | `entities` 1542,1626,1640; `hud` 322,349; `menu` 692,1712,1733…; `ui` 1085 |
| `autoheal` | 20 | daresh, elves, hukucha, mhonyo, micelia, niil_elf, nila_uchana, ooni, pinguu, tenduur, tokla, ursan, wullof, zhan_zhama | cooldown, range, amount, ally, levels, energy, heal_tags | `entities` 283,1422; `ui` 1095 |
| `passive` | 15 | croftian, elves, imperium, micelia, mycel, ursan, vadon, wullof | split_kills, detect, modifiers | `entities` 283,1557; `game` 1185,1324,1325…; `hud` 216,320,513…; `ui` 1068,2848 |
| `chant` | 11 | daresh, mhonyo, ooni, pinguu, shrissh_matah, tokla, ursan, wullof, zhan_zhama | radius, modifiers, ally | `entities` 1538,1539; `game` 3680,3685; `heroes` 343,347; `hud` 322,349; +1 modul |
| `gpower` | 7 | croftian, elves, hukucha, imperium, ooni, ursan, wullof | target, cooldown, effect, duration, radius, color, dps, slow, faith, buff, reveal, hq_hp, unit, count, lifetime, meteor, buildings | `ai` 721; `game` 1667; `hud` 396 |
| `blink` | 4 | elves, hukucha, wullof | cooldown, range, color, energy, range_research, land_damage, land_radius | `entities` 1307,1549; `game` 3631; `hud` 326; `ui` 1063,3815,4423… |
| `bpower` | 3 | elves, hukucha | effect, duration, cooldown, faith, mult, heal | `game` 1633 |
| `raise` | 2 | imperium | range, small, large, energy, levels | `entities` 1365,1620; `game` 3631; `hud` 326; `ui` 1063 |
| `merge` | 1 | micelia, mycel | count, into, range | `ai` 833; `hud` 415; `ui` 1082,4432,4530 |
| `anchor_rescue` | 1 | imperium |  | `game` 1388,1424; `hud` 328,1185; `ui` 1065 |
| `morph` | 1 | croftian | target_placement, into, via, time, cost | `ai` 793,795; `entities` 259,600,668…; `game` 594,595,1411…; `hud` 1195,1196; +3 modul |
| `garrison` | 1 | ursan |  | `ai` 799,803,805…; `entities` 1180,1677,1712…; `game` 1245,2131,2135…; `hud` 423,425,1169…; +1 modul |
| `mount` | 1 | wullof | range, into, riders | `ai` 818; `game` 2883; `ui` 1080 |
| `dismount` | 1 | wullof | mount, rider | `game` 2868; `ui` 1080 |
| `ward` | 1 | ooni | cooldown, energy, radius, amount, duration, color | `entities` 1567; `game` 1111,1114,2019; `ui` 1093 |
| `overdrive` | 1 | imperium | cooldown, self_hp, share, color | `entities` 1559; `ui` 1089 |
| `lay_mine` | 1 | hukucha | target_placement, cooldown, energy, unit, max | `entities` 1229,1571; `ui` 1091 |

Egy egység vagy épület sem hivatkozik rájuk közvetlenül (98; tárgy, elem, tábor, küldetés vagy idézés adhatja): `gp_isteni_szel_buff`, `h_aka_harag`, `h_aka_rengetes`, `h_aka_roham`, `h_aka_vorosvihar`, `h_bja_falka`, `h_bja_mamutcsorda`, `h_bja_medve`, `h_bja_szigony`, `h_bul_brekeges`, `h_bul_kampo`, `h_bul_kiraly`, `h_bul_ugras`, `h_cas_fegyelem`, `h_cas_harag`, `h_cas_legio`, `h_cas_roham`, `h_gya_halo`, `h_gya_melygyoker`, `h_gya_rengés`, `h_gya_talaj`, `h_hja_bunda`, `h_hja_fagyott_ido`, `h_hja_szilank`, `h_hja_vihar`, `h_iri_aldas`, `h_iri_gyongy`, `h_iri_gyongykapu`, `h_iri_szivarvany`, `h_itz_csapas`, `h_itz_darda`, `h_itz_jelenlet`, `h_itz_kukulkan`, `h_kar_szel`, `h_kar_tajfun`, `h_kar_tollak`, `h_kar_villam`, `h_keth_csap`, `h_keth_kod`, `h_keth_korushang`, `h_keth_sokasag`, `h_kur_het_ut`, `h_kur_taposas`, `h_kur_uvoltes`, `h_kur_verfalka`, `h_kva_arasztas`, `h_kva_forras`, `h_kva_lenyek`, `h_kva_ozonviz`, `h_livia_horgony`, `h_livia_legio`, `h_livia_lelegzet`, `h_livia_lelek`, `h_mor_ebredes`, `h_mor_emlekkert`, `h_mor_gyogyspora`, `h_mor_gyokerkotes`, `h_nera_batorsag`, `h_nera_horgony`, `h_nera_loves`, `h_nera_toltes`, `h_ragna_esku`, `h_ragna_hosszutel`, `h_ragna_pajzsfal`, `h_ragna_roham`, `h_shi_kapu`, `h_shi_oroktolany`, `h_shi_pajzs`, `h_shi_uvoltes`, `h_spo_kopeny`, `h_spo_raj`, `h_spo_rothadas`, `h_spo_tuske`, `h_ssa_felho`, `h_ssa_oltes`, `h_ssa_rajtudat`, `h_ssa_remszarnyak`, `h_szelai_csillagorok`, `h_szelai_levalasztas`, `h_szelai_napvihar`, `h_szelai_rezonancia`, `h_tava_farkasok`, `h_tava_madar`, `h_tava_nyil`, `h_tava_utenek`, `h_var_falka`, `h_var_holdfeny`, `h_var_kod`, `h_var_telihold`, `h_vor_larvak`, `h_vor_pancel`, `h_vor_robbanas`, `h_vor_sugar`, `h_xochi_harmonia`, `h_xochi_hullam`, `h_xochi_jadeor`, `h_xochi_napkorona`, `osztodas2`

## 4. Faj-szintű kulcsok

| Kulcs | Előfordul | Kód |
|---|---|---|
| `accent` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `faction_ui` 18,20,22…; `hud` 728,734,767…; `menu` 868; `render` 333; +1 modul |
| `buildings` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 51,269,436…; `campaign` 185,187,195…; `entities` 180,1840; `game` 144,313,322…; +10 modul |
| `color` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `campaign` 697,759; `entities` 151,1317,1321…; `game` 42,175,180…; `heroes` 407; +5 modul |
| `desc` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `id` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 28,54,70…; `campaign` 44,46,54…; `entities` 66,164,201…; `game` 39,97,257…; +16 modul |
| `mechanic` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 217,267,287…; `game` 757,800,921…; `ui` 1994 |
| `name` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `order` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 72,84,106…; `campaign` 46,61; `entities` 23,216,409…; `game` 482,485,487…; +6 modul |
| `style` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `render` 4,351; `ui` 3241,3249,3546 |
| `tagline` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `hud` 480,765,1224; `menu` 867; `ui` 1274 |
| `units` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 50,433,483…; `campaign` 181,184,194…; `entities` 766,1531; `game` 143,273,310…; +12 modul |
| `ai` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 14,389,399; `campaign` 150,196; `game` 715,3116; `data` 158; +3 modul |
| `researches` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 414,415; `game` 643,3099,3565…; `data` 55,117,120…; `hud` 381,382,383…; +2 modul |
| `ally` | 13: daresh, galoca, ghoblin, mhonyo, micelia, niil_elf (+7) | `ai` 378,382; `game` 52,179,645…; `data` 70,75,123…; `hud` 217,362,364…; +2 modul |
| `ally_pack` | 13: daresh, galoca, ghoblin, mhonyo, micelia, niil_elf (+7) | `game` 3444; `hud` 372,465,477… |
| `hq` | 9: croftian, elves, hukucha, imperium, mycel, ooni (+3) | `ai` 54; `game` 246,280,661…; `data` 122,140; `hud` 428; +1 modul |
| `start_gather` | 9: croftian, elves, hukucha, imperium, mycel, ooni (+3) | `game` 275 |
| `start_units` | 9: croftian, elves, hukucha, imperium, mycel, ooni (+3) | `game` 267 |
| `supply_building` | 9: croftian, elves, hukucha, imperium, mycel, ooni (+3) | `ai` 184; `game` 3515; `data` 140 |
| `worker` | 9: croftian, elves, hukucha, imperium, mycel, ooni (+3) | `ai` 128,147,151; `entities` 230; `game` 3169; `data` 140; +1 modul |
| `ascension` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `game` 659 |
| `paths` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 399; `game` 178,652,677…; `setup_screen` 241; `ui` 764,1226,1231 |
| `supply_desc` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `game` 3515; `hud` 1420 |
| `ally_cost` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 379; `game` 698; `hud` 472 |
| `spell_resource` | 6: croftian, elves, imperium, mycel, ursan, wullof | `game` 1183,1350; `hud` 1190 |
| `builder` | 5: hukucha, imperium, ooni, ursan, wullof | `game` 1466 |
| `regen` | 4: croftian, imperium, mycel, vadon | `entities` 191,674,677…; `game` 1972,1996,1999; `heroes` 18; `ui` 3908 |
| `rift` | 3: croftian, elves, imperium | `entities` 160,169,171…; `game` 1946,1949,1957…; `scenery` 220,388,528; `setup_screen` 265,430; +1 modul |
| `faith` | 2: elves, hukucha | `ai` 205,724,748…; `entities` 287,297,298; `game` 1599,1619,1620…; `hud` 77,147,336…; +1 modul |
| `gatherers` | 2: imperium, ooni | `ai` 129 |
| `repair` | 2: imperium, wullof | `ai` 857,866; `entities` 1406; `game` 1716,2108,3368…; `faction_ui` 309; +2 modul |
| `supply_unit` | 2: croftian, wullof | `ai` 171 |
| `bounty` | 1: wullof | `game` 1230 |
| `bounty_res` | 1: wullof | `game` 1234 |
| `build_boost` | 1: ooni | `game` 1489 |
| `camps` | 1: vadon | `ai` 593,600; `game` 167,368; `heroes` 396,404,415…; `menu` 881,893,894… |
| `creep_rot` | 1: croftian | `game` 1787 |
| `forest_stealth` | 1: wullof | `game` 1307,1308 |
| `foundation` | 1: imperium | `ai` 249; `game` 258,2344,2369…; `ui` 1967,1980 |
| `hidden` | 1: mycel | `campaign` 356,512,755; `game` 1317,1343,3085; `data` 70; `hud` 375,462; +2 modul |
| `infest` | 1: croftian | `game` 1239 |
| `morale` | 1: ooni | `entities` 329; `game` 1142,1143,1144…; `hud` 871,872,874… |
| `network_supply` | 1: mycel | `game` 2981 |
| `neutral` | 1: vadon | `ai` 434,462,484…; `game` 168,677,810…; `heroes` 408,409; `mission` 300; +5 modul |
| `pack_bonus` | 1: wullof | `game` 764,772 |
| `pond` | 1: hukucha | `ai` 235,237,241…; `campaign` 669; `entities` 161,164; `game` 150,253,257…; +3 modul |
| `pond_affinity` | 1: hukucha | `game` 795 |
| `pond_build_bonus` | 1: hukucha | `game` 1487,1488 |
| `resource_names` | 1: hukucha | `game` 86 |
| `self_build` | 1: elves | `ai` 841; `game` 1473,2845 |
| `shield_zone` | 1: imperium | `entities` 158 |
| `soul_anchor` | 1: imperium | `game` 1188,1393 |
| `spike_immune` | 1: croftian | `game` 2554,2574 |
| `summons_permanent` | 1: croftian | `entities` 17,398,1503 |
| `unpack_time` | 1: wullof | `game` 2843 |
| `wagon` | 1: wullof | `game` 2822 |
| `warp_in` | 1: elves | `game` 3169 |

## 5. Egységkulcsok

| Kulcs | Előfordul | Kód |
|---|---|---|
| `abilities` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 478,720,769…; `campaign` 297,298,765…; `entities` 310,311,338…; `game` 966,1189,1632…; +5 modul |
| `armor` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 102,358; `game` 979,1135; `heroes` 18; `hud` 899,901,903; +1 modul |
| `attack` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 279,443,468…; `campaign` 302,777; `entities` 81,108,112…; `game` 98,817,829…; +7 modul |
| `attack.cooldown` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 82,137,140…; `game` 1004,1649,1691…; `audio` 63,239,253; `hud` 332,334,410; +2 modul |
| `attack.damage` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 128,1351,1361…; `game` 1252; `heroes` 18; `hud` 893,894,895; +1 modul |
| `attack.projectile` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 147,1351,1360…; `ui` 3363 |
| `attack.range` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 827; `entities` 122,1346,1386…; `game` 849,2893,2929; `ui` 3666,3908 |
| `attack.targets` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `campaign` 622; `entities` 112,1353,1717 |
| `cost` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 181,214,253…; `entities` 979,989,1011…; `game` 333,1220,1232…; `heroes` 107; +2 modul |
| `desc` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `hp` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 427,578,726…; `entities` 74,75,133…; `game` 879,1107,1139…; `heroes` 18,88,91…; +3 modul |
| `name` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `radius` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 732; `campaign` 337,642,654…; `entities` 48,50,51…; `game` 394,447,456…; +5 modul |
| `shape` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `game` 661; `buildfx` 29; `calm` 57,97; `render` 4,344,351; +3 modul |
| `sight` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 105,770,817…; `game` 3059; `hud` 909,910 |
| `speed` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `entities` 363,381,481…; `game` 139,668,669; `mission` 239,262; `hud` 906,907; +3 modul |
| `supply` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 480; `entities` 405; `game` 1214,1371,1380…; `hud` 1167,1168,1418…; +1 modul |
| `tags` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 53,858; `entities` 71,73,239…; `game` 94,749,750…; `heroes` 312,317; +1 modul |
| `time` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `ai` 16,48,49…; `campaign` 406,430,448…; `entities` 141,187,320…; `game` 140,286,288…; +7 modul |
| `utype` | 22: croftian, daresh, elves, galoca, ghoblin, hukucha (+16) | `game` 951,952,1061; `audio` 213,214; `hud` 1021,1042 |
| `energy` | 21: croftian, daresh, elves, galoca, hukucha, imperium (+15) | `entities` 78,79,198…; `game` 92,3143,3154; `heroes` 18,89,93…; `hud` 338,339,866; +2 modul |
| `ally` | 13: daresh, galoca, ghoblin, mhonyo, micelia, niil_elf (+7) | `ai` 378,382; `game` 52,179,645…; `data` 70,75,123…; `hud` 217,362,364…; +2 modul |
| `flying` | 11: croftian, daresh, elves, hukucha, imperium, mycel (+5) | `ai` 656; `entities` 83,113,387…; `game` 448,458,469…; `mission` 346; +2 modul |
| `type_bonus` | 11: croftian, elves, ghoblin, hukucha, imperium, micelia (+5) | `game` 958; `hud` 1036 |
| `attack.splash` | 9: croftian, elves, ghoblin, hukucha, imperium, mycel (+3) | `entities` 1354,1800,1801; `game` 936,937,940… |
| `worker` | 9: croftian, elves, hukucha, imperium, micelia, mycel (+3) | `ai` 128,147,151; `entities` 230; `game` 3169; `data` 140; +1 modul |
| `attack.bonus` | 8: croftian, elves, ghoblin, hukucha, imperium, mycel (+2) | `entities` 128,191,199…; `game` 1094; `heroes` 73,85,90; `hud` 895,908 |
| `growth` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `heroes` 78 |
| `hero` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 559,560,564…; `campaign` 205,808; `entities` 260,261,336…; `game` 1012,1170,1190…; +3 modul |
| `hero_abilities` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `entities` 353; `heroes` 54,161,179; `data` 150; `hud` 274 |
| `path` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `campaign` 16,17,32…; `entities` 218,348,432…; `game` 49,178,438…; `heroes` 378; +19 modul |
| `requires` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `campaign` 100,102; `game` 642; `data` 144 |
| `deploy` | 6: croftian, elves, ghoblin, hukucha, imperium, ursan | `entities` 120,461,780; `ui` 3815,4424,4497 |
| `no_corpse` | 6: croftian, elves, hukucha, imperium, ursan, wullof | `game` 1261 |
| `ability_research` | 5: elves, hukucha, imperium, ursan, wullof | `entities` 343; `savegame` 183,186 |
| `sprite` | 4: elves, hukucha, micelia, wullof | `hud` 693; `ui` 89,3404,3408 |
| `summoned` | 4: hukucha, nila_uchana, ooni, tuz_elf | `game` 1169,1991 |
| `attack.poison` | 3: daresh, galoca, nila_uchana | `entities` 242,696,697…; `game` 1036,1040,1041…; `hud` 936; `savegame` 187; +2 modul |
| `attack.slow` | 3: ghoblin, pinguu, tokla | `entities` 1353,1478,1479…; `game` 997,1063,1079…; `ui` 2418,2617,2627… |
| `death_burst` | 3: croftian, micelia, mycel | `game` 1249 |
| `gathers` | 3: croftian, imperium, ooni | `ai` 91; `entities` 367 |
| `shield` | 3: elves, hukucha, ursan | `ai` 427; `entities` 76,77,186…; `game` 92,1116,1117…; `hud` 869; +1 modul |
| `builds` | 2: imperium, ooni | `entities` 372 |
| `carry` | 2: imperium, ooni | `entities` 375,884; `game` 1518 |
| `lifetime` | 2: imperium, wullof | `entities` 238,239,398…; `game` 332,1189,1206…; `heroes` 113 |
| `unit_supply` | 2: croftian, wullof | `game` 1213,2993 |
| `ability_path` | 1: croftian | `entities` 346 |
| `always_hidden` | 1: hukucha | `game` 1296 |
| `count` | 1: croftian | `ai` 153,426,836; `campaign` 339,651,659…; `entities` 405,1502; `game` 1232,1704,1932…; +3 modul |
| `find_items` | 1: ooni | `entities` 953 |
| `hidden` | 1: wullof | `campaign` 356,512,755; `game` 1317,1343,3085; `data` 70; `hud` 375,462; +2 modul |
| `mine_stealth` | 1: ooni | `game` 1298,1299 |
| `mine_trigger` | 1: hukucha | `game` 1923 |
| `per_building` | 1: wullof | `ai` 178; `game` 3509,3554; `ui` 1328 |
| `requires_research` | 1: wullof | `game` 644; `data` 155 |
| `shield_delay` | 1: ursan | `entities` 187 |
| `shield_slot` | 1: imperium | **nincs** |
| `shield_zone` | 1: hukucha | `entities` 158 |
| `spawn_group` | 1: wullof | `game` 3161 |
| `tap_wood` | 1: wullof | `entities` 885,886,906 |
| `type_nostrength` | 1: zhan_zhama | `game` 956; `hud` 1034 |
| `xp` | 1: vadon | `ai` 585; `entities` 262; `heroes` 103,104,130…; `hud` 886 |

## 6. Épületkulcsok

| Kulcs | Előfordul | Kód |
|---|---|---|
| `armor` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `entities` 102,358; `game` 979,1135; `heroes` 18; `hud` 899,901,903; +1 modul |
| `cost` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 181,214,253…; `entities` 979,989,1011…; `game` 333,1220,1232…; `heroes` 107; +2 modul |
| `desc` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `hp` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 427,578,726…; `entities` 74,75,133…; `game` 879,1107,1139…; `heroes` 18,88,91…; +3 modul |
| `name` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `produces` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 188,207,341…; `game` 3473; `data` 144; `hud` 373,412,492…; +1 modul |
| `requires` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `campaign` 100,102; `game` 642; `data` 144 |
| `researches` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 414,415; `game` 643,3099,3565…; `data` 55,117,120…; `hud` 381,382,383…; +2 modul |
| `shape` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `game` 661; `buildfx` 29; `calm` 57,97; `render` 4,344,351; +3 modul |
| `sight` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `entities` 105,770,817…; `game` 3059; `hud` 909,910 |
| `size` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 236,237,250…; `campaign` 238; `entities` 1064,1440,1657; `game` 247,2224,2347…; +12 modul |
| `time` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 16,48,49…; `campaign` 406,430,448…; `entities` 141,187,320…; `game` 140,286,288…; +7 modul |
| `ally` | 13: daresh, galoca, ghoblin, mhonyo, micelia, niil_elf (+7) | `ai` 378,382; `game` 52,179,645…; `data` 70,75,123…; `hud` 217,362,364…; +2 modul |
| `attack` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 279,443,468…; `campaign` 302,777; `entities` 81,108,112…; `game` 98,817,829…; +7 modul |
| `attack.cooldown` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `entities` 82,137,140…; `game` 1004,1649,1691…; `audio` 63,239,253; `hud` 332,334,410; +2 modul |
| `attack.damage` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `entities` 128,1351,1361…; `game` 1252; `heroes` 18; `hud` 893,894,895; +1 modul |
| `attack.projectile` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `entities` 147,1351,1360…; `ui` 3363 |
| `attack.range` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 827; `entities` 122,1346,1386…; `game` 849,2893,2929; `ui` 3666,3908 |
| `attack.targets` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `campaign` 622; `entities` 112,1353,1717 |
| `detect` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `game` 1324,1325,1326… |
| `dropoff` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `game` 661,924,3417; `render` 251 |
| `path` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `campaign` 16,17,32…; `entities` 218,348,432…; `game` 49,178,438…; `heroes` 378; +19 modul |
| `supply` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 480; `entities` 405; `game` 1214,1371,1380…; `hud` 1167,1168,1418…; +1 modul |
| `diplomacy` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `game` 688; `hud` 361 |
| `foundation_free` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 249; `game` 2767 |
| `no_creep` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `game` 2301 |
| `no_light` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 291; `game` 2295 |
| `no_pond` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 235,241; `game` 2280 |
| `no_power` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 287; `game` 2275,3007 |
| `powers` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 719,768; `game` 1634,1668; `hud` 392 |
| `rift_source` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `game` 1952; `ui` 3222 |
| `altar` | 6: croftian, elves, imperium, mycel, ursan, wullof | `ai` 558 |
| `no_sprite` | 3: croftian, hukucha, imperium | **nincs** |
| `mine` | 2: croftian, elves | `ai` 618,622,633; `entities` 1706; `game` 1269,1522,1527…; `mission` 332; +1 modul |
| `on_resource` | 2: croftian, elves | `ai` 621; `game` 2240,2242,2311; `ui` 1112,1115 |
| `pray_slots` | 2: elves, hukucha | `ai` 754; `game` 1584,3396; `hud` 384,388,389 |
| `aura` | 1: imperium | `entities` 283; `heroes` 308,343,347; `hud` 320; `ui` 1068,4432,4554 |
| `core` | 1: mycel | `game` 2269,2787 |
| `creep_radius` | 1: croftian | `entities` 1674; `game` 3022 |
| `field_radius` | 1: mycel | `game` 2809 |
| `foundation_tile` | 1: imperium | `game` 2392,2756; `ui` 1138,1394,1896… |
| `garrison` | 1: ursan | `ai` 799,803,805…; `entities` 1180,1677,1712…; `game` 1245,2131,2135…; `hud` 423,425,1169…; +1 modul |
| `hidden` | 1: croftian | `campaign` 356,512,755; `game` 1317,1343,3085; `data` 70; `hud` 375,462; +2 modul |
| `lantern` | 1: ooni | `ai` 217,218,291…; `game` 2295,2299 |
| `larva` | 1: croftian | `ai` 136,345; `entities` 1671,1701,1738…; `game` 280,281,3523…; `hud` 1163,1164; +1 modul |
| `larva_time` | 1: croftian | `entities` 1743 |
| `light_heal` | 1: ooni | `game` 2730 |
| `light_radius` | 1: ooni | `ai` 293; `entities` 179; `game` 742,1987,2298… |
| `link_radius` | 1: mycel | `ai` 270; `game` 2271,2272,2796 |
| `packable` | 1: wullof | `campaign` 187; `game` 2823; `hud` 420; `ui` 1126 |
| `parallel` | 1: croftian | `ai` 135,344; `entities` 1750; `game` 3522,3528,3538… |
| `pond_tile` | 1: hukucha | `ui` 1101,1160,1394… |
| `power_radius` | 1: elves | `ai` 289; `game` 738,2278,3003… |
| `regen_aura` | 1: croftian | `heroes` 328 |
| `requires_research` | 1: imperium | `game` 644; `data` 155 |
| `shield` | 1: elves | `ai` 427; `entities` 76,77,186…; `game` 92,1116,1117…; `hud` 869; +1 modul |
| `shield_reactor` | 1: imperium | `game` 1959 |
| `spike_tile` | 1: croftian | `entities` 836,1339; `game` 1047,3333,3361…; `ui` 878,1103,1149… |
| `sprite` | 1: micelia | `hud` 693; `ui` 89,3404,3408 |
| `torii` | 1: ooni | `entities` 1708; `game` 1828,2286,2292… |
| `wall` | 1: ursan | `game` 661; `ui` 3186,3205 |

## 7. Kutatáskulcsok

| Kulcs | Előfordul | Kód |
|---|---|---|
| `cost` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 181,214,253…; `entities` 979,989,1011…; `game` 333,1220,1232…; `heroes` 107; +2 modul |
| `desc` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `effects` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `entities` 1371; `game` 146,318,324…; `data` 99; `sprites` 223; +1 modul |
| `name` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `time` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 16,48,49…; `campaign` 406,430,448…; `entities` 141,187,320…; `game` 140,286,288…; +7 modul |
| `ally` | 13: daresh, galoca, ghoblin, mhonyo, micelia, niil_elf (+7) | `ai` 378,382; `game` 52,179,645…; `data` 70,75,123…; `hud` 217,362,364…; +2 modul |
| `requires_ascended` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `game` 648 |
| `requires_research` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `game` 644; `data` 155 |
| `rules` | 7: croftian, elves, hukucha, imperium, ooni, ursan (+1) | `ai` 237,688,698; `entities` 19,178,196…; `game` 51,307,730…; `hud` 1440; +2 modul |
| `exclusive` | 2: imperium, wullof | `game` 3565,3569 |
| `icon` | 1: imperium | `hud` 215,278,345…; `ui` 1214,1288 |
| `requires` | 1: imperium | `campaign` 100,102; `game` 642; `data` 144 |

## 8. Fejlődési ágak (`paths`) kulcsai

| Kulcs | Előfordul | Kód |
|---|---|---|
| `building` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 178; `entities` 72,73; `game` 95,97,3440…; `data` 125; +3 modul |
| `desc` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `effects` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `entities` 1371; `game` 146,318,324…; `data` 99; `sprites` 223; +1 modul |
| `name` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `rules` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 237,688,698; `entities` 19,178,196…; `game` 51,307,730…; `hud` 1440; +2 modul |
| `tagline` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `hud` 480,765,1224; `menu` 867; `ui` 1274 |
| `units` | 8: croftian, elves, hukucha, imperium, mycel, ooni (+2) | `ai` 50,433,483…; `campaign` 181,184,194…; `entities` 766,1531; `game` 143,273,310…; +12 modul |
| `extra_buildings` | 2: elves, ursan | **nincs** |

## 9. Szabálymódosítók (`rules`, ágakból és kutatásokból)

| Kulcs | Előfordul | Kód |
|---|---|---|
| `faith_rate` | 2: elves, hukucha | `game` 1618; `hud` 1440 |
| `forest_stealth` | 2: elves, mycel | `game` 1307,1308 |
| `no_weak` | 2: imperium, ursan | `game` 962,966 |
| `poison_attacks` | 2: croftian, mycel | `game` 1074 |
| `raise_from_kills` | 2: imperium, mycel | `game` 1205 |
| `bounty_gold` | 1: wullof | `game` 1236 |
| `build_boost_mult` | 1: ooni | `game` 1494 |
| `convert_kills` | 1: croftian | `game` 1211 |
| `creep_regen_mult` | 1: croftian | `entities` 196 |
| `detect_all` | 1: wullof | `game` 1329 |
| `drum_mult` | 1: ooni | `game` 1999 |
| `energy_regen_ids` | 1: elves | `entities` 200 |
| `garrison_dmg` | 1: ursan | `entities` 1718 |
| `heal_on_kill` | 1: wullof | `game` 1201 |
| `kill_heal` | 1: ooni | `game` 1222 |
| `kokutya_guard` | 1: ooni | `game` 2733 |
| `lantern_magic` | 1: ooni | `entities` 178 |
| `larva_add` | 1: croftian | `entities` 1738 |
| `larva_mult` | 1: croftian | `entities` 1743 |
| `light_add` | 1: ooni | `game` 742 |
| `low_hp_rage` | 1: ursan | `game` 1106 |
| `moon` | 1: wullof | `game` 307,1098 |
| `morale_hurt_mult` | 1: ooni | `game` 1145 |
| `network_supply_add` | 1: mycel | `game` 2983 |
| `pack_bonus` | 1: imperium | `game` 764,772 |
| `pond_dmg` | 1: hukucha | `game` 1101 |
| `pond_size` | 1: hukucha | `ai` 237; `game` 2436,2468,2603; `ui` 1102,1166,3595… |
| `power_add` | 1: elves | `game` 738 |
| `rage` | 1: ursan | `entities` 131; `ui` 4425,4500 |
| `rot_cap` | 1: croftian | `game` 1793 |
| `rot_mult` | 1: croftian | `game` 1803 |
| `spike_max` | 1: croftian | `game` 2500 |
| `spike_mult` | 1: croftian | `game` 2555,2581 |
| `spike_size` | 1: croftian | `ai` 688,698; `game` 2510; `ui` 1104,1155,3578… |
| `summon_life_mult` | 1: hukucha | `entities` 1530; `game` 1707 |
| `supply_add` | 1: ursan | `game` 2978 |
| `torii_pairs` | 1: ooni | `game` 2293 |
| `train_time_mult` | 1: croftian | `game` 3541 |

## 10. Hatások: statnevek és hatáskulcsok (`effects`)

| Kulcs | Előfordul | Kód |
|---|---|---|
| `<effect-kulcs> add` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `entities` 73,1545,1630…; `game` 425,1338,1749…; `mission` 95,106,146…; `audio` 170; +8 modul |
| `<effect-kulcs> stat` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `entities` 74,76,78…; `game` 3143,3147,3152…; `hud` 894 |
| `<effect-kulcs> tags` | 21: croftian, daresh, elves, galoca, ghoblin, hukucha (+15) | `ai` 53,858; `entities` 71,73,239…; `game` 94,749,750…; `heroes` 312,317; +1 modul |
| `armor` | 18: croftian, daresh, elves, galoca, ghoblin, hukucha (+12) | `entities` 102,358; `game` 979,1135; `heroes` 18; `hud` 899,901,903; +1 modul |
| `damage` | 18: croftian, daresh, elves, galoca, ghoblin, hukucha (+12) | `entities` 128,1351,1361…; `game` 1252; `heroes` 18; `hud` 893,894,895; +1 modul |
| `speed_pct` | 14: croftian, daresh, hukucha, imperium, micelia, mycel (+8) | `entities` 380; `heroes` 18; `hud` 908 |
| `hp` | 13: elves, hukucha, imperium, mhonyo, micelia, mycel (+7) | `ai` 427,578,726…; `entities` 74,75,133…; `game` 879,1107,1139…; `heroes` 18,88,91…; +3 modul |
| `regen` | 6: croftian, elves, galoca, hukucha, micelia, ursan | `entities` 191,674,677…; `game` 1972,1996,1999; `heroes` 18; `ui` 3908 |
| `sight` | 5: elves, imperium, ooni, tenduur, wullof | `entities` 105,770,817…; `game` 3059; `hud` 909,910 |
| `range` | 4: elves, ghoblin, hukucha, ooni | `ai` 827; `entities` 122,1346,1386…; `game` 849,2893,2929; `ui` 3666,3908 |
| `shield` | 2: elves, imperium | `ai` 427; `entities` 76,77,186…; `game` 92,1116,1117…; `hud` 869; +1 modul |
| `carry_pct` | 1: imperium | `entities` 376 |
| `energy` | 1: imperium | `entities` 78,79,198…; `game` 92,3143,3154; `heroes` 18,89,93…; `hud` 338,339,866; +2 modul |

## 11. Képességkulcsok (melyik típusban)

| Kulcs | Előfordul | Kód |
|---|---|---|
| `desc` | 23: anchor_rescue, area, aura, autoheal, blink, bolt (+17) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `icon` | 23: anchor_rescue, area, aura, autoheal, blink, bolt (+17) | `hud` 215,278,345…; `ui` 1214,1288 |
| `name` | 23: anchor_rescue, area, aura, autoheal, blink, bolt (+17) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `type` | 23: anchor_rescue, area, aura, autoheal, blink, bolt (+17) | `ai` 73,85,88…; `campaign` 296,617,839…; `entities` 24,282,475…; `game` 279,483,487…; +5 modul |
| `hotkey` | 20: area, aura, autoheal, blink, bolt, bpower (+14) | **nincs** |
| `cooldown` | 12: area, autoheal, blink, bolt, bpower, gpower (+6) | `entities` 82,137,140…; `game` 1004,1649,1691…; `audio` 63,239,253; `hud` 332,334,410; +2 modul |
| `energy` | 10: area, autoheal, blink, bolt, lay_mine, nova (+4) | `entities` 78,79,198…; `game` 92,3143,3154; `heroes` 18,89,93…; `hud` 338,339,866; +2 modul |
| `levels` | 9: area, aura, autoheal, bolt, nova, raise (+3) | `heroes` 29,30,44… |
| `ally` | 8: area, autoheal, bolt, chant, nova, self_buff (+2) | `ai` 378,382; `game` 52,179,645…; `data` 70,75,123…; `hud` 217,362,364…; +2 modul |
| `duration` | 7: area, bolt, bpower, gpower, nova, self_buff (+1) | `entities` 1353,1400,1479…; `game` 1650,1715,1726…; `ui` 3903 |
| `range` | 7: area, autoheal, blink, bolt, merge, mount (+1) | `ai` 827; `entities` 122,1346,1386…; `game` 849,2893,2929; `ui` 3666,3908 |
| `color` | 6: area, blink, gpower, nova, overdrive, ward | `campaign` 697,759; `entities` 151,1317,1321…; `game` 42,175,180…; `heroes` 407; +5 modul |
| `radius` | 6: area, aura, chant, gpower, nova, ward | `ai` 732; `campaign` 337,642,654…; `entities` 48,50,51…; `game` 394,447,456…; +5 modul |
| `faith` | 5: area, bpower, gpower, nova, summon | `ai` 205,724,748…; `entities` 287,297,298; `game` 1599,1619,1620…; `hud` 77,147,336…; +1 modul |
| `modifiers` | 5: aura, chant, passive, self_buff, toggle | `entities` 322,1626; `heroes` 34,35,313…; `ui` 3909 |
| `hp_cost` | 4: area, bolt, nova, self_buff | `entities` 289,299,300; `hud` 340,341; `ui` 3897,3898 |
| `slow` | 4: area, bolt, gpower, nova | `entities` 1353,1478,1479…; `game` 997,1063,1079…; `ui` 2418,2617,2627… |
| `ultimate` | 4: area, nova, self_buff, summon | `entities` 1590; `heroes` 48,184; `ui` 3919 |
| `count` | 3: gpower, merge, summon | `ai` 153,426,836; `campaign` 339,651,659…; `entities` 405,1502; `game` 1232,1704,1932…; +3 modul |
| `heal` | 3: area, bpower, nova | `ai` 580; `entities` 1431,1466,1470…; `game` 1204,1438,1614…; `heroes` 251,255,256…; +2 modul |
| `into` | 3: merge, morph, mount | `game` 1874,2176,2193… |
| `spores` | 3: area, bolt, nova | `entities` 291,301,302; `game` 1183,1184,1350…; `faction_ui` 27,199,234; `hud` 342,343,1190…; +1 modul |
| `target_placement` | 3: lay_mine, morph, summon | `game` 1879; `hud` 324; `ui` 1061 |
| `unit` | 3: gpower, lay_mine, summon | `entities` 1499,1740; `game` 1207,1243,1492…; `data` 125; `hud` 170,378,695…; +3 modul |
| `amount` | 2: autoheal, ward | `campaign` 632,639,680…; `entities` 921,924,926…; `game` 343,344,1203…; `mission` 166; +5 modul |
| `damage` | 2: bolt, nova | `entities` 128,1351,1361…; `game` 1252; `heroes` 18; `hud` 893,894,895; +1 modul |
| `dps` | 2: area, gpower | `entities` 1836,1837,1840…; `game` 1076,2477,2556…; `ui` 2642,3902 |
| `effect` | 2: bpower, gpower | `ai` 772; `game` 1645,1651,1656…; `ui` 1900,3355,3404… |
| `lifetime` | 2: gpower, summon | `entities` 238,239,398…; `game` 332,1189,1206…; `heroes` 113 |
| `after_slow` | 1: self_buff | `entities` 1403,1404 |
| `bounces` | 1: bolt | `entities` 1354,1788,1797 |
| `buff` | 1: gpower | `game` 1712,1715,1717…; `ui` 2411,2618,2628… |
| `building_damage` | 1: nova | `entities` 1480,1484,1487… |
| `buildings` | 1: gpower | `ai` 51,269,436…; `campaign` 185,187,195…; `entities` 180,1840; `game` 144,313,322…; +10 modul |
| `burrow` | 1: toggle | `entities` 1542; `ui` 4425,4512 |
| `cost` | 1: morph | `ai` 181,214,253…; `entities` 979,989,1011…; `game` 333,1220,1232…; `heroes` 107; +2 modul |
| `detect` | 1: passive | `game` 1324,1325,1326… |
| `drop_work` | 1: self_buff | `entities` 1406; `game` 1716 |
| `heal_tags` | 1: autoheal | `entities` 1425 |
| `hq_hp` | 1: gpower | `ai` 726; `game` 1685,1689,1690; `hud` 398,407 |
| `land_damage` | 1: blink | `entities` 1322,1324,1327 |
| `land_radius` | 1: blink | `entities` 1323,1325 |
| `large` | 1: raise | `game` 1212,1259,2961… |
| `max` | 1: lay_mine | `campaign` 341,597; `entities` 133; `game` 785,1607,1909…; `hud` 390,872,1443; +1 modul |
| `meteor` | 1: gpower | `ai` 79,205,424; `entities` 954; `game` 192,275,1305…; `points` 137,142; +6 modul |
| `mount` | 1: dismount | `ai` 818; `game` 2883; `ui` 1080 |
| `mult` | 1: bpower | `game` 1100,1108,1494… |
| `no_weak` | 1: self_buff | `game` 962,966 |
| `poison` | 1: bolt | `entities` 242,696,697…; `game` 1036,1040,1041…; `hud` 936; `savegame` 187; +2 modul |
| `projectile` | 1: bolt | `entities` 147,1351,1360…; `ui` 3363 |
| `range_research` | 1: blink | `entities` 1387 |
| `reveal` | 1: gpower | `game` 1697,1699 |
| `rider` | 1: dismount | `game` 2882; `render` 66 |
| `riders` | 1: mount | `ai` 821; `game` 2892,2895 |
| `self_destruct` | 1: nova | `entities` 1491 |
| `self_hp` | 1: overdrive | `game` 2121 |
| `share` | 1: overdrive | `ai` 89,91,114…; `game` 2112 |
| `small` | 1: raise | `game` 2962; `fonts` 9,10,36; `hud` 84,792,794…; `menu` 271,423,440…; +2 modul |
| `splash` | 1: bolt | `entities` 1354,1800,1801; `game` 936,937,940… |
| `split_kills` | 1: passive | `game` 1185,1356,1364; `hud` 1190,1191; `ui` 2848 |
| `sprite` | 1: area | `hud` 693; `ui` 89,3404,3408 |
| `stun` | 1: nova | `entities` 1476,1477,1478; `ui` 2618,2628,2660… |
| `target` | 1: gpower | `ai` 728; `campaign` 622,659,661…; `entities` 85,144,755…; `game` 1153,1676,1939…; +2 modul |
| `time` | 1: morph | `ai` 16,48,49…; `campaign` 406,430,448…; `entities` 141,187,320…; `game` 140,286,288…; +7 modul |
| `via` | 1: morph | `game` 1874,2191 |

## 12. Módosítók (`modifiers`: képességek és tárgyak)

| Kulcs | Előfordul | Kód |
|---|---|---|
| `speed_mult` | 45: beasas, dar_gyantafal, dar_rajenek, dardafal, fegyverbe, gal_harapas (+39) | `entities` 382; `ui` 3912 |
| `damage_mult` | 36: dar_rajenek, dardafal, fegyverbe, gal_harapas, geppuska_sorozat, gyorstoltes_u (+30) | `entities` 129 |
| `armor_add` | 22: dar_gyantafal, dardafal, fegyverbe, gho_kofal, h_bul_kiraly, h_cas_harag (+16) | `entities` 686 |
| `range_add` | 7: jade_celzas, lesallas, napfokusz, ostrom_telepites, ostrommod, telepites (+1) | `entities` 121,687,1626; `game` 1971 |
| `ranged_taken_mult` | 4: huk_kagylofal, kalapfal, pajzsfal, testudo | `game` 1110 |
| `regen_add` | 2: beasas, runako_aura | `entities` 193; `game` 1974; `heroes` 334 |

## 13. Tárgykulcsok

| Kulcs | Előfordul | Kód |
|---|---|---|
| `desc` | 15: arnykopeny, csizma, eletamulett, eletertom, emlekkonyv, ercpenge (+9) | `hud` 366,390,480…; `menu` 869,949; `setup_screen` 442; `ui` 1276,3921,3932… |
| `icon` | 15: arnykopeny, csizma, eletamulett, eletertom, emlekkonyv, ercpenge (+9) | `hud` 215,278,345…; `ui` 1214,1288 |
| `kind` | 15: arnykopeny, csizma, eletamulett, eletertom, emlekkonyv, ercpenge (+9) | `ai` 85,88,89…; `entities` 25,72,867…; `game` 95,342,887…; `heroes` 84,249; +11 modul |
| `name` | 15: arnykopeny, csizma, eletamulett, eletertom, emlekkonyv, ercpenge (+9) | `campaign` 158,160,265…; `entities` 92,801; `game` 40,86,249…; `heroes` 137,206,242…; +12 modul |
| `rarity` | 15: arnykopeny, csizma, eletamulett, eletertom, emlekkonyv, ercpenge (+9) | `heroes` 287; `ui` 2906,3927,3928 |
| `bonus` | 10: arnykopeny, csizma, eletamulett, ercpenge, eterko, hosipajzs (+4) | `entities` 128,191,199…; `game` 1094; `heroes` 73,85,90; `hud` 895,908 |
| `perm` | 2: eletertom, erotom | `ai` 585; `entities` 265; `heroes` 80,153,166… |
| `energy` | 1: eterfozet | `entities` 78,79,198…; `game` 92,3143,3154; `heroes` 18,89,93…; `hud` 338,339,866; +2 modul |
| `heal` | 1: gyogyfozet | `ai` 580; `entities` 1431,1466,1470…; `game` 1204,1438,1614…; `heroes` 251,255,256…; +2 modul |
| `xp` | 1: emlekkonyv | `ai` 585; `entities` 262; `heroes` 103,104,130…; `hud` 886 |

## 14. Egyéb adatállományok

- `data/elements.json`: kulcsai `_leiras`, `effects`, `abilities`, `units`, `kezi`
- `data/camps.json`: kulcsai `_leiras`, `recipe`, `peoples`
- `data/game.json`: `title`, `map`, `sim_hz`, `start_resources`, `max_supply`, `summon_research`, `resources`, `shield_regen_delay`, `shield_regen_rate`, `energy_regen`, `creep_speed_mult`, `creep_regen_mult`, `player_colors`, `subtitle`, `supply_name`, `creator`, `copyright`, `player_color_names`, `pond_speed`, `rift`, `unit_types`, `upkeep`

## 15. Kódhivatkozás nélküli adatkulcsok (kézzel ellenőrizendő)

4 kulcs: `unit:shield_slot`, `building:no_sprite`, `path:extra_buildings`, `ability:hotkey`

## 16. A kódba égetett tartalomazonosítók („rejtett szabályok”)

Ezekre az egységekre, épületekre, kutatásokra, képességekre vagy tárgyakra a Python-kód **név szerint** ágazik el. Az új motorban mindegyikből általános, adatból vezérelt szabály vagy a tartalomhoz tartozó Lua-szkript lesz.

| Azonosító | Fajta | Kód |
|---|---|---|
| `oon_kokutya` | egység | `game` 2733 |
| `oon_nian_or` | egység | `game` 1059 |
| `wul_emu` | egység | `mission` 17 |
| `wul_hos_tava` | egység | `campaign` 344 |
| `wul_landzsas` | egység | `game` 2882 |
| `imp_lelekhorgony_mentes` | kutatás | `savegame` 156 |
| `imp_nagy_alapozas` | kutatás | `ai` 251,253,255 |
| `erdei_rejtozes` | képesség | `game` 1307 |
| `lelekhorgony_mentes` | képesség | `game` 1412; `savegame` 157 |
| `oon_kapuorzes` | képesség | `game` 1059,2739,3688 |
| `cro_merges_tuskek` | épület | `ai` 687; `game` 2506,2542 |
| `imp_alapozas` | épület | `ai` 262,263; `game` 2388 |

## 17. Varázserő-szabály (D22)

A nem hős, nem `caster` címkés egység képessége csak a faj varázserő-forrásából fizethet (vagy csak újratöltés), ha az nem az energia. Sértések: **nincs**.
