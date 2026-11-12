# A TOTU-tanulmányok leltára és státusza

Kelt: 2026-11-11. Vizsgált anyag: `Downloads/TOTU/tanulmányok/Javitott_2026-11-11` (minden ZIP kibontva).
**Az irányadó dokumentum ettől kezdve a [TOTU_ALAPDOKUMENTUM.md](TOTU_ALAPDOKUMENTUM.md).** A tanulmányok háttéranyagként maradnak meg.

## Számok

- 78 szövegfájl; ebből **67 egyedi**, összesen 1,39 MB.
- A `00_Kozos_Kovetelmenyek.md` öt azonos példányban szerepel.
- A három legnagyobb fájl **teljes egészében más fájlok összefűzése** (kb. 450 KB ismétlés):
  - `TOTU_Sky_Rift_Atfogo_RTS_Tanulmany_v2.md` = 01 + 02 + 03 (1394 sorból 12 saját sor, mind fejléc);
  - `02_Atfogo_RTS_Tanulmany_v2_Forrasaudittal.md` = az előző + 2 figyelmeztető sor;
  - `TOTU_CPP_CSHARP_CROSSPLAY_MODDING_TELJES_v1_1.md` (+ azonos `.txt`) = a C++/C# csomag 01–09 fájlja + 2 sor.
- A szülőmappa fájljai a javított kiadás régebbi változatai. Az `új/` mappa tartalma azonos a C++/C# ZIP-pel.
- A 16 különálló R-tanulmány sablonra épül: mindegyikben 4 teszt, 3–4 „[NYITOTT]” kérdés, és a 6 lépéses záró blokk szó szerint ugyanaz.

## Fájlonkénti besorolás

| Dokumentum | Besorolás | Megjegyzés |
|---|---|---|
| `00_Kozos_Kovetelmenyek.md` | **beolvasztva** | Az alapdokumentum 3. fejezetébe került, a kóddal összevetve |
| `Hibajegyzek.md` | **beolvasztva** | A javított ellentmondások rendezve; több közülük a C++/C# csomagban visszatért |
| `01_Kutatas_es_Dontesi_Tanulmany.md` | háttér, kivonatolva | A fő érvelés és a „NE” lista megmaradt |
| `02_Frissitett_Teljes_Fejlesztesi_Kezikonyv.md` | háttér, kivonatolva | A 01 kibontása; a repostruktúra és a roadmap felülírva |
| `03_Meresi_es_Dontesi_Munkalap.md` | **eszköz** | A mérési azonosítók és az ADR-sablon érvényesek |
| `00_AI_Fejlesztesi_Munkajegyzek.md` | **munkamódszer érvényes**, sorrend felülírva | A kis lépések és a kapuk elve marad; a sorrendet az alapdokumentum 5. fejezete adja |
| `04_Megvalositasi_Utasitas.md` | elavult | A munkajegyzék és az alapdokumentum váltja |
| `TOTU_RTS_UI_es_jatekmechanika_specifikacio.md` | **teljesítve** | A HUD megvalósult; `tools/test_hud.py` ellenőrzi |
| `TOTU_hadjarat_es_kuldetes_utmutato.md` | **érvényes folyamatleírás** | Új küldetés készítésekor; a hivatkozott kánonfájlok a `docs/tortenet/` alatt vannak |
| `TOTU_Sky_Rift_RTS_teljesitmeny_tanulmany.md` | referencia | Történeti RTS-összevetés; nem normatív (a közös követelmények is így minősítik) |
| `Atfogo_v2` és forrásauditált másolata | **ismétlés** | Nem kell olvasni |
| R/`01`–`16` | referencia | A tesztazonosítók és a nyitott kérdések az alapdokumentumba kerültek |
| R/`00_Forrasregiszter`, `01_Forrasaudit`, `05_..._Allitas_Terkep`, `SHA256` | irodalom | – |
| LAN/`01`–`03` | referencia | A szerverautoritatív javaslat → D4/D5 döntés |
| LAN/`04_Verseny…`, `05_Tovabbi…` | későbbi | Rangsorolt verseny, további kutatási témák |
| REF/ (121 játékos katalógus, 24 kísérlet) | referencia | Átfed az R-tesztekkel |
| C++/C# csomag (`01`–`10`) | **döntésre vár** | Új, kötelezőnek jelölt igények (macOS, 0 A.D.-szintű modolás) → D1, D3, D9 |

## Ismétlődő vagy visszatérő hibák a tanulmányokban

| Hiba | Hol | Helyes |
|---|---|---|
| „3×4 parancsrács” | 01 (javítva), **C++/C# 01 §9 (visszatért)** | 4 oszlop × 3 sor, WERT/SDFG/XCVB – a játékban így van |
| „Három allies ikon” | 01, 02, **C++/C# 01 §9** | Egyetlen háromfigurás Szövetségesek-gomb |
| „GPLv2+ a korábban választott licenc” | C++/C# 01 | A 02-es kézikönyvben csak **cél**; a repó tényleges licence MIT |
| MonoGame kizárva licenc miatt, majd a Fázis 2-ben mégis jelölt | C++/C# 01 §1 ↔ §18 | Belső ellentmondás |
| Teljesítménykapuk 15/25, 20/35, 25/40 ms | 01, 02, teljesítménytanulmány | A közös követelményekben 25/40 ms (a hibajegyzék rendezte) |
| „1600 egység” = „1600 poppont” | több helyen | Két külön teszt: MP-8x200-POP és PERF-1600-ENTITIES |
| Nyersanyag „kő” | a repó `README.md` és `DESIGN.md` fájlja | A játékban: Fa, Arany (`stone` azonosító), Meteorit, Áhítat |

## Amit egyik tanulmány sem tudott

Mindegyik tanulmány kimondja, hogy a valódi kódot nem látta. Ezért:
- sok „hiányként” leírt rendszer már működik (adatvezérelt fajok, kisfaj-szövetség, JSON-küldetések, HUD, vezérlőcsoportok, rally, hősök, tárgyak);
- a tényleges szűk keresztmetszetet (az útkeresés a tickidő 51–76%-a) és a 8 játékosos összeomlást nem láthatták;
- az „Áhítat” nyersanyagot és a valós kánonforrásokat (`docs/tortenet/FOFAJOK.md`, `SZOVETSEGES_NEPEK.md`) nem ismerték.
