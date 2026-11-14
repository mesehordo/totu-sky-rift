# Küldetések és hadjárat az új motorban – terv nulláról

Kelt: 2026-11-12 · státusz: **terv** (a megvalósítás az M11-ben). Alap: D20 (győzelmi feltételek, hadjárattérkép),
D28 (a Micélia- és a novella-küldetések kimaradnak; a küldetések nulláról készülnek), K2 (WC3-szintű küldetések),
[FORRASOK_ES_TANULSAGOK.md](FORRASOK_ES_TANULSAGOK.md) 2.5, [VILAGEPITES_INTEGRACIO.md](VILAGEPITES_INTEGRACIO.md),
a kánon (`docs/EGSZAKADAS_vilag_es_tortenet_v01.md`, `docs/tortenet/story/`). A régi 46 küldetés csak helyben,
referenciaként van meg; az új küldetéseket a történetből tervezzük, nem a régiekből alakítjuk át.

## 1. Elvek

1. **A történet vezet.** Minden küldetés a kánon egy eseménye köré épül, valódi helyszínen (Edrava atlasza, a
   világépítés hely-azonosítóival), és a szereplők a kánon szerint viselkednek.
2. **Változatos küldetéstípusok** (WC3 mintájára), fejezetenként kevert ritmussal: bázisépítő, csak hősös
   (RPG-szerű), védekezés / túlélés, kíséret és evakuálás, lopakodás, időre menő, ostrom, felfedező, választásos.
3. **A döntéseknek tétje van** (a mai terv megtartott elve): a választás erősítést, párbeszédet, későbbi küldetést
   és epilógust módosít, nem láthatatlan pontszámot.
4. **Három nehézségi szint** küldetésenként (az ellenfél ereje, erőforrás, időkorlát, felderítés).
5. **Minden küldetés modolható módon készül:** ugyanazzal a szerkesztővel és Lua-API-val, amit a modderek kapnak.
6. **Hősök a hadjáratban:** a fejlődésük (szint, képességek, tárgyak) hadjáratról hadjáratra megmarad; ha a
   küldetésben elesnek, a küldetés elbukik (kivéve, ahol a történet mást mond).

## 2. A küldetésrendszer (M11)

| Elem | Leírás | Minta |
|---|---|---|
| **Trigger** | esemény → feltétel → akció; egyszeri vagy ismétlődő („megőrzött”); változók, időzítők | StarCraft, WC3 |
| **Események** | idő, halál / összes halott, sérülés, belépés/kilépés régióba, közelség, felfedezés, gyártás, építés, kutatás, képesség, tárgy felvétele, párbeszéd vége, cél teljesült | OpenRA `Trigger` |
| **Célok** | elsődleges (mind kell a győzelemhez), másodlagos (jutalom), rejtett (felfedezve jelenik meg); küldetésnapló | OpenRA, WC3 |
| **Régiók, útvonalak, kamerapontok** | a szerkesztőben rajzolva, névvel; a szkript név szerint hivatkozik rájuk | WC3 |
| **Átvezetők** | szélesvásznú sáv, kamerapálya, a játék megállítása vagy lassítása, egységek szkriptelt mozgása, feliratok | WC3 |
| **Párbeszéd** | portré, név, szöveg (fordítható), hang (a világépítés hangprototípusaiból); elágazó párbeszéd – formátuma ADR-ben (ink / saját Lua-formátum) | WC3, ink |
| **Erősítés** | egységcsoport érkezése szélről / szállítóval, időzítve vagy eseményre | OpenRA `Reinforcements` |
| **Ellenséges viselkedés** | küldetésre írt gépi stratégia (Lua), hullámok, őrjáratok, bázisépítés vagy csak védekezés | WC3, OpenRA |
| **Pályánkénti objektumadat** | a küldetés saját egység-, képesség- és kutatásváltozatai, az alapsablonokból örökölve (ADR-005) | WC3 objektumszerkesztő |
| **Hadjáratállapot** | jelzők és változók profilonként; átvitt hősök és tárgyak; döntések; elágazó küldetésgráf | WC3, a mai terv |
| **Mentés küldetés közben** | a teljes állapot a triggerekkel és a változókkal (M2 snapshot) | – |
| **Eligazítás és értékelés** | eligazító kép és szöveg (a tulajdonos festményei), célok; a végén statisztika és epilógus-jelzők | – |

