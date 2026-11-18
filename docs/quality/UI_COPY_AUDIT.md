# Felületi szövegek auditja

Kelt: 2026-11-18. Forrás: `engine/content/base/locale/hu.po` és `en.po` (2256 kulcs; ebből `ui.*`/`hud.*` 285), a
C++-ban rögzített szövegek (`main.cpp` `stderr`-üzenetei – nem a játékosnak szólnak).

## Összkép

- Nincs „TODO”, „debug”, „ideiglenes” a játékosnak látható szövegekben.
- A leghosszabb HUD-szöveg a morál tooltipje (171 karakter) – tooltipben ez helyénvaló (részletező szint).
- A párbeszédek, a fajleírások és a küldetésszövegek **világépítés**, nem rövidítendők.
- Angol: 980 tartalmi kulcs hiányzik (kutatás, képesség, egység, épület, faj) → a teljes angol fordítás a végén
  (tulajdonosi döntés); addig a magyar a tartalék. A `ui.*`/`hud.*` kulcsok angolja teljes.

## Javaslatok

| ID | Kulcs | Hol | Most | Javaslat | Indok | Státusz |
|---|---|---|---|---|---|---|
| C-01 | – | ablakcím | „TOTU: Sky Rift – fa … · arany … · létszám …” | „TOTU: Sky Rift” | ideiglenes kijelzés, a HUD már mutatja (AS-04) | JAVÍTVA |
| C-02 | `ui.settings.ui_scale` | Beállítások | „Felület mérete” | „Menü mérete”, vagy a HUD is kövesse | csak a menüre hat (AS-07) | DÖNTÉSRE VÁR |
| C-03 | (új) `hud.err.*` | HUD | – (csak villanás) | „Kevés arany”, „Kevés fa”, „Kevés meteorit”, „Nincs elég hely a seregben”, „Ide nem építhető” | a sikertelen parancs oka (AS-03) | JAVÍTVA: `hud.err.res` „Nincs elég: {}”, `hud.err.supply`; hiányzó feltételnél a tooltip „Hiányzik: …” szövege |
| C-04 | `ui.settings.sound` | Beállítások | „Hangerő és hangkategóriák” → gomb: „Hangbeállítások” | marad | a sor és a gomb szerepe más (címke ↔ művelet) | NEM HIBA |
| C-05 | `ui.main.*.hint` | főmenü | „hamarosan” jelölés | marad | valós állapot (nincs még képernyő) | NEM HIBA |
| C-06 | `chain.*` | küldetéssor | munkanevek („Küldetéssor”, „Választós próba-küldetéssor”) | a tulajdonos adja | D47 | DÖNTÉSRE VÁR |
| C-07 | `ui.opt.victory.*`, `ui.opt.tempo.*` | Csata | munkanevek | a tulajdonos adja | D20 | DÖNTÉSRE VÁR |

## Szabályok (összefoglaló)

1. Elsődleges HUD: ikon, szám, rövid név. 2. Tooltip: a teljes hatás, költség, hatótáv, töltődés. 3. Részletező nézet
(küldetésnapló, beállítások, szerkesztő): a hosszú szöveg jogos. A belső kulcs nem változik a felirat miatt.
