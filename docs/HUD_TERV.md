# HUD-terv – fajonként egyedi kezelőfelület és zászlók

*© Mesehordo – TOTU: Sky Rift.*

Minden főfaj saját anyagú, saját díszítésű HUD-ot kap, és minden nép (főfaj és szövetséges) saját zászlót. Az alsó sáv két álló zászlója:

- **bal oldali zászló:** mindig a saját faj zászlaja;
- **jobb oldali zászló:** amíg nincs szövetséges, szintén a saját fajé. Ha a Diplomácia csarnokában szövetséges népet választasz, a jobb oldali zászló **a szövetséges népé lesz**. Ez már működik a játékban: a kódból rajzolt zászló a szövetséges nép színeit veszi fel.

Ha egy kép elkészül, elég a megadott néven a helyére tenni: a játék magától használja, és ha hiányzik, kódból rajzolja meg.

## 1. A HUD részei és a képhelyek

```
 [tétlen]                    [ szövetségesek | meteorit | fa | arany | létszám ] [⚙] [Áhítat]      00:00 − ×1 +
                                         ▲ felső sáv: sötét háttér (olvashatóság), fajonként nem változik
 ...
 ┌──────────┐┃┌─────────────────┐┌──────┐┌──────────────┐┃┌──────────────┐
 │ kistérkép│┃│ portré + adatok │ │tárgyak│ │ kijelöltek   │┃│ parancsrács  │   ◄ alsó sáv (hud_<faj>)
 └──────────┘┃└─────────────────┘└──────┘└──────────────┘┃└──────────────┘
          bal zászló                              jobb zászló
          (banner_<faj>)                          (banner_<szövetséges> / banner_<faj>)
       szekció-keretek: frame_<faj>
```

| Kép | Fájl (assets/…) | Méret (PNG) | Mire |
|---|---|---|---|
| Alsó sáv | `ui/hud_<faj>.png` | 3168×428 | Az egész alsó HUD háttere (a játék a sáv méretére nyújtja). A szekciók helye maradjon nyugodt, a díszek a peremre és a szekciók közé kerüljenek. |
| Szekció-keret | `ui/frame_<faj>.png` | 96×96 | 9 szeletes keret minden szekció köré (kistérkép, portré, tárgyak, kijelöltek, parancsrács). A **24 px-es sarkok** változatlanok maradnak, az élek és a közép nyúlik. A közép legyen sötét és egyszínű. |
| Zászló | `ui/banner_<nép>.png` | 88×792 | Álló, keskeny zászló a rúdon, csúcsos vagy rojtos alsó véggel, középtájt a népe jelével. Átlátszó háttér. |
| Címer | `emblems/<nép>.png` | 512×512 | A nép jelvénye. A HUD-on akkor látszik, ha nincs kijelölés, és a tárgyak helyén nem hős egységnél. |
| Kurzorok | `ui/cursors/<faj>_<fajta>.png` | 32×32 | Fajták: `arrow`, `select`, `attack`, `cast`, `build`, `gather`. |
| Létszám-ikon | `ui/res/supply_<faj>.png` | 64×64 | A felső sáv létszám-ikonja. |
| Áhítat-ikon | `ui/res/faith.png` | 64×64 | Az elfek Áhítata és a Huk’ucha Tóéneke (közös ikon). |

A `<faj>` a főfajok kulcsa: `elves`, `ursan`, `croftian`, `wullof`, `imperium`, `hukucha`, `ooni`. A `<nép>` a főfajok és a szövetséges népek kulcsa (4. fejezet).

**Méretarány:** a HUD 1600 képpont széles képernyőn kb. 1584×214 képpont. A fenti méretek ennek kétszeresei (a zászló négyszerese), így 4K-n is élesek maradnak.

## 2. Közös szabályok

- **Ugyanaz a festett stílus,** mint a játék többi képén. Bal-felső fény, puha árnyék, olvasható formák.
- **A HUD ne kiabáljon:** a díszítés a peremeken és a szekciók közötti hézagokban legyen. A szekciók belseje (ahol ikonok, számok és portrék vannak) sötét, nyugodt felület maradjon.
- **Fajonként más az anyag, a díszítés és a forma,** nem csak a szín. Fél másodperc alatt is felismerhető legyen, melyik faj HUD-ja.
- **A felső sáv háttere minden fajnál sötét, majdnem fekete marad,** hogy a számok jól olvashatók legyenek. Ezt nem rajzoljuk fajonként.
- **A csapatszín nem kerül a HUD-ra.** A HUD a faj saját színeit viseli.
- **Nincs szöveg** a képeken.

