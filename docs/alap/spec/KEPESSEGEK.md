# Képességek – viselkedési specifikáció, átvizsgálás és javaslat

Kelt: 2026-11-12 · referencia: a Python-játék (`G` = `rts/game.py`, `E` = `rts/entities.py`, `H` = `rts/heroes.py`),
`data/abilities.json` (269 képesség, 23 típus). Formátum: [README.md](README.md).

Tartalom: **A.** a mai 23 típus pontos működése · **B.** átvizsgálás: mennyire egységes és értelmes ·
**C.** mit érdemes átvenni a 0 A.D.-ből és a Warcraft III-ból · **D.** javasolt képességmodell az új motorhoz.

---

## A. A mai működés

### KEP-0 – Közös keret

- **Szint:** hősnél a megtanult szint (`learned`); a `levels[n]` elem felülírja az alapértékeket, a `modifiers`-t
  kulcsonként összefésüli (H 23–41). A nem hős egység mindig az alapadatot használja. `ultimate`: csak magasabb
  hősszinten tanulható.
- **Elérhetőség:** a képesség az egységtípus `abilities` listájából jön; kutatáshoz (`ability_research`) vagy
  fejlődési irányhoz (`ability_path`) köthető; a hős a megtanultakat kapja meg; küldetés adhat továbbiakat.
  `ally`: a szövetséges kisnép eleme (csak a megkötött szövetséggel).
- **Használhatóság (`ab_ok`, E 281):** elnémítva csak `passive`, `aura`, `autoheal` működik; feltétel a költség
  megléte: `energy` (az egység energiája), `faith` (a játékos Áhítata), `hp_cost` (életerő > költség + 5),
  `spores` (az egység spórái). **Fizetés** (`ab_pay`): mindegyik levonódik, ami meg van adva.
- **Újratöltés:** `ab_ready[id] = most + cooldown`. Az alapértelmezés típusonként más (lásd B-2).
- **Varázserő-szorzó** (`spell_mult`): Égkő-fényben a faji `spell_mult` (elfek 1,25), lámpásfényben `lantern_magic`.
- **Gépi használat:** csak a gépi játékosnál (`_ai_cast`, típusonként kézzel írt feltétel); emberi játékosnál
  egyedül az `autoheal` megy magától (0,5 s-onként).

### Célzott képességek (`cmd_cast` → `_o_cast`, E 1301)

| Azonosító | Típus (db) | Működés | Fő adatkulcsok (alapérték) |
|---|---|---|---|
| KEP-AREA | `area` (36) | A pontra megy `range` (6) csempéig, ott **területi mező** jön létre `duration` (4) s-ig, `radius` (2) sugárral; 0,5 s-onként: `heal × 0,5` minden saját **egységnek** (gépnek is; épületnek nem), `dps × 0,5 × spell_mult` minden ellenséges egységnek (elemmel), `slow` 0,7 s-ra; `buildings` jelzővel az épületeknek is `dps × 0,5` (**szorzó nélkül**); `wind` elem: taszítás; irányító elemek (gyökér, alvás, félelem, vakság, víz, némítás, jel, páncéltörés) sebzés nélkül is. | `energy`/`hp_cost`/`spores`, `cooldown` (csak ha megadott) |
| KEP-BOLT | `bolt` (28) | Ellenséges egységre (1,6 csempés kereséssel a pont körül); `range + célsugár` távolságból; lövedékkel (`projectile`) vagy azonnal; varázssebzés `damage` (50) × `spell_mult`; `slow` `duration` (1,5) s-ig, `bounces`, `splash`, `poison`, `element`. | `cooldown` (csak ha megadott) |
| KEP-BLINK | `blink` (4) | Azonnali áthelyezés a pontra, legfeljebb `range` (kutatással `range_research`) csempére, szabad helyre; `land_damage` a landolás `land_radius` (1,2) körében (**nem varázssebzés**, szorzó nélkül). | `cooldown` (10) |
| KEP-RAISE | `raise` (2) | A pont 2,5 csempés körében a legközelebbi, legalább 1 s-os, fel nem használt tetemből `small` vagy (nagy: `large` címke vagy ≥ 4 létszám) `large` egység lesz – **állandó**, nem jár le. | `energy` |

### Azonnali képességek

