# Tulajdonosi döntések

Kelt: 2026-11-11. Ezek azok a kérdések, amelyekben a tanulmányok egymásnak vagy a játéknak ellentmondanak, és amelyeket a kódból nem lehet eldönteni.
Minden kérdésnél: tény → lehetőségek → **javaslat** → mit befolyásol. A többi kérdést szakmai alapon döntöm el, és az alapdokumentumban jelzem.

Állapot: `NYITOTT` / `ELDÖNTVE (dátum, választás)`.

## Összefoglaló (2026-11-11)

| # | Döntés |
|---|---|
| D1 | FELÜLÍRVA (2026-11-12) a D18-cal: a motor **C++20**, nulláról – [ADR-002](dontesek/ADR-002-uj-motor.md); a korábbi C#-döntés (ADR-001) az OpenBSD/FreeBSD-követelmény miatt elesett |
| D2 | ÚJRANYITVA (2026-11-12): a C++-irányban a megjelenítő-réteget az M1 ADR-je (ADR-003) méréssel dönti el; javaslat: saját RHI, OpenGL 3.3 alap + Vulkan. Korábban: **raylib-cs** (raylib, zlib), **OpenGL 3.3 alap**; 4.x csak opcionális magas grafikai profilban, játékszabályt nem érinthet. Rajzolási próba 2026-11-12: 1600 egység mind látható p99 10,5 ms, 2400 egység p99 15,8 ms (RX 550) – **megerősítve** |
| D3 | MÓDOSÍTVA (2026-11-12): minden rendszer, amin a 0 A.D. fut – Windows, Linux, macOS (Intel, Apple Silicon), FreeBSD, OpenBSD |
| D4 | ÚJRANYITVA (2026-11-12): az M10 előtt döntendő; javaslat: lockstep relészerverrel ([UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) 6. pont). Korábban: szerverautoritatív LAN |
| D5 | ELDÖNTVE (2026-11-11): az internetes mód közvetlenül a szerverautoritatív LAN után |
| D6 | ELDÖNTVE (2026-11-11): 30 Hz marad |
| D7 | ELDÖNTVE (2026-11-11): 8 × 200 pop, lépcsőkben; 2000–2400 csak terheléses próba |
| D8 | ELDÖNTVE (2026-11-12, módosítva): a kód **GPL-3.0-or-later** (mint az OpenRA); a világ, a történet és a grafika minden joga fenntartva (modkivétellel); szabályadatok és modding-doksi CC BY 4.0. Az eladást a GPL nem tiltja – ezt a tulajdonos tudomásul vette |
| D9 | ELDÖNTVE (2026-11-11): teljes 0 A.D.-szintű modolás (sandboxolt szkript, modkezelő, függőségek, modlock) |
| D10 | ELDÖNTVE (2026-11-11): átállás verziózott JSON-mentésre, a régi mentések egyszeri átalakításával |
| D11 | ELDÖNTVE (2026-11-11): 1 szövetséges nép, annak 3 épületével |
| D12 | ELDÖNTVE (2026-11-11): a Micéliák rejtett alapfaj (csak a Gombanép-hadjáratban játszható); a `micelia` mod kikapcsolt példa-mod |
| D13 | ELDÖNTVE (2026-11-11): patrol és stance is bekerül |
| D14 | ELDÖNTVE (2026-11-11): nincs régi tesztgép – a minimum becslés marad, a dokumentáció ezt jelzi |
| D15 | ELDÖNTVE (2026-11-11): a rangsorolt verseny cél; az internetes mód tervezésében számolunk vele |
| D18 | ELDÖNTVE (2026-11-12): **új motor nulláról, C++20** – minden 0 A.D.-rendszeren; 0 A.D. + OpenRA szintű modolás, WC3-szintű küldetések; terv: [UJ_MOTOR_TERV.md](UJ_MOTOR_TERV.md) |
| D19 | ELDÖNTVE (2026-11-12): a játéklogika, a modok, az AI és a küldetések szkriptnyelve **Lua** (5.4, sandbox, determinisztikus) |
| D20 | ELDÖNTVE (2026-11-12): **két Conquest-jellegű mód** – győzelmi feltételek és hadjárattérkép; a játékbeli nevüket a tulajdonos adja meg |
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
