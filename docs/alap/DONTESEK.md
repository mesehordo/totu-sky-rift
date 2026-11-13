# Tulajdonosi döntések

Kelt: 2026-11-11. Ezek azok a kérdések, amelyekben a tanulmányok egymásnak vagy a játéknak ellentmondanak, és amelyeket a kódból nem lehet eldönteni.
Minden kérdésnél: tény → lehetőségek → **javaslat** → mit befolyásol. A többi kérdést szakmai alapon döntöm el, és az alapdokumentumban jelzem.

Állapot: `NYITOTT` / `ELDÖNTVE (dátum, választás)`.

## Összefoglaló (2026-11-11)

| # | Döntés |
|---|---|
| D1 | FELÜLÍRVA (2026-11-12) a D18-cal: a motor **C++20**, nulláról – [ADR-002](dontesek/ADR-002-uj-motor.md); a korábbi C#-döntés (ADR-001) az OpenBSD/FreeBSD-követelmény miatt elesett |
| D2 | ELDÖNTVE (2026-11-12, [ADR-003](dontesek/ADR-003-megjelenito.md)): saját vékony RHI, első háttér OpenGL 3.3 core, második SDL3 GPU (Vulkan/Metal/D3D12). Korábban: **raylib-cs** (raylib, zlib), **OpenGL 3.3 alap**; 4.x csak opcionális magas grafikai profilban, játékszabályt nem érinthet. Rajzolási próba 2026-11-12: 1600 egység mind látható p99 10,5 ms, 2400 egység p99 15,8 ms (RX 550) – **megerősítve** |
| D3 | MÓDOSÍTVA (2026-11-12): minden rendszer, amin a 0 A.D. fut – Windows, Linux, macOS (Intel, Apple Silicon), FreeBSD, OpenBSD |
| D4 | ÚJRANYITVA (2026-11-12): az M10 előtt döntendő; javaslat: lockstep relészerverrel ([UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) 6. pont). Korábban: szerverautoritatív LAN |
| D5 | ELDÖNTVE (2026-11-11): az internetes mód közvetlenül a szerverautoritatív LAN után |
| D6 | ELDÖNTVE (2026-11-11): 30 Hz marad |
| D7 | ELDÖNTVE (2026-11-11): 8 × 200 pop, lépcsőkben; 2000–2400 csak terheléses próba |
| D8 | ELDÖNTVE (2026-11-12, módosítva): a kód **GPL-3.0-or-later** (mint az OpenRA); a világ, a történet és a grafika minden joga fenntartva (modkivétellel); szabályadatok és modding-doksi CC BY 4.0. Az eladást a GPL nem tiltja – ezt a tulajdonos tudomásul vette |
| D9 | ELDÖNTVE (2026-11-11): teljes 0 A.D.-szintű modolás (sandboxolt szkript, modkezelő, függőségek, modlock) |
| D10 | ELDÖNTVE (2026-11-11): átállás verziózott JSON-mentésre, a régi mentések egyszeri átalakításával |
| D11 | ELDÖNTVE (2026-11-11): 1 szövetséges nép, annak 3 épületével |
| D12 | FELÜLÍRVA (2026-11-12) a D28-cal. Korábban: a Micéliák rejtett alapfaj (csak a Gombanép-hadjáratban játszható) |
| D13 | ELDÖNTVE (2026-11-11): patrol és stance is bekerül |
| D14 | ELDÖNTVE (2026-11-11): nincs régi tesztgép – a minimum becslés marad, a dokumentáció ezt jelzi |
| D15 | ELDÖNTVE (2026-11-11): a rangsorolt verseny cél; az internetes mód tervezésében számolunk vele |
| D18 | ELDÖNTVE (2026-11-12): **új motor nulláról, C++20** – minden 0 A.D.-rendszeren; 0 A.D. + OpenRA szintű modolás, WC3-szintű küldetések; terv: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) |
| D19 | ELDÖNTVE (2026-11-12): a játéklogika, a modok, az AI és a küldetések szkriptnyelve **Lua** (5.4, sandbox, determinisztikus) |
| D20 | ELDÖNTVE (2026-11-12): **két Conquest-jellegű mód** – győzelmi feltételek és hadjárattérkép; a játékbeli nevüket a tulajdonos adja meg |
| D31 | ELDÖNTVE (2026-11-12): az új hadjárat váza – mind a hét főnépnek saját hadjárata (kb. 41 küldetés); Mór szövetséges hősként más fejezetekben, a gombanép-szál a VII. fejezetbe olvad – [KULDETESEK_TERV.md](KULDETESEK_TERV.md) |
| D30 | ELDÖNTVE (2026-11-12): **épületelfoglalás és megszállás** (két képesség) és **előléptetés** (veterán egységek) – [spec/ELFOGLALAS_ES_ELOLEPTETES.md](spec/ELFOGLALAS_ES_ELOLEPTETES.md) |
| D29 | ELDÖNTVE (2026-11-12): **kitsu-mata** (CF-006) – az `oon_kitsune` lövész lesz: 80 életerő, 12 sebzés, 5,5 lőtáv, 1,4 s, 2,8 sebesség, 70 fa + 30 arany + 10 meteorit, 20 s; energia és képesség nélkül (saját képesség később) |
| D28 | ELDÖNTVE (2026-11-12): a Micéliák csak szövetséges kisnép és mod-faj; a Micélia- és a novella-küldetések kikerülnek; a küldetések az új motorra nulláról készülnek (M11) |
| D27 | ELDÖNTVE (2026-11-12): ADR-003 (megjelenítő), ADR-004 (fixpontos szimuláció), ADR-005 (JSON-sablonok öröklődéssel és komponensekkel) elfogadva – az M0 lezárva |
| D26 | ELDÖNTVE (2026-11-12): kisnépek – egy kimondott szövetségi bónusz a gazda seregére, a többi kutatás csak saját; a tó a gazda népét nézi; zsoldosbérlés a kisnép-táborokból beállítással |
| D25 | ELDÖNTVE (2026-11-12): hősök, tárgyak, táborok – állandók adatba, elemek állapotként; tárgyak képességhordozók; bolt (semleges kereskedő, faji bolt) beállítás szerint; táborok újraéledése pályabeállítás |
| D24 | ELDÖNTVE (2026-11-12): kutatások – egységes módosítórendszer (0 A.D.), többszintű kutatások (WC3), a viselkedés-szabályok passzív képességek |
| D23 | ELDÖNTVE (2026-11-12): **mod.io-támogatás** a modrendszerben – kiegészítő online modtár, nem a modrendszer alapja; a játék nélküle is teljesen modolható |
| D22 | ELDÖNTVE (2026-11-12): képességmodell – faji varázserő-forrás (a hősök és a varázslók energiát használnak); egységes alapértékek, a varázssebzés mindenhol a szorzóval nő; bekerül az ellenséges aura, az eloszlatás és immunitás, a varázslási idő és csatornázás, az automatikus használat; élőt a gyógyítás, gépet a javítás gyógyít |
| D21 | ELDÖNTVE (2026-11-12): a specifikáció furcsaságai – a közeli hős eleste azonnal megtöri a morált; a fajnévhez kötött szabályok adatkulcsok lesznek; a mélységi rothadás a főépülethez közelebbi fáknál kezdődik; a felesleges `detect_units` és `balance_v6` törölve |
| D17 | ELDÖNTVE (2026-11-12): a cél a 0 A.D. / AoE / WC3 / SC1 minősége; **nem nulláról újraírás**, hanem bitre azonos port + Q1–Q6 minőségi szakasz mérhető kapukkal; a kliens `RenderSnapshot`-ot kap és interpolál (P8); a D4 az F5 előtt újranézendő |
| D16 | ELDÖNTVE (2026-11-12): **szabálybefagyasztás a C#-port idejére** – a Python-játékban csak hibajavítás és adatban (JSON) felvehető tartalom; új mechanika (pl. D13 patrol/stance) a port után, a C#-magban |

