# A szimuláció alapszabályai (a kódból kiolvasva)

Kelt: 2026-11-11 · kód: `c5bdbf5`. A [SZERZODES.md](SZERZODES.md) kiegészítése: **mit** számol a szimuláció.
A leírás a mai Python-kódot rögzíti, nem tervet. Ha a kód és ez a leírás eltér, a kód az igazság, és a leírást
kell javítani. Hivatkozások: `rts/game.py` (G), `rts/entities.py` (E).

A 3. fejezet (közös feladat) az F3 A/B méréshez készült: egy egyszerűsített részhalmaz, amit Pythonban, C#-ban és
C++-ban egyformán meg kell valósítani.

## 1. Egységek, távolság, idő

- **Csempe:** `TILE` = 32 px. A pálya `w × h` csempe; minden csempe járható (`FREE`) vagy blokkolt (terep, épület,
  nyersanyag).
- **Időlépés:** 1/30 s. A sebességek csempe/s-ban vannak az adatban; a kód px/s-ra váltja (`× TILE`).
- **Egység teste:** kör, sugara `radius` × TILE (alapérték 0,4). **Épület:** téglalap, `size` csempe.
- **Távolság (`gap`, E:38):** két entitás *széle* közti távolság. Egység–egység: középpontok távolsága mínusz a
  két sugár; épületnél téglalap-távolság.

## 2. Szabályok

### 2.1 Egy tick sorrendje (`Game.update`, G:284)

1. `tick += 1`, `time += dt`.
2. Térbeli hash újraépítése (128 px-es cellák az egységekhez, 48 px-es az ütközéshez).
3. Útkérés-sor feldolgozása: tickenként legfeljebb 6 egységnyi keret; újratervezés 3-at ér (G:427).
4. Időszakos rendszerek: felemelkedés (15 tickenként), fény/őrök (30), bányák (8), morál/pontok/áhítat (15) stb.
5. Minden élő **egység** `update`-je (E:655), majd minden **épület**, **lövedék** és **területi hatás**
   `update`-je, ebben a sorrendben, a listák sorrendjében.
6. **Szétválasztás** (`_separate`, G:560).
7. Halottak kiszűrése a listákból.
8. Létszám újraszámolása (G:2960); 30 tickenként a nyersanyag újranövése, győzelem-ellenőrzés, bevételek.
9. Látótér játékosonként 6 tickenként (G:3033).
10. Gépi ellenfelek (`AI.update`), kampány-rendező, mod-hookok.

### 2.2 Mozgás

- **Sebesség (E:378):** `speed × (1 + speed_pct/100) × módosítók × lassítás × (saját nyálkán: creep_speed_mult) × TILE`
  px/s. Gúzsba kötve 0.
- **Útkérés:** a cél beállításakor (`set_path`) a földi egység a sorba kerül; amíg a sor fel nem dolgozza, áll.
  Repülő egység egyenesen megy.
- **Útvonal (G:440):** ha a start és a cél között egyenes vonalon elfér (4 px-es mintavétel, sugárral), egyetlen
  pont. Különben 8 irányú A* csempén (`find_path`), lépésköltség 1 és √2, oktilis heurisztika, legfeljebb 9000
  kibontott csomópont; ha a cél elérhetetlen, a legközelebbi elért csempéig vezet. Átló csak akkor, ha mindkét
  oldalcella is elfér. Utána simítás (6 pontig előretekintve).
- **Útkövetés (E:456):** lépés = `speed × dt` a következő pont felé. Ha az új helyen a test nem fér el (fal) →
  újratervezés. Ha másik egység áll az útban (`_blocker`) → kitérési kísérlet (`_steer`: 6 elfordított irány),
  különben vár; `wait_t` nő. 0,8 s-onként (+0,15 s újratervezésenként) haladásellenőrzés: ha alig mozdult →
  újratervezés a közeli egységek elkerülésével (`repaths`, legfeljebb 8).
- **Szétválasztás (G:560):** az egymásba lógó, azonos rétegű (föld/levegő) egységpárok átfedésének felét
  tickenként feloldja; a mozgó egység tér ki, az álló nem tolódik. A gyűjtő munkások átmehetnek egymáson. A falba
  tolás tilos.

### 2.3 Célkeresés és harc

- **Tétlen harci egység (E:753):** 0,4 s-onként keres ellenséget a látótávolságán (`sight`, csempe) belül, és
  automatikus támadásba kezd; az otthonától 9 csempénél messzebbre nem üldöz.
- **Célválasztás (`find_enemy`, G:805):** a sugáron belüli ellenséges, látható, eltalálható (föld/lég)
  egységek közül a legkisebb `távolság + (nem harcoló: +4 csempe)` értékű; ha nincs, a legközelebbi épület
  (a tornyok 3 csempével előnyben).
- **Támadás (E:135):** ha a cél hatótávon belül van (`gap ≤ range × TILE + 3`), és a `cooldown` lejárt:
  `cooldown = attack.cooldown`. Lövedékes fegyvernél lövedék indul, közelharcnál azonnali találat.
- **Lövedék (E:1756):** 15 csempe/s (szikla: 11), követi a célt; becsapódáskor találat. Ha a cél közben eltűnt,
  csak a területi sebzés érvényes.
