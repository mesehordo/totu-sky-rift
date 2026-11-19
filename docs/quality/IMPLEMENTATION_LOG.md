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

## 6. A régi jobb felső sarok és a súgó (a régi játékkal összevetve)

A régi `hud.py` `draw_clock` és `ui.py` `draw_help` szerint: jobb felül az óra, mellette [−] ×1 [+] gomb, alatta
apró betűvel „N fps  •  F1: súgó”; a játéksebesség a régi lépcsőkben (50, 75, 100, 125, 150, 200, 300 %) a gombbal és
a + / − billentyűvel – **csak egyjátékos módban**, hálózatban a gombok szürkék (a lockstep üteme kötött); a futtató
ütemét és a rajzolás simítását állítja, a szimulációt nem. F1: a régi súgó panel (a felső sáv alatt, középen, arany
keret), a sorok a mostani irányításhoz igazítva (P + klikk: járőr, Alt+1..6: alakzat, Shift-sor). **Próba:**
`HUD jatekebesseg`. **Kép:** `kepek/regi_jatek_hud_ursan.jpg` (a régi játék) ↔ `kepek/utan_sugo_es_sebesseg.jpg`.

## 7. Hősportrék, F2–F4, Szóköz, riasztás, középső gomb, kezdőkamera (a régi játékkal összevetve)

A régi `ui.py` szerint: **hősportrék** jobb felül, az fps-sor alatt egymás alatt (legfeljebb 4): arany keret (kijelölve
világosabb), alul életerő- és energiacsík, jobb felül szintjelvény, bal felül pulzáló „+” elkölthető képességpontnál;
kattintás kijelöl, dupla kattintás odaugrik. **F2/F3/F4:** az 1./2./3. hős, újra lenyomva odaugrás. **Szóköz:** az
utolsó riasztás helye (10 mp-en belül), különben a bázis (főépület). **„Támadás ér!”** piros sor az üzenetsorban, ha
egy saját kész egység vagy épület életereje csökken (12 mp-enként legfeljebb egy – a régi szabály; a pillanatképből,
a szimuláció nem változik). **Középső gomb húzása:** kamera. **Kezdőkamera:** a bázisra (a régi `start_xy`).
A súgó új sora ezekről. **Próba:** `HUD a regi jatek szerint: F2 hos …`. **Kép:** `kepek/utan_hosportre.jpg`.

**Eltérés a régitől (a tulajdonos döntése, 2026-11-18):** a P billentyű **járőr marad** (P + kattintás, D46); a régi
játékban szünet volt – a szünet az Esc-kel (szünetmenü) érhető el.

## 8. Összeomlás, köd, szünetmenü

**Összeomlás:** a HUD nyers mutatót tartott a pillanatképre; két tick egy képkocka alatt felszabadította (indulás,
akadás, mérőmód) – most megtartja (`World::snapshot_shared`).
**Köd (a tulajdonos kérése):** a látható – felderített – felfedezetlen rész határa szélesebb (a mintatávolság 0,75 →
1,5 csempe: kb. 3 helyett kb. 6 csempés átmenet). **Elmosott köd** (a szürke helyett) kipróbálva: `--fog-blur`;
a felderített rész színes marad, elmosva és kissé sötétebben. **Költség** (RX 550, 1920×1080, `--bench`, 600
képkocka, felváltva 3–3 futás): szürke p50 49,3 / 48,7 / 49,7 ms, elmosott 50,3 / 49,3 / 49,3 ms – a különbség a
mérési zajon belül (≤ 1 ms). Az alap (a tulajdonos döntése, 2026-11-19): az elmosott (D41 kiegészítés); a szürke: `--fog-gray`. **Kép:**
`kepek/kod_osszevetes_A_szurke_B_szeles_C_elmosott.jpg`.
**Megjegyzés:** a rejtett ablakos mérőmód 1080p-ben kb. 50 ms/képkocka – ezt külön meg kell vizsgálni (NYITOTT).
**Szünetmenü (a tulajdonos kérése):** bal oldali panel a főmenü oldalsávjának stílusában (átmenetes sötét háttér,
arany szegély, díszvonalak), balról becsúszva (0,22 mp, lassuló), a játéktér elmosódik mögötte (a fényezőben, ugyanazzal
az elmosással), a HUD elsötétül; Esc/Enter folytatás, Q kilépés, kattintás a menüpontra, a panelen kívül folytatás.
**Kép:** `kepek/utan_szunetmenu.jpg`.

