# Kutatások, szabálymódosítók és fejlődési ágak – viselkedési specifikáció, átvizsgálás és javaslat

Kelt: 2026-11-12 · referencia: a Python-játék (`G` = `rts/game.py`, `E` = `rts/entities.py`), a fajadatok
(`data/factions/*.json`: 168 kutatás, 24 fejlődési ág) és a `data/game.json` (`summon_research`). Formátum:
[README.md](README.md). Kapcsolódó: [KEPESSEGEK.md](KEPESSEGEK.md) B-3 (öt módosítórendszer), D22.

Tartalom: **A.** kutatás · **B.** statnövelő hatások (`effects`) · **C.** szabálymódosítók (`rules`, 39 kulcs) ·
**D.** fejlődési ágak · **E.** átvizsgálás · **F.** 0 A.D.- és WC3-javaslatok és modell.

---

## A. KUT-RES – A kutatás menete

- **Adat:** `name`, `desc`, `icon`, `cost`, `time`, `effects`, `rules`, `requires` (épületek), `requires_research`,
  `requires_ascended`, `exclusive` (kizáró csoport), `ally` (kisnép eleme); épület `researches` (hol kutatható).
- **Szabály:**
  1. Indítás (`cmd_research`, G 3564): nem lehet már kész vagy folyamatban; nem tiltja **kizáró csoport** (ha a
     csoport bármely tagja kész vagy folyamatban, a többi tiltott); kell egy kész, **ellátott** (`powered`) épület,
     amelynek a listáján szerepel; teljesül minden előfeltétel; van rá nyersanyag.
  2. A legrövidebb sorú alkalmas épületbe kerül (sor ≤ 5); az ár azonnal levonódik; létszámot nem foglal.
  3. Elkészültekor (`finish_item`) bekerül a kész kutatások közé, és **egyszer** érvényesül (`apply_research`):
     a `rules` beolvad a játékos szabályaiba (C), az `effects` hozzáadódik a statnövelőkhöz (B).
  4. A már élő egységek és épületek `max_hp`, `hp`, `max_shield`, `shield`, `max_energy` értéke a növekménnyel
     azonnal nő (a sérülés abszolút értéke megmarad); az ezután készülők eleve a növelt értékkel jönnek.
  5. Visszavonáskor az ár visszajár; ha az épület elpusztul, a folyamatban lévő kutatás elvész (az ár nem jár vissza).
- **Kizáró csoportok ma:** Vasbirodalom `arato_fejlesztes` (2 kutatás), Wullof `groomi_kopes` (3 kutatás).
- **Hatás nélküli kutatások** (10 db: elfek 1, Huk’ucha 1, Vasbirodalom 3, Wullof 5): csak más elemek
  előfeltételei (`requires_research`, `ability_research`).
- **Közös kutatások:** `game.summon_research` – Idézés tartósítása I–II. (minden faj idéző épületénél).
- **Próba:** P1 – +3 páncél kutatás a meglévő és az utána képzett egységen is érvényes. P2 – 100/200 életerős
  egység +50 életerő kutatás után 150/250. P3 – kizáró csoport második tagja elutasítva.

## B. KUT-EFF – Statnövelő hatások (`effects`)

- **Forma:** `{stat, add, tags}`; a `tags` alapértéke `["all"]`. Csak **összeadás** van.
- **Érvényesülés** (`Player.stat`, G 88): a stat alapértékéhez hozzáadódik minden olyan növelés, amelynek címkéje
  `all`, vagy szerepel az egység/épület címkéi között, vagy egyezik az azonosítójával, vagy `building` (minden
  épületre), vagy `tower` (minden támadó épületre). Pajzs és energia csak annak nő, akinek eleve van.

| Stat | Kutatás/ág | Értékek | Hatás (kód) |
|---|---|---|---|
| `damage` | 65 | +2 … +6 | támadás alapsebzése (E 128) |
| `armor` | 40 | +1 … +3 | páncél (E 358) |
| `hp` | 34 | +3 … +13 | életerő (lásd A-4) |
| `speed_pct` | 22 | +10 … +30 % | sebesség (E 380) |
| `regen` | 7 | +1 /s | életerő-töltés (E 191) |
| `range` | 5 | +1 csempe | lőtáv (E 122) |
| `sight` | 5 | +1 … +3 csempe | látótáv |
| `shield` | 2 | +30 | pajzs |
| `energy` | 1 | +25 | energia |
| `carry_pct` | 1 | +50 % | rakomány (E 376) |

## C. KUT-RULE – Szabálymódosítók (`rules`)