### S1 – Sorrend (2026-11-11)
Előbb az A/B mérés (C#, C++, Python, azonos szimulációs feladat). Az Arcade-átállás csak akkor indul, ha a mérés után a Python marad; ha natív motor nyer, a megjelenítő a natív motor része lesz.

### Megjegyzések a döntések következményeiről
- **D1 + D9:** a 0 A.D.-szintű modolás sandboxolt szkriptnyelvet kíván. Pythonban ez nehezen oldható meg biztonságosan; ez az A/B döntés egyik szempontja lesz.
- **D4 + D6:** szerverautoritatív modellben a 30 Hz-es szerver a 8 kliens felé több adatot küld; a snapshot-gyakoriság külön, mért paraméter lesz.
- **D8:** ha a grafika korábban CC BY 4.0 alatt már nyilvánosan megjelent, az akkor kiadott példányokra a CC BY visszavonhatatlan; az új licenc az ezután kiadottakra vonatkozik.
- **D14:** a minimumgépet nem tudjuk igazolni; a kiadásban „ajánlott” gépigény szerepel, mért adat alapján.


---

### D1 – Motorirány: marad a Python, vagy natív átírás? · ELDÖNTVE (2026-11-11): C# (.NET 10) – ADR-001

**Tény:** a játék ~24 000 sor működő Python-kód. A szimuláció már grafikamentes. A mért lassulás oka elsősorban az algoritmus (útkeresés), nem a nyelv. A 01/02 tanulmány a Python mellett döntött (R), a délutáni C++/C# csomag a teljes átírást vizsgálja.
**Lehetőségek:** (a) Python marad, fokozatos javítás; (b) a Python játék megmarad, és mérés után, egy szimulációs magon C#/C++ A/B prototípus készül; (c) teljes átírás most.
**Javaslat: (a), a (b) lehetőségét nyitva tartva.** Előbb javítsuk az algoritmusokat, és mérjük meg, hol a Python valódi plafonja. Natív magról csak mért adat alapján döntsünk.
**Befolyásolja:** minden mást. Átírásnál a D2, D9 és D10 kérdés más választ kap.

### D2 – Megjelenítő: pygame-ce marad, vagy Arcade (OpenGL)? · ELDÖNTVE (2026-11-12): raylib-cs, OpenGL 3.3 alap

**Tény:** a teljes kliens pygame-ce-re épül (~13 000 sor). A tanulmányok az Arcade-ot a régi Intel GPU-k miatt javasolják. A rajzolási időt még nem mértük.
**Javaslat:** a pygame-ce marad. A megjelenítő cseréje csak akkor jöjjön szóba, ha a mérés szerint a rajzolás a szűk keresztmetszet.
**Befolyásolja:** a grafikai munkát és a minimum-gépigényt.

**Újranyitva (2026-11-11, ADR-001 után):** a megjelenítő C#-ban készül. Lehetőségek:
(a) **MonoGame (DesktopGL)** – kiforrott 2D-keretrendszer (SpriteBatch, szöveg, hang, bemenet), Windows/Linux/macOS, OpenGL; Ms-PL licenc, ami az MIT-kód mellett nem akadály;
(b) **raylib-cs** – egyszerű, könnyű 2D API (raylib, zlib licenc), OpenGL 3.3; a C#-kötés külön karbantartott;
(c) **Silk.NET / OpenTK + saját megjelenítő** – a legtöbb kontroll (atlasz, köd-shader), de a legtöbb munka.
**Döntés (2026-11-12):** a MonoGame (és az FNA, KNI) Ms-PL licence miatt kiesett; **raylib-cs** lett, OpenGL 3.3 alappal (a macOS legfeljebb 4.1-et tud, a régi Intel GPU-k 3.3–4.0-t). Tartalék: Silk.NET (MIT, szintén OpenGL).

### D3 – Platformok · ELDÖNTVE (2026-11-11): Windows + Linux; macOS akkor, ha lesz tesztgép

**Tény:** a kódban nincs Windows-specifikus rész, de csak Windowson próbáltuk ki. A macOS-hez (Intel és Apple Silicon) tesztgép és fejlesztői aláírás kell.
**Lehetőségek:** (a) Windows; (b) Windows + Linux; (c) Windows + Linux + macOS kötelező.
**Javaslat: (b).** A macOS akkor kerüljön elő, ha van tesztgép.

### D4 – LAN-hálózat: lockstep marad, vagy szerverautoritatív? · ELDÖNTVE (2026-11-11): szerverautoritatív LAN

**Tény:** a mai LAN lockstep, és ugyanazon a gépen determinisztikus. A tanulmányok a csalásvédelem és a gyenge kliensek miatt szerverautoritatív modellt javasolnak. Vegyes operációs rendszerek közötti lockstep lebegőpontos eltérés miatt kockázatos.
**Javaslat:** a LAN-on a lockstep marad. Először ki kell próbálni két gépen; ha ott működik, nincs ok lecserélni.

### D5 – Internetes játék és dedikált szerver prioritása · ELDÖNTVE (2026-11-11): az internetes mód közvetlenül a szerverautoritatív LAN után

**Tény:** nincs megvalósítva. Ez a legnagyobb munka a tanulmányokban (lobby, szűrt snapshot, rejoin, biztonság).
**Lehetőségek:** (a) a közeljövő célja; (b) később, a LAN és az egyjátékos mód csiszolása után; (c) nem cél.
**Javaslat: (b).**

### D6 – Szimulációs frekvencia: 30 Hz marad, vagy 20 Hz? · ELDÖNTVE (2026-11-11): 30 Hz marad

**Tény:** a játék 30 Hz-en fut, és a balansz (sebességek, cooldownok) erre van hangolva. A tanulmányok 20 Hz-et írnak a szerver terhelése miatt.
**Javaslat:** a 30 Hz marad. Váltani csak akkor érdemes, ha a mérés szerint ez az egyetlen út a 8 játékosos célhoz; akkor is minden időzítőt át kell számolni.

### D7 – Méretcél · ELDÖNTVE (2026-11-11): 8 × 200 pop, lépcsőkben; 2000–2400 csak terheléses próba

**Tény:** 8 játékos, 300 egység mellett ma a tickek 35%-a túllépi a keretet. A cél 8 × 200 poppont.
**Javaslat:** a cél marad, lépcsőzetesen: 8P/300 egység tisztán → 500 → 800 → 1600 pop. A 2000–2400-as szint nem cél, csak terheléses próba.

### D8 – Licenc · ELDÖNTVE (2026-11-12): kód GPL-3.0-or-later, grafika és történet fenntartva

**Tény:** a `LICENSE.md` szerint a kód MIT, a világ és a szöveg minden joga fenntartva, a grafika CC BY 4.0. A kézikönyv GPLv2+-t javasolt célként.
**Javaslat:** az MIT marad, mert egyszerűbb és nem korlátozza a függőségeket.
**Kérdés a grafikához:** biztosan CC BY 4.0 legyen a grafika? Ezzel bárki szabadon felhasználhatja, akár más játékban is.

### D9 – Modolás szintje · ELDÖNTVE (2026-11-11): teljes 0 A.D.-szintű modolás (sandboxolt szkript, modkezelő, függőségek, modlock)

**Tény:** ma JSON-modok vannak, plusz korlátlan Python-szkript (`script.py`). A C++/C# csomag 0 A.D.-szintű, sandboxolt Lua-modolást ír elő kötelezően.
**Lehetőségek:** (a) a mai szint marad; (b) adatmodok bővítése, a Python-szkriptek csak helyi, megbízható modokban; (c) teljes 0 A.D.-szint, Lua-sandbox, modkezelő.
**Javaslat: (b) most, a (c) csak akkor, ha az online modolt játék cél lesz.**

### D10 – Mentésformátum · ELDÖNTVE (2026-11-11): átállás verziózott JSON-mentésre, a régi mentések egyszeri átalakításával

**Tény:** a mentés pickle. Kódváltozásra érzékeny, és egy idegen mentés kódot futtathat.
**Javaslat:** később verziózott JSON-mentésre áll át, a régi mentések egyszeri átalakításával. Addig a mentést csak saját gépről szabad betölteni.

### D11 – Szövetséges kisfaj: egy nép vagy három hely? · ELDÖNTVE (2026-11-11): 1 szövetséges nép, annak 3 épületével

**Tény:** a játékban (és a korábbi terved szerint) a Diplomácia csarnokában egy kisfaj választható, és annak 3 épülete nyílik meg. Több tanulmány „3 szövetséges slotot” említ.
**Javaslat:** marad egy nép a 3 épületével. A tanulmányok „3”-a valószínűleg a 3 épületből lett félreértve.

### D12 – A Micéliák mint játszható főfaj (mod) · ELDÖNTVE (2026-11-11): rejtett alapfaj, a mod kikapcsolt példa-mod

**Tény:** a kánon (`SZOVETSEGES_NEPEK.md`) szerint a Micéliák szövetséges nép, és `ally_micelia` néven így szerepelnek a játékban. Emellett a `mods/micelia` (bekapcsolva) egyéni csatában is választhatóvá teszi a Micéliákat teljes népként, ezért gép–gép meccsekben is előkerültek.
**Lehetőségek:** (a) törlés; (b) megtartás kikapcsolt példa-modként; (c) marad így.
**Javaslat: (b).**

**Helyesbítés (2026-11-11):** először tévesen azt írtam, hogy a mod kikapcsolása elrontja a Gombanép-hadjáratot. Valójában a `mycel` faj már az alapadatokban megvan rejtett fajként (`data/factions/mycel.json`, `hidden: true`), és a mod csak a rejtést oldja fel. Ellenőrizve: mod nélkül mind az 5 Gombanép-küldetés elindul és fut, egyéni csatában a faj nem választható.
**Végrehajtva:** a `micelia` mod alapból kikapcsolt példa-mod (`mod.json`), és a `saves/mods.json` engedélyezett listájából is kikerült.

### D13 – Hiányzó parancsok: patrol és stance · ELDÖNTVE (2026-11-11): patrol és stance is bekerül

**Tény:** a tanulmányok kötelezőnek írják a járőrözést és a harci magatartást (agresszív / védekező / helytartás). A játékban a „Tart” (hold) megvan, a patrol és a stance nincs.
**Javaslat:** a patrol kerüljön be. A stance helyett elég a meglévő Tart és Állj, hacsak nem kéred kifejezetten.

### D14 – Minimum gép · ELDÖNTVE (2026-11-11): nincs régi tesztgép – a minimum becslés marad, a dokumentáció ezt jelzi

**Tény:** a tanulmányok Intel HD 4600-at (2013) javasolnak minimumnak. A fejlesztői gép i7-6700K + RX 550.
**Kérdés:** van hozzáférésed régi, integrált videókártyás géphez? Ha nincs, a minimumot nem tudjuk igazolni, csak becsülni.

### D15 – Rangsorolt verseny (MMR, ranglista) · ELDÖNTVE (2026-11-11): a rangsorolt verseny cél; az internetes mód tervezésében számolunk vele

**Javaslat:** most nem cél. Egy későbbi listára kerül.

### D16 – Szabálybefagyasztás a port idejére · ELDÖNTVE (2026-11-12): igen

**Tény:** a C#-port bitre azonos paritásra épül ([PORT_TERV.md](PORT_TERV.md)); minden szabálykód-változást mindkét oldalon el kellene végezni.
**Döntés:** a port végéig (P9) a Python-játékban csak hibajavítás (mindkét oldalon, naplózva) és adatban felvehető tartalom.
Új egység, képesség vagy mechanika – köztük a D13 patrol és stance – a port után, közvetlenül a C#-magba kerül.
Kivétel: a P0 determinisztikus matematikai cseréi (`dmath`), amelyek a portot szolgálják.

### D17 – Minőségi cél és minőségi szakasz · ELDÖNTVE (2026-11-12)

**Cél (tulajdonos):** a játék minőségben vetekedjen a 0 A.D.-vel, az Age of Empires-szel, a Warcraft III-mal és a StarCraft-tal.
**Elemzés:** [MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md) – a C#-alapok megfelelnek; a port terve a mai minőségi hiányokat (interpoláció, csoportmozgás, nagy léptékű útkeresés, AI, szerkesztő) átvinné.
**Döntés:** a port bitre azonos marad (nem nulláról írjuk újra), de a kódszerkezet új; a P8-ban `RenderSnapshot` + interpoláció; a P6 után Q1–Q6 minőségi szakasz mérhető kapukkal ([PORT_TERV.md](PORT_TERV.md) 5/A). A D4-et (szerverautoritatív hálózat) az F5 előtt mért sávszélesség alapján újranézzük – addig érvényes.

### D18–D20 – Új motor nulláról · ELDÖNTVE (2026-11-12)

**Kérés:** a 0 A.D. és az OpenRA szintjén modolható motor, WC3-szintű küldetések, kétféle Conquest-mód, minden
rendszer, amin a 0 A.D. fut, a mostani 7 + 13 faj teljes tartalmával és a mostani grafikával (árnyék, shader, effekt).
**Tény:** a 0 A.D. Windows, Linux, macOS (Intel, Apple Silicon), FreeBSD és OpenBSD alatt fut; a .NET OpenBSD-n nem,
FreeBSD-n csak közösségi portként.
**Döntés:** C++20 motor (D18), Lua 5.4 szkript (D19), győzelmi feltételek + hadjárattérkép (D20).
Terv: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md), [ADR-002](dontesek/ADR-002-uj-motor.md). A PORT_TERV és az ADR-001 felülírva.

