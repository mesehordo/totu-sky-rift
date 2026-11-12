"""A JSON-állapotmentés oda-vissza ellenőrzése: mentés → betöltés → folytatás ugyanazt adja-e, mint a megszakítás nélküli
futás (10 mp-enkénti állapothash).

    python tools/snapshot_check.py --map nyolc_tuz --players 8 --seed 1 --at 120 --then 60
    python tools/snapshot_check.py --campaign egsz_4_gombanep --mission g1_szaraz_kertek --at 60 --then 60
    python tools/snapshot_check.py --all-missions --at 20 --then 20      # minden kampányküldetés
"""
import argparse
import gzip
import json
import os
import random
import sys
import time

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import campaign as C, data as gamedata, snapshot  # noqa: E402
from rts.net import state_hash  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def run(g, seconds, hz, every=10):
    out = []
    step = int(every * hz)
    for i in range(1, int(seconds * hz) + 1):
        g.update(1 / hz)
        if i % step == 0:
            out.append((round(g.time, 1), state_hash(g)))
        if g.result:
            out.append(("vége", g.result[0]))
            break
    return out


def check(data, make, label, at, then):
    hz = data.game["sim_hz"]
    g = make()
    run(g, at, hz)
    t0 = time.perf_counter()
    doc = snapshot.dump({"game": g, "rng": random.getstate()}, data)
    text = json.dumps(doc, ensure_ascii=False, separators=(",", ":"))
    t_dump = time.perf_counter() - t0
    size = len(gzip.compress(text.encode("utf-8")))
    expected = run(g, then, hz)
    t0 = time.perf_counter()
    root = snapshot.load(json.loads(text), data)
    t_load = time.perf_counter() - t0
    random.setstate(root["rng"])
    got = run(root["game"], then, hz)
    ok = expected == got
    print(f"{'OK  ' if ok else 'HIBA'} {label}: {len(expected)} hash, mentés {size // 1024} KiB (gzip), "
          f"mentés {t_dump * 1000:.0f} ms, betöltés {t_load * 1000:.0f} ms")
    if not ok:
        for a, b in zip(expected, got):
            if a != b:
                print("     első eltérés:", a, "≠", b)
                break
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map")
    ap.add_argument("--players", type=int, default=2)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--campaign")
    ap.add_argument("--mission")
    ap.add_argument("--all-missions", action="store_true")
    ap.add_argument("--at", type=float, default=60, help="mentés ennyi játékmásodpercnél")
    ap.add_argument("--then", type=float, default=60, help="utána ennyi másodperc összevetés")
    a = ap.parse_args()
    data = gamedata.load()
    results = []
    if a.all_missions or a.campaign:
        for camp, _ in C.list_campaigns(data):
            if a.campaign and camp["id"] != a.campaign:
                continue
            for md in camp["missions_data"]:
                if a.mission and md["id"] != a.mission:
                    continue
                results.append(check(data, lambda md=md: C.build_game(data, md, seed=a.seed),
                                     f"{camp['id']}/{md['id']}", a.at, a.then))
    else:
        if a.map:
            slots = [{"kind": "ai_normal", "faction": None, "team": None, "color": i, "start": None}
                     for i in range(a.players)]

            def make():
                return M.custom_game(data, {"map": a.map, "slots": slots, "options": {"creeps": True}}, seed=a.seed)
            label = f"{a.map} {a.players}P seed {a.seed}"
        else:
            def make():
                g = M.Game(data, [("croftian", True, "Gép-1"), ("ooni", True, "Gép-2")], seed=a.seed)
                for p in g.players:
                    g.ais.append(M.AI(g, p))
                return g
            label = f"skirmish croftian–ooni seed {a.seed}"
        results.append(check(data, make, label, a.at, a.then))
    print(f"Összesen: {sum(results)}/{len(results)} egyezik")
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
