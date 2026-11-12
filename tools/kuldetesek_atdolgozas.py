"""A régi küldetések átdolgozása a hadjárat-útmutató szerint.

    python tools/kuldetesek_atdolgozas.py

Forrás: az eredeti küldetések (backup_grafika/kuldetesek_regi/…, a történet, a célok, a döntések és a jelzők
változatlanok maradnak) + docs/tortenet/story/edited/01_vilag_es_fotortenet.md. Minden pálya kap:
  * kézzel tervezett alaprajzot elnevezett helyekkel (utak, folyók, gerincek, erdők, nyersanyagok, díszletek);
  * helyhez kötött célokat céljelölővel (a régi 0..1 arányos pontok helyett);
  * portrés, beszélős párbeszédet (a régi „Név: szöveg” üzenetekből) és nyitó / záró átvezetőt;
  * a hadjárat-sorrend, a döntések és a továbbvitt jelzők nem változnak.
A „Két út között” (w4) már az új szabvány szerint készült, azt nem írja felül.
"""
import copy
import json
import math
import os
import random
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OLD = os.path.join(ROOT, "backup_grafika", "kuldetesek_regi")

PORTRAIT = {"Livia": "imp_hos_livia", "Nera": "imp_hos_nera", "Tava": "wul_hos_tava", "Ragna": "urs_hos_ragna",
            "Szelai": "elf_hos_szelai", "Mór": "myc_hos_mor", "Keth": "cro_hos_keth", "Ordan": "imp_szellemidezo",
            "Maurus": "imp_centurio", "Kórus": "cro_hos_ssarha", "Cassius": "imp_hos_cassius"}


def say(who, text, to=None, urgent=False):
    d = {"who": who, "text": text, "portrait": PORTRAIT.get(who.split(",")[0], "")}
    if to:
        d["to"] = to
    if urgent:
        d["urgent"] = True
    return d


def make_layout(seed, size, pts, starts, roads=(), river=None, fords=(), ridges=(), rocks=(), clumps=10, res=(),
                decor=(), ambient=(), water=(), forests=()):
    """Alaprajz egy küldetéshez: utak, folyó gázlókkal, gerincek, véletlen (de az utaktól és pontoktól távol eső)
    erdőfoltok, nyersanyagok a bázisok mellett."""
    W, H = size
    rng = random.Random(seed)
    lay = {"size": [W, H], "seed": seed, "points": dict(pts), "paint": [], "clear": [], "forest": [{"border": 3}],
           "resources": list(res), "starts": list(starts), "decor": list(decor), "ambient": list(ambient)}
    if river:
        lay["paint"].append({"t": "water", "line": river["line"], "w": river.get("w", 3)})
    for w in water:
        lay["paint"].append({"t": "water", "area": w})
    for r in ridges:
        lay["paint"].append({"t": "rock", "line": r[0], "w": r[1]})
    for r in rocks:
        lay["paint"].append({"t": "rock", "area": r})
    for f in fords:
        lay["clear"].append({"t": "dirt", "line": f, "w": 3})
    for a, b in roads:
        lay["clear"].append({"t": "dirt", "line": [a, b] if not isinstance(a, list) or len(a) != 2 or isinstance(a[0], list) else [a, b], "w": 2})
    for f in forests:
        lay["forest"].append({"area": f, "density": 0.75})

    def P(p):
        return pts[p] if isinstance(p, str) else p
    keep = [P(p) for p in pts.values()]
    road_pts = []
    for a, b in roads:
        (x0, y0), (x1, y1) = P(a), P(b)
        for i in range(11):
            road_pts.append((x0 + (x1 - x0) * i / 10, y0 + (y1 - y0) * i / 10))
    n = 0
    tries = 0
    while n < clumps and tries < 400:
        tries += 1
        x, y = rng.randrange(6, W - 6), rng.randrange(6, H - 6)
        r = rng.uniform(2.5, 5.0)
        if any(math.hypot(x - px, y - py) < r + 6 for px, py in keep) or \
                any(math.hypot(x - px, y - py) < r + 3 for px, py in road_pts):
            continue
        lay["forest"].append({"area": [[x, y], round(r, 1)], "density": 0.75})
        n += 1
    return lay


def base_res(pt, dx=6):
    x, y = pt
    return [{"kind": "stone", "at": [x - dx, y + 3]}, {"kind": "stone", "at": [x + dx, y - 4]},
            {"kind": "meteor", "at": [x + 3, y + dx]}]


def convert_messages(m):
    """A régi „Név: szöveg” üzenetek beszélős párbeszédsorrá alakulnak (portréval)."""
    for t in m.get("triggers", []):
        msg = t.get("message")
        if not msg:
            continue
        mm = re.match(r"^([A-ZÁÉÍÓÖŐÚÜŰ][\wáéíóöőúüű’\-]+): (.+)$", msg)
        if mm and mm.group(1) in PORTRAIT:
            t.pop("message")
            t.pop("color", None)
            t["say"] = [say(mm.group(1), mm.group(2).strip("„”\""))]
    for t in m.get("triggers", []):
        for o in (t.get("choice") or {}).get("options", []):
            msg = o.get("message")
            if msg:
                mm = re.match(r"^([A-ZÁÉÍÓÖŐÚÜŰ][\wáéíóöőúüű’\-]+): (.+)$", msg)
                if mm and mm.group(1) in PORTRAIT:
                    o.pop("message")
                    o["say"] = [say(mm.group(1), mm.group(2))]


