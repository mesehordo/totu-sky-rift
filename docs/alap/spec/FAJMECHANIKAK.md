# Fajmechanikák és faji rendszerek – viselkedési specifikáció

Kelt: 2026-11-12 · referencia: a Python-játék (`G` = `rts/game.py`, `E` = `rts/entities.py`, `H` = `rts/heroes.py`).
Formátum és jelölések: [README.md](README.md). Az értékek a mai `data/` szerint; a modok felülírhatják őket.

Tartalom: 1. közös faji rendszerek · 2. Mélység (Croftianok) · 3. Tó (Huk’ucha) · 4. Energiamező (Szürke elfek) ·
5. Hálózat (Micéliák) · 6. Lámpásfény és Harci kedv (Ooni) · 7. Standard népek: Ursanok, Wullof, Aurethi Birodalom.

---

## 1. Közös faji rendszerek

### MECH-ASC – Felemelkedés és fejlődési irány

- **Adat:** faj `ascension.buildings` (mind a 8 főfajnál 3); faj `paths.<id>`: `building`, `units`, `rules`,
  `effects` (lásd KUT-…); egységen/épületen `path`, `requires_ascended`.
- **Szabály:**
  1. 15 tickenként (0,5 s) minden még fel nem emelkedett, nem legyőzött, nem semleges, `paths`-szal bíró játékosnál
     megszámolja a **különböző fajtájú, kész, élő** saját épületeket; nem számít a főépület (`faction.hq`) és a
     `shape` ∈ {`wall`, `node`, `dropoff`} épület.
  2. Ha a szám ≥ `ascension.buildings` → `ascended = true`, értesítés; gépi játékos azonnal választ irányt.
  3. Irányválasztás (`cmd_choose_path`): csak felemelkedve, egyszer. `rules` = a meglévő szabályok + az irány
     szabályai (`merge_rules`, a kutatásokból jöttek megmaradnak); az irány `effects` hatásai kutatásként érvényesülnek.
  4. Az irányhoz kötött épület/egység (`path`) és a felemelkedéshez kötött (`requires_ascended`) csak ezután
     építhető; **hadjáratban** (rendező fut) nincs felemelkedés és nincs irányzár.
- **Kód:** G 656–734, G 641 (`missing_requirements`).
- **Próba:** P1 – egyéni csata, Ursanok: 2 különböző kész épület → nincs felemelkedés; a 3. elkészülte után
  legfeljebb 15 tickkel `ascended = true`. P2 – 3 azonos fajta épület → nincs felemelkedés. P3 – irányválasztás
  után az irány épülete építhető, a másik irányé nem.
- **Új motorban:** C++: számlálás (eseményre is, nem csak időzítve); a választás és az irányszabályok adatból.

### MECH-RIFT – Égkő-fény (meteorit és Égkő közelében)

- **Adat:** `game.rift`: `radius` 5, `energy_mult` 1,5, `mage_range` 1; faj `rift`: elfek `energy_mult` 2,0,
  `spell_mult` 1,25, `shield_regen_mult` 2,0; croftian `regen: {units: [cro_agyfalo, cro_csapos_horror], add: 2}`;
  Aurethi Birodalom `regen: {tags: [mech], add: 1}`; épület `rift_source`, `shield_reactor`.
- **Szabály:**
  1. 15 tickenként (a hős-aurák frissítésekor) minden egység `rift = false`, `near_reactor = false`.
  2. Forrás: minden élő meteorit-lelőhely, minden kész `rift_source` épület (bárkié) és minden Hasadékkő-pont.
     A forrástól `radius` csempén belüli egység `rift = true`.
  3. A kész `shield_reactor` épület 6 csempés körében a **saját** egységek `near_reactor = true`.
  4. `rift` egységnél: `caster` címkéjű, támadó egység `range_add = mage_range`; a faji `rift.regen` szerint
     `regen_add`. Az aura-módosítókba maximumként kerül (nem halmozódik).
  5. A faji szorzók (`rift_rule`): ha az egység `rift`, a faj `rift.<kulcs>`, különben a `game.rift.<kulcs>`
     értéke; nem `rift` egységnél 1. Hatás: energiatöltés × `energy_mult`, varázssebzés × `spell_mult`,
     pajzstöltés × `shield_regen_mult`.
- **Kód:** G 1937–1971 (a `H 306` hívja), E 167–203.
- **Próba:** P1 – elf varázsló meteorittól 4 csempére: energiatöltés `0,8 × 2,0` /s; 6 csempére: 0,8/s.
  P2 – ursan varázsló 4 csempére: `0,8 × 1,5`. P3 – croftian Agyfaló 4 csempére: életerő-töltés +2/s a faji 0,6-on felül.