## 9. Menük oldalt (a tulajdonos kérése)

A régi játékban a főmenü almenüje az oldalsáv mögül csúszott ki, a Beállítások középen, teljes képernyőn nyílt. Most a
**Beállítások, a Hang és a Billentyűk** az oldalsáv mellett nyílnak (a „Beállítások” pont kiemelve), balról
becsúszva (0,14 mp, lassuló), a főmenü hátterén; az oldalsáv pontjaira kattintva vissza a főmenübe, nyitott
almenüvel. A főmenü almenüje is a régi módon csúszik ki. A Csata képernyő elrendezését a tulajdonos adja (változatlan).
Szöveghiba javítva: a billentyűkiosztás „Foglalt: P (szünet)” → „P (járőr)”. **Képek:** `kepek/utan_menu_*.jpg`.

## 10. A meccs utáni összesítő (a régi `draw_summary` szerint; a tulajdonos jóváhagyásával)

Eddig a meccs végén csak egy „Győzelem / Vereség” sáv látszott. Most a régi összesítő: cím, eredmény és játékidő,
négy fül – **Áttekintés** (pontszám, katonai, gazdasági, építkezés, eredmény; a régi `_score` képlete),
**Gazdaság** (gyűjtött nyersanyagonként, összes, elköltött), **Katonaság** (kiképzett, megölt, elesett, épített,
lerombolt, elvesztett épület), **Grafikon** (a sereg értéke és az összes gyűjtött nyersanyag az idő függvényében,
15 mp-es mintákból) –, játékosonként egy sor, az oszlop legjobb értéke kiemelve; „Főmenü” gomb.
**Szimuláció:** új `PlayerStats` a játékosállapotban (kiképzés, ölés ellenséges gyilkostól, veszteség, kész épület,
költés a visszatérítéssel csökkentve, idősor). Csak kijelzés: nem része az állapothashnek (a goldenek
változatlanok); a mentés 20. sémája viszi, a 19-es mentés betölthető (üres számlálókkal). Az idősort a pillanatkép
csak a meccs végén másolja. **Próba:** `Osszesito` (ölés, veszteség, épület, idősor, mentés–betöltés).
**Képek:** `kepek/utan_osszesito_attekintes.jpg`, `kepek/utan_osszesito_grafikon.jpg` (próbameccs, a vége
kényszerítve).

## 11. „Nem lehet építeni”, „Elpusztult” (a régi játék szerint; az AS-03 nyitott pontjai)

**Építési hely:** a kliens minden képkockán elküldi a kurzor alatti helyet (sablon, csempe) a futtatónak; a
pillanatkép a szimuláció `can_place` válaszát hozza (a helyi játékos szemszögéből). A kísértetkép zöld, tiltott
helyen piros (a régi kurzor „nem”-je helyett); kattintásra az üzenetsorban „Nem lehet építeni: <ok>” (a régi
szöveg), parancs nem megy ki, az építési mód marad. Az okok (térképen kívül, terep, szirt, foglalt, lelőhely, a
népek külön szabályai) fordítási kulccsal, angolul is. Ismert korlát: a „foglalt” a ködben álló, nem látott
épületre is igaz (a régi játékban is így volt).
**Elpusztult:** ha a helyi játékos sérült épülete eltűnik a pillanatképből, „Elpusztult: <név>” (a régi `notify`
szövege és színe); a lebontott (ép) épület csendben, az elfoglalt (más tulajdonossal ott maradó) nem számít.
Csak kliens: a szimuláció, a hash és a mentés változatlan. **Próba:** `HUD` – „Nem lehet epiteni …, Elpusztult
epulet”.

## Döntésre vár

AS-07
(„Felület mérete”: csak menü, vagy a HUD is), C-06/C-07 (munkanevek), a diagnosztika exportjának helye és a
véleménymező.
