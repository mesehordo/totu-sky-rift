# Egyéni csata, pályák, LAN-többjátékos, nyelv

*© 2026 Mesehordo – lásd [LICENSE.md](../LICENSE.md).*

## Egyéni csata (főmenü → Egyéni csata)

A beállítóképernyő a régi RTS-ek (Warcraft II) játékindító képernyőjének felépítését követi:

* **Pályalista** bal oldalt (görgethető), alatta a kiválasztott pálya minitérképe számozott kezdőhelyekkel,
  a játékosszám, a méret, a táj és a leírás.
* **8 játékoshely** lenyíló menükkel: *Résztvevő* (Játékos / Nyitott (LAN) / Zárt / Gép – könnyű, közepes, nehéz),
  *Faj* (vagy Véletlen), *Csapat* (–, 1–4), *Szín* (8 szín), *Kezdőhely* (Véletlen vagy a minitérkép száma).
* **Játékbeállítások:** Játéktípus (mindenki mindenki ellen / csapatok / fent kontra lent), Nyersanyag (kevés /
  közepes / sok), Szörnytáborok (be/ki), Sebesség, Táj (pálya szerint / erdő / tél / pusztaság / mocsár),
  Köd (normál / felderített térkép / nincs köd).
* A gépi ellenfél nehézsége helyenként állítható: a könnyű később támad és lassabban dönt, a nehéz hamarabb
  támad, gyorsabban dönt és nyersanyag-előnnyel indul.
* Csapatjátékban a szövetségesek látása közös, és a győzelem csapatonként számít.

## A 20 pálya (`data/maps/*.json`, forrás: `tools/make_maps.py`)

| Pálya | Játékos | Méret | Jellege |
|---|---|---|---|
| Két part | 2 | 72 | folyó három gázlóval |
| Hasadék | 2 | 80 | sziklahasadék két hágóval |
| Kráterföld | 2 | 88 | gazdag középső meteorkráter |
| Ősi erdő | 2 | 96 | sűrű rengeteg, ösvények |
| Jéghíd | 2 | 96 | befagyott tó, szárazföldi hidak (tél) |
| Hármas folyó | 3 | 104 | három folyó, szektoronként két gázló |
| Háromkő | 3 | 96 | sziklafalak, kráter a háromszög közepén |
| Négy sarok | 4 | 112 | klasszikus négyes, középső kráter |
| Keresztút | 4 | 104 | négy tó között kereszt alakú utak |
| Gyűrűhegy | 4 | 120 | sziklagyűrű négy kapuval (tél) |
| Tavak vidéke | 4 | 112 | sok kis tó (mocsár) |
| Fent és lent | 4 | 112 | csapatpálya 2–2, folyó középen |
| Csillag | 5 | 128 | öt sziklakar, öt völgy |
| Hatszög | 6 | 128 | hat bázis körben, patakok |
| Mocsárvidék | 6 | 128 | csapatpálya 3–3 |
| Hét Út | 7 | 136 | hét út a középső kráterhez |
| Nyolc tűz | 8 | 144 | a legnagyobb mindenki-mindenki ellen |
| Csatamező | 8 | 144 | csapatpálya 4–4 |
| Az Égszakadás krátere | 8 | 152 | óriáskráter sziklafallal, nyolc kapuval |
| Labirintus | 4 | 120 | sziklafalak útvesztője (tél) |

A pályák szimmetrikusak (körben forgatott vagy tükrözött szektorok), így minden kezdőhely egyformán jó. Az erdők
összefüggő, zajból rajzolt tömbök, a talaj nagy, festett szélű foltokból áll. **Saját pálya:** új JSON a
`data/maps/` (vagy `mods/<mod>/maps/`) mappában – a mezők: `players`, `size`, `layout` (`ring` / `corners` / `rows`),
`theme`, `name`/`desc` (`hu`, `en`), `features` (`river`, `ring_wall`, `crater`, `lakes`, `ridges`,
`forest_cover`, `dirt`, `expansions`, `start_angle`).

## LAN-többjátékos (főmenü → Többjátékos (LAN))

* **Játék létrehozása:** a gazda lesz az 1. játékos; a beállítóképernyőn a helyeket *Nyitott*-ra állítja, a
  csatlakozók ezeket foglalják el. Csevegés, KÉSZ-jelzés; a gazda indít, ha mindenki kész.
* **Csatlakozás:** *Játékok keresése a helyi hálózaton* (UDP-felfedezés), vagy a gazda IP-címével.
  Portok: TCP 47810, UDP 47811 (a tűzfalon engedélyezni kell).
* **Működés:** minden gép ugyanazt a determinisztikus szimulációt futtatja; csak a játékosok parancsai utaznak,
  sorszámozott körökben (lockstep: 1 kör = 100 ms, a parancs a 2. körben hajtódik végre). A gépek időnként
  összevetik az állapotukat; eltérésnél „Szinkronhiba” jelenik meg. Kilépő játékost a gépi ellenfél veszi át.
* Hálózati játékban nincs mentés és szünet; az Esc kétszer megnyomva lép ki.

## Nyelv

Beállítások → **Nyelv: Magyar / English**. A fordítás a kiírás pontján történik (`rts/i18n.py`): a felület
szövegei, a játékadatok (egységek, épületek, képességek, fejlesztések, tárgyak, fajok), a pályák és a teljes
hadjárat angolul is megvannak. Forrás: `tools/lang_en.py`, `tools/lang_en_data.py`, `tools/lang_en_campaign.py`
→ `python tools/lang_en.py` → `data/lang/en.json`.
