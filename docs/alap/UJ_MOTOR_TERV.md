# A TOTU új motorja – terv nulláról

Kelt: 2026-11-12 · státusz: **irányadó terv** (D18–D20). A korábbi C#-port terve ([PORT_TERV.md](PORT_TERV.md),
[ADR-001](dontesek/ADR-001-motor.md)) ezzel felülírva. Alapanyag: [TARTALOM_LELTAR.md](TARTALOM_LELTAR.md) (generált),
[MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md), a tanulmányok, valamint a 0 A.D. és az OpenRA felépítése.
Az időigény nem szempont; a sorrendet a függőségek és a kockázatok adják.

## 1. Követelmények (tulajdonos, 2026-11-12)

| # | Követelmény | Mérce |
|---|---|---|
| K1 | Modolás a 0 A.D. **és** az OpenRA szintjén | adat-, grafika-, szkript- és játékmód-szintű mod; modkezelő; függőségek; többjátékos modegyeztetés |
| K2 | Warcraft III-szintű küldetések | világszerkesztő, trigger-rendszer, régiók, kamerák, átvezetők, párbeszéd, küldetésnapló, pályánkénti egyedi egység- és képességadatok, hadjáratlánc átvitt hősökkel |
| K3 | Két „Conquest” jellegű mód (a játékbeli nevük később) | (a) **győzelmi feltételek** választható játékmódként (pl. teljes megsemmisítés, csoda, királygyilkosság, ereklye); (b) **hadjárattérkép**: területfoglaló stratégiai térkép körökkel, a csaták RTS-ben |
| K4 | Minden operációs rendszer, amin a 0 A.D. fut | Windows, Linux, macOS (Intel és Apple Silicon), FreeBSD, OpenBSD |
| K5 | Mind a 7 főfaj, a rejtett Micélia-főfaj, a 13 szövetséges kisfaj és a semleges Vadon működik | a [leltár](TARTALOM_LELTAR.md) minden sora (220 egység, 152 épület, 168 kutatás, 269 képesség / 23 típus, 15 tárgy, 6 fajmechanika, a teljes szabályszókincs) |
| K6 | A mostani sprite-ok és kinézet, árnyékokkal, shaderekkel, effektekkel | a meglévő HD sprite-lapok és csapatszín-maszkok; árnyék, köd, víz, fény, részecske, utófeldolgozás |
| K7 | (korábbról érvényes) minőség a 0 A.D. / AoE / WC3 / SC1 szintjén, 8 × 200 pop, 60 FPS | [MINOSEG_ELEMZES.md](MINOSEG_ELEMZES.md) kapui |

## 2. Mit veszünk át a két mintából

| Terület | 0 A.D. (Pyrogenesis) | OpenRA | **TOTU-javaslat** |
|---|---|---|---|
| Motornyelv | C++ | C# (.NET) | **C++20** (D18) – minden K4-rendszeren fut, mint a 0 A.D. |
| Játéklogika | JavaScript-komponensek (SpiderMonkey) + C++-komponensek | C# „trait”-ek; modok saját C#-szerelvényt hozhatnak | **C++-rendszerek a forró részekre, Lua-komponensek a játékszabályokra** (D19); natív kódot a modok nem hoznak (biztonság, hordozhatóság, többjátékos egyezés) |
| Adat | XML-sablonok öröklődéssel (`parent=`) | MiniYaml: öröklés + trait-összerakás | **sablonok öröklődéssel és komponens-összerakással**; a modok kulcsonként felülírhatnak vagy törölhetnek |
| Küldetésszkript | JS-triggerek | Lua-API | **Lua** + grafikus trigger-szerkesztő, amely Luát generál (mint a WC3 GUI → szkript) |
| Fájlrendszer | VFS: a modok rétegesen takarják egymást | modmappák, `mod.yaml` | **VFS rétegezéssel**, `mod.json` (azonosító, verzió, függőségek, ütközések) |
| Szimuláció | fixpontos számok, lockstep | egész koordináták (1024 / cella), lockstep | **fixpontos / egész szimuláció**, lockstep-képes determinizmus |
| Hálózat | lockstep, P2P / lobbi | lockstep, szerveren át | **lockstep relészerverrel** (javaslat; a D4 újranyitva – 6. pont) |
| Szerkesztő | Atlas | külön térképszerkesztő | **játékba épített világszerkesztő** (Dear ImGui) |
| Felület | XML + JS GUI, modolható | YAML-chrome | **RmlUi** (HTML/CSS-szerű leírás + Lua), teljesen modolható |
| Mesterséges intelligencia | JS (Petra) | C#-bot modulok | **Lua-AI**, modolható, csak felderített információval |