- **Új motorban:** C++-rendszer (térbeli lekérdezés), a szorzók adatból.

### MECH-FAITH – Áhítat (Szürke elfek) és Tóének (Huk’ucha)

- **Adat:** faj `faith`: `rate` 0,3, `max` 8; épület `pray_slots`; a nyersanyag neve fajonként (`resource_names`).
  Kutatás: `rules.faith_rate`.
- **Szabály:**
  1. A munkás `pray` parancsot kap egy szentélyre; odamegy, és ott áll.
  2. 15 tickenként: a fohászkodók (élő, nem bent, nem kábult/alvó/félő/beásott) azonosító szerinti sorrendben
     aktívak lesznek, amíg a játékosnál ≤ `max` és szentélyenként ≤ `pray_slots`; a többi inaktív.
  3. Bevétel: `aktívak × rate × 0,5 s` (`faith` nyersanyag), a gyűjtött statisztikába is.
  4. Az Áhítatból fizetett képességek: `bpower` (épületképesség) és egységképesség `faith` költséggel (KEP-…).
- **Kód:** G 1579–1617, E 1093.
- **Próba:** P1 – 3 fohászkodó egy 2 helyes szentélynél: 2 aktív, 10 s alatt `2 × 0,3 × 10 = 6` Áhítat.
  P2 – 10 fohászkodó 3 szentélynél (3 hely mindegyikben): 8 aktív.
- **Új motorban:** Lua-modul (fajmechanika) + C++ időzítő.

### MECH-MINE – Bányára épülő épületek (Napfúró, Fertőzött tárna)

- **Adat:** épület `on_resource` (`["stone", "meteor"]`), `mine`: `slots`, `consume`, `passive` – Napfúró
  (`elf_napfuro`): 3 hely, `passive` 0,4; Fertőzött tárna (`cro_tarna`): 5 hely, `consume`, `passive` 0; nyersanyag `carry`, `gather_time`.
- **Szabály:**
  1. Csak a lelőhely bal felső csempéjére építhető, ha az épület belefér, és a lelőhelyen még nincs bánya.
  2. Egy munkás hozama: `mine_rate = carry / (gather_time + 7,4)` egység/s.
  3. Bent dolgozó munkás (`slots`-ig): az épületbe lép, eltűnik a pályáról. `consume` (tárna): a szolga végleg
     beleolvad (elfogy), és a helyét a `mine_n` számláló tartja.
  4. Tickenként (az épület frissítésekor): `acc += (passive + bent + beolvadt) × mine_rate × dt × gyorsítás`
     (`mine_boost`, Gyöngyérés/Napáldás); az egész rész levonódik a
     lelőhelyből és bevétel lesz (`income`, Hadtáppal). Kimerüléskor a bánya elpusztul, a munkások kijönnek.
  5. Ha a bánya elpusztul, a bent lévők a fél életerejükkel kijönnek.
- **Kód:** G 1511–1576, 2233–2247, 1772.
- **Próba:** P1 – arany-Napfúró 3 munkással, Hadtáp nélkül, 60 s: `(0,4 + 3) × 6/(2,6 + 7,4) × 60 = 122,4` → 122 arany
  (± 1, egészre kerekítés). P2 – Fertőzött tárna 5 beolvadt szolgával: `5 × 0,6 × 60 = 180` arany.
- **Új motorban:** C++ (gazdaság), az épületdefiníció adatából.

### MECH-BUILD – Építési stílusok

- **Adat:** faj `builder`: `stay` | `stay_single` | `enter` | `consume` | `leave`; ha nincs megadva: `creep` → `consume`,
  `standard` → `stay`, más → `leave`. Mai értékek: Ursanok `enter`, Wullof `stay`, Aurethi Birodalom és Ooni
  `stay_single`, Huk’ucha `leave`, Croftian (mechanika) `consume`, Szürke elfek és Micéliák (alap) `leave`.
- **Szabály – haladás (`build_rate`, tickenként):**
  - `stay`: 0 építő → 0; n építő → `1 + 0,5 × (min(n, 4) − 1)` (4 építőig gyorsul);
  - `stay_single`: van építő → 1, különben 0;
  - `enter`: az épületbe lépett építő él → 1, különben 0;
  - `consume`, `leave`: mindig 1 (az építő elfogy, illetve elmehet);
  - letelepített Wullof-épület (`self_build`): mindig 1;
  - szorzók: Huk’ucha `× (1 + pond_build_bonus × tógyűrű-arány)` (MECH-POND-4); Ooni: ha a `build_boost.unit`
    (Taiko-dobos) `radius` csempén belül áll → `× rules.build_boost_mult` vagy `mult` (1,5).
  - Az épülő épület életereje a haladással nő: `+ max_hp × 0,9 × lépés / build_time` (pajzs ugyanígy).