- **Sebzés (G:1041), a sorrend számít:**
  1. alap = `damage` (+ fejlesztések, hősbónusz) × módosítók × falkaszorzó (E:126);
  2. elemi hatás (tűz, jég, villám …) és megjelölés (×1,2);
  3. nem varázslatnál × **típusszorzó** (`data/game.json` `unit_types.matrix`, pl. közelharcos → lándzsás 1,25);
     plusz a címke szerinti `bonus`;
  4. faji szorzók (Holdtölte, Tavak útja, Medve-ösztön), távolsági sebzés-módosító;
  5. ideiglenes védőfény, majd **pajzs** nyeli el;
  6. a maradékból **páncél**: `max(0,5, sebzés − páncél)` (varázslat a páncélt figyelmen kívül hagyja);
  7. `hp -= sebzés`; ha `hp ≤ 0` → halál (G:1159).
- **Területi sebzés:** a `splash` sugarú körben minden további ellenség a sebzés 50%-át kapja.
- **Visszatámadás:** a tétlen, harcképes, nem munkás egység a támadóját automatikusan célba veszi.
- **Regenerálódás:** pajzs késleltetés után; életerő és energia folyamatosan, faji szabályok szerint.

### 2.4 Gazdaság, termelés, létszám

- **Nyersanyagok** (`data/game.json`): Fa (`wood`), Arany (`stone`), Meteorit (`meteor`), Áhítat (`faith`).
  Gyűjtési idő és rakomány nyersanyagonként (pl. fa: 2,2 s, 8 egység).
- **Gyűjtés (E:860):** odamegy (22 px-en belül „ott van”), dolgozik `gather_time × rakomány/alaprakomány` ideig,
  visszaviszi a legközelebbi lerakóhoz, újrakezdi; kimerült forrásnál a legközelebbi azonos fajtát keresi.
- **Kiképzés (G:3476):** ellenőrzi sorban: hős-korlát (3), termelő épület, előfeltételek, költség, épületenkénti
  korlát, **létszám** (`supply_used + supply ≤ supply_cap`). Ha jó, levonja a költséget, és a sorba teszi.
- **Termelési sor (E:1687):** a sor első eleme (párhuzamos épületnél az összes) halad; ha elkészül, az egység
  megjelenik.
- **Létszám (G:2960):** tickenként újraszámolva: `cap = Σ kész épület supply (+ faji szabályok)`, legfeljebb 200;
  `used = Σ élő egységek supply + a sorban lévők`.

### 2.5 Látótér

- Játékosonként csempe-alapú `visible` és `explored` bájttömb, 6 tickenként újraszámolva a saját és a szövetséges
  egységek/épületek látótávolságából. A célkeresés csak látható ellenséget választ.

### 2.6 Ami szándékosan kimaradt ebből a leírásból

A 39 faji szabály (`rules`), a hősök és tárgyak, a képességek és varázslatok (`_o_cast`, autocast), a morál, az
idézések, a bányák, a helyőrség, a felemelkedés és a fejlődési irányok. Ezek az adatokból és a `game.py` saját
metódusaiból jönnek; portoláskor modulonként kell őket leírni, a golden replay-ekkel ellenőrizve.

## 3. Közös feladat az F3 A/B méréshez (javaslat)

Cél: ugyanazt a terhelést Pythonban, C#-ban és C++-ban mérni, ugyanazokkal a szabályokkal. Ez szándékosan **nem
a teljes TOTU**, csak az a mag, ami a mérések szerint a költséget adja (útkeresés, mozgás, ütközés, célkeresés).

**Bemenet** (JSON, a Python-játékból exportálva): a pálya blokkrácsa (`w × h` bájt); egységtípusok
(`radius`, `speed`, `hp`, `armor`, `attack: damage/range/cooldown/projectile/splash`, `sight`, `utype`); a
típusszorzó-mátrix; kezdő egységek (tulajdonos, típus, x, y); parancsforgatókönyv (tick, egységlista, cél, fajta:
move / attack-move).

**Szabályok:** 1. fejezet és 2.1–2.3, a következő egyszerűsítésekkel: nincs gazdaság, termelés, képesség, elem,
pajzs, faji szabály; a látótér csak a célkereséshez kell (2.5).

**Forgatókönyvek:** a pálya `nyolc_tuz` (144 × 144) blokkrácsa; 8 csapat; egységszám 250 / 500 / 800 / 1200 /
1600 / 2400; 10 perc (18 000 tick); (a) mindenki a középre tart attack-move-val, (b) két-két sereg szemben egy
szoroson, (c) 25% / 50% / 100% aktív mozgás.

**Mérés:** tick p50/p95/p99/max, memória, C#-nál GC-szünetek és allokáció/tick; mindhárom nyelven ugyanazon a
gépen. **Helyesség:** a determinisztikus nyelveken azonos bemenetre azonos állapothash-sorozat; a három nyelv
között csak a statisztikák összevethetők (célbaérési arány, elakadások száma, túlélők), mert a lebegőpontos
kerekítés eltérhet.

**Döntési szabály** (`04_DONTESI_MUNKALAP` szerint): ha két jelölt 5 százalékponton belül van, nincs győztes;
a Python-kontroll eredménye megmutatja, mennyi a nyelv és mennyi az algoritmus hatása.