**A 0 A.D. tanulsága:** a túl sok szkriptből futó, egységenkénti tick-logika lassú. Ezért a TOTU-ban az egységenként
minden tickben futó munka (mozgás, útkeresés, ütközés, látótér, lövedék, sebzésszámítás) C++-ban van. A Lua
eseményekre fut: képesség használata, halál, termelés vége, trigger, fajmechanika ütemezett lépése.

## 3. Architektúra

```
┌──────────────── Eszközök: világszerkesztő (ImGui), modkezelő, fejléc nélküli futtató, tesztek ─────────────────┐
│ Kliens: RmlUi-felület (+Lua) │ Megjelenítő (RHI: OpenGL 3.3 → Vulkan) │ Hang (miniaudio) │ Bevitel     │
├───────────────────────────────── RenderSnapshot + interpoláció (csak olvasás) ─────────────────────────────────┤
│ Szimuláció (determinisztikus, fixpontos, 30 Hz)                                                              │
│   C++-rendszerek: térkép, járhatóság, útkeresés, mozgás/formáció, ütközés, látótér, harc, lövedék, gazdaság  │
│   Lua-réteg: képességek, fajmechanikák, triggerek, játékmódok, AI, küldetések (sandbox, utasításkeret)        │
│   Entitás–komponens tár, sablonok öröklődéssel, parancssor, állapothash, mentés/replay                       │
├──────────────────────── Tartalom: VFS + modrétegek, sablonbetöltő, sémaellenőrzés, honosítás ─────────────────┤
│ Platform: SDL3 (ablak, bevitel, szálak, fájl) – Windows, Linux, macOS, FreeBSD, OpenBSD                     │
└────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
Hálózat: lockstep-parancscsere relészerveren át (vagy a D4 szerinti modell) – a szimuláció felett, a kliensen kívül.
```

### 3.1 Megoldási javaslatok területenként

| Terület | Javaslat | Indok | Elvetett / tartalék |
|---|---|---|---|
| Fordítás | CMake, a külső könyvtárak a repóban vendorolva (mint a 0 A.D. `libraries/`), BSD-n rendszercsomagok | a vcpkg/Conan nem fut minden K4-rendszeren | vcpkg (OpenBSD-n nem) |
| Platform | SDL3 (zlib) | mind az 5 rendszeren támogatott; ablak, bevitel, hang, szálak | GLFW (kevesebb szolgáltatás) |
| Megjelenítés | saját vékony RHI; első háttér **OpenGL 3.3 core** (mindenhol van, OpenBSD-n is), második **Vulkan** (macOS-en MoltenVK-val) | a macOS OpenGL-je elavult, hosszú távon Vulkan/Metal kell; az OpenBSD-n a Vulkan gyenge | raylib (kevés kontroll a menetek felett), bgfx (BSD-támogatás bizonytalan) – az M1 ADR-je méréssel dönt |
| Számábrázolás | szimulációban 64 bites fixpontos (pl. 32.32) vagy egész világkoordináták | bitre azonos minden fordítón és processzoron, FMA- és könyvtárfüggetlen | lebegőpontos `dmath` (működik, de minden fordítóbeállítást őrizni kell) |
| Szkript | **Lua 5.4** (MIT): rögzített hash-mag, saját determinisztikus `math`, `io`/`os`/`debug` nélkül, tickenkénti utasításkeret; a szimulációs szkriptek egész / fixpontos típust kapnak; rendezett bejárás a `pairs` helyett | a WC3 Reforged és az OpenRA is Luát használ; kicsi, mindenhol fordul | Luau (jó sandbox, de kisebb közösség), JS (nehéz) |
| Adat | JSON a mostani formátum folytatásaként, `"inherits"` öröklődéssel és `"components"` összerakással; JSON-séma ellenőrzés; modfoltok (`"-kulcs"` törlés) | a mostani 22 fajfájl és a küldetések közvetlenül átvihetők; a modderek számára egyszerű | YAML (szebb, de két formátum lenne), XML |
| Mentés / replay | verziózott bináris snapshot + JSON-export; replay = kezdőállapot + parancsok + hash-ek | az F2 szerződés folytatása | – |
| Útkeresés | járhatósági osztályok (méret × mozgásmód) + hierarchikus keresés (HPA*) + csoportonként flow field + helyi elkerülés és formáció | a 2400 egység / 8 játékos csak így marad a keretben; a tanulmányok előírása | sima A* (a mostani; P-002) |
| Megjelenés | sprite-atlaszok fajonként; csapatszín a meglévő `_team` maszkokból shaderrel; vetett sziluett-árnyék; simított köd; víz-shader; részecskék; fény (napszak, fényforrások, emissive); utófeldolgozás (bloom, színkorrekció); **alakzatos tartalék**, ha egy egységnek nincs sprite-ja (ma 111/220) | a K6 a meglévő grafikára épít, normáltérkép nincs | 3D – kizárva |
| Felület | RmlUi (MIT): a HUD-elrendezés a `test_hud.py` szerinti; teljesen modolható | HTML/CSS-szerű, Lua-kötéssel; a modderek ismerik | saját GUI, ImGui (csak szerkesztőhöz) |
| Szöveg | FreeType + HarfBuzz, gettext PO-fájlok (hu, en, további nyelvek) | ékezetek, fordítói eszközök | – |
| Hang | miniaudio (MIT-0) | egyetlen fájl, minden rendszeren | OpenAL Soft (LGPL) |
| Szerkesztő | a motorba épített világszerkesztő Dear ImGui-val: terep, objektumok, régiók, kamerák, triggerek, objektumszerkesztő, hadjárat- és hadjárattérkép-szerkesztő | a WC3-szintű küldetésekhez elengedhetetlen | külső Tiled (csak import) |

