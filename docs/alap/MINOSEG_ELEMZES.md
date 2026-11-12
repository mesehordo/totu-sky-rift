# A C#-motor és a minőségi cél – elemzés

Kelt: 2026-11-12 · alap: a 67 tanulmány (`TANULMANY_LELTAR.md`), [TOTU_ALAPDOKUMENTUM.md](TOTU_ALAPDOKUMENTUM.md),
[PORT_TERV.md](PORT_TERV.md), a mérések (`docs/fejlesztes/meresek/`).
**Cél (tulajdonos, 2026-11-12):** a játék minőségben vetekedjen a 0 A.D.-vel, az Age of Empires-szel, a Warcraft III-mal
és a StarCraft-tal.

## 1. Röviden

A C#-motor **alapjai megfelelnek a tanulmányoknak és a célnak**: nyelv, teljesítmény, determinizmus, mentés,
replay, licenc és megjelenítő – mindegyik mért adatra épül. A **port terve azonban ma csak a jelenlegi játékot
másolja le**, és ezzel a mai minőségi hiányait is átviszi. Ezek a hiányok nem a nyelvből, hanem a mozgásból, a
megjelenítésből, a gépi ellenfélből és az eszközökből adódnak. A tanulmányok ezeket kifejezetten előírják, a
port terve viszont nem tartalmazza őket. A javaslat ezért **nem a motor nulláról újraírása**, hanem a port
kiegészítése egy mérhető kapukkal ellátott **minőségi szakasszal** (Q1–Q6, lásd a 4. pontot).

## 2. Mi felel meg már most

| Terület | Tanulmány / cél | A C#-irány | Bizonyíték |
|---|---|---|---|
| Szimulációs teljesítmény | 8 × 200 pop, p99 ≤ 40 ms | 2400 egység p99 < 9 ms (útkeresés), < 3 ms (harc) | `AB_EREDMENY.md` |
| Megjelenítés | 30 FPS minimum, 60 FPS cél | 1600 látható egység p99 10,5 ms (≈ 95 FPS) egy RX 550-en | `RENDER_PROBA.md` |
| Determinizmus | azonos seed + parancs → azonos állapot | bitre azonos 3 nyelven; a P0 platformfüggetlen matematikát vezetett be | `tools/ab/`, `rts/dmath.py` |
| Mentés, replay | verziózott, biztonságos | JSON-snapshot, `.trep`, golden készlet | `SZERZODES.md` |
| Grafika és szimuláció szétválasztása | a szimuláció nem ismeri a grafikát | `Totu.Core` nem hivatkozhat a raylib-re | `PORT_TERV.md` 3. pont |
| Modolás | 0 A.D.-szint (D9) | C#-ban sandboxolt szkript (Lua) megoldható; Pythonban nem volt | ADR-001 |
| Licenc | GPL, grafika és történet védett | GPL-3.0-or-later + fenntartott grafika (mint a 0 A.D. és az OpenRA esetében) | D8 |

## 3. Ahol a terv elmarad a tanulmányoktól és a céltól

| # | Hiány | Mit írnak a tanulmányok | A referenciajátékok | Ma a TOTU-ban |
|---|---|---|---|---|
| 1 | **Interpolált megjelenítés** | „30 FPS minimum / 60 FPS cél; interpolált animáció”; a kliens két hiteles állapot között interpolál | mind a négy: sima mozgás a szimulációs ütemtől függetlenül | a kliens 30 Hz-es lépésekben rajzolja a pozíciót, interpoláció nincs (`main.py` fő ciklusa); 60+ Hz-es kijelzőn szaggat |
| 2 | **Csoportmozgás és formáció** | a formáció a tanulmányok egyik leggyakoribb témája (~90 említés); közös célmező azonos célú csoportoknak; híd/kapu torlódás | WC3, AoE: formációban vonuló, egymást kikerülő csapatok | `_settle_formation` csak a célnál rendez; a minőségét nem mértük; NAV-BRIDGE/CHOKE teszt nincs |
| 3 | **Nagy léptékű útkeresés** | HPA* nagy távolságra, flow field csoportoknak – mért szükség esetén, ADR-rel | AoE2, 0 A.D.: hierarchikus útkeresés | sima A* 9000 csomópontos határral; elérhetetlen célnál a határig keres (P-002) |
| 4 | **Gépi ellenfél** | stratégiai célválasztás, gazdaság, felderítés; csak felderített információ (AI-FOG) | AoE2/0 A.D.: több nehézségi szint, személyiségek, csalásmentes mód | `ai.py` 887 sor; az Ooni megrekedhet (H-003); az AI-FOG nincs vizsgálva |
| 5 | **Pálya- és küldetésszerkesztő** | Tiled-terep + külön TOTU küldetésgráf-szerkesztő (trigger, feltétel, akció, párbeszéd, cél) | mindegyikhez tartozik szerkesztő; a WC3- és SC-szerkesztő a közösség motorja volt | nincs; a küldetések kézzel írt JSON-ok |
| 6 | **Hálózati modell** | D4: szerverautoritatív, ködszűrt nézet, 5–10 Hz-es snapshot + interpoláció | mind a négy **determinisztikus lockstep** (AoE: „1500 archers on a 28.8 modem”); a modern kiadások ezt relészerverrel egészítik ki | Python lockstep; a C#-ban lesz eldöntve |
| 7 | **Hang és visszajelzés** | parancsnyugtázás, egységhangok, harci zajok | a minőségérzet jelentős része | a Python-kliensben van alap hangkezelés; a port terve csak említi |

