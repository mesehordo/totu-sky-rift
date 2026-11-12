"""3. gazdasági változat: három nyersanyag (fa, arany, meteorit) és fajonként eltérő létszámnövelés.

    python tools/economy_v3.py

* Az étel megszűnik: minden ételköltség 65%-a fába, 35%-a kőbe kerül (5-re kerekítve).
* Létszám (a korábbi „ellátmány”): az egységek eltérő helyet foglalnak (pl. mamut 6), a korlát fajonként
  másképp nő:
    - szürke elfek: Napobeliszkek (épület, energiamező is),
    - ursanok: Tanyák (épület, a WC-s farmokhoz hasonlóan); a Hosszúház helyőrség-ház lett,
    - croftianok: csak élőlények – a Kaptár és a lebegő Szemhordozók hordozzák (egység),
    - wullof: Tipik – felszedhetők és vándorszekéren vihetők (mozgó épület),
    - gombanép: a hálózat – minden összekötött kész épület +1, a Szívgombák +6,
    - Vasbirodalom: Lélekreaktorok; az idézett szellemek nem foglalnak helyet.
Idempotens.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def r5(v):
    return int(round(v / 5.0)) * 5


def conv(cost):
    if not isinstance(cost, dict) or "food" not in cost:
        return cost
    f = cost.pop("food")
    if f:
        cost["wood"] = cost.get("wood", 0) + r5(f * 0.65)
        cost["stone"] = cost.get("stone", 0) + r5(f * 0.35)
        for k in ("wood", "stone"):
            if cost[k] == 0:
                cost.pop(k)
    return cost


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


# ---------------------------------------------------------------- game.json
gp = os.path.join(ROOT, "data", "game.json")
g = rd(gp)
g["resources"].pop("food", None)
g["start_resources"] = {"wood": 400, "stone": 250, "meteor": 0}
g["supply_name"] = "Létszám"
wr(gp, g)

# ---------------------------------------------------------------- képességek
ap = os.path.join(ROOT, "data", "abilities.json")
a = rd(ap)
for v in a.values():
    conv(v.get("cost"))
wr(ap, a)

# ---------------------------------------------------------------- fajok
fdir = os.path.join(ROOT, "data", "factions")
for fn in sorted(os.listdir(fdir)):
    p = os.path.join(fdir, fn)
    d = rd(p)
    for sect in ("units", "buildings", "researches"):
        for v in d.get(sect, {}).values():
            conv(v.get("cost"))
            if "dropoff" in v:
                v["dropoff"] = [k for k in v["dropoff"] if k != "food"]
            v.pop("income", None)
    d.pop("resource_names", None)
    if d.get("start_gather") == "food" or "start_gather" not in d:
        d["start_gather"] = "wood"
    ai = d.get("ai")
    if ai and "gather_ratio" in ai:
        gr = {k: v for k, v in ai["gather_ratio"].items() if k != "food"}
        gr.setdefault("wood", 0.5)
        tot = sum(gr.values())
        ai["gather_ratio"] = {k: round(v / tot, 2) for k, v in gr.items()}
    fid = d.get("id")
    U, B = d.get("units", {}), d.get("buildings", {})
    if fid == "ursan":
        B["urs_tanya"].update({"name": "Tanya", "supply": 10, "cost": {"wood": 130, "stone": 30},
                               "desc": "Árpaföld, ólak és szénapajták: a jarlság népe innen él. +10 létszám."})
        B["urs_hosszuhaz"].update({"supply": 0, "garrison": 4, "armor": 3, "hp": 700,
                                   "desc": "Erődített hosszúház: 4 fős helyőrség, riadóban a telepesek menedéke."})
        d["supply_building"] = "urs_tanya"
        d["supply_desc"] = "Tanyák"
    elif fid == "croftian":
        B["cro_tapgomba"].update({"supply": 0, "creep_radius": 8, "regen_aura": 2,
                                  "desc": "Tápláló gombatelep: messzire terjeszti a mélységet, és gyógyítja a rajt."})
        U["cro_szemhordozo"].update({"unit_supply": 8, "cost": {"wood": 60, "stone": 40},
                                     "desc": "Lebegő szemzsák: messzire lát, és a Kórus létszámát hordozza (+8)."})
        d["supply_unit"] = "cro_szemhordozo"
        d["supply_desc"] = "Szemhordozók (egységek)"
    elif fid == "wullof":
        B["wul_tipi"]["packable"] = True
        B["wul_tipi"]["desc"] = "Bőrsátor: +8 létszám. Felszedhető és vándorszekéren máshová vihető."
        d["supply_desc"] = "Tipik (felszedhetők)"
    elif fid == "mycel":
        d["network_supply"] = 1
        B["myc_szivgomba"]["supply"] = 6
        d["supply_desc"] = "A hálózat: minden összekötött épület +1"
    elif fid == "imperium":
        B["imp_reaktor"]["cost"] = {"stone": 100, "wood": 40}
        B["imp_reaktor"]["supply"] = 10
        d["supply_desc"] = "Lélekreaktorok"
        for uid in ("imp_szellemlegios", "imp_szellemkolosszus"):
            if uid in U:
                U[uid]["supply"] = 0
    elif fid == "elves":
        d["supply_desc"] = "Napobeliszkek"
    if fid == "elves":
        B["elf_obeliszk"]["desc"] = "+8 létszám. Energiamezőt sugároz – az elf épületek csak ebben működnek."
    if fid == "imperium":
        B["imp_reaktor"]["desc"] = "+10 létszám – lélekenergiát ad a gépeknek."
    if fid == "mycel":
        B["myc_szivgomba"]["desc"] = ("A hálózat szíve: kicsi főépület és védőtorony egyben. +6 létszám (és a hálózat minden "
                                      "összekötött épülete +1), Spóraszedőket nevel, spórákkal lő. Akárhány építhető.")
    for sect in ("units", "buildings"):
        for v in d.get(sect, {}).values():
            if "ellátmány" in v.get("desc", ""):
                v["desc"] = v["desc"].replace("ellátmány", "létszám")
    if fid == "wullof":
        d["bounty_res"] = "wood"
        U["wul_landzsas"]["hp"] = 125
        U["wul_tomahawkos"]["hp"] = 105
        B["wul_tipi"]["cost"] = {"wood": 65}
    wr(p, d)
print("gazdaság: fa, arany, meteorit; létszám fajonként")

# a 3. fajváltozat (képességek, faji varázslatrendszerek) ráépül
import runpy  # noqa: E402
runpy.run_path(os.path.join(ROOT, "tools", "factions_v3.py"), run_name="__main__")