### 3.2 Modolási szintek (K1)

1. **Adat:** sablonok öröklődéssel; új faj, egység, épület, kutatás, képesség és tárgy kód nélkül.
2. **Tartalom:** sprite, hang, zene, portré, felület, fordítás. A VFS-rétegek felülírják egymást.
3. **Lua:** új képességtípus, fajmechanika, AI, győzelmi feltétel, hadjárattérkép-szabály, pályagenerátor, küldetés.
4. **Natív kód: nincs** a modokban. A motor GPL, így aki ennél többet akar, forkolhatja.
5. **Infrastruktúra:** `mod.json` (verzió, függőségek, ütközések), modkezelő a menüben, modlock-hash a
   többjátékos lobbiban, mintamodok, SDK-dokumentáció (CC BY 4.0, D8). A grafika a modokban a D8 modkivétele
   szerint használható.

### 3.3 Küldetések (K2) és játékmódok (K3)

- **Világszerkesztő + trigger-rendszer:** esemény → feltétel → akció. Grafikusan szerkeszthető, és Luát generál.
  Közvetlen Lua is írható. Változók, időzítők, egységcsoportok, régiók, kamerák, átvezetők (letterbox,
  kamerapálya), párbeszéd portréval és hanggal, küldetésnapló, ranglisták.
- **Pályánkénti objektumadat:** egy küldetés saját egység-, képesség- és kutatásváltozatokat hozhat (WC3
  objektumszerkesztő), örökölve az alapsablonokból.
- **Hadjáratlánc:** elágazások, átvitt hősök és tárgyak, mentés küldetés közben.
- **Győzelmi feltételek:** mindegyik egy-egy Lua-játékmód. A lobbiban kombinálhatók, és modolhatók.
- **Hadjárattérkép:** tartományok gráfja, körökre osztott lépések, seregek (hős + egységcsomag), tartományi
  bevétel, gépi ellenfelek, csatánál RTS-pálya a tartomány sablonjából (a seregek a kezdő egységek),
  automatikus kiértékelés. Szabályai Lua és adat, így ez is modolható.

### 3.4 A fajok (K5) – hogyan kerülnek át

A leltár (`tools/tartalom_leltar.py`) a mostani játék teljes szabályszókincsét kigyűjti:
- 56 fajszintű, 62 egység- és 61 épületkulcs;
- 38 szabálymódosító;
- 23 képességtípus;
- 6 fajmechanika (`creep`, `pond`, `power`, `network`, `lantern`, `standard`) és a kapcsolódó rendszerek: morál, hit, hasadék, felemelkedés, fertőzés, pajzszóna, lélekhorgony, erdei lopakodás stb.;
- 12 helyen a kód név szerint ágazik el egy-egy tartalomra.

Mindegyikhez megvan a Python-kód helye, és ez a viselkedés referenciája. Az új motorban:
- az általános szabályok (harc, gazdaság, termelés, képességtípusok) C++-rendszerek vagy Lua-komponensek;
- a fajmechanikák Lua-modulok fajonként;
- a 12 név szerinti elágazásból adatvezérelt szabály vagy a tartalomhoz kötött szkript lesz;
- a paritást **viselkedési próbák** igazolják fajonként és mechanikánként (például: a nyálka X mp alatt Y
  csempét terjed; a lámpás hatósugara Z). Bitre azonos egyezés nem cél, mert a számábrázolás új.

