"""Reprodukálható teljesítménypróba (S02): gép–gép meccs, tickidő-percentilisek, részrendszer-bontás, memória.

    python tools/perf_probe.py --seed 7 --faction croftian --enemy ooni --minutes 8
    python tools/perf_probe.py --map nyolc_tuz --players 8 --minutes 10 --seed 1
    python tools/perf_probe.py ... --render          # a kliens rajzolását is méri (SDL dummy, 1280×720)
    python tools/perf_probe.py ... --profile 25      # cProfile: a 25 legdrágább függvény (lassítja a futást!)

Kimenet: összefoglaló a konzolra és JSON a docs/fejlesztes/meresek/ mappába (--out felülírja).
A részrendszer-idők egymásba ágyazódhatnak (pl. az AI útkeresést hív), ezért összegük nem a tickidő.
Csak mért értéket ír ki; becslést nem.
"""
import argparse
import ctypes
import json
import os
import platform
import subprocess
import sys
import time
from collections import defaultdict

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import ai as AI_MOD, data as gamedata, entities as E, game as G, pathfinding as PF  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ACC = defaultdict(float)     # részrendszer → ebben a tickben eltöltött idő (s)
CNT = defaultdict(int)
_pc = time.perf_counter


def timed(owner, name, label):
    fn = getattr(owner, name)

    def wrapper(*a, **k):
        t = _pc()
        try:
            return fn(*a, **k)
        finally:
            ACC[label] += _pc() - t
            CNT[label] += 1
    setattr(owner, name, wrapper)


def instrument():
    for meth, label in (("_rebuild_hash", "spatial_hash"), ("_process_paths", "path_queue"),
                        ("_separate", "separation"), ("_update_vis", "fow_vis")):
        timed(G.Game, meth, label)
    timed(AI_MOD.AI, "update", "ai")
    timed(E.Unit, "update", "units")
    timed(E.Building, "update", "buildings")
    timed(E.Projectile, "update", "projectiles")
    timed(E.AreaEffect, "update", "effects")
    # útkeresés: a modulok saját névterében hívott példányt is cserélni kell
    for name in ("find_path", "smooth_path", "local_detour"):
        orig = getattr(PF, name)
        for mod in (PF, G, E, AI_MOD):
            if getattr(mod, name, None) is orig:
                timed(mod, name, "pathfind")


def pct(vals, p):
    if not vals:
        return None
    s = sorted(vals)
    i = min(len(s) - 1, max(0, int(round(p / 100 * (len(s) - 1)))))
    return round(s[i], 3)


def stats(vals):
    return {"p50": pct(vals, 50), "p95": pct(vals, 95), "p99": pct(vals, 99),
            "max": round(max(vals), 3) if vals else None,
            "mean": round(sum(vals) / len(vals), 3) if vals else None}


def peak_mem_mib():
    if os.name != "nt":
        try:
            import resource
            return round(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024, 1)
        except Exception:
            return None

    class PMC(ctypes.Structure):
        _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong),
                    ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
    c = PMC()
    c.cb = ctypes.sizeof(c)
    try:
        k32 = ctypes.windll.kernel32
        k32.GetCurrentProcess.restype = ctypes.c_void_p
        k32.K32GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(PMC), ctypes.c_ulong]
        ok = k32.K32GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(c), c.cb)
        return round(c.PeakWorkingSetSize / 2 ** 20, 1) if ok else None
    except Exception:
        return None


def environment():
    def run(cmd):
        try:
            return subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT, timeout=10).stdout.strip()
        except Exception:
            return None
    commit = run(["git", "rev-parse", "--short", "HEAD"])
    dirty = bool(run(["git", "status", "--porcelain", "--untracked-files=no"]))
    cpu = platform.processor()
    if os.name == "nt":
        cpu = run(["powershell", "-NoProfile", "-Command", "(Get-CimInstance Win32_Processor).Name"]) or cpu
    import pygame
    return {"commit": commit, "worktree_dirty": dirty, "python": platform.python_version(),
            "pygame": pygame.version.ver, "os": platform.platform(), "cpu": cpu, "cpu_count": os.cpu_count()}


