# Napszakok és időjárás

> Állapot: **JAVASLAT** (2026-11-12), a tulajdonos kérésére (D35).
> - A rendszer a motorban az M5–M7-ben készül (látvány: M5, környezeti rendszer és szabályok: M6, faji hatások: M7).
> - A pályaszerkesztő és a küldetésvezérlés az M11-ben.
> - Minden szám adat (`game.json` → `environment`, fajonként a faj- és egységsablonokban), a balansz az M7-ben dől el.
> - Alap: D22 (állapotok), D24 (módosítók), D33 (magaslat, mélység), ADR-004 (determinizmus), ADR-006 (pálya, terepváltozás).

## 1. Meccsbeállítás (a lobbiban és a küldetésben)

| Beállítás | Értékek | Mit jelent |
|---|---|---|
| **Napszakok** | ki · hangulat · hatással | *ki*: állandó nappal. *hangulat*: a fény, az árnyékok iránya és a színek változnak, a játékszabályok nem. *hatással*: a napszakos szabályok is élnek (3. pont). |
| **Időjárás** | ki · hangulat · hatással · hatással + pályaváltozás | *hangulat*: csak látvány és hang. *hatással*: az időjárás szabályai (4. pont). *pályaváltozás*: a terep is változik (5. pont). |
| **Napciklus hossza** | rövid · normál · hosszú | Egy teljes nap: 10 / 16 / 24 perc játékidő (adat). |
| **Időjárás gyakorisága** | ritka · normál · gyakori | |

- Az alapértelmezés: napszakok *hatással*, időjárás *hangulat*.
- A küldetés felülírhatja a beállításokat, például „mindig éjszaka”, vagy egy forgatókönyv szerinti vihar.
- A beállítás a modlock része, ezért többjátékos módban mindenkinél ugyanaz.

## 2. Működés a motorban

- **Determinisztikus:**
  - A napszak a tickből számolódik.
  - Az időjárás egy saját véletlenszám-folyamból (ADR-004), a pálya *éghajlata* szerinti átmenetekkel.
  - Mindez a szimuláció állapotának része: benne van a hashben, a mentésben és a visszajátszásban.
- **Környezeti feltételek:** a rendszer feltételeket ad (OpenRA-feltételek, D22-állapotok).
  - Napszak: `napszak:hajnal | nappal | del | alkony | ejszaka | ejfel`.
  - Időjárás: `idojaras:eso | ho | ...`, erősséggel.
  - Csempe: `csempe:vizes | havas | sáros`.
  - Az egység-, faj- és kutatássablonok ezekre módosítókat és állapotokat köthetnek (D24): `{"when": {"napszak": ["del"]}, "modifiers": [...]}`. Így minden új szabály adat, a modok is bővíthetik.
- **Napciklus:** hajnal (5%), nappal (40%, közepén a dél 10%), alkony (5%), éjszaka (40%, közepén az éjfél 10%).
  - Az árnyék iránya és hossza a nap állásával fordul (M5).
  - Éjjel kékes, holdfényes a tónus; a fényforrások (lámpás, tűz, Égkő-fény) kiemelkednek.
  - A meglévő **telihold** szabály (wullof) az éjszakákhoz kötődik: minden 4. éjszaka telihold.
- **Időjárás:**
  - Állapotok: derült, felhős, eső, zivatar, köd, hó, hóvihar, homokvihar, hőség, hamueső.
  - Mindegyiknek van erőssége (enyhe / erős) és időtartama (2–6 perc).
  - Az átmenetek fokozatosak (felhősödés → eső), a pálya éghajlata szerint (mérsékelt, havas, sivatagi, mocsaras, vulkáni – a pályaadatban). Sivatagban nincs hó, havas pályán nincs homokvihar.
