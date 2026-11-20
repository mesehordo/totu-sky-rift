# A játék összevetése a tanulmányokkal

Kelt: 2026-11-20. Az alap a tanulmányok javított kiadása (2026-11-11): a normatív `00_Kozos_Kovetelmenyek`, a
`01_Kutatas_es_Dontesi_Tanulmany`, a `02_Frissitett_Teljes_Fejlesztesi_Kezikonyv`, a HUD- és játékmechanikai
specifikáció, a különálló R-tanulmányok (harc, gazdaság, köd, felület, hang, AI), valamint a 2026-11-18-i bővített
tervezési-kutatási kézikönyv. Az `Atfogo_v2` ezek összefűzése, ezért külön nem kell olvasni (lásd
[TANULMANY_LELTAR](../alap/TANULMANY_LELTAR.md)). A kampányok és a novellák kimaradtak.

A [tulajdonosi döntések](../alap/DONTESEK.md) több tanulmányi javaslatot már tudatosan felülírtak. Ezek nem
számítanak eltérésnek, csak jelölve vannak.

Jelölés: **megfelel** · **döntéssel feloldva** (Dxx) · **megbeszélendő** (a 7. fejezet sorszámával) · **hiányzik**.

## 1. Méret, szimuláció, teljesítmény

| Előírás | Állapot |
|---|---|
| 1–8 játékos, 200 pont/fő (`00`) | **megfelel**: `max_supply` 200 (D7) |
| 20 Hz szerverszimuláció | **döntéssel feloldva**: 30 Hz (D6) |
| Fix lépés, egész/fixpontos számítás, stabil ID-sorrend, nincs eldobott tick | **megfelel** (ADR-004) |
| Nincs globális `MAX_UNITS` | **megfelel** |
| MP-8x200 mérés, p99 ≤ 40 ms | mozgásra **megfelel** (p99 24,8 ms, M4); a teljes mechanikájú, 8 gépi játékosos csata külön mérése nincs dokumentálva |

## 2. Navigáció és mozgás

| Előírás | Állapot |
|---|---|
| Clearance lábnyomméret szerint | **megfelel** (méretosztályonkénti rács) |
| Átlós lépés csak, ha mindkét oldalsó cella átjárható (`00`) | **megfelel** (A*: „nincs sarokvágás”) |
| Ütemezett útkeresés csomópontkerettel | **megfelel** (`path_budget`, `node_budget`) |
| Elérhetetlen cél: legközelebbi pont + visszajelzés | **megfelel** (a visszajelzés 2026-11-20 óta) |
| Elakadásfigyelés, teleport nélkül | **megfelel** |
| Formációk, vonalra mozgatás, csoportparancs | **megfelel** (D32, D46) |
| HPA*/flow field csak mért igény esetén | **megfelel** (régiók és kapuk; flow fieldre a mérés szerint nincs szükség) |

## 3. Parancsok és irányítás

| Előírás | Állapot |
|---|---|
| Kijelölés, mozgás, támadás, attack-move, stop, hold, patrol, javítás, munkásfeladat, építés, kutatás, képesség, vezérlőcsoport, gyülekezőpont, Shift-sor, jobb gombos kontextus | **megfelel** |
| Autocast kapcsoló | **megfelel** (kapcsolós képességek) |
| Stance (agresszív / védekező / helytartás) | **kész (2026-11-20)**: 4 fokozat az AoE2 szerint (védekező alap, agresszív, helytartás, passzív) |
| Guard / follow (kísérés, követés) | **kész (2026-11-20)**: jobb kattintás saját egységre = követés; „Kísérés” gomb = követ és véd (egységet vagy épületet) |
| Tétlen munkás, hősválasztás (F2–F4), billentyű-újrakiosztás ütközésjelzéssel | **megfelel** |
| Minitérkép-jelzés (ping) | **kész (2026-11-20)**: Alt + kattintás a minitérképen vagy a terepen; a szövetségesek minitérképén 3 mp-ig lüktet, hanggal, a Szóköz odaugrik |
| A játékos parancsa felülírja az automatikát | **megfelel** |