- **Forma:** kulcs → érték (szám vagy szótár). Forrás: kutatás (26), fejlődési ág, közös kutatás.
- **Összefésülés** (`merge_rules`, G 3120): kulcsonként az **újabb felülírja** a régit – kivéve, ha mindkettő
  `chance` mezős szótár: ekkor a nagyobb esélyű marad. A szabályok **nem adódnak össze**.
- **Hatókör:** a játékos összes egységére, vagy a `tags` mezővel (`rule_tags`) szűrve.
- **A 39 kulcs csoportosítva** (mindegyikhez külön kódsor tartozik; a „Kód” a felhasználás helye):

**C-1. Paraméterhangolás (25 kulcs) – egy meglévő szám módosítása**

| Kulcs | Faj | Mit módosít | Kód |
|---|---|---|---|
| `power_add` | elfek | energiamező sugara + | G 738 |
| `energy_regen_ids` | elfek | megadott egységek energiatöltése × | E 200 |
| `faith_rate` | elfek, Huk’ucha | Áhítat/Tóének üteme (felülírás) | G 1618 |
| `garrison_dmg` | Ursanok | helyőrség sebzése fejenként | E 1718 |
| `supply_add` | Ursanok | épület létszáma + | G 2978 |
| `creep_regen_mult` | Croftian | mélységi gyógyulás × | E 196 |
| `larva_add`, `larva_mult` | Croftian | lárvák száma +, termelődése × | E 1738, 1743 |
| `rot_cap`, `rot_mult` | Croftian | rothadás kerete, üteme | G 1793, 1803 |
| `spike_max`, `spike_size`, `spike_mult` | Croftian | tüskék száma, mérete, ereje | G 2500, 2510, 2555 |
| `train_time_mult` | Croftian | kiképzési idő × (címkével) | G 3541 |
| `network_supply_add` | Micéliák | hálózati létszám + | G 2983 |
| `pond_size` | Huk’ucha | tavacska mérete | G 2436 |
| `summon_life_mult` | Huk’ucha | idézések élettartama × | E 1530 |
| `light_add`, `lantern_magic` | Ooni | fény sugara +, fénybeli varázserő × | G 742, E 178 |
| `drum_mult`, `morale_hurt_mult` | Ooni | dobos-töltés ×, morálvesztés × | G 1999, 1145 |
| `build_boost_mult`, `torii_pairs`, `kokutya_guard` | Ooni | építésgyorsítás, kapupárok, őrpáncél | G 1494, 2293, 2733 |
| `summon_research_mult` | közös | idézések élettartama × (felülírás: I. 1,25, II. 1,5) | E 19 |

**C-2. Viselkedés hozzáadása (14 kulcs) – új esemény- vagy feltételfüggő hatás**

| Kulcs | Faj | Hatás | Kód |
|---|---|---|---|
| `rage` | Ursanok | sebzés × `1 + max × hiányzó életerő-arány` (címkével) | E 131 |
| `low_hp_rage` | Ursanok | `below` életerő-arány alatt sebzés × `mult` | G 1106 |
| `no_weak` | Ursanok, Vasbirodalom | a megadott egységtípus ellen nincs gyengesége | G 962 |
| `pack_bonus` | Vasbirodalom (Légió) | falkaszellem (MECH-WUL-1) a címkézetteknek | G 764 |
| `raise_from_kills` | Vasbirodalom (2), Micéliák | ölésből esély idézett egységre | G 1205 |
| `convert_kills` | Croftian | ölésből esély az áldozat átállítására | G 1211 |
| `poison_attacks` | Croftian, Micéliák | nem varázs-támadás mérgez (élő célpontot) | G 1074 |
| `heal_on_kill` | Wullof | öléskor gyógyul (címkével) | G 1201 |
| `bounty_gold` | Wullof | zsákmány aranyat is ad | G 1236 |
| `moon` | Wullof | Holdtölte: `period` s-onként `dur` s-ig sebzés × `mult` | G 1098, 307 |
| `forest_stealth` | elfek, Micéliák | erdei rejtőzés (címkével) | G 1307 |
| `detect_all` | Wullof | minden egység felfed `n` csempén | G 1329 |
| `pond_dmg` | Huk’ucha | saját tavon sebzés × | G 1101 |
| `kill_heal` | Ooni | a megadott egységek öléskor gyógyulnak | G 1222 |

---

## D. KUT-PATH – Fejlődési ágak (`paths`)

- **Adat:** a 8 főfaj mindegyikének 3 ága (összesen 24): `name`, `tagline`, `desc`, `building` (az ág
  épülete), `units` (az ág egységei), `rules`, `effects`; `extra_buildings` – csak az adatgenerátor használja
  (`tools/factions_paths.py`: az ág további épületeit megjelöli), a játék a megjelölt elem `path` kulcsát nézi.