- **Lua:**
  - Események: `on_daytime(fázis)`, `on_weather(típus, erősség)`.
  - Lekérdezés: `Game.daytime()`, `Game.weather()`.
  - Küldetésnek: `Game.set_weather(...)`, `Game.set_time(...)`. WC3-szerűen: „éjfélkor jönnek a farkasok”.
- **Felület:**
  - Napóra a minitérkép mellett (WC3-szerűen).
  - Időjárás-ikon az erősséggel.
  - Előrejelzés: a vihar 20 mp-cel előbb jelez.
  - A súgóban az egységnél látszik, mely napszak vagy időjárás hat rá.

## 3. Napszakok hatásai (ha *hatással*)

**Általános szabályok** (minden népre):

| Napszak | Hatás |
|---|---|
| éjszaka | a látótáv −25% (a WC3 mintájára); a fényforrás körül nem csökken; az *éjjellátó* címkéjű egységre nem hat |
| éjszaka | az alvó semleges táborok mélyebben alszanak: a felderítő nem ébreszti fel őket (D25) |
| nappal | az épületek építése +0% (alap); éjjel −10% (kivétel: akinek fényforrása van ott) |
| hajnal, alkony | átmeneti fázisok, a hatások fokozatosan váltanak (nem ugrásszerűen) |

A faji hatások – **minden népre** hat a napszak és az időjárás is – a 4a. pontban, egy táblázatban.

## 4. Időjárás hatásai (ha *hatással*)

| Időjárás | Általános hatás (minden népre) |
|---|---|
| **eső** | a földi lövedékek hatótávja −10%, a tűzhatások gyengébbek (−25%), a sáros csempén lassulás |
| **zivatar** | mint az eső, plusz villámlás: rövid látótáv-kiesés; a magaslaton álló egységbe ritkán villám csap (kis sebzés) |
| **köd** | a látótáv felére csökken; a távolsági egységek csak látott célpontra lőnek |
| **hó** | havas réteg: −10% sebesség (a hó terepen −20%), az építés −15% |
| **hóvihar** | −25% sebesség, a látótáv −40%, a lövedékek szórnak |
| **homokvihar** | a lövészek látótávja és lőtávja −40%; a nyílt terepen álló egységek lassú sebzést kapnak |
| **hőség** | −10% munkásgyűjtés; a gázlók kiszáradnak (5. pont) |
| **hamueső** (vulkáni) | a látótáv −20%, az élő egységek lassú sebzést kapnak |
| **szél** (enyhe, minden éghajlat) | a lövedékek kissé eltérnek; a repülők a széllel +10%, ellene −10% |

## 4a. Faji hatások – minden népre hat a napszak és az időjárás

Minden nép kap legalább egy napszakos és egy időjárásos hatást, a jelleméhez illesztve; előnyt és hátrányt is, hogy
az idő taktikai döntés legyen (mikor támadjak, hol várjak). Mind adat (feltétel + módosító, D24), a számok az M7-ben.