## 4. Harc, képességek, hősök

| Előírás | Állapot |
|---|---|
| Adatvezérelt harc, kevés, látható sebzéskategória | **megfelel** (osztálymátrix, D22) |
| Célpontfajták (föld/lég/épület) | **megfelel** |
| Minimális lőtávolság | **kész (2026-11-20)**: `Attack.min_range`; a 7 lövő ostromgépé 2 csempe |
| Baráti tűz alapértelmezése képességenként | **kész (2026-11-20)**: `friendly_fire` a fegyveren és a képességen, alapból ki |
| Állapothatások halmozódása szabályból | **megfelel** (lejárati idő szerinti frissítés) |
| Idézések: populáció vagy külön keret, nem lehet korlátlan | **kész (2026-11-20)**: az idézés nem foglal létszámot (WC3); a lejárat és a hősönkénti korlát marad |
| Hős: 6 tárgyhely, tapasztalat, szint; semleges szörnyekből tárgy | **megfelel** (D25) |

## 5. Gazdaság, népek, szövetséges kisnép

| Előírás | Állapot |
|---|---|
| Meteorit, fa, arany, populáció | **megfelel** |
| Népenkénti építési logika adatból | **megfelel** (építőstílusok, népmodulok) |
| Egy lelőhelyen ne dolgozzon lehetetlen számú munkás (foglalás, sor) | **kész (2026-11-20)**: népenkénti munkahelyszám, átirányítás, sorban állás |
| Szövetséges kisnép: 3 épület | **megfelel** (D11: egy nép, három épülete) |
| A gép a kisnépet stratégiai rés alapján válassza, ne véletlenül | **kész (2026-11-20)**: a hiányzó szerep (gyógyító, lövő, ostrom, légvédelem) szerint; a preferencia döntetlennél |

## 6. Köd, információ, hálózat

| Előírás | Állapot |
|---|---|
| Három állapot, a legutóbb látott épületek, rejtőzés és felfedés | **megfelel** (D41) |
| Rejtett egységet se minitérkép, se hang ne áruljon el | **megfelel** (a hangok csak a látható egységekről) |
| Szerverautoritatív, látótérszűrt állapotküldés | **döntéssel feloldva**: lockstep relével (D4); a teljes állapot a kliensen van, ez vállalt kockázat |
| LAN-keresés, modlock, újracsatlakozás | **megfelel** |
| A lobbiban látsszon a verzió, a modlista, a csapat, a gép, a néző | **kész (2026-11-20)**: a relé a lobbiüzenetben küldi a csapatokat, a gépi helyeket (nép, nehézség), a nézők számát, a gazda verzióját és modlistáját; a lobbipanel kiírja. Nyitott: a hálózati meccs gazdája még nem tud gépi helyet és csapatot beállítani (a közös beállítóképernyővel jön) |
| A gyenge grafikai beállítás sem rejthet el játékfontosságú jelzést | **kész (2026-11-20)**: kikapcsolt állapot-animációnál álló, színes jel; az adatlapon WC3-szerű állapotikonok |

## 7. Felület és hozzáférhetőség

| Előírás | Állapot |
|---|---|
| HUD-sorrend, 1:1 portré, 2×3 tárgy, 4×3 kijelölés, 4×3 WERT/SDFG/XCVB, betű a jobb alsó sarokban, felső erőforrássáv a három alakkal és a fogaskerékkel, küldetések háttér nélkül bal fent | **megfelel** (D42; a régi HUD szerint) |
| Nem hősnél a tárgyhelyek helyén a népjelvény | **megfelel** |
| Felületméret 75/80–150% | **kész (2026-11-20)**: 75–200%; 100% alatt a HUD-sáv és a menük kisebbek, 900 px magasság alatt 90% a legkisebb |
| Színvak-mód, villogás csökkentése, feliratok a hangjelzésekhez | **megfelel** (színvak-beállítás, nyugodt mód, értesítések) |

