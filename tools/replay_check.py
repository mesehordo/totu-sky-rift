"""Visszajátszás-próba: felvétel szkriptelt „emberi” parancsokkal, majd visszajátszás és hash-összevetés.

    python tools/replay_check.py --seed 7 --minutes 5
    python tools/replay_check.py --map nyolc_tuz --players 8 --minutes 3
    python tools/replay_check.py --verify fajl.trep        # meglévő felvétel ellenőrzése

A 0. játékos gazdaságát a gép viszi, a seregének pedig egy szkript ad parancsokat (mozgás, támadás, attack-move,
állj, tart, kiképzés), amelyek a felvevőn át naplózódnak – pontosan úgy, ahogy a kezelőfelület parancsai.
"""
import argparse
import os
import random
import sys
import tempfile
import time

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import data as gamedata, replay  # noqa: E402
from rts.world import TILE  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def scripted(g, rec, rnd):
    """Néhány parancs a 0. játékos nevében; a hívás két szimulációs lépés között történik, mint az UI-nál."""
    p = g.players[0]
    army = [u for u in g.units if u.owner is p and u.alive and not u.is_worker and u.inside is None]
    out = []
    if army:
        grp = army[: rnd.randint(1, max(1, len(army)))]
        kind = rnd.choice(["move", "amove", "attack", "stop", "hold", "move_q"])
        x, y = rnd.uniform(2, g.map.w - 2) * TILE, rnd.uniform(2, g.map.h - 2) * TILE
        if kind == "move":
            out.append(("cmd_move", [grp, x, y], {}))
        elif kind == "move_q":
            out.append(("cmd_move", [grp, x, y], {"queue": True}))
        elif kind == "amove":
            out.append(("cmd_move", [grp, x, y], {"attack": True}))
        elif kind == "attack":
            foes = [b for b in g.buildings if p.hostile(b.owner)]
            if foes:
                out.append(("cmd_attack", [grp, rnd.choice(foes)], {}))
        elif kind == "stop":
            out.append(("cmd_stop", [grp], {}))
        else:
            out.append(("cmd_hold", [grp], {}))
    blds = [b for b in g.buildings if b.owner is p and b.complete and b.defn.get("produces")]
    if blds and rnd.random() < 0.5:
        b = rnd.choice(blds)
        out.append(("cmd_train", [p, rnd.choice(b.defn["produces"]), b], {}))
    for name, args, kw in out:
        rec.issue(0, name, args, kw)
        getattr(g, name)(*args, **kw)


def record(data, a, path):
    if a.map:
        slots = [{"kind": "ai_normal", "faction": None, "team": None, "color": i, "start": None}
                 for i in range(a.players)]
        g = M.custom_game(data, {"map": a.map, "slots": slots, "options": {"creeps": True}}, seed=a.seed)
    else:
        g = M.Game(data, [(a.faction, True, "Gép-1"), (a.enemy, True, "Gép-2")], seed=a.seed)
        for p in g.players:
            g.ais.append(M.AI(g, p))
    rec = replay.Recorder(g)
    rnd = random.Random(a.seed * 31 + 5)   # a szkript saját véletlene – a játék RNG-jét nem érinti
    hz = data.game["sim_hz"]
    every = int(a.cmd_every * hz)
    for i in range(int(a.minutes * 60 * hz)):
        if i and i % every == 0:
            scripted(g, rec, rnd)
        g.update(1 / hz)
        rec.after_tick()
        if g.result:
            break
    rec.save(path)
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--faction", default="croftian")
    ap.add_argument("--enemy", default="ooni")
    ap.add_argument("--map")
    ap.add_argument("--players", type=int, default=2)
    ap.add_argument("--minutes", type=float, default=5)
    ap.add_argument("--cmd-every", type=float, default=3, help="szkriptelt parancs ennyi mp-enként")
    ap.add_argument("--verify", help="meglévő .trep fájl ellenőrzése")
    ap.add_argument("--no-mods", action="store_true", help="modok nélkül (a golden készlethez)")
    ap.add_argument("--out", help="a felvétel helye (alapból ideiglenes mappa)")
    a = ap.parse_args()
    data = gamedata.load(enabled_mods=[]) if a.no_mods else gamedata.load()
    if a.verify:
        path = a.verify
    else:
        path = a.out or os.path.join(tempfile.gettempdir(), f"totu_replay_proba_{a.seed}.trep")
        rec = record(data, a, path)
        print(f"Felvétel: {len(rec.commands)} parancs, {len(rec.hashes)} hash, {rec.g.tick} tick, "
              f"{os.path.getsize(path) // 1024} KiB, teljes: {rec.complete} → {path}")
    t0 = time.perf_counter()
    ok, checked, bad = replay.verify(path, data)
    print(f"{'OK  ' if ok else 'HIBA'} visszajátszás: {checked} hash ellenőrizve"
          + (f", első eltérés a {bad}. ticknél" if bad is not None else "") + f" ({time.perf_counter() - t0:.1f} mp)")
    sys.exit(0 if ok and checked else 1)


if __name__ == "__main__":
    main()
