# TOTU: Sky Rift – modkészítői leírás (SDK)

*Ez a leírás és a szabályadatok formátuma CC BY 4.0 alatt szabadon felhasználható (D8). A világ, a történet és a
grafika minden joga fenntartva; modokban a D8 modkivétele szerint használhatók. A motor kódja GPL-3.0-or-later.*

Ez a leírás az **új motorhoz** (`engine/`) tartozik. A régi Python-játék moddolását a `docs/MODDING.md` írja le.

## 1. Mi a mod?

Egy mappa egy `mod.json`-nal. A mod **réteg** az alapadatok (`engine/content/base`) fölött: ugyanazokat a
mappákat használja, és amit tartalmaz, az kiegészíti vagy felülírja az alatta lévő rétegeket.

| Hol keresi a játék | Mire jó |
|---|---|
| `engine/content/mods/<mod>/` | a játékkal szállított modok (pl. `micelia`, a három `minta_*`) |
| `%APPDATA%/TOTU/SkyRift/mods/<mod>/` (Linuxon, macOS-en a felhasználói adatmappa) | a játékos saját és letöltött modjai |

Azonos azonosítónál a beépített mod nyer; a hibás `mod.json`-ú mappa kimarad (figyelmeztetéssel).

**ZIP-ben is terjeszthető:** a `.zip`-et elég a mods mappába tenni. A játék indításkor kibontja (a felhasználói
mappa `mods_kibontva` alá; csak ha a ZIP megváltozott), és onnan mappaként tölti be. A ZIP gyökerében vagy egyetlen
gyökérmappájában legyen a `mod.json`. Tömörítés: tárolt vagy deflate (a szokásos „Tömörített mappa”); a titkosított,
a 4 GB fölötti (ZIP64) és a `..`-t tartalmazó csomagot a játék elutasítja.

### mod.json

```json
{
  "id": "sajat_mod",
  "name": "Saját mod",
  "version": "1.0",
  "description": "Mit csinál a mod – a modkezelőben látszik.",
  "depends": ["totu"],
  "conflicts": ["masik_mod"]
}
```

- `id` (kötelező): a mod azonosítója; a függőségek és a beállítások ezzel hivatkoznak rá.
- `depends`: ezeknek a modoknak engedélyezve kell lenniük, **és előbb kell betölteni őket**. A `totu` az alap.
- `conflicts`: ezekkel együtt nem tölthető be.
- `name`, `version`, `description`: a modkezelőnek.

## 2. Engedélyezés

- **A modkezelő** (Főmenü → Modok): balra az elérhető, jobbra az engedélyezett modok a betöltési sorrendben;
  Engedélyez / Letilt / Fel / Le. A hiányzó vagy rossz sorrendű függőség és az ütközés mentés előtt látszik; a
  „Mentés és újraindítás” után a játék az új készlettel indul.
- **Parancssor:** `totu --mods a,b` (ebben a sorrendben; felülírja a beállítást), `totu --mods ""` csak az alap.
- **Beállításfájl:** `settings.json` → `"mods": ["a", "b"]`.

A betöltési sorrend számít: **a később betöltött mod felülírja a korábbit**, ha ugyanazt az adatot módosítják.

## 3. A tartalom szerkezete

| Mappa / fájl | Tartalom | Névtér |
|---|---|---|
| `templates/units/*.json` | egységek | `entity` |
| `templates/buildings/*.json` | épületek | `entity` |
| `templates/researches/*.json` | kutatások | `research` |
| `templates/abilities.json` | képességek | `ability` |
| `templates/items.json`, `statuses.json` | tárgyak, állapotok | `item`, `status` |
| `templates/factions/*.json` | népek | `faction` |
| `game.json` | játékszabályok (a felső réteg teljes fájlja érvényes) | – |
| `locale/hu.po`, `locale/en.po` | szövegek | – |
| `ai/<azonosító>/` | gépi ellenfél (Lua) | – |
| `missions/<azonosító>/mission.json` | küldetés | – |

