# A tanulmányok forrásai és a tanulságok az új motorhoz

Kelt: 2026-11-12 · alap: a 2026-11-11-i tanulmánycsomag (12 dokumentum és 5 csomag, összesen 65 fájl) és az általuk
megnevezett hivatkozások. Kapcsolódó: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md), [TANULMANY_LELTAR.md](TANULMANY_LELTAR.md).

## 1. Módszer

- A tanulmányokban **354 egyedi hivatkozás** szerepel; ebből kb. 150 szakmai forrás (dokumentáció, cikk, nyílt
  forráskódú motor), a többi bolti oldal, játékismertető vagy hír.
- A motor következő mérföldköveit (M2–M12) közvetlenül érintő forrásokat ellenőriztem és feldolgoztam; a többi
  témánként a 4. pont olvasási listájába került.
- Minden tanulsághoz megadom, **mit jelent a TOTU-ra** és **melyik mérföldkőben** érvényesül. Ahol egy szám vagy
  állítás nem ellenőrzött forrásból jön, jelölöm.

## 2. Tanulságok témánként

### 2.1 Hálózat és determinizmus (M2, M10)

**Forrás:** M. Terrano – P. Bettner, *1500 Archers on a 28.8: Network Programming in Age of Empires and Beyond*
(Game Developer, GDC 2001) – ellenőrizve.

- Fordulók kb. 200 ms-osak; az N. fordulóban kiadott parancs az N+2.-ban fut le, így a hálózati késés alatt a
  szimuláció folyamatosan halad. A fordulóhossz a leglassabb gép és a legnagyobb késés szerint **gyorsan nő és
  lassan csökken** (néhány ms fordulónként).
- Állapot helyett parancsokat küldtek – állapotküldéssel kb. 250 mozgó egység lett volna a korlát.
- A játékos érzete: 250 ms-ig észrevétlen, 250–500 ms játszható, felette zavaró; **az egyenletes késés jobb**, mint
  az ingadozó (állandó 500 ms jobb, mint 80 és 500 között ugráló).
- A játékosok átlagosan 1,5–2 mp-enként adnak parancsot, harcban 3–4/s; **az ismételt, azonos helyre kattintások
  szűrése** sokat csökkentett a forgalmon.
- A szinkronhibák a legmakacsabb hibák voltak; ellenőrzőösszeg a világra, az objektumokra, az útkeresésre és a
  célzásra; az RTS3-ban **rendszerenként jelölt szinkron-makrók**, kiadásból kifordíthatók, konfigurációból
  kapcsolhatók. A szimulációs kód nem függhet helyi állapottól (szabad idő, hardver, beállítás).
- A mérést (fordulónkénti képkockaidő, késés, csomagméret) **a kiadásban is hagyták volna** – a korai eltávolítás
  miatt sok hibát nem vettek észre.

**TOTU-következmények:**
1. **M10 – lockstep-paraméterek:** parancskésleltetés (N+2), alkalmazkodó fordulóhossz (gyors emelés, lassú
   csökkentés), az ismételt kattintások szűrése, a parancsok ellenőrzése küldéskor **és** végrehajtáskor.
2. **M2 – szinkron-ellenőrzés:** rendszerenkénti állapothash (mozgás, harc, gazdaság, AI) a teljes világhash mellett,
   hogy az eltéréskereső megmutassa, **melyik rendszer** tért el először.
3. **Mindig bekapcsolt mérés:** a kiadott játék is gyűjti a tick-, késés- és csomagméret-adatokat (helyben,
   kikapcsolható), a határérték átlépését jelzi.
4. **Hálózati szimulátor** a tesztekben (késés, ingadozás, csomagvesztés) az első hálózati kódtól kezdve.

### 2.2 Fix időlépés és megjelenítés (M2, M5)

**Források:** G. Fiedler, *Fix Your Timestep!* (ellenőrizve); a Recoil motor (Spring-utód) teljesítményleírása
(`ENGINE_PERFORMANCE.md`, ellenőrizve).

- Gyűjtő (akkumulátor): a valós időt gyűjtjük, és amíg van benne egy lépésnyi, léptetünk; a megjelenítés
  `alpha = maradék / lépés` arányban **interpolál** az előző és a mostani állapot között (a D17/P8 döntés alapja).
