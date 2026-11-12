# A fajok (2. változat)

*© 2026 Mesehordo – a fajok világa és leírásai: minden jog fenntartva (lásd [LICENSE.md](../LICENSE.md)). Ez a fájl a játékadatokból készül: `python tools/gen_faction_doc.py`.*

## Közös szabályok

* **Nyersanyagok:** fa, kő és meteorit (étel nincs). A meteorit ritka – a magasabb tech és a hősök kapuja.
* **Létszám:** minden egység létszámot foglal (a nagy vadak és gépek többet, pl. mamut 6), a korlát legfeljebb 200. Hogy mi növeli, az fajonként más (lásd a táblázatot).
* **Hősök:** minden faj saját hősidéző épületéből **3 féle hőst** hívhat, egyszerre legfeljebb hármat. A hős 10 szintig fejlődik, szintenként képességpontot kap; 4 képessége van (3 háromszintű + egy végső a 6. szinttől), és 6 tárgyat visel. Az elesett hős az oltárban újraéleszthető.

* **Felemelkedés és fejlődési irányok:** amint a főépületen kívül 3 kész épületed van, a nép felemelkedik – ingyen, magától. Egy ablakban **végleg** választasz a faj 3 iránya közül: mindegyik saját épületet, egységeket és egy szabályt nyit meg, a másik kettő zárva marad. A hadjáratban nincs irányzár.

| Faj | Egyedi mechanika | Létszám nő | Hősidéző |
|---|---|---|---|
| Szürke elfek | Energiamező • pajzs • önmagától épülő templomok | Napobeliszkek | Napkapu |
| Ursanok | Helyőrség • tanyák • javítás • pajzsfal | Tanyák | Esküfa |
| Croftianok | Mélység • lárvák • mutáció • fertőzés | Szemhordozók (egységek) | Kórus-kút |
| Wullof | Nomád táborok • falkaerő • vadászzsákmány | Emuk (Emukarámban tenyészthetők, karámonként legfeljebb 5) | Ősök köre |
| Micéliák | Spórafonal-hálózat • szívgombák • összeolvadás | A hálózat: minden összekötött épület +1 | Emlékfa |
| Vasbirodalom | Lélekidézés • gépjavítás • ostrom • lélekreaktorok | Lélekreaktorok | Panteon |

## Szürke elfek

**Kevés, de tökéletes.** Elit, drága egységek töltődő pajzzsal; Fénylépés (teleport) a harcosoknál. Az új egységek a gyülekezőponthoz legközelebbi Napobeliszknél jelennek meg (beidézés). **Energiamező és pajzs.** Az épületek csak Napobeliszk mezőjében működnek; a Kőfaragó lerakja az alapot, a templom magától nő ki, a munkás azonnal továbbmegy. Minden egységnek töltődő pajzsa van.

*Létszám:* Napobeliszkek.  *A varázslatok ára:* energia (mana).

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Kőfaragó | 30 fa, 20 kő | 1 | Fénylépés | Munkás. Megidézi az épületeket, majd visszatér gyűjteni. |
| Tollas harcos | 60 fa, 20 kő | 2 | Fénylépés | Obszidiánpengés buzogánnyal harcoló elit gyalogos. |
| Jáde-íjász | 80 fa, 20 kő | 2 | Jáde-célzás | Távolsági egység, földi és légi célpontokra is lő. |
| Raptorlovas *[Jaguár-harcosok]* | 95 fa, 30 kő | 3 | Vágta, Fénylépés | Farkatlan, tollas gyíkon lovagló gyors lovas. Bónusz a távolsági egységek ellen. |
| Napszóró pap *[Nap temploma]* | 70 fa, 60 meteorit, 20 kő | 2 | Napvihar | Meteoritmágus. Napvihart idéz az ellenségre. |
| Quetzal *[Nap temploma]* | 85 meteorit, 85 fa, 45 kő | 4 | Zuhanórepülés | Tollas, repülő dínó. Mindenre lecsap. |
| Tollaskoronás rex *[Jaguár-harcosok]* | 165 kő, 120 meteorit, 145 fa | 6 | Tollkorona-üvöltés | Díszes tollkoronás óriásdínó, hátán íjászokkal. Területi harapás. |
| Csillagdárdás | 75 fa, 15 meteorit, 25 kő | 2 | Csillagdárda | Csillagérc-hegyű dárdákat hajít – messzire hord, légi célpontokra is. |
| Obszidián-vető *[Csillagvizsgálók]* | 200 fa, 120 kő | 4 | Napfókusz | Kőfaragók ostromgépe: obszidiántömböket hajít, épületek ellen különösen erős. |
| Jádeőr *(idézett / mutációval)* | – | 0 | Jádetest (passzív) | Életre keltett jádeszobor (idézett). |
| Nap főpapja *[Nap temploma]* | 80 fa, 40 kő, 60 meteorit | 2 | Napfény-áldás | A Nap főpapja: folyamatosan gyógyítja a körülötte harcolókat. |
| Jaguár-orvlövész *(idézett / mutációval)* *[Jaguár-harcosok]* | 90 fa, 30 kő | 2 | Les | Jaguárbőrös vadász: az erdőből lő, lesállásban halálos. |
| Csillagjós *[Csillagvizsgálók]* | 90 fa, 30 kő, 90 meteorit | 2 | Meteorhívás | A csillagvizsgálók mestere: meteorzáport hív le messziről. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Nap temploma** | Papokkal támogatott tömeghadsereg | Naptemplom | Nap főpapja, Napszóró pap, Quetzal | Minden élő egységed folyamatosan gyógyul (+1 regeneráció). Főpapok gyógyítanak, napszóró papok égetnek, és a quetzalok az égből csapnak le. |
| **Jaguár-harcosok** | Vadászat, lesből támadás | Raptorkarám | Jaguár-orvlövész, Raptorlovas, Tollaskoronás rex | Az erdőben a gyalogosaid láthatatlanok, a közelharcosok +2 sebzést kapnak. Jaguár-orvlövészek, raptorlovasok és tollas rexek. |
| **Csillagvizsgálók** | Távolsági ostrom, lassú őrlés | Csillagvizsgáló | Obszidián-vető, Csillagjós | +1 lőtáv minden távolsági egységnek. Obszidián-vetők és csillagjósok meteorokat hívnak messziről. |