### D21 – A specifikáció furcsaságai · ELDÖNTVE (2026-11-12)

1. **Harci kedv:** a sérülés egyedül továbbra sem töri meg az egységet; **a közelében (8 csempe) elesett hős azonnal
   megtöri** a morált (`morale.hero_death_breaks`).
2. **Fajnév helyett adatkulcs:** `pond_affinity` (a tó gyorsítja a népet), `spike_immune` (a tüske nem hat rá, és
   nem mérgezi meg), `summons_permanent` (az idézések nem járnak le, a hős idézései állandók). A mai fajoknál a
   viselkedés változatlan; egy mod új faja is megkaphatja. A specifikáció README-jében felsorolt többi név
   szerinti szabály az új motorban ugyanígy adatkulcs lesz.
3. **Mélységi rothadás:** a keret a saját főépülethez legközelebbi fáknál fogy el először.
4. **Halott adat:** a `detect_units` felesleges volt (a Szemhordozó „A Kórus szeme” képességgel már felfed,
   9 csempén), a `balance_v6` régi jelölő – mindkettő törölve.

### D22 – Képességmodell · ELDÖNTVE (2026-11-12)

Alap: [spec/KEPESSEGEK.md](spec/KEPESSEGEK.md) B–D. Az új motorban (M7) valósul meg; a Python-referencia a D16
miatt nem kap új mechanikát.