- **„Halálspirál”** ellen: a mért képkockaidő korlátozása (pl. 0,25 s) és a képkockánkénti lépésszám korlátja – túl
  nagy terhelésnél a játék lassul, nem omlik össze.
- Recoil: 30 Hz-es szimuláció; lemaradáskor a függő lépések sorozatban futnak, **legfeljebb kb. 500 ms-nyi** egyszerre,
  hogy a rajzolás is sorra kerüljön; a párhuzamosítás csak **egy fázison belül, azonos műveletek tömegén** történik –
  így a szimuláció eredménye független a szálütemezéstől. Mérési vödrök: *Sim*, *Update*, *Draw*.

**TOTU-következmények:** a kliens fő ciklusa ezt a mintát követi (M5); a szimuláció párhuzamosítása csak
determinisztikus, azonos műveletekre bontott ciklusokban (M4/M6); a teljesítménymérés három vödre a `perf`-eszközben.

### 2.3 Mozgás, ütközés, útkeresés (M4)

**Források:** D. Pottinger, *Coordinated Unit Movement* (Game Developer, ellenőrizve); Red Blob Games útkeresési
áttekintése (ellenőrizve); ORCA / RVO2 (ellenőrizve); HPA* (Botea–Müller–Schaeffer, 2004 – **a cikk most nem
elérhető**, 404).

- **Pottinger:** állapotgépes mozgás (útra vár → következő pont → cél); **mozgásvonal** (a lépés szakaszként, hogy a
  gyors egység ne ugorjon át akadályt) és **részlépések**; **kemény és puha sugár** (a kemény sosem lapolódhat át, a
  puhát szabad megsérteni); **egységprioritás** az ütközés feloldásához (álló–álló, mozgó–álló, mozgó–mozgó
  esetekre külön szabály); előrejelzett pozíciók gördülő listája; **tervezési emlékezet** (a kikerült egységet egy
  ideig nem vizsgálja újra – nincs oda-vissza ingás); az útkeresőt csak végső esetben hívja; a **formáció** szétesik
  és újra összeáll, ha nincs út egyben.
- **Red Blob:** gráfcsökkentés (útpontok, navigációs háló, hierarchikus keresés, négyesfa), **JPS** nagy, egyenletes
  költségű rácsokra, útkiegyenesítés / Theta* a rácstól független, sima utakhoz, **tájékozódási pontos (landmark)
  heurisztika** statikus pályákon.
- **ORCA / RVO2:** kommunikáció nélküli kölcsönös ütközéskerülés; több ezer ágens néhány ms alatt; a hivatalos
  RVO2-könyvtár **Apache-2.0** (a GPL 3-mal összefér), de **lebegőpontos** – a TOTU-ban fixpontos átirat kell.

**TOTU-következmények (az M4 ADR-jébe):** Pottinger-féle helyi feloldás (kemény/puha sugár, prioritás, tervezési
emlékezet) mint első réteg; hierarchikus keresés + JPS a nagy pályákra (a HPA*-cikk számait az ADR előtt ellenőrizni
kell); flow field az azonos célú csoportokra (a tanulmányok szerint, mért igény esetén); ORCA csak fixpontos
átiratban, ha a mérés szerint a helyi feloldás nem elég.

### 2.4 Modolás és szkript (M2, M3, M13)

**Források:** OpenRA trait-dokumentáció és Lua-API (ellenőrizve); 0 A.D. entitás-dokumentáció (ellenőrizve); Luau
sandbox (ellenőrizve).

- **OpenRA – feltételrendszer:** a legtöbb trait `ConditionalTrait`: `RequiresCondition` (csak akkor él) és
  `PauseOnCondition` (szünetel); más traitek **feltételt adnak** (pl. álcázva, ugrás közben). Ez kód nélkül
  összerakható viselkedést ad – a TOTU kapcsolói, állapotai (D22) és fejlődési ágai ugyanígy kifejezhetők.
- **OpenRA – szkript:** Lua 5.1 sandboxban, **memória- és CPU-korláttal**; a küldetés eseményekre feliratkozó
  triggerekből és az egységekre sorba állított tevékenységekből áll (lásd 2.5).
