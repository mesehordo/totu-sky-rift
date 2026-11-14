# TOTU: Sky Rift – történet és hadjárat

*© 2026 Mesehordo – minden jog fenntartva. Lásd: [LICENSE.md](../LICENSE.md).*

A világ és a történet kánonja:
**[EGSZAKADAS_vilag_es_tortenet_v01.md](EGSZAKADAS_vilag_es_tortenet_v01.md)** (Avarra, Edrava, a Fényes Eső, a csillagérc,
Ordan, a kethri nép és az Idegen Kórus, a hat főszereplő, a 36 küldetéses szerkezet). Ez a fájl azt írja le, hogyan
valósul meg a játékban.

## Hogyan kapcsolódik a játékhoz

| Kánon | A játékban |
|---|---|
| Csillagérc (meteorit) | a „Meteorit” nyersanyag; mágia, lélekhorgonyok, elit egységek |
| Aurethi Örök Birodalom, Ordan – a Kéz, amely Megtart | Aurethi Birodalom; „Ordan temploma” épület; a lélekidézés a horgonymentés |
| Szerves ellátmány az aurethieknél | a 3. gazdasági változatban nincs étel: az Aurethi Birodalom létszámát lélekreaktorok adják |
| Idegen Kórus, kethri nép | Croftianok (a Kórus); a megszállt testek kethrik – a tojáskamrák, Keth és a leválás |
| A Mélykertek gombanépe | „Micéliák” munkacímen – a kánon szándékosan nem ad végleges fajnevet |
| Kilenc Lépcső / Északi Eskük / Hét Út | az elf, ursan és wullof fajleírásokban |
| Hősök | Tava, Ragna Kőváll, Szelai, Mór, Livia Varr, Nera, Keth – szintet lépnek (1–10), 4 tanulható képességük és 6 helyes leltáruk van; a fejlődésük a profilban hadjáratról hadjáratra megmarad; ha a küldetésben elesnek, a küldetés elbukik |

## A hadjárat (36 küldetés – `campaigns/egsz_*`, forrás: `tools/campaign_egszakadas.py`)

A hadjáratok sorban nyílnak meg (a `requires` mező szerint); a haladás profilonként mentődik.

| Hadjárat | Faj, hős | Küldetések |
|---|---|---|
| Előjáték – A második lélegzet | Aurethi Birodalom, Livia | A második lélegzet |
| I. A föld alatt énekelnek | Wullof, Tava (+Mór) | Hétfolyó vására; Keserű kutak; A föld megnyílik; Két út között; Akik bennünk maradtak |
| Átvezető – A vegyes konvoj | Ursan, Ragna + Tava | A vegyes konvoj |
| II. A pajzs mögött | Ursan, Ragna | Üres magtárak; A felolvadt bánya; Tojások a hamu alatt; A jarl adóssága; Nyitott kapuk |
| III. A kilencedik lépcső | Szürke elfek, Szelai | A tiszta érc; Idegen tornyok; A lezárt archívum; Repedés a hálózaton; A ledöntött lépcső |
| IV. Akikre emlékezünk | Gombanép, Mór | Száraz kertek; Az emlék ára; Az első betegek; Egy név a Kórusban; Az égő kert |
| Átvezető – A finomító | Aurethi Birodalom, Livia + Ragna | A finomító |
| V. Az isten, aki nem enged el | Aurethi Birodalom, Livia + Nera | A lefoglalt szállítmány; Az üres serlegek városa; Az Örök Hajnal; A szabad horgony; Feltöltés megtagadva |
| VI. A sokaság neve | Croftian/kethri, Keth | A sebek egyesítése; Ismerős kövek; A visszatérő név; Leválás; A belső kapu |
| Közös befejezés – A névtelen hajnal | Wullof / Aurethi Birodalom / Ursan + mind a hat hős | A híd; Az üres trón; A névtelen hajnal |

## Döntések és következményeik

A kánon szerint a döntések nem láthatatlan jó–rossz pontszámot adnak, hanem erősítéseket, párbeszédeket és az epilógust
módosítják. Döntéskor a játék megáll; a választás jelzőt (flag) állít, amely a **profilban marad**, így a későbbi
küldetések és a befejezés ehhez igazodnak. Újrajátszáskor az új döntés felülírja a régit.