### Hősök

**Szelai** – Rezonanciaépítész, akit a sors parancsnokká tett. Hálózatot épít és bont; Napvihart idéz.

* **Napvihar** (Q): Izzó meteorpor-zápor.
* **Rezonancia** (W): A közeli szövetségesek sebzése nő.
* **Leválasztás** (E): Rezonanciacsapás, amely egy célpontot kiszakít a hálózatából.
* **A Kilenc Lépcső őrei** (R) *(végső, 6. szinttől)*: Végső képesség: négy tollas harcos-szellem.

**Itzcoatl** – A Kilenc Lépcső tollas hadura. Közelharcos, a sereg élén áll.

* **Obszidián-dárda** (Q): Hajított obszidián-dárda: nagy sebzés, lassítás.
* **Földrengető csapás** (W): A hadúr a földbe sújt: a körülötte állókat megsebzi és megbénítja.
* **A hadúr jelenléte** (E): A közeli szövetségesek páncélja és sebzése nő.
* **Quetzlaru alakja** (R) *(végső, 6. szinttől)*: Végső képesség: a tollas kígyóisten ereje – kétszeres sebzés, +6 páncél.

**Xochi** – A rezonancia mestere: jádeőröket ébreszt és gyógyít.

* **Rezonanciahullám** (Q): Jádeszínű rezgés tépi szét a területet.
* **Harmónia** (W): A közeli szövetségesek életereje és pajzsa gyorsabban töltődik.
* **Jádeőr** (E): Élő jádeszobrot ébreszt, amely a hívójáért harcol.
* **Napkorona** (R) *(végső, 6. szinttől)*: Végső képesség: a területen minden szövetséges gyógyul.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Napszentély | 400 kő, 100 fa | Lépcsős piramis, főépület. Leadóhely, Kőfaragókat képez. |
| Napobeliszk | 75 kő | +8 létszám. Energiamezőt sugároz – az elf épületek csak ebben működnek. |
| Harcosok csarnoka | 125 fa, 75 kő | Tollas harcosokat és jáde-íjászokat képez. |
| Obszidián műhely | 100 fa, 100 kő | Fegyver- és pajzsfejlesztések. |
| Raptorkarám | 180 fa, 120 kő | Raptorlovasokat és tollaskoronás rexeket nevel. |
| Csillagvizsgáló | 150 kő, 50 meteorit | Napszóró papokat képez, a csillagok tudását kutatja. |
| Tollfészek-piramis | 150 fa, 150 kő, 100 meteorit | Quetzalokat nevel. |
| Jádeoszlop | 125 kő, 25 fa | Védőtorony, földi és légi célpontokra. |
| Napkapu | 180 fa, 100 kő | Hősidéző templomkapu: Szelai, Itzcoatl és Xochi. |
| Naptemplom | 160 fa, 160 kő, 40 meteorit | Aranyozott lépcsős templom: a Nap főpapjai és napszóró papok. |

## Ursanok

**A fal és az eskü.** Lassú, kemény gyalogság, formációk (Pajzsfal, Dárdafal), rúnakövek, cölöpfal. **Helyőrség és tanyák.** A Nagyterem, a Hosszúház, a Harcosok háza és az Őrtorony helyőrséget fogad: a bent lévők védve vannak, és mindegyikük erősíti az épület lövéseit (riadóra a telepesek is behúzódnak). A Telepes építéskor bemászik a félkész épületbe: bent védett, és csak készen jön ki (egy épületen egyszerre egy telepes dolgozik); javít is. Az ursanok nem viselnek sisakot, legfeljebb ágyékkötőt; nagy, lapos deszkapajzsuk és viking–jurta épületeik vannak.

*Létszám:* Tanyák.  *A varázslatok ára:* nincs ára – csak újratöltés.

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Telepes | 30 fa, 20 kő | 1 | Helyőrség | Munkás. Épít (a végéig ott marad), javít, gyűjt. |
| Pajzshordozó | 55 fa, 20 kő | 2 | Pajzsfal, Helyőrség | Az ursan sereg gerince. Pajzsfal: a távolsági sebzés nagy részét elnyeli. |
| Szigonyvető | 80 fa, 20 kő | 2 | Helyőrség, Horgonyszigony | Távolsági, bónusz a nagy testű célpontok ellen; légi célpontra is. |
| Medvebőrös berserker *[Berserker-klán]* | 75 fa, 25 meteorit, 25 kő | 2 | Helyőrség, Vérszomj | Gyors, nagy sebzésű közelharcos. |
| Rúnapap | 55 fa, 50 meteorit, 20 kő | 2 | Rúnás gyógyítás, Helyőrség, Rúnakő | Gyógyító. Automatikusan foltozza a sebesült harcosokat. |
| Kardfogú lovas *[Tundra hívása]* | 130 fa, 45 kő | 3 | Roham | Kardfogú tigrisen lovagló gyors harcos. Roham képesség. |
| Viharholló *[Tundra hívása]* | 115 fa, 50 meteorit, 35 kő | 2 | Zuhanó csapás | Óriás holló. Felderítő és légvédelem. |
| Harci mammut *[Tundra hívása]* | 345 fa, 155 kő, 75 meteorit | 6 | Taposás | Gyapjas mamut, hátán kőhajítókkal. Terület-sebzés, bónusz épületek ellen. |
| Dárdaőr | 70 fa, 20 kő | 2 | Helyőrség, Dárdafal | Hosszú dárdás védő – lovasság és nagy vadak ellen kiváló. |
| Ostromszán *[Rúnakovácsok]* | 215 fa, 110 kő | 3 | Telepítés | Szánra szerelt kőhajító, épületek ellen. |
| Kardfogú tigris *(idézett / mutációval)* | – | 0 | Szellemtest (passzív) | Bjarka hívására érkező vadállat (idézett). |
| Rúnakő *(idézett / mutációval)* | – | 0 | Rúnakő (passzív) | Rúnapap által állított kő (idézett). Aurája véd és gyógyít. |
| Medvealakváltó *[Berserker-klán]* | 150 fa, 60 kő, 40 meteorit | 4 | Medveüvöltés, Vérszomj | Az ursan teljesen medvévé változik: hatalmas karmok, rettentő üvöltés. |
| Harci kürtös *[Berserker-klán]* | 80 fa, 40 kő | 2 | Harci kürt, Helyőrség | Kürtszava felszítja a harcosokat: +15% sebzés, +10% sebesség a közelben. |
| Rúnaóriás *[Rúnakovácsok]* | 220 kő, 80 meteorit | 5 | Taposás | Rúnákkal életre keltett kőóriás: lassú, de épületeket zúz. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Berserker-klán** | Korai, nyers közelharci nyomás | Vérfogadalom-csarnok | Medvebőrös berserker, Medvealakváltó, Harci kürtös | Vérfogadalom: a közelharcosok annál erősebbek, minél sebesültebbek (+60% sebzésig). Berserkerek, medvévé változó alakváltók és harci kürtösök. |
| **Rúnakovácsok** | Erőd, technológia, késői erő | Rúnaszentély | Rúnaóriás, Ostromszán | Rúnákkal vésett falak és tornyok: +3 páncél minden épületnek, +6 toronysebzés. Rúnaóriások és ostromszánok törik át a frontot, amikor már minden megvan. |
| **Tundra hívása** | Nagytestű vadak, kevés, de hatalmas egység | Vadak karáma | Kardfogú lovas, Harci mammut, Viharholló | Mamutok, kardfogú lovasok és viharhollók. A vadállatok és a lovasság +60 életerőt és gyorsabb regenerációt kapnak. Lassú, nehéz ostrom. |

