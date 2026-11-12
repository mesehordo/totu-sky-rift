# A szövetséges népek – működés, egységek, épületek

*© Mesehordo – TOTU: Sky Rift. A fájlt a `python tools/gen_tortenet_fajok.py` állítja elő a játék adataiból.*

A szövetséges népek nem játszhatók önállóan: a főfajok a **Diplomácia csarnokában** köthetnek velük szövetséget (ára 150 arany és 50 meteorit), egyszerre egy néppel. Ezután a nép néhány épülete a saját bázison felépíthető, és ott képezhetők az egységei; a nép saját kutatásai is elérhetővé válnak.

| Nép | Jelmondat | Épület | Egység |
|---|---|---|---|
| Micéliák | Gyökérlábú gombanép | 3 | 5 |
| Pinguu | Jég, csont és vastag bőrök | 3 | 5 |
| Ghoblin | Kőnyűvő manók | 3 | 6 |
| Mhonyo | Kardkovács manók | 3 | 5 |
| Daresh | A darázsnép | 3 | 5 |
| Tokla | Fagyóriások és fagytörpék | 3 | 5 |
| Galóca | Tüskés kalapú gombaharcosok | 3 | 5 |
| Niil-elfek | A folyók elfjei | 3 | 5 |
| Tűz-elfek | A dűnék elfjei | 3 | 6 |
| Tendüür | Szárnyas erdei tündérek | 3 | 5 |
| Nila-uchana | Lótuszudvarok és szörnyidézők | 3 | 6 |
| Zhan-zhama | Apró nép, nagy kakasok | 3 | 5 |
| Shrissh-matah | Tollas rovarok rúnavárai | 3 | 5 |

## Micéliák

*Gyökérlábú gombanép*

Ezek a gyökérlábú gombalények élő fonalakkal kötik össze telepeiket, amelyek szívében egy óriási gomba lüktet. Míg a spóraszedők készleteket gyűjtenek és a pöfetegek őrzik a telepet, az Emlékfa gyökerei között régi társaik történetei pihennek.

**Háttér (FACTIONS_LORE.md):**

A **Mélykertek** népe: önálló gombaszemélyek, akiket a föld alatti fonalhálózat köt össze (a „Micéliák” munkacím – a kánon szándékosan nem ad végleges fajnevet). A *Grey Goo* „goo”-jához hasonlóan egy terjedő, összefüggő
élőlény részei.

**Mechanikák**
* **Spórafonal-hálózat**: az épületek csak akkor működnek (termelés, leadás, védelem), ha fonalakon keresztül
  kapcsolódnak egy **Szívgombához**. A hálózatot olcsó **Spórafonal**-csomópontokkal lehet kiterjeszteni;
  új épület csak a hálózat hatósugarába tehető. Elvágott ág = leálló épületek.
* **Bármely épület leadóhely**: a nyersanyag a legközelebbi *hálózatba kötött* épületbe vihető.
* **Szívgomba**: kicsi (2×2), olcsó főépület – ellátmányt ad, munkást nevel és **spóralövéssel védekezik**.
  Akárhány építhető belőle.
* **Összeolvadás**: 3 Pöfeteg egy *Óriáspöfeteggé* olvad össze.
* **Spórarobbanás**: a Pöfeteg halálakor mérgező spórafelhő sebzi a közeli ellenséget.
* A hálózat közelében az egységek gyorsan regenerálódnak.

| Egység | Szerep |
|---|---|
| Spóraszedő | munkás |
| Pöfeteg | olcsó közelharcos, halálakor spórarobbanás; 3 → Óriáspöfeteg |
| Tüskegomba-lövész | távolsági, lég+föld |
| Kalapos őr | páncélos tank |
| Álomgomba-sámán | *Gyökérkötés*: területen megbénítja az ellenséget |
| Óriáspöfeteg | összeolvasztott óriás |
| Lebegő pöfeteg | repülő, spóraeső |

Épületek: Szívgomba (HQ + védelem), Spórafonal, Termőtest (gyalogság), Tüskegomba-telep, Gyökérbarlang
(fejlesztés), Álomkert (sámán), Lebegőbölcső (repülők).

