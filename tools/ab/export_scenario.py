"""F3 A/B mérés: a közös bemenet exportja a valódi játékból (pálya, egységtípusok, kezdő egységek, cél).

    python tools/ab/export_scenario.py --units 500 --out tools/ab/scenarios/center_500.json

Forgatókönyv „battle” (--scenario battle): akadálymentes 144×144-es csatatér; a 8 sereg körben a közép körül,
egymáshoz közel, mind a középre tart – a harc azonnal kezdődik, az útkeresés (egyenes vonal) elhanyagolható.

Forgatókönyv „center”: a `nyolc_tuz` pálya (144×144) blokkrácsa a kezdő főépületekkel és nyersanyagokkal;
8 csapat, csapatonként units/8 földi harci egység a saját kezdőhelye körül, a 7 főfaj valódi egységadataival;
minden egység attack-move paranccsal a pálya közepére tart. Szabályok: docs/alap/SZABALYOK.md 3. fejezet.
"""
import argparse
import base64
import json
import os
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import data as gamedata  # noqa: E402
from rts.pathfinding import fits  # noqa: E402
from rts.world import TILE  # noqa: E402

UTYPES = ["kozelharcos", "landzsas", "lovesz", "lovas", "orias", "varazslo", "repulo", "ostrom", "munkas"]


def unit_types(data):
    out, by_faction = [], {}
    for f in data.faction_list():
        for uid, u in f["units"].items():
            a = u.get("attack")
            if u.get("hero") or not a or u.get("flying") or "ground" not in a.get("targets", ["ground"]):
                continue
            if u.get("utype") == "munkas":
                continue
            by_faction.setdefault(f["id"], []).append(len(out))
            out.append({
                "id": uid,
                "radius": round(u.get("radius", 0.4) * TILE, 4),
                "speed": round(u.get("speed", 2.5) * TILE, 4),
                "hp": float(u["hp"]),
                "armor": float(u.get("armor", 0)),
                "damage": float(a["damage"]),
                "range": round(a["range"] * TILE, 4),
                "cooldown": float(a["cooldown"]),
                "projectile": bool(a.get("projectile")),
                "proj_speed": float(a.get("proj_speed", 11 if a.get("projectile") == "boulder" else 15) * TILE),
                "splash": round(a.get("splash", 0) * TILE, 4),
                "sight": float(u.get("sight", 7) * TILE),
                "utype": UTYPES.index(u.get("utype")) if u.get("utype") in UTYPES else -1,
            })
    return out, by_faction


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--units", type=int, default=500)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--map", default="nyolc_tuz")
    ap.add_argument("--seconds", type=float, default=180)
    ap.add_argument("--scenario", choices=["center", "battle"], default="center")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    data = gamedata.load(enabled_mods=[])
    slots = [{"kind": "ai_normal", "faction": None, "team": None, "color": i, "start": None} for i in range(8)]
    g = M.custom_game(data, {"map": a.map, "slots": slots, "options": {"creeps": False}}, seed=a.seed)
    m = g.map
    types, by_faction = unit_types(data)
    matrix = [[1.0] * len(UTYPES) for _ in UTYPES]
    for s, row in data.game["unit_types"]["matrix"].items():
        for t, v in row.items():
            if s in UTYPES and t in UTYPES:
                matrix[UTYPES.index(s)][UTYPES.index(t)] = v
    units, used = [], set()
    per_team = a.units // 8
    block = bytes(1 if b else 0 for b in m.block)
    if a.scenario == "battle":
        import math
        block = bytes(m.w * m.h)                       # akadálymentes csatatér
        side = int(math.ceil(math.sqrt(per_team)))     # csapatonként side × side csempés négyzet
        ring = (side + 4) / (2 * math.sin(math.pi / 8))  # szomszédos négyzetek között legalább 4 csempe hely
        for team, p in enumerate(g.players):
            pool = by_faction[p.faction["id"]]
            ang = team * 2 * math.pi / 8
            cx, cy = m.w / 2 + math.cos(ang) * ring, m.h / 2 + math.sin(ang) * ring
            for k in range(per_team):
                t = pool[(k * 7 + team) % len(pool)]
                tx, ty = int(cx - side / 2) + k % side, int(cy - side / 2) + k // side
                units.append({"team": team, "type": t, "x": (tx + 0.5) * TILE, "y": (ty + 0.5) * TILE})
    for team, p in enumerate(g.players if a.scenario == "center" else []):
        pool = by_faction[p.faction["id"]]
        sx, sy = int(p.start_xy[0] // TILE), int(p.start_xy[1] // TILE)
        placed, ring = 0, 3
        while placed < per_team and ring < 60:
            cells = [(sx + dx, sy + dy) for dy in range(-ring, ring + 1) for dx in range(-ring, ring + 1)
                     if max(abs(dx), abs(dy)) == ring]
            for tx, ty in cells:
                if placed >= per_team:
                    break
                t = pool[(placed * 7 + team) % len(pool)]
                x, y = (tx + 0.5) * TILE, (ty + 0.5) * TILE
                if (tx, ty) in used or not m.inb(tx, ty) or not fits(m, x, y, types[t]["radius"]):
                    continue
                used.add((tx, ty))
                units.append({"team": team, "type": t, "x": x, "y": y})
                placed += 1
            ring += 1
    doc = {
        "format": "totu-ab-scenario", "schema": 1, "name": f"{a.scenario}_{a.units}",
        "map": a.map if a.scenario == "center" else "nyílt csatatér", "seed": a.seed,
        "tile": TILE, "hz": data.game["sim_hz"], "ticks": int(a.seconds * data.game["sim_hz"]),
        "w": m.w, "h": m.h, "block": base64.b64encode(block).decode("ascii"),
        "factions": [p.faction["id"] for p in g.players], "types": types, "matrix": matrix,
        "units": units, "dest": [m.w * TILE / 2, m.h * TILE / 2],
    }
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
    print(f"{a.out}: {len(units)} egység, {len(types)} típus, pálya {m.w}×{m.h}, {doc['ticks']} tick, "
          f"frakciók: {', '.join(doc['factions'])}")


if __name__ == "__main__":
    main()