1. **Varázserő-identitás:** a nem hős, nem `caster` címkés egység képessége a faj forrásából fizet (Ursanok és
   Wullof: csak újratöltés – a Wullof énekei ingyenesek; Croftianok: életerő; Micéliák: spóra); a hősök és a
   varázslók energiát használnak. A mai adat ennek már megfelel (a 35 energiás képesség mind hősé vagy varázslóé);
   a szabályt a `tools/tartalom_leltar.py` 17. pontja ellenőrzi, az új motor sémaellenőrzése tiltja.
2. **Egységes alapértékek:** közös alap-újratöltés, -hatótáv, -sugár, -időtartam minden képességre; a mai
   képességek a konvertáláskor a mai tényleges értéküket kapják meg kifejezetten (a játékmenet nem változik).
   A varázssebzés mindenhol a varázserő-szorzóval nő (a területi épületsebzés és a landolási sebzés is).
3. **Új elemek a játékban:** ellenséges aura; eloszlatás és varázslatimmunitás; varázslási idő és csatornázás
   (megszakítható nagy varázslatok); automatikus használat kapcsolója adatban leírt feltétellel (a gépi
   ellenfél is ezt használja). A konkrét képességek és számok balanszdöntések – képességenként, később.
4. **Gyógyítás:** minden gyógyítás csak élőlényre hat; a gépeket a javítás (munkás, Fabrikátor) és a kifejezett
   gépjavító képességek gyógyítják. Érintett mai képességek (javaslat a KEPESSEGEK.md E pontjában).

