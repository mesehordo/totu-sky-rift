# TOTU: Sky Rift – játéktervezési dokumentum

> A világ és a történet kánonja: [docs/EGSZAKADAS_vilag_es_tortenet_v01.md](docs/EGSZAKADAS_vilag_es_tortenet_v01.md); a megvalósított hadjárat: [docs/STORY_CAMPAIGN.md](docs/STORY_CAMPAIGN.md).

> Valós idejű stratégiai játék (RTS) három aszimmetrikus fajjal.
> Játékmenet: StarCraft / Warcraft III-szerű bázisépítés és gazdaság, egy csipet *Armies of Exigo* (nagyobb, "csatatér-szerű" seregek, szörnyek, fizikai páncél) és *Warcraft III* (hős-szerű varázslók, varázsképességek) hangulattal.
> Kezelőfelület: Command & Conquer-szerű jobb oldali **sidebar** (radar, nyersanyag-kijelző, építési és kiképzési ikonoszlopok, "óralapos" folyamatjelző).

---

## 1. Háttér és alapkoncepció

Évszázadokkal ezelőtt meteorzápor hullott a világra. A becsapódások krátereiben kékesen izzó **meteorit** maradt hátra – ez hozta el a mágiát. Három nép küzd a kráterekért:

| Faj | Megfelelő SC-logika | Hangulat |
|---|---|---|
| **Szürke elfek** | Protoss | aztékos, lépcsős piramisok, tollas díszek, jáde és obszidián, gyík- és dínóhátas lények |
| **Ursanok** | Terran / Birodalom | magas, medveszerű nép, viking hosszúházak, pajzsfalak, mamutok, kardfogúak, jégkorszaki állatok |
| **Croftianok** | Zerg | lovecrafti, rovarszerű, idegen raj, élő épületek, nyálkaszőnyeg ("mélység") |

A győzelem feltétele: az ellenfél összes épületének lerombolása.

---

## 2. Nyersanyagok

| Nyersanyag | Forrás | Gyűjtés | Szerep |
|---|---|---|---|
| **Fa** | erdők (fánként ~120 egység, a fák kivághatók → a térkép "nyílik") | 8 / 2,2 mp | épületek, íjak, lovas felszerelés |
| **Kő** | kőfejtők (2×2, 1500 egység) | 6 / 2,6 mp | épületek, tornyok, nehézegységek |
| **Meteorit** | kráterek (2×2, 1000) – kevés, a térkép közepén és a "kinti" pontokon | 4 / 3,5 mp | mágia, varázslók, elit egységek, fejlesztések |

* **Étel nincs** (3. gazdasági változat): a régi ételköltségek fába és kőbe váltak át.
* **Létszám**: külön nem nyersanyag; az egységek eltérő helyet foglalnak (mamut 6), max. 200. Fajonként másképp nő:
  elfek – Napobeliszk, ursanok – Tanya, croftianok – Szemhordozó (egység), wullof – felszedhető Tipi,
  gombanép – a hálózat minden összekötött épülete, Vasbirodalom – lélekreaktor. Részletek: docs/FACTIONS.md.
* Leadóhely: a főépület, valamint az ursan **Fatelep** és a croftian **Emésztőkamra**.
* A meteorit ritka → ez a "gáz": a magasabb tech kapuja, és egyben a térképkontroll célpontja.

---

## 3. A fajok

### 3.1 Szürke elfek (Protoss-logika)

**Fajmechanikák**
* **Megidézés**: a Kőfaragó csak *elindítja* az építést, utána visszamegy dolgozni – az épület magától "kel ki" a földből.
* **Napobeliszk-mező**: a Napszentély és az obeliszk kivételével minden épületet csak egy kész Napobeliszk energiamezejébe lehet tenni; ha az obeliszk elpusztul, az épület **áram nélkül marad** (nem termel).
* **Meteoritpajzs**: minden egységnek és épületnek pajzsa van, ami 6 mp sérülésmentesség után gyorsan visszatöltődik. Kevés, drága, de erős egység.
* Hátasaik **farkatlan gyíkok** és tollas, madárszerű **dínók** – díszes tollkoronákkal, jáde ékszerekkel.

