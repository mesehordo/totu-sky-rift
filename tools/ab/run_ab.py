"""F3 A/B mérési sorozat: ugyanaz a forgatókönyv Pythonban, C#-ban és C++-ban, egymás után futtatva.

    python tools/ab/run_ab.py --sizes 250,500,1000,1600,2400 --ticks 900 --reps 1 --langs python,csharp,cpp
    python tools/ab/run_ab.py --sizes 250,500,1000,1600,2400 --ticks 5400 --reps 3 --langs csharp,cpp

Minden futás JSON-ja: docs/fejlesztes/meresek/ab/<lang>_<size>_<ticks>_r<rep>.json.
A végén ellenőrzi, hogy azonos forgatókönyvre és tickre minden nyelv azonos állapothash-t adott-e.
"""
import argparse
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "docs", "fejlesztes", "meresek", "ab")
CMD = {
    "python": [sys.executable, os.path.join(ROOT, "tools", "ab", "python", "abcore.py")],
    "csharp": [os.path.join(ROOT, "tools", "ab", "csharp", "bin_out", "abcore.exe")],
    "cpp": [os.path.join(ROOT, "tools", "ab", "cpp", "build", "abcore.exe")],
}
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sizes", default="250,500,1000,1600,2400")
    ap.add_argument("--ticks", type=int, default=900)
    ap.add_argument("--reps", type=int, default=1)
    ap.add_argument("--langs", default="python,csharp,cpp")
    ap.add_argument("--scenario", default="center")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    results = []
    for size in a.sizes.split(","):
        sc = os.path.join(ROOT, "tools", "ab", "scenarios", f"{a.scenario}_{size}.json")
        for lang in a.langs.split(","):
            for rep in range(1, a.reps + 1):
                out = os.path.join(OUT, (f"{a.scenario}_" if a.scenario != "center" else "") + f"{lang}_{size}_{a.ticks}_r{rep}.json")
                subprocess.run(CMD[lang] + [sc, "--ticks", str(a.ticks), "--out", out], check=True,
                               stdout=subprocess.DEVNULL)
                with open(out, encoding="utf-8") as f:
                    r = json.load(f)
                r["rep"] = rep
                results.append(r)
                t = r["tick_ms"]
                gc = r.get("gc", {})
                print(f"{size:>5} {lang:7s} r{rep}: összesen {r['total_s']:8.2f} s | tick p50 {t['p50']:8.3f} "
                      f"p95 {t['p95']:8.3f} p99 {t['p99']:8.3f} max {t['max']:8.2f} ms | mem {r.get('peak_mem_mib')} MiB"
                      + (f" | GC {gc.get('gen0')}/{gc.get('gen1')}/{gc.get('gen2')}, szünet {gc.get('pause_ms_total')} ms"
                         if gc else ""), flush=True)
    bad = 0
    for size in a.sizes.split(","):
        hs = {(r["lang"], r["rep"]): r["hashes"] for r in results if r["scenario"] == f"{a.scenario}_{size}"}
        ref = next(iter(hs.values()), None)
        same = all(h == ref for h in hs.values())
        bad += not same
        print(f"{size:>5}: állapothash {'AZONOS' if same else 'ELTÉR'} minden futásban ({len(hs)} futás)")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
