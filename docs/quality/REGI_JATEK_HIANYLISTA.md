# Mit nem tud még az új motor a régi játékhoz képest

Kelt: 2026-11-19. A régi Python-játék (`rts/`, 24 600 sor) minden moduljának függvényeit és képernyőit vetettem
össze az új motorral (`engine/`). A **kampányok és a novellák** a tulajdonos kérésére kimaradtak.

**A szabályok rendben vannak.** A szimuláció szabályait a tartalmi leltár (`docs/alap/TARTALOM_LELTAR.md`) és a
leltár-kapu (`leltar_kapu.json`) tételesen fedi: 396 tétel, mindegyik megvalósított lépésre mutat, és teszt őrzi.
A hiányok a **megjelenítésben, a hangban, a menükben és a kiegészítő rendszerekben** vannak.

Súlyosság: **Nagy** – játék közben feltűnő vagy egy egész rendszer hiányzik; **Közepes** – érezhető, de van
kerülőút; **Kicsi** – kényelmi vagy ritka.

## 1. Hang (Nagy) – **kész (2026-11-20)**

SDL3 hangfolyamokkal; Ogg Vorbis (stb_vorbis), MP3 (dr_mp3) és WAV; a hangerő-beállítások hatnak (`client/audio`, `client/sound_bank`).

| Régi (`rts/audio.py`) | Új |
|---|---|
| menüzene és játékzene, sávváltással | **kész**: a menükben a menüzene ismétlődve, csatában véletlen sorrend (küldetésnél a saját zenéi előbb), felúszással |
| egységhangok: kijelölés, nyugtázás, támadás, halál, népenként | **kész**: a régi keresési sorrenddel (egység, nép, egységosztály, közös), változatokkal; kiképzés, épület kész, képesség, találat |
| „Támadás ér!” riasztóhang (12 mp-es korláttal) | **kész** |
| környezeti hangok | **kész**: a `common/environment` hangjai (most nincs ilyen fájl: csend, mint a régiben) |
| zenei menü (`MusicMenu`): a számok listája és lejátszása | **kész**: a szünetmenü „Zene” pontja |

## 2. Egységek és épületek a pályán

| Tétel | Régi | Új | Súly |
|---|---|---|---|
| Támadás-, varázslás- és munkaanimáció | `unit_anim`: attack, cast, work, gather, construct, repair | **kész (2026-11-20)** | – |
| Varázslathatások | elemenkénti képlapok (`vfx_<elem>_cast/bolt/impact/area/nova`, 13 elem), gyógyítás, szintlépés, gyűrű, por, nova | **kész (2026-11-20)**: varázslás, nova, varázslövedék, becsapódás, szintlépés, halál (vér/robbanás), gyógyítás | – |
| Területi hatások | `draw_area`: a tartós varázsterület (pl. tűzmező) képe | **kész (2026-11-20)** | – |
| Állapotjelek a figurán | `draw_status_fx`: láng, jégkristály, buborék, Z Z Z, csillagok, szikra; pulzáló szín | **kész (2026-11-20)**, a pulzáló színnel (beállítás) | – |
| Croftian fertőzött talaj | `draw_creep` | **kész (2026-11-20)** | – |
| Huk’ucha tavacskák | `draw_ponds`: kerek part, homoksáv, habvonal | **kész (2026-11-20)** | – |
| Micélia spórahálózat | `draw_network`: lüktető szálak az épületek között | **kész (2026-11-20)** | – |
| Lárvák, osztódás jelei | `draw_building_marks` (Kaptár lárvái), `draw_unit_marks` (micélia: pöttyök az osztódásig) | **kész (2026-11-20)**: a lárvák és az osztódás pöttyei | Kicsi |
| Rothadó fa | `draw_rot_bubbles` (Croftian) | **kész (2026-11-20)**: fakulás fokozatonként, buborékok | Kicsi |
| Félkész munkahelyek | `draw_worksites`, `draw_foundations`: ásás, alapozás, lapkő, tüskék haladása | **kész (2026-11-20)**: a lapkő lerakása és a tavacska ásása 4 fázisban; a lerakott alapozás márványlapjai | Közepes |
| Összekötött falak | `_wall_frame`: a falszakaszok a szomszédjuk szerint | **kész (2026-11-20)**: északi / déli szomszédnál a hosszanti kép (`_v`), sarkon mindkettő | Közepes |
| Kép nélküli egységek | `render.draw_shape`: népenként jellemző alakzat (kör, lándzsás, pajzsos …) a csapatszínnel | **kész (2026-11-20)**: a régi alakzatok; a 103 kép nélküli sablonnak (Huk’ucha 21, Ooni 18, a kisnépek egységei) ettől még kép kell | Közepes |
| Képpontpontos kijelölés | `hover_sprite`: a látható képpont dönt | **kész (2026-11-20)**: a kirajzolt képkocka levágott kerete (közelítés) | Kicsi |
| Díszletépületek, háttérszereplők | `draw_decor`, `draw_actor` | **kész (2026-11-20)**: utat záró díszlet; járőröző, menekülő szereplők | Kicsi |