- **Szabály:** MECH-ASC (felemelkedés, választás egyszer); az ág `rules` és `effects` hatása kutatásként érvényesül;
  az ághoz kötött épület, egység és képesség (`path`, `ability_path`) csak az ág választásával érhető el; a
  hadjáratban nincs irányzár.

---

## E. Átvizsgálás – mennyire egységes és értelmes

1. **Két párhuzamos rendszer ugyanarra.** A `effects` csak összead, címkével szűr, és kumulatív; a `rules`
   felülír, saját szűrővel (`rule_tags`), és nem kumulatív. Ugyanaz a szándék („+20%”) a kettőben másképp viselkedik.
2. **39 szabálykulcs, 39 kódsor.** A 25 paraméterhangoló kulcs mind egy-egy meglévő adatérték módosítása, mégis
   mindegyik saját kulcsot és kódot kapott (pl. `power_add` a `power_radius`-hoz, `light_add` a `light_radius`-hoz,
   `larva_add` a `larva`-hoz). Modder csak meglévő kulcsot használhat.
3. **A felülírás sorrendfüggő.** Két ugyanazt a kulcsot adó forrásnál (pl. `summon_research_mult` I. és II.)
   a később kész nyer; ez ma szándékosan így működik (a II. 1,5-öt ad, nem 1,25 × 1,5-öt), de rejtett.
   A `raise_from_kills` két Vasbirodalom-forrásnál a nagyobb esély marad – ez egyedi szabály.
4. **Csak összeadó statnövelés.** Százalékos növelésre (`speed_pct`, `carry_pct`) külön statnév kell; szorzó
   (`× 1,1`) nincs.
5. **A viselkedés-szabályok valójában passzív képességek** (C-2): ölésre, találatra, életerő-küszöbre vagy
   időszakra reagálnak – a D22-ben elfogadott képességmodell eseményalapú passzív képességei pontosan ezek.
6. **Jó és megtartandó:** a kutatások egyszeri, determinisztikus érvényesülése; az életerő-növelés a sérülés
   megtartásával (WC3-szerű); a kizáró csoportok (a 0 A.D. „pair” technológiáinak megfelelője); az ágak
   adatvezérelt felépítése.

## F. Javaslatok és modell az új motorhoz

| Forrás | Megoldás | Mit old meg |
|---|---|---|
| **0 A.D.** – *Technologies* + *Modifications* (érték-útvonal, `add` / `multiply` / `replace`, osztályszűrő) | a kutatás egységes **módosítólistát** ad: `{útvonal, művelet, érték, szűrő}`, pl. `{"path": "building.power_radius", "op": "add", "value": 2}` | E-1, E-2, E-4: a 25 paraméterhangoló kulcs adat lesz, új kulcs kód nélkül adható |
| **0 A.D.** – *Requirements* (`All` / `Any` kifejezés: épület, kutatás, fázis, ág) | egységes előfeltétel-kifejezés a mai `requires`, `requires_research`, `requires_ascended`, `path`, `ally` helyett | a feltételek kombinálhatók, a modder bármit kifejezhet |
| **0 A.D.** – *pair technologies* | a meglévő kizáró csoport megtartása, a felületen párként | – |
| **WC3** – *többszintű fejlesztés* (pl. kovácsolt kardok 1–3), szintenként növekvő ár és idő | `levels` a kutatásnál, mint a képességeknél | a sok „I., II., III.” kutatás egy elemmé válik |
| **D22 képességmodell** | a C-2 viselkedés-szabályok **passzív képességek** lesznek, amelyeket a kutatás vagy az ág a szűrt egységeknek ad (`grant_ability`) | E-5: egy rendszer a képességekre és a faji/kutatási viselkedésre |

**Halmozási szabály (javaslat):** a módosítók forrásonként halmozódnak (összeadás, majd szorzás, végül
felülírás, rögzített sorrendben); azonos forrás (ugyanaz a kutatás) nem kétszer. A mai „felülírás” esetek
(`summon_research_mult`) a konverzióban `replace` műveletet kapnak, így a játékmenet nem változik.

**Konverzió:** a 25 paraméterhangoló kulcs és a 10 statnév eszközzel átírható módosítókká; a 14 viselkedés-kulcs
passzív képesség lesz. A mai viselkedés (A–D) a próbák alapja.

### Tulajdonosi döntést kívánó pontok

1. **Egységes módosítórendszer** (F első sora) – a játékmenet nem változik, a modolás bővül. Javaslom.
2. **Többszintű kutatások** – a mai I./II./III. kutatások összevonása szintekre (a számok változatlanok); a
   felületen egy gomb szintjelzővel. Javaslom.
3. **Viselkedés-szabályok passzív képességként** – a kutatás/ág képességet ad, ami a hős- és egységpanelen is
   látszik (ma láthatatlan szabály). Javaslom.
