# F3 A/B mérés – közös szimulációs mag

Ugyanaz az algoritmus három nyelven: `python/abcore.py` (referencia), `csharp/Program.cs` (.NET 10), `cpp/main.cpp`
(C++20). A szabályok a `docs/alap/SZABALYOK.md` 3. fejezetét követik; az alábbi lista a pontos, közös változat.
**Azonos bemenetre mindhárom bitre azonos állapothash-sorozatot ad** (ellenőrizve), tehát ugyanazt számolják.

## Futtatás

```
python tools/ab/export_scenario.py --units 1600 --out tools/ab/scenarios/center_1600.json
. tools/fejlesztoi_kornyezet.ps1                                   # PowerShell: dotnet, g++, cmake
dotnet build tools/ab/csharp -c Release -o tools/ab/csharp/bin_out
cmake -S tools/ab/cpp -B tools/ab/cpp/build -G "MinGW Makefiles"; cmake --build tools/ab/cpp/build
python tools/ab/run_ab.py --sizes 250,500,1000,1600,2400 --ticks 5400 --reps 3 --langs csharp,cpp
```

## Bemenet (`totu-ab-scenario`, séma 1)

`w`, `h`, `block` (base64, csempénként 0 = járható), `hz` (30), `ticks`, `types` (sugár, sebesség px/s, életerő,
páncél, sebzés, hatótáv px, újratöltés s, lövedék?, lövedéksebesség, területi sugár, látótáv px, egységtípus-index),
`matrix` (9×9 típusszorzó), `units` (csapat, típus, x, y), `dest` (közös cél). Forrás: a valódi játék `nyolc_tuz`
pályája és a 7 főfaj földi harci egységei (84 típus); 8 csapat a saját kezdőhelyén.

## Egy tick

1. `tick += 1`; térbeli rács újraépítése (128 px-es cellák, egységek azonosító szerinti sorrendben).
2. Útkérés-sor: tickenként 6 egységnyi keret, újratervezés 3-at ér.
3. Egységek (azonosító szerint): újratöltés csökken; ha az alcél halott vagy messzebb van, mint látótáv × 1,6 →
   nincs alcél. Alcél nélkül 0,4 s-onként célkeresés a látótávon belül (legkisebb széltávolság, csak látható
   csempén álló ellenség); ha nincs, haladás a közös cél felé (attack-move). Alcéllal: hatótávon belül (széltávolság ≤
   hatótáv + 3) támadás, különben üldözés, 0,8 s-onként új útkéréssel.
4. Lövedékek: követik a célt; becsapódáskor sebzés, területi sebzésnél a sugáron belül 50%.
5. Szétválasztás: az átfedő párok átfedésének fele, a mozgó egység tér ki; falba nem tol.
6. Halottak törlése; 6 tickenként látótér-újraszámolás csapatonként (csempeközép a látókörön belül).

Sebzés: `max(0,5, sebzés × típusszorzó − páncél)`.
Útkeresés: egyenes vonal (4 px-es mintavétel, sugárral) → különben 8 irányú A* (sugaranként gyorsítótárazott
csempe-járhatóság, átló csak szabad oldalcellákkal, oktilis heurisztika, legfeljebb 9000 csomópont, elérhetetlen
célnál a legközelebbi elért csempe) → simítás 6 pontig. Újratervezéskor a 4 csempén belüli egységek akadályok
(512 csomópontos keresés). Útkövetés: falnál újratervezés; egymásba lépés helyett várakozás; 0,8 s (+0,15 s ×
újratervezés) után haladásellenőrzés.

## Bitazonosság feltételei

Csak `+ - * / sqrt floor` (nincs `hypot`, `sin`, `cos`); a kupac `(f, g, csomópont)` szerint rendez; a
lekérdezések cellasorrendje rögzített; a C++ `-ffp-contract=off`-fal fordul (nincs FMA-összevonás).

## Eltérés a valódi játéktól (szándékos egyszerűsítés)

Nincs gazdaság, képesség, elem, pajzs, faji szabály, repülő egység, épület-célpont, kitérés (`_steer`) és formációs
megállás. A mag a mért szűk keresztmetszetet (útkeresés, mozgás, ütközés, célkeresés) modellezi, nem a teljes TOTU-t.
Az elérhetetlen középpont (nagy egységeknek a kráter miatt) szándékosan maradt benne: a valódi játékban is így van
(P-002), és ez a legdrágább eset.