def apply(cid, mid, spec):
    m = json.load(open(os.path.join(OLD, cid, mid + ".json"), encoding="utf-8"))
    if m.get("layout"):
        return None
    m = copy.deepcopy(m)
    m["code"] = spec["code"]
    m["source"] = "docs/tortenet/story/edited/01_vilag_es_fotortenet.md – 8. fejezet (" + spec.get("src", "") + ")"
    m["canon"] = {"rogzitett": spec.get("rog", "A küldetés eseményei, döntése és jelzői az eredeti hadjáratból."),
                  "adaptacio": spec.get("ada", "Kézzel tervezett pálya elnevezett helyekkel, céljelölők, párbeszéd és átvezetők."),
                  "uj_javaslat": spec.get("uj", "–")}
    m["layout"] = spec["layout"]
    m.pop("map", None)
    convert_messages(m)
    for o in m.get("objectives", []):  # helyhez kötött célok: elnevezett pontok
        if o["id"] in spec.get("at", {}):
            o["at"] = spec["at"][o["id"]]
        if o["id"] in spec.get("tag", {}):
            o["tag"] = spec["tag"][o["id"]]
    for i, patch in spec.get("trig_patch", {}).items():  # finomhangolás (időzítés, létszám)
        m["triggers"][i].update(patch)
    near = spec.get("near", {})
    for i, t in enumerate(m.get("triggers", [])):  # erősítések és rohamok a pálya helyeiről
        sp = t.get("spawn")
        if sp and i in near:
            sp["near"] = near[i][0]
            if near[i][1]:
                sp["to"] = near[i][1]
    for k, v in spec.get("player", {}).items():
        m.setdefault("player", {})[k] = v
    for i, e in enumerate(spec.get("enemies", [])):
        if i < len(m.get("enemies", [])):
            m["enemies"][i].update(e)
    if spec.get("allies"):
        m["allies"] = spec["allies"]
    rep = spec.get("replace", {})  # célok cseréje változatosabb feladatra (az azonosító marad: a régi események rá épülnek)
    m["objectives"] = [dict(rep[o["id"]], id=o["id"]) if o["id"] in rep else o for o in m.get("objectives", [])]
    m["objectives"] += spec.get("extra_obj", [])
    if spec.get("units"):
        m["player"]["units"] = spec["units"]
    trig = []
    if spec.get("intro"):
        trig.append({"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
            {"camera": c, "say": [say(w, t) for w, t in lines]} for c, lines in spec["intro"]]}})
    trig += m.get("triggers", [])
    trig += spec.get("extra_trig", [])
    if spec.get("outro"):
        last = spec.get("outro_on") or next((o["id"] for o in reversed(m["objectives"]) if not o.get("optional")), None)
        trig.append({"id": "zaras", "on_objective": last, "cinematic": {"shots": [
            {"camera": c, "say": [say(w, t) for w, t in lines]} for c, lines in spec["outro"]]}})
    m["triggers"] = trig
    if spec.get("fail"):
        m["fail_if_lost"] = m.get("fail_if_lost", []) + spec["fail"]
    m.pop("creeps", None)
    return m


# ===================================================================== a küldetések
S = {}

# ---- Előjáték
p = {"korhaz": [30, 40], "kapu": [30, 26], "kut": [14, 44], "krater": [46, 10], "muhely": [44, 42], "tomeg": [30, 8]}
S[("egsz_0_elojatek", "p0_masodik_lelegzet")] = dict(
    code="PRO-01", src="Előjáték: A második lélegzet",
    layout=make_layout(1, [60, 54], p, ["korhaz", "tomeg"], roads=[("korhaz", "kapu"), ("kapu", "tomeg"), ("korhaz", "kut"), ("korhaz", "muhely")],
                       rocks=[["krater", 6]], clumps=7, res=[{"kind": "stone", "at": [22, 46]}, {"kind": "meteor", "at": [38, 48]}],
                       decor=[{"id": "imp_legiogyar", "at": [16, 34], "ruin": True}, {"id": "imp_ontode", "at": [46, 30], "ruin": True}],
                       ambient=[{"id": "imp_fabrikator", "n": 2, "at": "muhely", "mode": "idle", "shelter": "korhaz"}]),
    near={1: ("kapu", "korhaz")},
    intro=[("krater", [("Livia", "A második nap darabokra tört. A legnagyobb kő a kapunk előtt hullott le.")]),
           ("korhaz", [("Livia", "Víz, áram, ágyak. Aki tud járni, segítsen. Aki nem, azt mi visszük.")])],
    outro=[("korhaz", [("Livia", "Nera? Nera, hallasz? …A hangszórók megint szólnak.")])])

