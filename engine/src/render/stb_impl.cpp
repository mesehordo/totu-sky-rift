// SPDX-License-Identifier: GPL-3.0-or-later
// A beépített stb_image és stb_image_write (MIT / közkincs, engine/third_party/stb, változatlan) megvalósítása –
// csak PNG, fájlkezelés nélkül (a motor maga olvas és ír). Saját figyelmeztetés-beállítással fordul (CMake).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image.h"
#include "stb_image_write.h"