**D22 kiegészítés (2026-11-12, elfogadva):** a „Második lélegzet” (`h_livia_lelegzet`) gyógyít **és** javít; a
„Foltozás” (`mho_javitas`) gépjavító képesség.

### D23 – mod.io-támogatás · ELDÖNTVE (2026-11-12)

A modolhatósághoz (K1, D9) a [mod.io](https://mod.io/) online modtár is hozzátartozik: játékon belüli
modböngésző, letöltés, frissítés, feltöltés. Kiegészítő szolgáltatás – a helyi modok és a LAN mod.io nélkül is
működnek. Megvalósítás és feltételek: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) 3.2/6. pont; mérföldkő: M13.

### D24 – Kutatások és szabálymódosítók · ELDÖNTVE (2026-11-12)

Alap: [spec/KUTATAS_ES_SZABALYOK.md](spec/KUTATAS_ES_SZABALYOK.md) E–F. Az új motorban (M2 sablonformátum, M7):
1. **Egységes módosítórendszer:** minden kutatás, ág, aura, buff és faji szabály `{útvonal, művelet: add | mul |
   replace, érték, szűrő}` módosítót ad; a 25 paraméterhangoló szabálykulcs és a 10 statnév ezzé konvertálódik;
   halmozás: összeadás, szorzás, felülírás rögzített sorrendben, azonos forrás egyszer. A játékmenet nem változik
   (a mai felülírások `replace` műveletet kapnak).
