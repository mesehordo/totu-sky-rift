# TOTU: Sky Rift – A kölcsönkapott öröklét (RTS prototípus)

Kreátor: **Mesehordo** • A világ és a történet © 2026 Mesehordo, minden jog fenntartva, a grafika szintén; a kód GNU GPL 3 vagy későbbi – részletek: [LICENSE.md](LICENSE.md).

Nyolc aszimmetrikus faj (Szürke elfek / Ursanok / Croftianok / Wullof / Micéliák / Vasbirodalom / Ooni / Huk’ucha), StarCraft-szerű bázisépítés,
C&C-stílusú sidebar, adatvezérelt és moddolható motor Pythonban. A teljes terv: [DESIGN.md](DESIGN.md).

## Indítás

```bash
python -m pip install pygame-ce numpy
```

```bash
python main.py
```

Hasznos kapcsolók:

| Kapcsoló | Jelentés |
|---|---|
| `--faction elves\|ursan\|croftian` | saját nép |
| `--enemy ...\|random` | gépi ellenfél népe |
| `--autostart` | menü kihagyása |
| `--seed N` | térkép-mag |
| `--size 1600x900`, `--fullscreen` | ablakméret |
| `--spectate` | a saját oldalt is gép irányítja (nézőmód) |
| `--simulate 10` | gép a gép ellen szimuláció 10 percig ablak nélkül (balanszteszt) |
| `--mods pelda_mod` / `--no-mods` | modok kiválasztása |

## Főmenü, profilok, mentés

* **Főmenü**: Kampány, Csata (faj, ellenfél, nehézség), Betöltés, Profil, Beállítások, Készítők.
* **Profilok** (Armies of Exigo módra): mindegyiknek saját neve, címere (kedvenc faja), beállításai
  (sebesség, görgetés, FPS, nehézség, felbontás, teljes képernyő), statisztikái (csaták, győzelmek, ölések,
  játékidő fajonként) és saját kampány-haladása. Helyük: `saves/profiles/<név>/`.
* **Mentés/betöltés**: játék közben Esc → Mentés / Betöltés (névvel, bélyegképpel), **F5** gyorsmentés,
  **F9** gyorsbetöltés, a főmenüből is betölthető. A mentés a teljes szimulációt rögzíti (egységek parancsai, gépi ellenfelek,
  küldetés-célok, véletlenszám-állapot), így betöltés után pontosan ugyanúgy folytatódik.
  (A `.sav` Python pickle – csak saját mentést tölts be.)

## Kampány

A főmenü **KAMPÁNY** gombja: fejezetek és küldetések eligazítással, célokkal; a haladás a `saves/campaign.json`-ba
kerül. A *TOTU: Sky Rift* hadjárat 46 küldetést tartalmaz, előjátékkal, faji fejezetekkel, átvezetőkkel és közös befejezéssel. A hadjáratok sorban nyílnak meg. Történet, kampányterv és a küldetés-formátum:
[docs/STORY_CAMPAIGN.md](docs/STORY_CAMPAIGN.md); az új fajok: [docs/FACTIONS.md](docs/FACTIONS.md);
moddolás: [docs/MODDING.md](docs/MODDING.md).

## Gazdaság és fajok (2. változat)

* **Három nyersanyag:** fa, kő, meteorit (étel nincs). **Létszám:** az egységek eltérő helyet foglalnak; a korlátot
  fajonként más növeli – elfek: Napobeliszk, ursanok: Tanya, croftianok: Szemhordozó (egység), wullof: az
  Emukarámban tenyésztett emuk (karámonként legfeljebb 5), gombanép: a hálózat összekötött épületei, Vasbirodalom:
  lélekreaktor.
* **Felemelkedés és 3 fejlődési irány fajonként:** amint 3 kész épületed van (a főépületen kívül), a nép magától,
  ingyen felemelkedik: egy ablakban végleg választasz a faj 3 iránya közül (pl. wullof: Bölény-csorda, Groomi-fészek, Ősök útja). Az irány
  saját épületet, egységeket és szabályt nyit meg. Adatok: `tools/factions_paths.py`.
