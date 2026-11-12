"""A rögzített golden replay-készlet ellenőrzése: minden ``tests/golden/*.trep`` felvétel visszajátszva
ugyanazokat az állapothash-eket adja-e.

    python tools/golden_replays.py

Ha egy szándékos szabály- vagy adatváltozás (balansz, új mechanika) miatt eltér, a készletet újra kell
rögzíteni (`tests/golden/README.md`), és ezt a munkanaplóba kell írni. Optimalizálás után az eltérés HIBA.
"""
import glob
import os
import sys
import time

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from rts import data as gamedata, replay  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def main():
    files = sorted(glob.glob(os.path.join(ROOT, "tests", "golden", "*.trep")))
    cache, bad = {}, 0
    for f in files:
        mods = tuple(replay.read(f).get("mod_ids", []))
        data = cache.get(mods) or cache.setdefault(mods, gamedata.load(enabled_mods=list(mods)))
        t0 = time.perf_counter()
        ok, checked, at = replay.verify(f, data)
        bad += not (ok and checked)
        print(f"{'OK  ' if ok and checked else 'HIBA'} {os.path.basename(f)}: {checked} hash"
              + (f", első eltérés: {at}. tick" if at is not None else "") + f" ({time.perf_counter() - t0:.0f} mp)")
    print(f"{len(files) - bad}/{len(files)} egyezik")
    sys.exit(1 if bad or not files else 0)


if __name__ == "__main__":
    main()
