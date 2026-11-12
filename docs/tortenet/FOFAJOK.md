# A főfajok – működés, egységek, épületek, mechanikák

*© Mesehordo – TOTU: Sky Rift. A fájlt a `python tools/gen_tortenet_fajok.py` állítja elő a játék adataiból (a számok mindig a mostani állapotot mutatják). Történetíráshoz: mit tud, hogyan harcol és mire épül minden nép.*

Tartalom: Szürke elfek • Ursanok • Croftianok • Wullof • Aurethi Birodalom • Huk’ucha • Ooni

## Közös szabályok (minden népre)

- **Nyersanyagok:** fa, arany és meteorit. A meteorit ritka: a különleges egységek, a hősök és a legerősebb kutatások ára. Két népnek (Szürke elfek, Huk’ucha) saját hit-nyersanyaga is van (Áhítat, Tóének).
- **Felemelkedés:** amint a főépületen kívül 3 kész épület áll, a nép felemelkedik, és **végleg** választ három fejlődési irány közül. A hadjáratban nincs irányzár.
- **Hősök:** népenként 3 hős, egyszerre legfeljebb 3; szintet lépnek, 4 képességük van (a negyedik a végső), tárgyakat viselnek, elesve a főépületben újraéleszthetők.
- **Égi kegy:** a főépület gombjáról indítható, néphez kötött csapás vagy áldás (Napcsóva, Esküfa-riadó, Mélységi kitörés, Ősök hívása, Ordan villáma, Árhullám, Isteni szél); csak látható pontra.
- **Diplomácia csarnoka:** egy szövetséges nép választható; az ő épületei és egységei a saját bázison épülnek (lásd SZOVETSEGES_NEPEK.md).
- **Égkő:** lerakott meteoritszilánk; a fényében a varázslók erősebbek, a meteoritrokon népek gyógyulnak – mindenkire hat, és bárki lerombolhatja.
- **Erősség és gyengeség:** minden egységtípusnak van erőssége és gyengéje (pl. a lovasság a lövészek ellen erős, a lándzsa ellen gyenge); a sebzés- és páncélkutatásoknak három szintje van (a III. a Felemelkedés után).

**A játékon belüli tippek a játékmenetről (a játék saját megfogalmazásában):**

- A sok egység kevesebb aranyat hoz (Hadtáp).
- A tétlen munkás pénzt veszít: figyeld a jelzőt!
- A meteorit a különleges egységek és kutatások ára.
- Az Égkő-fényben a varázslók erősebbek.
- Az Égkő közelében a pajzsok gyorsabban töltődnek.
- A létszámkorlátnál nem képezhetsz új egységet.
- Minden egységtípusnak van erőssége és gyengéje.
- A lovasság a lövészek ellen erős, a lándzsa ellen gyenge.
- Az ostromgépek az épületek ellen a legjobbak.
- A hős szintet lép, képességet tanul, tárgyat visel.
- Az elesett hős a főépületben újraéleszthető.
- A főépület gombjáról indul az Égi kegy csapása.
- Az Égi kegy csak látható pontra csaphat le.
- A Felemelkedéskor választott irány végleges.
- A Diplomácia csarnokában szövetséges népet választhatsz.
- Szürke elfek: épületeik maguktól épülnek fel.
- Szürke elfek: a Napszentélynél fohászkodó munkás Áhítatot ad.
- Szürke elfek: a Napfúró magától is termel.
- Ursanok: a telepesek riadóra fegyvert ragadnak.
- Ursanok: a helyőrségbe húzódott munkás biztonságban van.
- Croftianok: a mélységen álló fák lassan elrothadnak.
- Croftianok: a szolga a Fertőzött tárnába olvadhat.
- Wullof: a csapolt fa hordás nélkül ad fát.
- Wullof: a tábor felszedhető és továbbvándorolhat.
- Aurethi Birodalom: alapozáson az épület teljes életerővel áll.
- Aurethi Birodalom: alapozás nélkül az épület lassan kopik.
- Aurethi Birodalom: a szobrok bónuszt adnak, amíg állnak.
- Huk’ucha: a saját tavacskán a békák gyorsabbak.
- Huk’ucha: az Ikrarakó csomóiból ebihalak kelnek ki.
- Huk’ucha: a Tóteremtő szentélynél énekelnek a Tóásók.
- Ooni: a lámpásfényben gyógyulnak az egységeid.
- Ooni: a Torii-kapupár között átkelhet a sereg.
- Ooni: a harci kedv elfogyhat – a dobos visszaadja.

## Szürke elfek

*Energiamező • pajzs • önmagától épülő templomok*

A Kő és a Csillag Városai: lépcsős templomvárosok, amelyeket a Kilenc Lépcső Tanácsa fog össze. Kevés, hosszan képzett harcos meteoritpajzzsal; az épületeket a rezonanciaobeliszkek mezejébe idézik meg.

**A történet szempontjából:** Fény, rend, áldozat: egy kicsi, de tökéletes nép, amelynek minden harcosa pótolhatatlan. A hit (Áhítat) szó szerint a munkából fakad – aki imádkozik, nem gyűjt.

### Alapadatok

- **Főépület:** Napszentély  •  **Munkás:** Kőfaragó  •  **Létszám:** Napobeliszkek
- **A varázslatok forrása:** energia
- **Áhítat:** A Napszentélyhez vagy Naptemplomhoz küldött Kőfaragók fohászkodnak (jobb klikk a szentélyre).

### Hogyan működik

- **Energiamező.** Az épületek csak a Napobeliszkek energiamezejében működnek; az obeliszk egyben a létszámot is adja. Ha egy obeliszk elesik, a mezején kívül rekedt épületek leállnak.
- **Maguktól épülő templomok.** A Kőfaragó csak lerakja az alapkövet, a templom magától nő ki a földből, a munkás pedig azonnal továbbmegy.
- **Beidézés.** Az új egységek nem az épületből lépnek ki, hanem a gyülekezőponthoz legközelebbi Napobeliszknél jelennek meg.
- **Töltődő pajzs.** Minden elf egységnek pajzsa van, amely harcon kívül magától visszatöltődik. Kevés, drága, de tartós sereg.
- **Áhítat.** A Napszentélyhez vagy Naptemplomhoz küldött Kőfaragók fohászkodnak, és Áhítatot gyűjtenek. Ebből mennek a Nap csodái (Nap kegyelme, Napáldás, Napkitörés) és az Égi kegy (Napcsóva).
- **Napfúró.** A bányára idézett fúrótorony magától is termel, és legfeljebb 3 Kőfaragó dolgozhat benne, hordás nélkül.
- **Raptorok.** Az alapsereg gerince a Raptorlovas; a Páncélos raptor a nehéz változata.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Nap temploma** – *Papokkal támogatott tömeghadsereg*. Minden élő egységed folyamatosan gyógyul (+1 regeneráció). Főpapok gyógyítanak, napszóró papok égetnek, és a quetzalok az égből csapnak le. (épület: Naptemplom; egység: Nap főpapja, Napszóró pap, Quetzal)
- **Jaguár-harcosok** – *Vadászat, lesből támadás*. Az erdőben a gyalogosaid láthatatlanok, a közelharcosok +2 sebzést kapnak. Jaguár-orvlövészek, páncélos raptorok és tollas rexek. (épület: Raptorkarám; egység: Jaguár-orvlövész, Páncélos raptor, Tollaskoronás rex)
- **Csillagvizsgálók** – *Távolsági ostrom, lassú őrlés*. +1 lőtáv minden távolsági egységnek. Obszidián-vetők és csillagjósok meteorokat hívnak messziről. (épület: Csillagvizsgáló; egység: Obszidián-vető, Csillagjós)

### Hősök

#### Szelai

Rezonanciaépítész, akit a sors parancsnokká tett. Hálózatot épít és bont; Napvihart idéz.

- **Napvihar** – Izzó meteorpor-zápor.
- **Rezonancia** – A közeli szövetségesek sebzése nő.
- **Leválasztás** – Rezonanciacsapás, amely egy célpontot kiszakít a hálózatából.
- **A Kilenc Lépcső őrei** – Végső képesség: négy tollas harcos-szellem.

#### Itzcoatl

A Kilenc Lépcső tollas hadura. Közelharcos, a sereg élén áll.

- **Obszidián-dárda** – Hajított obszidián-dárda: nagy sebzés, lassítás.
- **Földrengető csapás** – A hadúr a földbe sújt: a körülötte állókat megsebzi és megbénítja.
- **A hadúr jelenléte** – A közeli szövetségesek páncélja és sebzése nő.
- **Quetzlaru alakja** – Végső képesség: a tollas kígyóisten ereje – kétszeres sebzés, +6 páncél.

#### Xochi

A rezonancia mestere: jádeőröket ébreszt és gyógyít.

- **Rezonanciahullám** – Jádeszínű rezgés tépi szét a területet.
- **Harmónia** – A közeli szövetségesek életereje és pajzsa gyorsabban töltődik.
- **Jádeőr** – Élő jádeszobrot ébreszt, amely a hívójáért harcol.
- **Napkorona** – Végső képesség: a területen minden szövetséges gyógyul.

### Egységek (14)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Kőfaragó | munkás | 30 fa, 20 arany | 30 / 0 | 5 sebzés, közel | Munkás. Megidézi az épületeket, majd visszatér gyűjteni. |
| Tollas harcos | közelharcos | 45 fa, 40 arany | 122 / 1 | 13 sebzés, közel | Obszidiánpengés buzogánnyal harcoló elit gyalogos. |
| Jáde-íjász | lövész | 65 fa, 30 arany | 70 / 0 | 11 sebzés, 5 lőtáv | Távolsági egység, földi és légi célpontokra is lő. |
| Raptorlovas | lovas / hátas | 65 fa, 60 arany | 130 / 1 | 14 sebzés, közel | Farkatlan, tollas gyíkon lovagló gyors lovas. Bónusz a távolsági egységek ellen. |
| Napszóró pap | varázsló | 70 fa, 60 meteorit, 20 arany | 50 / 0 | 6 sebzés, 4 lőtáv | Meteoritmágus. Napvihart idéz az ellenségre. |
| Quetzal | repülő, repül | 85 meteorit, 85 fa, 45 arany | 160 / 1 | 16 sebzés, 4 lőtáv | Tollas, repülő dínó. Mindenre lecsap. |
| Tollaskoronás rex | óriás | 165 arany, 120 meteorit, 145 fa | 420 / 2 | 40 sebzés, közel | Díszes tollkoronás óriásdínó, hátán íjászokkal. Területi harapás. |
| Csillagdárdás | lándzsás | 75 fa, 15 meteorit, 25 arany | 85 / 0 | 13 sebzés, 6 lőtáv | Csillagérc-hegyű dárdákat hajít – messzire hord, légi célpontokra is. |
| Obszidián-vető | ostromgép | 200 fa, 120 arany | 260 / 1 | 34 sebzés, 8 lőtáv | Kőfaragók ostromgépe: obszidiántömböket hajít, épületek ellen különösen erős. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), mozgás előtt 1 mp-ig csukódik össze. |
| Jádeőr | óriás | – | 320 / 3 | 18 sebzés, közel | Életre keltett jádeszobor (idézett). |
| Nap főpapja | varázsló | 80 fa, 40 arany, 60 meteorit | 120 / 0 | 8 sebzés, 5 lőtáv | A Nap főpapja: folyamatosan gyógyítja a körülötte harcolókat. |
| Jaguár-orvlövész | lövész | 90 fa, 30 arany | 100 / 0 | 14 sebzés, 6 lőtáv | Jaguárbőrös vadász: az erdőből lő, lesállásban halálos. |
| Csillagjós | varázsló | 90 fa, 30 arany, 90 meteorit | 110 / 0 | 9 sebzés, 6 lőtáv | A csillagvizsgálók mestere: meteorzáport hív le messziről. |
| Páncélos raptor | lovas / hátas | 110 fa, 55 arany | 210 / 3 | 17 sebzés, közel | Obszidiánlemezes harci raptor jaguár-harcossal: lassabb, de jóval tartósabb a Raptorlovasnál. Bónusz a távolsági egységek ellen. |

**Az egységek képességei**

- **Fénylépés** – Menekülő teleport (5 mező).
- **Fénylépés** – Fénnyé válik, és a kijelölt pontra lép (7 mező).
- **Napfolt** – Napfénybe öltözik: 6 mp-ig semmilyen egységtípus ellen nem gyenge.
- **Jáde-célzás** – 6 mp-ig +2 lőtáv és +20% sebzés.
- **Vágta** – A gyíkhátas 4 mp-ig 80%-kal gyorsabb.
- **Napvihar** – Izzó meteorpor-zápor: 4 mp alatt 88 sebzés minden ellenségnek a területen.
- **Napkitörés** – A Nap haragja: erősebb Napvihar (másfélszeres sebzés, másfélszeres terület). Ára 50 energia és 40 Áhítat.
- **Zuhanórepülés** – Lecsap a földre: a közeli ellenségek sebződnek és megtántorodnak.
- **Tollkorona-üvöltés** – Rémisztő üvöltés: a közeli ellenségek lelassulnak.
- **Csillagdárda** – Célzott, csillagérc-hegyű dárda: 45 sebzés és lassítás.
- **Napfókusz** – Telepítés: nem mozog, de +3 lőtáv és +20% sebzés.
- **Jádetest** – A varázslatok csak fele sebzést okoznak neki.
- **Napfény-áldás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket.
- **Nap kegyelme** – 6 csempés körben azonnal 80 életerőt ad minden saját élőlénynek. Ára 30 Áhítat, 45 mp töltődés.
- **Les** – Lesállás: nem mozog, de +50% sebzés és +1 lőtáv.
- **Meteorhívás** – Meteorzápor: 3 mp alatt 105 sebzés a területen.

