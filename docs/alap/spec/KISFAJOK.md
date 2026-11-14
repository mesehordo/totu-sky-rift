# Szövetséges kisnépek és a Vadon – viselkedési specifikáció, átvizsgálás és javaslat

Kelt: 2026-11-12 · referencia: `G` = `rts/game.py`, `E` = `rts/entities.py`, `H` = `rts/heroes.py`; adatok:
`data/factions/ally_*.json` (13 kisnép), `data/factions/vadon.json`, `data/camps.json`. Formátum: [README.md](README.md).
Kapcsolódó: MECH-ALLY ([FAJMECHANIKAK.md](FAJMECHANIKAK.md)), TABOR-1–2 ([HOSOK_TARGYAK_TABOROK.md](HOSOK_TARGYAK_TABOROK.md)), D11.

---

## A. KIS-1 – A szövetség

- **Megkötés** (MECH-ALLY): kész Diplomácia csarnoka (`diplomacy`; mind a 7 főnépnek van, a rejtett Micéliáknak
  nincs), egyszer, véglegesen, `ally_cost` (150 arany + 50 meteorit) áron. Egyéni csatában a gépnek előírható a
  kisnép (`forced_ally`); egyébként a faj `ai.ally_pref` listájából választ (80%-ban az első kettőből).
- **Ami megnyílik:** a kisnép `ally_pack`-je – **3 épület** és **5–6 egység** – és az épületekben kutatható
  **3 kutatás**. Az elemek `ally` mezője a kisnépre mutat; csak a megkötött szövetséggel érhetők el.
- **Egységcímkék:** minden kisnép-egység `ally` és `ally_<nép>` címkét kap; a saját kutatásaik általában erre szűrnek.

## B. KIS-2 – Beilleszkedés a gazda népbe

A kisnép elemei a **gazda játékoshoz** tartoznak, ezért a gazda szabályai rájuk is érvényesek:

| Gazdaszabály | Hat a kisnép elemeire? | Kód |
|---|---|---|
| Építési hely (mélység, energiamező, saját tó, lámpásfény, fonalkapcsolat) | **igen** – a kisnép-épületeknek nincs `no_creep` / `no_power` / `no_pond` / `no_light` jelzőjük | G 2261–2298 |
| Építési stílus (`builder`), építési szorzók | igen (a gazda munkása építi) | G 1460 |
| Energiaellátás (elfek): ellátatlan épület nem termel | igen | E 1748 |
| Faji töltés (`regen`), mélységi gyógyulás és -sebesség | igen (`owner.faction`) | E 191, 387 |
| Harci kedv (Ooni), falkaszellem (Wullof), Égkő-fény | igen | G 1973, 762, 1937 |
| Tó sebessége (Huk’ucha) | **igen** (D26; korábban a tó az egység saját faját nézte, és a Huk’ucha szövetségeseit lassította) | G 789 |
| Felemelkedés | a kisnép-épület is számít a különböző fajták közé | G 656 |

## C. KIS-3 – A 13 kisnép

| Kisnép | Jelleg | Egységek (típus) | Képességtípusok | Kik választják (gépi előny) |
|---|---|---|---|---|
| Daresh | darázsnép | 2 lándzsás, varázsló, 2 repülő | kapcsoló, ének, gyógyítás | Croftian, Wullof |
| Galóca | gombanép | közelharcos, lándzsás, óriás, lövész, varázsló | buff, nova, terület | Croftian |
| Ghoblin | ostromnép | 2 közelharcos, ostrom, lándzsás, lövész, óriás | kapcsoló, 2 nova | Ursanok, Aurethi Birodalom |
| Mhonyo | gépész nép | közelharcos, lövész, lándzsás, varázsló, óriás | buff, kapcsoló, ének, javítás, nova | Aurethi Birodalom, Ooni |
| Micéliák (kisnép) | gombafonal | munkás, 2 közelharcos, varázsló, lövész | passzív (4), összeolvadás, gyógyítás, terület, kapcsoló | – (csak játékos) |
| Niil-elfek | folyami elfek | 2 lándzsás, lövész, varázsló, óriás | kapcsoló, gyógyítás, terület, nova | elfek, Huk’ucha |
| Nila-uchana | – | lándzsás, 2 óriás, varázsló, lövész, lovas | idézés, nova, terület, gyógyítás, buff | Huk’ucha, Ooni |
| Pinguu | jégnép | 2 lándzsás, lovas, varázsló, lövész | kapcsoló, buff, gyógyítás, terület, ének | Ursanok, Huk’ucha |
| Shrissh-matah | – | varázsló, közelharcos, lándzsás, repülő, lövész | ének, villám, buff | Croftian |
| Tendüür | lámpásnép | 2 varázsló, közelharcos, lövész, óriás | gyógyítás, terület, buff, nova | elfek, Wullof |
| Tokla | – | óriás, 2 varázsló, lándzsás, lövész | nova, ének, gyógyítás, terület | Ursanok, Ooni |
| Tűz-elfek | – | 3 közelharcos, lövész, 2 varázsló | buff, nova, terület, idézés | elfek, Aurethi Birodalom |
| Zhan-zhama | tollas nép | közelharcos, lövész, lovas, varázsló, repülő | ének, idézés, gyógyítás | Wullof |

