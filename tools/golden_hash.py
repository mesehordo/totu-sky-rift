"""Viselkedési alapvonal: gép–gép meccs állapothash-sorozata (a LAN-szinkron `state_hash`-ével).

    python tools/golden_hash.py --map nyolc_tuz --players 8 --seed 1 --minutes 3 > elotte.txt
    ... változtatás ...
    python tools/golden_hash.py --map nyolc_tuz --players 8 --seed 1 --minutes 3 > utana.txt
    fc elotte.txt utana.txt      (vagy: diff)

Ha egy optimalizálás nem változtathat a játékszabályon, a két kimenetnek bitre azonosnak kell lennie.
"""
import argparse
import os
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import data as gamedata  # noqa: E402
from rts.net import state_hash  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map")
    ap.add_argument("--players", type=int, default=2)
    ap.add_argument("--faction", default="croftian")
    ap.add_argument("--enemy", default="ooni")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--minutes", type=float, default=3)
    ap.add_argument("--every", type=float, default=10, help="hash ennyi játékmásodpercenként")
    a = ap.parse_args()
    data = gamedata.load()
    if a.map:
        slots = [{"kind": "ai_normal", "faction": None, "team": None, "color": i, "start": None}
                 for i in range(a.players)]
        g = M.custom_game(data, {"map": a.map, "slots": slots, "options": {"creeps": True}}, seed=a.seed)
    else:
        g = M.Game(data, [(a.faction, True, "Gép-1"), (a.enemy, True, "Gép-2")], seed=a.seed)
        for p in g.players:
            g.ais.append(M.AI(g, p))
    hz = data.game["sim_hz"]
    step = int(a.every * hz)
    for i in range(1, int(a.minutes * 60 * hz) + 1):
        g.update(1 / hz)
        if i % step == 0:
            print(f"{g.time:7.1f}s units={len(g.units):4d} hash={state_hash(g):08x}")
        if g.result:
            break
    print("vége", round(g.time, 1), "győztes:", g.result[1].faction["id"] if g.result and g.result[1] else None)


if __name__ == "__main__":
    main()