- **Kód:** G 1460–1491, E 1690–1705.
- **Próba:** P1 – Wullof-épület 1, 2, 4, 6 építővel: idő `T`, `T/1,5`, `T/2,5`, `T/2,5`. P2 – Aurethi Birodalom 3 építővel: `T`.
- **Új motorban:** C++ (építés), a stílus és a szorzók adatból.

### MECH-SUPPLY – Létszám

- **Adat:** `game.max_supply` 200; épület `supply`; egység `supply`, `count`, `unit_supply`; faj `network_supply`;
  szabály `supply_add`, `network_supply_add`; kampány `bonus_supply`.
- **Szabály (minden tickben újraszámolva):**
  1. `cap` = Σ kész épület `supply` + `rules.supply_add[épület]` + (Micélia: minden **kapcsolt** épület
     `network_supply + network_supply_add`) + Σ élő egység `unit_supply` (Croftian Szemhordozó: 8) + `bonus_supply`;
     legfeljebb `max_supply`.
  2. `used` = Σ a termelési sorok elemeinek `supply`-ja + Σ élő egység `supply()`; átalakuló egységnél a cél
     `supply / count` értéke.
- **Kód:** G 2961–2988.
- **Próba:** P1 – Croftian: Kaptár (10) + 2 Szemhordozó → cap 26. P2 – Micélia: 5 kapcsolt + 1 leszakadt épület → +5.

### MECH-UPKEEP – Hadtáp (WC3-szerű fenntartás)

- **Adat:** `game.upkeep.tiers` `[[80, 1,0], [140, 0,7], [200, 0,4]]`; a játék `upkeep_on` kapcsolója.
- **Szabály:** ha be van kapcsolva, **minden aranybevétel** (`stone`: hordás, bánya, zsákmány) × a felhasznált létszám
  sávjának szorzója (≤ 80: 100%, ≤ 140: 70%, különben 40%).
- **Kód:** G 2038–2054. **Próba:** P1 – 100 létszámnál 6 arany hordás → 4,2.

### MECH-ALLY – Szövetség egy kisnéppel (Diplomácia csarnoka)

- **Adat:** faj `ally_cost` (mind a 7 főfajnál `stone` 150, `meteor` 50); épület `diplomacy`; kisnép `ally_pack`
  (`buildings`, `units`); elem `ally`.
- **Szabály:** kész Diplomácia csarnoka kell; egyszer, véglegesen; a költség levonódik; utána a kisnép
  `ally` jelölésű elemei elérhetők. A gép a faj `ai.ally_pref` listájából 80%-ban az első kettő közül választ
  véletlenszerűen, különben az egész listából (a szimuláció véletlenszámával).
- **Kód:** G 687–722. Részletek: `KISFAJOK.md` (tervezett).

---

## 2. Mélység – Croftianok (`mechanic: creep`)

### MECH-CREEP-1 – A mélység terjedése

- **Adat:** épület `creep_radius` (Kaptár 10, Tápgomba 8, Emésztő 4 …), `size`.
- **Szabály:**
  1. Kezdő sugár (`creep_r`): kész épületnél `creep_radius`, építés alatt állónál 2,0.
  2. 30 tickenként (1 s): minden kész épület `creep_r = min(creep_radius, creep_r + 0,7)`.
  3. Lefedés: kész épületnél `r = creep_r + max(w, h) / 2`, épülőnél `1 + max(w, h) / 2`; a középpont körüli körben
     minden **építhető terepű** csempe mélység (MECH közös körlefedés).
  4. Épület elkészülte és pusztulása azonnal újraszámol (növekedés nélkül).
  5. Csak a saját mélység számít; minden játékos külön rácsot kap.
- **Kód:** G 3003–3032, E 1674.
- **Próba:** P1 – üres, sík pályán kész Tápgomba építése után 1 s, 5 s, 10 s: a mélységcsempék száma a referenciával
  azonos (tűrés 0). P2 – az épület lerombolása után a mélysége azonnal eltűnik.
- **Új motorban:** C++-rendszer (rácsművelet), növekedés és sugár adatból.

