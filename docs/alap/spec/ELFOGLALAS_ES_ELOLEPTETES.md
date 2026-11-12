# Épületelfoglalás, megszállás és előléptetés – tervezési specifikáció (D30)

Kelt: 2026-11-12 · új mechanikák az új motorhoz (M7), a 0 A.D. *Capturable* és *Promotion* komponense nyomán,
a tulajdonos döntései szerint. A számok (idők, arányok) balanszmunka – az M7-ben, próbával rögzítendők; a
felsorolt alapértékek kiinduló javaslatok.

## 1. Két képesség épületre

Mindkettő a D22 képességmodell szerinti **egységre célzott** képesség (célpont: ellenséges vagy semleges épület),
**varázslási idővel** (csatornázás – sebzés megszakítja), és adatból kiosztható.

### KEP-ELF – Elfoglalás

- **Hatás:** az épület a képesség használójáé lesz (tulajdonosváltás); a használó egység **elfogy** (beköltözik).
- **Mit tud az elfoglaló:** az épület **saját listáját** képezheti és kutathatja (a másik nép egységeit és
  kutatásait); az előfeltételeket az elfoglaló saját – köztük más elfoglalt – épületei adják. Az épület
  **képességeit** (épület- és főépület-képességek, `powers`) **nem** használhatja.
- **Ami nem száll át:** az eredeti tulajdonos már kikutatott fejlesztései nála maradnak; az épületben folyamatban
  lévő képzés és kutatás megszakad, az ára az eredeti tulajdonosnak visszajár.
- **Kiinduló javaslat:** varázslási idő 8 s; csak 50% életerő alatti épületre; a főépület nem foglalható el.

### KEP-MEG – Megszállás

- **Hatás:** a használó egységek **beköltöznek** az épületbe, de a tulajdonos **nem változik**.
- **Amíg bent van legalább egy megszálló, és a tulajdonos ellenfél:** a tulajdonos ebben az épületben **nem képezhet
  és nem kutathat** (a már kikutatott fejlesztései megmaradnak); a bent lévő **távolharcos** megszállók
  **kilőnek** az épületből (a helyőrség mintájára, a bent lévők védettek).
- **Férőhely az épület méretétől:** kicsi (legfeljebb 2 × 2) – 2, közepes (3 × 3) – 4, nagy (4 × 4-től) – 6 egység.
- **Kiűzés:** ha a tulajdonos 50% életerő alá sebzi a saját épületét, vagy saját egységgel visszafoglalja (elfoglalás-
  vagy megszállás-képességgel); ha az épület ledől, a megszállók a fél életerejükkel kijönnek.
- Semleges és szövetséges épületre: semleges épület megszállható (őrhely), szövetségesé nem.

### Kiosztás (a tulajdonos megbízásából – népenként 2–3 egység, két kisnépnél minden egység)

| Nép | Elfoglalás | Megszállás | Indok |
|---|---|---|---|
| Szürke elfek | Tollas harcos | Jaguár-orvlövész | a harcos-rend és a lesből lövő |
| Ursanok | Telepes | Szigonyvető | a telepes birtokba vesz; a szigonyos a falakról lő |
| Croftianok | Csápszolga | Tüskeköpő | a szolga „megfertőzi” az épületet; a köpő fészket rak belé |
| Wullof | Lándzsás harcos | Bumerángvető | a harcos foglal, a vadász lesben ül |
| Aurethi Birodalom | Fabrikátor, Légiós | Ballistarius | a gépész átalakítja, a légió birtokba veszi; a lövész bástyát csinál belőle |
| Huk’ucha | Kampókardos | Csúzlis | |
| Ooni | Oni harcos, Tanuki-bányász | Kitsu-mata | a tanuki a mesékben furfangos; a kitsu-mata nindzsa-múltú lövész |
| Micéliák (mod) | Pöfeteg | Tüskegomba-lövész | |
| **Ghoblin** (kisnép) | minden egység | minden egység | a vörössipkások portyázó, rajtaütő nép |
| **Daresh** (kisnép) | – | minden egység | a darázsnép fészket rak az épületekbe |

## 2. Előléptetés (veterán egységek)

- A **nem hős** harci egységek tapasztalatot gyűjtenek (a hősökével azonos képlettel, az ölés értéke szerint);
  két fokozat: **Veterán** és **Elit**. A hősök szintrendszere változatlan.
- **Kiinduló javaslat:** fokozatonként +10% életerő és sebzés, Elitnél +1 páncél; a küszöb az egység saját
  költségének kétszerese, illetve ötszöröse tapasztalatban; munkás, idézett és épület nem lép elő.
- A fokozat látszik az egységen (jelvény) és a panelen; modolható (`Promotion` komponens: fokozatok, küszöbök,
  módosítók – D24).

## 3. Megvalósítás az új motorban

- Komponensek: `Capturable` (elfoglalható/megszállható, férőhely, küszöb), `Occupants` (a bent lévők),
  `Promotion` (fokozatok); képességhatások: `capture`, `occupy` (D22 `transform`/`spawn` mellé); a tiltás
  („a tulajdonos nem képezhet”) a feltételrendszerrel (FORRASOK_ES_TANULSAGOK 2.4).
- Próbák (M7): elfoglalás után a másik nép egysége képezhető, a képesség nem; megszállás alatt a tulajdonos
  képzése tiltott, a kutatásai megmaradnak, a távolharcos megszálló lő; 50% alá sebzés kiűz; előléptetés a
  küszöbnél, a módosítók érvényesek.