---

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Termőtest, Szívgomba, Emlékfa
- **Egységek:** Spóraszedő, Pöfeteg, Fonalhálós, Tüskegomba-lövész, Emlékőr

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Spóraszedő | munkás | 35 fa, 15 arany | 40 / 0 | 5 sebzés, közel | Gyűjtögető gombanép: készleteket hord a telepre. |
| Pöfeteg | közelharcos | 40 fa, 15 arany | 45 / 0 | 7 sebzés, közel | A telep pöfeteg-őre. |
| Fonalhálós | varázsló | 90 fa, 70 meteorit | 110 / 0 | 9 sebzés, 5 lőtáv | Élő fonalakkal köti meg és gyógyítja a társakat. |
| Tüskegomba-lövész | lövész | 85 fa, 30 arany | 80 / 0 | 10 sebzés, 5 lőtáv | Mérgező spóratüskéket lő, földi és légi célpontokra. |
| Emlékőr | közelharcos | 110 fa, 60 arany | 170 / 3 | 9 sebzés, közel | Az Emlékfa kalapos őre; a régi társak emlékét védi. |

**Az egységek képességei**

- **Hálózati gyűjtés** – A terhét bármelyik hálózatba kötött épületbe leadhatja.
- **Összeolvadás** – Három közeli Pöfeteg egyetlen Óriáspöfeteggé olvad.
- **Osztódás (3 ölés)** – Minden három legyőzött ellenség után kettéosztódik.
- **Fonalgyógyítás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (10 életerő).
- **Fonalháló** – Területre ható csapás: 4 mp alatt 24 sebzés, és lassít.
- **Osztódás (4 ölés)** – Minden négy legyőzött ellenség után kettéosztódik.
- **Kalapfal** – A kalap pajzzsá feszül: +4 páncél, -40% távolsági sebzés, de lassú.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Termőtest | 160 fa, 80 arany | Spóraszedő, Pöfeteg, Fonalhálós | Spóraszedőket, pöfetegeket és fonalhálósokat képez. |
| Szívgomba | 160 fa, 80 arany | Tüskegomba-lövész, Emlékőr | Tüskegomba-lövészeket és emlékőröket képez; a telep lüktető szíve. |
| Emlékfa | 150 fa, 120 arany | – | A micélia fejlesztések fája; gyökerei között a régi társak történetei pihennek. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Ősök emléke | Emlékfa | +30 életerő a micélia egységeknek. |
| Spórafonal | Emlékfa | +10% sebesség minden egységnek. |
| Szívdobbanás | Emlékfa | +1 életerő-visszatöltés másodpercenként a micélia egységeknek. |

## Pinguu

*Jég, csont és vastag bőrök*

A sarkvidéki pingvinharcosok jégből, csontból és vastag bőrökből építenek menedéket. A hómezőkön kis csapatokban mozognak, és olyan útvonalakon is átvezetik szövetségeseiket, ahol mások eltévednének.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Jégcsarnok, Árapályműhely, Őrláng-szentély
- **Egységek:** Jégdárdás, Hópajzsos, Rozmárlovas, Ár-apály sámán, Kajakos portyázó

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Jégdárdás | lándzsás | 70 fa, 25 arany | 85 / 0 | 11 sebzés, 5.5 lőtáv | Jéghegyű dárdát hajít; lassítja a célpontot. |
| Hópajzsos | lándzsás | 65 fa, 25 arany | 185 / 2 | 12 sebzés, közel | Jégpajzsa mögött falat alkot. |
| Rozmárlovas | lovas / hátas | 150 fa, 50 arany | 340 / 2 | 16 sebzés, közel | Rozmárháton tör át a hómezőn. |
| Ár-apály sámán | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Gyógyít, és jeges hullámot zúdít az ellenségre. |
| Kajakos portyázó | lövész | 50 fa, 15 arany | 110 / 1 | 9 sebzés, 3.5 lőtáv | Gyors portyázó; vízen át is utat talál a szövetségeseknek. |

**Az egységek képességei**

- **Jégfal** – Be- és kikapcsolható állás: sebesség -50%, páncél +3.
- **Hasoncsúszás** – 4 mp-re: sebesség +90%, sebzés +15%.
- **Jeges gyógyítás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (9 életerő).
- **Jeges hullám** – Területre ható csapás: 4 mp alatt 56 sebzés, és lassít.
- **Ösvényvezetés** – Ének: a közeli szövetségesek – sebesség +12%.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Jégcsarnok | 160 fa, 80 arany | Jégdárdás, Hópajzsos, Rozmárlovas | Jégdárdásokat, hópajzsosokat és rozmárlovasokat képez. |
| Árapályműhely | 160 fa, 80 arany | Ár-apály sámán, Kajakos portyázó | Ár-apály sámánokat és kajakos portyázókat képez. |
| Őrláng-szentély | 150 fa, 120 arany | – | A pinguu fejlesztések szentélye. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Ösvényvezetők | Őrláng-szentély | +10% sebesség minden egységnek. |
| Jégpáncél | Őrláng-szentély | +1 páncél a pinguu egységeknek. |
| Hidegtűrés | Őrláng-szentély | +30 életerő a pinguu egységeknek. |

