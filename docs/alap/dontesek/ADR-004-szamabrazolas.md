# ADR-004: Számábrázolás a szimulációban

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-12
**Érintett:** M2 (determinisztikus mag), M3 (Lua), M10 (hálózat), replay és mentés.

## Probléma

A szimulációnak minden gépen, minden fordítóval és processzoron (x86-64, ARM) **bitre azonosan** kell futnia: ez a
lockstep-hálózat, a visszajátszás és a többjátékos modegyeztetés alapja. A modokat írók Lua-szkriptjei is a
szimulációban futnak (D19).

## Lehetőségek

| Lehetőség | Előny | Hátrány |
|---|---|---|
| **A. Fixpontos (Q16.16, `int32` tároló, `int64` köztes) + egész világkoordináták csempe-egységben** | bitre azonos minden fordítón és processzoron, külön fegyelem nélkül; a 0 A.D. (CFixed) és az OpenRA (egész világkoordináták) is így működik; a Lua 5.4 64 bites egészei természetesen kezelik | lassabb, mint a lebegőpontos (lásd mérés); a tartományra és a túlcsordulásra figyelni kell |
| B. `double` szigorú szabályokkal (FMA tiltva, saját sin/cos/gyök, mint a P0 `dmath`) | gyors; a P0 és az A/B mérés igazolta egy gépen | minden fordítón, beállításon és könyvtárban őrizni kell; a Lua-szkriptek lebegőpontos számításait is ellenőrizni kellene – modoknál ez nem tartható fenn |

## Mérés (`tools/ab/fixpont/bench.cpp`, `docs/fejlesztes/meresek/fixpont_bench.jsonl`)

2400 egység mozgása és szomszéd-ellenőrzése, 2000 tick, i7-6700K, GCC 16.2 `-O2`:

| Változat | Tickenként | A `double`-höz képest |
|---|---|---|
| `double` | 0,013 ms | 1× |
| fixpontos, bitenkénti egész gyök | 0,111 ms | 8,4× |
| fixpontos, **hardveres gyök + egész korrekció** | **0,057 ms** | 4,4× |

A gyors egész gyök a pontosan kerekített hardveres gyök közelítését egész korrekcióval igazítja a pontos alsó
egészrészre – az eredmény **bitre azonos** a bitenkénti változattal (ellenőrizve). A mozgás tickenkénti költsége így
a 33,3 ms-os keret 0,17%-a; az A/B mérés teljes szimulációs tickje (C++: p50 2–3 ms 2400 egységnél) mellett a
fixpontos többletköltség becslés szerint legfeljebb néhány tized ms.

## Döntési javaslat: A

1. **`Fixed`** típus: Q16.16 (`int32`), szorzás és osztás `int64` köztes értékkel; telítés vagy hibajelzés
   túlcsordulásnál (hibakereső fordításban ellenőrizve).
2. **Világkoordináták** csempe-egységben, `Fixed` pontossággal (1/65 536 csempe); a négyzetes távolság `int64`.
3. **Gyök:** a mérés gyors, pontos egész gyöke. **Szögfüggvények:** rögzített táblázat + interpoláció vagy
   fixpontos polinom (egyszer generálva, a táblázat a kód része).
4. **Idő:** tickekben (egész); másodperc csak az adatban és a megjelenítésben.
5. **Véletlenszám:** saját, rögzített algoritmus (pl. PCG32), játékonként egy mag, rendszerenként külön folyam
   (AI, harc, pályagenerálás), hogy egy új hívás ne tolja el a többit.
6. **Lua:** a szimulációs API csak egészeket és `Fixed` értékeket fogad és ad; lebegőpontos szám a szimulációs
   állapotba nem kerülhet (az API hibát jelez). A megjelenítés (`RenderSnapshot` után) lebegőpontos.
7. **Adat:** a JSON-ban továbbra is tizedes számok állnak (pl. `speed: 2.7`); a betöltő **egyszer, determinisztikusan**
   alakítja `Fixed`-re (egész-aritmetikás tizedes-elemzéssel, nem a platform lebegőpontos átalakítójával).

## Következmények

- A Python-referencia lebegőpontos marad; az új motor és a referencia között a viselkedési próbák tűréssel vetnek
  össze (bitre azonos egyezés nem cél – UJ_MOTOR_TERV 3.4).
- Az M2 kapuja (ugyanaz a replay minden rendszeren ugyanazt a hash-sorozatot adja) erre épül.