| Azonosító | Típus (db) | Működés | Megjegyzés |
|---|---|---|---|
| KEP-NOVA | `nova` (34) | Az egység körül `radius` (2,5): `heal` a saját **élő** (`organic`) egységeknek; `damage` (50) × `spell_mult` varázssebzés minden ellenséges egységnek; `stun` (kábulás; félelem/alvás elemnél az elem intézi) vagy `slow` `duration` (2) s; `building_damage`: épületeknek is; tüskéket sebez; `self_destruct`: a használó meghal. | `cooldown` (10) |
| KEP-BUFF | `self_buff` (31) | Saját magára `duration` (4) s-os hatás: a `modifiers` érvényes; `drop_work`: munkát félbehagy, utána visszamegy; `after_slow`: lejárta után lassul. | `cooldown` (10) |
| KEP-SUMMON | `summon` (27) | `count` egység az egység elé (szabad hely kell); `lifetime` (30) × `summon_life_mult` × idézéskutatás (`summons_permanent` fajnál nem jár le, hősnél hősönként ≤ 4); `target_placement`: pontra. | `cooldown` (**1**) |
| KEP-MINE | `lay_mine` (1) | Ikra-akna a helyre; ≤ `max` (12) élő; lásd MECH-POND-5. | `cooldown` (3) |
| KEP-WARD | `ward` (1) | A 5 csempés körben a saját egységek `amount` (50) pajzsot kapnak `duration` (10) s-ig (a sebzést előbb ez nyeli el). | `cooldown` (18) |
| KEP-OVER | `overdrive` (1) | Az épülő épület hátralévő idejének `share` (50%) része azonnal elkészül (kész épületnél a hiányzó életerő 50%-a); a használó `self_hp` (40%) életerőt veszít, páncél nélkül. | `cooldown` (20) |

### Kapcsolók és állandó hatások

| Azonosító | Típus (db) | Működés |
|---|---|---|
| KEP-TOGGLE | `toggle` (20) | Be/ki; bekapcsolva a `modifiers` érvényes (a `mod()` szorozza/összeadja). `burrow`: beásás (rejtett, MECH-WUL-3 szerinti felfedéssel). Elnémítás nem kapcsolja ki. |
| KEP-CHANT | `chant` (11) | Mint a kapcsoló, de egyszerre csak egy ének szól az egységen; bekapcsolva **aura**: a saját egységek `radius` (7) körében. |
| KEP-AURA | `aura` (22) | Állandó: a hős (megtanulva) vagy az egység körül `radius` (7) – csak a **saját** egységekre. Azonos kulcs nem halmozódik: szorzónál ≥ 1 a legnagyobb, < 1 a legkisebb, összeadónál a legnagyobb. |
| KEP-PASSIVE | `passive` (15) | Csak lekérdezett értékek: `split_kills` (osztódás), `detect` (felfedés sugara), `modifiers`. |
| KEP-HEAL | `autoheal` (20) | 0,5 s-onként: a `range` (5) körében a legsebesültebb `heal_tags` (alap `organic`) saját egység `amount` (10) életerőt kap. |

### Egyedi típusok (egy-egy képesség)

`merge` (3 egység → `into`, átlagolt életerővel; ha messze vannak, összegyűlnek), `morph` (`via` gubón át `time` s
alatt `into`, `cost` és létszám kell), `mount` / `dismount` (Wullof bölény + lovas), `garrison` (beszállás –
helyőrség, lásd épületek), `anchor_rescue` (Lélekhorgony mentése, MECH-IMP-3), `gpower` és `bpower` (épületről,
Áhítatból vagy nyersanyagból: területi csapás, idézés, `levy` – munkás-mozgósítás, `wind`; illetve gyorsítás és
Áradás, MECH-POND-5). Kód: G 2093–2215, 2854–2960, 1626–1727.

### Elemek

13 elem (`fire`, `ice`, `lightning`, `poison`, `water`, `root`, `sleep`, `fear`, `blind`, `wind`, `sunder`,
`silence`, `mark`) – a találatkor `apply_element` (G 970) állapotot ad (égés, fagyás, sokk, mérgezés, …).
Részletes leírás: `HOSOK_TARGYAK_TABOROK.md` (tervezett).

---

## B. Átvizsgálás – mennyire egységes és értelmes

### B-1. A fajok „varázserő-identitása” nem érvényesül

A faj `spell_resource` kulcsa szerint az Ursanok újratöltésből, a Croftianok életerőből, a Wullofok énekből, a
Micéliák spórából varáz­solnak. A tényleges képességköltségek ezt nem követik:

| Faj | `spell_resource` | Ténylegesen (képességszám) |
|---|---|---|
| Ursanok | `cooldown` | **8 energiás**, 10 csak újratöltés |
| Croftianok | `hp` | **9 energiás**, 6 életerős |
| Wullof | `song` | **10 energiás**, 15 csak újratöltés |
| Micéliák | `spores` | **8 energiás**, 4 spórás |
| Szürke elfek, Vasbirodalom | `energy` | rendben (15, illetve 6 energiás) |