| Hol | Döntés | Azonnali hatás | Később |
|---|---|---|---|
| I/4 Két út között | Folyóparti klán vagy Sziklavári családok előbb | harcosok, ill. kő+fa+munkások | I/5 és A híd: más erősítés; epilógus |
| II/1 Üres magtárak | Ragna beengedi a menekülteket, vagy a parancs szerint elküldi | −300 fa + munkások, ill. +130 fa és +70 kő | II/5: hűséges pajzshordozók vagy Ragna emlékezése; epilógus |
| III/2 Idegen tornyok | Szelai teljesíti a parancsot, vagy meghallgatja Tavát | totemek ledöntése, ill. a cél teljesül, Tava és íjászai csatlakoznak | epilógus |
| IV/5 Az égő kert | Mór a kethri betegek emlékeit vagy az ősi dalokat menti | –, ill. nyersanyag | VI/4: több leválasztható kethri; epilógus |
| V/4 A szabad horgony | Livia elzárja Nerát, vagy hagyja dönteni | Nera később, magától csatlakozik, ill. azonnal veled harcol | epilógus |
| VI/4 Leválás | Keth a harcosokat vagy a fiatalokat viszi | Tüskeköpők, ill. több Karmos | epilógus |
| Bef./1 A híd | Ostrom vagy evakuálás | erősítés, ill. nyersanyag | epilógus |

## A küldetés-motor (bővítéshez)

Küldetésfájl: `campaigns/<hadjárat>/<küldetés>.json`; a hadjárat: `campaign.json` (`title`, `desc`, `order`,
`requires`, `missions`). Modok saját mappájukban is hozhatnak hadjáratot (`mods/<mod>/campaigns/...`).

* **Kiinduló helyzet**: `map` (seed, méret), `player` (`resources`, `units` – hősök is, `buildings` előre lerakva,
  `start_packed` – wullof nomád kezdés), `enemies` (faj, név, `hold` = eddig nem támad, `aggression`, saját egységek
  és épületek).
* **Célok** (`after` mezővel láncolhatók): `destroy_all`, `destroy` (épület-/egységtípus – csak ha már létezett),
  `gather`, `survive`, `build`, `train`, `kill`, `reach` (egységek egy térképpontra; jelzővel a térképen és a radaron).
* **Bukás**: `fail_if_lost` – ha egy hős vagy védendő épület (pl. kethri tojáskamra, Fórum) elvész.
* **Triggerek**: időzített (`at`) vagy célhoz kötött (`on_objective`): üzenet (színnel), `spawn` (ellenséges hullám
  vagy szövetséges erősítés – `owner: 0` – a bázis felé, a hátországból vagy adott pontról), `reward`, `aggression`.
  Feltétel: `if_flag` (`"név"`, `"!név"` vagy lista). Az `on_choice` az aktuális küldetésben hozott döntés
  opciójának `flag` mezőjére figyel, például `"on_choice": "segitseg"`.
* **Döntés**: trigger `choice` mezővel (`title`, `text`, `options`); minden opció: `label`, `hint`, `flag` és bármely
  trigger-hatás, továbbá `complete` (cél teljesítése) és `add_objective` (új cél).
* **Következmények**: `player.units_if` (jelzőtől függő kezdő egységek), `victory_variants` (jelzőtől függő epilógus-sorok).
* **Hősök**: `hero_level` (a játékos hőseinek minimális kezdőszintje), `enemy_hero_level`; a profilban tárolt
  hős-állapot (szint, képességek, tárgyak) ennél magasabb is lehet. `creeps: true` = semleges szörnytáborok a pályán.

## Ami a kánonból még nincs megvalósítva

* **Kampányhősök (kreátori döntés, 2026-11-06):** a küldetésekben szerepelhetnek olyan történeti hősök is, akik a nép alap hőslistáján nincsenek (pl. Sairen az Oonihoz, Relluk a Huk’uchához, Mór a Micéliákhoz) – ez minden népnél engedélyezett. A sima csatában (és a többjátékos módban) mindig a nép alap 3 hőse marad. A részletes világ- és történetanyag: `tortenet/story/`.
* A hősök szerephez kötött, történeti különleges képességei (útnyitás, hálózatbontás, horgonymentés) küldetés-célként.
* Szövetséges gépi játékos (most a szövetségesek a játékos irányítása alá kerülnek), és kettőnél több fél egy pályán.
* Egyedi, kézzel tervezett pályák (most procedurális térképek, kijelölt célpontokkal); átvezető jelenetek.