### MECH-CREEP-2 – Építés csak mélységre

- **Szabály:** a `no_creep` nélküli Croftian-épület minden csempéjének saját mélységen kell állnia (a Kaptár `no_creep`).
- **Kód:** G 2294. **Próba:** P1 – mélységen kívüli helyre építés elutasítva.

### MECH-CREEP-3 – Mozgás és gyógyulás a mélységen

- **Adat:** `game.creep_speed_mult` 1,3, `game.creep_regen_mult` 3,0; faj `regen` 0,6; szabály `creep_regen_mult`.
- **Szabály:** saját mélységen álló **földi** egység sebessége × 1,3. Életerő-töltés: `faj regen + stat regen +
  bónusz + aura`; saját mélységen × `creep_regen_mult × rules.creep_regen_mult` (épületre is, ha kész).
- **Kód:** E 387, E 191–197, G 754, 797. **Próba:** P1 – Karmos sebessége mélységen és azon kívül: arány 1,3.

### MECH-CREEP-4 – A mélységen álló fák elrothadnak (fa-bevétel)

- **Adat:** faj `creep_rot`: `rate` 0,06, `hq_mult` 2,5, `hq_radius` 9, `cap` (alap 6,5); szabály `rot_mult`, `rot_cap`.
- **Szabály:** 30 tickenként (`dt` = 1 s), játékosonként:
  1. `keret = rot_cap (vagy cap) × dt`; a saját mélységen álló fák **a legközelebbi saját főépülettől mért távolság**
     szerint növekvő sorrendben (egyenlőségnél csempe-sor, majd -oszlop; főépület nélkül csak ez) – D21.
  2. Minden saját mélységen álló fa: `ütem = rate × rot_mult`, a saját főépülettől `hq_radius` csempén belül × `hq_mult`;
     `elvesz = min(maradék fa, ütem × dt, keret)`; a keret csökken; a fa „rothad”.
  3. Az elvett mennyiség gyűlik; az egész rész fa-bevétel lesz (a gyűjtött statisztikába is). Az elfogyott fa eltűnik.
- **Kód:** G 1781–1815.
- **Próba:** P1 – 10 fa a Kaptártól 5 csempére, mélységen: 60 s alatt `10 × 0,06 × 2,5 × 60 = 90` fa (keret alatt).
  P2 – 200 fa: a bevétel a kerettel korlátozott (≤ 6,5/s).
- **Döntés (D21):** a mélység a bázis felől „eszi” az erdőt. P3 – 400 fa a mélységen: az első percben a főépülethez
  legközelebbi fák fogynak, a távoliak érintetlenek.

### MECH-CREEP-5 – Fertőzés (ölésből kikelő Karmos)

- **Adat:** faj `infest`: `unit` cro_karmos, `chance` 0,3, `delay` 6.
- **Szabály:** ha a croftian egység öl, és az áldozat ellenséges, `organic` címkéjű, nem hős, nem idézett, nem semleges,
  és **a gyilkos tulajdonosának mélységén** halt meg → `chance` valószínűséggel `delay` s múlva a helyén kikel egy `unit`.
- **Kód:** G 1235–1240, 355–362. **Próba:** P1 – 1000 ilyen ölésből a kikelések aránya 0,3 ± 0,03 (rögzített maggal azonos).

### MECH-CREEP-6 – Lárvák (a Kaptár párhuzamos termelése)

- **Adat:** épület `larva` (Kaptár 4), `larva_time` 5,5, `parallel`; szabály `larva_add`, `larva_mult`.
- **Szabály:** a kiképzés egy lárvát használ (a legtöbb lárvájú épületből); a lárva újratermelődik `larva_time /
  larva_mult` s-onként, amíg `lárvák + sorban lévő egységek < larva + larva_add`. Visszavont kiképzés visszaadja.
  A `parallel` épület a sorát egyszerre termeli.
- **Kód:** E 1738–1747, G 3514–3530, 3604. **Próba:** P1 – 4 lárva elfogy, a 5. egység csak 5,5 s után indul.

### MECH-CREEP-7 – Mérgező tüskék

- **Adat:** épület `cro_merges_tuskek` (`cost`, `time` 2, `hp`); kód-állandók `slow` 0,6, `dps` 4,0, `max` 16;
  szabály `spike_size`, `spike_max`, `spike_mult`.
