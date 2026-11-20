# Mit nem tud még az új motor a régi játékhoz képest

Kelt: 2026-11-19. A régi Python-játék (`rts/`, 24 600 sor) minden moduljának függvényeit és képernyőit vetettem
össze az új motorral (`engine/`). A **kampányok és a novellák** a tulajdonos kérésére kimaradtak.

**A szabályok rendben vannak.** A szimuláció szabályait a tartalmi leltár (`docs/alap/TARTALOM_LELTAR.md`) és a
leltár-kapu (`leltar_kapu.json`) tételesen fedi: 396 tétel, mindegyik megvalósított lépésre mutat, és teszt őrzi.
A hiányok a **megjelenítésben, a hangban, a menükben és a kiegészítő rendszerekben** vannak.

Súlyosság: **Nagy** – játék közben feltűnő vagy egy egész rendszer hiányzik; **Közepes** – érezhető, de van
kerülőút; **Kicsi** – kényelmi vagy ritka.

## 1. Hang – teljesen hiányzik (Nagy)

Az új motorban egyáltalán nincs hanglejátszás; csak a hangerő-beállítások vannak meg (`settings.hpp`).

| Régi (`rts/audio.py`) | Új |
|---|---|
| menüzene és játékzene, sávváltással | nincs |
| egységhangok: kijelölés, nyugtázás, támadás, halál, népenként | nincs |
| „Támadás ér!” riasztóhang (12 mp-es korláttal) | csak a szöveges értesítés |
| környezeti hangok | nincs |
| zenei menü (`MusicMenu`): a számok listája és lejátszása | nincs |

## 2. Egységek és épületek a pályán

| Tétel | Régi | Új | Súly |
|---|---|---|---|
| Támadás-, varázslás- és munkaanimáció | `unit_anim`: attack, cast, work, gather, construct, repair | **kész (2026-11-20)** | – |
| Varázslathatások | elemenkénti képlapok (`vfx_<elem>_cast/bolt/impact/area/nova`, 13 elem), gyógyítás, szintlépés, gyűrű, por, nova | **kész (2026-11-20)**: varázslás, nova, varázslövedék, becsapódás, szintlépés, halál (vér/robbanás); a gyógyítás képe még nincs | – |
| Területi hatások | `draw_area`: a tartós varázsterület (pl. tűzmező) képe | **kész (2026-11-20)** | – |
| Állapotjelek a figurán | `draw_status_fx`: láng, jégkristály, buborék, Z Z Z, csillagok, szikra; pulzáló szín | **kész (2026-11-20)**; a pulzáló szín még nincs | – |
| Croftian fertőzött talaj | `draw_creep` | **kész (2026-11-20)** | – |
| Huk’ucha tavacskák | `draw_ponds`: kerek part, homoksáv, habvonal | **kész (2026-11-20)** | – |
| Micélia spórahálózat | `draw_network`: lüktető szálak az épületek között | **kész (2026-11-20)** | – |
| Lárvák, osztódás jelei | `draw_building_marks` (Kaptár lárvái), `draw_unit_marks` (micélia: pöttyök az osztódásig) | a lárvák **kész (2026-11-20)**; az osztódás pöttyei még nincsenek | Kicsi |
| Rothadó fa | `draw_rot_bubbles` (Croftian) | nincs | Kicsi |
| Félkész munkahelyek | `draw_worksites`, `draw_foundations`: ásás, alapozás, lapkő, tüskék haladása | csak az épület építési fázisa | Közepes |
| Összekötött falak | `_wall_frame`: a falszakaszok a szomszédjuk szerint | minden fal külön képként | Közepes |
| Kép nélküli egységek | `render.draw_shape`: népenként jellemző alakzat (kör, lándzsás, pajzsos …) a csapatszínnel | **kész (2026-11-20)**: a régi alakzatok; a 103 kép nélküli sablonnak (Huk’ucha 21, Ooni 18, a kisnépek egységei) ettől még kép kell | Közepes |
| Képpontpontos kijelölés | `hover_sprite`: a látható képpont dönt | téglalap a figura körül | Kicsi |
| Díszletépületek, háttérszereplők | `draw_decor`, `draw_actor` | nincs | Kicsi |