## 8. Hang

| Előírás | Állapot |
|---|---|
| Külön hangerők, legfeljebb N egyidejű hang, ismétlődő hangok ritkítása, környezeti hang | **megfelel** (2026-11-20) |
| Zenei átúsztatás | részben: felúszás van, a két szám közti átúsztatás nincs |
| Távolság szerinti hangerő | részben: a képen kívüli harc néma, a képen belül nincs távolság szerinti halkítás |

## 9. Pályák, szerkesztő, mentés, modok

| Előírás | Állapot |
|---|---|
| Tiled-alapú szerkesztés | **döntéssel feloldva**: saját szerkesztő (D45) |
| Deklaratív trigger, nincs tetszőleges kód | **megfelel** (Lua-sandbox, D19) |
| Szimmetrikus, ellenőrzött csatapályák; a díszlet és a semleges szereplő ne zárja el tartósan az utat | **megfelel** (a generátor ellenőrzi az elérhetőséget; a háttérszereplő nem akadály) |
| Mentés: teljes állapot + metaadat (verzió, pálya- és modhash, kép) | **kész (2026-11-20)**: a meta a tartalom ujjlenyomatát (a modlock hash-e) és a modlistát is tárolja; eltérésnél a betöltőlista és a meccs figyelmeztet, a betöltés nem tiltott |
| Visszajátszás lejátszása: szünet, gyorsítás, léptetés | **hiányzik (13)** – a visszajátszás a tesztekben és a mérésben fut, a kliensben nincs lejátszó |
| Modok: jegyzék, függőségek, ujjlenyomat, sandbox | **megfelel** (D9, D50) |

## 10. Megbeszélendő és hiányzó tételek

Tételenként kell megbeszélni; a régi játék szabálya csak akkor marad, ha nem ütközik a tanulmánnyal.

1. ~~**Stance**~~ – **kész (2026-11-20)**: 4 fokozat (a tulajdonos döntése), a „Tart” (T) helyén léptethető gomb.
2. ~~**Követés / kísérés**~~ – **kész (2026-11-20)**: a tulajdonos döntése szerint (követés jobb kattintással, kísérés gombbal).
3. ~~**Minimális lőtávolság**~~ – **kész (2026-11-20)**: a tulajdonos döntése szerint 2 csempe az ostromgépeknek.
4. ~~**Baráti tűz**~~ – **kész (2026-11-20)**: csak a lehetőség, alapból kikapcsolva (a tulajdonos döntése).
5. ~~**Idézések és a létszámkorlát**~~ – **kész (2026-11-20)**: nem foglalnak létszámot (a tulajdonos döntése, WC3).
6. ~~**Munkáshely-korlát**~~ – **kész (2026-11-20)**: népenként eltérő számok (a tulajdonos rám bízta).
7. ~~**A gép kisnép-választása**~~ – **kész (2026-11-20)**: a javaslat szerint (a tulajdonos továbblépett).
8. ~~**„Állapotok a figurán” kapcsoló**~~ – **kész (2026-11-20)**: a javaslat és a WC3 szerint (a tulajdonos döntése).
9. ~~**Felületméret**~~ – **kész (2026-11-20)**: 75% és 90% is választható.
10. ~~**Lobbi-adatok**~~ – **kész (2026-11-20)**: a megjelenítés és az átvitel; a gépi hely és a csapat beállítása a hálózati gazdánál a közös beállítóképernyővel jön.
11. ~~**Minitérkép-jelzés**~~ – **kész (2026-11-20)**: a WC3 szerint.
12. ~~**A mentés tartalom-ujjlenyomata**~~ – **kész (2026-11-20)**: figyelmeztetés, nem tiltás (0 A.D.).
13. **Visszajátszás-lejátszó** a kliensben (szünet, gyorsítás, léptetés) (hiány).