### Hősök

**Ragna Kőváll** – Egy határerőd parancsnoka. A pajzsa először a mellette állót védi.

* **Pajzsfal** (Q): A pajzs először a mellette állót védi: kevesebb távolsági sebzés, több páncél.
* **Az eskü súlya** (W): A közeli szövetségesek páncélja nő.
* **Pajzsroham** (E): Ragna nekiront egy ellenségnek: sebzi és rövid időre megbénítja.
* **Hosszú tél** (R) *(végső, 6. szinttől)*: Végső képesség: fagyos vihar, amely lassít és sebez.

**Hjalmar Fagyjós** – A jégkorszak emlékét őrző jós: fagy, vihar és lassítás.

* **Jégszilánk** (Q): Fagyos szilánk: sebez és erősen lassít.
* **Hóvihar** (W): Jeges vihar csap le a területre.
* **Téli bundák** (E): A közeli szövetségesek páncélja és regenerációja nő.
* **Megfagyott idő** (R) *(végső, 6. szinttől)*: Végső képesség: minden közeli ellenség jégbe dermed.

**Bjarka, a Vadak Ura** – Vadász és vadidomár: kardfogú tigriseket és mamutokat hív harcba.

* **Kardfogú hívása** (Q): Kardfogú tigrist hív a hegyekből.
* **A falka ereje** (W): A közeli szövetségesek (és vadak) sebzése nő.
* **Vadászszigony** (E): Szigonyt vet egy ellenségbe: sebzi és rövid időre megbénítja.
* **Mamutcsorda** (R) *(végső, 6. szinttől)*: Végső képesség: két harci mamut csatlakozik 40 másodpercre.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Nagyterem | 300 fa, 150 kő | Faragott hosszúcsarnok, főépület. Leadóhely, Telepeseket képez. |
| Hosszúház | 100 fa | Erődített hosszúház: 4 fős helyőrség, riadóban a telepesek menedéke. |
| Fatelep | 75 fa | Leadóhely fának, kőnek és meteoritnak. |
| Harcosok háza | 150 fa, 50 kő | Pajzshordozókat, szigonyvetőket és berserkereket képez. |
| Rúnakovács | 150 fa, 150 kő | Rúnapapokat képez, fegyvert és páncélt fejleszt. |
| Vadak karáma | 265 fa, 135 kő | Kardfogú lovasokat és harci mamutokat nevel. |
| Sasszirt | 150 fa, 100 kő, 50 meteorit | Viharhollókat nevel. |
| Őrtorony | 100 fa, 75 kő | Fa-kő őrtorony íjászokkal. |
| Esküfa | 180 fa, 100 kő | Ősi kőris, amely alatt a jarlok esküt tesznek: Ragna, Hjalmar és Bjarka. |
| Tanya | 130 fa, 30 kő | Árpaföld, ólak és szénapajták: a jarlság népe innen él. +10 létszám. |
| Cölöpfal | 20 fa | Hegyes karókból álló falszakasz – elzárja az utat. Olcsó, gyorsan épül; a telepesek javítják. |
| Vérfogadalom-csarnok | 220 fa, 80 kő | Itt teszik le a berserkerek a vérfogadalmat. |
| Rúnaszentély | 240 kő, 60 meteorit | Rúnakőkör kovácsműhellyel: rúnaóriások és ostromszánok. |

## Croftianok

**A raj.** Olcsó, gyors egységek, lesből támadás: a Beásással láthatatlanok és gyorsan gyógyulnak (csak felfedő – Szemhordozó, torony – vagy a nagyon közeli ellenség látja őket). **Mélység, lárvák, mutáció, fertőzés.** Minden egység a Kaptár lárváiból kel ki, párhuzamosan; a mélységen gyógyulnak és gyorsabbak. A Karmos gubóban Páncélbogárrá mutálódik; a mélységen elesett élő ellenségből Karmos kelhet ki.