| Egység | Szerep | Megjegyzés |
|---|---|---|
| Kőfaragó | munkás | építést indít, majd visszatér gyűjteni |
| Tollas harcos | közelharci gyalogos | obszidiánpengés buzogány (macuahuitl) |
| Jáde-íjász | távolsági, lég+föld | |
| Raptorlovas | gyors lovasság (farkatlan gyík) | bónusz a távolságiak ellen, **Vágta** képesség |
| Napszóró pap | varázsló | **Napvihar**: területi sebzés (Psionic Storm analóg) |
| Quetzal | repülő tollas dínó | lég+föld |
| Tollaskoronás rex | ostrom/óriás | területi harapás, nagy pajzs |

| Épület | Funkció |
|---|---|
| Napszentély | főépület, leadóhely, munkás |
| Napobeliszk | ellátmány + energiamező |
| Harcosok csarnoka | gyalogság |
| Obszidián műhely | fegyver/pajzs fejlesztések |
| Raptorkarám | lovasság, rex |
| Csillagvizsgáló | papok, látás-fejlesztés |
| Tollfészek-piramis | Quetzal |
| Jádeoszlop | védőtorony |

### 3.2 Ursanok (Terran/Birodalom-logika)

**Fajmechanikák**
* **Kétkezes építés**: a Telepes az építkezés teljes ideje alatt ott marad; **több munkás gyorsabban épít**. A sérült épületeket ingyen **javítják**.
* **Pajzsfal**: az alapegység a **Pajzshordozó** – bekapcsolva -60% távolsági sebzés, +2 páncél, fele sebesség. A fal mögött állnak a szigonyvetők.
* Strapabíró, olcsó, sok HP-s egységek; erős, páncélozott épületek.
* Jégkorszaki állatok: **gyapjas mamut** (rajta kőhajítók), **kardfogú tigris**, **viharholló**.

| Egység | Szerep |
|---|---|
| Telepes | munkás, épít, javít |
| Pajzshordozó | tank-gyalogos, **Pajzsfal** |
| Szigonyvető | távolsági, bónusz nagy testű célpontok ellen |
| Medvebőrös berserker | sebző közelharcos |
| Rúnapap | gyógyító (automatikus **Rúnás gyógyítás**) |
| Kardfogú lovas | gyors lovasság, **Roham** |
| Viharholló | repülő felderítő, légvédelem |
| Harci mamut | ostrom, hátán kőhajítók, terület-sebzés, épületbónusz |

Épületek: Nagyterem, Hosszúház, Fatelep, Harcosok háza, Rúnakovács, Vadak karáma, Sasszirt, Őrtorony.

### 3.3 Croftianok (Zerg-logika)

**Fajmechanikák**
* **Mélység (nyálkaszőnyeg)**: a főépület kivételével minden épület csak mélységre kerülhet; a mélységet a Kaptár, a Tápgomba és a többi épület **fokozatosan terjeszti**. Rajta az egységek 30%-kal gyorsabbak, és háromszor gyorsabban regenerálódnak.
* **Lárvák**: minden egység a Kaptár lárváiból alakul (max. 3, ~10 mp-enként újra). A lárvák **párhuzamosan** alakulnak át – egyszerre 3 egység készülhet.
* **Átalakulás**: a Csápszolga *maga válik* az épületté (elfogy).
* **Regeneráció**: minden egység és épület folyamatosan gyógyul.
* A Karmosból egy lárva **kettőt** ad: olcsó, gyors raj.

| Egység | Szerep |
|---|---|
| Csápszolga | munkás, épületté alakul |
| Karmos (×2) | olcsó, gyors rajegység |
| Tüskeköpő | távolsági, lég+föld |
| Savköpő | tüzérség, terület-sebzés, épületbónusz |
| Agyfaló | varázsló, **Őrület köde**: lassítás + sebzés |
| Szárnyas rém | repülő |
| Csápos horror | óriás közelharcos |

