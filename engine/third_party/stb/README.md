# stb_image, stb_image_write, stb_truetype, stb_vorbis (beépítve)

- Forrás: `https://github.com/nothings/stb`, rögzített commit `2a34860c372bc520e1521c6aa7e3e4b4138c76f8`
  (2026-10-14): `stb_image.h` v2.30 (PNG-olvasás), `stb_image_write.h` v1.16 (PNG-írás a képernyőképekhez),
  `stb_truetype.h` v1.26 (a játék TTF-betűinek raszterezése a HUD-hoz; sha256
  `2bd60d66b2af1e54b9ce6dc32393de79e66294137b94b44c92d0237099d3f4f3`, 2026-11-13-án felvéve).
- Licenc: kettős – MIT vagy közkincs (Unlicense), választható (`LICENSE`). A fájlok **változatlanok**.
- Fordítás: egyetlen fordítási egységben, `engine/src/render/image.cpp`, csak a PNG-kódolóval
  (`STBI_ONLY_PNG`), a többi formátum kikapcsolva (kisebb támadási felület a modok képeinél).
- `stb_vorbis.c` v1.22 (Ogg Vorbis hangfájlok dekódolása a hanghatásokhoz; ugyanabból a rögzített commitból, sha256
  `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`, 2026-11-20-án felvéve). Fordítás:
  `engine/src/client/audio_decode.cpp`, külön, figyelmeztetések nélküli könyvtárban (`totu_audio_codecs`).
