# Fejlesztési állapot

Irányadó: [UJ_MOTOR_TERV.md](../alap/UJ_MOTOR_TERV.md) (M0–M14) és [DONTESEK.md](../alap/DONTESEK.md); a részletek a
[MUNKANAPLO.md](MUNKANAPLO.md)-ban.

| Mező | Érték |
|---|---|
| Kész | M0–M11; M12 (győzelmi feltételek, tempóprofilok, küldetéssor-választás) |
| Folyamatban | anti-slop minőségbiztosítás ([docs/quality/](../quality/)) |
| Hátra | M13 (modrendszer: betöltés, modkezelő, ZIP, mintamodok, SDK, küldetés és győzelmi feltétel modból kész; hátra: mod.io), M14 (kiadás), teljes angol fordítás, a két BSD a CI-ben, végső összevetés a tanulmánnyal; **legutoljára** a hadjárat (D47, D48) |
| Utolsó ellenőrzött állapot | 2026-11-18: CTest 9/9 (core, golden M2–M7, net_loss_rovid, ai_batch) |
| Környezet | Windows 10, i7-6700K, RX 550, 32 GB; GCC 16.2 (WinLibs UCRT), CMake 4.4, SDL 3.4 |

A régi Python-játék hibalistája (`HIBAK_ES_FELADATOK.md`) a régi kódra vonatkozik; az új motor hibái és javításai a
munkanaplóban és a [quality/ANTI_SLOP_AUDIT.md](../quality/ANTI_SLOP_AUDIT.md)-ban vannak.