## Ghoblin

*Kőnyűvő manók*

A kőnyűvő manók a hegyoldalból fejtik ki falvaik alapjait, és alacsony, meredek tetejű házaikat faragott kővel erősítik meg. Úgy ismerik a kő repedéseit, ahogy más népek a fa erezetét.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Rágókő bánya, Kőcipó malom, Fogadalomharang
- **Egységek:** Kőharapó, Sziklagörgető, Pajzsköves, Mészköpő, Kőhéjú öreg, Mészbombás manó

### Egységek (6)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Kőharapó | közelharcos | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Csákányos manó; a köveket is megrágja (épületek ellen erős). |
| Sziklagörgető | ostromgép | 200 fa, 100 arany | 280 / 2 | 38 sebzés, 7.5 lőtáv | Óriási sziklát görget a falakra. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv). |
| Pajzsköves | lándzsás | 65 fa, 25 arany | 190 / 3 | 12 sebzés, közel | Kőlap-pajzzsal és kőbunkóval harcol. |
| Mészköpő | lövész | 70 fa, 25 arany | 85 / 0 | 12 sebzés, 5 lőtáv | Égetett mészgolyót dob; a találat elvakít (lassít). |
| Kőhéjú öreg | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Kővé keményedett bőrű vén manó. |
| Mészbombás manó | közelharcos | 40 fa, 25 arany | 70 / 0 | 4 sebzés, közel | Égetett mésszel teli kosarat cipel: az ellenség közé lép, és felrobban – az épületeket is rombolja, a mészköd pedig elvakít (lassít). |

**Az egységek képességei**

- **Kőfal** – Be- és kikapcsolható állás: sebesség -50%, páncél +3.
- **Kődobbantás** – Csapás maga körül: 40 sebzés, és megbénít.
- **Mészrobbanás** – Felrobban: a közeli ellenségeket megsebzi és elvakítja (lassítja), az épületeket erősen rombolja – a manó elpusztul.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Rágókő bánya | 160 fa, 80 arany | Kőharapó, Sziklagörgető | Kőharapókat és sziklagörgetőket képez. |
| Kőcipó malom | 160 fa, 80 arany | Pajzsköves, Mészköpő, Kőhéjú öreg, Mészbombás manó | Pajzsköveseket, mészköpőket és kőhéjú öregeket képez. |
| Fogadalomharang | 150 fa, 120 arany | – | A ghoblin fejlesztések harangja. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Kőbőr | Fogadalomharang | +1 páncél a ghoblin egységeknek. |
| Repedésismeret | Fogadalomharang | +3 sebzés a ghoblin egységeknek. |
| Mészvakítás | Fogadalomharang | +1 lőtáv a távolsági egységeknek. |

## Mhonyo

*Kardkovács manók*

A kardkovács manók műhelyeiben a pengék mellett fogaskerekek, emelők és finom szerkezetek is készülnek. Mestereik egy kard súlypontját éppolyan gondosan állítják be, mint egy gép legapróbb alkatrészét.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Szélpenge udvar, Parázsszentély, Hárompenge kovácsda
- **Egységek:** Hajnalpenge, Szerkezet-íjász, Szikraőr, Kardkovács, Fogaskerék-gólem

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Hajnalpenge | közelharcos | 50 fa, 15 arany | 110 / 1 | 8 sebzés, közel | Villámgyors párbajozó; kétszer csap. |
| Szerkezet-íjász | lövész | 70 fa, 25 arany | 85 / 0 | 14 sebzés, 6.5 lőtáv | Felhúzós számszeríj; hosszú lőtáv. |
| Szikraőr | lándzsás | 65 fa, 25 arany | 180 / 2 | 12 sebzés, közel | Izzó pengéjű, nehézpajzsos őr. |
| Kardkovács | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 10 sebzés, közel | Javítja és élezi a társak fegyverét. |
| Fogaskerék-gólem | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Kovácsolt gép-óriás. |

**Az egységek képességei**

