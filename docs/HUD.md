# TOTU: Sky Rift – kezelőfelület (HUD)

*© Mesehordo*

## Elrendezés

**Alsó sáv (balról jobbra, kötött):**

1. minimap;
2. fajbanner;
3. karakterlap: 1:1 portré, alatta életerő és mana/energia (vagy pajzs), mellette név, típus és adatok;
4. tárgyak 2×3 (nem hősnél: fajcímer);
5. kijelölt egységek 4×3;
6. fajbanner;
7. parancsrács 4×3.

Minden szekció saját, kiemelt keretben van.

**Felül középen:** [szövetségesek] [meteorit | fa | arany | létszám] [fogaskerék = menü].
**Bal felül:** háttér nélküli küldetéslista jelölőnégyzetekkel; 3 cél felett összecsukható.
**Jobb felül, visszafogottan:** játékidő és sebesség (−/+).

A geometria egyetlen függvényben van: `rts/hud.py: hud_layout()`. A HUD 1080 képpont magassághoz skálázódik, keskeny ablakon kicsinyít. A faj csak a kinézetet változtatja (`rts/faction_ui.py`). A rajzolt képek helye:
- `assets/ui/hud_<faj>.png`
- `assets/ui/banner_<faj>.png`
- `assets/ui/res/<nyersanyag>.png`

## Parancsrács: W E R T / S D F G / X C V B

A betű minden ikon jobb alsó sarkában látszik.

| Kijelölés | Kiosztás |
|---|---|
| **Egység** | W mozgás, E állj, R támadás, T helyben tartás. S…B: Építés (munkás), Letelepítés, Tanulás, képességek (előbb az aktívak, utána a passzívak). Ha több akció van, mint hely, a B a következő oldal. |
| **Építés / Tanulás oldal** | Az elemek sorban követik egymást. V a következő oldal, B a vissza. |
| **Épület** | A kiképezhető egységek és a kutatások sorban, utánuk a Felszedés, a Kiürítés és az Irány választása. B: a gyártósor utolsó elemének visszavonása. |

A billentyű és a kattintás ugyanazt a függvényt hívja (`activate_slot`), az pedig a közös parancs-API-t (`UI.cmd`; hálózaton lockstep).

A billentyűket a játék nem hajtja végre szünetben, döntési ablaknál, irányválasztáskor, a játék végén, vagy ha szövegbevitel aktív (`text_focus`). Egy lenyomás egy parancsot jelent: a billentyűismétlés ki van kapcsolva, és a lenyomott billentyűt is figyeli.

## Kijelölés-rács

- **Vegyes sereg:** típusonként összevonva, a darabszám jobb alul (`8×`). A vékony csík az átlagos életerő; tooltipben az átlagos életerő és a sérültek száma látszik.
- **Egyféle típus:** egyenként jelenik meg (felbontva).
- **Sorrend:** stabil, előbb a hősök, utána a faj egységsorrendje.
- **Kattintás:** csak azokat jelöli ki. **Shift + kattintás vagy jobb klikk:** kiveszi őket a kijelölésből.
- **Egy kijelölt épületnél:** az első mező maga az épület, utána a gyártósor (kattintás: visszavonás).
- **Beállítás:** `settings["sel_grid"]`, értéke `auto` (alapérték), `group` vagy `single`.

## Arany és aranybánya

A „kő” nyersanyag neve mindenhol **arany**, a belső kulcs `stone` maradt, a gazdaság változatlan. A lelőhely WC3-szerű **aranybánya**: ácsolt tárnabejárat, csille és aranyerek. Kitermelés közben a munkás „bent van a bányában”. Ez csak megjelenítés, a szimuláció és a LAN-szinkron nem változik.

## Tesztek és képek

- `python tools/test_hud.py`:
  - a panelsorrend és a rácsméretek 1280×720, 1920×1080 és 2560×1440 felbontáson;
  - az 1:1 portré;
  - a billentyű = kattintás paritás mind a 12 helyen, négy fajnál, munkással és főépülettel;
  - az ismétlés, a szövegbevitel, a szünet és a modális ablak tiltása;
  - a kijelölés csoportosítása, a halál utáni frissülés, a hős tárgyai és a nem hős fajcímere.
- `python tools/hud_shots.py`: a képeket a `docs/hud_kepek/` mappába menti. Négy fajnál készül hős, egység, épület, többes kijelölés és Építés oldal, valamint kép mindhárom felbontáson, a kampány küldetéslistájáról és a menüről.

## Ismert hiányok / nyitott pontok

- A **szövetségesek** ikonja most egyszerű listát nyit: név, faj, viszony, kiesett-e. Diplomácia vagy csere nincs a játékban.
- A **gombok nem állíthatók át**, a WERT/SDFG/XCVB rögzített profil. Az adatfájlok régi képesség-betűit (`hotkey`) a játék már nem használja.
- **UI-méretezés (80–140%)** nincs külön beállításként, a HUD a képernyőmagassághoz skálázódik.
- A **hősöknek nincs erő/ügyesség/intelligencia** értéke a játékban. A karakterlap a szintet, a tapasztalatot, a támadást, a páncélt, a mozgást és a látótávot mutatja.
- A **régi építés- és kiképzés-fülek megszűntek**:
  - építeni munkás kijelölésével lehet (S = Építés);
  - kiképezni a termelő épület kijelölésével.
- **Hangvisszajelzés** nincs, a visszajelzés vizuális:
  - ikonvillanás;
  - célzókurzor;
  - üzenetsor;
  - a hiányzó nyersanyag piros jelzése a felső sávon.

## Módosított fájlok

- **Új:** `rts/hud.py`, `tools/test_hud.py`, `tools/hud_shots.py`, `docs/HUD.md`.
- **Módosított:**
  - `rts/ui.py`: a régi oldalsáv helyett a HUD; az input és a tooltip;
  - `rts/faction_ui.py`: a `hud_panel` és a `banner`;
  - `rts/menu.py`: a logó mérete;
  - `tools/make_intro.py`: a 07-es logó;
  - `tools/spritegen/misc.py`: az aranybánya;
  - `tools/make_sprites.py`: a `--resources` kapcsoló;
  - `data/game.json`, `data/factions/ursan.json`;
  - EN szövegek, kampányszöveg, grafikai leírások;
  - `assets/ui/logo*.png`, `assets/ui/bg/*`, `assets/ui/res/stone.png`, `assets/sprites/resources/stone.*`.