A Python-játék a referencia: futtatható specifikáció, és a szabálybefagyasztás (D16) rá továbbra is érvényes.

## 4. Ütemterv – részcélok és kapuk

Minden mérföldkő végén működő, kipróbálható állapot van, és egyszerre egy részcél fut. A sorrend oka: a
determinizmust és a modolhatóságot utólag nem lehet beépíteni, ezért ezek jönnek először.

| # | Mérföldkő | Részcélok | Kapu |
|---|---|---|---|
| **M0** | Alapozás | ✔ tartalmi leltár; fajonkénti viselkedési specifikáció a Python-kódból (mechanikánként: szabály, számok, próba); ADR-ek (RHI, számábrázolás, adatformátum, hálózat); kódtárhely és CI a 5 rendszerhez | minden leltársorhoz tartozik specifikáció és tervezett próba |
| **M1** | Platform-váz | `engine/` CMake-projekt; SDL3-ablak, bevitel, OpenGL 3.3 környezet; naplózás, hibakezelés, összeomlási jelentés; egységtesztek | üres ablak és zöld tesztek **mind az 5 rendszeren** (Windows, Linux, macOS Intel/ARM, FreeBSD, OpenBSD) |
| **M2** | Determinisztikus mag | fixpontos matematika és trigonometria; determinisztikus RNG; entitás–komponens tár; VFS + modrétegek; sablonbetöltő öröklődéssel és sémával; parancssor; állapothash; snapshot; replay; fejléc nélküli futtató | ugyanaz a replay **minden rendszeren ugyanazt a hash-sorozatot** adja |
| **M3** | Lua-réteg | sandbox, determinisztikus `math`, rendezett bejárás, utasításkeret, API-kötés (entitás, komponens, esemény, időzítő), hibakezelés, újratöltés futás közben | a szkript nem fér a fájlrendszerhez és a hálózathoz; szkriptes replay minden rendszeren azonos |
| **M4** | Térkép és mozgás | terep, járhatósági osztályok, hierarchikus útkeresés, flow field, helyi elkerülés, formáció, híd/kapu torlódás, elérhetetlen cél gyors felismerése | NAV-DIAG/BRIDGE/MAMMOTH/CHOKE; 8 × 200 pop mozgás p99 ≤ 40 ms |
| **M5** | Megjelenítő | RHI (OpenGL 3.3), atlaszkészítés, Y-rendezés, csapatszín-shader, árnyék, köd, víz, fény, részecskék, utófeldolgozás, interpoláció, alakzatos tartalék, kamera | 1600 egység 60 FPS-en (p99 ≤ 16,7 ms) a fejlesztői gépen; minden rendszeren fut |
| **M6** | Játékmag – függőleges szelet | gazdaság, építés, termelés, harc, lövedék, látótér, lopakodás, kutatás; egy `standard` faj (Ursanok) teljesen | játszható csata az Ursanokkal szkriptelt ellenfél ellen |
| **M7** | Minden faj | a 23 képességtípus; a 6 mechanika és a fajrendszerek; ágak és felemelkedés; hősök, tárgyak, táborok, elemek; a 7 + 1 főfaj; a 13 kisfaj és a szövetségi rendszer; Vadon | a leltár minden sorának viselkedési próbája zöld |
| **M8** | Felület | RmlUi-HUD (a mostani elrendezés), menük, beállítások, honosítás (hu, en), billentyűkiosztás, akadálymentesség | a `test_hud.py` pontjai az új motorban zöldek |
| **M9** | Gépi ellenfél | Lua-AI keretrendszer, nehézségi szintek, személyiségek, csak felderített információ | 100 gép–gép meccs elakadás nélkül; AI-FOG = 0 |
| **M10** | Hálózat | lockstep relével (vagy a D4 döntése szerint), lobbi, modlock, szinkronhiba-észlelés, újracsatlakozás, megfigyelő | 8 kliens vegyes rendszereken szinkronhiba nélkül; NET-LOSS, NET-REJOIN |
| **M11** | Küldetésrendszer és szerkesztő | világszerkesztő, trigger-szerkesztő, objektumszerkesztő, átvezetők, párbeszéd, küldetésnapló, hadjáratlánc | egy mintaküldetés, amely minden WC3-szintű elemet használ; a szerkesztő változatlanul visszamenti |
| **M12** | Játékmódok | győzelmi feltételek; hadjárattérkép (tartományok, körök, seregek, csatagenerálás, automatikus kiértékelés) | egy teljes hadjárattérkép-játék végigjátszható |
| **M13** | Modrendszer kiadása | modkezelő, függőségek, SDK-dokumentáció, mintamodok (új kisfaj + képesség + küldetés + győzelmi feltétel) | a mintamod többjátékos módban, modlockkal működik |
| **M14** | Kiadás | csomagok (Windows-telepítő, AppImage/Flatpak, macOS dmg, FreeBSD/OpenBSD port), teljesítmény- és minőségellenőrzés, nyilvános teszt | tiszta gépen telepíthető mind az 5 rendszeren |

