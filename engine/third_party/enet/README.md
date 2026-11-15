# ENet 1.3.18 (beépítve)

- Forrás: a hivatalos tároló kiadási címkéje, `https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz`
  (SHA-256: `28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd`): a `*.c` fájlok, az `include/enet/`
  fejlécek, a `LICENSE` és a `ChangeLog`; a fordítási és dokumentációs fájlok nélkül.
- Licenc: MIT (`LICENSE`). A fájlok **változatlanok**.
- Szerep: megbízható, sorrendtartó UDP-átvitel a hálózati réteg alatt (ADR-009); ugyanezt használja a 0 A.D. is.
- Fordítás: `totu_enet` statikus könyvtár (`engine/CMakeLists.txt`); Windowson a `ws2_32` és a `winmm` kell.
- Frissítés: az új kiadás ugyanezen fájljait cseréld, majd futtasd a hálózati próbákat.
