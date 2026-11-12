"""Elemi sebzés: melyik fegyver és képesség milyen elemű (tűz, jég, villám, méreg).

    python tools/elemek.py        -> data/elements.json + docs/ELEMEK.md

A hozzárendelés a név, a leírás, az azonosító és a lövedék alapján szabályokkal készül; a data/elements.json
kézzel is javítható (a fájl "kezi" része felülírja a szabályokat, és újrafuttatáskor megmarad).

Hatások (data/elements.json "effects"):
    tűz    -> ÉGÉS: a találat után még néhány másodpercig sebez (páncélon át), frissül, nem halmozódik
    jég    -> FAGYÁS: lassulás néhány másodpercre (ha a fegyver maga is lassít, az marad, nem duplázódik)
    villám -> SOKK: rövid bénulás, egy célponton legfeljebb néhány másodpercenként egyszer
    méreg  -> MÉRGEZÉS: lassú, páncélon átmenő sebzés (gépeken nem hat); a meglévő mérgek érintetlenek
    víz    -> ÁTÁZOTT: eloltja az égést, nem gyullad meg; a villám kétszer hosszabb sokkot ad rá, szünet nélkül
    gyökér -> GÚZSBA KÖTVE: nem mozdulhat, de támadhat
    álom   -> ALSZIK: nem cselekszik, de az első sebzésre felébred (a képesség bénítása helyett)
    rémület-> MEGRÉMÜLT: néhány pillanatig menekül az okozója elől (a képesség bénítása helyett)
    vakítás-> ELVAKULT: távolsági támadásainak fele mellémegy
    szél   -> HÁTRALÖKÉS: a terület közepétől kifelé taszítja az egységeket
Összjáték: víz eloltja a tüzet és megakadályozza; tűz felolvasztja a fagyást; jég kioltja az égést;
víz + villám = kétszer hosszabb sokk.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
P = os.path.join(ROOT, "data", "elements.json")

EFFECTS = {
    "fire": {"name": "Tűz", "status": "Égés", "dur": 3.0, "ratio": 0.25, "min_dps": 2.0, "max_dps": 25.0},
    "ice": {"name": "Jég", "status": "Fagyás", "dur": 2.0, "slow": 0.6},
    "lightning": {"name": "Villám", "status": "Sokk", "dur": 0.4, "cooldown": 3.0},
    "poison": {"name": "Méreg", "status": "Mérgezés", "dur": 4.0, "ratio": 0.15, "min_dps": 1.5, "max_dps": 15.0},
    "water": {"name": "Víz", "status": "Átázott", "dur": 5.0, "shock_mult": 2.0},
    "root": {"name": "Gyökér", "status": "Gúzsba kötve", "dur": 2.5},
    "sleep": {"name": "Álom", "status": "Alszik", "dur": 6.0},
    "fear": {"name": "Rémület", "status": "Megrémült", "dur": 1.6, "flee": 3.0},
    "blind": {"name": "Vakítás", "status": "Elvakult", "dur": 4.0, "miss": 0.5},
    "wind": {"name": "Szél", "status": "Hátralökés", "push": 0.45},
    "sunder": {"name": "Zúzás", "status": "Páncéltörés", "dur": 6.0, "armor": 2},
    "silence": {"name": "Némítás", "status": "Elnémítva", "dur": 3.0},
    "mark": {"name": "Jelölés", "status": "Megjelölt", "dur": 8.0, "dmg_mult": 1.2},
}
RULES = [  # (elem, kulcsszavak) – az első egyező nyer
    ("fire", ["tűz", "tuz_tuzfal", "tuzkop", "láng", "parázs", "parazs", "izzó", "napvihar", "napkitör", "napcsóva",
              "meteorhív", "meteorpor", "égő", "lángcsóva", "pörzsöl", "hőszöv"]),
    ("ice", ["jég", "jeges", "jegveto", "fagy", "hóvihar", "hovihar", "hosszú tél", "zúzmara", "dér ", "fagyos"]),
    ("lightning", ["villámcsap", "villámot", "villámai", "villam", "mennydörg", "vihar-villám"]),
    ("water", ["árhull", "áraszt", "özönvíz", "vízörv", "arhullam", "arasztas"]),
    ("poison", ["sav", "méreg", "mérgez", "spóratüske", "spórarobban", "spóraeső", "rothaszt", "fojtó", "acid"]),
    ("sleep", ["álomspór", "altató"]),
    ("root", ["gyökér", "gyoker", "fonalháló"]),
    ("fear", ["üvölt", "uvolt", "rémít", "rettentő"]),
    ("blind", ["vakít", "mészrobban"]),
    ("wind", ["szélvihar", "szélörv", "tájfun", "isteni szél", "levélörv", "szelvihar"]),
]
NOT = {"tuz_pengetanc", "tuz_homokszablyas", "tuz_homokvihartancos", "tuz_delibab_csalo", "tuz_delibab_mas",
       "pin_jegdardas"}  # a nép neve „tűz”/„jég”, de a fegyver nem az (a jégdárda maga is lassít: marad úgy)


def classify(text, pid=""):
    t = text.lower()
    for el, kws in RULES:
        if any(k in t for k in kws):
            return el
    return None


def main():
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    from rts import data as DATA
    d = DATA.load(enabled_mods=[])
    old = json.load(open(P, encoding="utf-8")) if os.path.isfile(P) else {}
    manual = old.get("kezi", {"abilities": {}, "units": {}})
    ab_out, un_out = {}, {}
    for aid, ab in sorted(d.abilities.items()):
        lv = ab.get("levels") or []
        if not (ab.get("damage") or ab.get("dps") or ab.get("land_damage") or ab.get("slow") or ab.get("stun") or
                any(isinstance(x, dict) and (x.get("damage") or x.get("dps") or x.get("stun") or x.get("slow"))
                    for x in lv)):
            continue
        el = classify(" ".join([aid, ab.get("name", ""), ab.get("desc", ""), str(ab.get("projectile", ""))]))
        if el:
            ab_out[aid] = el
    for uid, u in sorted(d.defs.items()):
        a = u.get("attack")
        if not isinstance(a, dict) or uid in NOT or u.get("kind") == "building":
            continue
        if a.get("projectile") == "acid" or a.get("poison"):
            un_out[uid] = "poison"
            continue
        el = classify(" ".join([uid, u.get("name", ""), u.get("desc", "")]))
        if el in ("fire", "ice", "lightning", "poison", "water"):  # az irányító hatások csak képességen
            un_out[uid] = el
    ab_out.update(manual.get("abilities", {}))
    un_out.update(manual.get("units", {}))
    out = {"_leiras": "Elemi sebzés (tools/elemek.py írja). abilities / units: azonosító -> elem. A 'kezi' rész kézi "
                      "javítás: felülírja a szabályokat, újrafuttatáskor megmarad. Elem törléséhez a kézi részben "
                      "legyen az értéke null.",
           "effects": EFFECTS, "abilities": {k: v for k, v in ab_out.items() if v},
           "units": {k: v for k, v in un_out.items() if v}, "kezi": manual}
    json.dump(out, open(P, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    names = {k: v["name"] for k, v in EFFECTS.items()}
    L = ["# Elemi sebzés", "", "A `tools/elemek.py` állítja elő (javítás: `data/elements.json`, „kezi” rész).", "",
         "| Elem | Hatás | Időtartam | Erősség |", "|---|---|---|---|"]
    for k, e in EFFECTS.items():
        if k in ("water", "root", "sleep", "fear", "blind", "wind", "sunder", "silence", "mark"):
            v = {"water": lambda: "eloltja az égést; a villám kétszer hosszabb sokkot ad rá",
                 "root": lambda: "nem mozdulhat, de támadhat", "sleep": lambda: "az első sebzésre felébred",
                 "fear": lambda: f"{e['flee']:.0f} mezőnyit menekül az okozója elől",
                 "blind": lambda: f"távolsági támadásainak {int(e['miss'] * 100)}%-a mellémegy",
                 "wind": lambda: "a terület közepétől kifelé taszít",
                 "sunder": lambda: f"−{e['armor']} páncél (épületen is)",
                 "silence": lambda: "nem varázsolhat",
                 "mark": lambda: f"+{int(round((e['dmg_mult'] - 1) * 100))}% sebzést kap, a köd alól is látszik"}[k]()
            L.append(f"| {e['name']} | {e['status']} | {e.get('dur', '–')} mp | {v} |")
            continue
        if k == "fire":
            v = f"a találat {int(e['ratio'] * 100)}%-a másodpercenként ({e['min_dps']}–{e['max_dps']}), páncélon át"
        elif k == "ice":
            v = f"{int((1 - e['slow']) * 100)}% lassulás"
        elif k == "lightning":
            v = f"bénulás; egy célponton legfeljebb {e['cooldown']:.0f} mp-enként"
        else:
            v = f"a találat {int(e['ratio'] * 100)}%-a másodpercenként ({e['min_dps']}–{e['max_dps']}), gépeken nem"
        L.append(f"| {e['name']} | {e['status']} | {e['dur']} mp | {v} |")
    for title, src, dd in (("Képességek", ab_out, d.abilities), ("Egységek fegyvere", un_out, d.defs)):
        L += ["", f"## {title}", ""]
        for el in EFFECTS:
            ids = [k for k, v in sorted(src.items()) if v == el]
            if ids:
                L.append(f"- **{names[el]}** ({len(ids)}): " + ", ".join(f"{dd[k].get('name', k)} (`{k}`)" for k in ids))
    open(os.path.join(ROOT, "docs", "ELEMEK.md"), "w", encoding="utf-8").write("\n".join(L) + "\n")
    print(f"képesség: {len(out['abilities'])}, egység: {len(out['units'])}")


if __name__ == "__main__":
    main()
