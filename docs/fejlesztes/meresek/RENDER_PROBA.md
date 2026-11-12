# Rajzolási próba – raylib-cs, OpenGL 3.3

Kelt: 2026-11-12 · kód: `tools/ab/render/` · nyers adatok: `meresek/render/*.json`.
Gép: i7-6700K, **Radeon RX 550**, Windows 10; Raylib-cs 8.1.0, .NET 10, OpenGL 3.3 (`Rlgl.GetVersion` = `OpenGl33`).
Ablak 1280×720, vsync nélkül (a nyers teljesítményt mérjük), 2 mp bemelegedés után 15 mp mérés.

## Mit rajzolt

- **Valódi TOTU-sprite-ok:** 16 egységtípus HD lapjaiból a walk és attack animáció mind a 8 iránya, 1536 képkocka,
  az átlátszó szél levágásával egy 4096×4096-os atlaszba csomagolva (`build_atlas.py`). 70% sétál, 30% támad.
- Terep csempékből, egységenként árnyék-ellipszis, minden képkockában Y szerinti rendezés és képernyőn kívüli
  kivágás, HUD-váz (alsó sáv, minitérkép minden egységgel, 2×3, 4×3 és 4×3 rács, felső sáv, szöveg).
- Nézetek: **„all”** – minden egység egyszerre a képernyőn (0,3× méret, legrosszabb eset);
  **„typical”** – 4608 px-es világ, a kamera a sereg közepén (0,55× méret).

## Eredmény (képkockaidő ms; 30 FPS = 33,3 ms, 60 FPS = 16,7 ms)

| Nézet | Egység összesen | Látható átl. | p50 | p95 | p99 | legrosszabb | átlag FPS |
|---|---|---|---|---|---|---|---|
| all | 1600 | 1600 | 5,95 | 9,42 | 10,49 | 11,0 | 150 |
| typical | 1600 | 549 | 2,26 | 3,93 | 4,24 | 5,7 | 387 |
| all | 2400 | 2400 | 8,81 | 14,12 | 15,84 | 31,8 | 100 |
| typical | 2400 | 829 | 3,36 | 5,69 | 6,32 | 15,5 | 263 |

## Értelmezés

- A raylib-cs ezen a gépen **minden esetben a 30 FPS-es keret alatt maradt**, még akkor is, ha mind a 2400 egység
  egyszerre látszik (p99 15,8 ms). A tipikus nézet 60 FPS-sel is bőven elfér. **Mért.**
- A célzott minimumgép (Intel HD 4600) GPU-ja becslés szerint 3–4× gyengébb az RX 550-nél. A tipikus nézet ott
  várhatóan a 30 FPS-es keretben marad, a „minden látszik” legrosszabb eset határeset lehet. **Becslés – HD 4600-on
  nem mértük** (D14: nincs tesztgép).
- **A D2 döntés megerősítve:** a raylib-cs alkalmas a TOTU kliensére.

## Amit a próba nem tartalmaz, és a port tervében kezelni kell

- **Atlasz-méret:** a HD sprite-ok nagyok – 4096×4096-os lapra csak 16 egységtípus séta- és támadás-animációja fért.
  A teljes játékhoz fajonként külön lap(ok) és/vagy kisebb felbontású változat kell nagyításhoz; a lapváltás
  megtöri a kötegelést, ezért a rajzolási sorrendet laponként kell csoportosítani.
- Csapatszín (maszk), köd-réteg, épületek, lövedékek, effektek, hang – ezek még nincsenek benne.
- Egy gép, Windows; Linuxon nem mértük.
