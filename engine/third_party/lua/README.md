# Lua 5.4.8 (beépítve)

- Forrás: a hivatalos kiadás, `https://www.lua.org/ftp/lua-5.4.8.tar.gz`
  (SHA-256: `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`), a `src/` mappából,
  a `lua.c` és a `luac.c` nélkül. A fájlok tartalma egyezik a `github.com/lua/lua` tükör `v5.4.8` címkéjével.
- Licenc: MIT (`LICENSE`). A fájlok **változatlanok**.
- Fordítás: nem önállóan, hanem egyetlen C++ fordítási egységként: `engine/src/script/lua_build.cpp`.
  A determinizmushoz szükséges eltéréseket ott, makrókkal adjuk meg (rögzített hash-mag, rögzített
  rendezési pivot, saját hatványozás, bájtsorrendi szöveg-összehasonlítás); az `io`, `os`, `package`,
  `debug` és `math` könyvtár nem kerül be.
- Frissítés: az új kiadás `src/` fájljait cseréld, majd a `lua_build.cpp` makróit ellenőrizd
  (`luai_makeseed`, `l_randomizePivot`, `luai_numpow`, `strcoll`).