(A „jelleg” a fájlok `tagline` mezőjéből; a pontos egység- és képességadatok a [leltárban](../TARTALOM_LELTAR.md).)

## D. KIS-4 – Kisnép-táborok és a Vadon

- **Kisnép-tábor** (TABOR-1): a csatapálya szintenként kérhet egy kisnép-tábort (`map.camps`); 12 kisnépnek van
  tábor-leírása (`camps.json` `peoples`: szerepkörök és szintenkénti összetétel) – a kisnép-Micéliáknak nincs.
- **A Vadon** (`vadon.json`, semleges, `neutral`): 5 szörny – Kristálybogár (170 életerő, Kristálypáncél),
  Hamufarkas (230, Falkavadász), Éterlidérc (190, Éterlöket, távolsági), Kőtroll (650, Földrengés, nagy),
  Csillagérc-gólem (1600, Földrengés és Kristálypáncél, nagy); mind `creep` címkéjű. Az 5 tábor-szint összetétele
  és zsákmánya: TABOR-1.
- A semleges játékos mindenkinek ellensége; a tábor egységei nem hagyják el a táborukat (TABOR-2).

---

## E. Átvizsgálás

1. **A kisnép-kutatások egy része a gazda egész seregére hat.** 11 kisnépnél van ilyen (a Galóca és a
   Shrissh-matah kivétel): `all` címkével a Micélia Spórafonal, a Niil-elf Csatornarend, a Pinguu Ösvényvezetők,
   a Zhan-zhama Gyors keltetés (+sebesség) és a Tendüür Lámpásfény (+látótáv); más címkével a Daresh Rajzás
   (minden repülő), a Ghoblin Mészvakítás és a Tűz-elf Izzó nyílhegy (minden távolsági), a Mhonyo Kiegyensúlyozott
   penge (minden közelharcos) és Gépjavítás (minden gép), a Nila-uchana Kötés ereje és a Tokla Hágóőrség
   (minden nagy egység). Ez lehet szándékos („a szövetség a gazdát is erősíti”), de nincs kimondva, és
   egyenetlen: van kisnép, amelyik az egész sereget erősíti, és van, amelyik csak a saját egységeit.
2. **A Huk’ucha saját tava lassítja a saját szövetséges egységeit** (B táblázat) – valószínűleg nem szándékos:
   minden más gazdaszabály a kisnépre is érvényes.
3. **A kisnép-épületek a gazda építési szabályait kapják** (mélység, energiamező, tó, fény) – ez következetes és
   beleillik a népek identitásába; érdemes kimondani, és próbával védeni.
4. **A kisnép-Micéliák** egyetlen gépi faj előnylistáján sem szerepelnek, és táboruk sincs – a rejtett
   főfaj-Micéliákkal (D12) való átfedés miatt ez érthető, de dokumentálni kell.
5. **Jó és megtartandó:** a „1 kisnép, 3 épület” szerkezet (D11) egységes; a kisnépek szerepkörei (lándzsás,
   lövész, varázsló, óriás, repülő) lefedik a hiányokat; a táborok ugyanazokból a szerepkörökből épülnek.

## F. Javaslatok (0 A.D., WC3)

| Forrás | Megoldás | Mit old meg |
|---|---|---|
| **0 A.D.** – *csapatbónusz / civilizációs bónusz* kimondott hatókörrel | minden kisnép kap egy kifejezett **szövetségi bónuszt** (egy kutatás vagy állandó hatás, amely a gazda seregére hat), a többi kutatása csak a saját egységeire | E-1: egyenletes, kimondott szabály |
| **WC3** – *zsoldostábor* (semleges épület, ahol egységet lehet bérelni), **0 A.D.** – *zsoldosok* | a pályán álló kisnép-táborból – kiirtás helyett vagy után – zsoldos egységeket lehet bérelni, szövetség nélkül is, korlátozott számban | a táborok második szerepe; a kisnépek a szövetség előtt is megismerhetők |
| — | a „gazdaszabály” általános elve a motorban: a kisnép-elem a gazda népének mechanikáit örökli, kivéve, ahol az adat kifejezetten mást mond | E-2, E-3 |

### Tulajdonosi döntést kívánó pontok

1. **A kisnép-kutatások hatóköre:** (a) minden kisnép pontosan egy szövetségi bónuszt kap a gazda seregére, a többi
   kutatás csak a saját egységekre (javaslom); (b) maradjon a mai vegyes állapot; (c) minden kisnép-kutatás csak a
   saját egységekre.
2. **A Huk’ucha tava és a szövetséges egységek:** a tó a gazda népét nézze (a saját szövetségesei is gyorsulnak a
   tavon) – javaslom; vagy maradjon az egység saját faja.
3. **Zsoldosbérlés a kisnép-táborokból** (WC3, 0 A.D.) – bekerüljön-e.

---

**Döntés (D26, 2026-11-12):** minden kisnép pontosan egy kimondott szövetségi bónuszt ad a gazda seregére, a többi
kutatása csak a saját egységeire hat (a bónuszok balanszmunka); a tó a gazda népét nézi (a Python-referenciában is
javítva); zsoldosbérlés a kisnép-táborokból, pálya- vagy lobbibeállítással.