# ---- Wullof
W = "egsz_1_wullof"
p = {"vasar": [32, 40], "raktar1": [22, 34], "raktar2": [42, 34], "raktar3": [32, 28], "keserukut": [32, 6], "erdo": [10, 20]}
S[(W, "w1_hetfolyo_vasara")] = dict(
    code="WUL-01", src="I. hadjárat: Hétfolyó vására",
    layout=make_layout(701, [64, 56], p, ["vasar", "keserukut"], roads=[("vasar", "raktar1"), ("vasar", "raktar2"), ("vasar", "raktar3"), ("raktar3", "keserukut")],
                       river={"line": [[-1, 48], [20, 50], [44, 48], [65, 50]], "w": 3}, fords=[[[32, 46], [32, 52]]], clumps=9, res=base_res([32, 40]),
                       decor=[{"id": "wul_tipi", "at": [26, 44]}, {"id": "wul_tipi", "at": [38, 45], "ruin": True}],
                       ambient=[{"id": "wul_emu", "n": 3, "at": [20, 44], "path": [[20, 44], [26, 48], [16, 46]]}]),
    near={1: ("keserukut", "raktar3")},
    intro=[("vasar", [("Tava", "Hétfolyó vására. Ha a raktárak üresek, a szomszédok egymást kezdik okolni.")]),
           ("keserukut", [("Tava", "A Keserűkútiak éhesek. Az éhes ember hamar fosztogató lesz.")])],
    outro=[("vasar", [("Tava", "A vásár nyitva. De a csordák továbbra is tűnnek. Fel kell írnom, hová.")])])

p = {"tabor": [10, 46], "kut1": [22, 10], "kut2": [54, 30], "melyseg": [40, 18], "domb": [34, 38]}
S[(W, "w2_keseru_kutak")] = dict(
    code="WUL-02", src="I. hadjárat: Keserű kutak",
    layout=make_layout(7223, [64, 56], p, ["tabor", "melyseg"], roads=[("tabor", "domb"), ("domb", "kut2"), ("tabor", [18, 28]), ([18, 28], "kut1")],
                       rocks=[["melyseg", 5], [[46, 44], 4]], clumps=10, res=base_res([10, 46], 5)),
    at={"kut1": "kut1", "kut2": "kut2"},
    near={2: ("melyseg", "domb")},
    intro=[("kut1", [("Tava", "Az úténekek szerint ez a vidék mindig édes vizű volt.")]),
           ("tabor", [("Tava", "Két kút, két kérdés. Előbb az északi, aztán a keleti.")])],
    outro=[("kut2", [("Tava", "Ahol csillagérc halad a föld alatt, ott ők is. Ezt fel kell rajzolni.")])])

p = {"ter": [36, 30], "nyugati_kapu": [6, 34], "hasadek1": [44, 22], "hasadek2": [30, 40], "rakpart": [36, 50]}
S[(W, "w3_a_fold_megnyilik")] = dict(
    code="WUL-03", src="I. hadjárat: A föld megnyílik",
    layout=make_layout(6395, [64, 56], p, ["ter", "hasadek1"], roads=[("ter", "nyugati_kapu"), ("ter", "rakpart")],
                       rocks=[["hasadek1", 3], ["hasadek2", 2.5]], clumps=8, res=base_res([36, 30], 7),
                       decor=[{"id": "wul_tipi", "at": [30, 24], "ruin": True}, {"id": "wul_harcosok_tuze", "at": [42, 34], "ruin": True},
                              {"id": "wul_tipi", "at": [24, 30]}]),
    at={"menekules": "nyugati_kapu"},
    near={0: ("hasadek1", "ter"), 1: ("hasadek2", "ter"), 2: ("nyugati_kapu", None)},
    trig_patch={0: {"at": 10, "spawn": {"owner": 1, "units": {"cro_karmos": 5}}}},
    intro=[("ter", [("Tava", "Vásárnap. Zene, csereáru… és a föld belülről dobog.")])],
    outro=[("nyugati_kapu", [("Mór", "A támadások a föld alatti ércszállítmányokat követik. Ezt a saját kertemben láttam."),
                             ("Tava", "Akkor együtt rajzoljuk fel az utat.")])])

p = {"rakpart": [34, 48], "csonakok": [20, 52], "varos": [34, 30], "atkeles": [46, 20], "eszak": [30, 4]}
S[(W, "w5_akik_bennunk_maradtak")] = dict(
    code="WUL-05", src="I. hadjárat: zárócsata",
    layout=make_layout(598, [64, 56], p, ["rakpart", "eszak"], roads=[("rakpart", "varos"), ("varos", "atkeles"), ("rakpart", "csonakok")],
                       river={"line": [[-1, 14], [24, 16], [46, 14], [65, 16]], "w": 3}, fords=[[[46, 11], [46, 19]], [[18, 11], [18, 19]]],
                       water=[[[18, 54], 4]], clumps=8, res=base_res([34, 48], 7),
                       decor=[{"id": "wul_tipi", "at": [28, 30], "ruin": True}, {"id": "wul_tipi", "at": [40, 30], "ruin": True}]),
    at={"utolso": "csonakok"},
    near={3: ("atkeles", "rakpart"), 4: ("eszak", None)},
    intro=[("varos", [("Tava", "Hétfolyó nem tartható. A falakat nem védjük – az embereket.")]),
           ("csonakok", [("Tava", "A csónakok a déli parton. Minden család beszáll, aztán mi.")])],
    outro=[("csonakok", [("Tava", "Ismerem ezt a hangot. A vásári úton kínált vizet egy sebesült fuvarosnak.")])])