* **Egyedi fajmechanikák:** elf energiamező és önépítő templomok; ursan helyőrség (a bent lévők erősítik az épület
  lövéseit); croftian lárvák, mutáció (Karmos → gubó → Páncélbogár) és fertőzés; wullof nomád táborok és falkaerő;
  gombanép spórafonal-hálózat; Vasbirodalom lélekidézés, gépjavítás, ostrom. Részletek: [docs/FACTIONS.md](docs/FACTIONS.md)
  (a játékadatokból: `python tools/gen_faction_doc.py`).
* Az adatok forrása: `tools/factions_v2.py` (fajok, hősök, képességek), `tools/economy_v3.py` (gazdaság),
  `tools/factions_v3.py` (képességek), `tools/factions_aussie.py` (wullof) és `tools/factions_paths.py` (irányok) –
  a lánc újrafuttatható: `python tools/factions_v2.py`.

## Egyéni csata, 20 pálya, LAN, angol nyelv

Warcraft II-szerű játékindító képernyő lenyíló menükkel, 20 pálya 2–8 játékosra (csapatpályákkal), gépi ellenfél
könnyű / közepes / nehéz fokozatban, LAN-többjátékos (lobbi, csevegés, lockstep-szinkron), és teljes angol nyelv
(Beállítások → Nyelv). Részletek: [docs/MULTIPLAYER.md](docs/MULTIPLAYER.md).

## Parancskártya és képességek

Minden egységnek van legalább egy aktív vagy passzív képessége. A kijelölt egység(ek) parancskártyája (6×2 ikon,
WC3/SC-szerűen): Mozgás (M), Állj (S), Tart (H), Támad (A), majd a képességek ikonnal és gyorsbillentyűvel,
újratöltés-órával és árral (energia kék, életerő piros, spóra zöld); a passzív képességek halványan, súgóval.
Az egységek nem lóghatnak egymásra (csak a gyűjtés közben dolgozó munkások). Fajonként más a varázslatok
„üzemanyaga” – lásd [docs/FACTIONS.md](docs/FACTIONS.md).

## Hősök és RPG-elemek (Warcraft III-szerűen)

* **Hősök**: fajonként **3 féle**, a faj hősidéző épületéből (Napkapu, Esküfa, Kórus-kút, Ősök köre, Emlékfa,
  Panteon); egyszerre legfeljebb három. Elesés után ugyanott **újraéleszthetők** a megszerzett szinttel,
  képességekkel és tárgyakkal. A 18 hős listája és képességei: [docs/FACTIONS.md](docs/FACTIONS.md).
* **Tapasztalat és szintek** (1–10): a közelben (12 mező) elpusztított ellenség és szörny tapasztalatot ad;
  szintenként nő az életerő, a sebzés, a páncél és az energia (`growth`), és jár egy **képességpont**.
* **Négy képesség hősönként**: célzott csapás (sebzés + lassítás/bénítás), aura (a közeli szövetségesekre),
  idézés / területi varázslat, és egy **végső képesség** 6. szinttől. A szokásos képességek 3 szintig fejleszthetők
  (2. szint a 3., 3. szint az 5. hősszinttől). Tanulás: **Tanulás (+N)** gomb vagy **T**, majd a képesség gombja /
  gyorsbillentyűje (**Q W E R**).
* **Tárgyak és leltár** (6 hely): italok (életerő, energia), tómok (tapasztalat, örök bónusz), viselhető tárgyak
  (csizma, gyűrűk, amulett, pajzs, csillagérc-penge, árnyköpeny, az Örök Hajnal szilánkja) – közönséges, ritka és
  ereklye ritkaság. Felvétel: jobb klikk a földön heverő tárgyra; leltár: bal klikk = használat, jobb = eldobás.
