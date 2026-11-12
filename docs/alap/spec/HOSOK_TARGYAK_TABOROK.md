# Hősök, tárgyak, semleges táborok és elemek – viselkedési specifikáció, átvizsgálás és javaslat

Kelt: 2026-11-12 · referencia: `H` = `rts/heroes.py`, `G` = `rts/game.py`, `E` = `rts/entities.py`; adatok:
`data/items.json` (15 tárgy), `data/camps.json`, `data/factions/vadon.json`, `data/elements.json` (13 elem), a fajok
24 hőse. Formátum: [README.md](README.md). Kapcsolódó döntések: D22 (képességmodell), D24 (módosítók).

---

## A. Hősök

### HOS-1 – Toborzás, korlát, újraélesztés

- Hős: `hero: true` egység; a faj hősidéző épületében képezhető. Játékosonként **legfeljebb 3** élő vagy sorban
  álló hős, és ugyanabból egyszerre csak egy (G 3475–3491).
- Elesett hős állapota (szint, tapasztalat, pontok, megtanult képességek, leltár, örök bónuszok) megmarad
  (`hero_states`); újraképzése **újraélesztés**: idő × `(0,6 + 0,12 × szint)`, ugyanazon az áron (G 3544).
- Hadjáratban a hős állapota a profilban megmarad; a küldetés minimális kezdőszintet írhat elő (`set_min_level`).

### HOS-2 – Tapasztalat és szintek

- **Szinthatárok** (H 14): 0, 200, 500, 900, 1400, 2000, 2700, 3500, 4400, 5400 → legfeljebb 10. szint.
- **Tapasztalat egy ölésért** (`xp_value`): a célpont `xp` mezője; hősnél `100 + 60 × szint`; egyébként
  `8 + 0,18 × (költség / count)`, épületnél ennek fele. Idézett (`lifetime`) áldozat nem ad tapasztalatot.
- **Elosztás:** a gyilkos játékos minden élő, nem maximális szintű hőse, aki 12 csempén belül van, egyenlően
  osztozik; ha nincs ilyen, de a gyilkos maga hős, ő kapja.
- **Szintlépés:** +1 képességpont, a `growth` szerinti növekedés (szintenként: `hp`, `damage`, `armor`, `energy` …;
  a 24 hősnek 20 különböző növekedési profilja van), és azonnal +25% `max_hp` gyógyulás.

### HOS-3 – Képességtanulás

- A hős `hero_abilities` listájából tanul (a hősöknek 4 képessége van, ebből egy végső).
- Szintkövetelmény: a képesség `n`. szintje `2n − 1`. hősszinttől (1., 3., 5.); a végső (`ultimate`) a 6. hősszinttől.
  A képesség szintjeinek száma a `levels` hossza (alapból 3, végső 1).
- A gép és a hadjárat-kezdés automatikusan tanul: a végsőt előnyben, egyébként körkörösen (a legkevésbé tanultat).

### HOS-4 – Statisztika-összesítés

- `bonus` = növekedés × (szint − 1) + örök bónuszok (`perm`, tómokból) + viselt tárgyak `bonus` mezői
  (`hp`, `damage`, `armor`, `energy`, `regen`, `speed_pct`, `energy_regen`); minden **összeadódik**.
- Tárgycserénél az életerő és az energia **aránya** marad meg (különben a cserével gyógyítani lehetne); szintlépésnél
  és tómnál az abszolút növekmény adódik hozzá.

---

## B. Tárgyak

### TARGY-1 – Leltár és mozgatás

- 6 hely hősönként. Felvétel: a hős odamegy és felveszi (tele leltárnál nem). Eldobás: véletlen irányba 18 px-re.
  A földön heverő tárgyat bármelyik hős felveheti (a tulajdonostól függetlenül).
- Használat (`consumable`): `heal` (250 – teljes életerőnél elutasítva), `energy` (100), `xp` (300 – Emlékkönyv),
  `perm` (Erőtóm +2 sebzés, Életerőtóm +60 életerő); használat után elfogy.
- Viselt (`equip`): a `bonus` folyamatosan érvényes (HOS-4).
- **Ritkaság:** `common` (6), `rare` (6), `artifact` (3).