## 3. Értesítések játék közben (Közepes) – **nagyrészt kész (2026-11-20)**

Kész: elesett hős, szövetség, fejlődési irány, felemelkedés, új cél, a képesség még töltődik, játéksebesség. Még nincs: újratestesülés, osztódás, telihold, döntés, nincs célpont, az épület tele, a bánya betelt, elérhetetlen cél (ezekhez a szimuláció eseményei kellenek).

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
| Mentés és betöltés | `SaveMenu`, `LoadMenu`: helyek, kis kép, törlés; a szünetmenüből is | **kész (2026-11-20)**: mentés a szünetmenüből, betöltés és törlés a főmenüből; kis kép még nincs, a betöltés a szünetmenüből még nincs | – |
| Többjátékos menü | `LanMenu`: létrehozás, csatlakozás IP-vel, a LAN-on futó játékok keresése; lobbi | csak parancssorból (`--host`, `--join`), konzolos lobbi; LAN-keresés nincs | **Nagy** |
| Profilok | `ProfileMenu`: név, címer (kedvenc nép), statisztika, haladás; több profil | nincs | Közepes |
| Történetek | `StoriesMenu`: a világ és a népek olvasmányai képekkel (`stories/konyvtar.json`) | nincs | Közepes |
| Készítők | `CreditsMenu` | **kész (2026-11-20)** | – |
| Betöltőképernyő | `Loader`: tusrajzos térkép, forgó rúnakör, töltősáv, népenkénti tipp | **kész (2026-11-20)**, töltősáv nélkül | – |
| Tippek | főmenüben és betöltéskor (`tips.py`) | betöltéskor **kész (2026-11-20)**; a főmenüben még nincs | Kicsi |
| Nyitókép | `intro`: elhalványuló nyitókép | nincs | Kicsi |
| Élő menü-háttér | lassan úszó festett kép hulló meteorokkal | a kép úszik; meteorok nincsenek | Kicsi |
| Szünetmenü | mentés, betöltés, beállítások is | folytatás, diagnosztika, kilépés | Közepes |

## 5. Csata beállítása

| Tétel | Régi | Új | Súly |
|---|---|---|---|
| Játéktípus „Fent kontra lent” | a pálya két oldala két csapat | nincs (a csapat helyenként megadható) | Kicsi |
| Kezdőhely választása | helyenként (vagy véletlen) | **kész (2026-11-20)** | – |
| Kezdőhely a csapat szerint | a csapattársak egy oldalra (`teams_hint`) | nincs | Közepes |
| Szín választása | helyenként | **kész (2026-11-20)** | – |
| Kötelező fejlődési irány és szövetséges kisnép | helyenként („Gép dönt” vagy adott) | nincs | Kicsi |
| Nyersanyag-mennyiség | kevés / közepes / sok | **kész (2026-11-20)** | – |
| Játéksebesség | lassú / normál / gyors | csak játék közben állítható | Kicsi |
| Táj felülírása | erdő, tél, pusztaság, mocsár, ősz, hasadékvidék | a pálya saját tája | Kicsi |
| Véletlen pálya minden indításkor | a pályaleírásból maggal generálva (`mapgen.py`) | a pályák rögzített JSON-ok | Közepes |

## 6. Beállítások – **nagyrészt kész (2026-11-20)**

Kész: játékidő jobb fent, „F1: súgó” felirat, árnyékok, környezeti részecskék, állapotok a figurán, játéksebesség.
Még nincs: kamera-távolság, sebességgombok ki-be, hősjelölés módja, „nyugodt figurák”, az állapotok pulzáló színe,
mozgó víz ki-be, csatanehézség.

## 7. Egér és kurzor (Közepes) – **kész (2026-11-20)**, népenkénti kurzor nélkül

A régi helyzetfüggő, festett kurzorkészletet használt (`assets/ui/cursors`):
- görgetőnyíl a képernyő szélén;
- támadás, varázslat, építés, illetve tiltójel érvénytelen helyen;
- saját, ellenséges és semleges kijelölés;
- gyűjtés nyersanyagonként, javítás;
- népenként saját kurzor (`faction_ui`).

Az új a rendszer kurzorát mutatja, pedig a 42 kurzorfájl megvan.

## 8. Csapatok – a múltkori munka maradéka

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