Egy sablonfájl egy JSON-objektum; a kulcsai a sablonok azonosítói. Az azonosítók **névterenként** egyediek. A
`_` kezdetű kulcsok megjegyzések. A konkrét sablonok (nem `template_` kezdetűek) az öröklés feloldása után a
`engine/content/schema/` sémáin mennek át – a hibás mezőnév vagy típus betöltési hiba, nem csendes figyelmen kívül
hagyás.

### Öröklés

```json
"urs_minta_vertes": { "inherits": "urs_pajzshordozo", "Health": { "max": 220 } }
```

Az `inherits` (szöveg vagy lista) a szülő feloldott tartalmára fésüli rá a sablont – csak az eltérést kell
megadni.

### Folt (meglévő sablon módosítása)

Ha egy mod **ugyanazt az azonosítót** adja, mint egy alatta lévő réteg, az folt: mélyen összefésülődik.

| A folt értéke | Hatás |
|---|---|
| objektum | kulcsonként, rekurzívan |
| `null` | a kulcs törlése (az örökölté is) |
| `{"+": [...], "-": [...]}` egy lista helyén | előbb az `-` elemek eltávolítása, aztán a `+` elemek a végére |
| bármi más | felülírás |

```json
"urs_harcosok_haza": { "Producer": { "units": { "+": ["urs_minta_vertes"] } } }
```

### Kép

Az egység vagy épület képe alapból az azonosítójával egyező képlap (`assets/sprites/...`). Az
`Identity.sprite` egy meglévő képlapot rendel hozzá (változatokhoz, modokhoz):

```json
"Identity": { "sprite": "urs_pajzshordozo" }
```

Saját grafikánál a képlap a többi mellé kerül (`assets/sprites/units/<id>.png` és `.json`); a játék a következő
indításkor magától újraépíti az atlaszt. **Csapatszín:** az egységképen a **tiszta kék** (#0000FF árnyalatai)
helyére kerül a játékos színe (D52) – ezt a színt másra ne használd; az épületek csapatjele a lobogó (D51).

### Szövegek

A mod `locale/hu.po` és `locale/en.po` fájljai kiegészítik az alap fordítását (`msgctxt` a kulcs, `msgid` a
magyar szöveg, `msgstr` a fordítás). Ami hiányzik, angolra, majd a kulcsra esik vissza.

## 4. Küldetés

`missions/<azonosító>/mission.json`: a pálya (a küldetés mappájából vagy az alap `maps` mappájából), a játékosok,
régiók, pontok, egységek, célok (`at`: a cél helye a pályán – arany gyűrű jelzi), párbeszédek, átvezetők és
eseményindítók. Indítás: `totu --mission <mappa>`. Teljes példa minden elemre: `engine/content/base/missions/proba`;
a küldetés a beépített szerkesztővel (`totu --editor <küldetésmappa>`) is készíthető.

## 5. Többjátékos mód

A lobbi a **séma, az alap és az engedélyezett modok** tartalmából (a sorrendjükben) ujjlenyomatot számol; eltérő
modkészlettel nem lehet csatlakozni (modlock). A játékosoknak ugyanazokat a modokat kell ugyanabban a sorrendben
engedélyezniük.

## 6. A mintamodok

| Mod | Mit mutat |
|---|---|
| `minta_egyseg` | új egység örökléssel, meglévő képpel (`Identity.sprite`), folt a termelő épületre és a nép listájára, saját szövegek |
| `minta_kepesseg` | új képesség (`abilities.json`) és folt egy meglévő egység képességlistájára (`"+"`) |
| `minta_kuldetes` | küldetés a modban: régió, egységek, két cél (az egyik helyhez kötve), győzelem és bukás eseményindítóval |

Kipróbálás: `totu --mods minta_egyseg,minta_kepesseg` (csatában az Ursanokkal), illetve
`totu --mission engine/content/mods/minta_kuldetes/missions/minta`.

## 7. Amit még nem lehet (tervben)

- a küldetés és a küldetéssor tartalma csak az alap rétegből olvas (a modbeli egységet a küldetés még nem látja);
- új győzelmi feltétel szkriptből (a győzelmi módok most beépítettek);
- online modtár (mod.io, D23).