A `spell_resource` a kódban csak kijelzés és a spóra-ölés feltétele; a képesség költségét a képesség saját kulcsai
adják. **Következmény:** a fajok közti különbség, ami a tervben karakteres (vér, ének, spóra), a játékban elmosódik.

### B-2. Ugyanaz a fogalom típusonként másképp

- **Alapértelmezett újratöltés:** `blink`, `nova`, `self_buff` 10 s; `summon` **1 s**; `autoheal` 1 s; `area`,
  `bolt` 0 (csak ha megadott); `ward` 18; `overdrive` 20. Egy elfelejtett `cooldown` így idézésnél gyakorlatilag
  végtelen idézést enged.
- **Alapértelmezett sugár:** `area` 2, `nova` 2,5, `ward` 5, `aura`/`chant` 7; hatótáv `area`/`bolt` 6.
- **Varázssebzés:** a `nova`, a `bolt` és az `area` egységsebzése varázs (páncél nélkül) és `spell_mult`-tal nő; az
  `area` épületsebzése és a `blink` landolási sebzése **szorzó nélkül**, a `blink` ráadásul nem varázs.
- **Gyógyítás célpontja:** `autoheal` és `nova` csak élőlényt (`organic`) gyógyít, az `area` bármely saját
  egységet (gépet is).
- **Halmozódás:** a kapcsolók és a buffok szorzódnak/összeadódnak (`mod()`), az aurák nem halmozódnak (max/min),
  a kutatások összeadódnak (`upg`), a szabálymódosítók (`rules`) egyenként felülírják egymást.
- **Elnémítás:** a célzott és azonnali képességeket tiltja, a kapcsolókat és az énekeket nem.

### B-3. Öt különböző módosítórendszer

Ugyanazt a dolgot (pl. „+20% sebesség”) ma öt helyen lehet kifejezni, mindegyiknek más a szabálya:
1. kutatás `effects` → `stat` + `add`, címkeszűréssel (`upg`);
2. képesség `modifiers` (`speed_mult`, `armor_add`, `ranged_taken_mult` …) – kapcsoló, buff, aura;
3. faji/irány `rules` – 38 egyedi kulcs, mindegyikhez külön kódsor (`speed_pct`, `rot_mult`, `larva_add` …);
4. faji állandók (`pack_bonus`, `morale.broken`, `rift`);
5. épület-aura (`aura.modifiers`, `building_armor`, `regen_aura`).

### B-4. Túl sok típus, kézzel írt parancsokkal

23 típusból 9-hez egyetlen képesség tartozik, és mindegyiknek saját parancsfüggvénye és saját gépi logikája van
(`cmd_cast`, `cmd_nova`, `cmd_summon`, `cmd_buff`, `cmd_toggle`, `cmd_ward`, `cmd_lay_mine`, `cmd_overdrive`,
`cmd_mount`, `cmd_merge`, `cmd_morph` …). Egy modder új képességtípust ma csak Python-kóddal hozhat létre.

### B-5. Hiányzó, a műfajban alapvető lehetőségek

- **Célpontszűrés** csak részben: nincs általános „szövetséges / ellenség / saját, föld / lég, élő / gép, hős”
  célpontlista. A `bolt` csak ellenségre megy, gyógyító lövedék vagy szövetséges-buff célzottan nem adható.
- **Ellenségre ható aura nincs** (pl. „a közeli ellenség páncélja −2”); az aura csak a saját egységekre hat.
- **Nincs varázslási idő és csatornázás** (minden képesség azonnal hat); nincs megszakítható varázslat.
- **Nincs varázslatellenállás / -immunitás és eloszlatás (dispel)**: a buffoknak nincs „pozitív/negatív,
  mágikus/fizikai” jelzője.
- **Emberi automatikus használat** csak gyógyításnál; más hasznos képességnél (pl. buff, védőfény) nincs.

### B-6. Értelmes és jól működő részek (megtartandók)

- A **szintenkénti felülírás** (`levels`) egyszerű és modderbarát – a WC3 is így működik.
- A **területi mező** mint önálló, időzített entitás (`AreaEffect`) jó alap.
- Az **elemrendszer** (13 elem) karakteres, és jól illeszkedik egy általános állapothatás-modellbe.
- Az aurák **nem-halmozódási** szabálya (azonos hatás csak egyszer) megegyezik a WC3- és a 0 A.D.-gyakorlattal.

---

## C. Mit érdemes átvenni