**A 6. pontról:** a D4 (szerverautoritatív modell) indoka a térképcsalás kizárása, ami valós előny. Ugyanakkor
mind a négy referenciajáték lockstepet használ, mert 1600 egység állapotát tick-enként elküldeni sokkal
nagyobb sávszélesség, mint a parancsokat. Becslés: egységenként 12 bájt, 10 Hz, tömörítés nélkül kb.
190 KB/s kliensenként; ezt mérni kell. Most, hogy a P0 után a mag platformfüggetlenül determinisztikus lesz, a
lockstep vegyes operációs rendszerek között is működne, ami a D4 idején még nem volt igaz. **Ez nem
változtatja meg a D4-et,** de érdemes újra megnézni, mielőtt az F5 elindul.

## 4. Javaslat: port + minőségi szakasz (nem újraírás nulláról)

**Miért nem nulláról?** A 7 faj, a 46 küldetés és a 13 kisfaj szabályai ma csak a Python-kódban léteznek pontosan.
A bitre azonos port az egyetlen mód annak igazolására, hogy semmi nem veszett el. Egy nulláról írt motor hónapokig
nem tudná megmutatni, hogy a kampány ugyanúgy működik. A **kódszerkezetet** viszont nem kell másolni: a paritás
viselkedésre vonatkozik, nem a fájlokra. A 3700 soros `game.py`-ból a C#-ban külön rendszerek lesznek
(mozgás, harc, gazdaság, látótér stb.).

**A port tervének kiegészítései:**
- **P3–P8:** a C#-kliens a magtól csak egy olvasható `RenderSnapshot`-ot kap (a tanulmány szerint), és a két
  utolsó tick között **interpolál**. Ez a szimulációt nem érinti, ezért a port alatt is megépíthető.
- **P6 után (a mag kész) – minőségi szakasz, a szabálybefagyasztás feloldásával, mindegyik saját ADR-rel:**

| Szakasz | Tartalom | Mérhető kapu |
|---|---|---|
| Q1 Mozgás | csoportcél, formációban vonulás, híd/kapu torlódás, elérhetetlen cél gyors felismerése (P-002) | NAV-DIAG/BRIDGE/MAMMOTH/CHOKE tesztpályák; 50 egység hídon átkel X mp alatt, beragadás nélkül |
| Q2 Nagy léptékű útkeresés | HPA* és/vagy flow field, ha a Q1 mérése igazolja | 8 × 200 pop, p99 ≤ 40 ms a teljes TOTU-szabályokkal |
| Q3 Megjelenés | 60 FPS interpolációval, animációs átmenetek, effektek, csapatszín, kijelölési visszajelzés | 1600 egység 60 FPS-en a fejlesztői gépen; képkockaidő p99 ≤ 16,7 ms |
| Q4 Gépi ellenfél | nehézségi szintek, személyiségek, csalásmentes mód (AI-FOG), H-003 | AI-FOG = 0 szivárgás; győzelmi arány szintenként; 100 gép–gép meccs elakadás nélkül |
| Q5 Szerkesztő | pályaszerkesztő + küldetésgráf-szerkesztő a meglévő JSON-formátumra | a 46 küldetés megnyitható és változatlanul menthető |
| Q6 Hálózat | F5/F6 a D4 újranézése után | LAN-2…8, NET-LOSS, NET-REJOIN, FOW-LEAK = 0 |

## 5. Amit őszintén nem lehet garantálni

A referenciajátékok minőségének nagy része nem a motoron múlik: grafikán, animáción, hangon, pályatervezésen,
balanszon és rengeteg játékteszten. A motor ezeket lehetővé teszi, de nem pótolja. A fenti kapuk azt mérik,
ami mérhető (folyamatosság, válaszidő, mozgás, nagy csaták, AI-hibák). A „vetekszik velük” végső ítélete a
játékosoké lesz. Ehhez a Q-szakaszok után nyilvános tesztre van szükség.