Épületek: Kaptár, Tápgomba, Emésztőkamra, Ikrafészek, Tüskeverem, Agyszentély, Csontcsúcs, Tüskés gyökér.

### 3.4 Egyensúly-filozófia
* Elfek: **minőség** – drága, pajzzsal, varázslattal; sebezhetőek, ha elvesztik az obeliszkeket.
* Ursanok: **kitartás** – páncél, gyógyítás, javítás, pajzsfal; lassúak.
* Croftianok: **mennyiség és mobilitás** – parallel termelés, regeneráció, mélységen gyorsak; gyenge egyedi egységek.
* Ellenkezési háromszög: *közelharc → távolsági → lovas/óriás → közelharc*, plusz `bonus` értékek (pl. szigony a "large" ellen, raptor a "ranged" ellen).

---

## 4. Játékmotor

### 4.1 Technológia
* **Python 3.11+ és pygame-ce** – gyors prototípus, könnyű moddolás, nincs fordítás.
* Grafika: **előrenderelt pixel-art sprite-ok** (WC2/SC1 stílus) – a `tools/make_sprites.py` csontvázas 3D-primitív modellekből renderel 8 irányú, animált sheet-eket JSON metaadattal; a játék a `manifest.json` alapján tölti be őket. Hiányzó sprite esetén kódból rajzolt alak a tartalék.
* Későbbi lehetőség: a szimulációs mag (útkeresés, harc) átírható Cythonra/Rustra, a Python-réteg (adatok, UI, script) maradhat.

### 4.2 Architektúra

```
main.py                 belépési pont, menü, fő ciklus
rts/data.py             JSON-adatok betöltése, modok összefésülése, validálás
rts/world.py            térkép, csempék, foglaltsági rács, procedurális térképgenerátor
rts/pathfinding.py      A* (8 irány, sarokvágás nélkül), útvonal-simítás
rts/entities.py         Unit / Building / Projectile / AreaEffect – állapotgépek
rts/game.py             szimuláció: játékosok, parancsok, harc, köd, mélység, energiamező
rts/ai.py               adatvezérelt gépi ellenfél
rts/render.py           procedurális rajzolás (egységek, épületek, ikonok)
rts/ui.py               C&C sidebar, kamera, kijelölés, beviteli módok
data/                   alapjáték adatai (game.json, abilities.json, factions/*.json)
mods/                   modok (felülírás + új tartalom + Python hookok)
```

* **Fix lépésközű szimuláció** (30 Hz), a rajzolás ettől független → determinisztikus, később *lockstep* multiplayer és visszajátszás építhető rá.
* **Parancs-minta**: minden játékos-akció (`cmd_move`, `cmd_attack`, `cmd_build`, `cmd_train`, `cmd_cast` …) a `Game` objektumon megy át – az ember UI-ja és a gépi ellenfél **ugyanazt** az API-t használja. Hálózati játéknál ezek a parancsok utaznának.
* **Egység-állapotgép**: minden egység egy aktuális *parancsot* (move, attack, amove, gather, build, construct, repair, hold, cast) és egy parancs-sort (Shift) futtat.
* **Útkeresés**: A* csempe-rácson, kéréssor (tick-enként korlátozott számú keresés → nincs akadás nagy seregnél), elakadás-figyelés és újratervezés, csoportos mozgásnál alakzat-célpontok.
* **Térbeli hash** (4×4 csempés cellák) a célpontkereséshez és az ütközéshez.
* **Köd (fog of war)**: felfedezetlen / felfedezett / látott állapot játékosonként.

### 4.3 Moddolhatóság
Minden játékadat JSON: egységek, épületek, költségek, képességek, fejlesztések, a gépi ellenfél építési sorrendje, térkép-paraméterek.

