# ADR-003: A megjelenítő-réteg

**Státusz:** ELFOGADOTT – a tulajdonos jóváhagyta 2026-11-12
**Érintett:** M5 (megjelenítő), K4 (öt rendszer), K6 (grafika, árnyék, shader, effekt). Felülírja a D2-t (raylib-cs).

## Probléma

Egy 2D sprite-os, 3/4 nézetű RTS megjelenítője kell, amely mind az öt célrendszeren fut (Windows, Linux, macOS
Intel/ARM, FreeBSD, OpenBSD), a régebbi grafikus kártyákon is, és hosszú távon sem függ elavuló API-tól (a macOS
OpenGL-je elavult). Igény: atlaszos sprite-rajzolás nagy tételben (1600+ egység 60 FPS-en), csapatszín-shader,
árnyék, köd, víz, fény, részecskék, utófeldolgozás (render-célpontok), interpoláció.

## Lehetőségek

| Lehetőség | Előny | Hátrány |
|---|---|---|
| **A. Saját vékony RHI, első háttér OpenGL 3.3 core, második SDL3 GPU (Vulkan/Metal/D3D12)** | a GL 3.3 mindenhol van (OpenBSD, régi gépek); az SDL3 GPU a már használt SDL3 része, és megoldja a macOS-t (Metal) és a modern gépeket; a 2D-hez kevés funkció kell, a réteg kicsi | két háttér karbantartása; shader-fordítási lánc kell |
| B. sokol_gfx | kiforrott, kicsi, zlib; GL, Metal, D3D11 | a GL-háttér újabb kiadásai a forrásaink szerint GL 4.1/4.3-at kívánhatnak (nem egyértelmű) – veszélyezteti a 3.3-as alapot; saját ablakkezelést feltételez |
| C. bgfx | sok háttér | nagy, saját fordítási rendszer; a BSD-támogatás bizonytalan |
| D. csak SDL3 GPU | egy API, modern | nincs OpenGL-háttere: régi gépek és az OpenBSD (Vulkan-támogatás korlátozott) kiesnek |
| E. raylib | egyszerű, a rajzolási próba ezzel készült | csak OpenGL; kevés kontroll a renderelési menetek felett |

## Döntési javaslat: A

1. **RHI-felület** a motorban (`engine/src/render/rhi/`): textúra, puffer, shader-program, render-célpont,
   rajzolási parancslista – csak ami a 2D renderelőhöz kell.
2. **1. háttér: OpenGL 3.3 core** (M5) – az alap minden rendszeren.
3. **2. háttér: SDL3 GPU** (M5 után) – Vulkan, Metal, Direct3D 12; macOS-en ez lesz az alapértelmezés.
4. **Shaderek:** egy forrásnyelv (GLSL 330), ebből a fordítási lánc készíti a többi formát (SPIR-V, MSL, DXIL –
   pl. glslang + SPIRV-Cross vagy SDL_shadercross); a modok csak a motor által adott shaderkészletből választanak
   (biztonság, D9).
5. **Kapu (M5):** 1600 egység 60 FPS-en (p99 ≤ 16,7 ms) a fejlesztői gépen GL 3.3-mal; a CI-ben mind az öt
   rendszeren fordul; a 2. háttér ugyanazt a képet adja (képernyőkép-összevetés).

## Következmények

- A raylib-próba (`tools/ab/render/`) mérése érvényes kiinduló adat; a végleges renderelő saját kód.
- A D2 (raylib-cs) felülírva, ha a javaslat elfogadott.

## Megvalósítás (M5) – lépések

1. Grafikus réteg (OpenGL 3.3 core): saját függvénybetöltő, textúra, shader, render-célpont, példányos sprite-köteg,
   csapatszín-shader (a meglévő „shade” mód), PNG (stb_image), képernyőkép, mérés; Linux-képpróba a CI-ben (Mesa).
2. Atlaszkészítő: a sprite-lapok vágva, determinisztikusan 4096²-es lapokra, igény szerinti betöltés, minőségi lépcső
   (a HD lap felezése gyengébb kártyán).
3. Világnézet: kamera, terep átmenetekkel és a magaslat festett peremével (D33), Y-rendezés, 8 irány, animáció,
   interpoláció a tickek között, alakzatos tartalék a sprite nélküli egységeknek.
4. Fények: napszaki tónus és a napszakkal forgó árnyék (D35), fényforrások, köd, víz.
5. Effektek: részecskék, időjárás (D35), utófeldolgozás.
6. Kapu: 1600 egység p99 ≤ 16,7 ms; képernyőkép-összevetés a CI-ben; mind az öt rendszeren fordul.

**Állapot (2026-11-12):** mind a hat lépés kész, a kapu teljesül – 2 × 800 egység éjszakai zivatarban, csatában, a
fejlesztői gépen (RX 550, 1280 × 720): a rajzolás p99 7,3 ms (1,5-ös nagyításnál), 4,6 ms (1,0-snál); a CI Linux-
feladata négy képpróbát vet össze a tárolt referenciaképekkel (, Mesa szoftveres OpenGL), és
a motor mind az öt rendszeren (hat feladatban) fordul és tesztel. A referenciaképek frissítése: a CI 
műtermékéből, szándékos látványváltozáskor (a változással együtt commitolva).
