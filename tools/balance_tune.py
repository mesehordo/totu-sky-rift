"""Balanszhangolás – a gépi meccs-mátrix (tools/balance.py) eredményei alapján.

    python tools/balance_tune.py     (a factions_v3.py láncának végén automatikusan lefut)

Abszolút értékeket állít be (nem szoroz), így többször lefuttatva is ugyanazt adja.
v7 (30 meccs): micélia 90%, wullof 60%, elfek 50%, croftian 30%, ursan 20%, Vasbirodalom 20%.
v11 (84 meccs, 7 főfaj): wullof 79%, Vasbirodalom 71%, ursan 50%, croftian 38%, Huk'ucha 38%, Ooni 25%, elfek 21%.
  -> gyűjtési arányok a ténylegesen elköltött nyersanyaghoz (elf/croftian/Huk'ucha/Ooni: több fa, kevesebb arany/meteorit),
     wullof lándzsás és emu drágább/lassabb, Vasbirodalom légiós/ballistarius kicsit gyengébb, elf tollas harcos erősebb.
v12 (84 meccs): croftian 79%, Huk'ucha 58%, ursan 54%, elfek 50%, Vasbirodalom 38%, wullof 33%, Ooni 29%.
  -> croftian gyűjtés félúton vissza, emu ára vissza, lándzsás csak lassabb (18 mp), légiós életereje vissza;
     Ooni: még több fa, aranyosabb egység-/lakóház-ár, oni harcos 16 mp.
v13 (84 meccs): Huk'ucha 62%, ursan 54%, elfek 50%, Vasbirodalom 50%, wullof 46%, croftian 38%, Ooni 33%.
v23 (126 meccs, VÉGLEGES gépi hangolás): croftian 61%, Vasbirodalom 56%, Huk'ucha 53%, elfek 50%, Ooni 44%,
  wullof 39%, ursan 36% – minden faj 35–65% között; a további finomhangolás élő játék alapján.
v22 (126 meccs): croftian 67%, Vasbirodalom 53%, Huk'ucha 53%, elfek 47%, Ooni 44%, wullof 39%, ursan 36%.
v21 (126 meccs): croftian 64%, Huk'ucha 61%, Ooni 50%, Vasbirodalom 47%, ursan 39%, elfek 36%, wullof 31%.
  (a gépi meccsek szórása nagy: egy faj 1 mp képzési idő miatt is 30 pontot leng)
v20 (84 meccs): croftian 62%, wullof 62%, elfek 46%, Vasbirodalom 42%, ursan 38%, Huk'ucha 38%, Ooni 38%.
v19 (84 meccs, új kutatásokkal; a rothasztás-plafon 2 fa/mp-nél hibásan alacsony volt): wullof 71%, ursan 62%, elfek 54%,
  Vasbirodalom 54%, Ooni 50%, Huk'ucha 42%, croftian 0% -> plafon 7, Lándzsás (dárdavetéssel) és Pajzshordozó kicsit gyengébb.
v18 (84 meccs, tüskékkel): croftian 67%, ursan 54%, Ooni 50%, elfek 46%, Huk'ucha 46%, Vasbirodalom 42%, wullof 25%.
v17 (84 meccs): croftian 67%, elfek 58%, ursan 54%, Huk'ucha 50%, Vasbirodalom 42%, Ooni 38%, wullof 17%.
v16 (84 meccs): wullof 71%, Huk'ucha 58%, ursan 50%, Ooni 50%, Vasbirodalom 46%, elfek 38%, croftian 8%.
v15 (84 meccs): Huk'ucha 75%, elfek 62%, Vasbirodalom 58%, ursan 50%, Ooni 50%, wullof 25%, croftian 4%.
v14 (elf–Huk nyomkövetés; egyenlő értékű seregben az elf nyer, egyenlő létszámban a Huk'ucha): az elfek ugyanannyit gyűjtenek, de az aranyuk parlagon marad (fás egységek), és a
  Huk'ucha Csúzlis 1 létszámért annyit ér, mint a Jáde-íjász 2-ért -> Csúzlis drágább/gyengébb, Jáde-íjász kevesebb fa,
  elf gyűjtés fásabb; a Tóidéző ingyenes tavi lénye ritkábban és rövidebb időre jön.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")

# egység: {mező: érték}; "damage" az attack.damage-et állítja
UNITS = {
    "ursan": {  # erősebb, olcsóbb közelharc; kevesebb kőigény
        "urs_pajzshordozo": {"hp": 157, "damage": 10, "cost": {"wood": 55, "stone": 15}},
        "urs_dardaor": {"hp": 145, "damage": 12, "cost": {"wood": 65, "stone": 15}},
        "urs_szigonyvezo": {"damage": 14, "cost": {"wood": 75, "stone": 15}},
        "urs_berserker": {"hp": 150},
    },
    "imperium": {  # a légiós ne egye meg az összes követ; tartósabb gépek
        "imp_legios": {"hp": 182, "cost": {"wood": 35, "stone": 30}},   # v17: 42%
        "imp_ballistarius": {"hp": 100, "damage": 14},
        "imp_praetorianus": {"cost": {"stone": 80, "meteor": 40}},
    },
    "mycel": {  # olcsó, hatékony sereg – kicsit gyengébb
        "myc_tuskegomba_lovesz": {"damage": 10},
        "myc_kalapos_or": {"hp": 170},
        "myc_pofeteg": {"cost": {"wood": 40, "stone": 15}},
        "myc_tetemgomba": {"hp": 100, "damage": 8},
    },
    "croftian": {
        "cro_tuskekopo": {"damage": 14, "cost": {"wood": 55, "stone": 25}},
        "cro_karmos": {"hp": 60, "damage": 8},          # v15: 4% -> v17: 67% (+tüskék) -> kicsit vissza
        "cro_vadaszkarmos": {"hp": 100},
    },
    "elves": {
        "elf_tollas_harcos": {"hp": 122, "damage": 13},
        "elf_jade_ijasz": {"damage": 11, "cost": {"wood": 65, "stone": 30}},   # v16: 38% -> vissza
        "elf_raptor_lovas": {"cost": {"wood": 65, "stone": 60}},   # alapsereg: az elfnél a fa szűkös, az arany marad
    },
    "hukucha": {  # a Csúzlis 1 létszámért túl hatékony volt (minden más alap távolsági egység 2)
        "huk_csuzlis": {"hp": 65, "supply": 2, "damage": 9, "cost": {"wood": 60, "stone": 20}},
        "huk_kampokardos": {"hp": 138},                 # v15: 75% -> v20: 38%
    },
}
UNITS["wullof"] = {  # a korai lándzsás-roham túl erős volt
    "wul_landzsas": {"time": 16, "hp": 128, "damage": 11, "cost": {"wood": 57, "stone": 25}},   # 25%->71%->17%->25%
    "wul_tomahawkos": {"hp": 118},
}
UNITS["ooni"] = {  # fa-szűke, felhalmozott arany: az összetétel aranyosabb, az oni harcos gyorsabban kész
    "oon_oni_harcos": {"time": 15, "damage": 15, "cost": {"wood": 60, "stone": 30}},   # v17/v20: 38%
    "oon_kohajito": {"cost": {"wood": 65, "stone": 40}},
    "oon_nian_or": {"cost": {"wood": 100, "stone": 70}},
}
# épületek: {mező: érték}; "damage"/"range" az attack-ot állítja
BUILDINGS = {
    "ursan": {"urs_tanya": {"cost": {"wood": 95, "stone": 20}}},           # a legdrágább létszám volt (160/10)
    "imperium": {"imp_reaktor": {"cost": {"stone": 80, "wood": 30}}},      # 140/10 -> 110/10
    "mycel": {"myc_szivgomba": {"damage": 4, "range": 5}},
    "elves": {"elf_obeliszk": {"cost": {"wood": 55, "stone": 25}}},
    # v16: a croftian 4–8%: egyetlen Kaptár lárváiból képez, lassan indul -> több lárva, olcsóbb Ikrafészek
    "croftian": {"cro_kaptar": {"larva": 4, "larva_time": 5.5}, "cro_ikrafeszek": {"cost": {"wood": 140, "stone": 40}}},     # 75 arany: aranyhiányban elakadt a létszám
    "ooni": {"oon_lakohaz": {"cost": {"wood": 75, "stone": 30}}},                # a gép 7–10-et épít: bázisszerte tornyok
}
# képességek (abszolút értékek a data/abilities.json-ban)
ABILITIES = {
    "sporatuske": {"damage": 30, "cooldown": 9},   # micélia: túl erős, ölésből újratermelődő távolsági csapás
    "sporarobbanas": {"damage": 45},
    "huk_tavi_leny_hivas": {"cooldown": 45, "lifetime": 30},   # Huk'ucha: folyamatos ingyen fal volt
    "huk_nagy_tavi_leny_hivas": {"faith": 50, "cooldown": 45,
                                 "desc": "Erősebb tavi lényt idéz 60 mp-re (+50% életerő és sebzés). Ára 50 Tóének."},
}
# gépi ellenfél gyűjtési aránya: a ténylegesen elköltött nyersanyagokhoz igazítva
GATHER = {
    "ursan": {"wood": 0.55, "stone": 0.32, "meteor": 0.13},
    "imperium": {"wood": 0.36, "stone": 0.50, "meteor": 0.14},
    "elves": {"wood": 0.62, "stone": 0.26, "meteor": 0.12},
    "croftian": {"wood": 0.48, "stone": 0.33, "meteor": 0.19},
    "hukucha": {"wood": 0.60, "stone": 0.27, "meteor": 0.13},
    "ooni": {"wood": 0.64, "stone": 0.23, "meteor": 0.13},
}
# egyéb gépi beállítások (abszolút)
AI_CFG = {
    "elves": {"mine_max": 2},   # 3 Napfúró × 3 munkás: parlagon heverő arany, fa-hiány, kis sereg
    "croftian": {"mine_max": 3, "build_order": ["cro_tapgomba", "cro_ikrafeszek", "cro_tuskeverem", "cro_tapgomba",
                                                 "cro_kaptar", "cro_korus_kut", "cro_diplomacia", "cro_tuskes_gyoker",
                                                 "cro_csontcsucs"],
                 "army": {"cro_karmos": 3, "cro_tuskekopo": 4, "cro_savkopo": 1, "cro_szarnyas_rem": 1}},
}
# irány-szabályok (abszolút)
PATH_RULES = {
    ("mycel", "rothadas"): {"raise_from_kills": {"unit": "myc_tetemgomba", "chance": 0.12, "lifetime": 30,
                                                 "color": [170, 200, 120]}},
}


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


def main():
    ap = os.path.join(ROOT, "data", "abilities.json")
    ab = rd(ap)
    for k, ch in ABILITIES.items():
        if k in ab:
            ab[k].update(ch)
    wr(ap, ab)
    for fn in sorted(os.listdir(FDIR)):
        fid = fn[:-5]
        p = os.path.join(FDIR, fn)
        d = rd(p)
        U = d.get("units", {})
        for uid, ch in UNITS.get(fid, {}).items():
            if uid not in U:
                continue
            for k, v in ch.items():
                if k == "damage":
                    if U[uid].get("attack"):
                        U[uid]["attack"]["damage"] = v
                else:
                    U[uid][k] = v
        B = d.get("buildings", {})
        for bid, ch in BUILDINGS.get(fid, {}).items():
            if bid not in B:
                continue
            for k, v in ch.items():
                if k in ("damage", "range"):
                    if B[bid].get("attack"):
                        B[bid]["attack"][k] = v
                else:
                    B[bid][k] = v
        if fid in GATHER:
            d.setdefault("ai", {})["gather_ratio"] = dict(GATHER[fid])
        if fid in AI_CFG:
            d.setdefault("ai", {}).update(AI_CFG[fid])
        for (f2, pid), rules in PATH_RULES.items():
            if f2 == fid and pid in d.get("paths", {}):
                d["paths"][pid].setdefault("rules", {}).update(rules)
        wr(p, d)
    print("balansz: hangolás alkalmazva")


if __name__ == "__main__":
    main()