- **0 A.D. – komponensek:** Identity, Cost, Health, Attack, UnitAI, Auras, **Promotion** (tapasztalatból
  előléptetés), **Capturable** (épületfoglalás), **Formation**, ResourceGatherer, ProductionQueue,
  StatusEffectsReceiver – a TOTU komponensei (ADR-005) ezzel nagyrészt egyeznek; az előléptetés és a foglalás
  ma hiányzik a TOTU-ból.
- **Luau-sandbox:** a veszélyes könyvtárak (io, package, a debug és az os nagy része) eltávolítva; **bájtkód
  betöltése tiltva**; szkriptenként saját globális tér; a közös könyvtárak **csak olvashatók**; a VM függvényhíváskor
  és ciklusonként megszakítást hív (futáskorlát); memóriakorlát beállítható; maradék kockázat: hosszú C-hívás,
  `getfenv`/`setfenv`, a gazda által kitett API.

**TOTU-következmények:**
1. **M3 – Lua 5.4 sandbox:** `io`, `os`, `package`, `debug`, `load`/`dofile` (bájtkód) nélkül, csak szöveges
   betöltés; modonként saját `_ENV` és saját állapot; a közös táblák csak olvasható proxyn át; **utasításszámláló
   horog** (`lua_sethook`, tickenkénti keret) és saját memóriafoglaló korláttal; a gazda-API minden bemenetet ellenőriz.
2. **ADR-005 kiegészítés (javaslat):** **feltételek** a komponensekben (`requires_condition`, `pause_on_condition`,
   `grant_condition`) – a kapcsolók, állapotok, ágak és kutatások egységes kapcsolója.
3. **Játékmenet-javaslat (tulajdonosi döntés):** előléptetés (veterán egységek) és épületfoglalás – 6. pont.

### 2.5 Küldetések és párbeszéd (M11)

**Források:** OpenRA Lua-API (ellenőrizve); a StarCraft triggerrendszer leírása (classic.battle.net, ellenőrizve);
ink (inkle, ellenőrizve).

- **OpenRA:** `Trigger` (késleltetés, halál, sérülés, „mind meghalt”, terület belépés/kilépés, közelség, felfedezés,
  gyártás, időzítő, győzelem/vereség), `Media` (üzenet, hang, zene, film), `Reinforcements` (erősítés, szállítóval),
  `Map` (egységek dobozban/körben, címkék), egységparancsok sorban (mozgás, támadó mozgás, őrjárat, várakozás,
  feltétel adása), **célok** (elsődleges / másodlagos; a győzelemhez az összes elsődleges kell).
- **StarCraft:** játékosonkénti triggerlista (feltételek + akciók), **„Preserve Trigger”** (egyszeri vagy ismételt),
  **kapcsolók** (logikai jelzők), halálszámlálók, kb. 2 mp-es kiértékelés rögzített játékos-sorrendben; korlát: nincs
  változó és elágazás – ezt a WC3 változókkal és a GUI → szkript fordítással oldotta meg.
- **ink:** MIT-licencű, elágazó párbeszédnyelv; a futtató C#-ban (JSON-ra fordított történet), JavaScript-port van;
  **C++-futtatót a hivatalos oldal nem említ** (külső C++-port létezik – ellenőrizendő).

**TOTU-következmények:** a trigger-API az OpenRA mintájára (Lua, eseményfeliratkozás, célok, erősítés), a grafikus
szerkesztőben a StarCraft/WC3-féle esemény–feltétel–akció szerkezettel, ismétlés-jelzővel, változókkal; a
párbeszédformátumról az M11 előtt ADR (ink C++-futtatóval, Yarn Spinner vagy saját, Luában leírt formátum).

### 2.6 Gépi ellenfél (M9) és rangsorolt játék (M10+)

**Források:** microRTS (GPL-3.0, ellenőrizve); Glicko-2 (Glickman, ellenőrizve – a pontos paraméterekhez a teljes
leírás kell).