* **Semleges szörnytáborok** (Vadon): kristálybogarak, hamufarkasok, éterlidércek, kőtroll, ércgólem – a térképen
  tükrözve, középen a legerősebb. Csak a táboruk közelében támadnak, túl messzire nem követnek; a kiirtott tábor
  tárgyat dob. Csatában mindig, a hadjáratban a fejezetek 2. küldetésétől vannak.
* **Hadjárat**: a hősök fejlődése a profilban marad, és a következő küldetésben folytatódik (minden küldetésnek
  van minimális kezdő hősszintje).
* **Gépi ellenfél**: képez hőst (félreteszi rá a nyersanyagot), képességet tanul, célzott csapást és gyógyítást
  használ, italt iszik, tárgyat szed fel, és kis csapattal kitakarítja a közeli táborokat.

## Irányítás (F1 a játékban)

* **Bal klikk / húzás**: kijelölés (Shift: hozzáad, dupla klikk: azonos típus a képernyőn)
* **Jobb klikk**: kontextus-parancs – mozgás, támadás, gyűjtés, segít építeni / javít (ursan), gyülekezőpont
* **A** + klikk: támadó mozgás, **S**: állj, **H**: tartás, **Shift**: parancssor
* Képességek: panel-gombok vagy gyorsbillentyű (Pajzsfal **F**, Napvihar / Őrület köde **T**, Vágta / Roham **V**)
* **Sidebar**: épület-ikon → lerakás; egység-ikon → kiképzés (a legrövidebb sorú épületben, Shift: 5×); jobb klikk → visszavonás.
  **B / N / U** fülváltás
* **Ctrl+1..9** csoport, **1..9** kijelölés (duplán odaugrik), **Szóköz** bázis / utolsó riasztás
* Kamera: nyilak, képernyőszél, középső gomb húzása, radar-klikk. **+/-** sebesség, **P** szünet
* Wullof: **F** tábor felszedése, **L** letelepítés; Micéliák: **M** összeolvadás; Vasbirodalom: jobb klikk sérült gépre
  = javítás, **R** lélekidézés, **O** ostrom-mód
* Hősök: bal felső portrék (klikk: kijelölés, dupla: odaugrás), **F2/F3/F4** hős kijelölése, **T** tanulás,
  **Q W E R** hős-képességek, jobb klikk tárgyra: felvétel

## Moddolás röviden

Minden tartalom JSON a `data/` mappában. Egy mod a `mods/<név>/` mappában ugyanazokat a relatív
útvonalakat használja, és **mélyen összefésül** (a `null` törli a kulcsot):

```
mods/pelda_mod/
  mod.json              {"name": ..., "enabled": false, "priority": 10}
  game.json             kezdő nyersanyag felülírása
  factions/ursan.json   új egység + a Vadak karáma termelési listája
  script.py             on_load(data), on_game_start(game), on_tick(game, dt)
```

Új faj: tegyél egy teljes `factions/<id>.json`-t a modba (lásd a meglévőket) – a menü automatikusan felveszi.
Egység-alakzatok (`shape`): worker, warrior, shieldbearer, archer, rider, caster, beast, flyer, bug, spitter,
artillery, horror. Épület-stílusok: aztec, nordic, organic. Képesség-típusok: toggle, area, autoheal, self_buff.

## Grafika (pixel-art sprite-ok)

A `assets/sprites/` mappában minden egységnek, épületnek, nyersanyagnak, lövedéknek, effektnek és a terepnek
valódi, átlátszó hátterű PNG sprite sheetje van, mellette géppel olvasható JSON metaadattal, és egy
`manifest.json`, amit a játék automatikusan betölt (azonosító alapján). A StarCraft 1-hez hasonlóan
**előrenderelt 3D-ből** készülnek: `tools/spritegen/` csontvázas primitív modelleket renderel numpy-jal
ferde vetítéssel, z-pufferrel, poszterizált fénnyel, sötét körvonallal és vetett árnyékkal.

