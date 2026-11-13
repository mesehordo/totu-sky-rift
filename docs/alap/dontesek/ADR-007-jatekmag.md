# ADR-007: Játékmag – függőleges szelet (M6)

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-13 (D37)
**Érintett:** M6 (játékmag), M7 (minden faj – erre a keretre épül), M8 (felület), M9 (gépi ellenfél: a látott
információ), M10 (hálózat: a parancssor és a hash), M11 (küldetések).
Alap: ADR-004 (fixpontos számábrázolás), ADR-005 (adatformátum), ADR-006 (térkép és mozgás), D35 (napszak és
időjárás), D37; a viselkedés forrása a régi Python-játék (`rts/game.py`, `rts/entities.py`) és a specifikációk
([FAJMECHANIKAK](../spec/FAJMECHANIKAK.md), [KEPESSEGEK](../spec/KEPESSEGEK.md),
[KUTATAS_ES_SZABALYOK](../spec/KUTATAS_ES_SZABALYOK.md), [NAPSZAK_ES_IDOJARAS](../spec/NAPSZAK_ES_IDOJARAS.md)).

## Probléma

Az M2–M5 után megvan a determinisztikus mag, a Lua-réteg, a pálya és a mozgás, valamint a megjelenítő, de még nincs
játék: nincs gazdaság, építés, termelés, és a harc is csak az M2-es próbaváltozat (közelharc, egyetlen sebzésszám).
Az M6 célja egy **függőleges szelet**: egyetlen nép (az Ursanok) teljesen, minden alaprendszerrel, és egy végigjátszható
csata egy szkriptelt ellenfél ellen. Közben a szimuláció a rajzolástól külön szálra kerül (az M5 mérése szerint
1600 egység csatájában egy tick p99 ~30 ms, ami egy szálon nem fér el a 60 FPS mellett).

## Döntések (D37, a tulajdonos 2026-11-13)

1. **Felület:** az M6-ban ideiglenes, saját rajzolású HUD (a sprite-köteggel és bitképes betűvel); a végleges
   RmlUi-felület az M8-ban váltja le.
2. **Szabályok:** a régi játék képletei és számai szerint; a szabályokat próbák rögzítik, és minden eltérés csak
   döntéssel lehetséges (a balansz az M7-ben).
3. **Hősök:** az M6-ban egységként (harcolnak, használják a képességeiket, de szint és tárgy nélkül); a hősrendszer
   (szint, tapasztalat, tárgyak, táborok) az M7-ben.
4. **Kapumeccs:** ursan tükörmeccs; az ellenfél egyszerű Lua-szkript (építési sorrend, támadóhullámok) – a valódi
   gépi ellenfél az M9-ben.

## Döntési javaslat

### 1. A szimuláció külön szálon

- A szimuláció saját szálon fut a fix 30 Hz-es ütem szerint; a kliens **parancsokat** küld neki (szálbiztos sor; a
  parancs a következő tickben érvényesül, mint a hálózati játékban), és **pillanatképet** kap vissza.
- **Pillanatkép:** tickenként egy megváltoztathatatlan, a rajzoláshoz elég adatcsomag: az egységek és épületek helye,
  iránya, állapota (mozog, támad, épít, gyűjt), életereje, tulajdonosa, a lövedékek, a látványesemények (találat,
  halál, képesség) és a játékos láthatósági rácsa. Hármas puffer: a szimuláció ír, a kliens a legutóbbi kettő között
  simít. A rajzolás sosem várja a szimulációt.
- A determinizmust a szál nem érinti: a szimuláció bemenete csak a tickhez kötött parancssor; a hash, a mentés és a
  visszajátszás változatlan.

### 2. Rendszerek és sorrend

Tickenként rögzített sorrendben futnak:

1. parancsok;
2. környezet (napszak, időjárás);
3. módosítók és állapotok;
4. gazdaság;
5. építés;
6. termelés;
7. célválasztás és mozgás;
8. harc és lövedékek;
9. képességek;
10. halál és takarítás;
11. látótér;
12. Lua-horgok.

A rendszerek C++-ban vannak. A faji különlegességek az M3-as Lua-horgokon át csatlakoznak (a specifikáció jelöli,
melyik melyik).

Az adat a meglévő sablonokból jön (ADR-005; `templates/units`, `buildings`, `researches`, `abilities`, `statuses`);
új mező csak sémával kerül be.

### 3. Pályaobjektumok

- A `map.json` új szakasza:
  - kezdőhelyek (játékosonként a főépület és a kezdő munkások);
  - fák (csempénként, `node_amount`);
  - arany- és meteoritlelőhelyek;
  - semleges objektumok (a táborok az M7-ben).
- A fák és a lelőhelyek akadályok a navrácsban. A kivágott fa helye járható lesz (navrács-frissítés, mint az építésnél).

### 4. Lépések

1. **Szimulációs szál és pillanatkép:**
   - a kliens a pillanatképből rajzol, parancsot a sorba tesz;
   - mérés: 1600 egység csatájában a képkocka p99 ≤ 16,7 ms akkor is, ha a tick ~30 ms.