- **Szélvágás** – 4 mp-re: sebesség +50%, sebzés +40%.
- **Szikrapajzs** – Be- és kikapcsolható állás: sebesség -40%, páncél +3.
- **Élezés** – Ének: a közeli szövetségesek – sebzés +10%.
- **Foltozás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (8 életerő).
- **Gőzkitörés** – Csapás maga körül: 38 sebzés, és megbénít.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Szélpenge udvar | 160 fa, 80 arany | Hajnalpenge, Szerkezet-íjász | Hajnalpengéket és szerkezet-íjászokat képez. |
| Parázsszentély | 160 fa, 80 arany | Szikraőr, Kardkovács, Fogaskerék-gólem | Szikraőröket, kardkovácsokat és fogaskerék-gólemeket képez. |
| Hárompenge kovácsda | 150 fa, 120 arany | – | A mhonyo fejlesztések kovácsműhelye. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Kiegyensúlyozott penge | Hárompenge kovácsda | +2 sebzés a közelharcosoknak. |
| Szikrapáncél | Hárompenge kovácsda | +1 páncél a mhonyo egységeknek. |
| Gépjavítás | Hárompenge kovácsda | +60 életerő a gépeknek. |

## Daresh

*A darázsnép*

A darázsnép napmeleg vidékeken emel kaptárakat, amelyeket fonott díszek, festett minták és okkerszínű falak tesznek felismerhetővé. Szárnyas őreik magasból figyelik a környéket, és gyors rajokban csapnak le.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Agyagkaptár, Szárnyudvar, Gyantaműhely
- **Egységek:** Fullánclándzsás, Gyantapajzsos, Rajénekes, Darázsraj, Szárnyas íjász

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Fullánclándzsás | lándzsás | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Mérges fullánkú lándzsával döf. |
| Gyantapajzsos | lándzsás | 65 fa, 25 arany | 185 / 2 | 12 sebzés, közel | Borostyán-gyantapajzzsal véd. |
| Rajénekes | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Énekével gyorsítja a rajt, és gyógyít. |
| Darázsraj | repülő, repül | 110 fa, 35 arany, 40 meteorit | 120 / 0 | 8 sebzés, 1.5 lőtáv | Repülő darázsraj; gyorsan csap le. |
| Szárnyas íjász | repülő, repül | 110 fa, 35 arany, 40 meteorit | 120 / 0 | 12 sebzés, 5 lőtáv | A magasból nyilaz. |

**Az egységek képességei**

- **Gyantafal** – Be- és kikapcsolható állás: sebesség -40%, páncél +3.
- **Rajének** – Ének: a közeli szövetségesek – sebesség +12%, sebzés +5%.
- **Mézgyógyítás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (9 életerő).

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Agyagkaptár | 160 fa, 80 arany | Fullánclándzsás, Gyantapajzsos | Fullánclándzsásokat és gyantapajzsosokat képez. |
| Szárnyudvar | 160 fa, 80 arany | Rajénekes, Darázsraj, Szárnyas íjász | Rajénekeseket, darázsrajt és szárnyas íjászokat képez. |
| Gyantaműhely | 150 fa, 120 arany | – | A daresh fejlesztések műhelye. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Gyantapáncél | Gyantaműhely | +1 páncél a daresh egységeknek. |
| Méregfullánk | Gyantaműhely | +2 sebzés a daresh egységeknek. |
| Rajzás | Gyantaműhely | +15% sebesség a repülőknek. |

## Tokla

*Fagyóriások és fagytörpék*

A fagyóriások és fagytörpék közös népét messziről felismerni faragott, busószerű maszkjairól. A törpék műhelyei a hegy gyomrában húzódnak, az óriások pedig a havas hágókat és a telepek kapuit őrzik.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Maszkcsűr, Harangkapu, Zúzmaraműhely
- **Egységek:** Fagyóriás, Harangjáró, Hóvihar-busó, Zúzmaratörpe, Jégvető törpe

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Fagyóriás | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Jégbalták óriása; csapásai lelassítanak. |
| Harangjáró | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Harangszóval bátorít, és elűzi a gonoszt. |
| Hóvihar-busó | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Hóvihart kavar az ellenségre. |
| Zúzmaratörpe | lándzsás | 65 fa, 25 arany | 165 / 2 | 12 sebzés, közel | Jégbaltás, kerek pajzsos törpe. |
| Jégvető törpe | lövész | 70 fa, 25 arany | 85 / 0 | 12 sebzés, 5.5 lőtáv | Jégtömböket hajít. |

