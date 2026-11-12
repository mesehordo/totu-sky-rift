"""A determinisztikus matematika (rts/dmath.py) ellenőrzése.

    python tools/test_dmath.py                 # pontosság a platform-matematikához képest
    python tools/test_dmath.py --vectors OUT   # referencia-vektorok a C#-paritáshoz (P1)

A pontosság mértéke ULP-ben (a lebegőpontos szám utolsó helyiértéke). A játékhoz néhány ULP bőven elég; a lényeg,
hogy a C#-átirat ugyanazokra a bemenetekre bitre ugyanazt adja – ezt a --vectors kimenet ellenőrzi majd.
"""
import argparse
import json
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from rts import dmath  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def ulp_diff(a, b):
    if a == b:
        return 0
    ia = struct.unpack("<q", struct.pack("<d", a))[0]
    ib = struct.unpack("<q", struct.pack("<d", b))[0]
    if ia < 0:
        ia = -(ia & 0x7FFFFFFFFFFFFFFF)
    if ib < 0:
        ib = -(ib & 0x7FFFFFFFFFFFFFFF)
    return abs(ia - ib)


def angles(rnd, n):
    out = [0.0, -0.0, math.pi, -math.pi, math.pi / 2, math.tau, 1e-9, -1e-9, 1000.0, -1000.0]
    for _ in range(n):
        k = rnd.random()
        if k < 0.6:
            out.append(rnd.uniform(-math.tau, math.tau))
        elif k < 0.9:
            out.append(rnd.uniform(-200.0, 200.0))
        else:
            out.append(rnd.uniform(-1e5, 1e5))
    return out


def pairs(rnd, n):
    out = [(0.0, 0.0), (0.0, -0.0), (-0.0, 0.0), (-0.0, -0.0), (1.0, 0.0), (-1.0, 0.0), (0.0, 1.0), (0.0, -1.0),
           (1.0, 1.0), (-1.0, -1.0), (3.0, -4.0), (1e-300, 1.0), (1.0, 1e-300), (-5.0, -1e-12)]
    for _ in range(n):
        s = 10 ** rnd.uniform(-4, 4)
        out.append((rnd.uniform(-1, 1) * s, rnd.uniform(-1, 1) * s))
    for _ in range(n // 4):                  # egész csempe-/pixelkülönbségek, mint a játékban
        out.append((float(rnd.randint(-5000, 5000)), float(rnd.randint(-5000, 5000))))
    return out


def check(n):
    rnd = random.Random(20261112)
    worst = {}
    for x in angles(rnd, n):
        for name, f, g in (("sin", dmath.sin, math.sin), ("cos", dmath.cos, math.cos)):
            ref = g(x)
            u = ulp_diff(f(x), ref) if abs(ref) > 1e-6 else 0
            ab = abs(f(x) - ref)
            w = worst.setdefault(name, [0, 0.0])
            w[0] = max(w[0], u)
            w[1] = max(w[1], ab)
    for y, x in pairs(rnd, n):
        for name, val, ref in (("atan2", dmath.atan2(y, x), math.atan2(y, x)),
                               ("hypot", dmath.hypot(x, y), math.hypot(x, y)),
                               ("dist", dmath.dist((x, y), (y, x)), math.dist((x, y), (y, x)))):
            w = worst.setdefault(name, [0, 0.0])
            if ref != 0.0 and abs(ref) > 1e-290:
                w[0] = max(w[0], ulp_diff(val, ref))
            w[1] = max(w[1], abs(val - ref))
            if name == "atan2" and (val == 0.0) and math.copysign(1, val) != math.copysign(1, ref):
                raise SystemExit(f"atan2({y!r}, {x!r}) előjeles nulla eltér: {val!r} vs {ref!r}")
    ok = True
    for name, (u, ab) in sorted(worst.items()):
        limit = 4
        status = "rendben" if u <= limit or ab < 1e-12 else "HIBA"
        ok &= status == "rendben"
        print(f"{name:6} legnagyobb eltérés: {u} ULP, abszolút {ab:.3g}  → {status}")
    return ok


def vectors(path, n):
    rnd = random.Random(7)
    out = {"format": "totu-dmath-vectors", "schema": 1, "sin": [], "cos": [], "atan2": [], "hypot": []}
    hx = lambda v: struct.unpack("<Q", struct.pack("<d", v))[0]  # noqa: E731
    for x in angles(rnd, n):
        out["sin"].append([hx(x), hx(dmath.sin(x))])
        out["cos"].append([hx(x), hx(dmath.cos(x))])
    for y, x in pairs(rnd, n):
        out["atan2"].append([hx(y), hx(x), hx(dmath.atan2(y, x))])
        out["hypot"].append([hx(x), hx(y), hx(dmath.hypot(x, y))])
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f)
    print(f"{sum(len(v) for v in out.values() if isinstance(v, list))} vektor → {path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=200000)
    ap.add_argument("--vectors")
    a = ap.parse_args()
    if a.vectors:
        vectors(a.vectors, a.n)
        return
    if not check(a.n):
        raise SystemExit(1)
    print("dmath rendben")


if __name__ == "__main__":
    main()