2. **Gazdaság:**
   - pályaobjektumok, nyersanyagok (fa, arany, meteorit; az áhítat az M7-ben);
   - munkás gyűjt, hord és lead a legközelebbi leadóhelyen, kimerült lelőhely után a következőre megy;
   - létszám (MECH-SUPPLY), Hadtáp (MECH-UPKEEP), nyersanyag-statisztika.
3. **Építés:**
   - lerakás a lábnyom szerint (terep, magaslat, foglaltság; a navrács frissül);
   - építési stílusok (MECH-BUILD; az Ursanoknál `enter`: a Kalapácshordozó bemegy és bentről épít);
   - előfeltételek;
   - a félkész épület megszakítása 75%-os visszatérítéssel; kész épület lebontása 70%-kal, csak egyes népeknél (D38);
   - az épülő épület életereje a haladással nő.
4. **Termelés:**
   - termelési sor (költség, létszám, idő), gyülekezőpont;
   - visszavonás visszatérítéssel; a létszámkorlát megállítja a sort;
   - kutatás ugyanebben a sorban (az 5. lépés hatásaival).
5. **Harc:**
   - a régi `apply_damage` csővezetéke: típusszorzó, címkebónusz, páncél, pajzs, területi sebzés fél erővel;
   - lövedékek (repülési idő, fixpontos pálya, ugyanazon a gépen és minden gépen azonos);
   - célválasztás (a legközelebbi ellenség), támadó mozgás, állások (támad / tart / passzív), üldözési határ;
   - a magaslat szabályai (D33, ADR-006);
   - elemek és állapotok annyiban, amennyit az Ursanok használnak, a közös kerettel együtt (`statuses.json`).
6. **Látótér és lopakodás:**
   - játékosonkénti láthatósági rács a szimulációban (determinisztikus, a hash része);
   - magaslatról és magaslatra (D33); éjjel −25% (D35), fényforrásnál nem;
   - felderített terület;
   - rejtett egységek és felfedés (`detect`);
   - a kliens ködét ez adja (az M5 ideiglenes köde megszűnik);
   - a parancsok és a Lua-API csak a látottat adja (AI-FOG alapja, M9).
7. **Kutatás, módosítók és környezet:**
   - kutatás: KUT-RES, KUT-EFF, KUT-RULE; feltételes módosítók (D24: `when` napszak / időjárás / csempe);
   - környezeti rendszer (D35): napszak a tickből; időjárás saját véletlenszám-folyamból, a pálya éghajlata szerint, fokozatos átmenettel; a kapcsolók a meccs indításakor;
   - az általános hatások és az Ursanok faji hatásai *hatással* módban;
   - pályaváltozás az esőre (sár, pocsolya) és a hóra (befagyó tó és gázló); a többi az M7-ben.
8. **Az Ursanok teljesen:**
   - 18 egység, 15 épület, 16 kutatás;
   - 16 képesség: beszállás, fegyverbe, pajzsfal, dárdafal, roham, taposás, harci kürt, medveüvöltés, vérszomj, szellemtest, gyógyítás, horgonyszigony, zuhanó csapás, rúnakő és rúnakő-aura, település;
   - a három hős egységként;
   - minden elemhez viselkedési próba a specifikáció szerint.
9. **Játszható szelet:**
   - ideiglenes HUD:
     - felül: nyersanyagok, létszám, napóra és időjárás-ikon;
     - alul: kijelölés, életerő, termelési sor, parancsgombok, építési menü;
     - épület lerakása kísértetképpel; a billentyűk a régi játék szerint;
   - szkriptelt ursan ellenfél (Lua);
   - győzelem és vereség (minden épület elpusztult);
   - kapuvisszajátszás (golden M6).

### 5. Kapu (M6)

- A **golden M6** visszajátszás (gazdaság, építés, termelés, kutatás, harc, időjárás, egy teljes meccs) mind az öt
  rendszeren azonos hash-sorozatot ad.
- Egy ursan tükörmeccs végigjátszható a szkriptelt ellenfél ellen, győzelemmel vagy vereséggel.
- 1600 egység csatájában a képkocka p99 ≤ 16,7 ms (a szimuláció külön szálon), és a szimulációs tick p99 ≤ 33 ms
  (a 30 Hz-es ütem tartható).
- Az ursan egységek, épületek, kutatások és képességek viselkedési próbái zöldek.

## Következmények

- A pillanatkép a később hálózati játék megfigyelőjének és a visszajátszás-nézőnek is az alapja.
- Az M2-es próbaharc az új harcrendszerrel váltódik ki; a golden M2–M4 visszajátszások érvényessége
  megmarad, mert a régi parancsok viselkedése (mozgás, alakzat, kapu, építés-próba) nem változik. Ha a harc miatt
  mégis változna, a változás szándékos, és újrarögzítés jár vele.
- Az ideiglenes HUD-ot az M8 eldobja; ezért csak a játszhatósághoz szükséges része készül el, kinézeti munka nélkül.