**Az egységek képességei**

- **Fagydobbantás** – Csapás maga körül: 36 sebzés, és megbénít.
- **Harangszó** – Ének: a közeli szövetségesek – sebzés +8%, páncél +1.
- **Rontásűzés** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (8 életerő).
- **Hóvihar** – Területre ható csapás: 5 mp alatt 70 sebzés, és lassít.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Maszkcsűr | 160 fa, 80 arany | Fagyóriás, Harangjáró, Hóvihar-busó | Fagyóriásokat, harangjárókat és hóvihar-busókat képez. |
| Harangkapu | 160 fa, 80 arany | Zúzmaratörpe, Jégvető törpe | Zúzmaratörpéket és jégvető törpéket képez. |
| Zúzmaraműhely | 150 fa, 120 arany | – | A tokla fejlesztések műhelye. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Busómaszk | Zúzmaraműhely | +2 sebzés a tokla egységeknek. |
| Fagyos fegyver | Zúzmaraműhely | +1 páncél a tokla egységeknek. |
| Hágóőrség | Zúzmaraműhely | +60 életerő a nagy testű egységeknek. |

## Galóca

*Tüskés kalapú gombaharcosok*

A Galócák tüskés kalapú, fogas gombaharcosok, akik a sűrű növényzetből rontanak a betolakodókra. Telepeik peremén már a földből előbújó tüskék is jelzik, meddig tart a türelmük.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Harapófészek, Spórakürt, Tüskekert
- **Egységek:** Fogaskalap, Tüskebunkós, Óriásgalóca, Spóraköpő, Gyökérgáncsoló

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Fogaskalap | közelharcos | 50 fa, 15 arany | 120 / 1 | 9 sebzés, közel | Fogas kalapú, mohó gombaharcos. |
| Tüskebunkós | lándzsás | 65 fa, 25 arany | 165 / 2 | 14 sebzés, közel | Két tüskés bunkóval csapkod. |
| Óriásgalóca | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Hatalmas, fogas gombaóriás. |
| Spóraköpő | lövész | 70 fa, 25 arany | 85 / 0 | 12 sebzés, 5.5 lőtáv | Mérges spórafelhőt köp. |
| Gyökérgáncsoló | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Gyökerekkel köti meg az ellenséget. |

**Az egységek képességei**

- **Lesből harapás** – 4 mp-re: sebesség +60%, sebzés +35%.
- **Kalaptaposás** – Csapás maga körül: 40 sebzés, és megbénít.
- **Gyökérgáncs** – Területre ható csapás: 4 mp alatt 24 sebzés, és lassít.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Harapófészek | 160 fa, 80 arany | Fogaskalap, Tüskebunkós, Óriásgalóca | Fogaskalapokat, tüskebunkósokat és óriásgalócákat képez. |
| Spórakürt | 160 fa, 80 arany | Spóraköpő, Gyökérgáncsoló | Spóraköpőket és gyökérgáncsolókat képez. |
| Tüskekert | 150 fa, 120 arany | – | A galóca fejlesztések kertje; tüskéi sebzik a közelébe érő ellenséget. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Tüskés talaj | Tüskekert | +1 páncél a galóca egységeknek. |
| Mérges spóra | Tüskekert | +2 sebzés a galóca egységeknek. |
| Gyors burjánzás | Tüskekert | +1 életerő-visszatöltés másodpercenként. |

## Niil-elfek

*A folyók elfjei*

A vízi elfek oszlopos szentélyei és faragott kőkapui a folyók és csatornák rendjéhez igazodnak. A víz számukra útvonal, településeik tengelye és szertartásaik központja.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Lótuszkikötő, Árvízszentély, Nádpalota
- **Egységek:** Papiruszlándzsás, Nádhajós íjász, Lótuszpajzsos, Árapálypap, Krokodilőr

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Papiruszlándzsás | lándzsás | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Nádpajzsos, hosszú lándzsás harcos. |
| Nádhajós íjász | lövész | 70 fa, 25 arany | 85 / 0 | 12 sebzés, 5.5 lőtáv | Gyorsan mozgó, nádcsónakos íjász. |
| Lótuszpajzsos | lándzsás | 65 fa, 25 arany | 180 / 2 | 12 sebzés, közel | Aranylótusz-pajzzsal véd. |
| Árapálypap | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Gyógyít, és árhullámot zúdít. |
| Krokodilőr | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Megszelídített óriáskrokodil. |

**Az egységek képességei**

