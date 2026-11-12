"""A JSON-állapotmentés automata tesztjei: oda-vissza azonosság és a hamisított mentés elutasítása.

    python tools/test_snapshot.py
"""
import json
import os
import random
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import main as M  # noqa: E402
from rts import data as gamedata, snapshot  # noqa: E402
from rts.net import state_hash  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
FAILS = []


def check(cond, msg):
    print(("  OK   " if cond else "  HIBA ") + msg)
    if not cond:
        FAILS.append(msg)


def rejects(doc, data, msg):
    try:
        snapshot.load(doc, data)
    except snapshot.SnapshotError:
        check(True, msg)
        return
    except Exception as e:  # más kivétel is elutasítás, de jelezzük
        check(False, f"{msg} (nem SnapshotError: {type(e).__name__}: {e})")
        return
    check(False, msg + " – betöltötte!")


def main():
    data = gamedata.load(use_mods=False)
    g = M.Game(data, [("croftian", True, "A"), ("ooni", True, "B")], seed=3)
    for p in g.players:
        g.ais.append(M.AI(g, p))
    for _ in range(30 * 45):
        g.update(1 / 30)
    print("Oda-vissza")
    doc = json.loads(json.dumps(snapshot.dump({"game": g, "rng": random.getstate()}, data)))
    root = snapshot.load(doc, data)
    g2 = root["game"]
    check(state_hash(g) == state_hash(g2), "azonos állapothash betöltés után")
    check(g2.data is data, "a GameData a futó adatkészletre kötődik vissza")
    u = next(x for x in g2.units if x.defn.get("id") in data.defs)
    check(u.defn is data.defs[u.defn["id"]], "az egységdefiníció a futó adat ugyanazon objektuma (nem másolat)")
    check(len({id(x) for x in g2.units}) == len(g2.units), "minden egység külön objektum")
    owners = {id(x.owner) for x in g2.units}
    check(owners <= {id(p) for p in g2.players}, "az egységek tulajdonosa a betöltött játékos-objektum (megosztott hivatkozás)")

    print("Hamisított mentés")
    bad = json.loads(json.dumps(doc))
    bad["objects"][0]["c"] = "os.system"
    rejects(bad, data, "idegen osztály (os.system) elutasítva")
    bad = json.loads(json.dumps(doc))
    bad["objects"][0]["c"] = "builtins.eval"
    rejects(bad, data, "beépített függvény mint osztály elutasítva")
    bad = json.loads(json.dumps(doc))
    bad["root"] = {"$data": ["hook"]}
    rejects(bad, data, "adathivatkozás metódusra elutasítva")
    bad = json.loads(json.dumps(doc))
    bad["root"] = {"$data": ["scripts"]}
    rejects(bad, data, "adathivatkozás a mod-szkriptekre elutasítva")
    bad = json.loads(json.dumps(doc))
    bad["schema"] = 99
    rejects(bad, data, "ismeretlen sémaverzió elutasítva")
    rejects({"format": "valami"}, data, "nem TOTU-dokumentum elutasítva")
    bad = json.loads(json.dumps(doc))
    bad["root"] = {"$ismeretlen": 1}
    rejects(bad, data, "ismeretlen jelölő elutasítva")

    print("\nMinden mentésteszt rendben." if not FAILS else f"\n{len(FAILS)} hiba.")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()
