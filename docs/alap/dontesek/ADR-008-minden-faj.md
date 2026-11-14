# ADR-008: Minden faj (M7)

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-13 (D43)
**Alap:** ADR-007 (játékmag, M6), D22 (képességmodell), D24 (kutatás), D25 (hősök, tárgyak, táborok, elemek),
D26 (kisnépek), D28 (Micéliák), D30 (elfoglalás, megszállás, előléptetés), D33, D35, D40; a viselkedés forrása a régi
Python-játék és a specifikációk (`spec/FAJMECHANIKAK.md`, `KEPESSEGEK.md`, `HOSOK_TARGYAK_TABOROK.md`,
`KISFAJOK.md`, `ELFOGLALAS_ES_ELOLEPTETES.md`).

## Cél és kapu

A 7 + 1 főfaj (Ursanok, Wullof, Aurethi Birodalom, Ooni, Croftianok, Huk’ucha, Szürke elfek, Micéliák) és a 13 kisnép
teljesen, a hat fajmechanikával, a hősrendszerrel, a táborokkal és a Vadonnal. **Kapu:** a tartalmi leltár minden
sorának viselkedési próbája zöld (`tools/tartalom_leltar.py` sorai ↔ tesztek), és egy golden M7 visszajátszás
(vegyes népek, gépi ellenfelekkel) minden rendszeren azonos.

## Döntések (D43, a tulajdonos 2026-11-13)

1. **Sorrend:** mechanika szerint, a legegyszerűbbtől – előbb a közös rendszerek, utána a népek (lásd a lépéseket).
2. **A régi játék nyilvánvaló hibái** (pl. a bekapcsolt ének a használójára kétszer hat) azonnal javulnak, mindegyik
   a munkanaplóba kerül; balanszszámot az M7 nem módosít (az külön döntés).

## Lépések

1. **Közös képességkeret:** a 13 elem állapotként (a képességek, a fegyverek és a kézi felülírások szerint), a
   lassító és mérgező fegyverek, `blink`, a `gpower` közös hatásai (terület, idézés, szél); a régi hibák (D43). A
   népekhez kötött típusok a népük lépésében (`morph` – Croftianok, `mount`/`dismount` – Wullof, `lay_mine`, áradás –
   Huk’ucha, `overdrive`, `anchor_rescue` – Aurethi Birodalom, `ward` – Ooni, `raise` a tetemekkel, `merge`, a
   `bpower` – Szürke elfek). A D22 új elemei (ellenséges aura, eloszlatás és immunitás, varázslási idő, automatikus
   használat) akkor, amikor az első tartalom használja őket.
2. **Hősök, tárgyak, táborok, Vadon** (D25): tapasztalat, szint, képességtanulás, újraélesztés (Eskü-fa és társai),
   tárgyak, semleges táborok, a Vadon.
3. **Wullof és Aurethi Birodalom** (standard népek: falkaszellem, zsákmány, erdei rejtőzés, vándorszekér; alapozás,
   Reaktorpajzs, Lélekhorgony).
4. **Ooni** (lámpásfény, harci kedv, Taiko-dobos, Torii-kapu).
5. **Croftianok** (mélység: terjedés, építés, mozgás, rothadás, fertőzés, lárvák, tüskék).
6. **Huk’ucha** (tó: ásás, építés, mozgás, áradás, Gyöngybója).
7. **Szürke elfek** (energiamező, behívás, Áhítat).
8. **Micéliák** (fonalgráf, spórák, osztódás; D28).
9. **Kisnépek és szövetség** (D26): a 13 kisnép, a Diplomácia csarnoka, beilleszkedés, kisnép-táborok.
10. **Elfoglalás, megszállás, előléptetés** (D30; a D39 jelzéssel az elfoglalható épületeken).
11. **Lefedettség és kapu:** népenként az M6-os `test_ursan` mintájára adatvezérelt próba (minden egység képezhető,
    minden épület felépíthető, minden kutatás elkészül és hat); a leltár minden sora próbával; golden M7.

## Munkamód

Lépésenként egy commit és egy CI-futás; népenként a közös, adatvezérelt lefedettségi próba (nem népenként kézzel
írt); a gépi ellenfél szkriptje (M6) népenként a nép `ai` adataiból paraméterezve.