### Épületek (13)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Napszentély | 400 arany, 100 fa | Kőfaragó | Lépcsős piramis, főépület. Leadóhely, Kőfaragókat képez. csoda: Napcsóva (Izzó fényoszlop a kijelölt pontra: 8 mp alatt 200 sebzés a körzetben, és 10 mp-re felfedi a környéket. Ára 150 Áhítat, 3 perc töltődés.). |
| Napobeliszk | 55 fa, 25 arany | – | +8 létszám. Energiamezőt sugároz – az elf épületek csak ebben működnek. |
| Harcosok csarnoka | 125 fa, 75 arany | Tollas harcos, Jáde-íjász, Raptorlovas | Tollas harcosokat és jáde-íjászokat képez. |
| Obszidián műhely | 100 fa, 100 arany | Csillagdárdás | Fegyver- és pajzsfejlesztések. |
| Raptorkarám | 180 fa, 120 arany | Páncélos raptor, Tollaskoronás rex | Raptorlovasokat és tollaskoronás rexeket nevel. |
| Csillagvizsgáló | 150 arany, 50 meteorit | Obszidián-vető, Csillagjós | Napszóró papokat képez, a csillagok tudását kutatja. |
| Tollfészek-piramis | 150 fa, 150 arany, 100 meteorit | Quetzal | Quetzalokat nevel. |
| Jádeoszlop | 125 arany, 25 fa | – | Védőtorony, földi és légi célpontokra. |
| Napkapu | 180 fa, 100 arany | Szelai, Itzcoatl, Xochi | Hősidéző templomkapu: Szelai, Itzcoatl és Xochi. |
| Naptemplom | 160 fa, 160 arany, 40 meteorit | Nap főpapja, Napszóró pap | Aranyozott lépcsős templom: a Nap főpapjai és napszóró papok. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |
| Napfúró | 120 fa, 40 arany | – | A bányára (arany vagy meteorit) idézett fúrótorony: magától is termel, és legfeljebb 3 Kőfaragó dolgozhat benne, hordás nélkül (jobb klikk a Napfúróra). Ha lerombolják, a bent lévők sérülten kiugranak. csoda: Napáldás (A Napfúró 30 mp-re 50%-kal többet termel. Ára 25 Áhítat, 60 mp töltődés.). |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |

### Kutatások (16)

| Kutatás | Hol | Hatás |
|---|---|---|
| Arany fohász | Napszentély | A fohászkodó Kőfaragók 0,4 Áhítatot adnak másodpercenként (0,3 helyett). |
| Rezonancia | Napszentély | A Napobeliszkek energiamezeje 2 csempével nagyobb. |
| Napfolt | Harcosok csarnoka | A Tollas harcosok megkapják a Napfolt képességet: 6 mp-ig semmilyen egységtípus ellen nem gyengék. |
| Obszidián pengék | Obszidián műhely | +2 sebzés a közelharci egységeknek. |
| Obszidián pengék II. | Obszidián műhely | +2 sebzés a közelharci egységeknek. (II. szint) |
| Obszidián pengék III. | Obszidián műhely | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Jádehegyű nyilak | Obszidián műhely | +2 sebzés a távolsági egységeknek. |
| Jádehegyű nyilak II. | Obszidián műhely | +2 sebzés a távolsági egységeknek. (II. szint) |
| Jádehegyű nyilak III. | Obszidián műhely | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Napégi pajzs | Obszidián műhely | +30 pajzs minden elf egységnek. |
| Obszidián vértezet II. | Obszidián műhely | +1 páncél minden élő elf egységnek. (II. szint) |
| Obszidián vértezet III. | Obszidián műhely | +1 páncél minden élő elf egységnek. (III. szint; a Felemelkedés után) |
| Obszidián vértezet | Obszidián műhely | +1 páncél minden élő elf egységnek. |
| Csillagfény-látás | Csillagvizsgáló | +1 lőtáv a távolsági egységeknek, +1 látótáv mindenkinek. |
| Napszemű papok | Naptemplom | A Napszóró papok energiája 50%-kal gyorsabban töltődik. |
| Égi kegy | Naptemplom | A Nap megáldja a népet: minden egység +10% életerőt kap. |

## Ursanok

*Helyőrség • tanyák • javítás • pajzsfal*

Az Északi Eskük Birodalma: esküvel összekötött jarlságok, jurtákkal kevert hosszúcsarnokok, árpaföldes tanyák. Sisakot nem hordanak, legfeljebb ágyékkötőt; nagy, lapos deszkapajzzsal harcolnak. A csarnokok és tornyok helyőrséget fogadnak (riadókor a telepesek is behúzódnak), a létszámot a tanyák adják. Pajzsfal és jégkorszaki harci vadak.

**A történet szempontjából:** Eskü, közösség, szívósság: a falu maga a hadsereg. A telepes és a harcos ugyanaz a medve – riadóra mindenki fegyvert ragad.

### Alapadatok

- **Főépület:** Nagyterem  •  **Munkás:** Telepes  •  **Létszám:** Tanyák
- **A varázslatok forrása:** töltődési idő (nincs mana)

### Hogyan működik

- **Helyőrség.** A Nagyterem, a Hosszúház, a Harcosok háza és az Őrtorony egységeket fogad be: a bent lévők védve vannak, és fejenként erősítik az épület lövéseit. Riadóra a telepesek is behúzódnak.
- **Telepesek.** A Telepes épít (építés közben bemászik a félkész épületbe), gyűjt és javít; a Népfelkelés kutatással és a Fegyverbe! riadóval harcossá válik. Az Esküfa-riadó (Égi kegy) az egész falut felfegyverzi 30 másodpercre.
- **Tanyák.** A létszámot a tanyák adják; a Kiadós tanya kutatás bővíti.
- **Pajzsfal és formációk.** Lassú, kemény gyalogság: Pajzsfal, Dárdafal, rúnakövek, cölöpfal. A varázslatok nem manából, hanem töltődési időből mennek.
- **Az ursanok.** Medveemberek: nem viselnek sisakot, nagy, lapos deszkapajzsuk és viking–jurta stílusú épületeik vannak.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Berserker-klán** – *Korai, nyers közelharci nyomás*. Vérfogadalom: a közelharcosok annál erősebbek, minél sebesültebbek (+60% sebzésig). Berserkerek, medvévé változó alakváltók és harci kürtösök. (épület: Vérfogadalom-csarnok; egység: Medvebőrös berserker, Medvealakváltó, Harci kürtös)
- **Rúnakovácsok** – *Erőd, technológia, késői erő*. Rúnákkal vésett falak és tornyok: +3 páncél minden épületnek, +6 toronysebzés. Rúnaóriások és ostromszánok törik át a frontot, amikor már minden megvan. (épület: Rúnaszentély; egység: Rúnaóriás, Ostromszán)
- **Tundra hívása** – *Nagytestű vadak, kevés, de hatalmas egység*. Mamutok, kardfogú lovasok és viharhollók. A vadállatok és a lovasság +60 életerőt és gyorsabb regenerációt kapnak. Lassú, nehéz ostrom. (épület: Vadak karáma; egység: Kardfogú lovas, Harci mammut, Viharholló)

### Hősök

#### Ragna Kőváll

Egy határerőd parancsnoka. A pajzsa először a mellette állót védi.

- **Pajzsfal** – A pajzs először a mellette állót védi: kevesebb távolsági sebzés, több páncél.
- **Az eskü súlya** – A közeli szövetségesek páncélja nő.
- **Pajzsroham** – Ragna nekiront egy ellenségnek: sebzi és rövid időre megbénítja.
- **Hosszú tél** – Végső képesség: fagyos vihar, amely lassít és sebez.

#### Hjalmar Fagyjós

A jégkorszak emlékét őrző jós: fagy, vihar és lassítás.

- **Jégszilánk** – Fagyos szilánk: sebez és erősen lassít.
- **Hóvihar** – Jeges vihar csap le a területre.
- **Téli bundák** – A közeli szövetségesek páncélja és regenerációja nő.
- **Megfagyott idő** – Végső képesség: minden közeli ellenség jégbe dermed.

#### Bjarka, a Vadak Ura

Vadász és vadidomár: kardfogú tigriseket és mamutokat hív harcba.

- **Kardfogú hívása** – Kardfogú tigrist hív a hegyekből.
- **A falka ereje** – A közeli szövetségesek (és vadak) sebzése nő.
- **Vadászszigony** – Szigonyt vet egy ellenségbe: sebzi és rövid időre megbénítja.
- **Mamutcsorda** – Végső képesség: két harci mamut csatlakozik 40 másodpercre.

### Egységek (15)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Telepes | munkás | 30 fa, 20 arany | 55 / 0 | 6 sebzés, közel | Munkás. Építéskor bemászik a félkész épületbe (bent védett, készen jön ki), javít, gyűjt. |
| Pajzshordozó | lándzsás | 55 fa, 15 arany | 157 / 2 | 10 sebzés, közel | Az ursan sereg gerince. Pajzsfal: a távolsági sebzés nagy részét elnyeli. |
| Szigonyvető | lándzsás | 75 fa, 15 arany | 90 / 1 | 14 sebzés, 5.5 lőtáv | Távolsági, bónusz a nagy testű célpontok ellen; légi célpontra is. |
| Medvebőrös berserker | közelharcos | 75 fa, 25 meteorit, 25 arany | 150 / 1 | 16 sebzés, közel | Gyors, nagy sebzésű közelharcos. |
| Rúnapap | varázsló | 55 fa, 50 meteorit, 20 arany | 80 / 0 | – | Gyógyító. Automatikusan foltozza a sebesült harcosokat. |
| Kardfogú lovas | lovas / hátas | 130 fa, 45 arany | 210 / 2 | 16 sebzés, közel | Kardfogú tigrisen lovagló gyors harcos. Roham képesség. |
| Viharholló | repülő, repül | 115 fa, 50 meteorit, 35 arany | 110 / 0 | 9 sebzés, 3.5 lőtáv | Óriás holló. Felderítő és légvédelem. |
| Harci mammut | óriás | 345 fa, 155 arany, 75 meteorit | 650 / 3 | 30 sebzés, 4.5 lőtáv | Gyapjas mamut, hátán kőhajítókkal. Terület-sebzés, bónusz épületek ellen. |
| Dárdaőr | lándzsás | 65 fa, 15 arany | 145 / 2 | 12 sebzés, 1.0 lőtáv | Hosszú dárdás védő – lovasság és nagy vadak ellen kiváló. |
| Ostromszán | ostromgép | 215 fa, 110 arany | 300 / 2 | 40 sebzés, 8 lőtáv | Szánra szerelt kőhajító, épületek ellen. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), mozgás előtt 1 mp-ig csukódik össze. |
| Kardfogú tigris | közelharcos | – | 260 / 1 | 16 sebzés, közel | Bjarka hívására érkező vadállat (idézett). |
| Rúnakő | óriás | – | 220 / 4 | – | Rúnapap által állított kő (idézett). Aurája véd és gyógyít. |
| Medvealakváltó | óriás | 150 fa, 60 arany, 40 meteorit | 520 / 2 | 24 sebzés, közel | Az ursan teljesen medvévé változik: hatalmas karmok, rettentő üvöltés. |
| Harci kürtös | közelharcos | 80 fa, 40 arany | 160 / 1 | 9 sebzés, közel | Kürtszava felszítja a harcosokat: +15% sebzés, +10% sebesség a közelben. |
| Rúnaóriás | óriás | 220 arany, 80 meteorit | 760 / 5 | 30 sebzés, közel | Rúnákkal életre keltett kőóriás: lassú, de épületeket zúz. |

**Az egységek képességei**

- **Helyőrség** – A kijelölt egységek beszállnak a legközelebbi helyőrség-épületbe; ott védve vannak, és minden beszállt egység erősíti az épület lövéseit.
- **Fegyverbe!** – Riadó: a telepesek 45 mp-re fegyvert ragadnak (erősebbek, gyorsabbak, páncélosabbak), de közben nem dolgoznak; utána visszamennek a munkájukhoz.
- **Pajzsfal** – Összezárt pajzsok: -60% távolsági sebzés, +2 páncél, de fele sebesség.
- **Horgonyszigony** – Szigony egy ellenségbe: sebez és 1,2 mp-re megbénít.
- **Vérszomj** – 7 mp-ig +50% sebzés, +20% sebesség, de -2 páncél.
- **Rúnás gyógyítás** – Automatikusan gyógyítja a közeli sérült élő szövetségeseket.
- **Rúnakő** – Rúnakövet állít: a közeli szövetségesek páncélja és gyógyulása nő.
- **Roham** – 4 mp-ig +70% sebesség és +30% sebzés.
- **Zuhanó csapás** – A holló lecsap egy célpontra.
- **Taposás** – A mamut a földbe dobbant: sebez és megbénít.
- **Dárdafal** – Leszúrt dárdák: nem mozog, de +60% sebzés és +2 páncél.
- **Telepítés** – A szán lehorgonyoz: nem mozog, de +3 lőtáv és +25% sebzés.
- **Szellemtest** – Idézett lény; idővel szertefoszlik.
- **Rúnakő** – A közeli szövetségesek páncélja és gyógyulása nő.
- **Medveüvöltés** – Rettentő üvöltés: sebez és megbénítja a közeli ellenséget.
- **Harci kürt** – Kürtszó (egyszerre egy): a közeli szövetségesek +15% sebzés és +10% sebesség.