## 3. Főfajok

### 3.1 Szürke elfek – napkő és jáde

- **Anyag:** világos, napszítta mészkő lépcsőzetes tömbökből (a templomvárosok anyaga), aranyozott peremmel, jáde betétekkel.
- **Díszítés:** napkorongok a szekciók közötti hézagokban, lépcsős (zikkurat) formájú sarkok, tollmotívum a felső sáv két végén.
- **Szekció-keret:** vastag kőperem, a sarkokban kis jáde kocka, belül vékony aranycsík.
- **Zászló:** fehér-arany szövet, jádezöld szegély, középen a napkorongos templom-címer, alul három tollas rojt.
- **Színek:** mészkő (176, 168, 150), arany (226, 190, 96), jáde (40, 190, 150).

### 3.2 Ursanok – gerenda, vas és prém

- **Anyag:** sötét, gyalult tölgygerendák, kovácsoltvas pántokkal és szegecsekkel. A felső peremen prémcsík.
- **Díszítés:** mancsnyomok beégetve, rúnás vaslapok a sarkokban, láncszemek a szekciók között.
- **Szekció-keret:** deszkakeret, a sarkokban vas sarokvasalás.
- **Zászló:** barna gyapjú, kék-ezüst szegély, középen a mancsos pajzs, alul rojtos szél, a rúd végén medvekarom.
- **Színek:** tölgy (92, 62, 40), vas (150, 160, 176), jégkék (120, 180, 255).

### 3.3 Croftianok – hús, kitin és erek

- **Anyag:** lila-fekete kitinlemezek, közöttük lüktető hús és világoszöld erek.
- **Díszítés:** tüskék a peremen, apró szemek a sarkokban, nyálkacseppek a szekciók között.
- **Szekció-keret:** egymásra boruló kitinpáncél-lemezek, a sarkokban tüske.
- **Zászló:** szakadt bőrlebernyeg tüskés rúdon, lila alapon zöld, világító jel, alul cafatos szél.
- **Színek:** lila (118, 74, 140), csont (226, 214, 186), méregzöld (150, 255, 120).

### 3.4 Wullof – bőr, csont és pöttyfestés

- **Anyag:** kifeszített, okkerre festett bőr csontkereten. A bőrt fehér és vörös pontfestés díszíti (álomidő-minták).
- **Díszítés:** bumeránggal keresztezett lándzsák a sarkokban, tollak és fogfüzérek a szekciók között.
- **Szekció-keret:** csontkeret szíjjal átkötve, a sarkokban csomók.
- **Zászló:** festett bőr, koncentrikus pöttykörök, középen a farkas-címer, alul tollak és fogak lógnak.
- **Színek:** okker (196, 150, 92), csont (236, 226, 206), vörös (215, 60, 40).

### 3.5 Aurethi Birodalom – márvány, réz és gőz

- **Anyag:** fehér-szürke márványlapok rézkerettel, szegecsekkel és apró csövekkel. Itt-ott izzó zöld reaktor-üveg.
- **Díszítés:** babérkoszorú a felső sáv közepén, fogaskerekek a sarkokban, sasos fémplakettek a szekciók között.
- **Szekció-keret:** rézprofil, sarkokban négy szegeccsel.
- **Zászló:** vörös-arany légiós vexillum keresztrúdon, középen a sasos címer, alul arany rojt.
- **Színek:** márvány (120, 116, 110), réz (196, 150, 80), reaktorzöld (120, 255, 190).

### 3.6 Huk’ucha – nád, korall és gyöngyház