| Forrás | Megoldás | Mit old meg a TOTU-ban |
|---|---|---|
| **0 A.D.** – *Modifications* (egységes módosító: érték-útvonal + `add`/`multiply`/`replace`, osztályszűrés) | **egy** módosítórendszer a kutatásokhoz, aurákhoz, buffokhoz, kapcsolókhoz, irányokhoz és faji szabályokhoz | B-3 (öt rendszer → egy), a 38 egyedi `rules`-kulcs nagy része adat lesz |
| **0 A.D.** – *Auras* (hatósugár, érintett osztályok, **érintett játékosok**: saját / szövetséges / ellenség, globális vagy formációs aura) | aura ellenségre és szövetségesre is; épület- és hősaura ugyanazzal a formával | B-5 (ellenséges aura), az épület-aura és a hős-aura egységesítése |
| **0 A.D.** – *StatusEffects* (időtartam, időközönkénti hatás, halmozási szabály: `Ignore` / `Extend` / `Replace` / `Stack`) | minden időzített hatás (buff, méreg, égés, kábulás, lassítás) egy állapot-objektum | B-2 halmozás, az elemek egységes leírása |
| **0 A.D.** – *Identity classes* (osztálylista, logikai szűrők) | a meglévő `tags` mint osztályok, egységes célpontszűrővel | B-5 célpontszűrés |
| **WC3** – *Ability = célzási mód + célpontlista + költség + újratöltés + hatótáv + terület + időtartam + szintek* | egységes képességkeret: 23 típus helyett néhány célzási mód és összerakható hatások | B-4, B-2 |
| **WC3** – *Buff mint külön objektum* (pozitív/negatív, mágikus/fizikai, eloszlatható) | eloszlatás (dispel), varázslatimmunitás (pl. golem, hős részleges) | B-5 |
| **WC3** – *Cast point / backswing, csatornázás* | rövid varázslási idő, megszakítható nagy varázslatok (ultimate) | B-5; játékérzet és ellenjáték |
| **WC3** – *Autocast kapcsoló* (gyógyítás, átok, páncél) | bármely képességnél bekapcsolható automatikus használat, adatban leírt feltétellel; a gép ugyanezt használja | B-5, B-4 (a kézzel írt gépi logika helyett) |
| **WC3** – *Hős vs. normál időtartam* (a hősre rövidebb ideig hat a kábítás) | `hero_duration` | hősök túl könnyű kiiktatása |

Amit **nem** javaslok átvenni: a 0 A.D. sebzéstípus-rendszerét (Hack/Pierce/Crush) – a TOTU típusszorzó-mátrixa
(`unit_types.matrix`) ugyanezt adja, és a balansz rá épül; és a WC3 hős-tulajdonságait (Erő/Ügyesség/Intelligencia)
– a TOTU-hősök szintje és tárgyai elég mélységet adnak.

---

## D. Javasolt képességmodell az új motorhoz

### D-1. Három fogalom

1. **Módosító** (0 A.D.): `{érték, művelet: add | mul | set, szűrő: címkék}` – minden statváltozás ezzel.
2. **Állapot** (0 A.D. StatusEffect + WC3 buff): `{id, időtartam (hősre külön), időköz, időközi hatások,
   módosítók, halmozás: ignore | extend | replace | stack, jelleg: pozitív | negatív, mágikus | fizikai,
   eloszlatható}`. Ide tartozik minden buff, méreg, égés, kábulás, lassítás, elem.
3. **Képesség** (WC3): `{célzás, célpontok, költség, újratöltés, hatótáv, varázslási idő, csatornázás, automatikus
   használat, szintek, hatások[]}`.
   - **Célzás:** `passive` | `instant` | `toggle` | `unit` | `point` | `area` | `global` (épület- és főépület-képesség).
   - **Célpontok:** `ally` / `enemy` / `self` / `own`, `ground` / `air`, címkék (`organic`, `mech`, `hero`,
     `building` …), kizárások.
   - **Költség:** `energy` | `hp` | `spores` | `song` | nyersanyag (`faith` …) | csak újratöltés – a **faj**
     varázserő-forrása az alapértelmezés (B-1).
   - **Hatások** (összerakható lista): `damage`, `heal`, `apply_status`, `aura` (állandó, kinek: saját / szövetséges /
     ellenség), `field` (időzített területi mező, időközi hatásokkal), `projectile`, `summon`, `transform`
     (morph, merge, mount), `teleport`, `spawn_entity` (akna, tetemből egység), `building_progress` (túlhajtás),
     `modify` (módosító a használóra).
   - Ami ennél bonyolultabb, az a képességhez csatolt **Lua-szkript** (modolható, D19).

### D-2. A mai 23 típus megfeleltetése