### Épületek (15)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Nagyterem | 300 fa, 150 arany | Telepes | Faragott hosszúcsarnok, főépület. Leadóhely, Telepeseket képez. helyőrség: 6 fő; csoda: Esküfa-riadó (Megkondul az Esküfa: a falu minden telepese 30 mp-re fegyvert ragad (a Fegyverbe! hatása, kutatás nélkül is), utána visszamegy dolgozni. 4 perc töltődés.). |
| Hosszúház | 100 fa | – | Erődített hosszúház: 4 fős helyőrség, riadóban a telepesek menedéke. helyőrség: 4 fő. |
| Fatelep | 75 fa | – | Leadóhely fának, aranynak és meteoritnak. |
| Harcosok háza | 150 fa, 50 arany | Pajzshordozó, Dárdaőr, Szigonyvető | Pajzshordozókat, szigonyvetőket és berserkereket képez. helyőrség: 4 fő. |
| Rúnakovács | 150 fa, 150 arany | Rúnapap | Rúnapapokat képez, fegyvert és páncélt fejleszt. |
| Vadak karáma | 265 fa, 135 arany | Kardfogú lovas, Harci mammut | Kardfogú lovasokat és harci mamutokat nevel. |
| Sasszirt | 150 fa, 100 arany, 50 meteorit | Viharholló | Viharhollókat nevel. |
| Őrtorony | 100 fa, 75 arany | – | Fa-kő őrtorony íjászokkal. helyőrség: 4 fő. |
| Esküfa | 180 fa, 100 arany | Ragna Kőváll, Hjalmar Fagyjós, Bjarka, a Vadak Ura | Ősi kőris, amely alatt a jarlok esküt tesznek: Ragna, Hjalmar és Bjarka. |
| Tanya | 95 fa, 20 arany | – | Árpaföld, ólak és szénapajták: a jarlság népe innen él. +10 létszám. |
| Cölöpfal | 20 fa | – | Hegyes karókból álló falszakasz – elzárja az utat. Olcsó, gyorsan épül; a telepesek javítják. |
| Vérfogadalom-csarnok | 220 fa, 80 arany | Medvebőrös berserker, Medvealakváltó, Harci kürtös | Itt teszik le a berserkerek a vérfogadalmat. |
| Rúnaszentély | 240 arany, 60 meteorit | Rúnaóriás, Ostromszán | Rúnakőkör kovácsműhellyel: rúnaóriások és ostromszánok. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |

### Kutatások (16)

| Kutatás | Hol | Hatás |
|---|---|---|
| Népfelkelés | Nagyterem | A telepesek harcosként is megállják a helyüket: +5 sebzés, +40 életerő, +1 páncél; és megnyílik a „Fegyverbe!” riadó. |
| Kiadós tanya | Nagyterem | A Tanyák +4 létszámot adnak (8 helyett 12). |
| Lőréses falak | Nagyterem | A helyőrségbe húzódott egységek erősebben lőnek ki (+7 sebzés fejenként a +4 helyett). |
| Rúnás pengék | Rúnakovács | +2 sebzés a közelharci egységeknek. |
| Rúnás pengék II. | Rúnakovács | +2 sebzés a közelharci egységeknek. (II. szint) |
| Rúnás pengék III. | Rúnakovács | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Vashegyű szigonyok | Rúnakovács | +2 sebzés a távolsági egységeknek. |
| Vashegyű szigonyok II. | Rúnakovács | +2 sebzés a távolsági egységeknek. (II. szint) |
| Vashegyű szigonyok III. | Rúnakovács | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Medvebunda-páncél | Rúnakovács | +1 páncél minden élő egységnek. |
| Medvebunda-páncél II. | Rúnakovács | +1 páncél minden élő egységnek. (II. szint) |
| Medvebunda-páncél III. | Rúnakovács | +1 páncél minden élő egységnek. (III. szint; a Felemelkedés után) |
| Télegi edzés | Rúnakovács | +25 életerő a gyalogságnak és a lovasságnak. |
| Rúnapajzs | Rúnakovács | Rúnák az épületeken: minden épület +2 páncélt kap, és lassan magától javul (+1 életerő/mp). |
| Tüskés pajzsfal | Rúnakovács | A Pajzshordozók nem gyengék a közelharcosok és az ostromgépek ellen. |
| Medve-ösztön | Esküfa | Vérszag: a 40% életerő alá sebesült ursan egységek 20%-kal nagyobbat sebeznek. |

## Croftianok

*Mélység • lárvák • mutáció • fertőzés*

Az Idegen Kórus: a meteorokkal érkezett spórák megszállták a hangyaszerű kethri népet. A Kaptár lárváiból minden egység párhuzamosan kel ki, a Karmosok gubóban páncélos bogárrá mutálódnak, és a mélységen elesett élő ellenségből új Karmos kelhet ki.

**A történet szempontjából:** Éhség és terjeszkedés: a raj megemészti a tájat, a fák elrothadnak alatta. A munkás szó szerint feloldódik a bányában.

### Alapadatok

- **Főépület:** Kaptár  •  **Munkás:** Csápszolga  •  **Létszám:** Szemhordozók (egységek)
- **A varázslatok forrása:** saját életerő

### Hogyan működik

- **Mélység.** Az épületek a mélységet (élő talajt) terítik maguk köré, és csak azon építhetők. A mélységen a croftianok gyógyulnak és gyorsabbak; a rajta álló fák lassan elrothadnak, és ebből fa folyik a népnek (legfeljebb 6,5 fa/mp, kutatással 9).
- **Lárvák.** Minden egység a Kaptár lárváiból kel ki, egyszerre többféle is.
- **Csápszolga.** A munkás nem hord nyersanyagot: beleolvad a Fertőzött tárnába, és onnan termel. Mérgező tüskéket is szúr a földbe (lassítanak és mérgeznek minden nem croftian egységet).
- **Beásás és lesből támadás.** A beásott egységek láthatatlanok és gyorsan gyógyulnak; csak felfedő (Szemhordozó, torony) vagy a nagyon közeli ellenség látja őket.
- **Létszám élő egységekből.** A létszámot a Szemhordozók adják – ha meghalnak, a raj is zsugorodik.
- **Mutáció és fertőzés.** A Karmos gubóban Páncélbogárrá mutálódik; a mélységen elesett élő ellenségből Karmos kelhet ki. A varázslatok a saját életerőből mennek.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Raj** – *Olcsó tömeg, korai elárasztás*. A lárvák 30%-kal gyorsabban kelnek ki, a rajlények gyorsabbak. Robbanó atkák és vadászkarmosok öntik el az ellenfelet, mielőtt felkészülne. (épület: Rajkamra; egység: Robbanó atka, Vadászkarmos)
- **Páncél** – *Lassú, szívós menetelés*. +2 páncél minden élő egységnek. A Karmosok páncélbogárrá alakulhatnak; ostromtaréjak és alagutat ásó férgek törik fel a védelmet. (épület: Kitinkamra; egység: Páncélbogár, Ostromtaréj, Ásóféreg)
- **Kórus** – *Irányítás és átállítás*. A Kórus hangja: a megölt ellenséges egységek 15%-a a te oldaladon kel fel. Agyfalók és csápos horrorok bomlasztják az ellenséges sereget. (épület: Agyszentély; egység: Agyfaló, Csápos horror)

### Hősök

#### Keth

Egy megszállt kethri város emlékőre; a Kórus parancsnoki csomópontja – de régi önmaga nem tűnt el.

- **Őrület köde** – Elmét bomlasztó spórafelhő.
- **Kórus-hang** – A közeli szövetségesek gyorsabbak és regenerálódnak.
- **Csápcsapás** – Savas csapás egy célpontra.
- **A sokaság** – Végső képesség: hat Karmos kel ki.

#### Vor-Uthul

A Mutáció Mestere: a saját testéből költ lárvákat, kitinpáncélt növeszt.

- **Savsugár** – Maró savsugár.
- **Mutáns páncél** – Kitinlemezek nőnek a mester testén.
- **Lárvahullám** – Karmosokat költ ki a saját testéből.
- **Kitinrobbanás** – Végső képesség: kitinszilánkok repesznek szét a mester körül.

#### Ssarha, a Raj Hangja

A Kórus hangja: a rajt egy akaratba fogja, és összeölti a sebeket.

- **Spórafelhő** – Fojtó spórafelhő.
- **Rajtudat** – A raj egy akarattal mozog: gyorsabb és erősebb.
- **Hús-öltés** – Automatikusan összefoltozza a közeli sérült élő szövetségeseket.
- **Rémszárnyak** – Végső képesség: négy szárnyas rém érkezik.

### Egységek (14)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Csápszolga | munkás | 30 fa, 20 arany | 40 / 0 | 5 sebzés, közel | Munkás. Épületté alakul át, vagy beleolvad egy Fertőzött tárnába (elfogy). Nem gyűjt: a fát a mélység rothasztja el, az aranyat és a meteoritot a tárnák adják. |
| Karmos | közelharcos | 30 fa, 20 arany | 60 / 0 | 8 sebzés, közel | Olcsó, gyors rajegység. Egy lárvából kettő kel ki. |
| Tüskeköpő | lövész | 55 fa, 25 arany | 95 / 0 | 14 sebzés, 5 lőtáv | Csonttüskéket köp, földi és légi célpontokra. |
| Savköpő | ostromgép | 150 fa, 50 meteorit, 50 arany | 150 / 1 | 25 sebzés, 7 lőtáv | Élő tüzérség. Messzire köpött savhólyagok, terület-sebzés, bónusz épületek ellen. |
| Agyfaló | varázsló | 80 fa, 75 meteorit, 20 arany | 70 / 0 | 5 sebzés, 4 lőtáv | Pszionikus rém. Őrület ködét bocsátja az ellenségre. |
| Szárnyas rém | repülő, repül | 140 fa, 50 meteorit, 35 arany | 120 / 0 | 10 sebzés, 3 lőtáv | Hártyás szárnyú repülő ragadozó. |
| Csápos horror | óriás | 190 arany, 100 meteorit, 160 fa | 520 / 2 | 30 sebzés, közel | A mélység óriása. Csápjaival egyszerre több ellenfelet zúz. |
| Páncélbogár | óriás | 75 arany, 25 meteorit, 50 fa | 340 / 4 | 18 sebzés, közel | Karmosból mutálódott, páncélos rohamállat. Csak mutációval jön létre. |
| Gubó | közelharcos | – | 200 / 6 | – | Mutálódó egység – sérülékeny, de páncélos burok. |
| Szemhordozó | repülő, repül | 60 fa, 40 arany | 160 / 1 | – | Lebegő szemzsák: messzire lát, és a Kórus létszámát hordozza (+8). |
| Robbanó atka | közelharcos | 30 fa, 15 arany | 40 / 0 | 4 sebzés, közel | Olcsó, gyors atka: az ellenség közé rohan és felrobban. |
| Vadászkarmos | közelharcos | 45 fa, 25 arany | 100 / 0 | 9 sebzés, közel | Fürge karmos-változat: gyorsan üldöz és szaggat. |
| Ostromtaréj | ostromgép | 120 fa, 120 arany, 40 meteorit | 360 / 3 | 30 sebzés, 8 lőtáv | Élő ostromgép: messzire köpi a maró kitintüskéket, épületekre különösen. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), mozgás előtt 1 mp-ig csukódik össze. |
| Ásóféreg | közelharcos | 90 fa, 70 arany | 260 / 2 | 16 sebzés, közel | A föld alatt közlekedő féreg: beásva rejtőzik, és alulról támad. |

**Az egységek képességei**

- **Beásás** – A föld alá ássa magát: láthatatlan (csak felfedő találja meg), gyorsan gyógyul, de nem támad.
- **Mutáció: Páncélbogár** – A Karmos gubóba zárkózik, és páncélos bogárrá alakul.
- **Tüskezápor** – Csonttüskék záporoznak a területre. Ára: 20 életerő.
- **Savtócsa** – Maró savtócsa: sebez és lassít. Ára: 30 életerő.
- **Őrület köde** – Elmét bomlasztó spórafelhő: 50% lassítás és folyamatos sebzés 6 mp-ig.
- **Ragadozó zuhanás** – Lecsap egy célpontra. Ára: 15 életerő.
- **Csápsöprés** – Körbecsapó csápok: nagy sebzés és lassítás. Ára: 40 életerő.
- **Kitinroham** – 5 mp-es roham: +60% sebesség, +30% sebzés. Ára: 20 életerő.
- **Gubó** – Mutálódik; ha elpusztul, az átalakulás elvész.
- **A Kórus szeme** – Felfedi a közeli rejtőzködő és beásott ellenségeket.
- **Önrobbantás** – Felrobban: nagy sebzés a közelben – az atka elpusztul.