2. **Többszintű kutatások:** az összetartozó I./II./III. kutatások egy elem szintjei; a számok változatlanok.
3. **Viselkedés-szabályok passzív képességként:** a 14 viselkedés-kulcs a D22 képességmodell eseményalapú passzív
   képessége lesz, amelyet a kutatás vagy az ág a szűrt egységeknek ad; a panelen látható.

### D25 – Hősök, tárgyak, táborok, elemek · ELDÖNTVE (2026-11-12)

Alap: [spec/HOSOK_TARGYAK_TABOROK.md](spec/HOSOK_TARGYAK_TABOROK.md) E–F. Az új motorban:
1. **Állandók adatba, elemek állapotként:** `game.heroes` és `game.camps` adatszakasz (szinthatárok, tapasztalat,
   leltár, hőskorlát, újraélesztés, tábor-körök, zsákmány); az elem a D22 állapotmodell része, a kölcsönhatások
   (`removes`, `amplifies`, `immune_tags`) adatban. A játékmenet nem változik.
2. **Tárgyak képességhordozók** (WC3): aktív képesség töltetekkel, passzív, aura, találatkori hatás, egyedi tárgy;
   a mai 15 tárgy változatlan, az újak balanszmunka.
3. **Bolt beállítás szerint:** a motor a semleges kereskedőt (a pályán) és a faji tárgyboltot is tudja; a pálya és a
   lobbi beállítása dönti el, melyik legyen (mindkettő, egyik, vagy egyik sem – ez utóbbi a mai állapot); a
   hadjáratban küldetésenként.