*Létszám:* Szemhordozók (egységek).  *A varázslatok ára:* életerő (vérmágia).

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Csápszolga | 30 fa | 1 | Beásás | Munkás. Épületté alakul át (elfogy). |
| Karmos | 30 fa, 20 kő | 1 | Mutáció: Páncélbogár, Beásás | Olcsó, gyors rajegység. Egy lárvából kettő kel ki. |
| Tüskeköpő | 65 fa, 25 kő | 2 | Tüskezápor, Beásás | Csonttüskéket köp, földi és légi célpontokra. |
| Savköpő | 150 fa, 50 meteorit, 50 kő | 3 | Savtócsa | Élő tüzérség. Messzire köpött savhólyagok, terület-sebzés, bónusz épületek ellen. |
| Agyfaló *[Kórus]* | 80 fa, 75 meteorit, 20 kő | 2 | Őrület köde | Pszionikus rém. Őrület ködét bocsátja az ellenségre. |
| Szárnyas rém | 140 fa, 50 meteorit, 35 kő | 2 | Ragadozó zuhanás | Hártyás szárnyú repülő ragadozó. |
| Csápos horror *[Kórus]* | 190 kő, 100 meteorit, 160 fa | 6 | Csápsöprés | A mélység óriása. Csápjaival egyszerre több ellenfelet zúz. |
| Páncélbogár *(idézett / mutációval)* *[Páncél]* | 75 kő, 25 meteorit, 50 fa | 2 | Kitinroham, Beásás | Karmosból mutálódott, páncélos rohamállat. Csak mutációval jön létre. |
| Gubó *(idézett / mutációval)* | – | 0 | Gubó (passzív) | Mutálódó egység – sérülékeny, de páncélos burok. |
| Szemhordozó | 60 fa, 40 kő | 0 | A Kórus szeme (passzív) | Lebegő szemzsák: messzire lát, és a Kórus létszámát hordozza (+8). |
| Robbanó atka *[Raj]* | 30 fa, 15 kő | 1 | Önrobbantás | Olcsó, gyors atka: az ellenség közé rohan és felrobban. |
| Vadászkarmos *[Raj]* | 45 fa, 25 kő | 1 | Beásás | Fürge karmos-változat: gyorsan üldöz és szaggat. |
| Ostromtaréj *[Páncél]* | 120 fa, 120 kő, 40 meteorit | 4 | Beásás | Élő ostromgép: messzire köpi a maró kitintüskéket, épületekre különösen. |
| Ásóféreg *[Páncél]* | 90 fa, 70 kő | 2 | Beásás | A föld alatt közlekedő féreg: beásva rejtőzik, és alulról támad. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Raj** | Olcsó tömeg, korai elárasztás | Rajkamra | Robbanó atka, Vadászkarmos | A lárvák 30%-kal gyorsabban kelnek ki, a rajlények gyorsabbak. Robbanó atkák és vadászkarmosok öntik el az ellenfelet, mielőtt felkészülne. |
| **Páncél** | Lassú, szívós menetelés | Kitinkamra | Páncélbogár, Ostromtaréj, Ásóféreg | +2 páncél minden élő egységnek. A Karmosok páncélbogárrá alakulhatnak; ostromtaréjak és alagutat ásó férgek törik fel a védelmet. |
| **Kórus** | Irányítás és átállítás | Agyszentély | Agyfaló, Csápos horror | A Kórus hangja: a megölt ellenséges egységek 15%-a a te oldaladon kel fel. Agyfalók és csápos horrorok bomlasztják az ellenséges sereget. |

### Hősök

**Keth** – Egy megszállt kethri város emlékőre; a Kórus parancsnoki csomópontja – de régi önmaga nem tűnt el.

* **Őrület köde** (Q): Elmét bomlasztó spórafelhő.
* **Kórus-hang** (W): A közeli szövetségesek gyorsabbak és regenerálódnak.
* **Csápcsapás** (E): Savas csapás egy célpontra.
* **A sokaság** (R) *(végső, 6. szinttől)*: Végső képesség: hat Karmos kel ki.

**Vor-Uthul** – A Mutáció Mestere: a saját testéből költ lárvákat, kitinpáncélt növeszt.

* **Savsugár** (Q): Maró savsugár.
* **Mutáns páncél** (W): Kitinlemezek nőnek a mester testén.
* **Lárvahullám** (E): Karmosokat költ ki a saját testéből.
* **Kitinrobbanás** (R) *(végső, 6. szinttől)*: Végső képesség: kitinszilánkok repesznek szét a mester körül.

**Ssarha, a Raj Hangja** – A Kórus hangja: a rajt egy akaratba fogja, és összeölti a sebeket.

* **Spórafelhő** (Q): Fojtó spórafelhő.
* **Rajtudat** (W): A raj egy akarattal mozog: gyorsabb és erősebb.
* **Hús-öltés** (E): Automatikusan összefoltozza a közeli sérült élő szövetségeseket.
* **Rémszárnyak** (R) *(végső, 6. szinttől)*: Végső képesség: négy szárnyas rém érkezik.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Kaptár | 205 kő, 195 fa | Élő főépület. Lárvákat nevel (max. 3), minden egység innen kel ki – párhuzamosan. |
| Tápgomba | 65 fa, 20 kő | Tápláló gombatelep: messzire terjeszti a mélységet, és gyógyítja a rajt. |
| Emésztőkamra | 50 fa, 25 kő | Leadóhely minden nyersanyagnak. |
| Ikrafészek | 175 fa, 50 kő | Lehetővé teszi a Karmosokat. |
| Tüskeverem | 165 fa, 85 kő | Lehetővé teszi a Tüskeköpőket és a Savköpőket. |
| Agyszentély | 135 kő, 75 meteorit, 65 fa | Lehetővé teszi az Agyfalót és a Csápos horrort. |
| Csontcsúcs | 150 kő, 100 meteorit, 100 fa | Lehetővé teszi a Szárnyas rémeket. |
| Tüskés gyökér | 100 fa, 25 kő | Védőépület, földi és légi célpontokra. |
| Kethri tojáskamra | – | Fertőzetlen kethri tojások – a jövő nemzedéke. Védd meg! |
| Kórus-kút | 150 kő, 100 fa | Lüktető hangkút: innen szólnak a Kórus hősei – Keth, Vor-Uthul és Ssarha. |
| Rajkamra | 150 fa, 80 kő | Lüktető keltetőkamra: robbanó atkák és vadászkarmosok (a Kaptárban kelnek ki). |
| Kitinkamra | 160 fa, 140 kő | Páncélnövesztő kamra: páncélbogarak, ostromtaréjak, ásóférgek. |

