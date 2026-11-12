# Új fajok és szövetséges népek – terv jóváhagyásra

*© Mesehordo – TOTU: Sky Rift*

**Döntések (2026-11-04):** a Micéliák **csak szövetséges nép** lesznek (nem játszható főfaj); az Ooni **mindkét** mechanikát
megkapja (Kapuőrzők + Papírlámpások). A terv többi része jóváhagyva.

**Állapot (2026-11-05):** beépítve és gépi meccsekkel kipróbálva:
- az Aurethi Birodalom alapozása;
- a Huk’ucha (tavacskák, 9 egység + 3 hős, 8 épület, 3 irány);
- az Ooni (papírlámpások, kapuőrzők, 9 egység + 3 hős, 9 épület, 3 irány);
- a Diplomácia csarnoka mind a 7 főfajnál;
- a 13 szövetséges nép (a Micéliák a régi sprite-okkal).

Az egyéni csatában a gépnek megadható a szövetséges nép. A determinizmus és a mentés rendben.

**Hátra van:**
- a grafikák elkészültéig kódból rajzolt helyőrzők látszanak;
- az angol szövegek;
- a Gombanép-kampány átírása (a régi micélia-faj addig rejtve megmarad);
- a balansz-mérés az új fajokkal. Ami a te leírásodból jön, az **adott**. Amit én javaslok, az *dőlt betűs*, és szabadon átírható. A koncept-lapok a `docs/koncept/` mappában vannak.

---

## 1. Alapfrakciók

| Faj | Állapot | Különleges mechanika |
|---|---|---|
| Szürke elfek | megvan | energiamező (obeliszk) |
| Ursanok | megvan | hagyományos építés |
| Croftianok | megvan | kúszó réteg (élő talaj) |
| Wullofok | megvan | vándortábor, emuk adják a létszámot |
| Aurethi Birodalom | megvan, **bővül** | **alapozás** (Dune 2000-szerű) |
| **Huk’ucha** | **új** | **tavacskák**: csak elárasztott területre építhet |
| **Ooni** | **új** | *javaslat lent (2.3)* |

> **Nyitott kérdés – Micéliák:** a listádban a szövetséges népek közé kerültek. Ma teljes, játszható főfaj: van saját kampánya („Gombanép”), 14 egysége és 10 épülete. Lásd a kérdéseket a végén.

### 1.1 Aurethi Birodalom – alapozás

- A Fabrikátor új parancsa: **Alapozás**. Egy 2×2-es márvány-lapkő, *ára 10 arany, 3 mp alatt kész*. Egymás mellé rakva összefüggő padló lesz belőle.
- Ha egy épület **teljes egészében alapozáson áll**, teljes életerővel készül el.
- **Alapozás nélkül vagy csak részben alapozva** is felépíthető, de **fél életerővel** készül el. *A Fabrikátor utána feljavíthatja teljesre, mint egy sérült gépet.*
- *Az alapozás megmarad, ha az épület elpusztul, újra lehet rá építeni. Ellenség nem tudja lebontani.*
- A gépi ellenfél előbb alapoz, utána épít.

## 2. Új alapfrakciók

### 2.1 Huk’ucha – a békanép

**Tavacskák (adott):**
- Az építő lerak egy kis tavacskát tavirózsalevelekkel; *2×2 csempe, ára 15 fa*.
- Az egymás mellé tett tavacskák összefüggő tóvá olvadnak.
- A Huk’ucha **csak tavacskára** építhet: az épületnek teljes egészében tavon kell állnia.
- A tavon át lehet sétálni. A **békanép gyorsabb** rajta (*+30%*), **a többi nép lassabb** (*−30%*). A repülőkre nem hat.
- *Technikailag a Croftian kúszó rétegének testvére, csak lerakott és nem terjedő réteg.*

**Harcmodor (adott):** csúzli vagy tompa végű, apró kampóban végződő lapos kard, **pajzs nélkül**. A mágusok lábon járó tavi lényeket idéznek.

| Egység | Szerep |
|---|---|
| *Tóásó* | munkás: tavacskát rak, gyűjt, épít |
| **Csúzlis** | alap távolsági harcos (gyöngygolyó) |
| **Kampókardos** | közelharcos; *Horog: a célpontot 2 mezőnyit magához rántja* |
| **Tóidéző** | mágus; *Tavi lényt idéz (ideiglenes)* |
| *Tavi lény* | idézett, lábon járó víz-test; *ahol jár, rövid ideig tavacska marad utána* |
| *Gőtelovas* | gyors portyázó óriásgőtén |
| *Szitakötő-felderítő* | repülő felderítő |
| *Teknőshátas mozsár* | ostromgép: gyöngyágyú egy óriásteknősön |
| *Gyöngyóriás* | késői, nehéz egység |

*Hősök:* *Kvarra, a Tóanya* (mágus, árasztás) • *Bul’gok Kampókirály* (közelharc) • *Iriss Szivárványgyöngy* (támogató).