* `mods/<név>/mod.json` – név, leírás, `enabled`, `priority`.
* A mod mappájában ugyanazokkal a relatív útvonalakkal lévő JSON fájlok **mélyen összefésülődnek** az alapadatokkal (`null` érték = kulcs törlése). Így egy mod lehet 3 soros egyensúly-javítás vagy egy teljes **új faj** (`factions/uj_faj.json`).
* `mods/<név>/script.py` – opcionális Python hookok: `on_load(data)`, `on_game_start(game)`, `on_tick(game, dt)`.
* Indításkor validálás: hiányzó hivatkozások (pl. nem létező egység a `produces` listában) figyelmeztetést adnak.
* Képesség-típusok (adatból paraméterezhetők): `toggle`, `area`, `autoheal`, `self_buff`. Fejlesztés-effektek: `damage`, `armor`, `hp`, `shield`, `range`, `sight`, `speed_pct`, `regen`.

### 4.4 Kezelőfelület (C&C-stílus)
* **Jobb oldali sidebar**: felül nyersanyagok + ellátmány, alatta **radar (minitérkép)**, majd fülek: **ÉPÜLETEK / EGYSÉGEK / FEJLESZTÉS**, alattuk ikonrács.
* Egység-ikon kattintás: a megfelelő épület *legrövidebb* sorába kerül (nem kell kijelölni az épületet – mint a C&C-ben). Jobb klikk: visszavonás. "Óralapos" folyamat-takarás az ikonon, darabszám-jelző.
* Épület-ikon kattintás → elhelyezés mód (zöld/piros szellem, elf-energiamező / croftian mélység kijelzés) → a kijelölt vagy a legközelebbi munkás megy építeni.
* A sidebar alján: kijelölés-panel (HP/pajzs/energia, gyártási sor, parancs- és képesség-gombok).
* Egér: bal = kijelölés/doboz, jobb = kontextus-parancs (mozgás, támadás, gyűjtés, segít építeni, javít, gyülekezőpont).

### 4.5 Gépi ellenfél
Adatvezérelt (`ai` blokk a faj-JSON-ban): munkáslétszám, nyersanyag-arányok, építési sorrend, sereg-összetétel súlyokkal, fejlesztési sorrend, támadási küszöb. Viselkedés: gazdaság → ellátmány → építés → termelés → fejlesztés → védekezés/hullámtámadás (növekvő küszöb), képességhasználat (Napvihar/Őrület köde csoportokra, Pajzsfal távolsági ellenfélnél, Vágta/Roham üldözéskor).

---

## 5. Fejlesztési ütemterv

| Fázis | Tartalom | Kimenet |
|---|---|---|
| **0. Prototípus** *(ez a repó)* | szimulációs mag, 3 faj adatból, gazdaság, harc, gépi ellenfél, C&C UI, modrendszer | játszható skirmish 1v1 |
| **1. Vertikális szelet** | sprite-ok/animációk (1 faj teljesen), hangok, kampány-prototípus 1 küldetéssel | belső demo |
| **2. Tartalom** | mindhárom faj grafikája, 3-4 térkép, térképszerkesztő (JSON), hősök (WC3-hatás), tárgyak | alfa |
| **3. Multiplayer** | determinisztikus lockstep, parancs-szinkron, visszajátszás, lobby | zárt béta |
| **4. Egyensúly & polírozás** | telemetria, balansz patch-ek (adatból!), gépi ellenfél nehézségi szintjei, lokalizáció | béta |
| **5. Kiadás & mod-támogatás** | mod-böngésző, dokumentált adat-séma, Steam Workshop | 1.0 |

Csapat-javaslat: 2 programozó (mag + UI/eszközök), 1 game designer (adat/balansz), 2 grafikus (egységek, épületek), 1 hang. Rövid, 2 hetes sprintek; minden sprint végén játszható build, játékteszt; a balansz döntések mindig JSON-ban élnek → nincs kódváltozás.

Kockázatok: Python teljesítmény nagy seregnél (megoldás: útkeresési sor, térbeli hash, később natív mag), determinizmus lebegőpontos számításnál (multiplayer előtt fixpontos mag), aszimmetrikus fajok balansza (gyakori gép a gép elleni tömegszimuláció – a `--simulate` kapcsoló erre már most használható).
