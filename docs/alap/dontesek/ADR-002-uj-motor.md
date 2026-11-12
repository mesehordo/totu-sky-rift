# ADR-002: Új motor nulláról – C++20 és Lua

**Státusz:** ELFOGADOTT – a tulajdonos döntése 2026-11-12 (D18, D19)
**Felülírja:** [ADR-001](ADR-001-motor.md) (C# .NET 10)

## Probléma

Az új követelmények ([UJ_MOTOR_TERV.md](../UJ_MOTOR_TERV.md) K1–K7): 0 A.D. + OpenRA szintű modolás, WC3-szintű
küldetések, és minden rendszer, amin a 0 A.D. fut (Windows, Linux, macOS Intel/ARM, FreeBSD, OpenBSD).

## Lehetőségek

| | C++20 | C# (.NET 10) |
|---|---|---|
| OpenBSD | fut (a 0 A.D. is C++) | **nincs .NET** |
| FreeBSD | fut | csak közösségi port |
| Teljesítmény (A/B mérés, `AB_EREDMENY.md`) | 4–12%-kal gyorsabb, 4× kevesebb memória | GC-szünetek (legrosszabb tick 128 ms) |
| Fejlesztési sebesség | lassabb, több hibalehetőség (sanitizerek, tesztek kellenek) | gyorsabb |
| Minta | 0 A.D. (Pyrogenesis) | OpenRA |

Szkriptnyelv: **Lua 5.4**. Ezt használja a WC3 Reforged és az OpenRA küldetésrendszere; kicsi, mindenhol fordul,
jól sandboxolható. A JavaScript (0 A.D., SpiderMonkey) nagy, és nehezen frissíthető.

## Döntés

C++20 motor, Lua 5.4 szkript. A modok nem hoznak natív kódot. Az egységenként tickenként futó munka C++-ban, a
szabályok és események Luában. A szimuláció fixpontos/egész számokkal, determinisztikus (ADR-004).

## Következmények

- A C#-port terve (PORT_TERV P1–P9) nem indul; a P0 a Python-referenciában marad.
- A fordítás minden rendszeren CI-t kíván (nyitott tulajdonosi kérdés: kódtárhely, tesztgépek).
- A C++ hibái ellen: ASan/UBSan a Linux-CI-ben, fordítói figyelmeztetések hibaként, fuzzolás a betöltőkre.