| Épület | Szerep |
|---|---|
| **Korallcsarnok** | főépület (a koncept-lap nagy csarnoka) |
| **Gyöngybuborék-otthon** | létszám |
| **Tóteremtő szentély** | mágusok, hősök |
| *Tavacska* | építési alap (lásd fent) |
| *Nádbarakk* | Csúzlis, Kampókardos |
| *Gőteistálló* | Gőtelovas, Teknőshátas mozsár |
| *Gyöngyműhely* | fejlesztések |
| *Korallőr* | védőtorony |

*Fejlődési irányok:*
- **Árvíz:** a tavak maguktól is terjednek, az ellenséges épületek alatt is.
- **Gyöngy:** gazdaság és pontos csúzlik.
- **Mélység:** nagy idézett lények.

### 2.2 Ooni – onik és nianok

**Adott:** buzogányos oni harcosok, négylábú nian őrzők, szárnyas tengu követek. A csarnokok márványoszlopai fölé ívelt cseréptető borul, a kapuknál faragott őrzők és papírlámpások állnak.

| Egység | Szerep |
|---|---|
| *Lámpásvivő* | munkás (kis oni) |
| **Oni harcos** | alap közelharcos, buzogány |
| **Nian őrző** | négylábú, kitartó; *épülethez köthető (lásd 2.3)* |
| **Tengu követ** | repülő mágus / felderítő |
| *Kőhajító oni* | távolsági (kőzápor) |
| *Kitsune-maszkos* | illúziókat szövő mágus |
| *Kapudöngető* | ostromgép, onik tolják |
| *Ō-oni* | óriás oni, késői nehéz egység |

*Hősök:* *Akahana, a Vörös Buzogány* • *Shiro, a Kapuőr nian* • *Karasu, a tengu bölcs*.

| Épület | Szerep |
|---|---|
| **Nian csarnok** | főépület |
| **Oni lakóház** | létszám |
| **Meselények szentélye** | mágusok, hősök |
| *Edzőudvar* | Oni harcos, Kőhajító |
| *Tengu-fészek* | repülők |
| *Lámpáskert* | fejlesztések |
| *Kapuőrző torony* | védőtorony |
| *Ostromszín* | Kapudöngető, Ō-oni |

*Fejlődési irányok:*
- **Vörös buzogány:** oni roham.
- **Kapuőrzők:** nian védelem.
- **Tengu szél:** légierő és mágia.

### 2.3 Ooni-mechanika – javaslat (válassz)

- **A) Kapuőrzők** *(ajánlott)*: a Nian őrző egy épülethez köthető. Ott élő kapuőrként áll, az épület páncélt kap, a nian magától védi. Ha az épületet támadják, a nianok gyorsan odaérnek.
- **B) Papírlámpások:** a munkás olcsó lámpásokat rak le. A lámpások fénye bevilágít a ködbe, és tágítja az építési területet; az Ooni egységek a fényben gyógyulnak.
- **C) Mindkettő** (a lámpás egyszerűbb változatban).

## 3. Szövetséges népek és a Diplomácia csarnoka

**Adott:**
- A **Diplomácia csarnoka** minden főfajnál megépíthető.
- Ott upgrade-szerűen **egy** szövetséges nép választható, véglegesen.
- Ezután megjelenik a nép **3 épülete**. Közülük **2 egységet gyárt**, **1 fejlesztéseket** ad.
- Népenként **5 egység** van: 3 a koncept-lapról, *2 az én javaslatom*.

*Javaslat:*
- A szövetség megkötése *150 arany + 50 meteorit*.
- Mind a 13 nép mindenkinek elérhető.
- A szövetséges épületeket a saját munkásaid építik. Ugyanaz a nép két játékosnál is lehet.
- A gépi ellenfél a faja és az iránya alapján választ, és az egyéni csata beállításainál előre megadható neki.

A táblázatban az egység épület szerinti csoportokban szerepel (melyik épület gyártja), a ⚙ jelöli a fejlesztő épületet. A **vastag** név a koncept-lapról jön, a *dőlt* az én javaslatom.

