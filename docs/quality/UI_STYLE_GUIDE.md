# Felületi stílusútmutató

Kelt: 2026-11-18. A [HUD.md](../HUD.md) és a [HUD_TERV.md](../HUD_TERV.md) döntései **érvényesek**; ez a lap csak
összefoglalja és a működő kódból vett értékekkel kiegészíti őket. Ütközésnél a HUD.md nyer.

## 1. Identitás

Festett, 2D sprite-os, 3/4 nézetű fantasy RTS a 90-es évek klasszikusai nyomán. A felület praktikus és olvasható, de
**nem** lapos, nem „dashboard”, nincs neon és színátmenetes sablon. A fajok keretanyaga és díszítése érték.

## 2. Képernyőrétegek (alulról felfelé)

világ → világjelzők (kijelölés, célpont, építési hely) → HUD (felső sáv, küldetéslista, alsó sáv) → tooltip →
párbeszéd / átvezető-sáv → szünetmenü / küldetéssor-választó (teljes fedés) → menü. A menü külön ciklusban fut.

## 3. Komponensek (ténylegesen meglévők)

| Komponens | Hol | Grafika |
|---|---|---|
| Gomb (menü) | `menu_view.cpp` | `button_small.png/.json` (9-szelet) |
| Parancsmező | `hud.cpp` | `cmd_button.png/.json`, ikon `ui/icons/*`, betű a jobb alsó sarokban |
| Tárgymező | `hud.cpp` | `item_frame_<ritkaság>.png` |
| Kijelölésmező | `hud.cpp` | portré + csapatszín-téglalap (nincs maszk), életerő-csík, csoportszám („2×”) |
| Lenyíló | `menu_view.cpp` | `dropdown_header/.json`, `dropdown_item`, `dropdown_arrow` |
| Jelölőnégyzet | `menu_view.cpp` | `checkbox_checked/unchecked/disabled` |
| Keret (szekció) | `hud.cpp` | `frame_<faj>.png/.json` (9-szelet, fajonként) |
| Nyersanyagszámláló | `hud.cpp` | `ui/res/*`, a létszám ikonja `supply_<faj>` |
| Párbeszédablak | `hud.cpp` | `dialog.png/.json` |
| Tooltip | `hud.cpp` `draw_tooltip` | sötét, félig áttetsző panel |
| Állapotüzenet | – | **nincs** (AS-03) |

## 4. Állapotok

normál · fölötte (hover) · lenyomva (villanás 0,18 s) · kijelölt · tiltott (szürke) · töltődés (`cooldown.png`
körív) · kevés nyersanyag (a hiányzó nyersanyag számlálója villan). Új állapotot csak a meglévő grafikával vagy
specifikált új képpel szabad bevezetni.

## 5. Méretek (a `hud_layout` szerint, 1080 képpont magas referencián, `s` = arány)

| Érték | Képlet | Megjegyzés |
|---|---|---|
| belső margó (`pad`) | max(4, 8·s) | |
| szekcióköz (`gap`) | max(6, 11·s) | |
| szekciókeret (`fp`) | max(3, 5·s) | |
| cellaköz (`cg`) | max(2, 4·s) | |
| cella | (214·s − 2·pad − 2·fp − 2·cg) / 3 | parancs-, kijelölés-, tárgycella egyforma |
| alsó sáv magassága | ≤ 0,33 × képernyőmagasság | |
| betűk | név 19 · kicsi 14 · apró 12 · betűjel 13 · nyersanyag 16 (·s) | |

Új képernyőn ezekből kell kiindulni; új varázsszámot csak indoklással.

## 6. Szöveg és nyelv

Magyar elsődleges, angol tartalék (hiányzó angol kulcsnál a magyar látszik). Ékezetek kötelezők; a betűkészlet a
meglévő (jogtiszta) font. A belső azonosító nem változik a felirat miatt.

## 7. Fajonkénti téma

A `Hud::theme()` öt színe (alap, keret világos/sötét, kiemelés, kitöltés) fajonként; a csapatszín **nem** azonos a
faj színével. A közös: geometria, sorrend, állapotlogika.

## 8. Ikonok

Egy ikon = egy jelentés. A betűjel mindig látszik. Az új ikonigény specifikációként megy (méret, alfa, sziluett,
fajstílus, hely, tartalék); kép nem készül engedély nélkül. Leltár: [UI_ICON_INVENTORY.md](UI_ICON_INVENTORY.md).

## 9. Hozzáférhetőség

Színvak-barát csapatszínek (beállítás). Fontos állapot ne csak színnel jelenjen meg (pl. a kevés nyersanyag villanása
mellé szöveg – AS-03). Szöveg játéktéren árnyékkal (`stext`) vagy sötét panelen.

## 10. Képpróbák

1280×720, 1920×1080, 2560×1440; a hét faj HUD-ja (`--world <pálya> --faction <faj> --build-demo`); menü
(`--menu-screen skirmish|settings|sound|keys`). Referenciakép nem frissül automatikusan.