- **Szabály:**
  1. A Csápszolga odamegy és lerak egy `spike_size × spike_size` foltot (nem olvad bele); legfeljebb `spike_max` csempe.
  2. 15 tickenként minden földi, nem bent lévő, **nem `spike_immune` fajú** egység, amely tüskés csempén áll:
     lassítás `max(0,2, 1 − 0,4 × spike_mult)` 0,7 s-ig; méreg `4 × spike_mult` /s 2 s-ig.
  3. A tüske támadható célpont, de **nem blokkolja** az útvonalat. Tűz- és méregsebzés teljes, más × 0,2, és a nem
     `spike_immune` fajú támadót megmérgezi.
- **Kód:** G 2470–2580. **Megjegyzés:** az épületazonosító (`cro_merges_tuskek`) még név szerint szerepel (README).

### MECH-CREEP-8 – Egyéb croftian szabályok

- **Építés:** `consume` – a Csápszolga beleolvad az épületbe (elfogy).
- **Szemhordozó:** `unit_supply` 8 (létszámot ad); a gép a `supply_unit`-ként termeli.
- **Idézések:** `summons_permanent` (D21) – az idézések **nem járnak le**, a hős idézése állandó (hősönként legfeljebb 4).
- **Varázserő:** `spell_resource: hp` (a képesség életerőből fizet – KEP-…).
- **Felfedés:** a Szemhordozó „A Kórus szeme” passzív képességgel 9 csempén belül felfed (a felesleges `detect_units`
  kulcs törölve, D21); a Tüskés gyökér épület szintén felfedő (`detect`).

---

## 3. Tó – Huk’ucha (`mechanic: pond`)

### MECH-POND-1 – Tavacska ásása

- **Adat:** faj `pond`: `size` 2, `cost` `{wood: 15}`; szabály `pond_size` (Árvíz-irány: 3); `POND_TIME` 4 s.
- **Szabály:**
  1. Feltétel: minden csempe a pályán, építhető terep, szabad vagy épület alatti, felderített; legalább egy még nem saját tó.
  2. A Tóásó odamegy, 4 s-ig ás; az ár a parancskor fizetendő. A félkész hely (`worksite`) megmarad, bármelyik építő
     folytathatja új költség nélkül; 90 s munka nélkül a hely megszűnik, az ára visszajár.
  3. Kész: a csempék saját tóvá válnak (a szomszédosak összeolvadnak). Tervezett tóra már előre lehet építést sorba tenni.
- **Kód:** G 2425–2467, 2576–2706.
- **Próba:** P1 – ásás 4 s; a 2. munkás nem fizet újra. P2 – 90 s tétlenség után visszatérítés.

### MECH-POND-2 – Építés csak saját tóra

- **Szabály:** a `no_pond` nélküli Huk’ucha-épület minden csempéje saját tó vagy saját tervezett tó. A főépület
  kezdéskor tavon áll, 2 csempés peremmel.
- **Kód:** G 2273–2278, 253–257.

### MECH-POND-3 – Mozgás tavon

- **Adat:** `game.pond_speed` `own` 1,3, `other` 0,7; faj `pond_affinity` (Huk’ucha: true).
- **Szabály:** **bármely** tavon álló földi egység: ha a **gazdája népe** `pond_affinity` → × 1,3, különben × 0,7 (a tó
  gazdájától függetlenül; D26: a Huk’ucha szövetséges kisnép-egységei is gyorsulnak). **Próba:** P1 – ursan egység
  tavon: 0,7×; huk’ucha egység: 1,3×; a Huk’ucha játékos Pinguu-egysége: 1,3×.

### MECH-POND-4 – A tó gyorsítja az építést

- **Adat:** faj `pond_build_bonus` 0,6.
- **Szabály:** építési sebesség × `(1 + 0,6 × gyűrű)`, ahol a gyűrű = az épület körüli 1 csempés keretben a saját tó aránya.
- **Kód:** G 1493–1508. **Próba:** P1 – teljesen tóval körülvett épület 1,6× gyorsabban épül.

### MECH-POND-5 – Áradás és Gyöngybója

- **Áradás** (`bpower`, `effect: flood`, Tóénekből): minden saját tó egy csempével kitágul (4-szomszédság, építhető,
  nem tó csempékre); a tartam alatt a saját tavon álló sebesült egységek `heal`/s életerőt kapnak; a végén
  visszahúzódik, kivéve ahol közben épület áll. Ismételt használat csak meghosszabbít.
- **Gyöngybója** (épület `pond_ring`): elkészültekor a körülötte lévő `pond_ring` csempés keret tó lesz.
- **Gyöngyház-pajzs** (`shield_zone: pond`): a pajzs csak saját tavon töltődik.
- **Ikra-akna** (`lay_mine`, `mine_trigger`): 8 tickenként, ha ellenséges földi egység ér a sugarába, `count`
  (saját tavon `pond_count`) idézett egység kel ki és támad; az akna elfogy. Egyszerre legfeljebb `max` (12).