### Épületek (16)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Kaptár | 205 arany, 195 fa | Csápszolga, Szemhordozó, Karmos, Tüskeköpő, Savköpő, Szárnyas rém, Agyfaló, Csápos horror, Robbanó atka, Vadászkarmos, Ostromtaréj, Ásóféreg | Élő főépület. Lárvákat nevel (max. 3), minden egység innen kel ki – párhuzamosan. csoda: Mélységi kitörés (Tüskék törnek elő a mélyből a kijelölt ponton: 3 mp alatt 120 sebzés és lassítás. Ára a Kaptár 250 életereje, 3 perc töltődés.). |
| Tápgomba | 65 fa, 20 arany | – | Tápláló gombatelep: messzire terjeszti a mélységet, és gyógyítja a rajt. |
| Emésztőkamra | 50 fa, 25 arany | – | Emésztőkamra: a mélység lassú bendője. Itt fejleszthető a lárvázás, a rothasztás, a méreg és a gyógyulás; nyersanyag leadására is jó. |
| Ikrafészek | 140 fa, 40 arany | – | Lehetővé teszi a Karmosokat. |
| Tüskeverem | 165 fa, 85 arany | – | Lehetővé teszi a Tüskeköpőket és a Savköpőket. |
| Agyszentély | 135 arany, 75 meteorit, 65 fa | – | Lehetővé teszi az Agyfalót és a Csápos horrort. |
| Csontcsúcs | 150 arany, 100 meteorit, 100 fa | – | Lehetővé teszi a Szárnyas rémeket. |
| Tüskés gyökér | 100 fa, 25 arany | – | Védőépület, földi és légi célpontokra. |
| Kethri tojáskamra | – | – | Fertőzetlen kethri tojások – a jövő nemzedéke. Védd meg! |
| Kórus-kút | 150 arany, 100 fa | Keth, Vor-Uthul, Ssarha, a Raj Hangja | Lüktető hangkút: innen szólnak a Kórus hősei – Keth, Vor-Uthul és Ssarha. |
| Rajkamra | 150 fa, 80 arany | – | Lüktető keltetőkamra: robbanó atkák és vadászkarmosok (a Kaptárban kelnek ki). |
| Kitinkamra | 160 fa, 140 arany | – | Páncélnövesztő kamra: páncélbogarak, ostromtaréjak, ásóférgek. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |
| Fertőzött tárna | 75 fa | – | A bányára (arany vagy meteorit) növesztett tárna. A Csápszolgák beleolvadnak (jobb klikk a tárnára, legfeljebb 5): mindegyik végleg egy munkásnyi hozamot ad, hordás nélkül. Ha a tárna elpusztul vagy a bánya kimerül, a beolvasztott szolgák is elvesznek. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |
| Mérgező tüskék | 20 fa, 10 arany | – | A Csápszolga a földbe szúrja (2 mp, nem olvad bele). Át lehet rajta menni, de minden nem croftian egységet lassít (–40%) és mérgez (4 sebzés/mp, a szövetségeseket is). Legfeljebb 16 mező. A Tüskemező kutatással 2×2-es. |

### Kutatások (16)

| Kutatás | Hol | Hatás |
|---|---|---|
| Éles karmok II. | Emésztőkamra | +2 sebzés a közelharci egységeknek. (II. szint) |
| Éles karmok III. | Emésztőkamra | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Éles karmok | Emésztőkamra | +2 sebzés a közelharci egységeknek. |
| Gyors lárvázás | Emésztőkamra | A lárvák 25%-kal gyorsabban kelnek, és a Kaptár +1 lárvát tarthat. |
| Mélyebb rothasztás | Emésztőkamra | A mélységen álló fák 50%-kal gyorsabban rothadnak, és a rothasztás felső határa 6,5-ről 9 fa/mp-re nő. |
| Bénító méreg | Emésztőkamra | A Mérgező tüskék kétszer erősebben mérgeznek és lassítanak, a távolsági egységek lövései pedig mérgeznek (4 sebzés/mp, 3 mp). |
| Mélységi gyógyulás | Emésztőkamra | A saját mélységen álló egységek kétszer gyorsabban gyógyulnak. |
| Inas lábak | Ikrafészek | +30% sebesség a Karmosoknak. |
| Savas tüskék | Tüskeverem | +2 sebzés a távolsági egységeknek. |
| Savas tüskék II. | Tüskeverem | +2 sebzés a távolsági egységeknek. (II. szint) |
| Savas tüskék III. | Tüskeverem | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Tüskemező | Tüskeverem | A Mérgező tüskék 2×2-es mezőre kerülnek (ugyanazért az árért), és legfeljebb 24 mező rakható. |
| Kitinpáncél | Agyszentély | +1 páncél minden egységnek. |
| Kitinpáncél II. | Agyszentély | +1 páncél minden egységnek. (II. szint) |
| Kitinpáncél III. | Agyszentély | +1 páncél minden egységnek. (III. szint; a Felemelkedés után) |
| Életnedv | Agyszentély | +1 életerő-regeneráció másodpercenként. |

## Wullof

*Nomád táborok • falkaerő • vadászzsákmány*

A Hét Út Szövetsége: dingónép folyóvárosokkal, vándorló közösségekkel és útének-dalokkal a vörös pusztán és az eukaliptuszerdőkben. A táboraik felszedhetők és máshol letelepíthetők; a létszámot az emucsordák adják, falkában erősebbek, és minden zsákmány fát hoz a törzsnek.

**Háttér (FACTIONS_LORE.md):**

A **Hét Út Szövetsége**: antropomorf dingók, ausztrál hangulatú, de önálló, fiktív társadalom a vörös pusztán és az
eukaliptuszerdőkben: kéregkunyhók, bumerángok és dárdavetők, emuk, termeszvárak, okker-fehér pöttyfestés, viharkakaduk.

**Mechanikák**
* **Nomád tábor**: minden kunyhó-épület *Felszedhető* – vándorbatyuvá alakul, átvonszolható és
  máshol 8 mp alatt újra *Letelepíthető*. Gyors terjeszkedés, menekülés, előretolt bázisok.
* **Falkaszellem**: minden harcos +8% sebzést kap minden 4 csempén belüli wullof harcostársért (max. +40%).
* **Vadászzsákmány**: a megölt ellenség ételköltségének 30%-a a wullofok éléskamrájába kerül.
* **Szellemidézés**: a Holdsámán ideiglenes szellemfarkasokat hív.

| Egység | Szerep |
|---|---|
| Gyűjtögető | munkás |
| Lándzsás harcos | gyors közelharcos |
| Bumerángvető | távolsági, lég+föld; *Bumeráng*: visszatérő íven még két ellenséget eltalál |
| Dárdavető portyázó | gyors portyázó dárdavetővel (woomera) |
| Holdsámán | gyógyító + *Szellemfarkasok* idézése |
| Emulovas | óriásemun száguldó lovasság, *Roham* |
| Viharkakadu | repülő, villám-terület-sebzés |
| Termeszvár-óriás | élő termeszvár, ostrom (épületbónusz) |

Épületek: Tanácskunyhó (HQ), Kéregkunyhó (ellátmány), Harcosok tüze, Emukarám, Sámánkunyhó,
Eukaliptusz-őrfa (torony, nem felszedhető), Vörös szikla.

---

**A történet szempontjából:** Vándorlás, falka, vadászat: a tábor sosem végleges, a zsákmány az élelem. A Hold ritmusa a harc ritmusa.

### Alapadatok

- **Főépület:** Tanácskunyhó  •  **Munkás:** Gyűjtögető  •  **Létszám:** Emuk (Emukarámban tenyészthetők, karámonként legfeljebb 5)
- **A varázslatok forrása:** ének

### Hogyan működik

- **Nomád tábor.** Az épületek felszedhetők, vándorszekérre rakva máshová vihetők, és 8 mp alatt újra felállnak. A Gyűjtögető belép az épülőbe, és belülről építi.
- **Falka.** Az együtt harcoló egységek erősebbek (közelben álló társanként +10%, legfeljebb +40%); a falka kapcsolóval terelhető együtt.
- **Vadászzsákmány.** Minden ölés fát hoz; a csapolt fa hordás nélkül ad fát.
- **Erdei rejtőzés.** Fák mellett láthatatlanok, amíg nem támadnak.
- **Emuk.** A létszámot emuk adják, amelyeket Emukarámban tenyésztenek (karámonként legfeljebb 5).
- **Ének és Hold.** A varázslatok énekből mennek; a Holdtölte kutatással 4 percenként 1 percig +15% sebzés. A bölényre a Bölénylovaglás kutatás után ülhet fel a gyalogos.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Bölény-csorda** – *Kevés, drága, megállíthatatlan roham*. Bölényeket nevelsz, és a harcosaid felülnek rájuk. A lovasság és a vadállatok gyorsabbak és erősebbek; a Csordavezér bika épületeket tipor. Középjátékban egyetlen, döntő rohamra épít. (épület: Bölénykarám; egység: Bölény, Bölénylovas, Csordavezér bika)
- **Groomi-fészek** – *Gyors portyázás, vadászat a gyengékre*. Piros szörnymadarak: a Groomik öléskor gyógyulnak és gyorsabbak, a Groomi-anya fiókákat keltet. A köpésük (sav, tűz vagy fagy) egyszer, végleg választható. Munkásvadászat, térképuralom. (épület: Groomi-fészek; egység: Groomi, Groomi-fióka, Groomi-anya)
- **Ősök útja** – *Totemlánc, lassú és biztos előrenyomulás*. Totemkertet emelsz: a tornyok erősebbek és messzebbre látnak, az épületek szívósabbak. Totemfaragók és Álomjárók támogatják a termeszvár-óriásokat. Védekező, területfoglaló játék. (épület: Totemkert; egység: Totemfaragó, Álomjáró, Termeszvár-óriás)

### Hősök

#### Tava

Folyami térképész és követ a Hét Út Szövetségéből. Utat nyit és felderít.

- **Nyomkereső nyíl** – Tava célzott lövése: nagy sebzés és lassítás egy ellenségen.
- **Útének** – A közeli szövetségesek gyorsabban mozognak (és jobban védekeznek).
- **Szellemfarkasok** – Szellem-dingókat idéz.
- **A Hét Út hívása** – Végső: két viharkakadu érkezik 45 másodpercre.

#### Varnu, a Holdsámán

Az ősök szellemeivel beszél: gyógyít, ködöt és szellemfalkát hív.

- **Holdfény** – Automatikusan gyógyítja a közeli sérült szövetségeseket.
- **Ősök köde** – Az ősök szellemei ködként fonják körül az ellenséget.
- **Szellemfalka** – Szellem-dingók falkáját hívja.
- **Telihold** – Végső képesség: telihold-fény gyógyít a területen.

#### Kurrin Harcfőnök

Harci bölényen lovagló harcfőnök: tapos, üvölt, és a Hét Út harcosait hívja.

- **Bölénytaposás** – A harci bölény a földbe dobbant: sebez és megbénít.
- **Harci üvöltés** – A közeli szövetségesek sebzése nő.
- **Vérfalka** – Vadászláz: rövid ideig gyorsabb és erősebb.
- **A Hét Út harcosai** – Végső képesség: öt lándzsás harcos érkezik a Hét Útról.

### Egységek (20)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Gyűjtögető | munkás | 30 fa, 20 arany | 45 / 0 | 5 sebzés, közel | Munkás. Fát csapol: a fához kötődik, és hordás nélkül, lassan, folyamatosan jön a fa (egy fán egy csapoló). Aranyat hord, javít; építéskor mellette állva dolgozik, többen együtt gyorsabban építenek. |
| Lándzsás harcos | lándzsás | 57 fa, 25 arany | 128 / 1 | 11 sebzés, közel | Gyors, kovakőhegyű lándzsás, keskeny festett hárítópajzzsal; dárdavetővel (woomera) messzire is hajít. Falkában erős. |
| Bumerángvető | lövész | 95 fa, 25 arany | 75 / 0 | 11 sebzés, 5.5 lőtáv | Távolsági vadász: a bumeráng visszatérő íven a közeli ellenségeket is eltalálja. Földi és légi célpontokra. |
| Holdsámán | varázsló | 85 fa, 70 meteorit, 25 arany | 70 / 0 | 6 sebzés, 4 lőtáv | Gyógyít, és szellemfarkasokat hív a holdfényből. |
| Szellemfarkas | közelharcos | – | 80 / 0 | 9 sebzés, közel | A holdfényből idézett szellem-dingó. Fél percig él. |
| Bölénylovas | lovas / hátas | 155 fa, 45 arany | 320 / 2 | 17 sebzés, közel | Bölényen ülő wullof harcos: erős roham. Le is szállhat. |
| Viharkakadu | repülő, repül | 95 meteorit, 100 fa, 55 arany | 170 / 1 | 14 sebzés, 4 lőtáv | Óriás viharkakadu; villámai a célpont környékét is sújtják. |
| Termeszvár-óriás | óriás | 330 fa, 90 meteorit, 85 arany | 520 / 3 | 32 sebzés, közel | Életre kelt vörös termeszvár – lassú, de az épületeket porrá zúzza. |
| Vándorbatyu | óriás | – | 300 / 1 | – | Felszedett tábor, rudakra kötve. Jobb gomb: Letelepítés máshol. |
| Dárdavető portyázó | lándzsás | 65 fa, 20 arany | 118 / 0 | 11 sebzés, közel | Gyors portyázó dárdavetővel (woomera): a munkások és a távolsági egységek réme. |
| Emu | közelharcos | 60 fa | 110 / 0 | – | Tenyészemu: +6 létszám. Emukarámonként legfeljebb 5 tartható. |
| Bölény | közelharcos | 110 fa, 40 arany | 260 / 2 | 9 sebzés, közel | Erős, de lassú bölény. Felültethető rá egy wullof gyalogos harcos. |
| Groomi | közelharcos | 85 fa, 25 meteorit | 160 / 1 | 13 sebzés, közel | Piros szörnymadár: gyors, harapós földi vadász. A Groomi-fészekben sav-, tűz- vagy fagyköpést tanulhat (csak egyet). |
| Csordavezér bika | óriás | 220 fa, 90 arany, 40 meteorit | 650 / 3 | 26 sebzés, közel | Hatalmas bölénybika: épületeket tipor, rohamában mindent elsöpör. |
| Groomi-fióka | közelharcos | 40 fa | 70 / 0 | 6 sebzés, közel | Kicsi, de fürge és harapós Groomi. |
| Groomi-anya | óriás | 160 fa, 60 meteorit | 420 / 2 | 20 sebzés, közel | Óriás Groomi-tojó: fiókákat keltet, és keményen harap. |
| Totemfaragó | varázsló | 90 fa, 30 meteorit | 110 / 0 | 7 sebzés, közel | Ősök-dalát éneklő faragó: a közeli szövetségesek +2 páncélt kapnak. |
| Álomjáró | varázsló | 80 fa, 70 meteorit | 100 / 0 | 9 sebzés, 5 lőtáv | Az ősök álmait járja: álomködöt borít az ellenségre, és az álmok útján ugrik egyik helyről a másikra. |
| Bumerángvető-falka (3) | lövész | 247 fa, 65 arany | 75 / 0 | 11 sebzés, 5.5 lőtáv | Három Bumerángvető egyszerre, olcsóbban. (Falkaharc kutatás kell hozzá.) |
| Lándzsás harcos-falka (3) | lándzsás | 148 fa, 65 arany | 128 / 1 | 11 sebzés, közel | Három Lándzsás harcos egyszerre, olcsóbban. |