# ---- Konvoj
p = {"erod": [10, 46], "hago": [54, 8], "kanyar": [30, 30], "szoros": [44, 18], "les": [20, 14]}
S[("egsz_1b_konvoj", "x1_vegyes_konvoj")] = dict(
    code="INT-01", src="Átvezető: A vegyes konvoj",
    layout=make_layout(2101, [64, 56], p, ["erod", "les"], roads=[("erod", "kanyar"), ("kanyar", "szoros"), ("szoros", "hago")],
                       ridges=[[[[22, 40], [40, 24], [50, 10]], 2]], rocks=[[[50, 34], 6], [[24, 18], 5]], clumps=6,
                       res=base_res([10, 46], 5)),
    at={"konvoj": "hago", "tava": "hago"},
    near={1: ("les", "kanyar"), 2: ("les", "szoros")},
    intro=[("hago", [("Ragna", "Áthaladás – nem menedék. A hágó túloldaláig kísérünk, nem tovább.")]),
           ("erod", [("Tava", "Ursan pajzs, Wullof család, kethri tojáskamra. Egy konvoj. Egy út.")])],
    outro=[("hago", [("Tava", "Átértünk."), ("Ragna", "…A kaput nem zárom be mögöttetek.")])])

# ---- Ursan
U = "egsz_2_ursan"
p = {"erod": [30, 42], "magtar": [36, 36], "kapu": [30, 28], "klanok": [30, 6], "erdo": [12, 20]}
S[(U, "u1_ures_magtarak")] = dict(
    code="URS-01", src="II. hadjárat: Üres magtárak",
    layout=make_layout(3101, [64, 56], p, ["erod", "klanok"], roads=[("erod", "kapu"), ("kapu", "klanok")],
                       ridges=[[[[0, 26], [26, 28]], 3], [[[34, 28], [64, 26]], 3]], clumps=10, res=base_res([30, 42]),
                       decor=[{"id": "urs_tanya", "at": [44, 46]}, {"id": "urs_tanya", "at": [18, 48], "ruin": True}],
                       ambient=[{"id": "urs_mammut", "n": 1, "at": [48, 40], "mode": "idle"}]),
    near={2: ("klanok", "kapu")},
    intro=[("magtar", [("Ragna", "A termés elfagyott. Ami a magtárban van, az a tavaszi hadjáraté.")]),
           ("kapu", [("Ragna", "Egyszer már lezártam egy hágót. Emlékszem, kik maradtak a túloldalon.")])],
    outro=[("kapu", [("Ragna", "A magtár tele. A kapu… a kapu ezúttal nyitva marad.")])])

p = {"tabor": [12, 44], "banya": [44, 18], "tarna1": [30, 30], "tarna2": [52, 40], "krater": [50, 10]}
S[(U, "u2_felolvadt_banya")] = dict(
    code="URS-02", src="II. hadjárat: A felolvadt bánya",
    layout=make_layout(3202, [64, 56], p, ["tabor", "tarna2"], roads=[("tabor", "tarna1"), ("tarna1", "banya"), ("tabor", "tarna2")],
                       rocks=[["krater", 5], [[30, 10], 6]], clumps=7,
                       res=base_res([12, 44], 5) + [{"kind": "meteor", "at": [44, 22]}, {"kind": "meteor", "at": [40, 16]}]),
    near={1: ("tarna1", "tabor"), 2: ("tarna2", "tabor")},
    intro=[("banya", [("Ragna", "A felolvadt jég régi csillagércet hozott elő. A jarl kéri. Megkapja.")]),
           ("tarna1", [("Ragna", "Pajzsfal a tárnák elé. Ezek alulról jönnek, nem a kapun.")])],
    outro=[("tabor", [("Ragna", "A bányászok köhögnek. A jarl nem akar róla hallani. Én igen.")])])

p = {"kamrak": [32, 30], "tabor": [32, 46], "falu": [8, 10], "raj": [56, 8]}
S[(U, "u3_tojasok_a_hamu_alatt")] = dict(
    code="URS-03", src="II. hadjárat: Tojások a hamu alatt",
    layout=make_layout(5338, [64, 56], p, ["tabor", "raj"], roads=[("tabor", "kamrak"), ("kamrak", "falu"), ("kamrak", "raj")],
                       clumps=9, res=base_res([32, 46], 7),
                       decor=[{"id": "cro_tapgomba", "at": [26, 24], "ruin": True}, {"id": "cro_tapgomba", "at": [38, 22], "ruin": True}]),
    player={"buildings": [{"id": "kethri_tojaskamra", "at": [28, 30]}, {"id": "kethri_tojaskamra", "at": [34, 26]},
                         {"id": "kethri_tojaskamra", "at": [36, 32]}]},
    near={1: ("falu", "kamrak"), 2: ("raj", "kamrak")},
    intro=[("kamrak", [("Ragna", "Fertőzetlen tojáskamrák egy leégett telepen. A tisztjeim szerint égessük el.")]),
           ("tabor", [("Ragna", "Ezek gyerekek. Senki nem gyújt fáklyát a közelükben.")])],
    outro=[("kamrak", [("Ragna", "A kamrák épek. Hívjatok árulónak. A gyerekek élnek.")])])