- **Kód:** G 1729–1770, 2749–2756, E 161, G 1893–1934.

---

## 4. Energiamező – Szürke elfek (`mechanic: power`)

### MECH-POWER-1 – Energiamező és ellátás

- **Adat:** épület `power_radius` (Napobeliszk 6,5); szabály `power_add` (Rezonancia); épület `no_power`.
- **Szabály:**
  1. Építés: a `no_power` nélküli épület középpontja egy kész, saját mezőadó `power_radius + power_add` csempés körében.
  2. Ellátás (30 tickenként és minden építés/pusztulás után): `powered` = a középpont valamely kész mezőadó körében
     van (vagy `no_power`).
  3. Ellátatlan épület **nem termel, nem kutat és nem lő** (a sor áll).
- **Kód:** G 2268–2272, 2990–3001, E 1736, 1748.
- **Próba:** P1 – az obeliszk lerombolása után a mezőjében lévő Kaszárnya sora megáll, újraépítés után folytatódik.

### MECH-POWER-2 – Behívás (warp-in)

- **Adat:** faj `warp_in: true`.
- **Szabály:** ha a termelő épületnek van gyülekezőpontja, és a ponthoz legközelebbi saját mezőadó körébe esik, a nem
  munkás egység **a mezőadónál** jelenik meg (nem az épületnél).
- **Kód:** G 3155–3197. **Próba:** P1 – gyülekezőpont egy távoli obeliszk mellett: az egység ott jelenik meg.

### MECH-POWER-3 – Egyéb elf szabályok

- **Építés:** `leave` (a Kőfaragó elindítja, és elmehet). A faj `self_build` kulcsát csak a gépi ellenfél használja.
- **Égkő-fény:** a legerősebb (MECH-RIFT). **Áhítat:** MECH-FAITH. **Varázserő:** `spell_resource: energy`.

---

## 5. Hálózat – Micéliák (`mechanic: network`, rejtett főfaj)

### MECH-NET-1 – Fonalgráf

- **Adat:** épület `core` (Szívgomba), `link_radius` (Szívgomba 7, Spórafonal 6, a többi 4), `field_radius`
  (Szívgomba 6, Spórafonal 3, alap 4).
- **Szabály (15 tickenként és minden építés/pusztulás után):**
  1. A kész `core` épületekből mélységi bejárás a kész épületeken; két épület össze van kötve, ha a középpontjuk
     távolsága ≤ `max(link_radius_a, link_radius_b)` csempe.
  2. `connected` = elérhető (vagy még épül); a nem kapcsolt kész épület **leáll** (`powered = false`).
  3. Mező: minden kapcsolt kész épület `field_radius` körében a csempék a játékos mezőjébe kerülnek (`creep` rács).
- **Építés:** a `core` nélküli épület középpontja egy kész, kapcsolt saját épület `max(link_radius)` körében.
- **Kód:** G 2769–2808, 2262–2267.
- **Próba:** P1 – a Spórafonal elpusztítása után a mögötte lévő épületek leállnak, és nem adnak létszámot.
- **Megjegyzés:** a bejárás sorrendje az élek listáját (`links`, megjelenítés) befolyásolja, a kapcsoltságot nem.

### MECH-NET-2 – Spórák és osztódás

- **Adat:** faj `spell_resource: spores`; képesség `spores` költség; passzív `split_kills`.
- **Szabály:** spóratöltet egységenként legfeljebb 6: +1 ellenség megöléséért; 300 tickenként (10 s) +1, ha egy
  kapcsolt saját épület 8 csempén belül van. Osztódás: `split_kills` ölés után, ha van létszám (hős idézésénél
  hősönként legfeljebb 4), az egység kettéválik: mindkettő fél életerővel, az iker a spórák felét kapja,
  véletlen irányban `2 × sugár` távolságra.
- **Kód:** G 1179–1183, 1342–1379.
- **Egyéb:** mezőn gyorsabb gyógyulás (MECH-CREEP-3 szerint, `in_own_field`), faj `regen` 0,3, `network_supply` 1.

---

## 6. Lámpásfény és Harci kedv – Ooni (`mechanic: lantern`)

### MECH-LANT-1 – Lámpásfény