**Az egységek képességei**

- **Erdei rejtőzés** – Fák mellett, álló helyzetben láthatatlan (lesállás), amíg nem támad; a közeli ellenség észreveszi.
- **Harci ének** – Ének (egyszerre egy): a közeli szövetségesek +8% sebzés.
- **Dárdavetés** – Dárdavetővel (woomera) hajított dárda: messzire repül, sebez és lassít.
- **Bumeráng** – Visszatérő bumeráng: a célpont után még két közeli ellenséget eltalál (csökkenő sebzéssel).
- **Vadászles** – Lesállás: nem mozog, de +2 lőtáv és +20% sebzés.
- **Holdfény-gyógyítás** – Automatikusan gyógyítja a közeli sérült élő szövetségeseket.
- **Szellemfarkasok** – Két szellem-dingót idéz 30 másodpercre.
- **Vándorének** – Ének (egyszerre egy): a közeli szövetségesek +10% sebesség.
- **Szellemtest** – Idézett lény; idővel szertefoszlik.
- **Roham** – 4 mp-ig +70% sebesség és +30% sebzés.
- **Leszállás** – A lovas leszáll: újra külön bölény és gyalogos harcos.
- **Villámcsapás** – A madár villámot sújt maga köré.
- **Ősök haragja** – Ének: a közeli szövetségesek +2 páncél.
- **Vándortábor** – Felszedett tábor – bárhol letelepíthető.
- **Riadófutás** – Az emu megriad és elszalad: 4 mp-ig +80% sebesség.
- **Felültetés** – A legközelebbi wullof gyalogos harcos felül a bölényre: Bölénylovas lesz belőlük.
- **Savköpés** – Maró sav: 8 sebzés, majd 6 mp-en át másodpercenként 6 méregsebzés.
- **Tűzköpés** – Lángcsóva: 26 sebzés a célpontra, fele a körülötte állókra.
- **Fagyköpés** – Jeges köpet: 10 sebzés, és a célpont 2,5 mp-re megfagy (nem mozog, nem támad).
- **Taposás** – A mamut a földbe dobbant: sebez és megbénít.
- **Fiókakeltetés** – Két Groomi-fióka kel ki a tojásaiból 45 másodpercre.
- **Álomköd** – Álomködöt borít egy területre 5 mp-re: az ellenség benne nagyon lelassul (–65%) és kicsit sebződik.
- **Álomséta** – Az álmok útján a kijelölt pontra lép (legfeljebb 7 csempe).

### Épületek (13)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Tanácskunyhó | 365 fa, 35 arany | Gyűjtögető | A törzs nagy kéregkunyhója. Leadóhely, Gyűjtögetőket nevel. Felszedhető. csoda: Ősök hívása (Három szellemfarkas jelenik meg a kijelölt ponton 40 mp-re. 3 perc töltődés.). |
| Kéregkunyhó | 85 fa | Lándzsás harcos, Lándzsás harcos-falka (3) | Eukaliptuszkéreg-kunyhó: Lándzsás harcosokat képez. Felszedhető. |
| Harcosok tüze | 180 fa, 20 arany | Bumerángvető, Bumerángvető-falka (3) | Dárdavetőket és bumerángvetőket képez. Felszedhető. |
| Emukarám | 150 fa | Emu | Emukat tenyészt (karámonként legfeljebb 5). Csak az emuk növelik a létszámot. Felszedhető. |
| Sámánkunyhó | 150 fa, 50 meteorit | Holdsámán | Holdsámánokat képez, fejlesztéseket kutat. Felszedhető. |
| Vörös szikla | 150 arany, 100 fa, 75 meteorit | Viharkakadu | A viharkakaduk fészke a pusztai vörös sziklán. |
| Eukaliptusz-őrfa | 100 fa, 50 arany | – | Kilátós őrfa: a fenti vadászok bumerángokkal védik a tábort. |
| Ősök köre | 180 fa, 100 arany | Tava, Varnu, a Holdsámán, Kurrin Harcfőnök | Faragott oszlopok köre: Tava, Varnu és Kurrin hívása. Felszedhető. |
| Bölénykarám | 220 fa, 90 arany | Bölény, Csordavezér bika | Bölényeket és termeszvár-óriásokat ad. Felszedhető. |
| Groomi-fészek | 200 fa, 60 meteorit | Groomi, Groomi-anya | Groomikat nevel; itt választható ki a köpésük. |
| Totemkert | 220 fa, 60 meteorit | Totemfaragó, Álomjáró, Termeszvár-óriás | Faragott totemek kertje: Totemfaragókat, Álomjárókat és termeszvár-óriásokat ad. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |

### Kutatások (18)

| Kutatás | Hol | Hatás |
|---|---|---|
| Zsákmányszerzés | Tanácskunyhó | A vadászzsákmány (ölésből) aranyat is hoz: a fa-zsákmány felét. |
| Falkaharc | Harcosok tüze | A Lándzsás harcosok és a Bumerángvetők falkában (hármasával, olcsóbban) is képezhetők: a képző épületen a „Falkában képez” kapcsolóval. |
| Kovakő lándzsahegyek | Sámánkunyhó | +2 sebzés a közelharci egységeknek. |
| Kovakő lándzsahegyek II. | Sámánkunyhó | +2 sebzés a közelharci egységeknek. (II. szint) |
| Kovakő lándzsahegyek III. | Sámánkunyhó | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Vasfa bumerángok | Sámánkunyhó | +2 sebzés a távolsági egységeknek. |
| Vasfa bumerángok II. | Sámánkunyhó | +2 sebzés a távolsági egységeknek. (II. szint) |
| Vasfa bumerángok III. | Sámánkunyhó | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Kengurubőr vértek | Sámánkunyhó | +1 páncél minden élő egységnek. |
| Kengurubőr vértek II. | Sámánkunyhó | +1 páncél minden élő egységnek. (II. szint) |
| Kengurubőr vértek III. | Sámánkunyhó | +1 páncél minden élő egységnek. (III. szint; a Felemelkedés után) |
| A falka szelleme | Sámánkunyhó | +10% sebesség minden egységnek. |
| Ősi nyomolvasás | Sámánkunyhó | A wullof egységek 8 csempén belül felfedik a láthatatlan ellenséget. |
| Holdtölte | Sámánkunyhó | Minden 4. percben 1 percig telihold van: a wullof egységek 15%-kal nagyobbat sebeznek. |
| Bölénylovaglás | Bölénykarám | A wullof gyalogos harcosok felülhetnek a bölényre (a Bölény Felültetés gombja): Bölénylovas lesz belőlük. |
| Savköpés | Groomi-fészek | A Groomik maró, mérgező savat köphetnek. Csak egy köpésfajta választható: ha ezt kifejleszted, a másik kettő többé nem fejleszthető. |
| Tűzköpés | Groomi-fészek | A Groomik lángcsóvát köphetnek. Csak egy köpésfajta választható: ha ezt kifejleszted, a másik kettő többé nem fejleszthető. |
| Fagyköpés | Groomi-fészek | A Groomik jeges köpetet köphetnek. Csak egy köpésfajta választható: ha ezt kifejleszted, a másik kettő többé nem fejleszthető. |

## Aurethi Birodalom

*Lélekidézés • gépjavítás • ostrom • lélekreaktorok*

Aureth népe: az Égszakadás éterbetegségében haldokló embereket Ordan, a Kéz, amely Megtart, géptestekbe vezette át. Valódi emberi szellemek acélban – nem születnek új tagjaik, ezért minden nyersanyag számukra életfeltétel. Létszámukat a lélekreaktorok adják; a Fabrikátorok javítják a gépeket, a szellemidézők az elesettekből légiót támasztanak.

**Háttér (FACTIONS_LORE.md):**

Aureth népe: az Égszakadás éterbetegségében haldokló embereket **Ordan, a Kéz, amely Megtart** vezette át géptestekbe. Valódi emberi szellemek acélban; nem születnek új tagjaik.
Dízelpunk mechák, légiós sasok, bronz és vas, zöldes lélekfény.

**Mechanikák**
* **Lélekreaktorok**: a géptestbe kerülés óta a létszámukat meteoritot igénylő lélekreaktorok adják (a 3. gazdasági változatban az étel megszűnt).
* **Gépszellemek**: nem gyógyulnak; a Fabrikátorok javítják őket (egységeket is!).
* **Szellemidézés**: a Szellemidéző a csatatéren maradt **tetemekből** (bármely faj halottaiból) szellemlégióst
  vagy nagy tetemből szellemkolosszust támaszt.
* **Kis és nagy gépek**: a gyalogság kis lépegető-test, a nehéz egységek mechák; a Kolosszus **ostrom-módba**
  telepíthető (mozdulatlan, de +3 lőtáv és nagyobb sebzés).

| Egység | Szerep |
|---|---|
| Fabrikátor | pókszerű munkás-gép, épít és javít |
| Légiós | kis gép, pajzsos közelharcos |
| Ballistarius | kis gép, távolsági |
| Szellemidéző | varázsló, *Lélekidézés* tetemekből |
| Centurio-mecha | közepes mecha, gépágyú |
| Aquila-drón | repülő |
| Kolosszus | óriás mecha, *Ostrom-mód* |
| (Szellemlégiós, Szellemkolosszus) | csak idézéssel |

Épületek: Fórum (HQ), Lélekreaktor (ellátmány), Légió-gyár, Öntöde (mechák), Ordan temploma, Hangár,
Aquila-torony.

**A történet szempontjából:** Halhatatlanság és ár: a gépek lelke visszatér, de minden újjászületésnek ára van. Rend, alap, márvány – ami alapozás nélkül épül, az lassan elkopik.

### Alapadatok

- **Főépület:** Fórum  •  **Munkás:** Fabrikátor  •  **Létszám:** Lélekreaktorok
- **A varázslatok forrása:** energia

### Hogyan működik

- **Lélekhorgony.** Az elesett gépek lelke 40 mp-ig horgonyban marad: egy Fabrikátor az eredeti ár 35%-áért újraépítheti. A népet „a halhatatlan légió” teszi.
- **Lélekidézés.** Az elesettek tetemeiből szellemlégiósok kelnek; az idézett szellemek nem foglalnak létszámot.
- **Alapozás.** A Fabrikátor márvány lapkövet rak le; alapozáson az épület teljes életerővel áll, alapozás nélkül csak félerővel, és lassan kopik.
- **Gépek és javítás.** A Fabrikátorok javítják a gépeket; a Kolosszus ostrom-módba telepíthető. Az Arató lánctalpas gépként bányássza az aranyat és a meteoritot.
- **Reaktorok és szobrok.** A létszámot a lélekreaktorok adják, pajzs-zónát is vetítenek. A három isten-szobor (Ferrox, Arcanor, Umbrax) bónuszt ad, amíg áll.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Légió** – *Fegyelmezett tömeg alakzatban*. Alakzat: a gyalogosok minden közeli bajtársért +6% sebzést kapnak (max. +30%), és +1 páncélt. Praetoriánusok és Centurio-mechák. (épület: Praetorium; egység: Praetoriánus, Centurio-mecha)
- **Gépisten** – *Kevés, hatalmas gép*. Kolosszusok és onager-mechák, +150 életerővel és +4 ostromsebzéssel. Lassú, drága, de megállíthatatlan. (épület: Öntöde; egység: Onager-mecha, Kolosszus)
- **Lélek** – *Kifárasztó, élőhalott játék*. A megölt élő ellenségek 20%-a szellemlégiósként kel fel nálad (40 mp-re). A Lélekkútban állandó szellemlégiósok és szellemkolosszusok képezhetők. (épület: Lélekkút; egység: Szellemlégiós, Szellemkolosszus)

