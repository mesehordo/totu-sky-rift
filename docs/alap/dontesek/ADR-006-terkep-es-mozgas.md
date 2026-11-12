# ADR-006: Térkép és mozgás

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-12 (D32, D33)
**Érintett:** M4 (térkép, mozgás), M5 (megjelenítő: terep, szintek), M6 (látótér, harc a magaslaton), M7 (fajok:
tó, fény, terjedő talaj), M9 (gépi ellenfél), M11 (szerkesztő).
Alap: ADR-004 (fixpontos számábrázolás), ADR-005 (adatformátum), D32, [FORRASOK_ES_TANULSAGOK](../FORRASOK_ES_TANULSAGOK.md) 2.3.

## Probléma

A régi játék 88 × 88-as csempés pályát használ négy terepfajtával (fű, föld, víz, szikla). Az útkeresése csempés A*,
amelyhez simítás és helyi kitérés tartozik. A csoportmozgás csak a célnál rendez, és nincs mért minőségi próbája.
Az új motor céljai:

- 8 játékos × 200 létszám mozgása;
- WC3/AoE-szintű csoportmozgás;
- az elérhetetlen cél gyors felismerése;
- hidak és kapuk torlódás nélkül;
- nagy egység szűk helyen;
- StarCraft 1-szerű magaslatok;
- sokféle, modolható terep.

Mindezt bitre determinisztikusan, mind az öt rendszeren.

## Döntési javaslat

### 1. Pálya

- **Rács:** 1 csempe = 1 egység (fixpontos), négyzetes csempék. **Pályaméretek (D34):** kicsi 96 × 96, közepes
  160 × 160, nagy 256 × 256; egyedi méret legfeljebb 512 × 512 (`game.json` → `map_sizes`).
- **Navigációs rács:** fél csempe (navcella). A sugarak (0,22–0,95 csempe) így pontosan érvényesülnek. Egy
  512-es pálya 1024² navcella, ami osztályonként egy bájttömb.