- **microRTS:** determinisztikus, egyszerűsített RTS a mesterséges intelligencia kutatásához; referencia-ágensek:
  szkriptelt stratégiák, fakeresés (minimax, Monte-Carlo, MCTS), kombinatorikus „bandit” módszerek; versenyek
  2017-től.
- **Glicko-2:** a pontszám mellé bizonytalanság és ingadozás; következetlen eredmény nagyobb változást és nagyobb
  bizonytalanságot ad.

**TOTU-következmények:** M9 – a gépi ellenfél rétegzett: stratégiai szint szkriptelve (Lua, modolható), taktikai
szint (csatakimenet-becslés) később keresőalapú módszerrel; a microRTS ötletei kis tesztpályákon kipróbálhatók.
A rangsoroláshoz Glicko-2 vagy TrueSkill 2 – az internetes mód tervezésekor (D15) ADR.

## 3. Javasolt kiegészítések a motortervhez (összefoglaló)

| # | Kiegészítés | Mérföldkő | Döntés |
|---|---|---|---|
| 1 | Rendszerenkénti állapothash és eltéréskereső | M2 | szakmai – beépítem |
| 2 | Mindig bekapcsolt teljesítmény- és hálózatmérés a kiadásban is (helyben, kikapcsolható) | M2/M10 | szakmai – beépítem |
| 3 | Fő ciklus: akkumulátor, időkorlát, lépéskorlát, interpoláció; sorozatos felzárkózás ≤ 500 ms | M5 | szakmai – beépítem |
| 4 | Pottinger-féle helyi ütközésfeloldás, majd hierarchikus keresés + JPS, flow field, ORCA fixpontosan | M4 | M4 ADR |
| 5 | Lua 5.4 sandbox: könyvtárak, bájtkód tiltva, `_ENV`, csak olvasható közös táblák, utasításkeret, memóriakorlát | M3 | szakmai – beépítem |
| 6 | Feltételrendszer a komponensekben (OpenRA) | M2/M7 | ADR-005 kiegészítés |
| 7 | Lockstep: N+2 parancskésleltetés, alkalmazkodó forduló, kattintásszűrés, kettős ellenőrzés, hálózati szimulátor | M10 | ADR-006 |
| 8 | Trigger-API az OpenRA mintájára, ismétlés-jelző, változók; párbeszédformátum-ADR | M11 | M11 ADR |
| 9 | **Előléptetés (veterán egységek)** – 0 A.D. Promotion | M7 | **elfogadva (D30)** |
| 10 | **Épületfoglalás** – kétféle képességként: elfoglalás és megszállás | M7 | **elfogadva (D30)** |

## 4. Olvasási lista (még feldolgozandó, mérföldkövenként)

- **M4:** HPA* eredeti cikk (Botea–Müller–Schaeffer 2004, új elérhetőség kell); *Game AI Pro* 81. fejezet; a
  C&C Remastered `FINDPATH.CPP` (EA, GPL-3) – a klasszikus útkereső; Zero-K és Recoil mozgásleírások.
- **M5:** SDL3 GPU-dokumentáció; az *Ashes of the Singularity* rajzolási hívásokról szóló írása.
- **M8:** Game Accessibility Guidelines; a Microsoft játék-akadálymentességi irányelvei; FMOD eseményalapú hang.
- **M9:** BWAPI; Dave Churchill StarCraft-AI anyagai; az AIIDE-cikkek (4 db).
- **M10:** Gaffer: *Snapshot Interpolation*, *State Synchronization*, *Snapshot Compression*; OpenRA dedikált szerver;
  ENet; a *Supreme Commander* szálkezelési esettanulmánya; OWASP Game Security Framework.
- **M11:** WC3-terepezési és küldetésleírások; Yarn Spinner; Tiled (sablonok, egyedi tulajdonságok, JSON-formátum –
  pálya-importhoz).
- **M13 / internet:** FAF-szerver (GPL), Zero-K matchmaking, TrueSkill 2, a 0 A.D. modleírásai.
- **Nyílt forrású RTS-motorok mintának:** OpenRA, 0 A.D., Stratagus/Wargus, Warzone 2100, Spring/Recoil, OpenSAGE,
  openage, MegaGlest, Widelands, Mindustry, Homeworld (forráskiadás).