p = {"tabor": [10, 46], "banya": [30, 30], "terem": [52, 12], "zsoldos": [40, 22]}
S[(U, "u4_a_jarl_adossaga")] = dict(
    code="URS-04", src="II. hadjárat: A jarl adóssága",
    layout=make_layout(3404, [64, 56], p, ["tabor", "terem"], roads=[("tabor", "banya"), ("banya", "zsoldos"), ("zsoldos", "terem")],
                       river={"line": [[-1, 34], [20, 36], [40, 34], [65, 36]], "w": 3}, fords=[[[24, 31], [24, 40]], [[46, 31], [46, 40]]],
                       clumps=9, res=base_res([10, 46], 5)),
    intro=[("banya", [("Ragna", "A jarl tudta. Ércben akarta fizetni a zsoldosait, a bányászok tüdejével.")]),
           ("terem", [("Ragna", "Az eskü a népemnek szól, nem egy embernek, aki eladja őket.")])],
    outro=[("terem", [("Ragna", "A bizonyíték a királyhoz kerül. Tudom, hogy ezzel a címemnek vége.")])])

p = {"erod": [50, 10], "deli_ut": [10, 50], "kanyar": [34, 28], "szoros": [20, 40], "raj": [56, 46]}
S[(U, "u5_nyitott_kapuk")] = dict(
    code="URS-05", src="II. hadjárat: Nyitott kapuk",
    layout=make_layout(3505, [64, 56], p, ["erod", "raj"], roads=[("erod", "kanyar"), ("kanyar", "szoros"), ("szoros", "deli_ut")],
                       rocks=[[[18, 26], 6], [[40, 44], 5]], clumps=7, res=base_res([50, 10], 6)),
    at={"evakual": "deli_ut", "ragna": "deli_ut"},
    near={3: ("raj", "szoros"), 4: ("deli_ut", None)},
    intro=[("erod", [("Ragna", "A kapu nyitva. Aki jön, jöjjön.")]),
           ("deli_ut", [("Ragna", "Délre, a Hét Út felé. A pajzs először a mellettünk állót védi.")])],
    outro=[("deli_ut", [("Ragna", "Már nem egy birodalmat képviselek. Azokat vezetem, akik vállalták velem a döntést.")])])

# ---- Szürke elfek
E = "egsz_3_elfek"
p = {"allomas": [12, 44], "telep": [48, 16], "banya1": [40, 26], "banya2": [54, 30]}
S[(E, "e1_tiszta_erc")] = dict(
    code="ELF-01", src="III. hadjárat: A tiszta érc",
    layout=make_layout(4101, [64, 56], p, ["allomas", "telep"], roads=[("allomas", "banya1"), ("banya1", "telep"), ("banya1", "banya2")],
                       rocks=[[[30, 12], 6]], clumps=9, res=base_res([12, 44], 5) + [{"kind": "meteor", "at": [42, 28]}, {"kind": "meteor", "at": [52, 26]}]),
    intro=[("banya1", [("Szelai", "A tisztítatlan érc pora mérgez. Ezt a telepet le kell állítani.")]),
           ("allomas", [("Szelai", "A tiszta érc életet ment. A tisztátlan elvesz.")])],
    outro=[("telep", [("Szelai", "Nem barbárok. Éhesek. Ezt nem írták a parancsba.")])])

p = {"tabor": [10, 46], "varos": [46, 14], "totem1": [38, 18], "totem2": [52, 22], "hegy": [56, 48]}
S[(E, "e2_idegen_tornyok")] = dict(
    code="ELF-02", src="III. hadjárat: Idegen tornyok",
    layout=make_layout(4202, [64, 56], p, ["tabor", "varos"], roads=[("tabor", [30, 30]), ([30, 30], "varos"), ([30, 30], "hegy")],
                       river={"line": [[-1, 28], [24, 30], [44, 28], [65, 30]], "w": 3}, fords=[[[30, 25], [30, 34]]], clumps=9, res=base_res([10, 46], 5)),
    near={},
    intro=[("varos", [("Szelai", "Szabályosan tisztítják az ércet. Csak nem ismerik el a mi ellenőrzésünket.")])],
    outro=[("hegy", [("Szelai", "A raj valódi. A tanács tudott róla? Az archívum megmondja.")])])

p = {"tabor": [10, 48], "archivum": [52, 10], "lepcso1": [24, 36], "lepcso2": [38, 22], "feszek": [46, 30]}
S[(E, "e3_lezart_archivum")] = dict(
    code="ELF-03", src="III. hadjárat: A lezárt archívum",
    layout=make_layout(4303, [64, 56], p, ["tabor", "feszek"], roads=[("tabor", "lepcso1"), ("lepcso1", "lepcso2"), ("lepcso2", "archivum")],
                       ridges=[[[[16, 30], [30, 30]], 2], [[[32, 16], [46, 16]], 2]], clumps=8, res=base_res([10, 48], 5),
                       decor=[{"id": "elf_csillagvizsgalo", "at": [56, 6]}, {"id": "elf_naptemplom", "at": [44, 6], "ruin": True}]),
    at={"archiv": "archivum", "vissza": "tabor"}, tag={"archiv": None},
    intro=[("archivum", [("Szelai", "Engedély nélkül. Ha kérdeznek, én hoztam a döntést.")])],
    outro=[("tabor", [("Szelai", "Hetekkel előbb látták. Csak a saját városaikat figyelmeztették. Ezt az eltitkolást nem folytatom.")])])

