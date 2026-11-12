"""A motor rögzített szögtáblázatainak előállítása (ADR-004).

    python tools/gen_fixed_tables.py      → engine/src/core/angle_tables.inc

A táblázatok egyszer készülnek, és a kód részeként a repóba kerülnek: így a szimuláció szögfüggvényei sosem
függnek a fordító vagy a platform matematikai könyvtárától. Újragenerálni csak a formátum változásakor kell
(az eredmény akkor is ugyanaz, mert a kerekítés egész számra történik, és a Python `math` pontossága bőven elég).
"""
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "engine", "src", "core", "angle_tables.inc")

SIN_SEGMENTS = 1024          # negyedhullám szakaszai
CORDIC_STEPS = 24
CORDIC_UNITS = 1 << 24       # teljes kör a CORDIC-akkumulátorban (a végén 2^16-ra kerekítve)


def main():
    sin_q = [round(math.sin(i * (math.pi / 2) / SIN_SEGMENTS) * 65536) for i in range(SIN_SEGMENTS + 1)]
    atan_q = [round(math.atan(2.0 ** -i) / (2 * math.pi) * CORDIC_UNITS) for i in range(CORDIC_STEPS)]
    lines = [
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "// GENERÁLT FÁJL – tools/gen_fixed_tables.py. Kézzel ne szerkeszd.",
        f"// kSinQuarter: sin(i * (pi/2) / {SIN_SEGMENTS}) * 65536, i = 0..{SIN_SEGMENTS} (Q16.16)",
        f"// kCordicAtan: atan(2^-i) / (2*pi) * 2^24, i = 0..{CORDIC_STEPS - 1}",
        f"constexpr int kSinSegments = {SIN_SEGMENTS};",
        f"constexpr int kCordicSteps = {CORDIC_STEPS};",
        f"constexpr int32_t kSinQuarter[{SIN_SEGMENTS + 1}] = {{",
    ]
    for k in range(0, len(sin_q), 12):
        lines.append("    " + ", ".join(str(v) for v in sin_q[k:k + 12]) + ",")
    lines.append("};")
    lines.append(f"constexpr int64_t kCordicAtan[{CORDIC_STEPS}] = {{")
    for k in range(0, len(atan_q), 8):
        lines.append("    " + ", ".join(str(v) for v in atan_q[k:k + 8]) + ",")
    lines.append("};")
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{OUT}: {len(sin_q)} szinuszérték, {len(atan_q)} CORDIC-szög")


if __name__ == "__main__":
    main()