4. **Táborok újraéledése pályabeállítás:** alapból nem élednek újra; a pálya vagy a lobbi bekapcsolhatja,
   újraéledési idővel.

### D26 – Szövetséges kisnépek · ELDÖNTVE (2026-11-12)

Alap: [spec/KISFAJOK.md](spec/KISFAJOK.md) E–F.
1. **Szövetségi bónusz:** minden kisnép pontosan egy kimondott bónuszt ad a gazda seregére; a többi kutatása csak a
   saját (`ally_<nép>`) egységeire hat. A konkrét bónuszok a mai széles hatókörű kutatásokból választhatók – balanszmunka.
2. **A tó a gazda népét nézi:** a Huk’ucha szövetséges egységei is gyorsulnak a saját tavon. Hibajavításként a
   Python-referenciában is (`pond_speed`, D16 szerint megengedett); a golden felvételek változatlanok.
3. **Zsoldosbérlés** a pályán álló kisnép-táborokból (WC3, 0 A.D.), szövetség nélkül is, korlátozott számban; a
   pálya vagy a lobbi kapcsolja be. Új motor (M7/M12).

### D27 – Műszaki ADR-ek · ELDÖNTVE (2026-11-12)

[ADR-003](dontesek/ADR-003-megjelenito.md) megjelenítő (saját RHI: OpenGL 3.3 + SDL3 GPU),
[ADR-004](dontesek/ADR-004-szamabrazolas.md) számábrázolás (fixpontos Q16.16, mérve),
[ADR-005](dontesek/ADR-005-adatformatum.md) adat- és sablonformátum – elfogadva. Ezzel az M0 lezárult; következik az M2.

### D28 – Micéliák és küldetések · ELDÖNTVE (2026-11-12)

1. **A Micéliák csak szövetséges kisnép és mod-faj.** Az alaptartalomban a kisnép-változat (`faction_micelia`) van; a
   játszható főnép a `micelia` modban (`engine/content/mods/micelia/`, függősége az alap). A D12 felülírva.
2. **A Micélia-küldetések és a novella-küldetések kikerülnek** (a novellák maradnak).
3. **A küldetések az új motorra nulláról készülnek** (M11, WC3-szintű küldetésrendszer és szerkesztő); a régi
   küldetések helyben referenciaként megmaradnak, a GitHubon nincsenek fent.

### D30 – Épületelfoglalás, megszállás, előléptetés · ELDÖNTVE (2026-11-12)

1. **Elfoglalás** (képesség): az épület a használóé lesz; az épület saját listáját képezheti és kutathatja (a másik
   nép elemeit is), az épület képességeit nem.
2. **Megszállás** (képesség): a tulajdonos nem változik; ellenfél tulajdonosnál amíg bent van megszálló, a
   tulajdonos ott nem képezhet és nem kutathat (a kikutatott fejlesztései megmaradnak); a távolharcos megszálló
   kilő. Férőhely az épület méretétől (2 / 4 / 6); kiűzés 50% alatti életerővel vagy visszafoglalással.
3. **Kiosztás:** főnépenként 3 egység, változó kombinációban (pl. 2 elfoglaló + 1 megszálló, 1 + 2, vagy
   egység, amely mindkettőre képes) – a tulajdonos pontosítása szerint, a kiosztást rám bízta; két kisnépnél
   (Ghoblin, Daresh) minden egység.
4. **Előléptetés:** a nem hős harci egységek két fokozatot léphetnek (Veterán, Elit).
A számok balanszmunka (M7). Részletek: [spec/ELFOGLALAS_ES_ELOLEPTETES.md](spec/ELFOGLALAS_ES_ELOLEPTETES.md).

### D32 – Terep és csoportmozgás · ELDÖNTVE (2026-11-12)

1. **Terep:** sík pálya, **sokféle**, adatból bővíthető terepfajtával; StarCraft 1-szerű „kamu” magaslatokkal
   (szint csempetulajdonságként, szirt járhatatlan, rámpa átjáró; a megjelenítő festett peremmel mutatja).
2. **Csoportmozgás:** alapból WC3-szerű (a csoport együtt, a leglassabb tag sebességével, közelharcosok elöl,
   kikapcsolható), emellett választható AoE-alakzatok (sor, doboz, szétszórt, szárnyas).
Megvalósítás: [ADR-006](dontesek/ADR-006-terkep-es-mozgas.md), M4.

### D33 – Magaslat és mélység szabályai; ADR-006 elfogadva · ELDÖNTVE (2026-11-12)