p = {"varos": [14, 44], "torony1": [26, 30], "torony2": [40, 24], "kaptar": [52, 10]}
S[(E, "e4_repedes_a_halozaton")] = dict(
    code="ELF-04", src="III. hadjárat: Repedés a hálózaton",
    layout=make_layout(4404, [64, 56], p, ["varos", "kaptar"], roads=[("varos", "torony1"), ("torony1", "torony2"), ("torony2", "kaptar")],
                       clumps=9, res=base_res([14, 44], 6),
                       decor=[{"id": "elf_obeliszk", "at": "torony1", "ruin": True}, {"id": "elf_obeliszk", "at": "torony2", "ruin": True}]),
    intro=[("torony2", [("Szelai", "A hálózatot én terveztem. A fertőzés az én tornyaimon fut.")]),
           ("varos", [("Szelai", "Akkor én tudom, hol kell elvágni.")])],
    outro=[("kaptar", [("Szelai", "A fészek elesett. A hálózat sebezhetősége most már mindenki előtt ismert.")])])

p = {"varos": [32, 14], "torony": [32, 22], "deli_kapu": [32, 52], "terasz": [44, 30], "raj": [60, 40]}
S[(E, "e5_a_ledontott_lepcso")] = dict(
    code="ELF-05", src="III. hadjárat: A ledöntött lépcső",
    layout=make_layout(4505, [64, 56], p, ["varos", "raj"], roads=[("varos", "torony"), ("torony", [32, 36]), ([32, 36], "deli_kapu"), ("torony", "terasz")],
                       ridges=[[[[20, 30], [28, 34]], 2], [[[36, 34], [44, 38]], 2]], clumps=7, res=base_res([32, 14], 6),
                       decor=[{"id": "elf_napszentely", "at": [22, 8], "ruin": True}]),
    at={"menekul": "deli_kapu"},
    near={1: ("raj", "terasz")},
    intro=[("torony", [("Szelai", "A torony nélkül nincs pajzs. Leválasztom a várost, hogy a többi éljen.")]),
           ("deli_kapu", [("Szelai", "Tegyétek, amit a nagyapáitok tettek: álljatok sorba.")])],
    outro=[("deli_kapu", [("Szelai", "A rangom maradt a lépcsőn. A feljegyzéseket viszem: a Fekete Tükörnél valami épül.")])])

# ---- Micéliák
G = "egsz_4_gombanep"
for mid, code, seed, extra in (("g1_szaraz_kertek", "MIC-01", 5601, {}), ("g2_az_emlek_ara", "MIC-02", 5602, {}),
                               ("g3_az_elso_betegek", "MIC-03", 5603, {"at": {"kert": "emlekkert"}}),
                               ("g4_egy_nev_a_korusban", "MIC-04", 5604, {}), ("g5_az_ego_kert", "MIC-05", 5605, {})):
    p = {"kert": [14, 44], "emlekkert": [50, 12], "szurdok": [32, 28], "goc1": [46, 34], "goc2": [36, 10], "melyseg": [56, 46]}
    S[(G, mid)] = dict(
        code=code, src="Szövetséges hadjárat: Akikre emlékezünk",
        layout=make_layout(seed, [64, 56], p, ["kert", "melyseg"], roads=[("kert", "szurdok"), ("szurdok", "emlekkert"), ("szurdok", "goc1"), ("szurdok", "goc2")],
                           rocks=[[[26, 12], 6], [[46, 24], 4]], water=[[[20, 30], 3]], clumps=10, res=base_res([14, 44], 5),
                           ambient=[{"id": "myc_sporaszedo", "n": 3, "at": [18, 50], "mode": "idle", "shelter": "kert"}]),
        intro=[("kert", [("Mór", {"g1_szaraz_kertek": "A kert nem gyógyul meg magától. De nem is felejt.",
                                   "g2_az_emlek_ara": "Ha a fertőzött gócok összeérnek, a raj az egész hálózatot eléri.",
                                   "g3_az_elso_betegek": "Az ősi emlékkert őrzi az első kethri betegeket. Talán választ ad.",
                                   "g4_egy_nev_a_korusban": "Keth hangja a fonalban. Ha válaszolok, a Kórus is meghallja.",
                                   "g5_az_ego_kert": "A Kórus megtalálta a nagy emlékkertet. Le kell választanunk egy részét."}[mid])])],
        outro=[("emlekkert", [("Mór", {"g1_szaraz_kertek": "A tanácsok vitatkoznak. De már ugyanazon a fonalon.",
                                        "g2_az_emlek_ara": "Egy kis közösség hálózata velük együtt halt el. Megjegyzem a nevüket.",
                                        "g3_az_elso_betegek": "Nincs csodaszer. De van remény – egyenként.",
                                        "g4_egy_nev_a_korusban": "Keth hangja megmaradt bennem. Halkan.",
                                        "g5_az_ego_kert": "Nem vihettem mindet magammal. Nem is fogom elfelejteni, amit itt hagytam."}[mid])])],
        **extra)

