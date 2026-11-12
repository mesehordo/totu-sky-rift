// SPDX-License-Identifier: GPL-3.0-or-later
// A beépített Lua 5.4.8 (MIT, engine/third_party/lua) egyetlen fordítási egységként, C++-ként fordítva.
// C++-ként a Lua-hibák kivételek (nem longjmp), így a motor C++ kódjában a destruktorok mindig lefutnak.
// A Lua fájljai változatlanok; a determinizmushoz szükséges eltéréseket itt, makrókkal adjuk meg:
//   - luai_makeseed: rögzített hash-mag (alapból idő + címek → a táblák belső sorrendje gépenként eltérne);
//   - l_randomizePivot: rögzített érték (alapból clock() + time() a table.sort pivotjához);
//   - luai_numpow: saját hatványozás (a libm pow() rendszerenként eltérhet): egész kitevő ismételt szorzással,
//     0,5 = sqrt (az IEEE szerint pontos), más kitevő hiba;
//   - strcoll → strcmp: a szöveg-összehasonlítás bájtsorrendű, nem a területi beállítástól függ.
// Csak a mag, a segédkönyvtár és a base/coroutine/string/table/utf8 könyvtár kerül be; az io, os, package,
// debug és math nem (a math-ot a script/host.cpp adja, determinisztikusan).
#include "lprefix.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LUA_CORE
#define LUA_LIB
#define ltable_c
#define lvm_c
#include "luaconf.h"

// a belső függvények ne legyenek kifelé látható szimbólumok (mint a Lua saját onelua.c-jében)
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC static
#define LUAI_DDEC(def) /* üres */
#define LUAI_DDEF static

#define luai_makeseed(L) (static_cast<unsigned int>(0x544F5455u))
#define l_randomizePivot() (0x9E3779B9u)
#define strcoll strcmp

struct lua_State;
static double totu_numpow(struct lua_State* L, double a, double b);
#define luai_numpow(L, a, b) totu_numpow(L, a, b)

#include "lzio.c"
#include "lctype.c"
#include "lopcodes.c"
#include "lmem.c"
#include "lundump.c"
#include "ldump.c"
#include "lstate.c"
#include "lgc.c"
#include "llex.c"
#include "lcode.c"
#include "lparser.c"
#include "ldebug.c"
#include "lfunc.c"
#include "lobject.c"
#include "ltm.c"
#include "lstring.c"
#include "ltable.c"
#include "ldo.c"
#include "lvm.c"
#include "lapi.c"
#include "lauxlib.c"
#include "lbaselib.c"
#include "lcorolib.c"
#include "lstrlib.c"
#include "ltablib.c"
#include "lutf8lib.c"

static double totu_numpow(struct lua_State* L, double a, double b) {
  if (b == 0.5) return sqrt(a);
  if (b == floor(b) && fabs(b) <= 1073741824.0) {
    // bináris hatványozás: minden lépés egyetlen IEEE-szorzás, így minden gépen ugyanaz az eredmény
    long long n = static_cast<long long>(fabs(b));
    double base = a, r = 1.0;
    while (n > 0) {
      if (n & 1) r *= base;
      base *= base;
      n >>= 1;
    }
    return b < 0 ? 1.0 / r : r;
  }
  luaG_runerror(L, "a hatványozás kitevője csak egész vagy 0.5 lehet (determinizmus)");
  return 0.0;
}
