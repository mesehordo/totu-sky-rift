# TOTU: Sky Rift – A kölcsönkapott öröklét

Kreátor: **Mesehordo** · A világ, a történet, a grafika, a zene és a hangok © 2026 Mesehordo, minden jog fenntartva;
a motor kódja GNU GPL 3 vagy későbbi – részletek: [LICENSE.md](LICENSE.md).

Klasszikus, 2D sprite-os, 3/4 nézetű fantasy valós idejű stratégia: bázisépítés, hősök és tárgyak, aszimmetrikus
népek, hadjárat, modolhatóság a 0 A.D. és az OpenRA szintjén. Mérce: a 0 A.D., az Age of Empires, a Warcraft III és
a StarCraft minősége.

## Állapot

Az új motor (C++20 + Lua) fejlesztés alatt áll – terv és ütemterv: [docs/alap/UJ_MOTOR_TERV.md](docs/alap/UJ_MOTOR_TERV.md).

| Mérföldkő | Állapot |
|---|---|
| M0 Alapozás – tartalmi leltár, viselkedési specifikáció, döntések, ADR-ek | kész |
| M1 Platform-váz – SDL3-ablak, OpenGL 3.3, CI mind az öt rendszeren | kész |
| M2 Determinisztikus mag – fixpontos matematika, tartalomréteg, modrétegek, sablonok, mentés, visszajátszás | kész – a golden visszajátszás mind az öt rendszeren egyezik |
| M3–M14 – Lua, mozgás, megjelenítő, játékmag, fajok, felület, gépi ellenfél, hálózat, küldetések, játékmódok, modrendszer, kiadás | tervezett |

Célrendszerek: Windows, Linux, macOS (Intel és Apple Silicon), FreeBSD, OpenBSD – mindegyiket a CI ellenőrzi.

## A játék

* **Hét főnép:** Szürke elfek, Ursanok, Croftianok, Wullof, Aurethi Birodalom, Huk’ucha, Ooni – mindegyik saját
  mechanikával. A **Micéliák** szövetséges kisnépként és modként érhetők el.
* **Szövetséges kisnépek:** a Diplomácia csarnokában egy választható közülük, és annak három épülete nyílik meg.
* **Nyersanyagok:** Fa, Arany, Meteorit, Áhítat (az elfeknél; a Huk’uchánál Tóének); létszám legfeljebb 200.
* **Felemelkedés:** három különböző kész épület után a nép választ a három fejlődési iránya közül.
* **Hősök** szintekkel, megtanulható képességekkel és tárgyakkal; semleges szörnytáborok.
* **Játékmódok:** hadjárat, győzelmi feltételek, hadjárattérkép (tervezett).

A szabályok pontos leírása: [docs/alap/spec/](docs/alap/spec/) · a tartalom leltára:
[docs/alap/TARTALOM_LELTAR.md](docs/alap/TARTALOM_LELTAR.md).

## Fordítás

```bash
cmake -S engine -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build --parallel
```

```bash
ctest --test-dir build --output-on-failure
```

Részletek: [engine/README.md](engine/README.md). A játéktartalom az új sablonformátumban: `engine/content/`.

## Dokumentáció

| Dokumentum | Tartalom |
|---|---|
| [docs/alap/UJ_MOTOR_TERV.md](docs/alap/UJ_MOTOR_TERV.md) | az új motor terve és ütemterve (M0–M14) |
| [docs/alap/DONTESEK.md](docs/alap/DONTESEK.md) | a tulajdonosi döntések |
| [docs/alap/dontesek/](docs/alap/dontesek/) | műszaki döntések (ADR) |
| [docs/alap/spec/](docs/alap/spec/) | a szabályok viselkedési specifikációja |
| [docs/fejlesztes/](docs/fejlesztes/) | állapot, munkanapló, mérések |
| [docs/tortenet/](docs/tortenet/), [stories/](stories/) | a világ, a történet és a novellák (a játékbeli könyvtár is) |