# ---- Finomító
p = {"finomito": [34, 26], "tabor": [10, 46], "reaktor1": [26, 36], "reaktor2": [42, 36], "reaktor3": [34, 16], "raj": [56, 8],
     "erc": [48, 20]}
S[("egsz_4b_finomito", "x2_finomito")] = dict(
    code="INT-02", src="Átvezető: A közös finomító",
    layout=make_layout(4801, [64, 56], p, ["tabor", "raj"], roads=[("tabor", "reaktor1"), ("reaktor1", "finomito"), ("finomito", "reaktor2"), ("finomito", "reaktor3")],
                       clumps=8, res=base_res([10, 46], 5) + [{"kind": "meteor", "at": "erc"}, {"kind": "meteor", "at": [52, 26]}],
                       decor=[{"id": "imp_finomito", "at": "finomito", "ruin": True}]),
    near={1: ("tabor", None)},
    intro=[("finomito", [("Livia", "Ugyanaz az érc fűtené a menekülteket, és töltené a horgonyainkat. Mindkettő igaz.")])],
    outro=[("finomito", [("Livia", "A finomító dolgozik. Mögöttünk pékek, jogászok, gyerekek – nem gépek.")])])

# ---- Aureth
A = "egsz_5_aureth"
p = {"tabor": [10, 46], "szallitmany": [46, 18], "orseg": [54, 10], "ut": [30, 32]}
S[(A, "a1_lefoglalt_szallitmany")] = dict(
    code="VAS-01", src="VI. hadjárat: A lefoglalt szállítmány",
    layout=make_layout(5101, [64, 56], p, ["tabor", "orseg"], roads=[("tabor", "ut"), ("ut", "szallitmany"), ("szallitmany", "orseg")],
                       clumps=9, res=base_res([10, 46], 5) + [{"kind": "meteor", "at": [44, 22]}, {"kind": "meteor", "at": [50, 16]}]),
    near={1: ("orseg", "ut")},
    intro=[("szallitmany", [("Livia", "Minden horgony egy ember. Minden ércdarab egy nap. De nem ölünk több ursant, mint muszáj.")])],
    outro=[("orseg", [("Livia", "Ragna üzent: holnap menjek vissza beszélni. Megyek.")])])

p = {"forum": [32, 40], "kapu": [32, 24], "pekseg": [20, 44], "hatar": [32, 4], "kelet": [58, 20]}
S[(A, "a2_ures_serlegek_varosa")] = dict(
    code="VAS-02", src="VI. hadjárat: Az üres serlegek városa",
    layout=make_layout(5202, [64, 56], p, ["forum", "hatar"], roads=[("forum", "kapu"), ("kapu", "hatar"), ("kapu", "kelet"), ("forum", "pekseg")],
                       ridges=[[[[0, 22], [26, 24]], 3], [[[38, 24], [52, 22]], 3]], clumps=7, res=base_res([32, 40], 7),
                       decor=[{"id": "imp_reaktor", "at": [44, 44]}, {"id": "imp_legiogyar", "at": "pekseg"}],
                       ambient=[{"id": "imp_fabrikator", "n": 3, "at": [24, 48], "path": [[24, 48], [40, 48], [32, 50]], "shelter": "forum"}]),
    intro=[("pekseg", [("Livia", "A pékségben ma is sütnek. Az illat emléket tart életben.")]),
           ("kapu", [("Livia", "Mögöttünk nem katonák állnak. Pékek. Színészek. Gyerekek, akik hetven éve gyerekek.")])],
    outro=[("forum", [("Livia", "Nera megsérült. Haragudnom kellene. Inkább… büszke vagyok.")])])

for mid, code, seed, a_, b_ in (("a3_az_orok_hajnal", "VAS-03", 5303, "Ordan: Nem engedem el őket.", "A „néhány év” hetven éve tart."),
                                ("a4_a_szabad_horgony", "VAS-04", 5404, "Egy lélek Ordan nélkül is fenntartható. Bizonyítsuk.", "Szabadon. Több karbantartással, de szabadon."),
                                ("a5_feltoltes_megtagadva", "VAS-05", 5505, "A feltöltés megtagadva. Percek alatt kell átállítanunk a hálózatot.",
                                 "Akiket elvesztettünk, végleg elvesztek. Ordan tervét ez nem állítja meg.")):
    p = {"tabor": [10, 46], "templom": [50, 12], "torony1": [36, 22], "torony2": [52, 30], "reaktor": [24, 34]}
    S[(A, mid)] = dict(
        code=code, src="VI. hadjárat: " + mid,
        layout=make_layout(seed, [64, 56], p, ["tabor", "templom"], roads=[("tabor", "reaktor"), ("reaktor", "torony1"), ("torony1", "templom"), ("torony1", "torony2")],
                           clumps=8, res=base_res([10, 46], 5), decor=[{"id": "imp_forum", "at": [56, 48], "ruin": True}]),
        intro=[("templom", [("Livia", a_.split(": ", 1)[-1])])],
        outro=[("tabor", [("Livia", b_)])])