- **Adat:** épület `light_radius` (Nian-csarnok 8, Papírlámpás 6, Torii 3), `light_heal` (2, Torii 1), `lantern`,
  `no_light` (Égkő); szabály `light_add` (Messzefény), `lantern_magic`.
- **Szabály:**
  1. Építés: a `no_light` nélküli épület középpontja egy saját fényforrás `light_radius + light_add` körében; a
     lámpás (`lantern`) már építés közben is világít.
  2. 30 tickenként: minden sebesült **ooni** egység az első saját, kész fényforrás körében `+light_heal` életerőt kap.
  3. `lantern_magic` szabály: fényben a varázssebzés × az értéke.
- **Kód:** G 2288–2293, 2708–2722, E 176–182. **Megjegyzés:** a gyógyulás a faj nevéhez kötött (README).

### MECH-LANT-2 – Harci kedv (morál)

- **Adat:** faj `morale`: `max` 100, `hurt` 60, `ally_death` 10, `hero_death` 25, `hero_death_breaks` true, `regen` 2, `drum` (`oon_dobos`,
  `radius` 4, `regen` 8), `light_min` 30, `break_until` 50, `broken` (`damage_mult` 0,7, `speed_mult` 1,2);
  szabály `morale_hurt_mult`, `drum_mult`.
- **Szabály:**
  1. Csak a nem munkás, támadó, nem idézett ooni egységnek van morálja (első frissítéskor `max`).
  2. Sérüléskor: `morál −= sebzés / max_hp × hurt × morale_hurt_mult`; 0-nál **megtört**.
  3. Haláleset: az elesett (nem idézett) társ 5 csempés körében a saját egységek morálja −10. **Hős** eleste:
     a 8 csempés körében a morál azonnal 0 – megtörnek (`hero_death_breaks`, D21; nélküle −`hero_death`).
  4. 15 tickenként: töltés `regen`/s, ha 3 s óta nem sérült; + `drum.regen × drum_mult`/s, ha egy **másik** dobos
     `drum.radius` csempén belül van; legfeljebb `max`. Saját fényforrás körében legalább `light_min`.
  5. Megtört egység: a módosítói közé kerül a `broken` (sebzés × 0,7, sebesség × 1,2); `break_until` (50) elérésekor helyreáll.
- **Kód:** G 1141–1146, 1167–1176, 1973–1997, E 328–331.
- **Próba:** P1 – 100 max_hp-s egység 50 sebzést kap (páncél után) → morál 100 → 70. P2 – ezután 5 csempén belül
  7 társ elesik (−70) → morál 0, megtört: sebzés × 0,7, sebesség × 1,2; töltődés után 50-nél helyreáll.
  P3 – dobos mellett, sérülés nélkül 0-ról 100-ra 10 s alatt (2 + 8 = 10/s).
- **Döntés (D21):** `hurt` 60 mellett a sérülés egyedül nem töri meg az egységet (a halálig legfeljebb −60) –
  ez így marad; a közeli hős eleste viszont azonnal megtöri. P4 – a hős 6 csempén belül elesik → morál 0, megtört.

### MECH-LANT-3 – Taiko-dobos, Torii-kapupár, őrök

- **Építésgyorsítás:** MECH-BUILD (`build_boost`: a dobos 4 csempén belül → × 1,5).
- **Torii-kapupár** (`torii`): az első kapu bárhova (szabályok szerint), a második csak látott helyre; párok száma
  ≤ `rules.torii_pairs` (1, fejlesztéssel 2). Belépő egység a kapuban vár; 2 s múlva (vagy a kapu 8 s-os
  újratöltése után) a párnál jelenik meg; utána mindkét kapu 8 s-ig tölt. Ha a pár elpusztul, a várakozók kijönnek.
- **Őrök:** 30 tickenként az épületek `guard_armor`-ja nullázódik, majd: Kőkutya (`rules.kokutya_guard`) és a
  bekapcsolt Kapuőrzés (+3) a 2,5 csempén belüli saját épületnek páncélt ad; épületenként legfeljebb 6.
- **Kód:** G 1818–1863, 2279–2287, 2723–2737.

---

## 7. Standard népek

### 7.1 Ursanok

- **Építés:** `enter` – a Kalapácshordozó az épületbe lép, és csak bentről épít (MECH-BUILD).
- **Varázserő:** `spell_resource: cooldown` (a képességek csak újratöltési időt használnak – KEP-…).
- Fajszintű különleges rendszere nincs; a többi szabályuk az egységek, képességek és ágak adatában van.

### 7.2 Wullof