## Wullof

**Üss és fuss.** Gyors egységek, erdei rejtőzés (fák mellett láthatatlanok, amíg nem támadnak), énekek. **Nomád táborok, falka, vadászzsákmány.** Az épületek (a Kéregkunyhók és az Ősök köre is) felszedhetők és vándorszekéren máshová vihetők; az együtt harcoló falka erősebb; minden ölés zsákmányt (fát) hoz.

*Létszám:* Emuk (Emukarámban tenyészthetők, karámonként legfeljebb 5).  *A varázslatok ára:* énekek (kapcsolható aurák, egyszerre egy) és újratöltés.

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Gyűjtögető | 30 fa, 20 kő | 1 | Erdei rejtőzés (passzív) | Munkás. Gyűjt, épít, javít; fonott háti kosárban hordja a zsákmányt. Építéskor az épület mellett áll; többen együtt gyorsabban építenek. |
| Lándzsás harcos | 60 fa, 25 kő | 2 | Harci ének, Erdei rejtőzés (passzív) | Gyors, kovakőhegyű lándzsás, keskeny festett hárítópajzzsal. Falkában veszélyes. |
| Bumerángvető | 95 fa, 25 kő | 2 | Bumeráng, Vadászles, Erdei rejtőzés (passzív) | Távolsági vadász: a bumeráng visszatérő íven a közeli ellenségeket is eltalálja. Földi és légi célpontokra. |
| Holdsámán | 85 fa, 70 meteorit, 25 kő | 2 | Holdfény-gyógyítás, Szellemfarkasok, Vándorének | Gyógyít, és szellemfarkasokat hív a holdfényből. |
| Szellemfarkas *(idézett / mutációval)* | – | 0 | Szellemtest (passzív) | A holdfényből idézett szellem-dingó. Fél percig él. |
| Bölénylovas *(idézett / mutációval)* *[Bölény-csorda]* | 155 fa, 45 kő | 3 | Roham, Leszállás | Bölényen ülő wullof harcos: erős roham. Le is szállhat. |
| Viharkakadu | 95 meteorit, 100 fa, 55 kő | 4 | Villámcsapás | Óriás viharkakadu; villámai a célpont környékét is sújtják. |
| Termeszvár-óriás *[Ősök útja]* | 330 fa, 90 meteorit, 85 kő | 5 | Ősök haragja | Életre kelt vörös termeszvár – lassú, de az épületeket porrá zúzza. |
| Vándorbatyu *(idézett / mutációval)* | – | 0 | Vándortábor (passzív) | Felszedett tábor, rudakra kötve. Jobb gomb: Letelepítés máshol. |
| Dárdavető portyázó | 65 fa, 20 kő | 2 | Dárdavetés, Vándorének, Erdei rejtőzés (passzív) | Gyors portyázó dárdavetővel (woomera): a munkások és a távolsági egységek réme. |
| Emu | 60 fa | 0 | Riadófutás | Tenyészemu: +6 létszám. Emukarámonként legfeljebb 5 tartható. |
| Bölény *[Bölény-csorda]* | 110 fa, 40 kő | 1 | Felültetés | Erős, de lassú bölény. Felültethető rá egy wullof gyalogos harcos. |
| Groomi *[Groomi-fészek]* | 85 fa, 25 meteorit | 2 | Roham, Savköpés, Tűzköpés, Fagyköpés | Piros szörnymadár: gyors, harapós földi vadász. A Groomi-fészekben sav-, tűz- vagy fagyköpést tanulhat (csak egyet). |
| Csordavezér bika *[Bölény-csorda]* | 220 fa, 90 kő, 40 meteorit | 4 | Taposás, Roham | Hatalmas bölénybika: épületeket tipor, rohamában mindent elsöpör. |
| Groomi-fióka *(idézett / mutációval)* *[Groomi-fészek]* | 40 fa | 1 | Roham | Kicsi, de fürge és harapós Groomi. |
| Groomi-anya *[Groomi-fészek]* | 160 fa, 60 meteorit | 4 | Fiókakeltetés, Roham | Óriás Groomi-tojó: fiókákat keltet, és keményen harap. |
| Totemfaragó *[Ősök útja]* | 90 fa, 30 meteorit | 2 | Ősök haragja, Erdei rejtőzés (passzív) | Ősök-dalát éneklő faragó: a közeli szövetségesek +2 páncélt kapnak. |
| Álomjáró *[Ősök útja]* | 80 fa, 70 meteorit | 2 | Holdfény-gyógyítás, Szellemfarkasok | Az ősök álmait járja: gyógyít, és szellemfarkasokat hív. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Bölény-csorda** | Kevés, drága, megállíthatatlan roham | Bölénykarám | Bölény, Bölénylovas, Csordavezér bika | Bölényeket nevelsz, és a harcosaid felülnek rájuk. A lovasság és a vadállatok gyorsabbak és erősebbek; a Csordavezér bika épületeket tipor. Középjátékban egyetlen, döntő rohamra épít. |
| **Groomi-fészek** | Gyors portyázás, vadászat a gyengékre | Groomi-fészek | Groomi, Groomi-fióka, Groomi-anya | Piros szörnymadarak: a Groomik öléskor gyógyulnak és gyorsabbak, a Groomi-anya fiókákat keltet. A köpésük (sav, tűz vagy fagy) egyszer, végleg választható. Munkásvadászat, térképuralom. |
| **Ősök útja** | Totemlánc, lassú és biztos előrenyomulás | Totemkert | Totemfaragó, Álomjáró, Termeszvár-óriás | Totemkertet emelsz: a tornyok erősebbek és messzebbre látnak, az épületek szívósabbak. Totemfaragók és Álomjárók támogatják a termeszvár-óriásokat. Védekező, területfoglaló játék. |

### Hősök

**Tava** – Folyami térképész és követ a Hét Út Szövetségéből. Utat nyit és felderít.

