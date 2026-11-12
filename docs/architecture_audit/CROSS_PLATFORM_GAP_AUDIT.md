# Keresztplatformos hiányaudit

Kelt: 2026-11-11 · commit `320d42e`. Követelmény: Windows x64, Linux x86_64, macOS Intel, macOS Apple Silicon; vegyes platformú LAN és online.
Minden sor `PASS / FAIL / NOT_RUN / BLOCKED`. Csak Windows-gép állt rendelkezésre.

| Ellenőrzés | Windows x64 | Linux x86_64 | macOS x64 | macOS arm64 | Megjegyzés |
|---|---|---|---|---|---|
| Szimuláció ablak nélkül (`--simulate`) | PASS | NOT_RUN | NOT_RUN | NOT_RUN | Python + pygame-ce + numpy mind a négy platformon elérhető; a kódban nincs Windows-specifikus hívás |
| Kliens indítása, HUD | PASS (dummy SDL, `test_hud.py`, `--screenshot`) | NOT_RUN | NOT_RUN | NOT_RUN | pygame-ce szoftveres blittelés, nem OpenGL – az OpenGL 3.3 követelmény ma nem releváns rá |
| Fájlutak | PASS | NOT_RUN | NOT_RUN | NOT_RUN | `os.path`, a modútvonal `/`-re normalizálva (`data.py` 42) |
| Egy gépen determinizmus | PASS (2 × 8 perc azonos) | NOT_RUN | NOT_RUN | NOT_RUN | |
| **Vegyes platformú lockstep LAN** | – | – | – | – | **Magas kockázat**: a szimuláció float-számítást és `math.sin/cos/atan2/sqrt` hívásokat használ; ezek a platform libm-jétől függhetnek. A CRC32-hash (`net.py` 142) eltérésnél „Szinkronhiba”-t jelez, javítani nem tud. NOT_RUN |
| Online / dedikált szerver | BLOCKED – nincs implementálva | | | | |
| Csomagolás, aláírás, notarizálás | NOT_RUN – nincs build-folyamat (`dist/` csak grafikai csomag) | | | | |
| CI | NINCS | | | | |

## Mit jelent ez a döntéshez?

1. A mai Python-változat a négy platform közül egyiken sem bukott meg, de csak Windows-on ellenőriztem. Platform-hordozhatóságban ez a legkisebb kockázatú kiindulópont.
2. A vegyes platformú, lockstep alapú multiplayer bármelyik nyelven a lebegőpontos determinizmuson múlik. Két kiút van: fixpontos/egész szimuláció, vagy szerverautoritatív modell (ezt írják elő a követelmények). A nyelvválasztás ezt a kérdést nem oldja meg.
3. macOS-hez fizikai Intel- és Apple Silicon-gép kell a „támogatott” státuszhoz (`04_DONTESI_MUNKALAP.md` kiegészítő 3. kérdés).
