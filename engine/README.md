# TOTU-motor (C++20)

Az új motor nulláról, a [motorterv](../docs/alap/UJ_MOTOR_TERV.md) szerint. Állapot: **M1 platform-váz**.

```
cmake -S engine -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
build/totu --version
```

Ha a rendszeren nincs SDL3, a CMake a rögzített kiadást (3.4.20) forrásból fordítja. Célrendszerek: Windows, Linux,
macOS (Intel és Apple Silicon), FreeBSD, OpenBSD – mindegyiket a `.github/workflows/motor.yml` ellenőrzi.

Licenc: GPL-3.0-or-later (lásd [`COPYING`](../COPYING) és [`LICENSE.md`](../LICENSE.md)).