## 3. Értesítések játék közben (Közepes) – **kész (2026-11-20)**

Kész: elesett hős, szövetség, fejlődési irány, felemelkedés, új cél, a képesség még töltődik, játéksebesség; újratestesülés, osztódás, telihold, nincs célpont, az épület tele, elérhetetlen mozgáscél (a szimuláció átmeneti, nem mentett eseményeiből). Nem kell: a „döntés” csak a hadjáratban volt; a bánya munkáshely-korlátja az új motorban nem létezik.

Az új motor csak a hiányzó nyersanyagot, létszámot vagy feltételt jelzi, az építési hely hibáját, az elpusztult
épületet és a támadást. A régi kb. 35 féle üzenetéből ezek hiányoznak:
- elesett hős („újraéleszthető”), újratestesülés, osztódás;
- szövetség megkötése, fejlődési irány, „Felemelkedés! Válaszd ki…”;
- telihold (Wullof);
- új küldetéscél, döntés;
- a képesség még töltődik vagy nincs célpontja;
- az épület tele van, a bánya munkáshelye betelt;
- a mozgás célpontja elérhetetlen;
- a játéksebesség változott.

## 4. Menük és képernyők (a kampány és a novellák nélkül)

| Tétel | Régi | Új | Súly |
|---|---|---|---|
| Mentés és betöltés | `SaveMenu`, `LoadMenu`: helyek, kis kép, törlés; a szünetmenüből is | **kész (2026-11-20)**: mentés és betöltés a szünetmenüből is, törlés, kis kép a kijelölt mentésről | – |
| Többjátékos menü | `LanMenu`: létrehozás, csatlakozás IP-vel, a LAN-on futó játékok keresése; lobbi | **kész (2026-11-20)**: menü, LAN-keresés, lobbi a képernyőn; a lobbiban népválasztás és csevegés-beírás is **kész** | Kicsi |
| Profilok | `ProfileMenu`: név, címer (kedvenc nép), statisztika, haladás; több profil | **kész (2026-11-20)**: profilkártya a főmenün, Profilok képernyő, a csata végén statisztika (a beállítások közösek) | Közepes |
| Történetek | `StoriesMenu`: a világ és a népek olvasmányai képekkel (`stories/konyvtar.json`) | **kész (2026-11-20)**: kötetek, fejezetek, görgethető olvasó; a képek helyén aláírás (a novellák kötete nélkül) | Közepes |
| Készítők | `CreditsMenu` | **kész (2026-11-20)** | – |
| Betöltőképernyő | `Loader`: tusrajzos térkép, forgó rúnakör, töltősáv, népenkénti tipp | **kész (2026-11-20)**, töltősáv nélkül | – |
| Tippek | főmenüben és betöltéskor (`tips.py`) | **kész (2026-11-20)**: betöltéskor és a főmenüben | Kicsi |
| Nyitókép | `intro`: elhalványuló nyitókép | **kész (2026-11-20)** | Kicsi |
| Élő menü-háttér | lassan úszó festett kép hulló meteorokkal | **kész (2026-11-20)**: úszó kép, hulló meteorok | Kicsi |
| Szünetmenü | mentés, betöltés, beállítások is | **kész (2026-11-20)**: folytatás, mentés, betöltés, beállítások, diagnosztika, kilépés | Közepes |

