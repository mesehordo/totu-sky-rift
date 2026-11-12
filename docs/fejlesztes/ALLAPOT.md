# Fejlesztési állapot

**Irányadó dokumentum: [`docs/alap/TOTU_ALAPDOKUMENTUM.md`](../alap/TOTU_ALAPDOKUMENTUM.md)** (fázisok F1–F8), nyitott döntések: [`docs/alap/DONTESEK.md`](../alap/DONTESEK.md). A korábbi S00–S14 munkajegyzékből csak a munkamódszer érvényes.

| Mező | Érték |
|---|---|
| Aktív szakasz | Új motor (D18–D20): M0 Alapozás (leltár kész) · **M1 platform-váz kész**: CI zöld mind az 5 rendszeren (6 feladat) |
| Aktív feladat | S01-02: ablakos kézi próba (F-002) |
| Kiindulópont | nyilvános GitHub-repó, új történet 2026-11-12; a régi történet helyi mentése a repó melletti `.bundle` fájlban; grafika és zene a repóban, a generáló anyagok a helyi `_privat/` mappában |
| Utolsó ellenőrzött állapot | 2026-11-12: `test_dmath.py`, `test_hud.py`, `test_snapshot.py`, `golden_replays.py` 3/3 (újrarögzítve a `dmath` után), `snapshot_check.py --all-missions` 46/46 |
| Környezet | Windows 10, i7-6700K, RX 550, 32 GB; Python 3.13.14, pygame-ce 2.5.8, numpy 2.5.4; .NET SDK 10.0.404, GCC 16.2 (WinLibs), CMake 4.4.4 – betöltés: `. tools/fejlesztoi_kornyezet.ps1` |
| Következő konkrét lépés | M0: fajonkénti viselkedési specifikáció a Python-kódból; ADR-003 (RHI), ADR-004 (számábrázolás); utána M2 determinisztikus mag |

## Nyitott döntések (felhasználó)

1. ~~Verziókezelés~~ – eldöntve: helyi Git (2026-11-11).
2. **Licencütközés:** a `LICENSE.md` szerint a kód MIT, a C++/C# tanulmány (v1.1) „GPLv2+ TOTU-kódról” beszél, és erre építi a MonoGame-kizárást.
3. **Motorirány:** a munkajegyzék Python + Arcade felé visz, a délután érkezett v1.1 csomag C++/C# migrációt vizsgál. S01–S03-at nem érinti, S04 előtt el kell dönteni.
4. **Szimulációs frekvencia:** a játék `sim_hz = 30`, a követelmény 20 Hz-es hiteles ticket ír. Átálláskor minden időzítőt át kell számolni (S04).
