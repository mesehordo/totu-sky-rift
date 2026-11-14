# ADR-005: Adat- és sablonformátum

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-12
**Érintett:** M2 (sablonbetöltő, VFS), M3 (Lua), M7 (tartalom), M11 (szerkesztő), M13 (modok, mod.io).
Alap: D9, D22 (képességmodell), D24 (módosítók), D25 (hősök, tárgyak, elemek), D23 (mod.io).

## Probléma

A tartalmat (fajok, egységek, épületek, kutatások, képességek, állapotok, tárgyak, pályák, küldetések) olyan
formában kell leírni, amely (1) a mai 22 fajfájlból és a hadjáratból eszközzel átvihető, (2) modderek számára
egyszerű, és **öröklődéssel** és **felülírással** (0 A.D., OpenRA) bővíthető, (3) sémával ellenőrizhető, (4) a
szerkesztő változatlanul visszamenti, (5) determinisztikusan töltődik be (ADR-004).

## Döntési javaslat

1. **Formátum: JSON** (UTF-8), a mai fájlok folytatásaként. Megjegyzés `"_doc"` / `"_leiras"` kulcsban; a betöltő a
   `_` kezdetű kulcsokat figyelmen kívül hagyja. (YAML/XML elvetve: két formátum és új eszközlánc lenne.)
2. **Sablonok és öröklődés:** minden elem egy sablon azonosítóval; `"inherits": "sablon_id"` (vagy lista) – a
   mezők mélyen összefésülődnek, a gyermek felülír. Közös alapsablonok (`template_unit`, `template_infantry`,
   `template_building` …) a 0 A.D. mintájára.
3. **Komponensek:** a sablon `components` része nevesített komponenseket ad (`Health`, `Attack`, `Movement`,
   `Vision`, `Producer`, `Gatherer`, `Builder`, `Abilities`, `Inventory`, `Aura` …); a motor C++-rendszerei és a
   Lua-komponensek ezekből dolgoznak. Az átmenet idején a mai lapos kulcsok (pl. `hp`, `speed`) a konverterben
   képződnek le a komponensekre.
4. **Közös modellek** (a döntések szerint):
   - **Módosító** (D24): `{ "path": "Attack.damage", "op": "add|mul|replace", "value": 2, "filter": {...} }`.
   - **Állapot** (D22, D25): időtartam (hősre külön), időköz, időközi hatások, módosítók, halmozás, jelleg,
     eloszlathatóság; az elemek is állapotok, kölcsönhatásaik (`removes`, `amplifies`, `immune_tags`) adatban.
   - **Képesség** (D22): célzás, célpontszűrő, költség (a faj forrása az alapérték), újratöltés, hatótáv,
     varázslási idő, csatornázás, automatikus használat (feltétellel), szintek, hatáslista, opcionális Lua-szkript.
   - **Kutatás** (D24): szintek, előfeltétel-kifejezés (`all` / `any`), módosítók, adott képességek, kizáró csoport.
   - **Tárgy** (D25): fajta, ritkaság, egyedi, töltetek, módosítók, képességek.
   - **Előfeltétel-kifejezés** (0 A.D. Requirements): `{ "all": [ {"building": …}, {"research": …},
     {"path": …}, {"ally": …}, {"ascended": true} ] }`, `any`-vel kombinálható.
5. **Séma:** minden fájltípushoz JSON Schema; a betöltő és a CI ellenőrzi (hibás mod nem töltődik be, érthető
   hibaüzenettel). A szabálykiterjesztések (pl. a D22 varázserő-szabálya) a sémán túli ellenőrzésként.
6. **Modok és felülírás:** VFS-rétegek (alap → modok a betöltési sorrendben); egy mod a sablonokat
   **foltként** módosíthatja: mély összefésülés, `null` törli a kulcsot (a mai `mods/` működése), listákhoz
   `"+": […]` hozzáadás és `"-": […]` eltávolítás. A modlock-hash a modok **betöltött, összefésült** tartalmából
   készül (D23).
7. **Számok:** tizedes számok az adatban; a betöltő determinisztikusan alakítja `Fixed`-re (ADR-004).
8. **Szövegek:** a látható szövegek kulcsai a sablonban, a fordítások gettext PO-fájlokban (hu, en, …); a mai
   magyar szövegek a konverterrel kerülnek a PO-ba.
9. **Konverzió:** `tools/` alatt egy konverter a mai `data/`, `campaigns/` és `abilities.json` tartalmából
   előállítja az új formát (a mai alapértékeket kifejezetten beírva – D22/2); a próbák (spec/) a mai viselkedést
   ellenőrzik.

## Következmények

- A modderek egy formátumot tanulnak; az öröklődés miatt egy új egység néhány sor.
- A szerkesztő (M11) a sémából építheti az objektumszerkesztő mezőit.
- A Python-referencia a régi formátumot használja tovább; a konverter a két formátum közti hídat adja.

## Kiegészítés (2026-11-12, M2 3. lépés)

**Fajtánkénti névtér.** A valódi tartalom betöltése megmutatta, hogy a mai adatokban ugyanaz az azonosító két fajtához
is tartozhat (`imp_nagy_alapozas` épület és kutatás, `ten_lampasfeny` kutatás és képesség) – a Python-játékban ezek
külön névtérben éltek. Ezért a sablonazonosítók fajtánként külön névtérben vannak: `entity` (egységek, épületek,
alapsablonok), `research`, `ability`, `item`, `status`, `faction`; a hivatkozás helyéből mindig egyértelmű, melyik
névtérről van szó (pl. `Researcher.list` → `research`).