Párhuzamosítható: az M5 (megjelenítő) az M2 után az M3–M4 mellett haladhat; az M8 az M6 után.

## 5. Ami a mostani munkából megmarad

| Elem | Sorsa |
|---|---|
| Python-játék | futtatható referencia és specifikáció; játszható marad; D16 szabálybefagyasztás érvényes |
| Adatok (`data/`, `campaigns/`, `assets/`) | átvesszük; a fajadatok az új sablonformátumba konvertálódnak (eszközzel, nem kézzel) |
| F2 szerződés, snapshot, replay, golden készlet | a fogalmak átkerülnek; a golden felvételek a Python-referenciát védik |
| `dmath`, P0 | a Python-referencia determinizmusát javította; a C#-port kapujaként nem kell |
| A/B mérés, rajzolási próba | az eredmények érvényesek (C++ mért); a raylib-próba az M5 RHI-döntésének kiinduló adata |
| D16, D17 | érvényes; a Q1–Q6 kapui az M4/M5/M9/M10/M11 kapuiba olvadtak |

## 6. Döntések és nyitott kérdések

**Eldöntve (2026-11-12):** D18 C++ motor, nulláról; D19 Lua; D20 két Conquest-mód (győzelmi feltételek +
hadjárattérkép), saját játékbeli névvel; K4 = a 0 A.D. összes rendszere (a D3 bővül).

**Tulajdonosi döntés kell (a megadott mérföldkő előtt):**
1. ~~**Kódtárhely és CI (M0/M1):**~~ eldöntve 2026-11-12: egyetlen nyilvános GitHub-repó, CI mind az 5 rendszeren. Korábbi kérdés: az 5 rendszeren csak automatikus fordítással és teszteléssel lehet
   megbízhatóan dolgozni (macOS-futtató, FreeBSD/OpenBSD virtuális gép). Ehhez a repónak távoli tárhely kell
   (pl. GitHub, Codeberg vagy saját Gitea). Van-e Mac vagy BSD-gép a teszteléshez?
2. **Hálózati modell (M10 előtt, D4 újranyitva):** javaslat: lockstep relészerverrel, mint mind a négy
   referenciajátéknál. Hátránya: a térképcsalás elvben lehetséges. Ez ellen a köd szerveroldali ellenőrzése
   és replay-elemzés véd.
3. **A két játékmód neve (M12 előtt).**
4. **A korábbi D2 (raylib-cs):** a C++-irányban a megjelenítőt az M1 ADR-je dönti el, méréssel.

**Szakmai ADR-ek (én készítem, jóváhagyásra):** ADR-002 motornyelv és szkript (ez a terv); ADR-003 RHI;
ADR-004 számábrázolás; ADR-005 adat- és sablonformátum; ADR-006 hálózat.

## 7. Kockázatok

| Kockázat | Kezelés |
|---|---|
| A Lua-szkriptek elrontják a determinizmust (hash-sorrend, lebegőpont) | rögzített mag, saját `math`, rendezett bejárás, a szimulációs API-ban csak fixpontos típus; a replay-kapu minden rendszeren |
| A sok szkript lassú lesz (0 A.D.-tanulság) | az egységenként tickenként futó munka C++-ban; a Lua eseményre fut; profilozás az M3-tól |
| macOS / BSD hiba, ha nincs rá gép | CI mind az 5 rendszerre az M1-től (nyitott kérdés 1) |
| A megjelenítő a macOS-en (OpenGL elavult) | RHI-réteg; a Vulkan/MoltenVK-háttér az M5 után |
| A fajok viselkedése az átírásban eltér | mechanikánkénti viselkedési próbák a Python-referenciával összevetve |
| A grafika hiányos (111 egység sprite nélkül) | alakzatos tartalék a megjelenítőben; az új grafika a tulajdonos döntése |