| Tárgy | Fajta | Ritkaság | Hatás |
|---|---|---|---|
| Gyógyfőzet / Éterfőzet | fogyó | közönséges | +250 életerő / +100 energia |
| Emlékkönyv | fogyó | ritka | +300 tapasztalat |
| Erőtóm / Életerőtóm | fogyó | ritka | örökre +2 sebzés / +60 életerő |
| Csizma, Védőgyűrű, Karomkesztyű | viselt | közönséges | +15% sebesség / +2 páncél / +5 sebzés |
| Életamulett, Regenerációs gyűrű, Éterkő, Hősi pajzs | viselt | ritka | +150 életerő / +2 töltés / +60 energia és +1 energiatöltés / +4 páncél és +100 életerő |
| Ércpenge, Árnyköpeny, Örök Hajnal | viselt | ereklye | +12 sebzés és +5% / +12% sebesség, +2 páncél, +1 töltés / +200 életerő, +100 energia, +6 sebzés |

### TARGY-2 – Tárgy a pályán

- Tábor kiirtása (C), küldetés-esemény, illetve az Ooni Tanuki-bányász ritka lelete (`found_item`) tesz tárgyat a
  földre.

---

## C. Semleges táborok (Vadon és kisnép-táborok)

### TABOR-1 – Elhelyezés

- Egyéni csatában (a hadjáratban a küldetés beállítása szerint) a semleges „Vadon” játékos táborai: kezdőhelyenként egy könnyű és
  egy közepes, a felezőkön erősebbek, középen a főnöktábor – **tükrözve**; a kezdőhelytől 12 csempén belül nincs tábor.
- **Szintek** (`vadon.camps`, 1–5): pl. 1: 3 Kristálybogár; 5: Ércgólem, Kőtroll, 2 Éterlidérc. Zsákmány: 1–2
  közönséges, 3 közönséges vagy ritka, 4 ritka, 5 ereklye.
- A csatapálya szintenként egy **kisnép táborát** is kérheti (`map.camps`): a nép szerepköreiből (`roles`: közelharcos,
  pajzsos, lövész, varázsló, nagy) a szintenkénti recept vagy a nép saját összetétele (`levels`) szerint; a
  zsákmány a Vadon azonos szintű táboráé.

### TABOR-2 – Viselkedés

- A tábor egysége csak a tábora mellett (1,5 csempén belül) keres célpontot, 5 csempés körben; ha egy egység
  támad, **az egész tábor felriad**. Ha 7 csempénél messzebb kerül, visszatér, közben nem támad, és 10%/s-mal gyógyul.
- A teljesen kiirtott tábor egyszer zsákmányt dob (a szint ritkaságlistájából véletlen ritkaság, abból véletlen tárgy);
  3. szinttől 60% eséllyel egy főzet is. A tábor nem éled újra.
- A tábor egységei tapasztalatot adnak (HOS-2).

---

## D. Elemek

### ELEM-1 – Hozzárendelés és hatás

- Az elem a képességhez vagy az egységhez **külön táblában** tartozik (`elements.json`: `abilities` 61, `units` 42;
  `kezi`: kézi felülírások). Találatkor (`apply_element`, G 970) a célpont állapotot kap; a paraméterek az
  `effects` részben.

| Elem | Állapot | Hatás | Kölcsönhatás |
|---|---|---|---|
| Tűz | égés | sebzés idővel: `sebzés × 0,25`, 2–25/s, 3 s, páncélon át | felolvasztja a fagyást; vizes célpont nem gyullad meg (a víz felszárad) |
| Jég | fagyás | lassítás 0,6, 2 s | kioltja az égést |
| Villám | sokk | 0,4 s kábulás, célpontonként 3 s-onként | vizes célponton szünet nélkül és 2× hosszabb |
| Méreg | mérgezés | `sebzés × 0,15`, 1,5–15/s, 4 s | gépre nem hat |
| Víz | átázott | 5 s | eloltja az égést; erősíti a villámot |
| Gyökér | gúzsba kötve | 2,5 s nem mozdul (támadhat) | repülőre nem hat |
| Álom | alszik | 6 s; az első sebzésre felébred | hősre csak 1 s kábulás |
| Rémület | megrémült | 1,6 s menekül 3 csempére az okozótól | – |
| Vakítás | elvakult | 4 s; távolsági támadásai 50%-ban mellémennek | – |
| Szél | hátralökés | 0,45 csempe taszítás (területi mező) | repülőre és óriás hősre nem hat |
| Zúzás | páncéltörés | −2 páncél 6 s (épületre is) | – |
| Némítás | elnémítva | 3 s nem varázsolhat; a bekapcsolt képességei kikapcsolnak | – |
| Jelölés | megjelölt | 8 s; +20% sebzést kap, a jelölő a köd alól is látja | – |