* **Nyomkereső nyíl** (Q): Tava célzott lövése: nagy sebzés és lassítás egy ellenségen.
* **Útének** (W): A közeli szövetségesek gyorsabban mozognak (és jobban védekeznek).
* **Szellemfarkasok** (E): Szellem-dingókat idéz.
* **A Hét Út hívása** (R) *(végső, 6. szinttől)*: Végső: két viharkakadu érkezik 45 másodpercre.

**Varnu, a Holdsámán** – Az ősök szellemeivel beszél: gyógyít, ködöt és szellemfalkát hív.

* **Holdfény** (Q): Automatikusan gyógyítja a közeli sérült szövetségeseket.
* **Ősök köde** (W): Az ősök szellemei ködként fonják körül az ellenséget.
* **Szellemfalka** (E): Szellem-dingók falkáját hívja.
* **Telihold** (R) *(végső, 6. szinttől)*: Végső képesség: telihold-fény gyógyít a területen.

**Kurrin Harcfőnök** – Harci bölényen lovagló harcfőnök: tapos, üvölt, és a Hét Út harcosait hívja.

* **Bölénytaposás** (Q): A harci bölény a földbe dobbant: sebez és megbénít.
* **Harci üvöltés** (W): A közeli szövetségesek sebzése nő.
* **Vérfalka** (E): Vadászláz: rövid ideig gyorsabb és erősebb.
* **A Hét Út harcosai** (R) *(végső, 6. szinttől)*: Végső képesség: öt lándzsás harcos érkezik a Hét Útról.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Tanácskunyhó | 365 fa, 35 kő | A törzs nagy kéregkunyhója. Leadóhely, Gyűjtögetőket nevel. Felszedhető. |
| Kéregkunyhó | 85 fa | Eukaliptuszkéreg-kunyhó: Lándzsás harcosokat képez. Felszedhető. |
| Harcosok tüze | 180 fa, 20 kő | Dárdavetőket és bumerángvetőket képez. Felszedhető. |
| Emukarám | 150 fa | Emukat tenyészt (karámonként legfeljebb 5). Csak az emuk növelik a létszámot. Felszedhető. |
| Sámánkunyhó | 150 fa, 50 meteorit | Holdsámánokat képez, fejlesztéseket kutat. Felszedhető. |
| Vörös szikla | 150 kő, 100 fa, 75 meteorit | A viharkakaduk fészke a pusztai vörös sziklán. |
| Eukaliptusz-őrfa | 100 fa, 50 kő | Kilátós őrfa: a fenti vadászok bumerángokkal védik a tábort. |
| Ősök köre | 180 fa, 100 kő | Faragott oszlopok köre: Tava, Varnu és Kurrin hívása. Felszedhető. |
| Bölénykarám | 220 fa, 90 kő | Bölényeket és termeszvár-óriásokat ad. Felszedhető. |
| Groomi-fészek | 200 fa, 60 meteorit | Groomikat nevel; itt választható ki a köpésük. |
| Totemkert | 220 fa, 60 meteorit | Faragott totemek kertje: Totemfaragókat, Álomjárókat és termeszvár-óriásokat ad. |

## Micéliák

**A növekvő hálózat.** Osztódás: a harcos egységek X legyőzött ellenség után kettéosztódnak (a pöttyök az egység fölött mutatják, mennyit „evett”); a varázslatok spórából mennek. **Spórafonal-hálózat.** Az épületeket fonalak kötik össze; bármelyik összekötött épület leadóhely; a kicsi Szívgomba sokszor megépíthető, és egyben védőépület. A pöfetegek összeolvadhatnak.

*Létszám:* A hálózat: minden összekötött épület +1.  *A varázslatok ára:* spóratöltet (ölésből és a hálózat közelében).

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Spóraszedő | 35 fa, 15 kő | 1 | Hálózati gyűjtés (passzív) | Munkás. Bármelyik hálózatba kötött épületbe leadhatja a terhét. |
| Pöfeteg | 35 fa, 15 kő | 1 | Összeolvadás, Osztódás (3 ölés) (passzív) | Olcsó, ugráló gombalény. Halálakor mérgező spórafelhő tör ki belőle. Hármasával Óriáspöfeteggé olvad. |
| Tüskegomba-lövész | 85 fa, 30 kő | 2 | Spóratüske, Osztódás (4 ölés) (passzív) | Mérgező spóratüskéket lő, földi és légi célpontokra. |
| Kalapos őr | 110 fa, 60 kő | 2 | Kalapfal, Osztódás (4 ölés) (passzív) | Vastag kalapú, páncélos gombaharcos. |
| Álomgomba-sámán *[Álom]* | 70 fa, 70 meteorit, 20 kő | 2 | Gyökérkötés | Gyökérkötéssel egy területen megbénítja az ellenséget. |
| Óriáspöfeteg *(idézett / mutációval)* | 125 fa, 45 kő | 3 | Spórarobbanás | Három Pöfetegből összeolvadt óriás. Halálakor hatalmas spórarobbanás. |
| Lebegő pöfeteg | 60 meteorit, 70 fa, 40 kő | 3 | Spóraeső | Gázzal telt, lebegő gombatest; spóraesőt szór. |
| Rothasztó *[Rothadás]* | 75 fa, 25 kő | 2 | Osztódás (4 ölés) (passzív) | Bomló gombatest: közelharcban rothaszt, halálakor mérgező spórafelhőt ereget. |
| Tetemgomba *[Rothadás]* | 50 fa, 20 kő | 1 | Osztódás (4 ölés) (passzív) | Az elesettekből kelt, gombával átszőtt test: lassú, de nem adja fel. |
| Fagomba-óriás *[Óriásliget]* | 200 fa, 100 kő, 40 meteorit | 5 | Gyökérkötés | Járó, ősi fagomba: a liget védelmezője, gyökereivel megköt. |
| Álomszövő *(idézett / mutációval)* *[Álom]* | 90 fa, 70 meteorit | 2 | Álomspóra | Altató spórákat szövő gomba-mágus. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Rothadás** | Felőrlő, méreggel nyújtott harc | Rothadókert | Rothasztó, Tetemgomba | A közelharcosaid mérgeznek, és az elesett élőkből 25% eséllyel tetemgomba-zombi kel. Rothasztók vezetik a lassan terjedő pusztulást. |
| **Óriásliget** | Terjeszkedés és területfoglalás | Óriásliget | Fagomba-óriás | A hálózat minden összekötött épülete +2 létszámot ad, az épületek +250 életerőt kapnak. Járó fagomba-óriások védik a ligetet. |
| **Álom** | Csel, rejtőzés, rajtaütés | Álomkert | Álomgomba-sámán, Álomszövő | Az erdőben minden egységed láthatatlan, amíg nem mozog és nem támad. Álomsámánok és Álomszövők altató spórákkal bénítják meg az ellenfelet. |