### Hősök

#### Livia Varr

Egykori orvos, a lélekhorgonyokat mentő légió tisztje.

- **Lélekmentés** – Egy közeli tetemből szellemlégióst támaszt.
- **Horgonyjavítás** – Automatikusan javítja a közeli sérült gépeket.
- **A légió fegyelme** – A közeli szövetségesek sebzése nő.
- **Második lélegzet** – Végső képesség: a területen minden szövetséges gyógyul.

#### Nera

Livia lánya: gyermekként került géptestbe; hetven év tapasztalattal, de a birodalmi jog szerint még mindig kiskorú.

- **Gyorstöltés** – Rövid ideig nagyobb sebzés.
- **Célzott lövés** – Messzire hordó, erős lövés.
- **Bátorság** – „Félek – és mégis megyek.” A közeli szövetségesek páncélja nő.
- **Szabad horgony** – Végső képesség: Nera felszabadult lelke 10 másodpercre félelmetes erővé válik.

#### Cassius Ferrum legátus

A Hetedik Légió parancsnoka nehéz géptestben: roham, fegyelem és Ordan haragja.

- **Légió-roham** – A legátus gépteste előrelendül, és szétszórja az ellenséget.
- **Vasfegyelem** – A közeli szövetségesek páncélja nő.
- **Mechalégió** – Légiósokat rendel a frontra.
- **Ordan haragja** – Végső képesség: Ordan lelke tölti meg a legátus gépét.

### Egységek (13)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Fabrikátor | munkás | 30 fa, 20 arany | 60 / 1 | 5 sebzés, közel | Pókszerű munkásgép. Épít (egy épületen egyszerre csak egy Fabrikátor dolgozhat – Túlhajtással gyorsíthat), alapoz, fát hord, és javítja az épületeket és a gépeket. Aranyat és meteoritot az Arató gyűjt. |
| Légiós | közelharcos | 35 fa, 30 arany | 182 / 2 | 10 sebzés, közel | Kis lépegetőtestbe zárt légiós-szellem, scutummal és gladiusszal. |
| Ballistarius | lövész | 65 fa, 15 arany | 100 / 1 | 14 sebzés, 6 lőtáv | Kis gép vállra szerelt íjpuskával. |
| Szellemidéző | varázsló | 50 arany, 75 meteorit, 30 fa | 70 / 0 | 6 sebzés, 4 lőtáv | Lélekmentő gép-pap. A csatatér tetemeiből lélekhorgonyt ment, és szellemlégióst támaszt. |
| Centurio-mecha | lövész | 120 arany, 25 meteorit, 60 fa | 320 / 3 | 6 sebzés, 4.5 lőtáv | Közepes lépegető mecha forgócsöves gépágyúval. |
| Aquila-drón | repülő, repül | 75 meteorit, 65 fa, 35 arany | 140 / 1 | 11 sebzés, 4 lőtáv | Sas formájú repülő gép. |
| Kolosszus | óriás | 270 arany, 100 meteorit, 130 fa | 750 / 4 | 38 sebzés, 6 lőtáv | Óriás ostrom-mecha. Ostrom-módban mozdulatlan, de messzebbre és nagyobbat lő. |
| Szellemlégiós | közelharcos | 60 arany, 30 meteorit | 110 / 0 | 11 sebzés, közel | Egy elesett harcos lelke, a Szellemidéző akaratához kötve. |
| Szellemkolosszus | óriás | 160 arany, 120 meteorit | 400 / 1 | 28 sebzés, közel | Egy elesett óriás lelke. |
| Onager-mecha | ostromgép | 180 arany, 40 meteorit, 70 fa | 320 / 2 | 36 sebzés, 8.5 lőtáv | Négylábú hajítógép: messzire hord, épületek és csoportok ellen. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), mozgás előtt 1 mp-ig csukódik össze. |
| Lélekhorgony | közelharcos | – | 400 / 0 | – | Egy elesett gép lelke. Egy Fabrikátor 40 mp-en belül újraépítheti (javítással) – különben a lélek elillan. |
| Praetoriánus | közelharcos | 80 arany, 40 meteorit | 260 / 4 | 16 sebzés, közel | Elit gépharcos aranyozott páncélban: a légió gerince. |
| Arató | munkás | 100 fa, 50 arany | 300 / 3 | – | Lánctalpas gyűjtőgép: aranyat és meteoritot bányászik, egyszerre nagy rakománnyal (30 arany vagy 20 meteorit). Lassú és drága – egy kilőtt Arató egy teljes rakomány. |

**Az egységek képességei**

- **Gépjavítás és újratestesítés** – Jobb klikk sérült gépre: javítás. A lélekhorgonyokat is újraépíti.
- **Túlhajtás** – A Fabrikátor túlhajtja magát: annak az épületnek, amelyen dolgozik, azonnal elkészül a hátralévő építés fele (javításnál a hiányzó életerő fele). Ára a Fabrikátor életerejének 40%-a – bele is pusztulhat.
- **Testudo** – Teknősalakzat: -55% távolsági sebzés, +1 páncél, de lassú.
- **Lélekhorgony** – Ha a gép elpusztul, a lelke horgonyban marad 40 mp-ig – egy Fabrikátor újraépítheti.
- **Gőztúlnyomás** – 6 mp-re +40% sebesség; utána 3 mp-ig lassabb, amíg a kazán lehűl.
- **Gyorstöltés** – 6 mp-ig +40% sebzés.
- **Lélekidézés** – Egy közeli tetemből szellemlégióst (nagy tetemből szellemkolosszust) támaszt.
- **Gépágyú-sorozat** – 5 mp-ig +70% sebzés, de lassabb.
- **Zuhanóbomba** – Bombát old ki maga alá.
- **Ostrom-mód** – A Kolosszus letelepszik: nem mozog, de +3 lőtáv és +25% sebzés.
- **Szellemtest** – Idézett lény; idővel szertefoszlik.
- **Ostrom-telepítés** – Lehorgonyoz: nem mozog, de +3 lőtáv és +25% sebzés.

### Épületek (19)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Fórum | 350 arany, 150 fa | Fabrikátor, Arató | Oszlopcsarnokos főtér gőzölgő gépházzal. Leadóhely, Fabrikátorokat gyárt. csoda: Ordan villáma (Ordan villáma csap le a kijelölt pontra: azonnal 160 sebzés egységeknek és épületeknek is. Ára 100 meteorit, 2,5 perc töltődés.). |
| Lélekreaktor | 80 arany, 30 fa | – | +10 létszám – lélekenergiát ad a gépeknek. |
| Légió-gyár | 100 fa, 100 arany | Légiós, Ballistarius | Légiósokat és Ballistariusokat szerel össze. |
| Öntöde | 150 fa, 175 arany | Onager-mecha, Kolosszus | Centurio-mechákat és Kolosszusokat önt. |
| Ordan temploma | 150 arany, 75 meteorit | Szellemidéző | Ordan, a Kéz, amely Megtart hálózatának szentélye. Szellemidézőket avat, fejlesztéseket kutat. |
| Hangár | 150 arany, 75 fa, 50 meteorit | Aquila-drón | Aquila-drónokat gyárt. |
| Aquila-torony | 125 arany, 25 fa | – | Sasos őrtorony íjpuskával. |
| Panteon | 180 arany, 100 fa | Livia Varr, Nera, Cassius Ferrum legátus | A megmentett hősök csarnoka: Livia, Nera és Cassius. |
| Praetorium | 220 arany, 60 meteorit | Praetoriánus, Centurio-mecha | A légió parancsnoksága: praetoriánusok és Centurio-mechák. |
| Lélekkút | 180 arany, 120 meteorit | Szellemlégiós, Szellemkolosszus | Lélekfénnyel izzó kút: állandó szellemlégiósok és szellemkolosszusok. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |
| Alapozás | 10 arany | – | Márvány-lapkő (2×2): a Fabrikátor 3 mp alatt rakja le, a mozgást nem akadályozza. Ami teljes egészében alapozáson áll, teljes életerővel épül fel; ami nem, az fél életerővel, és lassan kopik is (fél életerőig). |
| Finomító | 100 fa, 40 arany | – | Lerakóhely aranyhoz és meteorithoz – érdemes a bányák mellé építeni. Itt választható az Arató fejlesztése: gyorsabb hajtómű VAGY nagyobb tartály. |
| Ferrox-szobor | 60 fa, 80 arany | – | Ferrox, az acél ura: kalapácsos, öntöttvas-szakállú alak. A körzetében (6 csempe) a gépek és az épületek +2 páncélt kapnak – amíg a szobor áll. |
| Arcanor-szobor | 60 fa, 80 arany | – | Arcanor, a mágia ura: csillagköpenyes alak, lebegő gömbbel. A körzetében (6 csempe) a távolsági egységek +1 lőtávot kapnak – amíg a szobor áll. |
| Umbrax-szobor | 60 fa, 80 arany | – | Umbrax, a túlvilág kapusa: fátylas alak lámpással és kulccsal. A körzetében (6 csempe) az egységek +1 életerőt töltenek vissza másodpercenként – amíg a szobor áll. |
| Szoborpark | 150 fa, 100 arany | – | Az öreg istenek – Ferrox, Arcanor és Umbrax – szoborparkja. Itt kutathatók az isteni fejlesztések, aranyért; mindegyik csak akkor, ha az adott isten szobra áll. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |
| Nagy alapozás | 30 arany | – | Márvány-lapkő (4×4): csempénként olcsóbb, a Fabrikátor 6 mp alatt rakja le. (A Nagy alapozás kutatás kell hozzá az Öntödében.) |

### Kutatások (24)

| Kutatás | Hol | Hatás |
|---|---|---|
| Nagy alapozás | Öntöde | A Fabrikátorok 4×4-es márvány-lapkövet is lerakhatnak (30 arany, 6 mp): csempénként olcsóbb. A meglévő lapkövek nem változnak. |
| Gőztúlnyomás | Öntöde | A gépek megkapják a Gőztúlnyomás képességet: 6 mp-re +40% sebesség, utána 3 mp-ig lassabbak. |
| Szikrafogó vértek | Öntöde | A gépek nem gyengék a varázslók ellen. |
| Légiós pengék | Ordan temploma | +2 sebzés a közelharci egységeknek. |
| Légiós pengék II. | Ordan temploma | +2 sebzés a közelharci egységeknek. (II. szint) |
| Légiós pengék III. | Ordan temploma | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Gyorstöltés | Ordan temploma | +2 sebzés a távolsági egységeknek. |
| Gyorstöltés II. | Ordan temploma | +2 sebzés a távolsági egységeknek. (II. szint) |
| Gyorstöltés III. | Ordan temploma | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Edzett acél | Ordan temploma | +1 páncél minden gépnek. |
| Edzett acél II. | Ordan temploma | +1 páncél minden gépnek. (II. szint) |
| Edzett acél III. | Ordan temploma | +1 páncél minden gépnek. (III. szint; a Felemelkedés után) |
| Megerősített váz | Ordan temploma | +30 életerő minden gépnek. |
| Reaktorpajzs | Ordan temploma | A gépek 30 pajzsot kapnak; a pajzs csak Lélekreaktor (6 csempe), meteorit vagy Égkő közelében töltődik. |
| Gyorsított hajtómű | Finomító | +25% sebesség az Aratóknak. (Kizárja a Bővített tartályt.) |
| Bővített tartály | Finomító | +50% rakomány az Aratóknak. (Kizárja a Gyorsított hajtóművet.) |
| Ferrox kalapácsa | Szoborpark | +2 sebzés a közelharcos egységeknek. (Kell hozzá: Ferrox-szobor.) |
| Ferrox kalapácsa II. | Szoborpark | +2 sebzés a közelharcos egységeknek. (Kell hozzá: Ferrox-szobor.) (II. szint) |
| Ferrox kalapácsa III. | Szoborpark | +2 sebzés a közelharcos egységeknek. (Kell hozzá: Ferrox-szobor.) (III. szint; a Felemelkedés után) |
| Öntöttvas burok | Szoborpark | +60 életerő a nagy gépeknek. (Kell hozzá: Ferrox-szobor.) |
| Arcanor csillagai | Szoborpark | +25 energia a varázslóknak. (Kell hozzá: Arcanor-szobor.) |
| Csillagtérkép | Szoborpark | +2 látótáv minden egységnek. (Kell hozzá: Arcanor-szobor.) |
| Umbrax fátyla | Szoborpark | +15% sebesség a szellem-egységeknek. (Kell hozzá: Umbrax-szobor.) |
| Lélekkapu | Szoborpark | Az elesett ellenségek 8% eséllyel szellemlégiósként kelnek fel a te oldaladon (30 mp-re). (Kell hozzá: Umbrax-szobor.) |

## Huk’ucha

*A békanép tavai*

A békanép maga hozza létre tavirózsás tavacskáit, amelyekre vízinövényekből, korallból és lakható üvegbuborékokból épít otthonokat. Harcosaik csúzlival vagy tompa végű, apró kampóban végződő lapos karddal küzdenek, pajzs nélkül. Mágusaik lábon járó tavi lényeket idéznek.

**A történet szempontjából:** A tó mint otthon és fegyver: a nép maga ássa ki a világát, és dalban kéri a Tóanya kegyét.

### Alapadatok

