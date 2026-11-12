# Gépi ellenfél (AI)

> Állapot: **ELFOGADVA** (2026-11-12, D36). Megvalósítás: M9 (keretrendszer, személyiségek,
> nehézségek), M11 (küldetés-AI), M13 (AI-modok). Alap: ADR-004 (determinizmus), M3 (Lua-réteg, homokozó),
> [KULDETESEK_TERV](../KULDETESEK_TERV.md), [FORRASOK_ES_TANULSAGOK](../FORRASOK_ES_TANULSAGOK.md) (0 A.D. Petra, WC3
> AI-szkriptek, microRTS).

## 1. Két tengely: nehézség és személyiség

A gépi ellenfelet két, egymástól független beállítás írja le. A lobbiban helyenként (slotonként) választható.

**Nehézség** – mennyire ügyes:

| Szint | Mit jelent |
|---|---|
| **Kezdő** | lassan reagál, kevés egyidejű feladat, nem használ képességet, nem portyázik |
| **Könnyű** | egyszerű építési rend, gyenge mikro (nem húzza vissza a sebesült egységet) |
| **Normál** | teljes gazdaság, képességek, felderítés, ellencsapat-választás |
| **Nehéz** | gyors reakció, jó mikro (fókuszlövés, visszavonulás, varázslat-időzítés), több front |
| **Mester** | a legjobb, amit a szabályok szerint tud – csalás nélkül |
| **Őrült** (választható kapcsoló) | a Mesteren felül +50% gyűjtés (nyíltan jelölve „csaló”); a többi szint sosem csal |

A nehézség **nem** ad információt: a gépi ellenfél csak azt látja, amit a saját egységei felderítettek (AI-FOG = 0,
az M9 kapuja).

**Személyiség** – hogyan játszik:

| Személyiség | Játékstílus | Küldetésben például |
|---|---|---|
| **Védekező** | falak, tornyok, kapuk; erős helyőrség; csak túlerőben támad, ellentámad | erődítmény, amelyet ostromolni kell |
| **Támadó** | korai és folyamatos nyomás, kevés védelem | hullámokban érkező ellenség |
| **Kiegyenlített** | alapértelmezett: gazdaság, védelem és támadás arányosan | általános ellenfél |
| **Gyors roham** | nagyon korai, olcsó egységes támadás, a gazdaság rovására | „tartsd ki 10 percig” küldetés |
| **Terjeszkedő** | sok bánya és telep, késői, de óriási hadsereg | „állítsd meg, mielőtt megerősödik” |
| **Technológus** | gyors kutatás, felemelkedés, elit és ostrom egységek, hősök | erős, de kevés egység |
| **Portyázó** | gerillaharc: munkásokat és távoli bányákat támad, kerüli a nyílt csatát | „védd a karavánt / a falvakat” |
| **Ostromló** | ostromgépekkel és szárazföldi támogatással épületeket bont | „tartsd meg a várat” |
| **Csapatjátékos** | a szövetségest segíti, közös támadásokra időzít, erősítést küld | szövetséges gépi társ |
| **Alkalmazkodó** | a felderítés alapján vált a fenti stílusok között (ellenstratégia) | „okos” ellenfél a Mester szinten |
| **Népe szerinti** | a faj jelleméhez illő alapstílus (pl. Wullof: portyázó éjszakai támadások; Ursanok: védekező; Croftianok: terjeszkedő fertőzés) | alapértelmezett a hadjáratban |

A személyiség **adat** (paraméterkészlet). Ugyanaz a kód többféleképpen viselkedik:

- **Erőforrás-arányok:** gazdaság / hadsereg / védelem / kutatás aránya.
- **Ütemezés:** az első támadás ideje, és mekkora túlerő kell egy támadáshoz.
- **Egységösszetétel:** egységmix-súlyok.
- **Taktika:** visszavonulási küszöb, portyázási hajlam, terjeszkedési ütem, falépítési hajlam.
- **Hős és képesség:** hős- és képességhasználat.
- **Időhatások:** a napszak és az időjárás kihasználása (D35). Például a Wullof éjjel támad, a Huk'ucha esőben.

## 2. A motorban

- **Lua-AI keretrendszer (M9).** Minden gépi játékos egy homokozott Lua-állapot az M3-as szkriptréteggel:
  - utasításkeret és memóriakorlát;
  - determinisztikus, a szimuláció része, ezért lockstep hálózaton is működik.