- **Lótuszfal** – Be- és kikapcsolható állás: sebesség -40%, páncél +3.
- **Árapály-áldás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (10 életerő).
- **Árhullám** – Területre ható csapás: 3 mp alatt 48 sebzés, és lassít.
- **Halálforgás** – Csapás maga körül: 42 sebzés, és megbénít.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Lótuszkikötő | 160 fa, 80 arany | Papiruszlándzsás, Nádhajós íjász | Papiruszlándzsásokat és nádhajós íjászokat képez. |
| Árvízszentély | 160 fa, 80 arany | Lótuszpajzsos, Árapálypap, Krokodilőr | Lótuszpajzsosokat, árapálypapokat és krokodilőröket képez. |
| Nádpalota | 150 fa, 120 arany | – | A niil-elf fejlesztések palotája. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Csatornarend | Nádpalota | +10% sebesség minden egységnek. |
| Lótuszáldás | Nádpalota | +40 életerő a niil-elf egységeknek. |
| Áradás | Nádpalota | +2 sebzés a niil-elf egységeknek. |

## Tűz-elfek

*A dűnék elfjei*

A dűnék elfjei vörös tetők alatt, árnyékos udvarok köré építik telepeiket. Ismerik a homokviharok járását, és a tűz mágiáját éppúgy használják a műhelyeikben, mint a harcban.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Dűnepavilon, Délibábtorony, Parázskemence
- **Egységek:** Homokszablyás, Parázsíjász, Homokvihar-táncos, Hőszövő, Délibáb-csaló

### Egységek (6)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Homokszablyás | közelharcos | 50 fa, 15 arany | 110 / 1 | 10 sebzés, közel | Ívelt szablyával vág. |
| Parázsíjász | lövész | 70 fa, 25 arany | 85 / 0 | 13 sebzés, 5.5 lőtáv | Izzó hegyű nyilakat lő. |
| Homokvihar-táncos | közelharcos | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Pörgő pengés táncos; körben sebez. |
| Hőszövő | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Tűzfalat sző az ellenségre. |
| Délibáb-csaló | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Délibáb-másokat idéz, hogy megtévessze az ellent. |
| Délibáb-más | közelharcos, idézett | – | 40 / 1 | 5 sebzés, közel | Rövid életű délibáb-harcos. |

**Az egységek képességei**

- **Homokroham** – 4 mp-re: sebesség +70%, sebzés +25%.
- **Pengetánc** – Csapás maga körül: 30 sebzés.
- **Tűzfal** – Területre ható csapás: 4 mp alatt 96 sebzés.
- **Délibáb** – Lényt idéz 20 mp-re.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Dűnepavilon | 160 fa, 80 arany | Homokszablyás, Parázsíjász, Homokvihar-táncos | Homokszablyásokat, parázsíjászokat és homokvihar-táncosokat képez. |
| Délibábtorony | 160 fa, 80 arany | Hőszövő, Délibáb-csaló | Hőszövőket és délibáb-csalókat képez. |
| Parázskemence | 150 fa, 120 arany | – | A tűz-elf fejlesztések kemencéje. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Izzó nyílhegy | Parázskemence | +2 sebzés a távolsági egységeknek. |
| Homokvihar | Parázskemence | +15% sebesség a tűz-elf egységeknek. |
| Tűzálló páncél | Parázskemence | +1 páncél a tűz-elf egységeknek. |

## Tendüür

*Szárnyas erdei tündérek*

A szárnyas erdei tündérek élő fák ágai között laknak, ösvényeiket függő lámpások jelzik. Harcosaik testén tetovált minták futnak, és a lombkoronán át közelítik meg azokat, akik veszélyeztetik a ligeteiket.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Lámpásfa, Tövisudvar, Fényfészek
- **Egységek:** Lámpásőr, Szélhímző, Tövisvívó, Lombíjász, Tölgyőr

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Lámpásőr | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Lámpása fénye gyógyít, és bevilágít a ködbe. |
| Szélhímző | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Levélörvényt kavar. |
| Tövisvívó | közelharcos | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Tövispengével vív, tündérszárnyon szökell. |
| Lombíjász | lövész | 70 fa, 25 arany | 85 / 0 | 12 sebzés, 6 lőtáv | A lombok közül nyilaz. |
| Tölgyőr | óriás | 180 fa, 90 arany, 40 meteorit | 600 / 3 | 26 sebzés, közel | Élő, mozgó fa-óriás. |

**Az egységek képességei**