- **Anyag:** összekötözött nádszálak és sima, zöldes kő, korallágakkal és gyöngyház berakással. A sarkokban üvegbuborék.
- **Díszítés:** tavirózsák a szekciók között, vízcseppek és kis csigák a peremen.
- **Szekció-keret:** nádköteg-keret háncskötéssel, a sarkokban gyöngyházkagyló.
- **Zászló:** zöld-türkiz szövet, középen tavirózsás béka-jel, alul hullámos szél, a rúd tetején gyöngy.
- **Színek:** tavi zöld (70, 150, 80), türkiz (120, 230, 210), gyöngyház (214, 226, 200).

### 3.7 Ooni – lakk, papír és lámpás

- **Anyag:** vörös lakkfa fekete szegéllyel, aranyozott sarokvasakkal, és rizspapír-betétek.
- **Díszítés:** kis kőlámpások a szekciók között, torii-formájú felső sarkok, oni-maszk a felső sáv közepén.
- **Szekció-keret:** lakkozott keret, a sarkokban arany veretek.
- **Zászló:** hosszú, keskeny nobori (álló zászló) fehér alapon, vörös-fekete jellel, alul jádezöld szegély.
- **Színek:** lakkvörös (170, 40, 40), arany (226, 190, 110), jádezöld (80, 200, 140).

## 4. A szövetséges népek zászlói

A zászló a választás után a HUD jobb oldalán áll, és a nép címerét viseli. Fájlok: `ui/banner_<nép>.png` (88×792) és `emblems/<nép>.png` (512×512).

| Nép (kulcs) | Zászló | Címer |
|---|---|---|
| Micéliák (`micelia`) | Barnás gombaszövet, élő fonalakkal átszőve, alul gyökérszerű rojtok. | Lüktető szívgomba fonálhálóban |
| Pinguu (`pinguu`) | Sötét bőrlobogó jégkék szegéllyel, csontrúdon, alul jégcsap-rojtok. | Pingvinfej halszálka-koszorúban |
| Ghoblin (`ghoblin`) | Durva szürke vászon vörös foltokkal, kőlap-súlyokkal az alján. | Csákány és meredek tetejű kőház |
| Mhonyo (`mhonyo`) | Sötétkék szövet vörös szegéllyel, fém fogaskerekek a rúdon. | Penge fogaskerékben |
| Daresh (`daresh`) | Okkersárga, fonott mintás lobogó, alul hatszögletű rojtok. | Darázs hatszögletű lépben |
| Tokla (`tokla`) | Fehér prém vörös díszítéssel, alul gyapjúrojtok. | Szarvas busómaszk |
| Galóca (`galoca`) | Vörös alapon fehér pöttyök (galóca-kalap), tüskés rúdvég. | Fogas, tüskés gombakalap |
| Niil-elfek (`niil_elf`) | Türkiz szövet arany hullámmintával, alul vízcsepp-rojtok. | Kapuoszlop hullámvonalakkal |
| Tűz-elfek (`tuz_elf`) | Vörös-narancs lobogó, lángnyelv-szerű alsó széllel. | Láng a dűne fölött |
| Tendüür (`tenduur`) | Zöld levélszövet aranyszállal, a rúdon függő lámpás. | Szárnyas tündér-sziluett ágon |
| Nila-uchana (`nila_uchana`) | Mélykék selyem narancs szegéllyel, lótuszos alsó vég. | Lótusz nága-fejjel |
| Zhan-zhama (`zhan_zhama`) | Vörös-sárga tollas lobogó, alul kakastoll-rojtok. | Kakasfej napkorongban |
| Shrissh-matah (`shrissh_matah`) | Okkerbarna kéregszövet rúnákkal, tüskés rúdvég, alul tollpihék. | Tollas rovar rúnakörben |

A főfajok címerei a 3. fejezet szerint. A Huk’uchának és az Ooninak még nincs címere (`emblems/hukucha.png`, `emblems/ooni.png`), ezeket is el kell készíteni.

## 6. Ajánlott sorrend

1. **Címerek:** Huk’ucha, Ooni és a 13 szövetséges nép. Ezek a zászlókon és a HUD-on is megjelennek.
2. **Zászlók:** a 7 főfaj és a 13 szövetséges nép.
3. **Szekció-keretek** fajonként. Kevés munka, nagy hatás.
4. **Alsó sávok** fajonként.
5. **Kurzorok és létszám-ikonok** a Huk’uchának és az Ooninak, valamint az Áhítat-ikon.