A szerkesztő (M11) grafikusan építi a triggereket, és Luát generál; közvetlen Lua is írható. A küldetések adatai az
`engine/content/campaigns/` alá kerülnek (pálya, objektumadat, szkript, párbeszéd, fordítás).

## 3. A hadjárat váza

A kánon szerkezete (előjáték, főhadjáratok, átvezetők, közös befejezés) marad; a régi játék Ooni- és
Huk’ucha-hadjárata a világépítés helyeihez kötve megmarad (lásd 5. pont, döntés). A Micélia-hadjárat kimarad (D28).

| Rész | Nép, hős(ök) | Helyszín (atlasz) | Kb. küldetés |
|---|---|---|---|
| Előjáték – A második lélegzet | Aurethi Birodalom, Livia | Aureth, Fekete Tükör (a Fényes Eső, 0–2. év) | 1 |
| I. A föld alatt énekelnek | Wullof, Tava | Hétfolyó (70–72. év) | 5 |
| Átvezető – A vegyes konvoj | Ursan + Wullof, Ragna, Tava | Hétfolyó → Hosszú Tél → Kilenc Lépcső | 1 |
| II. A pajzs mögött | Ursanok, Ragna | Hosszú Tél hágói, Ghordur | 5 |
| III. A kilencedik lépcső | Szürke elfek, Szelai | Kilenc Lépcső, déli fennsík | 5 |
| IV. Akiket átengedünk | Ooni | Ooni kapuvárosok, keleti varrat | 5 |
| V. Külön tavak, közös folyó | Huk’ucha | Tavirózsa-medencék, Hétfolyó | 5 |
| Átvezető – A közös finomító | Aurethi Birodalom + Ursan, Livia, Ragna | Aureth környéke | 1 |
| VI. Az isten, aki nem enged el | Aurethi Birodalom, Livia, Nera | Aureth, Fekete Tükör | 5 |
| VII. A sokaság neve | kethrik / Croftianok, Keth | Néma Öv, Visszhang-kapu | 5 |
| Közös befejezés – A névtelen hajnal | több nép, a hősök | Fekete Tükör | 3 |

Összesen kb. 41 küldetés. A fejezeteken belüli küldetéseket **nulláról** tervezzük: fejezetenként a kánon
eseményeiből, a helyszín földrajzából, a nép mechanikájából (spec/FAJMECHANIKAK) és a típusok változatosságából.

## 4. Munkamenet

1. **Küldetéstervlap** fejezetenként (`docs/alap/kuldetesek/<fejezet>.md`): küldetésenként cél, helyszín, típus,
   szereplők, a kánon eseménye (és mi a játékhoz hozzáadott), célok, triggerek vázlata, döntések, nehézség, új
   mechanika, amit bemutat. A tulajdonos fejezetenként jóváhagyja.
2. **A küldetésrendszer és a szerkesztő** (M11), majd a fejezetek megvalósítása a jóváhagyott lapok szerint.
3. **Próbák:** minden küldetésre automatikus végigjátszás-próba (szkriptelt győzelmi útvonal), elérhetőségi és
   pályaellenőrzés (MISSION-REACHABILITY), és kézi játékpróba.

A tervlapok a motor megvalósításától függetlenül már most készülhetnek; a megvalósítás az M11-ben jön.

## 5. Döntések (D31, 2026-11-12)

1. **Mór** szövetséges hősként szerepel más fejezetekben (a Wullof-, a VII. és a befejező fejezetben); a
   gombanép-szál (a kethri betegek emléke) a VII. fejezetbe olvad; a Micéliák kisnépként jelennek meg.
2. **Az Ooni- és a Huk’ucha-hadjárat marad:** mind a hét főnépnek saját hadjárata van – kb. 41 küldetés.

**Következő lépés:** a fejezetenkénti küldetéstervlapok, az előjátékkal és az I. fejezettel kezdve, tulajdonosi
jóváhagyással.