1. A **pályaszerkesztőben** a terep magaslattá (1–2. szint) vagy mélységgé (−1. szint) nyilvánítható; a szirtperem
   automatikus, a rámpa külön ecset.
2. A szintekre **extra szabályok** érvényesek (a tulajdonos kérése, az én javaslatom szerint): magasabbról
   messzebb látni és messzebbre lőni, felülről nagyobb, alulról kisebb a sebzés, felfelé lőve hibalehetőség,
   alulról a magaslat ködben marad, a mélység fedezéket ad, a rámpa lassít felfelé. A számok adatban
   (`elevation_rules`), a balansz az M6–M7-ben véglegesíti. Részletek: ADR-006, 3. pont.
3. Az **ADR-006 elfogadva**; indul az M4 1. lépése.

### D34 – Pályaméretek · ELDÖNTVE (2026-11-12)

Többféle pályaméret (a tulajdonos kérése): **kicsi** 96 × 96 csempe (ajánlott 1 v 1), **közepes** 160 × 160 (2–4
játékos), **nagy** 256 × 256 (4–8 játékos); a szerkesztőben egyedi méret legfeljebb 512 × 512. A méretek és az
ajánlott játékosszámok adatban (`game.json` → `map_sizes`), a modok bővíthetik. A régi játék 88 × 88-as pályája a
kicsinél is kisebb volt.

### D35 – Napszakok és időjárás · ELDÖNTVE (2026-11-12)

A tulajdonos kérése: legyenek **napszakok** (mozgó árnyékok, esteledés, virradat) és **időjárás** a motorban,
egyelőre főleg díszletként, de WC3-szerűen játékhatással is (faji és általános szabályok), az időjárás pedig a pályát
is változtathatja (eső: kiárad a folyó, pocsolyák, visszanő 1–2 fa; hó: befagynak a tavak stb.). Mindkettő a meccs
indításakor kapcsolható (napszak: ki / hangulat / hatással; időjárás: ki / hangulat / hatással / hatással +
pályaváltozás). **Minden népre hat a napszak és az időjárás is** (a tulajdonos kiegészítése) – mind a 21 népnek
(főnépek, kisnépek, Vadon) van napszakos és időjárásos hatása. A javasolt működés, a faji és az általános hatások, a pályaváltozások és az ütemezés:
[spec/NAPSZAK_ES_IDOJARAS.md](spec/NAPSZAK_ES_IDOJARAS.md); a megvalósítás M5–M8, M11. **Döntések:** alapbeállítás
napszak *hatással*, időjárás *hangulat*; éjszaka a Wullof vadászai és felderítői láthatatlanok; a számok az M7-ben.

### D36 – Többféle gépi ellenfél, modolható AI · ELDÖNTVE (2026-11-12)

A tulajdonos kérése: ne csak nehézségi szintek legyenek, hanem többféle **személyiség** is (védekező, támadó,
kiegyenlített és más logikus stílusok) – ez a küldetésekhez is jól illik –, és az AI **modként** is hozzáadható
legyen. Javaslat: két független tengely – nehézség (6 szint, a legfelső nyíltan „csaló”) és személyiség (11 stílus:
védekező, támadó, kiegyenlített, gyors roham, terjeszkedő, technológus, portyázó, ostromló, csapatjátékos,
alkalmazkodó, népe szerinti) –, a személyiség adat; Lua-AI keretrendszer menedzserekkel, csak a látott információval;
küldetés-AI (építési lista, hullámok); AI-modok három szinten. Részletek: [spec/GEPI_ELLENFEL.md](spec/GEPI_ELLENFEL.md);
megvalósítás M9, M11, M13. **Döntések:** 6 nehézségi szint (csalás csak a jelölt „Őrült” szinten); a hadjáratban
népe szerinti személyiség; **tanuló AI** is legyen – a meccsen belül alkalmazkodik, a végén felejt (determinizmus).
### D37 – Az M6 kerete · ELDÖNTVE (2026-11-13)

Az M6 (játékmag, függőleges szelet) négy kérdésére a tulajdonos válasza: (1) **ideiglenes HUD** az M6-ban, a végleges
RmlUi-felület az M8-ban; (2) a szabályok **a régi játék képletei és számai** szerint, próbákkal rögzítve – eltérés csak
döntéssel; (3) a három ursan hős az M6-ban **egységként** (képességekkel, szint és tárgy nélkül), a hősrendszer az M7-ben;
(4) a kapumeccs **ursan tükörmeccs** szkriptelt (Lua) ellenféllel. A terv: [ADR-007](dontesek/ADR-007-jatekmag.md).