# ---- Keth
C = "egsz_6_keth"
for mid, code, seed, intro, outro in (
        ("c1_sebek_egyesitese", "CRO-01", 6101, ("Kórus", "Az elszigetelt tudat fél, téved és meghal. Mi gyógyítunk."), ("Keth", "…hány név maradt bennük?")),
        ("c2_ismeros_kovek", "CRO-02", 6202, ("Keth", "Ezen a téren cseréltem jádét sóra."), ("Keth", "A terasz elesett. Nem tudom, ki győzött bennem.")),
        ("c3_a_visszatero_nev", "CRO-03", 6303, ("Kórus", "Foglald el a Nagytermet."), ("Keth", "Tesha. Megjegyzem a hangját.")),
        ("c4_levalas", "CRO-04", 6404, ("Keth", "Akik velem jönnek, fájni fog nekik."), ("Keth", "A leválás sikerült. Részben.")),
        ("c5_a_belso_kapu", "CRO-05", 6505, ("Keth", "A minta a fészekanyánál van."), ("Keth", "Két akarat, egy kapu. A Kórus és Ordan ugyanazt akarja."))):
    p = {"feszek": [10, 46], "cel": [50, 12], "ter": [32, 28], "kapu": [56, 50]}
    S[(C, mid)] = dict(
        code=code, src="VII. hadjárat: " + mid,
        layout=make_layout(seed, [64, 56], p, ["feszek", "cel"], roads=[("feszek", "ter"), ("ter", "cel"), ("ter", "kapu")],
                           clumps=9, res=base_res([10, 46], 5)),
        at={"kapu": "kapu", "keth": "kapu"},
        intro=[("ter", [intro])], outro=[("cel" if mid != "c4_levalas" else "kapu", [outro])])

# ---- Finálé
F = "egsz_7_hajnal"
p = {"part": [10, 48], "hid": [32, 30], "tukor": [54, 8], "biztos_part": [6, 30], "jarat": [44, 44], "totem1": [20, 40], "totem2": [26, 34], "totem3": [38, 26]}
S[(F, "f1_a_hid")] = dict(
    code="FIN-01", src="Közös befejezés: A híd",
    layout=make_layout(7101, [64, 56], p, ["part", "tukor"], roads=[("part", "totem1"), ("totem1", "totem2"), ("totem2", "hid"), ("hid", "tukor"), ("part", "biztos_part"), ("hid", "jarat")],
                       river={"line": [[-1, 22], [24, 24], [44, 22], [65, 24]], "w": 4}, fords=[[[32, 18], [32, 30]]], clumps=7, res=base_res([10, 48], 5)),
    at={"civilek": "biztos_part"},
    intro=[("hid", [("Tava", "Ezen az úton a következő télen is meg kell érkeznie az élelemnek.")])],
    outro=[("hid", [("Tava", "A híd áll. A föld alatt a szerelőjárat kész. Mindenki megérkezett, aki megígérte.")])])

p = {"tabor": [10, 46], "szentely": [52, 10], "udvar": [36, 24], "kapu": [26, 34]}
S[(F, "f2_az_ures_tron")] = dict(
    code="FIN-02", src="Közös befejezés: Az üres trón",
    layout=make_layout(7202, [64, 56], p, ["tabor", "szentely"], roads=[("tabor", "kapu"), ("kapu", "udvar"), ("udvar", "szentely")],
                       clumps=7, res=base_res([10, 46], 5)),
    at={"szentely": "szentely"},
    intro=[("szentely", [("Livia", "Nincs trónon ülő óriás. Csak felvételek. Az első kórházak hangjai.")])],
    outro=[("szentely", [("Ordan", "Ha elmennek, meghalnak. Ha meghalnak… én buktam el."), ("Livia", "Nem buktál el. Csak most engedsz.")])])

p = {"tabor": [10, 46], "mag": [50, 12], "k1": [36, 18], "k2": [52, 30], "k3": [30, 8]}
S[(F, "f3_a_nevtelen_hajnal")] = dict(
    code="FIN-03", src="Közös befejezés: A névtelen hajnal",
    layout=make_layout(7303, [64, 56], p, ["tabor", "mag"], roads=[("tabor", [24, 30]), ([24, 30], "k1"), ("k1", "mag"), ([24, 30], "k2"), ("k1", "k3")],
                       rocks=[[[40, 40], 5]], clumps=6, res=base_res([10, 46], 5)),
    intro=[("mag", [("Ragna", "A pajzs először a mellettünk állót védi. Ma mindenki mellettünk áll.")])],
    outro=[("mag", [("Ordan", "Most már elengedhetlek.")])])


def main():
    n = 0
    for (cid, mid), spec in S.items():
        m = apply(cid, mid, spec)
        if m is None:
            continue
        with open(os.path.join(ROOT, "campaigns", cid, mid + ".json"), "w", encoding="utf-8") as f:
            json.dump(m, f, ensure_ascii=False, indent=1)
        n += 1
    print(f"átdolgozva: {n} küldetés")


if __name__ == "__main__":
    main()
