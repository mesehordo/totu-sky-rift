# ADR-009: Hálózat – lockstep relével (M10)

**Státusz:** ELFOGADOTT – a tulajdonos döntése 2026-11-15 (D4 lezárva)
**Érintett:** M10 (hálózat), M11 (kooperatív küldetés), M13 (modlock a lobbiban), M14 (dedikált relé kiadása).
Alap: ADR-004 (fixpontos számábrázolás), ADR-007 (a parancssor és az állapothash), D6 (30 Hz).

## Probléma

Az átfogó tanulmány (v2.0, 2026-11-11, 9. és 11.1. fejezet) szerverautoritatív kezdő modellt javasolt (a szerver
számol, a kliens látótér szerint szűrt, delta-kódolt állapotot kap). Indokai: a köd titkossága, a gyenge kliensgép
és a könnyebb újracsatlakozás. A lockstepet méréssel kellett volna összevetni. A tanulmány még a Python-motorhoz
készült. A C++ motor azóta teljesen determinisztikus (fixpont, saját Lua `math`, minden rendszeren egyező, rögzített
hash), és egy tick 1600 egységnél is belefér a keretbe. Így a lockstep olcsó, a szerverautoritatív modell viszont
állapotküldést, tömörítést, interpolációt és erős szervert kívánna.

## Referenciák

- **Warcraft 3:** lockstep; a parancsokat a gazda vagy a Battle.net továbbítja.
- **Age of Empires 2:** egyenrangú gépek közötti lockstep (*1500 Archers on a 28.8*); a DE-ben relészerverek továbbítják a parancsokat.
- **0 A.D.:** lockstep; a gazda szerverként továbbít (ENet felett), újracsatlakozáskor a teljes állapotot küldi, és a
  hash-eltérést jelzi.
- **OpenRA:** lockstep, dedikált szerver mint relé.

## Döntés

1. **Determinisztikus lockstep relével.** Minden kliens a teljes szimulációt futtatja; a relé (gazda vagy dedikált
   folyamat) csak sorba rendezi és továbbítja a parancsokat. Ugyanazt a szimulációt maga is futtatja (fej nélkül):
   így ellenőrzi a hash-t, és újracsatlakozáskor állapotot küld.
2. **Fordulók:** 1 forduló = 3 tick (100 ms, 30 Hz mellett); a parancs az N+2. fordulóban hajtódik végre (mint a régi
   játékban és az AoE-ben). A fordulóhossz rögzített (a játékidő így nem változik); a lassú klienst a relé bevárja
   (25 forduló, azaz 2,5 s lemaradás felett nem zár újat – 0 A.D., OpenRA).
3. **Szinkronhiba-észlelés:** a kliensek fordulónként küldik az állapothash-t; a relé összeveti, eltérésnél jelez, és
   menti a mentést és a visszajátszást a hibakereséshez.
4. **Újracsatlakozás:** a relé mentést küld (a meglévő, mind-vagy-semmi `Sim::save`/`load`), utána a fordulók
   parancsait.
5. **Megfigyelő:** parancs nélküli kliens, teljes látással vagy egy játékos nézetével.
6. **Modlock:** a lobbiban a tartalom- és szabályhash egyezése a csatlakozás feltétele.
7. **Átvitel:** UDP az **ENet** könyvtárral (MIT, rögzített verzió, változatlan forrás a `third_party` alatt; a 0 A.D.
   is ezt használja); a lobbi és a hálózati réteg az átviteltől elválasztva, a próbákhoz memóriabeli átvitellel.

## Következmények

- A térképcsalás elvben lehetséges (minden kliensen ott az állapot). Ez ellen véd a relé saját szimulációja (a
  szabálytalan parancs eldobása), és később a visszajátszás-elemzés.
- A gyenge kliensnek is a teljes szimulációt kell futtatnia; a mérés (M5, M7) szerint ez a C++ motorban belefér.
- A tanulmány által kért összehasonlító mérés helyett a lockstep terhelését mérjük (NET-LOSS: 0–5% csomagvesztés,
  10–200 ms RTT, 2/4/8 kliens).