| Nép | Napszak | Időjárás |
|---|---|---|
| **Szürke elfek** | dél: a pajzs és az életerő +50%-kal gyorsabban töltődik; éjfél: −15% energia-visszatöltés | eső: az energiamező +10% (a víz vezeti); homokvihar: −20% pajzs-visszatöltés |
| **Wullof** | éjszaka: a vadászok és a felderítők állva vagy lassan haladva láthatatlanok (támadáskor felfedik magukat), a falkaerő +1 fokozat; telihold: +15% sebzés (a meglévő szabály) | köd: szaglás – 6 csempén belül ködben is látnak; hóvihar: fele lassulás (nomád, edzett nép) |
| **Huk’ucha** (békák) | alkony: −10% sebesség (hűvösödés); hajnal: +10% sebesség (harmat) | eső: +15% sebesség, a tavak körüli bónusz kétszeres; hőség: −10% sebesség, a tavacskák zsugorodnak |
| **Ooni** | éjszaka: a lámpásfény +2 csempével tágabb, a gyógyulás benne +25% | köd: a lámpásfény átvilágít rajta; eső: a papírlámpások fénye −1 csempe |
| **Ursanok** | éjszaka: a helyőrségben lévők látótávja nem csökken (őrtüzek); hajnal: a tanyák +10% termelés | hó, hóvihar: a pajzsfalban állók nem lassulnak és nem fáznak (vastag bunda); hőség: −10% sebesség |
| **Croftianok** | éjszaka: a lárvák gyorsabban fejlődnek; nappal: a fertőzés lassabban terjed | eső: a fertőzés gyorsabban terjed (a nedves talajon); hamueső: nem kapnak sebzést, a mutáció olcsóbb |
| **Aurethi Birodalom** | éjfél: a lélekidézés −20% költség; nappal: a lélekreaktorok +10% termelés | zivatar: a lélekreaktorok +20% (villám tölti őket); eső: az ostromgépek −10% sebesség (sár) |
| **Tűz-elfek** | dél: +10% sebzés; éjszaka: −10% sebesség (a dűnék hidege) | homokvihar: nem hat rájuk, és állva láthatatlanok benne; eső, hó: −10% sebzés |
| **Niil-elfek** | hajnal: a folyóparti egységek gyógyulnak | eső: a vízparton +15% sebesség és gyógyulás; hőség: a kiszáradt gázlókon −10% sebzés |
| **Pinguu** | éjszaka: +10% páncél (a hideg erősíti) | hó, hóvihar: +15% sebesség, nem lassulnak; a befagyott vízen +20%; hőség: −10% sebesség |
| **Tokla** (fagyóriások) | éjszaka: +10% páncél; dél: −5% sebesség | hó: a fagytörpék építése nem lassul; hóvihar: a fagyóriások +10% sebzés; hőség: −10% életerő-visszatöltés |
| **Daresh** (darázsnép) | éjszaka: a rajok −15% sebesség; nappal: +10% sebesség | eső: a repülő egységek −20% sebesség (vizes szárny); szél: hátszélben +15% |
| **Ghoblin** (kőnyűvő manók) | éjszaka: a bányászat +15% (föld alatti nép, a fény zavarja); dél: −10% látótáv (vakító fény) | eső: a kőfejtés lassabb (−10%); hamueső: a kőből több érc jön (+10%) |
| **Galóca** (gombaharcosok) | éjszaka: gyorsabb regeneráció, a tüskék +10% sebzés; dél: −10% sebesség (kiszáradás) | eső: +20% regeneráció, gyorsabb spóraterjedés; hőség: −10% életerő |
| **Mhonyo** (kardkovács manók) | éjszaka: a kovácsműhelyek tüze fényforrás (látótáv nem csökken a környékükön); hajnal: +10% kovácsolási sebesség | eső: a kovácsolás −10% (nedves szén); hőség: a kovácsolás +10% |
| **Micéliák** (gyökérlábú gombanép) | éjszaka: gyorsabb gyökérterjedés; dél: −10% sebesség | eső: a gyökérhálózat +20% hatótáv; hőség és homokvihar: −15% regeneráció |
| **Tendüür** (szárnyas tündérek) | hajnal, alkony („tündérórák”): a varázslatok −20% energiába kerülnek | köd: az erdőben láthatatlanok; szél: repülőként érzékenyebbek (±15%) |
| **Zhan-zhama** (apró nép, nagy kakasok) | hajnal: a kakasszó felfedi a közeli láthatatlan ellenséget; éjszaka: a kakasok −10% sebesség (alszanak) | eső: a kakasok −10% sebesség (ázott toll); szél: a kakaslovasok hátszélben +10% |
| **Shrissh-matah** (tollas rovarok) | éjszaka: a rúnavárak fénye látótávot ad | eső: a rúnák felragyognak (+10% varázserő); hideg (hó, hóvihar): −15% sebesség (rovarok) |
| **Nila-uchana** (lótuszudvarok) | éjfél: a szörnyidézés +1 szint; dél: a lótuszok +10% termelés | eső: a lótusztavak bővülnek (+10% termelés); hőség: a lótuszok −15% |
| **Vadon** (semleges táborok) | éjszaka: mélyebben alszanak, a felderítő nem ébreszti fel őket (D25); hajnal: a ragadozók vadászni indulnak (szűkebb a tábor köre) | hóvihar: a vadon lakói behúzódnak (a táborok nem támadnak); zivatar: a táborok idegesek (nagyobb ébresztési kör) |

