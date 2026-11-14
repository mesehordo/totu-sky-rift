# Hangok és zene – előkészítés

*© Mesehordo – TOTU: Sky Rift. A játék a fájlokat magától megtalálja; ami hiányzik, ott csend van.*

## Zene – `assets/music/`

- **`menuzene.mp3`** – indításkor és a menükben szól, ismétlődve.
- **Bármennyi további `.mp3`** – csata indulásakor a játék véletlenszerűen választ egyet (a menüzene kivételével); ha véget ér, másikat (nem ugyanazt).
- Új szám: elég bemásolni a mappába. Hangerő: Beállítások → Zene hangereje.

## Hanghatások – `assets/sounds/`

Formátum: `.ogg` (ajánlott), `.wav` vagy `.mp3`; mono vagy sztereó, 44,1 kHz, rövid (0,3–2 mp), csúcson -3 dB, elején csend nélkül. Több változat: `select_1.ogg`, `select_2.ogg`, … – a játék véletlenül váltogat.

**Keresési sorrend** egy egység eseményéhez (az első létező nyer):

1. `units/<egység-azonosító>/<esemény>` – egyedi hang (hősöknek, különleges egységeknek)
2. `factions/<nép>/<esemény>` – a nép közös hangja (ezzel a legtöbb egység azonnal megszólal)
3. `common/<egységtípus>/<esemény>` – típus szerint (pl. minden lövész íjhúrja)
4. `common/<esemény>`

| Esemény | Mikor szól |
|---|---|
| `select` | saját egység kijelölésekor |
| `move` | mozgásparancsra |
| `attack_order` | támadásparancsra |
| `attack` | csapás / lövés (képen látható egységnél) |
| `death` | halálkor |
| `trained` | egység elkészült |
| `building_done` | épület elkészült |
| `under_attack` | riasztás: a képen kívül támadják az egységeidet / épületeidet (legfeljebb 12 mp-enként) |

## Javasolt első kör (ezzel minden megszólal)

| Fájl | Mit |
|---|---|
| `common/under_attack` | rövid riasztó kürt / dob |
| `common/building_done` | kalapácsütés + elégedett „kész” csengés |
| `common/kozelharcos/attack_1..3` | kard / buzogány csapás |
| `common/landzsas/attack_1..3` | dárdaszúrás |
| `common/lovesz/attack_1..3` | íj- vagy parittyahang |
| `common/varazslo/attack_1..3` | varázslövés |
| `common/lovas/attack_1..3` | csapás + pata |
| `common/orias/attack_1..3` | nehéz, tompa ütés |
| `common/ostrom/attack_1..3` | ostromgép lövés, becsapódás |
| `common/repulo/attack_1..3` | szárnysuhogás + csapás |
| `common/munkas/attack_1..2` | gyenge ütés |
| `common/death_1..3` | általános elhalás |

## Népenkénti hangok – `factions/<nép>/`

Népenként: `select_1..4`, `move_1..3`, `attack_order_1..3`, `death_1..2`, `trained_1..2`. A szöveg magyar, rövid (1–4 szó), a nép jellegével; vagy szavak nélküli hang (morgás, csiripelés, gépzúgás).

### Szürke elfek – `factions/elves/`

- **Hangkarakter:** nyugodt, kimért, ünnepélyes elf hang; napra, kőre utaló szavak.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Szelai (`elf_hos_szelai`), Itzcoatl (`elf_hos_itzcoatl`), Xochi (`elf_hos_xochi`)

### Ursanok – `factions/ursan/`

- **Hangkarakter:** mély, dörmögő medvehang, barátságos, de harcos.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Ragna Kőváll (`urs_hos_ragna`), Hjalmar Fagyjós (`urs_hos_hjalmar`), Bjarka, a Vadak Ura (`urs_hos_bjarka`)

### Croftianok – `factions/croftian/`

- **Hangkarakter:** szavak nélküli: csattogás, sziszegés, kitinzörgés, kórusos zúgás.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Keth (`cro_hos_keth`), Vor-Uthul (`cro_hos_vor`), Ssarha, a Raj Hangja (`cro_hos_ssarha`)

### Wullof – `factions/wullof/`

- **Hangkarakter:** élénk, csaholó, vidám dingóhang, útra, falkára utaló szavak.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Tava (`wul_hos_tava`), Varnu, a Holdsámán (`wul_hos_varnu`), Kurrin Harcfőnök (`wul_hos_kurrin`)

### Aurethi Birodalom – `factions/imperium/`

- **Hangkarakter:** fémes, visszhangos emberi hang a gépből, latinos katonai fegyelem; gőzszelep és zúgás.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Livia Varr (`imp_hos_livia`), Nera (`imp_hos_nera`), Cassius Ferrum legátus (`imp_hos_cassius`)

### Huk’ucha – `factions/hukucha/`

- **Hangkarakter:** brekegő, kedves békahang, vízre, tóra utaló szavak; csobbanás.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Kvarra, a Tóanya (`huk_hos_kvarra`), Bul’gok Kampókirály (`huk_hos_bulgok`), Iriss Szivárványgyöngy (`huk_hos_iriss`)

### Ooni – `factions/ooni/`

- **Hangkarakter:** mély oni-hörgés, taiko-ütés; a tenguknál szárnysuhogás, a nianoknál morgás.
- **Hősök egyedi hangja** (`units/<id>/select_*`, `move_*`, `attack_order_*`): Akahana, a Vörös Buzogány (`oon_hos_akahana`), Shiro, a Kapuőr (`oon_hos_shiro`), Karasu, a tengu bölcs (`oon_hos_karasu`)