* Egységek: 8 irány (sorok: K, DK, D, DNy, Ny, ÉNy, É, ÉK); animációk: `idle`, `walk`, `attack`, `death`
  (+ `work` a munkásoknak, `cast` a varázslóknak). A csapatszín külön maszkból (`*_team.png`) kerül rá.
* Épületek: `idle` animáció (füst, izzás, lüktetés) + 4 építési fázis; nyersanyagok változatokkal / állapotokkal.
* Ha egy (pl. modból jövő) egységnek nincs sprite-ja, a játék a régi, kódból rajzolt alakot használja.
* v2: 2× szuperszampling, környezeti árnyékolás, fém-csillanás, anyagtextúrák (tégla, deszka, bunda, pikkely),
  sprite-onkénti 96–128 színű paletta; WC2-szerű terepátmenetek (zajos határ, homokos vízpart), sziklatömbök.
* v3 (WC2-szintű részletesség): festett, 6 fokozatú fény-rámpák meleg csúcsfénnyel és hideg árnyékkal,
  telítettebb színek, peremfény; az egységeken szíjak, övtáskák, díszítések, nyakláncok, fonatok; az épületek
  körül hordók, ládák, zsákok, urnák, kosarak, fogaskerekek, tojások; a terepen virágok, fűcsomók, kavicsok,
  gombák, csontok, tuskók, farönkök, bokrok, nád a parton, tavirózsák (`props/doodads.png`). Új effektek:
  `hosszutel` (fagyos vihar), `lelegzet` (gyógyító fény), `levelup` (szintlépés).
* **v4 – egységes, kézzel rajzolt hatású stílus** (alapértelmezés, minden fajra): 3/4-es kamera, festett fények,
  2× felbontású render és festett lekicsinyítés, éles sötét körvonal, peremfény, belső tintavonalak, 48 színű
  sprite-paletta, zömök-hősies arányok, fajcímerek a pajzsokon és zászlókon (mancs, nap, szem, holdsarló, spóra, sas).
  Az ursanok sisak és ruha nélkül, ágyékkötőben, nagy négyszögletes deszkapajzzsal; épületeik viking–jurta keverékek.
* **Retró megjelenés:** a játéktér duplázott pixelekkel jelenik meg (Beállítások → Pixelnagyítás 1×/2×), mint a
  Warcraft II 640×480-on; a sprite-ok és csempék közös, 224 színű játékpalettán állnak rendezett (Bayer)
  ditherrel (`tools/make_palette.py`); festett, bronzkeretes kezelőfelület.
* **Grafikai munkacsomag:** a megrajzolt képek visszahozása: `python tools/import_drawn.py <mappa vagy zip>`
  (csak a változott képeket viszi be, a 2× rajzot lekicsinyíti, mentést készít); képlap-eszköz:
  `tools/sheet_frames.py`. **Logó és nyitókép:** `python tools/make_intro.py`
  (assets/ui/logo.png, assets/ui/bg/intro.png, menu.png – indításkor nyitókép, a menüben logó és festett háttér).
* **Fajonkénti kezelőfelület és egérkurzor** (`rts/faction_ui.py`): a játékos faja szerint más az oldalsáv anyaga,
  kerete, díszítése és a kurzorkészlet (nyíl, kijelölés, támadás, varázslat, építés, gyűjtés); kézzel rajzolt
  változat: `assets/ui/sidebar_<faj>.png`, `assets/ui/cursors/<faj>_<fajta>.png`.
