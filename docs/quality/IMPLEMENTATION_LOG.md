# Megvalósítási napló (anti-slop csomag)

Formátum: probléma → bizonyíték → legkisebb javítás → nem változó szerződés → fájlok → teszt → előtte/utána →
visszaállítás.

## 1. P0–P1: alapállapot, audit, leltárak (2026-11-18)

Dokumentumok: `00_BASELINE_AND_RISKS`, `ANTI_SLOP_AUDIT`, `UI_STYLE_GUIDE`, `UI_COPY_AUDIT`, `UI_ICON_INVENTORY`,
`DIAGNOSTICS_DESIGN`, `REGRESSION_MATRIX`; képek `kepek/alap_*`. Kód nem változott. Visszaállítás: a commit revertje.

## 2. P2: kliensoldali kis javítások (2026-11-18)

| ID | Javítás | Fájlok | Nem változik |
|---|---|---|---|
| AS-01, AS-05 | a görgetés a `scroll_speed` szerint, eltelt idővel (képkockaszámtól független); képernyőszéli görgetés a beállítás szerint (képpróbában, mérésnél, rejtett ablakban és fókusz nélkül nincs); FPS-kijelzés az óra mellett; képkocka-korlát a képcsere után | `settings.hpp/.cpp` (`scroll_delta`), `main.cpp`, `hud.hpp/.cpp` | szimuláció, hash, mentés, HUD-geometria, gyorsbillentyűk |
| AS-02 | a meccs indulásakor a kamera a helyi játékos első egységére áll csatában is (eddig csak küldetésben); megfigyelőnél marad középen | `main.cpp` (`center_on_local`), `session.hpp` (`has_sim`) | szimuláció |
| AS-04 | az ideiglenes ablakcím-kijelzés törölve | `main.cpp` | – |
| AS-08 | a `--faction` a parancssori `--world` meccsre is hat (képpróba); a menüs indítás változatlan | `main.cpp` | menü, hálózat |

**Teszt:** új `AS-01 kamera-gorgetes` próba (időarányosság, két fél képkocka = egy egész, szél csak bekapcsolva és
ablakban); CTest 9/9, goldenek bitre azonosak. **Képek:** `kepek/alap_hud_ursan_1280.jpg` (fekete, középen) →
`kepek/utan_hud_<faj>_1280.jpg` (a saját bázis, mind a hét faj HUD-ja, FPS az óra mellett).
**Teljesítmény:** a változás képkockánként néhány összeadás; NOT_MEASURED. Visszaállítás: a commit revertje.

## 3. Dokumentum-frissítés (AS-06, AS-11, AS-12)

`docs/fejlesztes/ALLAPOT.md` (M0–M12), `HIBAK_ES_FELADATOK.md` (a régi játékra vonatkozik – jelölve), `README.md`
(állapottábla, CI-állítás, játékmódok), `docs/HUD.md` (a geometria helye).

## 4. AS-03: a sikertelen kiadás oka szövegesen (a tulajdonos jóváhagyásával, 2026-11-18)

**Probléma:** kiképzés, építés, kutatás kattintásra csak a nyersanyagszámláló villant. **Javítás:** a HUD-modell
(`activate_slot`) a parancs kiadása előtt, ugyanabból az adatból, amiből a mező tiltott, rövid okot ír
(`notice`): „Nincs elég: Fa, Arany” (a hiányzó nyersanyagok a faj nyersanyagnevével), „A létszám betelt – több
lakóhely kell”, hiányzó feltételnél a tooltip „Hiányzik: …” szövege. A HUD 2 mp-ig mutatja az alsó sáv fölött,
középen (`draw_notice`), **a régi játék szerint** (a tulajdonos kérése, `rts/ui.py` `draw_hud`): bal oldalt, az alsó sáv fölött, kis
betűvel, soronként félig áttetsző fekete sávon, narancsszínnel, legfeljebb 6 sor, 8 mp-ig; a szövegek is a régiek
(„Nincs elég nyersanyag: …”, „Elérted a létszámkorlátot – növeld: <a faj létszámforrása>”). **Nem változik:** a parancs útja, a szimuláció, a hash, a
billentyű–kattintás azonosság. **Nyitott:** „Ide nem építhető” – az elhelyezés ellenőrzése a szimuláció szálán fut,
a HUD onnan nem olvashat; ehhez a pillanatképbe kell a foglaltság (külön lépés); az „érvénytelen cél”,
„elérhetetlen” okhoz szimulációs visszajelzés kell (döntés). **Teszt:** `AS-03 HUD` próba (hiányzó nyersanyag,
betelt létszám, rendben: nincs üzenet); a 8 HUD-próba zöld. **Kép:** `kepek/utan_as03_uzenet.jpg`.

## 5. Karakterlap: a Támadás sora (a régi játékkal összevetve)

**Hiba:** a pillanatkép a sebzést a bemutató (M1) `Attack` komponenséből olvasta; a sablonból indított egységeknek
ilyen nincs, ezért a karakterlapon **egyetlen egységnél sem** látszott a Támadás. **Javítás:** a sablon támadása a
kutatásokkal (`Session::hud_fields`). Most a régi elrendezés: Támadás/Páncél, Mozgás/Látótáv két oszlopban.
**Próba:** `HUD karakterlap: a munkas tamadasa`.

## Döntésre vár

AS-07
(„Felület mérete”: csak menü, vagy a HUD is), C-06/C-07 (munkanevek), a diagnosztika exportjának helye és a
véleménymező.
