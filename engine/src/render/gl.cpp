// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/gl.hpp"

#include <cstring>

namespace totu::render::gl {

bool load(Api& api, Loader loader, std::string* missing) {
  bool ok = true;
#define TOTU_GL_LOAD(ret, name, args)                                                    \
  {                                                                                      \
    void* p = loader("gl" #name);                                                        \
    if (!p) {                                                                            \
      if (ok && missing) *missing = "gl" #name;                                          \
      ok = false;                                                                        \
    }                                                                                    \
    static_assert(sizeof(p) == sizeof(api.name), "függvénymutató mérete");               \
    std::memcpy(&api.name, &p, sizeof(p));                                               \
  }
  TOTU_GL_FUNCS(TOTU_GL_LOAD)
#undef TOTU_GL_LOAD
  return ok;
}

}  // namespace totu::render::gl