* **Játéksebesség:** csata előtt (Egyéni csata → Sebesség, 0,5×–3×) és játék közben (bal felül −/+ gomb, vagy a
  +/− billentyű); hálózati játékban a gazda állítja, mindenkinél egyszerre. **Gépi irány:** az egyéni csata
  beállításánál gépi ellenfelenként megadható, melyik fejlődési irányt válassza (vagy Automatikus). A megrajzolt arcképeket (`assets/portraits/`), képesség-ikonokat
  (`assets/abilities/`), kutatás-ikonokat (`assets/research/`), parancsikonokat (`assets/ui/icons/`) és irány-címereket
  (`assets/paths/`) a játék magától használja, ha a fájl létezik.
* **Kézzel festett sprite-csomag** (`assets/pack/`, csak kérésre: `RTS_PACK=1`): a
  `meteor_rts_sprite_alapcsomag` és `meteor_rts_animaciok` csomagból – szürke elf, ursan és croftian egységek,
  épületek (az ursanoknál alapozással induló építési fázisokkal, a croftianoknál „kinövéssel”), fák, kő-, bogyó- és
  meteoritlelőhelyek. A megfeleltetés: `tools/pack_map.json`. A csomag egy nézőirányú (DK): a nyugati irányok
  tükrözéssel készülnek; a Pajzshordozó, Bárdhajító, Obszidiánőr, Csillagdárdás, Hasító és Tűköpő kézzel rajzolt
  animációt kap, a többi egység egyszerű generált mozgást (ringás, előredőlés, eldőlés). Az ursan kék posztó
  csapatszínre vált; a többi festett egység alatt csapatszínű gyűrű jelzi a gazdát. Újraimportálás:
  `python tools/import_pack.py --base <alapcsomag.zip> --anim <animációk.zip>`.
  A Wullof, a Micéliák, a Vasbirodalom és a Vadon egységeire (és az ursan Viharhollóra) még nincs festett
  sprite – ezek generáltak maradnak.
* **Tárgyikonok** (`assets/items/<tárgy>.png`, 48 px): 3D-ből renderelt, ritkaság szerinti keretes leltár-ikonok.
* **Faj-címerek** (`assets/emblems/`): Warcraft III fajlogóinak stílusát idéző, domborított, fémes emblémák –
  `python tools/make_sprites.py --emblems`. Modban a faj `emblem` mezője saját képet adhat meg.

```bash
pip install numpy pillow
```

```bash
python tools/make_sprites.py
```

```bash
python tools/check_sprites.py --preview
```

Az első parancs újragenerálja az összes sprite-ot (néhány perc, minden magon párhuzamosan; `--only id1,id2`
csak a megadottakat), a második ellenőrzi a méreteket, az átlátszóságot, a képkockákat és a lefedettséget.

## Balansz-teszt

```bash
python tools/balance.py --seeds 4
```

Minden fajpárt mindkét kezdőhelyről gép a gép elleni meccsekben lejátszik párhuzamosan, és győzelmi arányt számol.

## Felépítés

```
main.py              menü, fő ciklus, szimulációs mód
rts/data.py          adat- és modbetöltés, validálás
rts/world.py         térkép és generátor
rts/pathfinding.py   A* + simítás
rts/entities.py      egység/épület állapotgépek, lövedékek, területi hatások
rts/game.py          szimuláció és parancs-API (a felület és a gépi ellenfél közös)
rts/ai.py            gépi ellenfél
rts/heroes.py        hősfejlődés, képességszintek, aurák, tárgyak, semleges szörnytáborok
rts/render.py        procedurális grafika
rts/ui.py            C&C-stílusú kezelőfelület
rts/sprites.py       sprite-betöltés, csapatszínezés
rts/campaign.py      kampányok, küldetés-rendező (célok, triggerek)
rts/menu.py          főmenü és képernyők (csata, kampány, betöltés/mentés, profil, beállítások)
rts/profiles.py      játékosprofilok
rts/savegame.py      mentés és betöltés
campaigns/           kampány- és küldetésfájlok
tools/make_sprites.py, tools/spritegen/   sprite-generátor
tools/check_sprites.py                    sprite-ellenőrző
tools/balance.py                          balansz-teszt
```