---

## E. Átvizsgálás

1. **Az állandók a kódban vannak:** szinthatárok, 12 csempés tapasztalat-kör, 6 leltárhely, 3 hős, az újraélesztés
   képlete, a szintlépési 25% gyógyulás, a tábor 1,5/5/7 csempés körei, a 60%-os főzet és a főzetek azonosítója –
   modból nem állíthatók.
2. **A tárgyak csak statot adnak.** A műfajban (WC3) a tárgyak aktív képességeket (töltetekkel), aurákat és
   találatkori hatásokat is adnak; a TOTU 15 tárgya mind statnövelő vagy egyszeri fogyó. Nincs bolt vagy kereskedő,
   így tárgyhoz csak tábor és küldetés révén lehet jutni.
3. **Két hely egy elemhez.** Az elem nem a képesség/egység adatában, hanem külön táblában van (kézi felülírásokkal) –
   a modder két helyen dolgozik.
4. **Az elemek kölcsönhatása kódba égetett** (tűz–jég, víz–tűz, víz–villám): jól átgondolt, karakteres rendszer, de
   új elem vagy új kölcsönhatás csak kóddal adható.
5. **Halmozás elemenként más:** égés és méreg a nagyobb sebzésűt tartja meg, a fagyás a nagyobb lassítást, a
   többi a hosszabb időt – ez a D22 állapotmodell halmozási szabályaival egységesíthető.
6. **Újabb név szerinti szabály:** G 1058 – a Nian-őr (`oon_nian_or`) bekapcsolt Kapuőrzéssel (`oon_kapuorzes`) a
   lovasokat (`utype: lovas`) egyszer lelassítja. (A README táblázatába felvéve.)
7. **Jó és megtartandó:** a WC3-szerű szintrendszer és képességtanulás; a hős állapotának megőrzése (újraélesztés,
   hadjárat); a tárgycsere aránymegtartása; a tükrözött táborok és a felriadó tábor; az elemek kölcsönhatásai.

## F. Javaslatok (0 A.D., WC3) és modell

| Forrás | Megoldás | Mit old meg |
|---|---|---|
| — | **`game.heroes` és `game.camps` adatszakasz**: szinthatárok, tapasztalat-kör és -képlet, leltárméret, hőskorlát, újraélesztési képlet, szintlépési gyógyulás, tábor-körök, főzet-esély és -lista | E-1; a játékmenet nem változik |
| **WC3** – *tárgy = képességek hordozója* (aktív képesség töltetekkel, passzív, aura, találatkori hatás; egyedi tárgy egy példányban viselhető) | a tárgy a D22 képességeket és a D24 módosítókat adja a viselőjének | E-2 (tárgymélység) |
| **WC3** – *bolt / kereskedő* (semleges bolt a pályán, faji tárgybolt, főzetbolt) | semleges kereskedő épület a csatapályán és/vagy fajonként egy tárgybolt | E-2 (tárgyhoz jutás) |
| **WC3** – *tárgytáblák szintenként* (a tábor szintje szerinti zsákmánylista) | a zsákmány adatban leírt táblákból | E-1 |
| **0 A.D. / D22** – *StatusEffects* | az elem egy állapot (D22) + `elem` címke a képességen/egységen; a kölcsönhatás adatban: `removes` (pl. a víz eltávolítja az égést), `amplifies` (a víz × 2 a sokkra), `immune_tags` (méreg: `mech`) | E-3, E-4, E-5; a mai viselkedés pontosan leírható |

### Tulajdonosi döntést kívánó pontok

1. **Állandók adatba és elemek állapotként** (F 1. és 5. sor) – a játékmenet nem változik, a modolás bővül. Javaslom.
2. **Tárgyak mint képességhordozók** (aktív tárgyak töltetekkel, aurák, találatkori hatások, egyedi tárgyak) – új
   tartalmat kíván (a mai 15 tárgy marad; újak a balansz része). Javaslom.
3. **Bolt / kereskedő** – semleges kereskedő a csatapályán, faji bolt, vagy egyik sem (a tárgy csak táborból és
   küldetésből jön, mint ma).
4. **Táborok újraéledése** – ma nem élednek újra (mint a WC3-ban); maradjon így, vagy pályabeállítás legyen.

---

**Döntés (D25, 2026-11-12):** állandók adatba és elemek állapotként; tárgyak képességhordozók; bolt (semleges
kereskedő, faji bolt) pálya- és lobbibeállítás szerint; a táborok újraéledése pályabeállítás.