def build_game(data, a):
    if a.map:
        slots = [{"kind": "ai_" + a.difficulty, "faction": (a.factions[i] if i < len(a.factions) else None),
                  "team": None, "color": i, "start": None} for i in range(a.players)]
        return M.custom_game(data, {"map": a.map, "slots": slots, "options": {"creeps": True}}, seed=a.seed)
    import random
    ids = [f["id"] for f in data.faction_list()]
    pick = random.Random(a.seed)
    fa = a.faction or pick.choice(ids)
    fb = a.enemy or pick.choice(ids)
    g = G.Game(data, [(fa, True, "Gép-1"), (fb, True, "Gép-2")], seed=a.seed)
    for p in g.players:
        g.ais.append(AI_MOD.AI(g, p))
    return g


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--faction")
    ap.add_argument("--enemy")
    ap.add_argument("--map", help="csatapálya-azonosító (data/maps); ekkor --players gépi játékos")
    ap.add_argument("--players", type=int, default=2)
    ap.add_argument("--factions", default="", help="vesszős fajlista a --map módhoz")
    ap.add_argument("--difficulty", default="normal")
    ap.add_argument("--minutes", type=float, default=8)
    ap.add_argument("--render", action="store_true", help="kliensrajzolás mérése (minden szimulációs tick után)")
    ap.add_argument("--profile", type=int, default=0)
    ap.add_argument("--no-mods", action="store_true")
    ap.add_argument("--out")
    a = ap.parse_args()
    a.factions = [f for f in a.factions.split(",") if f]

    data = gamedata.load(use_mods=not a.no_mods)
    instrument()
    g = build_game(data, a)
    hz = data.game["sim_hz"]
    dt = 1 / hz
    ui = None
    if a.render:
        import pygame
        pygame.init()
        screen = pygame.display.set_mode((1280, 720))
        from rts.ui import UI
        ui = UI(g, screen, g.players[0], M.make_fonts(pygame))
        ui.settings = dict(ui.settings, edge_scroll=False)
        ui.zoom = ui.wanted_zoom()

    prof = None
    if a.profile:
        import cProfile
        prof = cProfile.Profile()
        prof.enable()

    tick_ms, render_ms, sub = [], [], defaultdict(list)
    timeline = []
    t_wall = _pc()
    n_ticks = int(a.minutes * 60 / dt)
    for i in range(n_ticks):
        ACC.clear()
        t = _pc()
        g.update(dt)
        tick_ms.append((_pc() - t) * 1000)
        for k, v in ACC.items():
            sub[k].append(v * 1000)
        if ui is not None:
            if i % (hz * 5) == 0:  # a kamera kövesse a csatát: a legnagyobb saját sereg
                army = [u for u in g.units if u.owner is g.players[0] and not u.is_worker] or \
                       [u for u in g.units if u.owner is g.players[0]]
                if army:
                    ui.center_on(army[0].x, army[0].y)
            t = _pc()
            ui.draw(0)
            render_ms.append((_pc() - t) * 1000)
        if i % int(60 / dt) == 0 or g.result:
            timeline.append({
                "minute": round(g.time / 60, 2), "units": len(g.units), "buildings": len(g.buildings),
                "projectiles": len(g.projectiles), "effects": len(g.effects), "path_queue": len(g.path_queue),
                "supply": {p.faction["id"]: [round(p.supply_used, 1), p.supply_cap] for p in g.players},
                "tick_ms_p95_last_min": pct(tick_ms[-int(60 / dt):], 95)})
        if g.result:
            break
    wall = _pc() - t_wall
    if prof:
        prof.disable()

    # részrendszerek: a nem hívott tickeket 0-nak vesszük a percentilishez
    n = len(tick_ms)
    subsystems = {k: dict(stats(v + [0.0] * (n - len(v))), total_s=round(sum(v) / 1000, 2),
                          share_of_tick_pct=round(100 * sum(v) / max(1e-9, sum(tick_ms)), 1))
                  for k, v in sorted(sub.items(), key=lambda kv: -sum(kv[1]))}
    res = {
        "test_id": "S02-" + (f"{a.map}-{a.players}P" if a.map else "SKIRMISH-2P") + ("-RENDER" if a.render else ""),
        "status": "measured", "date": time.strftime("%Y-%m-%d %H:%M"), "env": environment(),
        "seed": a.seed, "map": a.map or "skirmish (generált)", "players": [p.faction["id"] for p in g.players],
        "difficulty": a.difficulty, "mods": not a.no_mods, "sim_hz": hz, "tick_budget_ms": round(1000 / hz, 2),
        "game_minutes": round(g.time / 60, 2), "ticks": n, "wall_s": round(wall, 1),
        "realtime_factor": round(g.time / wall, 2),
        "result": (g.result[1].faction["id"] if g.result and g.result[1] else None),
        "tick_ms": stats(tick_ms), "ticks_over_budget": sum(1 for v in tick_ms if v > 1000 / hz),
        "render_ms": stats(render_ms) if render_ms else None, "render_resolution": [1280, 720] if ui else None,
        "subsystems_ms": subsystems, "peak_working_set_mib": peak_mem_mib(),
        "max_units": max((t["units"] for t in timeline), default=0), "timeline": timeline,
        "profiled": bool(prof),
        "notes": "cProfile aktív – az időértékek torzak" if prof else "",
    }
    print(f"{res['test_id']}  {res['players']}  seed={a.seed}  {res['game_minutes']} perc / {res['wall_s']} mp "
          f"(×{res['realtime_factor']})  győztes={res['result']}")
    print(f"tick ms  {res['tick_ms']}  keret túllépés: {res['ticks_over_budget']}/{n} (keret {res['tick_budget_ms']} ms)")
    if render_ms:
        print(f"render ms {res['render_ms']}")
    for k, v in subsystems.items():
        print(f"  {k:13s} p95={v['p95']:8.3f}  max={v['max']:8.2f}  össz={v['total_s']:7.2f} s  ({v['share_of_tick_pct']}%)")
    print(f"max egység: {res['max_units']}  csúcsmemória: {res['peak_working_set_mib']} MiB")
    if prof:
        import io
        import pstats
        s = io.StringIO()
        pstats.Stats(prof, stream=s).sort_stats("tottime").print_stats(a.profile)
        res["profile_top"] = s.getvalue()
        print(s.getvalue())

    out = a.out or os.path.join(ROOT, "docs", "fejlesztes", "meresek",
                                time.strftime("%Y%m%d_%H%M%S_") + res["test_id"] + f"_s{a.seed}.json")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False, indent=1)
    print("JSON:", out)


if __name__ == "__main__":
    main()