- **Főépület:** Korallcsarnok  •  **Munkás:** Tóásó  •  **Létszám:** Gyöngybuborék-otthonokkal
- **Tóének:** A Tóteremtő szentélyhez küldött Tóásók a Tóanyának énekelnek (jobb klikk a szentélyre).

### Hogyan működik

- **Tavacskák.** Minden épület tavacskára épül. A Tóásó ássa ki (ott kell állnia, amíg ás), és a tavacskák összefüggő tavat alkotnak. A saját tavacskán a békák gyorsabbak.
- **Maguktól növő otthonok.** A Tóásó elindítja az építkezést és továbbmegy; az épület magától nő, és minél több tavacska veszi körül, annál gyorsabban (legfeljebb +60%).
- **Tóének.** A Tóteremtő szentélyhez küldött Tóásók a Tóanyának énekelnek. Ebből mennek a tó csodái (Áradás, Gyöngyérés, tavi lények hívása) és az Égi kegy (Árhullám).
- **Idézés és ikrák.** A mágusok lábon járó tavi lényeket idéznek; az Ikrarakó csomóiból ebihalak kelnek ki.
- **Harcmodor.** Csúzli vagy tompa végű, kampós lapos kard, pajzs nélkül; a Kagylópajzsos a kivétel.

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Árvíz** – *A tó mindent elnyel*. A tavacskák 3×3-asok lesznek, az egész nép gyorsabb; az Áradás papjai árhullámot zúdítanak, az iszapbékák a frontot tartják. Terjeszkedő, mozgékony játék a vízen. (épület: Árvízkapu; egység: Áradás papja, Iszapbéka)
- **Gyöngy** – *Pontos lövés, kemény héj*. Mesterlövész gyöngyvetők és kagylópajzsos fal; a távolsági egységek erősebbek. Védekező, lőállásból építkező játék. (épület: Gyöngykagyló-kert; egység: Gyöngyvető, Kagylópajzsos)
- **Mélység** – *A tó mélyének óriásai*. Gyöngyóriások és tömeges idézés; az idézett lények tovább élnek. Kevés, de hatalmas egység. (épület: A mélység kapuja; egység: Gyöngyóriás, Mélységi hívó)

### Hősök

#### Kvarra, a Tóanya

Az öreg tóanya: tavat fakaszt, és vízből gyúrt lényeket kelt életre.

- **Árasztás** – Vízzel árasztja el a területet: sebez és lassít.
- **Gyógyforrás** – A közeli szövetségesek gyorsabban gyógyulnak.
- **Vízlények** – Tavi lényeket kelt életre.
- **Özönvíz** – Végső: hatalmas áradás a csatatéren.

#### Bul’gok Kampókirály

A kampókardosok királya: rántás, ugrás, tavi tombolás.

- **Királyi kampó** – Megrántja és megbénítja a célpontot.
- **Békaugrás** – Nagyot ugrik és lecsap: körben sebez.
- **Harci brekegés** – A közeli szövetségesek erősebben csapnak.
- **A Tó Királya** – Végső: óriássá nő, sebzése és páncélja megnő.

#### Iriss Szivárványgyöngy

Gyöngyhalász-hercegnő: gyógyít, és szivárványfénnyel véd.

- **Igazgyöngy-lövés** – Ragyogó gyöngy: nagy sebzés egy célpontra.
- **Szivárványpajzs** – A közeli szövetségesek páncélt kapnak.
- **Tavi áldás** – Gyógyítja a közeli sebesülteket.
- **Gyöngykapu** – Végső: gyöngyfény-robbanás – sebez és megbénít.

### Egységek (19)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Tóásó | munkás | 50 fa | 45 / 0 | 4 sebzés, közel | Munkás: tavacskát ás (2×2 – erre építhet a nép), gyűjt, és elindítja az építkezést. Az épület magától nő; minél több tavacska veszi körül, annál gyorsabban (legfeljebb +60%). |
| Csúzlis | lövész | 60 fa, 20 arany | 65 / 0 | 9 sebzés, 5.5 lőtáv | Gyöngygolyós csúzlival lő; pajzsa nincs. |
| Kampókardos | közelharcos | 65 fa, 20 arany | 138 / 1 | 13 sebzés, közel | Tompa végű, kampós lapos karddal harcol – a kampóval magához rántja az ellenfelet. |
| Tóidéző | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Mágus: lábon járó tavi lényt idéz, és gyógyít. |
| Tavi lény | óriás, idézett | – | 220 / 1 | 14 sebzés, közel | Lábon járó, megidézett víztest; rövid ideig harcol. |
| Gőtelovas | lovas / hátas | 140 fa, 45 arany | 280 / 2 | 15 sebzés, közel | Óriásgőte hátán száguldó portyázó. |
| Szitakötő-felderítő | repülő, repül | 70 fa, 20 arany | 70 / 0 | 5 sebzés, 2.5 lőtáv | Gyors, repülő felderítő; gyenge, de messzire lát. |
| Teknőshátas mozsár | ostromgép | 210 fa, 110 arany | 330 / 3 | 38 sebzés, 7.5 lőtáv | Óriásteknős hátán gyöngyágyú – épületek ellen. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), mozgás előtt 1 mp-ig csukódik össze. |
| Áradás papja | varázsló | 80 fa, 30 arany, 60 meteorit | 90 / 0 | 7 sebzés, 4.5 lőtáv | Árhullámot zúdít, és a tavat kiterjeszti. |
| Iszapbéka | óriás | 150 fa, 70 arany | 520 / 3 | 22 sebzés, közel | Hatalmas, iszapos varangy: elnyeli a csapásokat. |
| Gyöngyvető | lövész | 90 fa, 50 arany | 90 / 0 | 20 sebzés, 7.5 lőtáv | Mesterlövész: igazgyöngyöt lő, hosszú lőtávval. |
| Kagylópajzsos | lándzsás | 90 fa, 50 arany | 240 / 4 | 11 sebzés, közel | Óriáskagyló-teknőt hord – a nép egyetlen pajzsos harcosa. |
| Gyöngyóriás | óriás | 220 fa, 120 arany, 60 meteorit | 760 / 4 | 34 sebzés, közel | A mélység gyöngyhéjú óriása. |
| Mélységi hívó | varázsló | 90 fa, 40 arany, 70 meteorit | 95 / 0 | 6 sebzés, 4 lőtáv | Két tavi lényt hív egyszerre a mélyből. |
| Ikrarakó | közelharcos | 60 fa, 20 arany | 60 / 0 | – | Fürge kis béka-dajka: láthatatlan ikra-csomókat rak le, amelyekből rálépéskor harapós ebihalak kelnek ki. |
| Ikra-csomó | közelharcos, idézett | – | 30 / 0 | – | Láthatatlan ikra: ha ellenség lép rá, ebihalak kelnek ki belőle. |
| Ebihal | közelharcos, idézett | – | 35 / 0 | 6 sebzés, közel | Harapós ebihal az ikrából; rövid ideig harcol. |
| Nagy tavi lény | óriás, idézett | – | 330 / 1 | 21 sebzés, közel | A Tóének erejével megidézett, nagyobb víztest; rövid ideig harcol. |
| Megidézett gyöngyóriás | óriás, idézett | – | 760 / 4 | 34 sebzés, közel | A Mélység hangjára előjövő gyöngyóriás; 45 mp-ig harcol. |

**Az egységek képességei**

- **Békaugrás** – A béka nagyot ugrik a kijelölt pontra (vízen és akadályon át is): 2,5 csempe, az Ugrólábak kutatással 5. Landoláskor megsebzi a közeli ellenséget.
- **Horog** – A kampó a célpontba akad: sebez és egy pillanatra megbénít.
- **Tavi lény** – Lábon járó víztestet idéz 40 másodpercre.
- **Tóvíz-gyógyítás** – Gyógyítja a közeli sebesülteket.
- **Nagy tavi lény** – Erősebb tavi lényt idéz 60 mp-re (+50% életerő és sebzés). Ára 50 Tóének.
- **Gőtevágta** – Rövid, sebes roham.
- **Árhullám** – Árhullám: sebez és lelassít.
- **Iszapcsapás** – Iszapot fröccsent: körben sebez és megbénít.
- **Kagylófal** – Teknőjébe húzódik: erős védelem, lassú mozgás.
- **Héjba húzódás** – Behúzódik a kagylóhéjba: 8 mp-ig semmilyen egységtípus ellen nem gyenge, de lassabb.
- **Mélységdobbantás** – Földrengető dobbantás.
- **Mélységi hívás** – Két tavi lényt hív 60 másodpercre.
- **Mélység hangja** – Egy gyöngyóriást hív elő a mélyből 45 mp-re. Ára 60 Tóének és 50 energia.
- **Ikrarakás** – Láthatatlan ikra-csomót rak le (egyszerre legfeljebb 12). Ha ellenség lép rá, 3 harapós ebihal kel ki belőle (saját tavacskán 5), és 10 mp-ig harcolnak.

### Épületek (13)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Korallcsarnok | 360 fa, 40 arany | Tóásó | A békanép csarnoka korallból és üvegbuborékokból. Leadóhely, Tóásókat nevel. csoda: Árhullám (A Tóanya hulláma elönti a kijelölt pontot: 4 mp alatt 60 sebzés, és felére lassítja az ellenséget. Ára 120 Tóének, 2,5 perc töltődés.). |
| Tavacska | 15 fa | – | Tavirózsás vízfolt (2×2): a Tóásó ott állva 4 mp alatt ássa ki. A Huk’ucha csak tavacskára építhet; a szomszédos tavacskák egybefüggő tóvá olvadnak. A tavon a békanép gyorsabb, mindenki más lassabb. |
| Gyöngybuborék-otthon | 90 fa, 10 arany | – | Lakható üvegbuborék a tavon: +8 létszám. |
| Nádbarakk | 150 fa, 40 arany | Csúzlis, Kampókardos, Szitakötő-felderítő, Ikrarakó | Csúzlisokat, kampókardosokat és szitakötő-felderítőket képez. |
| Tóteremtő szentély | 160 fa, 80 arany, 40 meteorit | Tóidéző, Kvarra, a Tóanya, Bul’gok Kampókirály, Iriss Szivárványgyöngy | Tóidézőket és hősöket képez. csoda: Áradás (30 mp-re minden saját tavacska egy csempével kitágul, és a rajtuk álló saját egységek másodpercenként 3 életerőt kapnak. Ára 30 Tóének, 90 mp töltődés.). |
| Gyöngyműhely | 150 fa, 120 arany | – | A békanép fejlesztései: gyöngy, kagylóhéj, tavirózsa. csoda: Gyöngyérés (Az épület 30 mp-re kétszer gyorsabban kutat és képez. Ára 25 Tóének, 60 mp töltődés.). |
| Gőteistálló | 180 fa, 90 arany | Gőtelovas, Teknőshátas mozsár | Gőtelovasokat és teknőshátas mozsarakat nevel. |
| Korallőr | 100 fa, 60 arany | – | Korall-védőtorony gyöngyhajítóval. |
| Árvízkapu | 180 fa, 100 arany | Áradás papja, Iszapbéka | Áradás papjait és iszapbékákat nevel. |
| Gyöngykagyló-kert | 180 fa, 100 arany | Gyöngyvető, Kagylópajzsos | Gyöngyvetőket és kagylópajzsosokat nevel. csoda: Gyöngyérés (Az épület 30 mp-re kétszer gyorsabban kutat és képez. Ára 25 Tóének, 60 mp töltődés.). |
| A mélység kapuja | 200 fa, 120 arany, 40 meteorit | Gyöngyóriás, Mélységi hívó | Gyöngyóriásokat és mélységi hívókat nevel. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |

### Kutatások (16)

| Kutatás | Hol | Hatás |
|---|---|---|
| Tóanya kórusa | Tóteremtő szentély | Az éneklő Tóásók 0,4 Tóéneket adnak másodpercenként (0,3 helyett). |
| Iszapos bőr | Tóteremtő szentély | Minden béka +1 páncélt kap. |
| Mély víz | Tóteremtő szentély | A megidézett lények 30%-kal tovább maradnak. |
| Gyöngygolyó | Gyöngyműhely | +2 sebzés a távolsági egységeknek. |
| Gyöngygolyó II. | Gyöngyműhely | +2 sebzés a távolsági egységeknek. (II. szint) |
| Gyöngygolyó III. | Gyöngyműhely | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Kagylóhéj-vért | Gyöngyműhely | +1 páncél az élő egységeknek. |
| Kagylóhéj-vért II. | Gyöngyműhely | +1 páncél az élő egységeknek. (II. szint) |
| Kagylóhéj-vért III. | Gyöngyműhely | +1 páncél az élő egységeknek. (III. szint; a Felemelkedés után) |
| Tavirózsa-tea | Gyöngyműhely | +1 életerő-visszatöltés másodpercenként. |
| Ugrólábak | Gyöngyműhely | +10% sebesség minden egységnek, és a Békaugrás 5 csempére nő (2,5 helyett). |
| Kampóhegy | Gyöngyműhely | +2 sebzés a közelharci egységeknek. |
| Kampóhegy II. | Gyöngyműhely | +2 sebzés a közelharci egységeknek. (II. szint) |
| Kampóhegy III. | Gyöngyműhely | +2 sebzés a közelharci egységeknek. (III. szint; a Felemelkedés után) |
| Tavak útja | Gyöngyműhely | A saját tavacskán álló békák 20%-kal nagyobbat sebeznek. |
| Héjba húzódás | Gyöngykagyló-kert | A Kagylópajzsosok megkapják a Héjba húzódás képességet: 8 mp-ig semmi ellen nem gyengék. |

