# Regressziós mátrix

Kelt: 2026-11-18. Minden anti-slop commit előtt: ✔ = kötelező.

| Ellenőrzés | Eszköz | Mikor | Alap (c840c29) |
|---|---|---|---|
| Fordítás (mag, tesztek, kliens, relé) | CMake | ✔ mindig | PASS |
| `totu_tests` (364+ próba, köztük `test_hud`, `test_menu`, `test_editor`, `test_net`) | CTest `core` | ✔ mindig | PASS |
| Golden visszajátszások M2, M3, M4, M6, M7 – bitre azonos hash | CTest `golden_replay*` | ✔ mindig | PASS |
| Hálózati veszteség (rövid) | CTest `net_loss_rovid` | ✔ mindig | PASS |
| Gépi meccsek (rövid, alap) | CTest `ai_batch_*` | ✔ mindig | PASS |
| HUD-geometria, parancsrács WERT/SDFG/XCVB, kattintás–billentyű azonosság | `test_hud` | ✔ HUD-változásnál | PASS |
| Beállításfájl oda-vissza, hibatűrés | `test_menu` | ✔ beállításnál | PASS |
| Képek: HUD 1280/1920/2560, hét faj, menüképernyők | `totu --screenshot` | ✔ látható változásnál, előtte–utána | `kepek/alap_*` |
| Windows, Linux, macOS (Intel, Apple Silicon) | GitHub CI | push után | a CI szerint |
| FreeBSD, OpenBSD | – | – | NOT_RUN (nincs a CI-ben) |
| Teljesítmény (képkocka p50/p95/p99, tick) | `--bench`, `totu_netbench` | diagnosztikánál, nagyobb rajzolási változásnál | NOT_MEASURED |