- **Felépítés** (a 0 A.D. Petra mintájára):
  - menedzserek: gazdaság, építés, hadsereg, felderítés, védelem, kutatás, diplomácia;
  - feladatlisták és célok;
  - a személyiség súlyozza a menedzserek döntéseit.
- **AI-API** – csak a látott információ:
  - saját egységek, épületek, erőforrások;
  - a felderített térkép, a látott ellenséges egységek;
  - a szabályok és az adatok (sablonok, költségek);
  - parancsok: ugyanazok, mint a játékosé, a parancssoron át.
- **Teljesítmény:**
  - az AI nem fut minden tickben: a menedzserek ütemezve, eltolva futnak, tickenkénti kerettel;
  - a drága elemzés (térkép, veszélytérkép) ritkán és fokozatosan készül.
- **Küldetés-AI (M11, WC3-szerűen):** a küldetés szkriptje egy gépi játékost vezérelhet:
  - **építési lista:** mit és mikor építsen;
  - **támadási hullámok:** összetétel, időzítés, célpont;
  - **egyéb utasítások:** őrzött terület, „ne terjeszkedj”, „ennyi egységnél ne többet”.

  Teljes AI-modul is lecserélhető egy küldetésre szabottra.

## 3. Modolás (M13)

- Egy AI **mod** a `mods/<név>/ai/<azonosító>/` mappában van:
  - `ai.json`: név, leírás, támogatott népek, személyiség-paraméterek, alapértelmezések;
  - Lua-modulok.
- A lobbi listázza a telepített AI-kat. A mod.io-ról letölthető, és a modlock része.
- Három szint:
  1. új **személyiség** a beépített keretrendszerhez (csak adat);
  2. **menedzser** cseréje vagy bővítése (Lua);
  3. teljesen **saját AI** (Lua, ugyanazzal az API-val).
- A beépített AI-k is így készülnek (`base/ai/...`), mintaként.

## 4. Próbák (M9 kapu, kiegészítve)

- 100 gép–gép meccs elakadás nélkül, minden személyiséggel és nehézséggel.
- AI-FOG = 0: rejtett információt nem használ (a tesztben a köd mögötti állapot változtatása nem változtat a döntésen).
- A személyiségek mérhetően különböznek: az első támadás ideje, a védelmi arány és a terjeszkedés üteme.
- A nehézségek sorrendje tartja magát: a magasabb szint az esetek többségében nyer az alacsonyabb ellen.
- A mintamod (új személyiség, adatból) betölthető és a lobbiban választható.

## 5. Tanuló AI (D36: kérve)

A gépi ellenfél **a meccsen belül** tanul, és a meccs végén elfelejt mindent (nincs meccsek közötti emlékezet –
a hálózati játék és a visszajátszás determinizmusa ezt kívánja).

- **Mit figyel:** a felderített ellenséges egységösszetétel, az ellenfél támadásainak ideje és iránya, mely egységei
  vesztek el mi ellen, hol vannak a falai és a tornyai.
- **Mit igazít:** az egységmix súlyait (ellencsapat: sok lovas ellen lándzsás, sok íjász ellen gyors lovas …), a
  védekezés helyét (ahonnan jöttek a támadások), a támadás irányát (a gyenge pontra), az időzítést (az ellenfél
  szokásos támadási ideje előtt védekezik).
- **Hogyan:** az alkalmazkodás a személyiség paramétereit tolja el, korlátok között (a védekező nem válik
  rohamozóvá); a tanulás állapota a gépi játékos Lua-állapotában, a szimuláció része (hash, mentés, visszajátszás).
- **Nehézség szerint:** Kezdő és Könnyű nem tanul; Normál lassan; Nehéz és Mester gyorsan; az *Alkalmazkodó*
  személyiség a legerősebben.
- **Próba (M9):** egy ismétlődő játékosstratégia ellen a tanuló AI a meccs második felében mérhetően jobban teljesít,
  mint ugyanaz tanulás nélkül.

## 6. Döntések (tulajdonos, 2026-11-12)

- **Nehézség:** 6 szint – Kezdő, Könnyű, Normál, Nehéz, Mester, és a választható „Őrült” (nyíltan jelölt csalás:
  +50% gyűjtés); a többi szint sosem csal.
- **Hadjárat:** az ellenfél alapértelmezett személyisége a *népe szerinti* (a küldetés felülírhatja).
- **Tanuló AI:** igen, a meccsen belül (5. pont).