- **Lámpásfény** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (9 életerő).
- **Levélörvény** – Területre ható csapás: 4 mp alatt 64 sebzés.
- **Szökkenés** – 4 mp-re: sebesség +80%, sebzés +20%.
- **Gyökérdobbantás** – Csapás maga körül: 38 sebzés, és megbénít.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Lámpásfa | 160 fa, 80 arany | Lámpásőr, Szélhímző | Lámpásőröket és szélhímzőket képez. |
| Tövisudvar | 160 fa, 80 arany | Tövisvívó, Lombíjász, Tölgyőr | Tövisvívókat, lombíjászokat és tölgyőröket képez. |
| Fényfészek | 150 fa, 120 arany | – | A tendüür fejlesztések fészke. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Lombsuhanás | Fényfészek | +15% sebesség a tendüür egységeknek. |
| Tetovált védelem | Fényfészek | +1 páncél a tendüür egységeknek. |
| Lámpásfény | Fényfészek | +1 látótáv minden egységnek. |

## Nila-uchana

*Lótuszudvarok és szörnyidézők*

A kék bőrű, látható külső fül nélküli nép lótuszos udvarokat és díszes szentélyeket emel. A nága, a makara és a rakshasa náluk a telepek faragványain és a csatatéren is megjelenik, idézett lényként vagy szövetségesként.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Szörnyek csarnoka, Makara-medence, Lótuszudvar
- **Egységek:** Szörnykötő, Rakshasa, Makara-idéző, Nága szövetséges, Makara

### Egységek (6)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Szörnykötő | lándzsás | 65 fa, 25 arany | 165 / 2 | 12 sebzés, közel | Láncos bottal harcol; rakshasát hív maga mellé. |
| Rakshasa | óriás | 180 fa, 90 arany, 40 meteorit | 520 / 3 | 26 sebzés, közel | Megkötött, vörös szőrű démoni fenevad. |
| Makara-idéző | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Vízörvényt kavar, és gyógyít. |
| Nága szövetséges | lövész | 70 fa, 25 arany | 140 / 0 | 12 sebzés, 5.5 lőtáv | Kígyótestű lándzsás; mérgező dárdát vet. |
| Makara | lovas / hátas | 150 fa, 50 arany | 360 / 2 | 16 sebzés, közel | Elefántfejű vízi szörny. |
| Megkötött rakshasa | óriás, idézett | – | 260 / 3 | 26 sebzés, közel | Rövid ideig a szörnykötő mellett harcol. |

**Az egységek képességei**

- **Rakshasa hívása** – Lényt idéz 30 mp-re.
- **Tombolás** – Csapás maga körül: 40 sebzés, és megbénít.
- **Vízörvény** – Területre ható csapás: 4 mp alatt 64 sebzés, és lassít.
- **Lótuszgyógyítás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (10 életerő).
- **Makara-roham** – 4 mp-re: sebesség +60%, sebzés +30%.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Szörnyek csarnoka | 160 fa, 80 arany | Szörnykötő, Rakshasa | Szörnykötőket és rakshasákat képez. |
| Makara-medence | 160 fa, 80 arany | Makara-idéző, Nága szövetséges, Makara | Makara-idézőket, nága szövetségeseket és makarákat képez. |
| Lótuszudvar | 150 fa, 120 arany | – | A nila-uchana fejlesztések udvara. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Kötés ereje | Lótuszudvar | +3 sebzés a nagy testű egységeknek. |
| Nága-pikkely | Lótuszudvar | +1 páncél a nila-uchana egységeknek. |
| Lótuszszentség | Lótuszudvar | +40 életerő a nila-uchana egységeknek. |

## Zhan-zhama

*Apró nép, nagy kakasok*

A sötétbőrű, apró termetű nép mellett egy közönséges tyúk is nagynak látszik, egy kakas pedig már hátasnak is megfelel. Csirkepásztoraik és keltetőik köré épül a telep élete, a Tolljósok pedig madárszellemeket idéznek.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Fészekház, Kakasudvar, Keltetőszentély
- **Egységek:** Csirkepásztor, Tojásdobáló, Kakaslovas, Tolljós, Madárszellem

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Csirkepásztor | közelharcos | 35 fa, 10 arany | 80 / 1 | 9 sebzés, közel | Olcsó, gyors harcos – tyúkjai csipkedik az ellent. |
| Tojásdobáló | lövész | 50 fa, 15 arany | 65 / 0 | 10 sebzés, 5.5 lőtáv | Kemény tojásokat hajít. |
| Kakaslovas | lovas / hátas | 110 fa, 35 arany | 230 / 2 | 16 sebzés, közel | Lándzsás, kakasháton száguldó lovas. |
| Tolljós | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Madárszellemeket idéz, és gyógyít. |
| Madárszellem | repülő, repül | 110 fa, 35 arany, 40 meteorit | 90 / 0 | 10 sebzés, 3.5 lőtáv | Arany fényből szőtt madárszellem. |

