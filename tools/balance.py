"""Balanszteszt: gép a gép ellen meccsek tömegesen, párhuzamosan.

    python tools/balance.py --seeds 6 --minutes 25
    python tools/balance.py --mods pelda_mod

Minden fajpárt mindkét kezdőhelyről lejátszik, és kiírja a győzelmi arányokat,
az átlagos meccshosszt, valamint fajonként a kiképzett/elvesztett egységeket.
"""
import argparse
import os
import sys
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def play(job):
    fa, fb, seed, minutes, mods = job
    from rts import data
    from rts.ai import AI
    from rts.game import Game
    d = data.load(enabled_mods=mods)
    g = Game(d, [(fa, True, "A"), (fb, True, "B")], seed=seed)
    for p in g.players:
        g.ais.append(AI(g, p))
    dt = 1 / d.game["sim_hz"]
    for _ in range(int(minutes * 60 / dt)):
        g.update(dt)
        if g.result:
            break
    winner = g.result[1].faction["id"] if g.result and g.result[1] else None
    stats = {p.faction["id"] + str(p.id): (p.units_made, p.lost, p.kills) for p in g.players}
    return fa, fb, seed, winner, g.time / 60, stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=4)
    ap.add_argument("--minutes", type=float, default=25)
    ap.add_argument("--mods", default=None)
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) - 1))
    args = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
    mods = args.mods.split(",") if args.mods else None
    from rts import data
    ids = [f["id"] for f in data.load(enabled_mods=mods).faction_list()]
    jobs = [(a, b, 1000 + s, args.minutes, mods) for a in ids for b in ids if a != b for s in range(args.seeds)]
    wins = defaultdict(int)
    games = defaultdict(int)
    pair = defaultdict(lambda: [0, 0])
    lengths = []
    draws = 0
    with ProcessPoolExecutor(args.workers) as ex:
        for fa, fb, seed, w, mins, stats in ex.map(play, jobs):
            games[fa] += 1
            games[fb] += 1
            lengths.append(mins)
            key = tuple(sorted((fa, fb)))
            if w is None:
                draws += 1
            else:
                wins[w] += 1
                pair[key][0 if w == key[0] else 1] += 1
            print(f"{fa:9s} vs {fb:9s} seed {seed}: győztes {w or 'döntetlen':9s} {mins:5.1f} perc")
    print("\nGyőzelmi arány fajonként:")
    for f in ids:
        print(f"  {f:9s} {wins[f]:3d}/{games[f]:3d}  ({100 * wins[f] / max(1, games[f]):.0f}%)")
    print("Párosítások:")
    for (a, b), (wa, wb) in sorted(pair.items()):
        print(f"  {a} {wa} – {wb} {b}")
    print(f"Döntetlen (időlimit): {draws}   átlagos hossz: {sum(lengths) / len(lengths):.1f} perc")


if __name__ == "__main__":
    main()