## 5. Csata beállítása

| Tétel | Régi | Új | Súly |
|---|---|---|---|
| Játéktípus „Fent kontra lent” | a pálya két oldala két csapat | **kész (2026-11-20)**: a kezdőhely magassága szerint | – |
| Kezdőhely választása | helyenként (vagy véletlen) | **kész (2026-11-20)** | – |
| Kezdőhely a csapat szerint | a csapattársak egy oldalra (`teams_hint`) | **kész (2026-11-20)** | – |
| Szín választása | helyenként | **kész (2026-11-20)** | – |
| Kötelező fejlődési irány és szövetséges kisnép | helyenként („Saját döntés” vagy adott) | **kész (2026-11-20)** | – |
| Nyersanyag-mennyiség | kevés / közepes / sok | **kész (2026-11-20)** | – |
| Játéksebesség | lassú / normál / gyors | **kész (2026-11-20)** | – |
| Táj felülírása | erdő, tél, pusztaság, mocsár, ősz, hasadékvidék | **kész (2026-11-20)**: időjárás és csempekészlet | – |
| Véletlen pálya minden indításkor | a pályaleírásból maggal generálva (`mapgen.py`) | **kész (2026-11-20)**: a régi generátor C++-ban (`map/mapgen`); a Csatában „Pálya: rögzített / új minden indításkor” | Közepes |

## 6. Beállítások – **kész (2026-11-20)**

Kész: játékidő jobb fent, „F1: súgó” felirat, árnyékok, környezeti részecskék, állapotok a figurán, játéksebesség;
kamera-távolság, sebességgombok ki-be, hősjelölés, „nyugodt figurák”, az állapotok pulzáló színe, mozgó víz ki-be,
csatanehézség.

## 7. Egér és kurzor (Közepes) – **kész (2026-11-20)**, népenkénti kurzor nélkül

A régi helyzetfüggő, festett kurzorkészletet használt (`assets/ui/cursors`):
- görgetőnyíl a képernyő szélén;
- támadás, varázslat, építés, illetve tiltójel érvénytelen helyen;
- saját, ellenséges és semleges kijelölés;
- gyűjtés nyersanyagonként, javítás;
- népenként saját kurzor (`faction_ui`).

Az új a rendszer kurzorát mutatja, pedig a 42 kurzorfájl megvan.

## 8. Csapatok – a múltkori munka maradéka – **kész (2026-11-20)**

Kész: a kezdőhely a csapat oldalán (ha a pálya jelöli az oldalakat, és legfeljebb két csapat van); a gép a csapattárs
kezdőhelyét nem tekinti ellenséges bázisnak; a gyógyítás, az aurák, a gyógyító mező és az önműködő gyógyítás a
csapattársra is hat.

- kezdőhely a csapat szerint (lásd 5.);
- a gép a szövetséges kezdőbázisát is választhatja célnak;
- a baráti képességek (gyógyítás, erősítés, szél) csak a saját egységre hatnak, a régiben a csapattársra is.

## 9. Ami a régiben sem volt bekapcsolva

**Égi uralom (Hasadékkövek, pontfoglalás):** a régiben „Ö2 elvetve”, alapból ki, csak küldetésből volt
kapcsolható. Nem számít hiánynak.

## Javasolt sorrend

1. **Támadás- és varázsanimáció, varázslathatások, területi hatások**: a képek megvannak, csak be kell kötni.
2. **Népmechanikák a pályán**: fertőzött talaj, tavacskák, spórahálózat.
3. **Hang**: zene, egységhangok, riasztás. A fájlok megvannak (1279 hangfájl az `assets/music` és `assets/sounds` alatt), csak a lejátszó hiányzik.
4. **Mentés és betöltés**, a szünetmenüből is.
5. **Értesítések, állapotjelek a figurán, kurzorok.**
6. **Többjátékos menü**, LAN-kereséssel.
7. **Csata beállítása**: kezdőhely, szín, nyersanyag; aztán a profilok, a betöltőképernyő és a tippek.
8. Apróságok: beállítási kapcsolók, összekötött falak, készítők, nyitókép, menü-háttér.
