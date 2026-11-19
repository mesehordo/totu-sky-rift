# Helyi diagnosztika – terv

Kelt: 2026-11-18 · státusz: **MVP KÉSZ** (2026-11-19; `client/diagnostics.{hpp,cpp}`, l. IMPLEMENTATION_LOG 13.).

## 1. Ami most van

| Forrás | Hol | Megjegyzés |
|---|---|---|
| hibaüzenetek | `stderr` (`main.cpp`, 18 hely) | nincs fájl, nincs eseménykód |
| képkockaidő | `--bench` (p50, p99), `times`, `draw_times` (`main.cpp`) | csak mérőmódban |
| ticktartam | `sim_times` (`main.cpp`), `totu_netbench` | csak mérőmódban |
| visszajátszás | `sim/replay` (8-as változat): pálya, mag, fajok, parancsok, ellenőrzőpontok | a reprodukció alapja |
| szinkronhiba | `net/lockstep` (hash-eltérés), relé | a hálózati meccsben |
| beállítások | `settings.json` a felhasználói mappában | atomikus írás (`settings.cpp:213`) |
| küldetéssor-profil | `kuldetessor_<id>.json` | |

## 2. Elvek

- **Semmi nem megy hálózatra.** Csak kézi export, a játékos dönti el, mit küld el.
- A gyűjtés a kliensben él: **nem ír a szimulációba**, nem érinti az RNG-t, a parancssort, a hash-t; be- és kikapcsolva
  ugyanaz a hash (próba).
- Engedélyezőlista: csak az alább felsorolt mezők. Nincs útvonal, felhasználónév, IP, chat, fájlnév a felhasználói
  mappából.
- Nem mért érték: `null`. Nulla csak mért nulla.
- Korlátos memória: képkockaidő-hisztogram (rögzített rekeszek, nem minden képkocka), számlálók; nincs folyamatos
  fájlírás játék közben.

## 3. Séma (1. változat)

```json
{
  "schema_version": 1,
  "app": {"version": "0.1.0", "platform": "windows", "arch": "x86_64", "renderer": "opengl33", "build_type": "release"},
  "session": {"id": "<véletlen, munkamenetenként>", "mode": "skirmish|mission|chain|net",
              "map_id": "csatamezo", "seed": 0, "players": 2, "factions": ["ursan", "elves"],
              "victory": "conquest", "tempo": "classic", "duration_ticks": 0, "result": "won|lost|quit|null"},
  "performance": {"frame_ms_p50": null, "frame_ms_p95": null, "frame_ms_p99": null,
                  "tick_ms_p95": null, "tick_ms_p99": null, "over_budget_tick_ratio": null, "peak_units": null},
  "errors": {"total": 0, "by_code": {}},
  "ux": {"command_rejected_local": {"no_resource": 0, "no_supply": 0, "bad_place": 0}},
  "feedback": {"liked": null, "disliked": null, "confusing": null},
  "privacy": {"sent_automatically": false}
}
```

## 4. Eseményszótár (első kör)

| Kód | Kiváltó | Számlálás | Haszon |
|---|---|---|---|
| `ASSET_FALLBACK` | hiányzó kép, tartalék rajzolódik | darab | hiányzó grafika felderítése |
| `LOCALE_MISSING` | hiányzó fordítási kulcs | darab (kulcs nélkül) | fordítási hiány mérete |
| `LOAD_FAILED` | pálya/küldetés/tartalom betöltési hiba | darab + rövid kód | indítási hibák |
| `SAVE_FAILED` | profil- vagy beállításírás hiba | darab | adatvesztés jele |
| `NET_DESYNC` | hash-eltérés | darab + tick | szinkronhiba |
| `CMD_LOCAL_NO_RESOURCE` / `_NO_SUPPLY` / `_BAD_PLACE` | a HUD elutasítja a kattintást | darab | érthetőség (AS-03) |

## 5. Export

Az eredmény-/szünetmenüben „Diagnosztikai jelentés mentése”: a felugró ablak megmondja, mi kerül a fájlba és hová
(felhasználói mappa `diagnosztika_<dátum>.json`), a három rövid véleménymező kihagyható. Atomikus írás (ideiglenes
fájl + átnevezés); hiba esetén lokalizált üzenet, a játék fut tovább.

## 6. Tesztek

Sémavalidáció; az engedélyezőlistán kívüli mező nem kerül be; a hash be- és kikapcsolva azonos; túlméretes
véleményszöveg levágva; írási hiba (nem írható mappa) nem omlik össze.

## 7. Nyitott kérdés a tulajdonosnak

Kell-e a véleménymező (3 rövid kérdés)? Az MVP-ben a mezők `null`-ok (szövegbevitel kellene hozzá). Az export
gombja most mindkét helyen ott van: a szünetmenüben („Diagnosztika mentése”) és az összesítőn („Diagnosztikai
jelentés”); ha csak az egyik kell, egy sor törlése.
