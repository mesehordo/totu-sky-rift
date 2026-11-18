# Anti-slop audit

Kelt: 2026-11-18 · alap: HEAD `c840c29` · bizonyítékok: [kepek/](kepek/), forráshivatkozások. Rangsor: játékosi haszon és
regressziós kockázat szerint. Státusz: NYITOTT / JAVÍTVA (commit) / DÖNTÉSRE VÁR / TERVEZETT.

Mezők: **képernyő · hely · reprodukció · kár · bizonyíték · legkisebb megoldás · kockázat · teszt · döntés**.

## Rangsor

| ID | Kat. | Képernyő | Hiba | Kockázat | Státusz |
|---|---|---|---|---|---|
| AS-01 | interakció | Beállítások → játék | 4 kapcsoló nem hat a játékra (görgetés sebessége, képernyőszéli görgetés, FPS-kijelzés, képkocka-korlát) | alacsony (csak kliens) | JAVÍTVA |
| AS-02 | hierarchia | Csata indulása | a kamera a pálya közepén, ködben indul: fekete kép a saját bázis helyett | alacsony | JAVÍTVA |
| AS-03 | interakció | HUD | a sikertelen parancs csak villan (nyersanyag), szöveges ok nincs („Kevés arany”, „Nincs hely”) | közepes | JAVÍTVA (nyersanyag, létszám, hiányzó feltétel); az elhelyezés és a szimulációs okok NYITOTT |
| AS-04 | technikai | ablakcím | „ideiglenes kijelzés” a címsorban (nyersanyag, létszám) – a HUD már mutatja | alacsony | JAVÍTVA |
| AS-05 | technikai | kamera | a billentyűs görgetés képkockánként 12 px: a sebesség a képkockaszámtól függ | alacsony | JAVÍTVA (AS-01-gyel) |
| AS-06 | szöveg | dokumentáció | az állapotlapok az M2-nél és a Python-játéknál tartanak | nincs | JAVÍTVA |
| AS-07 | szöveg | Beállítások | „Felület mérete” csak a menüre hat, a játékbeli HUD-ra nem | közepes (HUD-geometria) | DÖNTÉSRE VÁR |
| AS-08 | teszt | képpróba | a `--world` meccs mindig ursan; a hét faj HUD-ja parancssorból nem fényképezhető | nincs | JAVÍTVA |
| AS-09 | lokalizáció | minden | az angol fordításból 980 tartalmi kulcs hiányzik (kutatás 271, képesség 232, egység 222, épület 167) | nincs | TERVEZETT (a végén, emlékeztető szerint) |
| AS-11 | szöveg | README | „mindegyiket a CI ellenőrzi” – a két BSD még nincs a CI-ben | nincs | JAVÍTVA (a szöveg) |
| AS-12 | szöveg | `docs/HUD.md` | a geometria helyeként a régi `rts/hud.py` | nincs | JAVÍTVA |
| AS-10 | napló | hibák | a hibák csak a `stderr`-re mennek; nincs naplófájl és diagnosztikai export | – | TERVEZETT (DIAGNOSTICS_DESIGN) |

## Részletek

### AS-01 – Nem működő beállítások
- **Hely:** `engine/src/client/settings.hpp` (`scroll_speed`, `edge_scroll`, `show_fps`, `fps_limit`), `menu.cpp:521–526`;
  a `main.cpp` csak a `language`, `width/height`, `fullscreen`, `colorblind` mezőt olvassa.
- **Reprodukció:** Beállítások → Görgetés sebessége 300 vagy 1500, Képernyőszéli görgetés Be, FPS-kijelzés Be → Csata:
  a görgetés ugyanolyan, a képernyő szélén nem görget, FPS nem látszik, a korlát mindig a függőleges szinkron.
- **Kár:** a játékos átállít valamit, és nem történik semmi – a beállítások hitelét rontja; képernyőszéli görgetés nélkül
  klasszikus RTS-ben sok játékos nem találja a kamerát.
- **Legkisebb megoldás:** a kamera a `scroll_speed` szerint, eltelt idővel (px/s) görget; `edge_scroll` esetén a szélső
  8 px-en is; `show_fps` a jobb felső sarokban az óra alatt; `fps_limit` a képkockák közti várakozással (a függőleges
  szinkron mellett). Csak kliens, a szimuláció nem érintett.
- **Teszt:** egységpróba a görgetés-számításra (sebesség × idő, szél), golden változatlan, kép előtte–utána.

### AS-02 – Fekete kezdőkép
- **Hely:** `main.cpp` 700–706: a kamera a pálya közepére áll; a `center_on_local` csak küldetésben fut.
- **Bizonyíték:** `kepek/alap_hud_ursan_1280.jpg` – a játéktér fekete, a minitérképen a bázis a bal felső sarokban.
- **Megoldás:** a meccs indulásakor a kamera a helyi játékos első egységére (vagy főépületére) áll – ugyanaz a függvény.

### AS-03 – Néma elutasítás
- **Hely:** `hud_model.cpp:487, 947` (`res_flash`), `hud.cpp:970, 1020`.
- **Kár:** kezdő játékos nem tudja, miért nem indult a kiképzés/építés.
- **Javaslat:** rövid, lokalizált üzenetsor a HUD fölött (2 s), csak a kliensoldalon biztosan ismert okokkal (kevés
  nyersanyag – melyik –, létszámkorlát, nem helyezhető el). A szimuláció elutasítási okához (érvénytelen cél,
  elérhetetlen) új parancs-visszajelzés kellene → az **a szimulációt érinti, külön döntés**.

### AS-04 – Ideiglenes ablakcím
- **Hely:** `main.cpp:1345–1354` („ideiglenes kijelzés (a HUD az M6 9. lépésében)”).
- **Megoldás:** törlés; az ablakcím „TOTU: Sky Rift”.

### AS-07 – Felület mérete
- A menü a `ui_scale`-t használja (`menu.cpp:299`), a HUD a saját, felbontásból számolt méretét. A HUD méretezése a
  HUD-kontraktus (`docs/HUD.md`) része → **tulajdonosi döntés**: a beállítás felirata legyen „Menü mérete”, vagy a
  HUD is kövesse.

### AS-08 – Faj-HUD képek
- A `--world` meccs a `{"ursan","ursan"}` párral indul (`main.cpp:682`); a `--faction` csak hálózaton hat.
- **Megoldás:** a `--faction` a `--world` meccsre is hasson (csak képpróbához; a menüs indítást nem érinti).

## Amit az audit nem talált hibának
- A fajonkénti keretek, bannerek és díszítés (identitás, nem zaj).
- A menü „hamarosan” jelölései: valós állapotot mutatnak (a lobbi, a Történetek, a Modok képernyője még nincs).
- A HUD 1280×720, 1920×1080, 2560×1440 felbontáson arányos, nincs levágás és átfedés.