| Nép | 1. gyártó épület | 2. gyártó épület | ⚙ Fejlesztés | Jelleg |
|---|---|---|---|---|
| **Pinguu** | Jégcsarnok: **Jégdárdás**, **Hópajzsos**, *Rozmárlovas* | Árapályműhely: **Ár-apály sámán**, *Kajakos portyázó* | Őrláng-szentély: *Ösvényvezetők* (a szövetségesek gyorsabbak nehéz terepen), *Jégpáncél*, *Hidegtűrés* | vezetés, mozgás |
| **Ghoblin** | Rágókő bánya: **Kőharapó**, *Sziklagörgető* (ostrom) | Kőcipó malom: **Pajzsköves**, **Mészköpő**, *Kőhéjú öreg* | Fogadalomharang: *Kőbőr*, *Repedésismeret* (+sebzés épületek ellen), *Mészvakítás* | kő, ostrom |
| **Mhonyo** | Szélpenge udvar: **Hajnalpenge**, *Szerkezet-íjász* | Parázsszentély: **Szikraőr**, **Kardkovács**, *Fogaskerék-gólem* | Hárompenge kovácsda: *Kiegyensúlyozott penge*, *Szikrapajzs*, *Gépjavítás* | kovácsolt fegyver, gépek |
| **Daresh** | Agyagkaptár: **Fullánclándzsás**, **Gyantapajzsos** | Szárnyudvar: **Rajénekes**, *Darázsraj* (repülő), *Szárnyas íjász* (repülő) | Gyantaműhely: *Gyantapáncél*, *Méregfullánk*, *Rajzás* | gyors rajok, magasból |
| **Tokla** | Maszkcsűr: **Fagyóriás**, **Harangjáró**, *Hóvihar-busó* | Harangkapu: **Zúzmaratörpe**, *Jégvető törpe* | Zúzmaraműhely: *Busómaszk* (ijesztés), *Fagyos fegyver* (lassít), *Hágóőrség* | fagy, kapuvédelem |
| **Galóca** | Harapófészek: **Fogaskalap**, **Tüskebunkós**, *Óriásgalóca* | Spórakürt: **Spóraköpő**, *Gyökérgáncsoló* (megköti az ellenséget) | Tüskekert: *Tüskés talaj* (a telep körül sebez), *Mérges spóra*, *Gyors burjánzás* | lesből támadás, tüskék |
| **Micéliák** | Termőtest: **Spóraszedő**, **Pöfeteg**, *Fonalhálós* | Szívgomba: **Tüskegomba-lövész**, *Emlékőr* | Emlékfa: *Ősök emléke*, *Spórafonal* (gyorsabb gyűjtés), *Szívdobbanás* (gyógyítás) | kitartás, gazdaság |
| **Niil-elfek** | Lótuszkikötő: **Papiruszlándzsás**, *Nádhajós íjász* | Árvízszentély: **Lótuszpajzsos**, **Árapálypap**, *Krokodilőr* | Nádpalota: *Csatornarend* (gyorsabbak víz mellett), *Lótuszáldás*, *Áradás* | víz, rend |
| **Tűz-elfek** | Dűnepavilon: **Homokszablyás**, **Parázsíjász**, *Homokvihar-táncos* | Délibábtorony: **Hőszövő**, *Délibáb-csaló* (illúzió) | Parázskemence: *Izzó nyílhegy*, *Homokvihar*, *Tűzálló páncél* | tűz, illúzió |
| **Tendüür** | Lámpásfa: **Lámpásőr**, **Szélhímző** | Tövisudvar: **Tövisvívó**, *Lombíjász*, *Tölgyőr* (élő fa) | Fényfészek: *Lombsuhanás* (erdőben rejtve), *Tetovált védelem*, *Lámpásfény* | erdő, repülés |
| **Nila-uchana** | Szörnyek csarnoka: **Szörnykötő**, *Rakshasa* | Makara-medence: **Makara-idéző**, **Nága szövetséges**, *Makara* | Lótuszudvar: *Kötés ereje*, *Nága-pikkely*, *Lótuszszentség* | szörnyidézés |
| **Zhan-zhama** | Fészekház: **Csirkepásztor**, *Tojásdobáló* | Kakasudvar: **Kakaslovas**, **Tolljós**, *Madárszellem* (idézett) | Keltetőszentély: *Gyors keltetés*, *Kakaskukorékolás* (bátorítás), *Tollszellem* | sok olcsó egység, lovasság |
| **Shrissh-matah** | Tollvár: **Rúnaszövő**, **Sziklafutó**, *Pajzsbogár* | Tüskekeltető: **Tűszúnyog** (repülő), *Tüskelövő* | Rúnaszellőző: *Rúnavésés*, *Tollas páncél*, *Szikla-kapaszkodás* | rúnák, sziklán át |

## 4. A koncept-képek felhasználása (javaslat)

- A koncept-lapokról kivágom az alakokat és az épületeket. A **szövetséges és az új egységek arcképei** és **ikonjai** ezekből lesznek. A pályán a mostani, kódból rajzolt stílusú sprite-ok maradnak.
- Az új **unciális betűkészlet** (`egszakadas_uncialis.ttf`) a **címekben** lesz: főmenü, ablakcímek, a HUD-on a név, a felső cím. *A hosszú szövegek a jól olvasható betűvel maradnak.*

## 5. Sorrend a jóváhagyás után

1. Aurethi Birodalom-alapozás.
2. Diplomácia csarnoka és a szövetséges-rendszer: választás, épület-feloldás, gépi döntés, beállítás.
3. A 13 nép adatai és sprite-jai.
4. Huk’ucha: tavacska-mechanika, egységek, épületek, hősök, irányok, gépi ellenfél.
5. Ooni ugyanígy.
6. Arcképek a koncept-lapokból; grafikai lista és leírások frissítése; balansz-mátrix; angol szövegek.