## Ooni

*Onik, nianok és tengu követek*

Az onik és nianok közösségében a buzogányos harcosok mellett négylábú őrzők és szárnyas tengu követek is szolgálnak. Csarnokaik márványoszlopai fölé ívelt cseréptetők borulnak, kapuiknál faragott őrzők és papírlámpások állnak.

**A történet szempontjából:** Bátorság és fény: a sötétben a harci kedv elfogy, a lámpás és a dob tartja egyben a sereget.

### Alapadatok

- **Főépület:** Nian csarnok  •  **Munkás:** Lámpásvivő  •  **Létszám:** Oni lakóházakkal

### Hogyan működik

- **Lámpásfény.** A lámpások és épületek fénykörében az egységek gyógyulnak; a fény határozza meg, hol építhet a nép.
- **Harci kedv.** Minden egységnek harci kedve van: sebesüléskor, társ vagy hős halálakor fogy, nyugalomban visszatölt. Ha elfogy, az egység megtörik: gyengébben üt és menekül, amíg vissza nem tér a bátorsága. A Taiko-dobos ritmusa visszaadja, a lámpásfény védi.
- **Dobos építés.** A dobos ritmusa a közelben gyorsítja az építkezést (+50%).
- **Szakosodott gyűjtők.** A Favágó oni fát vág, a Tanuki aranyat és meteoritot hord; a Lámpásvivő épít.
- **Torii-kapuk.** A kapupár egyik felét lámpásfényben, a másikat bárhová lehet tenni, ahol látsz; a két kapu között átkel a sereg.
- **Kőkutyák.** A Kőkutya-őr mellett álló épület páncélt kap (kutatással még többet).

### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)

- **Vörös buzogány** – *Oni roham*. Tomboló onik és harci dobok: a közelharcosok erősebbek és gyorsabbak, a Taiko-dobosok szívósabbak, és ritmusuk +75%-kal gyorsítja az építkezést. Korai, kemény nyomás. (épület: Vörös dódzsó; egység: Tomboló oni)
- **Kapuőrzők** – *A nian-kapuk erőssége*. Kőkutya-őrök és lámpáspapok; az épületek és a nagy testű őrzők erősebbek. Lassú, áttörhetetlen védelem. (épület: Kapuszentély; egység: Kőkutya-őr, Lámpáspap)
- **Tengu szél** – *Légierő és vihar*. Hollótengu-harcosok és viharhívók; a repülők gyorsabbak. Mozgékony, légi portyázó játék. (épület: Szélcsúcs; egység: Hollótengu-harcos, Viharhívó tengu)

### Hősök

#### Akahana, a Vörös Buzogány

Az onik vezére: rohamoz és földet rengeti.

- **Vörös roham** – Nekiront a célpontnak: sebez és megbénít.
- **Földrengetés** – Buzogányával a földbe sújt: körben sebez.
- **Oni harag** – A közeli onik erősebben csapnak.
- **Vörös vihar** – Végső: tomboló vörös vihar – sebzése és sebessége megnő.

#### Shiro, a Kapuőr

A fehér nian: védi a kapukat és a társait.

- **Nian-pajzs** – A közeli társak páncélt kapnak.
- **Nian-üvöltés** – Rettentő üvöltés: körben sebez és megbénít.
- **Őrkapu** – Gyógyítja a közeli sebesülteket.
- **Az őrök hívása** – Végső: két kőkutya-őrt hív.

#### Karasu, a tengu bölcs

Az öreg tengu: szélvihart és villámot hív.

- **Tengu-villám** – Villám egy célpontra.
- **Szélörvény** – Szélörvény: sebez és lassít.
- **Hollótollak** – A közeli repülők gyorsabbak.
- **Tájfun** – Végső: tájfun söpör végig a csatatéren.

### Egységek (17)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Lámpásvivő | munkás | 50 fa | 55 / 0 | 5 sebzés, közel | Kis oni munkás: papírlámpást állít (a nép csak lámpásfényben építhet), épít, és végig ott marad az építkezésen. Nem gyűjt. Több munkás nem gyorsít, de egy közeli Taiko-dobos ritmusa +50%-kal igen. |
| Oni harcos | közelharcos | 60 fa, 30 arany | 190 / 2 | 15 sebzés, közel | Vörös bőrű, szarvas oni, vasbuzogánnyal. |
| Kőhajító oni | lövész | 65 fa, 40 arany | 120 / 1 | 14 sebzés, 5.5 lőtáv | Sziklákat hajít az ellenségre. |
| Nian őrző | óriás | 100 fa, 70 arany | 380 / 3 | 17 sebzés, közel | Négylábú, zöld sörényű őrző. Kapuőrként egy épület mellé állhat: az épület páncélt kap, a nian helyben marad és védi. |
| Tengu követ | repülő, repül | 100 fa, 35 arany, 50 meteorit | 110 / 0 | 9 sebzés, 4 lőtáv | Szárnyas, hollótollú követ: legyezőjével szélvihart kavar. |
| Kitsune-maszkos | varázsló | 75 fa, 25 arany, 60 meteorit | 85 / 0 | 6 sebzés, 4 lőtáv | Rókamaszkos mágus: illúzió-másokat szőve megtéveszti az ellent, és gyógyít. |
| Illúzió-más | közelharcos, idézett | – | 50 / 0 | 5 sebzés, közel | Rövid életű rókaszellem-másolat. |
| Kapudöngető | ostromgép | 200 fa, 100 arany | 420 / 3 | 45 sebzés, közel | Faltörő kos, onik tolják – épületek ellen. |
| Ō-oni | óriás | 220 fa, 110 arany, 50 meteorit | 780 / 4 | 34 sebzés, közel | Óriás oni, a hegyek ura. |
| Tomboló oni | közelharcos | 110 fa, 40 arany | 260 / 2 | 20 sebzés, közel | Kétbuzogányos, megszállott oni. |
| Taiko-dobos | közelharcos | 80 fa, 30 arany, 40 meteorit | 120 / 1 | 8 sebzés, közel | Harci dobjával bátorítja az onikat, és ritmusával +50%-kal gyorsítja a közeli építkezést. |
| Kőkutya-őr | óriás | 150 fa, 90 arany | 620 / 5 | 18 sebzés, közel | Élő kőoroszlán – a kapuk legkeményebb őre. |
| Lámpáspap | varázsló | 80 fa, 30 arany, 60 meteorit | 95 / 0 | 6 sebzés, 4 lőtáv | A lámpások papja: gyógyít, és védőfényt von a társakra. |
| Hollótengu-harcos | repülő, repül | 120 fa, 45 arany, 40 meteorit | 170 / 1 | 14 sebzés, 1.5 lőtáv | Kardos tengu, a magasból csap le. |
| Viharhívó tengu | repülő, repül | 100 fa, 40 arany, 80 meteorit | 120 / 0 | 10 sebzés, 4.5 lőtáv | Villámot és tájfunt hív. |
| Favágó oni | munkás | 55 fa | 95 / 0 | 6 sebzés, közel | Erős, széles vállú oni baltával: csak fát gyűjt, de egyszerre sokat (12). Harcban gyenge. |
| Tanuki-bányász | munkás | 50 fa, 10 arany | 55 / 0 | 4 sebzés, közel | Alakváltó tanuki: aranyat és meteoritot bányászik. A bányák közelében, amíg nem harcol, álcázva dolgozik; néha egy-egy tárgyat is talál a mélyben (a leadóhelynél hagyja). |

**Az egységek képességei**

- **Buzogányroham** – Rövid, kemény roham.
- **Kapuőrzés** – Kapuőrként megáll egy épület mellett: nem mozdul, az épület +3 páncélt kap (legfeljebb +6).
- **Szélvihar** – Legyezővel kavart szélvihar: sebez és lassít.
- **Rókaillúzió** – Két rókaszellem-másolatot sző 20 másodpercre.
- **Lámpásfény-gyógyítás** – Gyógyítja a közeli sebesülteket.
- **Óriásdobbantás** – Földrengető dobbantás.
- **Tombolás** – Tombol: erősebben üt, védtelenebb.
- **Taiko-ritmus** – Harci dob: a közeli társak erősebben és gyorsabban harcolnak.
- **Védőfény** – A lámpás fénye védőburkot von a közeli társakra: 10 mp-ig 50 sebzést nyel el.
- **Villámcsapás** – Villám: nagy sebzés, rövid bénulás.

### Épületek (14)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Nian csarnok | 360 fa, 60 arany | Lámpásvivő, Favágó oni, Tanuki-bányász | Márványoszlopok, ívelt cseréptető, faragott nian-őrzők. Leadóhely; Lámpásvivőket, Favágó onikat és Tanuki-bányászokat nevel; fénye körül építhet a nép. csoda: Isteni szél (Isteni szél söpör végig a kijelölt ponton: 8 mp-re felére lassítja az ellenséget, a saját egységek pedig 20%-kal gyorsabbak. 2,5 perc töltődés.). |
| Papírlámpás | 25 fa | – | Olcsó lámpás: fénye bevilágít, tágítja az építési területet, és az Ooni egységek gyógyulnak benne. |
| Oni lakóház | 75 fa, 30 arany | – | Cseréptetős oni ház: +8 létszám. |
| Edzőudvar | 150 fa, 50 arany | Oni harcos, Kőhajító oni, Nian őrző, Taiko-dobos | Oni harcosokat, kőhajítókat, nian őrzőket és taiko-dobosokat képez. |
| Meselények szentélye | 160 fa, 80 arany, 40 meteorit | Kitsune-maszkos, Akahana, a Vörös Buzogány, Shiro, a Kapuőr, Karasu, a tengu bölcs | Kitsune-maszkosokat és hősöket képez. |
| Tengu-fészek | 160 fa, 70 arany | Tengu követ | Tengu követeket nevel. |
| Lámpáskert | 150 fa, 120 arany | Kapudöngető, Ō-oni | Az Ooni fejlesztései és ostromszíne. |
| Kapuőrző torony | 110 fa, 60 arany | – | Faragott nian-szoborral díszített őrtorony. |
| Vörös dódzsó | 180 fa, 100 arany | Tomboló oni | Tomboló onikat képez; a dódzsó dobjai erősebbek. |
| Kapuszentély | 180 fa, 120 arany | Kőkutya-őr, Lámpáspap | Kőkutya-őröket és lámpáspapokat nevel. |
| Szélcsúcs | 180 fa, 100 arany, 40 meteorit | Hollótengu-harcos, Viharhívó tengu | Hollótengu-harcosokat és viharhívókat nevel. |
| Torii-kapu | 120 fa, 60 arany | – | Kapupár egyik fele. Az elsőt lámpásfényben, a másodikat bárhová teheted, ahol épp látsz; a két kapu között az egységek átkelnek (jobb klikk a kapura, 2 mp, utána 8 mp töltődés). Kis lámpásfényt is ad. |
| Égkő | 75 meteorit | – | Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja. |
| Diplomácia csarnoka | 180 fa, 120 arany | – | Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után a nép épületei a munkásaid építési listáján jelennek meg. |

### Kutatások (18)

| Kutatás | Hol | Hatás |
|---|---|---|
| Messzefény | Papírlámpás | +2 csempe lámpásfény minden lámpásnál: több helyen lehet építeni, és több helyen gyógyulnak az egységek. |
| Harci dobok | Edzőudvar | A harci kedv 40%-kal lassabban fogy, a Taiko-dobos kétszer gyorsabban tölti. |
| Oni vérszomj | Edzőudvar | Az oni harcosok (Oni harcos, Tomboló oni, Ō-oni) ölésenként 20 életerőt kapnak vissza. |
| Tengu-tollak | Tengu-fészek | A repülő egységek +1 lőtávot és +10% sebességet kapnak. |
| Vasbuzogány | Lámpáskert | +2 sebzés a közelharcosoknak. |
| Vasbuzogány II. | Lámpáskert | +2 sebzés a közelharcosoknak. (II. szint) |
| Vasbuzogány III. | Lámpáskert | +2 sebzés a közelharcosoknak. (III. szint; a Felemelkedés után) |
| Lakkozott vért | Lámpáskert | +1 páncél az élő egységeknek. |
| Lakkozott vért II. | Lámpáskert | +1 páncél az élő egységeknek. (II. szint) |
| Lakkozott vért III. | Lámpáskert | +1 páncél az élő egységeknek. (III. szint; a Felemelkedés után) |
| Lámpásolaj | Lámpáskert | +1 látótáv minden egységnek. |
| Szélpárna | Lámpáskert | +15% sebesség a repülőknek. |
| Kettős kapu | Lámpáskert | Két Torii-kapupár is építhető. |
| Fonott hajítókötél | Lámpáskert | +2 sebzés a távolsági egységeknek. |
| Fonott hajítókötél II. | Lámpáskert | +2 sebzés a távolsági egységeknek. (II. szint) |
| Fonott hajítókötél III. | Lámpáskert | +2 sebzés a távolsági egységeknek. (III. szint; a Felemelkedés után) |
| Lámpás-mágia | Lámpáskert | A lámpásfényben álló varázslók 20%-kal erősebbek. |
| Kőkutya-hűség | Kapuszentély | A Kőkutya-őr mellett álló (2,5 csempén belüli) épület +2 páncélt kap. |