### Hősök

**Mór** – Fiatal emlékkertész, magában hordja egy sérült emlékkert töredékeit. Gyógyít és fertőzést vizsgál.

* **Gyógyspóra** (Q): Automatikusan gyógyítja a közeli sérült élő szövetségeseket.
* **Gyökérkötés** (W): Gyökerek fonják körbe az ellenséget.
* **Emlékkert** (E): A közeli szövetségesek gyorsabban regenerálódnak.
* **Az Emlékkert ébredése** (R) *(végső, 6. szinttől)*: Végső képesség: két Óriáspöfeteg sarjad.

**A Gyökéranya** – Az ősi micélium teste: gyökeret ereszt, gúzsba köt, és megrengeti a földet.

* **Gyökérháló** (Q): Gyökerek fonják gúzsba az ellenséget.
* **Mélygyökér** (W): Gyökeret ereszt: lassú, de szinte elpusztíthatatlan.
* **Tápláló talaj** (E): A közeli szövetségesek regenerációja és páncélja nő.
* **Gyökérrengés** (R) *(végső, 6. szinttől)*: Végső képesség: a föld alól feltörő gyökerek mindenkit megbénítanak.

**Sporin, a Bomlasztó** – A bomlás vándora: tüskéi rothasztanak, köpenye spórafelhő.

* **Bomlasztó tüske** (Q): Rothasztó spóratüske.
* **Rothadás** (W): Minden elrothad a területen.
* **Spóraköpeny** (E): Spórafelhőbe burkolózik: gyorsabb, és a lövések elvétik.
* **Pöfeteg-raj** (R) *(végső, 6. szinttől)*: Végső képesség: hat pöfeteg sarjad – robbanó spórazsákok.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Szívgomba | 255 fa, 70 kő | A hálózat szíve: kicsi főépület és védőtorony egyben. +6 létszám (és a hálózat minden összekötött épülete +1), Spóraszedőket nevel, spórákkal lő. Akárhány építhető. |
| Spórafonal | 25 fa | Hálózati csomópont: összeköti az épületeket a Szívgombával. |
| Termőtest | 165 fa, 35 kő | Pöfetegeket és Kalapos őröket növeszt. |
| Tüskegomba-telep | 140 fa, 85 kő | Tüskegomba-lövészeket növeszt. |
| Gyökérbarlang | 125 kő, 50 meteorit | Fejlesztések a hálózat mélyén. |
| Álomkert | 75 meteorit, 65 fa, 35 kő | Álomgomba-sámánokat nevel. |
| Lebegőbölcső | 150 kő, 75 meteorit, 100 fa | Lebegő pöfetegeket nevel. |
| Emlékfa | 215 fa, 35 kő | Az elfeledett nevek fája: Mór, a Gyökéranya és Sporin hívása. |
| Rothadókert | 180 fa, 60 kő | Bomló tetemeken nőtt gombakert: rothasztók és tetemgombák. |
| Óriásliget | 240 fa, 100 kő | Ősi fagombák ligete: fagomba-óriások. |

## Vasbirodalom

**A halhatatlan légió.** Az elesett gépek lelke 40 mp-ig lélekhorgonyban marad: egy Fabrikátor az eredeti ár 35%-áért újraépítheti. **Lélekidézés, gépjavítás, ostrom.** Az elesettek tetemeiből szellemlégiósok kelnek; a gépeket a Fabrikátorok javítják; a Kolosszus ostrom-módba telepíthető. Az idézett szellemek nem foglalnak létszámot.

*Létszám:* Lélekreaktorok.  *A varázslatok ára:* energia (mana).

### Egységek

| Egység | Költség | Létszám | Képességek | Leírás |
|---|---|---|---|---|
| Fabrikátor | 30 fa, 20 kő | 1 | Gépjavítás és újratestesítés (passzív) | Pókszerű munkásgép. Épít, gyűjt, és javítja az épületeket és a gépeket (jobb klikk a sérült gépre). |
| Légiós | 45 kő, 25 fa | 2 | Testudo, Lélekhorgony (passzív) | Kis lépegetőtestbe zárt légiós-szellem, scutummal és gladiusszal. |
| Ballistarius | 65 fa, 15 kő | 2 | Gyorstöltés, Lélekhorgony (passzív) | Kis gép vállra szerelt íjpuskával. |
| Szellemidéző | 50 kő, 75 meteorit, 30 fa | 2 | Lélekidézés | Lélekmentő gép-pap. A csatatér tetemeiből lélekhorgonyt ment, és szellemlégióst támaszt. |
| Centurio-mecha *[Légió]* | 120 kő, 25 meteorit, 60 fa | 4 | Gépágyú-sorozat, Lélekhorgony (passzív) | Közepes lépegető mecha forgócsöves gépágyúval. |
| Aquila-drón | 75 meteorit, 65 fa, 35 kő | 3 | Zuhanóbomba, Lélekhorgony (passzív) | Sas formájú repülő gép. |
| Kolosszus *[Gépisten]* | 270 kő, 100 meteorit, 130 fa | 7 | Ostrom-mód, Lélekhorgony (passzív) | Óriás ostrom-mecha. Ostrom-módban mozdulatlan, de messzebbre és nagyobbat lő. |
| Szellemlégiós *[Lélek]* | 60 kő, 30 meteorit | 2 | Szellemtest (passzív) | Egy elesett harcos lelke, a Szellemidéző akaratához kötve. |
| Szellemkolosszus *[Lélek]* | 160 kő, 120 meteorit | 5 | Szellemtest (passzív) | Egy elesett óriás lelke. |
| Onager-mecha *[Gépisten]* | 180 kő, 40 meteorit, 70 fa | 4 | Ostrom-telepítés, Lélekhorgony (passzív) | Négylábú hajítógép: messzire hord, épületek és csoportok ellen. |
| Lélekhorgony *(idézett / mutációval)* | – | 0 | Lélekhorgony (passzív) | Egy elesett gép lelke. Egy Fabrikátor 40 mp-en belül újraépítheti (javítással) – különben a lélek elillan. |
| Praetoriánus *[Légió]* | 100 kő, 40 meteorit | 3 | Testudo, Lélekhorgony (passzív) | Elit gépharcos aranyozott páncélban: a légió gerince. |