- **Rétegek:**
  1. terep (csempénként terepfajta-azonosító);
  2. **szint** (−1–2, D32, D33);
  3. fedőrétegek (Huk'ucha-tó, terjedő talaj, fény; ezek tartalma M7, a hely most készül);
  4. statikus akadályok (épület alapterülete, erőforrás, fa);
  5. dinamikus akadályok (egységek, kapuk).

### 2. Terepfajták – adatban, sokféle (D32)

`content/<mod>/terrain/terrains.json`: minden terepfajta az ADR-005 szerinti sablon, öröklődéssel. Mezői:

- név;
- **járhatósági osztály** (`land`, `shallow`, `deep`, `cliff`, `ramp`, `bridge` …);
- mozgásosztályonkénti **sebességszorzó**;
- építhető-e;
- elrejt-e (pl. sűrű erdő a lopakodáshoz, M6);
- a megjelenítő adatai (csempekép, változatok, átmenetek; M5).

A sok terepfajta nagyrészt *látványváltozat* azonos szabályokkal: dús fű, száraz fű, virágos rét, föld, sár,
kövezett út, földút, homok, kavics, hó, jég, hamu, láva-kőzet, mohás kő, sekély víz (gázló), mély víz, mocsár,
szirt, rámpa, híd, romos kövezet … A modok újakat adhatnak. Az induló készlet a régi pályák négy fajtáját
átveszi, a többit a grafikai készlethez igazítjuk (a csempeképek a tulajdonoséi).

### 3. „Kamu” magaslatok és mélységek (D32, D33; StarCraft 1-szerűen)

- **A szint csempetulajdonság:**

  | Szint | Jelentés |
  |---|---|
  | −1 | mélység: szurdok, mélyút, völgy |
  | 0 | alapszint |
  | 1 | magaslat |
  | 2 | nagyon magas |

  Valódi 3D nincs: a megjelenítő festett szirtperemmel és árnyékkal mutatja (M5).
- A **szirt** járhatatlan a szintek határán. A pálya `auto_cliffs` beállítása a szinthatáron magától jelöli. A
  **rámpa** járható terepfajta, két szintet köt össze. Az útkereső számára a szirt egyszerű akadály.
- **Pályaszerkesztő (M11, D33):**
  - „magaslat”, „mélység” és „alapszint” ecset, szintenként;
  - a szirtperem automatikusan keletkezik, a rámpa külön ecset;
  - az ellenőrző szól, ha egy magaslat vagy mélység rámpa nélkül elérhetetlen (ez lehet szándékos, pl. csak
    repülőknek).
- **Szabályok (D33, javaslat):** Δ = a támadó (vagy megfigyelő) szintje − a célpont szintje. Minden szám adatban van
  (`game.json` → `elevation_rules`), kutatás és képesség módosíthatja (D24), a balansz az M6–M7-ben véglegesíti.

  | Szabály | Javasolt érték |
  |---|---|
  | **Látótáv** | +1 csempe szintenként 0 fölött (magaslat +1, nagyon magas +2); mélységben −1 |
  | **Ráláthatás** | alacsonyabb szintről a magasabb szint ködben marad, kivéve: saját vagy szövetséges egység áll ott (2 csempén belül), repülő lát oda, vagy a magasabbról lövő egység a lövés után 2 mp-re láthatóvá válik (SC1) |
  | **Lőtáv** | távolsági egység Δ > 0 esetén +0,5 csempe szintenként (legfeljebb +1); torony és más épület +1 szintenként |
  | **Találat felfelé** | távolsági lövés Δ < 0 esetén 25% esély a hibára szintenként, legfeljebb 40%; közelharcra, varázslatra és képességre nem vonatkozik |
  | **Sebzés** | lefelé (Δ > 0) +10% szintenként, felfelé (Δ < 0) −10% szintenként (AoE-szerűen); közelharcra is érvényes |
  | **Mozgás** | rámpán felfelé ×0,85, lefelé ×1,1 |
  | **Mélység mint fedezék** | a mélységben álló egység a nála magasabb szintről csak 6 csempén belülről látható (rejtőzködő mélyút) |
  | **Repülők** | nincs szintjük: mindenhova látnak, nem kapnak és nem adnak szintbónuszt, a földről rájuk leadott lövés nem számít felfelé lövésnek |
  | **Kivételek** | az `ignore_elevation` címkéjű egység (pl. ostromgép, tüzérség) a hiba- és sebzésszabályt figyelmen kívül hagyja |

  A szabályok így két taktikai helyzetet adnak, mint a WC3-ban és a StarCraftban: a magaslat a védőnek, a mélység a
  lesben állónak kedvez. A sebzés mellett a hibalehetőség a véletlenszám-folyamot használja (ADR-004), tehát
  determinisztikus.

### 4. Mozgásosztályok és méret

- **Mozgásosztályok adatban:** `land` (alap), `amphibious`, `water` (hajóknak, a modok kedvéért; az alapjátékban
  nincs hajó), `flying` (útkeresés nélkül, csak elkerülés).
- Minden osztály terepmaszk: mely járhatósági osztályokon mehet, és milyen szorzóval.
- **Méretosztályok:** kicsi (sugár ≤ 0,5), közepes (≤ 1), nagy (≤ 1,5). Minden (mozgásosztály, méretosztály)
  párhoz *szabadtér-térkép* készül, azaz a navcellánkénti távolság a legközelebbi akadálytól. Ebből a nagy
  egységnek csak a kellően széles átjárók látszanak járhatónak (NAV-MAMMOTH). Ez a 0 A.D. megoldása.

### 5. Útkeresés: három réteg

1. **Hierarchikus réteg (0 A.D. mintájára):**
   - A navrácsot 16 × 16 navcellás klaszterek osztják fel.
   - Minden klaszterben az összefüggő járható részek *régiók*. A szomszédos régiók gráfot alkotnak.
   - A gráf összefüggő komponensei globális azonosítót kapnak. Így az **elérhetőség O(1)**: két pont között akkor
     van út, ha egy komponensben vannak.
   - **Elérhetetlen cél:** azonnal a cél körüli legközelebbi elérhető navcellára cserélődik, nincs hosszú,
     sikertelen keresés (P-002).
   - Ha egy épület elkészül vagy lebomlik, csak az érintett klaszterek számolódnak újra.
2. **Hosszú út:**
   - A* a régiógráfon, majd finomítás a kapott folyosóban (JPS, egyenletes költségű részeken).
   - Útsimítás látóvonallal, a terep sebességszorzója a költségben.
   - A kérések sorba kerülnek, és tickenként rögzített kerettel, rögzített sorrendben teljesülnek. A válasz egy
     kérés legfeljebb néhány tick múlva érkezik meg, ami determinisztikus.
3. **Flow field** azonos célú nagy csoportnak:
   - a küszöb adat, alapból 12 egység;
   - a folyosóra korlátozott integrálmező, gyorsítótárban.

   Ha a mérés szerint nem kell, kimarad. A tanulmányok tanulsága: csak mért igény esetén legyen.

### 6. Helyi réteg: ütközés és elkerülés (Pottinger)

- **Kemény sugár:** soha nem lapolódhat át. **Puha sugár:** sűrű csoportban megsérthető.
- **Prioritás:** a mozgó egység félretolja az álló, saját vagy szövetséges egységet. A harcoló egység nem tolható.
- **Tervezési emlékezet:** egy kikerült egységet egy ideig nem vizsgál újra, így nincs oda-vissza ingás.
- Szomszédkeresés **térbeli rácsban**, azonosító szerint rendezett bejárással.
- **Beragadásfigyelő:** ha egy egység N tickig nem halad, helyi kitérést próbál, aztán újratervez. Ha a cél
  közben elérhetetlenné vált, megáll a legközelebbi ponton.
- Az ORCA fixpontos átirata csak akkor jön, ha a mérés szerint a Pottinger-réteg nem elég.

### 7. Formáció (D32: mindkettő)

- **Alapból WC3-szerű:**
  - a kijelölt csoport egy egységként halad, a leglassabb tag sebességével;
  - az alakzat elöl a közelharcosokat, hátul a távolsági egységeket és a varázslókat tartja;
  - kapcsolóval kikapcsolható; ekkor mindenki a saját sebességével megy.
- **Választható AoE-alakzatok:** sor, doboz, szétszórt, szárnyas. Az alakzatok adatban vannak (helyek és
  sorrendek), így modolhatók.
- **Szűkületben** (híd, kapu, szoros) az alakzat libasorrá esik szét, a túloldalon újra összeáll (Pottinger). A
  csoport vezérpontja a hierarchikus úton halad, a tagok ehhez képest kapják a helyüket.

### 8. Híd, kapu, rámpa

- **Híd:** terepfajta (`bridge`) a víz fölött; a torlódást a helyi réteg és a formáció bontása kezeli (NAV-BRIDGE).
- **Kapu:** épület, amely a tulajdonosának és a szövetségeseinek nyitott, másoknak akadály.
  - A régiógráf a kaput játékoscsoportonként (legfeljebb 8 csapat) kezeli: a kapus élek csapatmaszkot kapnak.
    Ennek két következménye van: az ellenségnek a kapu nem út, és a kapu nyitása vagy zárása nem számolja újra a
    klasztereket.
- **Rámpa:** terepfajta, egyszerűen járható (3. pont).

### 9. Determinizmus

- Fixpontos és egész számítás.
- Rögzített döntetlenfeloldás (költség, majd navcella-index).
- Nincs rendezetlen tároló a szimulációban.
- Tickenkénti keret az útkérésekre, rögzített sorrendben.
- Az útkereső belső gyorsítótára nem része a mentésnek, mert újraszámolható. A mentés csak a kéréseket és a
  kiadott utakat tartalmazza.

### 10. Pályaformátum

`map.json`, az ADR-005 szerint:

- méret és csempekészlet;
- **terep** szöveges sorokkal és jelmagyarázattal (tesztpályákhoz, kézzel írhatóan) vagy futáshossz-kódolva (a
  szerkesztőnek);
- **szint**réteg, ugyanígy;
- objektumok: kezdőpontok, erőforrások, fák, épületek, semleges egységek.

A szerkesztő (M11) ugyanezt a formátumot menti vissza.

### 11. Próbák és kapu

Tesztpályák (`engine/tests/data/maps/`), ablak nélkül futnak, mind az öt rendszeren:

| Próba | Mit mér |
|---|---|
| **NAV-DIAG** | átlós mozgás akadályok között, mindkét irányban; nincs sarokvágás, nincs beragadás |
| **NAV-BRIDGE** | két sereg (2 × 50) egy hídon szemből; mindenki átér a mért időkereten belül, beragadás nélkül |
| **NAV-MAMMOTH** | nagy egység szűk kapun: átfér, ha elég széles, és azonnal kerül, ha nem |
| **NAV-CHOKE** | útlezárás építéssel: a lezárás után azonnal más út vagy a legközelebbi pont, nincs keringés |
| **NAV-HIGH** | rámpa és szirt (magaslat és mélység): a szirten nem lehet átmenni, a rámpán igen |
| **NAV-UNREACH** | elérhetetlen cél (szigeten): a válasz egy tickben, a legközelebbi ponttal |
| **NAV-FORM** | formációs vonulás és újraösszeállás szűkület után |

**Kapu:**

- minden próba zöld;
- 8 × 200 egység folyamatos mozgása mellett a tick p99 ≤ 40 ms a fejlesztői gépen;
- golden M4 visszajátszás mind az öt rendszeren.

### 12. Lépések

1. Pálya és terepadat, `map.json`, navrács, szintek, szabadtér-térképek. Tesztek.
2. Hierarchikus réteg: klaszterek, régiók, komponensek, elérhetőség, a legközelebbi elérhető pont, A* + finomítás,
   útsimítás. NAV-DIAG, NAV-UNREACH, NAV-HIGH.
3. Helyi réteg: térbeli rács, kemény és puha sugár, prioritás, félretolás, beragadásfigyelő. NAV-MAMMOTH,
   NAV-CHOKE.
4. Formáció és flow field. NAV-BRIDGE, NAV-FORM.
5. Kapuk, az épület-akadályok frissítése, mérés (8 × 200), golden M4, CI.

## Elvetett lehetőségek

- **Navigációs háló (navmesh):** sima utakat ad, de a dinamikus építés miatt az újraszámolása bonyolult. A
  rácsos hierarchia a 0 A.D.-ben és a régi játékban is bevált.
- **Valódi 3D magasság:** a D32 a StarCraft 1-szerű megoldást választotta.
- **Csak flow field (SC2-szerűen):** kis csoportoknál pazarló, a mozgás minőségét pedig a helyi réteg adja.
- **ORCA elsőként:** lebegőpontos, a fixpontos átirata munkás. Csak mérés után jön.

## Nyitott kérdések (nem akadályozzák a kezdést)

- A magaslat- és mélységszabályok számai (3. pont) – M6–M7, balansz.
- A terepfajták végleges listája – a grafikai készlettel együtt (M5); az M4 az induló készlettel dolgozik.
- A HPA*-cikk számai (klaszterméret) – a mérés dönt (16 × 16 a kiinduló).