## 5. Pályaváltozás (ha *hatással + pályaváltozás*)

- A változás **fokozatos**: csempénként, rögzített sorrendben, az időjárás erősségével arányosan. Az időjárás elmúltával vissza is alakul, lassabban.
- A navigáció a meglévő helyi újraszámolással (ADR-006) követi.
- A szerkesztőben jelölhető, hol változhat a pálya. Az alapértelmezést a terep adja: minden alacsony part ártér, minden tó befagyhat.

| Időjárás | Változás |
|---|---|
| **eső** | a jelölt **ártér** csempéin a folyó kiárad: a gázló szélesedik, a part sekély vízzé válik. A földúton és a földön **pocsolyák, sár** keletkeznek (lassítanak). A kivágott erdőből **1–2 fa visszanő** (a tuskókból, hosszabb eső után). |
| **zivatar** | mint az eső, erősebben; ritkán **villám gyújt fát**: a fa leég, a helye tisztás lesz |
| **hó** | **havas** réteg a földön (lassít); a **tavak és a gázlók befagynak**: a jégen át lehet kelni (új útvonalak), olvadáskor visszaalakul, és a jégen álló egység kiszorul a partra |
| **hóvihar** | hótorlaszok a szirtek tövében (ideiglenes akadály) |
| **homokvihar** | **homokdűnék vándorolnak**: a jelölt sivatagi csempéken homoktorlasz keletkezik vagy tűnik el (útvonalak záródnak és nyílnak) |
| **hőség** | a **gázlók kiszáradnak** (átkelhető föld lesz), a mocsár részben kiszárad, a sár földdé válik |
| **köd** | nincs pályaváltozás |
| **hamueső** | a növényzet elszürkül (látvány); hosszan tartva a fák egy része elhal |

**Biztonsági szabályok:**
- Pályaváltozás nem zárhat el kezdőpontot vagy főépületet. A szerkesztő ellenőrzője figyelmeztet, a motor pedig a kezdőpontok körül nem változtat.
- Épület alá nem kerül víz.
- A változások száma tickenként korlátos (a teljesítmény miatt).

## 6. Megvalósítás mérföldkövenként

| Mérföldkő | Feladat |
|---|---|
| M5 | napfény iránya és színe, árnyékforgatás, éjszakai tónus, fényforrások, időjárás-részecskék (eső, hó, homok, köd), pocsolyák és hóréteg látványa |
| M6 | környezeti rendszer a szimulációban (napszak, időjárás, véletlenszám-folyam, mentés), a feltételek, általános hatások, a pályaváltozás motorja, meccsbeállítások, Lua-felület |
| M7 | faji hatások (3–4. pont) adatban, balansz |
| M8 | napóra, időjárás-ikon, előrejelzés, súgó |
| M11 | szerkesztő: éghajlat, ártér, fagyó víz, vándorló dűnék, visszanövő erdő jelölése; küldetésvezérlés |

## 7. Nyitott kérdések (a tulajdonosnak)

- A faji javaslatok (3–4. pont) jók-e így, vagy van, amit a világ tényei másképp kívánnak?
- Az alapértelmezés: napszak *hatással*, időjárás *hangulat* – legyen-e más?
- A Wullof éjszakai láthatatlansága mely egységeket érintse (javaslat: a vadászok és a felderítők)?