### Fejlődési irányok (Felemelkedés után, egyet végleg)

| Irány | Stílus | Épület | Egységek | Szabály |
|---|---|---|---|---|
| **Légió** | Fegyelmezett tömeg alakzatban | Praetorium | Praetoriánus, Centurio-mecha | Alakzat: a gyalogosok minden közeli bajtársért +6% sebzést kapnak (max. +30%), és +1 páncélt. Praetoriánusok és Centurio-mechák. |
| **Gépisten** | Kevés, hatalmas gép | Öntöde | Onager-mecha, Kolosszus | Kolosszusok és onager-mechák, +150 életerővel és +4 ostromsebzéssel. Lassú, drága, de megállíthatatlan. |
| **Lélek** | Kifárasztó, élőhalott játék | Lélekkút | Szellemlégiós, Szellemkolosszus | A megölt élő ellenségek 20%-a szellemlégiósként kel fel nálad (40 mp-re). A Lélekkútban állandó szellemlégiósok és szellemkolosszusok képezhetők. |

### Hősök

**Livia Varr** – Egykori orvos, a lélekhorgonyokat mentő légió tisztje.

* **Lélekmentés** (Q): Egy közeli tetemből szellemlégióst támaszt.
* **Horgonyjavítás** (W): Automatikusan javítja a közeli sérült gépeket.
* **A légió fegyelme** (E): A közeli szövetségesek sebzése nő.
* **Második lélegzet** (R) *(végső, 6. szinttől)*: Végső képesség: a területen minden szövetséges gyógyul.

**Nera** – Livia lánya: gyermekként került géptestbe; hetven év tapasztalattal, de a birodalmi jog szerint még mindig kiskorú.

* **Gyorstöltés** (Q): Rövid ideig nagyobb sebzés.
* **Célzott lövés** (W): Messzire hordó, erős lövés.
* **Bátorság** (E): „Félek – és mégis megyek.” A közeli szövetségesek páncélja nő.
* **Szabad horgony** (R) *(végső, 6. szinttől)*: Végső képesség: Nera felszabadult lelke 10 másodpercre félelmetes erővé válik.

**Cassius Ferrum legátus** – A Hetedik Légió parancsnoka nehéz géptestben: roham, fegyelem és Ordan haragja.

* **Légió-roham** (Q): A legátus gépteste előrelendül, és szétszórja az ellenséget.
* **Vasfegyelem** (W): A közeli szövetségesek páncélja nő.
* **Mechalégió** (E): Légiósokat rendel a frontra.
* **Ordan haragja** (R) *(végső, 6. szinttől)*: Végső képesség: Ordan lelke tölti meg a legátus gépét.

### Épületek

| Épület | Költség | Szerep |
|---|---|---|
| Fórum | 350 kő, 150 fa | Oszlopcsarnokos főtér gőzölgő gépházzal. Leadóhely, Fabrikátorokat gyárt. |
| Lélekreaktor | 100 kő, 40 fa | +10 létszám – lélekenergiát ad a gépeknek. |
| Légió-gyár | 100 fa, 100 kő | Légiósokat és Ballistariusokat szerel össze. |
| Öntöde | 150 fa, 175 kő | Centurio-mechákat és Kolosszusokat önt. |
| Ordan temploma | 150 kő, 75 meteorit | Ordan, a Kéz, amely Megtart hálózatának szentélye. Szellemidézőket avat, fejlesztéseket kutat. |
| Hangár | 150 kő, 75 fa, 50 meteorit | Aquila-drónokat gyárt. |
| Aquila-torony | 125 kő, 25 fa | Sasos őrtorony íjpuskával. |
| Panteon | 180 kő, 100 fa | A megmentett hősök csarnoka: Livia, Nera és Cassius. |
| Praetorium | 220 kő, 60 meteorit | A légió parancsnoksága: praetoriánusok és Centurio-mechák. |
| Lélekkút | 180 kő, 120 meteorit | Lélekfénnyel izzó kút: állandó szellemlégiósok és szellemkolosszusok. |

---

# Háttér: az újabb fajok

*© 2026 Mesehordo – minden jog fenntartva. Lásd: [LICENSE.md](../LICENSE.md).*

Mindhárom új faj **saját alapmechanikára** épül, ami egyik meglévőre (elf energiamező, ursan kétkezes építés,
croftian mélység + lárvák) sem hasonlít.

| Faj | Mechanika röviden |
|---|---|
| **Wullof** | nomád tábor (az épületek felszedhetők és máshol letelepíthetők), falkabónusz, vadászzsákmány, szellemidézés |
| **Micéliák** | spórafonal-hálózat: az épületek csak a Szívgombához kötve működnek, a nyersanyag bármelyik hálózati épületbe leadható; egységek összeolvasztása |
| **Vasbirodalom** | gépszellemek: nem esznek, nem gyógyulnak, javítani kell őket; a szellemidéző a holtakból szellemlégiót támaszt; kis és nagy mechák, ostrom-mód |

---

## Wullof – a Holdfalka
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

## Micéliák – a Fonal népe
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

## Vasbirodalom – Ferrum Aeternum
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