| Mai típus | Célzás | Hatások |
|---|---|---|
| `area` | `area` | `field` (időköz 0,5: `damage`, `heal`, `apply_status` lassítás/elem) |
| `bolt` | `unit` (ellenség) | `projectile` → `damage` + `apply_status` |
| `blink` | `point` | `teleport` + (`damage` a landoláskor) |
| `nova` | `instant` | `damage` + `heal` + `apply_status` (kábulás/lassítás) a használó körül |
| `self_buff` | `instant` (`self`) | `apply_status` (módosítókkal) |
| `toggle`, `chant` | `toggle` | `apply_status` (állandó amíg be van kapcsolva); ének: + `aura`, kizárólagos csoport |
| `aura` | `passive` | `aura` |
| `passive` | `passive` | `modify` / jelzők (`detect`, `split_kills`) |
| `autoheal` | `unit` (saját, sebesült) + automatikus | `heal` |
| `summon` | `instant` / `point` | `summon` |
| `raise` | `point` | `spawn_entity` (tetemből) |
| `lay_mine` | `point` | `spawn_entity` (akna) |
| `ward` | `instant` | `apply_status` (pajzs) a körben |
| `overdrive` | `instant` | `building_progress` + `damage` (önmagára) |
| `morph`, `merge`, `mount`, `dismount` | `instant` | `transform` |
| `garrison`, `anchor_rescue` | `unit` | parancs (nem képesség) – a parancsrendszerbe kerül |
| `gpower`, `bpower` | `global` / `instant` (épületről) | `field`, `summon`, `apply_status`; Áradás: Lua |

A meglévő `abilities.json` eszközzel konvertálható az új formára; a mai viselkedés (A rész) a próbák alapja.

### D-3. Tulajdonosi döntést kívánó pontok

1. **Varázserő-identitás (B-1):** a fajok képességei a saját forrásukból fizessenek (Ursanok újratöltés, Croftianok
   életerő, Wullof ének, Micéliák spóra)? Ez balanszváltozás, képességenként át kell számolni.
2. **Egységes alapértékek (B-2):** egy közös alap-újratöltés (pl. 10 s) és egységes sugár-alapértékek; varázssebzés
   mindenhol `spell_mult`-tal.
3. **Új lehetőségek (B-5):** ellenséges aura, eloszlatás és varázslatimmunitás, varázslási idő és csatornázás,
   automatikus használat kapcsolója – melyik kerüljön a játékba, és melyik legyen csak modolási lehetőség.
4. **Gyógyítás célpontja:** csak élőlény, vagy a gépeket a javítás gyógyítja (WC3-szerűen) – egységesen.

---

## E. Döntések (D22, 2026-11-12) és következményeik

| # | Döntés | Következmény a mai tartalomra |
|---|---|---|
| 1 | Faji varázserő-forrás; a hősök és a `caster` varázslók energiát használnak | **nincs változás**: az Ursanok, Croftianok, Wullof és Micéliák 35 energiás képessége mind hősé vagy varázslóé. Ellenőrzés: `tools/tartalom_leltar.py` → leltár 17. pont (ma: nincs sértés). A Huk’ucha és az Ooni `spell_resource` kulcsa hiányzik – a gyakorlatban energia; a konverzió `energy`-re állítja. |
| 2 | Egységes alapértékek; varázssebzés mindenhol a szorzóval | A konverter minden képességbe beírja a mai tényleges értéket (pl. az idézés 1 s-os alap-újratöltését kifejezetten), így csak az új tartalomra hat az új alapérték. Változás: a területi mezők épületsebzése és a Békaugrás landolási sebzése Égkő-fényben és lámpásfényben nő. |
| 3 | Ellenséges aura, eloszlatás és immunitás, varázslási idő és csatornázás, automatikus használat | Motorképesség (M7); a konkrét képességek kiosztása balanszmunka. Javasolt kezdés: a végső képességek (`ultimate`, 24 db) kapjanak varázslási időt; a gépek és gólemek (21 `mech` egység) közül a nagyok varázslatimmunitást; a gyógyító és a buffoló képességek automatikus használatot. |
| 4 | Élőt gyógyítás, gépet javítás | A 20 `autoheal` közül csak a `h_livia_horgony` (Horgonyjavítás) gépjavító. **Két képesség, ahol döntés kell:** `h_livia_lelegzet` (Második lélegzet, Vasbirodalom-hős, területi) ma a gépeket is gyógyítja – javaslat: kifejezetten gyógyít **és** javít, különben a Vasbirodalom gyengül; `mho_javitas` (Foltozás, Mhonyo) a neve szerint javítás, de ma csak élőt gyógyít – javaslat: gépjavító legyen. |