- **MECH-WUL-1 Falkaszellem** (`pack_bonus`: `radius` 4, `per` 0,1, `max` 0,4, `speed_per` 0,02):
  `n` = a saját, nem munkás, nem jószág (`livestock`) egységek száma az egység körüli **négyzetben** (± 4 csempe),
  legfeljebb `max / per` = 4; 15 tickenként egyszer számolva. Sebzés × `(1 + 0,1 n)`, sebesség × `(1 + 0,02 n)`.
  A Légió-irány `rules.pack_bonus` címkékkel szűkítheti. **Próba:** 5 társ mellett sebzés × 1,4, sebesség × 1,08.
- **MECH-WUL-2 Vadászzsákmány** (`bounty` 0,2, `bounty_res` wood): ellenséges egység vagy épület megölésekor
  `Σ költség / count × 0,2 × 0,5` fa; `rules.bounty_gold` esetén ennek ennyiszerese arany is (Hadtáppal).
- **MECH-WUL-3 Erdei rejtőzés** (`forest_stealth` + `erdei_rejtozes` képesség, vagy `rules.forest_stealth`):
  rejtett, ha 3 s óta nem támadott, földi, áll, és a 3 × 3-as környezetében fa van. Felfedi: felfedő egység/épület
  (`detect`), `rules.detect_all`, vagy 2,2 csempén belüli ellenség (15 tickenként).
- **MECH-WUL-4 Vándorszekér** (`wagon` wul_vandorszeker, `unpack_time` 8): a `packable` épület összecsomagolható
  (sora visszavonva, szekér lesz, életereje arányosan); letelepítés bárhová, ahol építhető (előfeltétel nélkül):
  az épület `build_time − 8` s-tól indul, munkás nélkül épül.
- **Építés:** `stay` (több építő gyorsít). **Javítás:** `repair: true`. **Varázserő:** `song`.

### 7.3 Aurethi Birodalom

- **MECH-IMP-1 Alapozás** (`foundation`: `size` 2, `cost` `{stone: 10}`, `unfounded_hp` 0,5, `decay` 0,2%/s, `min` 0,5):
  a Fabrikátor 2 × 2-es (Nagy alapozás 4 × 4) lapkövet rak (3 s, illetve 6 s; félkész hely mint MECH-POND-1);
  nem blokkolja a mozgást. A teljesen alapozáson álló épület teljes életerővel készül el; alapozás nélkül fél
  életerővel, és 15 tickenként `max_hp × 0,002 /s`-mal kopik, fél életerőig (javítható). A főépület alapozáson kezd
  (1 csempés peremmel).
- **MECH-IMP-2 Reaktorpajzs** (`shield_zone: reactor`): a pajzs csak Lélekreaktor 6 csempéjén belül vagy
  Égkő-fényben (MECH-RIFT) töltődik.
- **MECH-IMP-3 Lélekhorgony** (`soul_anchor` imp_lelekhorgony): a `lelekhorgony` képességű, nem hős, nem idézett gép
  halálakor a helyén horgony marad (`max_hp = max(50, eredeti hp)`, 15%-on). Egy másik gép elviheti (`anchor_rescue`);
  teljesen helyreállítva újra az eredeti gép lesz.
- **Építés:** `stay_single`. **Gyűjtők:** `gatherers` (aranyat és meteoritot csak az Arató – a gép ezt használja).
  **Javítás:** `repair: true`. **Gyógyulás:** faj `regen` 0,25; Égkő-fényben a `mech` címkéjűek +1.
- **Kód:** G 2329–2423, 2757–2762, 1184–1191, 1382–1435, E 157–165.

---

## Lefedettség

A leltár 4. pontjának (faj-szintű kulcsok) minden játékszabályt hordozó kulcsa szerepel ebben a fájlban, kivéve:
`hq`, `worker`, `supply_building`, `start_units`, `start_gather`, `units`, `buildings`, `researches`, `paths`,
`ai`, `camps`, `color`, `accent`, `style`, `name`, `desc`, `tagline`, `order`, `id`, `supply_desc`, `resource_names`
(kezdés, tartalom, megjelenés, szöveg – a sablonformátum része), a `hidden` (a Micéliák csak a hadjáratban
választhatók, D12) és a `neutral` (a Vadon semleges szörnyei, `KISFAJOK.md`). Új kulcsok (D21): `pond_affinity`,
`spike_immune`, `summons_permanent`, `morale.hero_death_breaks`.
A képességek (`KEP-`), a kutatások és szabálymódosítók (`KUT-`), a hősök és a kisnépek külön fájlba kerülnek.
