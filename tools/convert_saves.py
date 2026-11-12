"""Régi (pickle, .sav) mentések átalakítása az új JSON-formátumba (.tsav).

    python tools/convert_saves.py            # minden profil minden régi mentése
    python tools/convert_saves.py --dry-run  # csak listáz

FIGYELEM: a régi .sav fájl Python pickle – beolvasása tetszőleges kódot futtathat. Csak a SAJÁT gépeden készült
mentéseket alakítsd át, idegentől kapott .sav fájlt soha.

Mentésenként: beolvasás a mentés saját modlistájával → JSON-mentés → visszaolvasás és állapothash-összevetés →
a régi fájl megmarad ``<név>.sav.regi`` néven (biztonsági másolat; a játék nem látja).
"""
import argparse
import glob
import json
import os
import pickle
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from rts import data as gamedata, savegame as SG  # noqa: E402
from rts.net import state_hash  # noqa: E402

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def convert(base, dry_run=False):
    name = os.path.basename(base)
    with open(base + SG.LEGACY_EXT, "rb") as f:
        mods = pickle.load(f).get("mod_ids") or ()
    if dry_run:
        print(f"  átalakítandó: {name}  (modok: {', '.join(mods) or 'nincs'})")
        return True
    data = gamedata.load(enabled_mods=list(mods))
    if tuple(data.mod_ids) != tuple(mods):
        print(f"  HIBA {name}: a mentés modjai nem tölthetők be ({', '.join(mods)})")
        return False
    g, cam, seen, extra = SG.load_legacy_pickle(base, data)
    before = state_hash(g)
    SG.write_state(base + SG.SAVE_EXT, g, cam, seen, extra)
    g2, *_ = SG.load_game(base, data)
    after = state_hash(g2)
    if before != after:
        os.remove(base + SG.SAVE_EXT)
        print(f"  HIBA {name}: az állapothash eltér ({before:08x} ≠ {after:08x}); a régi mentés változatlan")
        return False
    meta_path = base + ".json"
    if os.path.isfile(meta_path):
        with open(meta_path, encoding="utf-8") as f:
            meta = json.load(f)
        meta.update(version=SG.SAVE_VERSION, format="tsav")
        SG._replace_file(meta_path, json.dumps(meta, ensure_ascii=False, indent=1).encode("utf-8"))
    os.replace(base + SG.LEGACY_EXT, base + SG.LEGACY_EXT + ".regi")
    print(f"  OK   {name}  (modok: {', '.join(mods) or 'nincs'}; hash {after:08x}; "
          f"{os.path.getsize(base + SG.SAVE_EXT) // 1024} KiB)")
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--dir", default=os.path.join(ROOT, "saves", "profiles"))
    a = ap.parse_args()
    legacy = sorted(p[:-len(SG.LEGACY_EXT)] for p in glob.glob(os.path.join(a.dir, "**", "*" + SG.LEGACY_EXT),
                                                                 recursive=True))
    todo = [b for b in legacy if not os.path.isfile(b + SG.SAVE_EXT)]
    print(f"Régi mentés: {len(todo)}")
    ok = sum(convert(b, a.dry_run) for b in todo)
    print(f"Kész: {ok}/{len(todo)}")
    sys.exit(0 if ok == len(todo) else 1)


if __name__ == "__main__":
    main()