**Az egységek képességei**

- **Kukorékolás** – Ének: a közeli szövetségesek – sebzés +10%.
- **Madárszellem hívása** – Lényt idéz 30 mp-re.
- **Tollgyógyítás** – Automatikusan gyógyítja a közeli sebesült szövetségeseket (9 életerő).

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Fészekház | 160 fa, 80 arany | Csirkepásztor, Tojásdobáló | Csirkepásztorokat és tojásdobálókat képez. |
| Kakasudvar | 160 fa, 80 arany | Kakaslovas, Tolljós, Madárszellem | Kakaslovasokat, tolljósokat és madárszellemeket képez. |
| Keltetőszentély | 150 fa, 120 arany | – | A zhan-zhama fejlesztések szentélye. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Gyors keltetés | Keltetőszentély | +10% sebesség minden egységnek. |
| Kakaskukorékolás | Keltetőszentély | +2 sebzés a zhan-zhama egységeknek. |
| Tollszellem | Keltetőszentély | +30 életerő a zhan-zhama egységeknek. |

## Shrissh-matah

*Tollas rovarok rúnavárai*

A száraz sziklavidék tollas rovarai tüskékkel és rúnákkal díszített termeszvárakban fészkelnek. A tűszúnyogok a levegőből portyáznak, a molnárka formájú Sziklafutók kampós lábakkal kapaszkodnak a köveken, a Rúnaszövők pedig jeleket vésnek a kaptárak agyagfalába.

### A szövetség

A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők a nép épületei, és képezhetők az egységei:

- **Épületek:** Tollvár, Tüskekeltető, Rúnaszellőző
- **Egységek:** Rúnaszövő, Sziklafutó, Pajzsbogár, Tűszúnyog, Tüskelövő

### Egységek (5)

| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |
|---|---|---|---|---|---|
| Rúnaszövő | varázsló | 70 fa, 25 arany, 50 meteorit | 80 / 0 | 6 sebzés, 4 lőtáv | Rúnákat vés: védő jelet és robbanó rúnát. |
| Sziklafutó | közelharcos | 50 fa, 15 arany | 110 / 1 | 9 sebzés, közel | Kampós lábakkal száguld a köveken át. |
| Pajzsbogár | lándzsás | 65 fa, 25 arany | 200 / 3 | 12 sebzés, közel | Tollas, vastag páncélú bogár. |
| Tűszúnyog | repülő, repül | 110 fa, 35 arany, 40 meteorit | 120 / 0 | 12 sebzés, közel | Tűhegyes ormányú repülő portyázó. |
| Tüskelövő | lövész | 70 fa, 25 arany | 85 / 0 | 13 sebzés, 5.5 lőtáv | Tollas tüskéket lő. |

**Az egységek képességei**

- **Védőrúnák** – Ének: a közeli szövetségesek – páncél +2.
- **Robbanó rúna** – Különleges képesség.
- **Kapaszkodás** – 4 mp-re: sebesség +70%, sebzés +15%.

### Épületek (3)

| Épület | Ár | Képez | Leírás |
|---|---|---|---|
| Tollvár | 160 fa, 80 arany | Rúnaszövő, Sziklafutó, Pajzsbogár | Rúnaszövőket, sziklafutókat és pajzsbogarakat képez. |
| Tüskekeltető | 160 fa, 80 arany | Tűszúnyog, Tüskelövő | Tűszúnyogokat és tüskelövőket képez. |
| Rúnaszellőző | 150 fa, 120 arany | – | A shrissh-matah fejlesztések rúnatornya. |

### Kutatások (3)

| Kutatás | Hol | Hatás |
|---|---|---|
| Rúnavésés | Rúnaszellőző | +2 sebzés a shrissh-matah egységeknek. |
| Tollas páncél | Rúnaszellőző | +1 páncél a shrissh-matah egységeknek. |
| Szikla-kapaszkodás | Rúnaszellőző | +15% sebesség a shrissh-matah egységeknek. |

