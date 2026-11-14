"""Az M7 leltár-kapuja (ADR-008): a tartalmi leltár (docs/alap/tartalom_leltar.json) minden sorához tartozik-e
besorolás a docs/alap/leltar_kapu.json-ban, és a hivatkozott próbák (TEST_CASE) léteznek-e.

    python tools/leltar_kapu.py            # összesítés és a hiányzó sorok
    python tools/leltar_kapu.py --check    # 1-es kilépés, ha sor nincs besorolva vagy próbanév nem létezik
    python tools/leltar_kapu.py --gate     # 1-es kilépés, ha még van „hianyzik” sor (a kapu teljesülése)

Besorolások: teszt (próba gyakorolja), atfedes (más néven/szerkezetben él, próbával), megjelenites (csak kliens),
halott (a régi kód sem használja), hianyzik (megvalósítandó vagy próba nélküli).
"""
import collections
import glob
import json
import os
import re
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rows():
    d = json.load(open(os.path.join(ROOT, "docs", "alap", "tartalom_leltar.json"), encoding="utf-8"))
    out = [f"mechanic:{k}" for k in d["mechanics"]] + [f"ability_type:{k}" for k in d["ability_types"]]
    for cat, v in d["vocabulary"].items():
        out += [f"{cat}:{k}" for k in v]
    return out


def test_names():
    names = set()
    for p in glob.glob(os.path.join(ROOT, "engine", "tests", "**", "*.cpp"), recursive=True):
        s = open(p, encoding="utf-8").read()
        for m in re.finditer(r'TEST_CASE\("((?:[^"\\]|\\.)*)"\)', s):
            names.add(m.group(1))
    return names


def main():
    gate = json.load(open(os.path.join(ROOT, "docs", "alap", "leltar_kapu.json"), encoding="utf-8"))
    names = test_names()
    errors = []
    kinds = collections.Counter()
    missing = []
    for r in rows():
        g = gate.get(r)
        if not g:
            errors.append(f"nincs besorolva: {r}")
            continue
        kind = next((k for k in ("hianyzik", "halott", "megjelenites", "atfedes", "teszt") if k in g), None)
        kinds[kind] += 1
        if kind == "hianyzik":
            missing.append((r, g["hianyzik"]))
        if "teszt" in g and g["teszt"] not in names:
            errors.append(f"nem létező próba ({r}): {g['teszt']}")
    for k in sorted(set(gate) - set(rows())):
        errors.append(f"fölös sor: {k}")
    total = sum(kinds.values())
    print(f"leltár-kapu: {total} sor – " + ", ".join(f"{k} {n}" for k, n in kinds.most_common()))
    for r, why in missing:
        print(f"  hiányzik  {r}: {why}")
    for e in errors:
        print(f"  HIBA  {e}")
    if "--check" in sys.argv and errors:
        return 1
    if "--gate" in sys.argv and (errors or missing):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
