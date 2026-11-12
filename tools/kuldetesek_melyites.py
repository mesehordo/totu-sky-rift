"""A régi küldetések mélyítése: a csak gyűjtés / ölés / kitartás célú pályák változatosabb, helyhez és
történethez kötött feladatot kapnak, a Micélia- és a Kethri-hadjárat pedig saját, egyedi térképeket.

    python tools/kuldetesek_melyites.py     (a tools/kuldetesek_atdolgozas.py helyett ezt futtasd)

A célok azonosítója marad (a régi események és jelzők rájuk épülnek), csak a feladat változik.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kuldetesek_atdolgozas as K  # noqa: E402
from kuldetesek_atdolgozas import S, base_res, make_layout, say  # noqa: E402


def upd(key, **kw):
    S[key].update(kw)


# ---- Előjáték: a víz, az áram és a mentőcsapat helyhez kötve
upd(("egsz_0_elojatek", "p0_masodik_lelegzet"),
    replace={"viz": {"type": "guard", "at": "kut", "radius": 2, "seconds": 8, "text": "Indítsd újra a kútszivattyút (Livia vagy egy munkás álljon mellette)",
                     "marker": "Kút"},
             "energia": {"type": "build_at", "target": "imp_reaktor", "at": "korhaz", "radius": 12, "count": 2, "after": "viz",
                         "text": "Építs 2 Lélekreaktort a kórház köré", "marker": "Kórház"}},
    extra_obj=[{"id": "mentok", "type": "escort", "optional": True, "activate_near": {"at": "muhely", "r": 6}, "tag": "mentok",
                "min": 2, "at": "korhaz", "radius": 6, "text": "Kísérd a műhelyben rekedt mentőcsapatot a kórházba", "marker": "Kórház"}],
    extra_trig=[{"id": "mentok_csatl", "on_enter": {"at": "muhely", "r": 5}, "join": "mentok",
                 "say": [say("Livia", "Ti is jöttök. Minden kéz kell, ami még meleg.")]}],
    allies=[{"faction": "imperium", "name": "Mentőcsapat", "units": [{"id": "imp_fabrikator", "n": 3, "at": "muhely", "tag": "mentok"}]}])

# ---- Hétfolyó vására: a raktárak a vásártér körül, az útkereszteződés biztosítása
upd(("egsz_1_wullof", "w1_hetfolyo_vasara"),
    replace={"raktar": {"type": "build_at", "target": "wul_tipi", "at": "vasar", "radius": 12, "count": 3,
                        "text": "Állíts fel 3 Kéregkunyhót raktárnak a vásártér körül", "marker": "Vásártér"},
             "ut": {"type": "guard", "at": "raktar3", "radius": 2, "seconds": 10, "after": "raktar",
                    "text": "Biztosítsd az északi útkereszteződést", "marker": "Útkereszteződés"}})

# ---- Üres magtárak: a beengedett menekültek kísérése (csak ha Ragna beengedte őket)
upd(("egsz_2_ursan", "u1_ures_magtarak"),
    extra_obj=[{"id": "menekultek", "type": "escort", "hidden": True, "optional": True, "tag": "menekult", "min": 3, "at": "magtar",
                "radius": 6, "text": "Kísérd a beengedett menekülteket a magtárak mellé", "marker": "Magtár"}],
    extra_trig=[{"id": "menekult_jon", "if_flag": "ragna_kapu", "every": 1, "max": 1, "activate": "menekultek",
                 "spawn": {"owner": 0, "near": "kapu", "tag": "menekult", "units": {"urs_telepes": 4}},
                 "say": [say("Ragna", "Befelé. Ne a kaput nézzétek, hanem a tüzet.")]}])

# ---- A felolvadt bánya: a két tárna lezárása ölésszámláló helyett
upd(("egsz_2_ursan", "u2_felolvadt_banya"),
    replace={"tisztit": {"type": "destroy", "targets": ["tarna"], "text": "Zárd le a két fertőzött tárnát (pusztítsd el a fészkeiket)"}},
    enemies=[{"buildings": [{"id": "cro_ikrafeszek", "at": "tarna1", "tag": "tarna"}, {"id": "cro_ikrafeszek", "at": [50, 36], "tag": "tarna"}]}])

# ---- Tojások a hamu alatt: a fáklyások megállítása külön cél
upd(("egsz_2_ursan", "u3_tojasok_a_hamu_alatt"),
    extra_obj=[{"id": "faklyasok", "type": "destroy", "hidden": True, "targets": ["faklyas"],
                "text": "Állítsd meg a fáklyásokat, mielőtt a kamrákhoz érnek"}],
    trig_patch={1: {"activate": "faklyasok", "spawn": {"owner": 1, "tag": "faklyas", "units": {"urs_berserker": 3, "urs_pajzshordozo": 2}}}})

# ---- A tiszta érc: a két tárna leállítása helyben
upd(("egsz_3_elfek", "e1_tiszta_erc"),
    replace={"leallit": {"type": "guard", "at": "banya2", "radius": 2, "seconds": 8,
                         "text": "Állítsd le a szabálytalan tárnát: Szelai álljon a bejáratnál", "marker": "Tárna"}})

# ---- Az égő kert: két vágópont a hálózaton, a kitartás előtt
upd(("egsz_4_gombanep", "g5_az_ego_kert"),
    extra_obj=[{"id": "vagas1", "type": "guard", "at": "goc1", "radius": 2, "seconds": 6, "text": "Vágd el a keleti fonalköteget", "marker": "Vágópont"},
               {"id": "vagas2", "type": "guard", "at": "goc2", "radius": 2, "seconds": 6, "text": "Vágd el az északi fonalköteget", "marker": "Vágópont"}])

# ---- A finomító: a reaktorok a finomító körül
upd(("egsz_4b_finomito", "x2_finomito"),
    replace={"reaktor": {"type": "build_at", "target": "imp_reaktor", "at": "finomito", "radius": 12, "count": 3,
                         "text": "Indíts újra 3 Lélekreaktort a finomító körül", "marker": "Finomító"}})

# ---- A lefoglalt szállítmány: a szekerek visszakísérése ölésszámláló helyett
upd(("egsz_5_aureth", "a1_lefoglalt_szallitmany"),
    replace={"visszaver": {"type": "escort", "tag": "szallitmany", "min": 2, "at": "tabor", "radius": 7,
                           "text": "Kísérd vissza az ércszállító Aratókat a táborba", "marker": "Tábor"}},
    allies=[{"faction": "imperium", "name": "Lefoglalt szállítmány", "units": [{"id": "imp_arato", "n": 3, "at": "szallitmany", "tag": "szallitmany"}]}],
    extra_trig=[{"id": "szallitmany_csatl", "on_enter": {"at": "szallitmany", "r": 5}, "join": "szallitmany",
                 "say": [say("Livia", "Megvannak a szekerek. Most vissza – lassan, hogy senki ne kövessen dühből.")]}])

# ---- Az üres serlegek városa: előbb a kapu bezárása, aztán a kitartás
upd(("egsz_5_aureth", "a2_ures_serlegek_varosa"),
    extra_obj=[{"id": "kapu", "type": "guard", "at": "kapu", "radius": 3, "seconds": 8, "text": "Zárd le a városkaput, mielőtt a roham odaér",
                "marker": "Városkapu"}])

# ---- Lélekreaktorok az átálláshoz: a reaktorpontnál
upd(("egsz_5_aureth", "a5_feltoltes_megtagadva"),
    replace={"atallit": {"type": "build_at", "target": "imp_reaktor", "at": "reaktor", "radius": 10, "count": 3,
                         "text": "Építs 3 független Lélekreaktort a lázadó negyedben", "marker": "Lázadó negyed"}})

# ---- Micéliák: saját térképek
G = "egsz_4_gombanep"
MIC = {
    "g1_szaraz_kertek": dict(seed=5601, p={"kert": [14, 44], "forras": [50, 10], "szurdok": [32, 28], "goc1": [46, 34], "goc2": [36, 10],
                                          "melyseg": [56, 46], "emlekkert": [50, 10]},
                             water=[[[50, 6], 3]], rocks=[[[26, 14], 6], [[44, 24], 4]]),
    "g2_az_emlek_ara": dict(seed=5602, p={"kert": [10, 30], "goc1": [40, 14], "goc2": [44, 44], "szurdok": [28, 30], "melyseg": [58, 30],
                                         "emlekkert": [52, 28]}, water=[[[24, 44], 3]], rocks=[[[36, 30], 5]]),
    "g3_az_elso_betegek": dict(seed=5603, p={"kert": [8, 48], "emlekkert": [56, 8], "szurdok": [24, 34], "goc1": [40, 22], "goc2": [20, 12],
                                            "melyseg": [56, 46]}, water=[[[38, 40], 4]], rocks=[[[34, 10], 5], [[46, 34], 4]]),
    "g4_egy_nev_a_korusban": dict(seed=5604, p={"kert": [32, 48], "emlekkert": [32, 8], "szurdok": [32, 28], "goc1": [14, 20], "goc2": [50, 20],
                                               "melyseg": [56, 6]}, water=[[[20, 38], 3], [[44, 38], 3]], rocks=[[[32, 18], 3]]),
    "g5_az_ego_kert": dict(seed=5605, p={"kert": [32, 30], "emlekkert": [32, 30], "szurdok": [20, 30], "goc1": [46, 30], "goc2": [32, 14],
                                        "melyseg": [56, 50]}, water=[[[14, 44], 4]], rocks=[[[22, 14], 4], [[44, 46], 5]]),
}
for mid, d in MIC.items():
    p = d["p"]
    roads = [("kert", "szurdok"), ("szurdok", "goc1"), ("szurdok", "goc2"), ("szurdok", "emlekkert"), ("goc1", "melyseg")]
    S[(G, mid)]["layout"] = make_layout(d["seed"], [64, 56], p, ["kert", "melyseg"], roads=roads, rocks=d["rocks"], water=d["water"],
                                        clumps=10, res=base_res(p["kert"], 5),
                                        ambient=[{"id": "myc_sporaszedo", "n": 3, "at": [p["kert"][0] + 4, p["kert"][1] + 4], "mode": "idle",
                                                  "shelter": "kert"}])

# ---- Kethri: saját térképek és feladatok
C = "egsz_6_keth"
KETH = {
    "c1_sebek_egyesitese": dict(seed=6101, p={"feszek": [10, 46], "cel": [50, 14], "ter": [30, 30], "kapu": [56, 48], "dal": [40, 22]},
                                ridges=[[[[20, 20], [36, 12]], 2]], decor=[{"id": "wul_tipi", "at": [46, 20]}, {"id": "wul_tipi", "at": [54, 18]}]),
    "c2_ismeros_kovek": dict(seed=6202, p={"feszek": [8, 48], "cel": [48, 10], "ter": [36, 26], "kapu": [56, 48]},
                             ridges=[[[[24, 34], [40, 34]], 2], [[[40, 18], [56, 18]], 2]], decor=[{"id": "elf_obeliszk", "at": [38, 22], "ruin": True}]),
    "c3_a_visszatero_nev": dict(seed=6303, p={"feszek": [54, 48], "cel": [12, 10], "ter": [30, 28], "kapu": [56, 8], "keltetoe": [42, 40]},
                                ridges=[[[[16, 24], [28, 18]], 2]], decor=[{"id": "urs_tanya", "at": [20, 16]}]),
    "c4_levalas": dict(seed=6404, p={"feszek": [32, 48], "cel": [32, 6], "ter": [32, 30], "kapu": [56, 10], "visszhang": [56, 10]},
                       ridges=[[[[10, 30], [24, 30]], 3], [[[40, 30], [54, 30]], 3]], decor=[]),
    "c5_a_belso_kapu": dict(seed=6505, p={"feszek": [10, 10], "cel": [52, 46], "ter": [30, 28], "kapu": [56, 8]},
                            ridges=[[[[20, 40], [40, 40]], 2]], decor=[]),
}
for mid, d in KETH.items():
    p = d["p"]
    S[(C, mid)]["layout"] = make_layout(d["seed"], [64, 56], p, ["feszek", "cel"], roads=[("feszek", "ter"), ("ter", "cel"), ("ter", "kapu")],
                                        ridges=d["ridges"], clumps=9, res=base_res(p["feszek"], 5), decor=d["decor"])
upd((C, "c1_sebek_egyesitese"),
    replace={"bekebelez": {"type": "destroy", "targets": ["wul_tanacssator"], "text": "Kebelezd be a határtelepet: döntsd le a Tanácskunyhót"}},
    extra_obj=[{"id": "dal", "type": "reach", "optional": True, "tag": None, "units": ["cro_hos_keth"], "at": "dal", "radius": 3,
                "text": "(Keth emléke) Állj meg a régi kereskedőhelynél", "marker": "Egy dal emléke"}],
    extra_trig=[{"id": "dal_emlek", "on_objective": "dal", "say": [say("Keth", "Itt énekelt nekem egy Wullof kereskedő. Fizetség helyett."),
                                                                   say("Kórus", "Az emlék zaj. Haladj.")]}])
upd((C, "c3_a_visszatero_nev"),
    extra_obj=[{"id": "keltetoe", "type": "reach", "optional": True, "units": ["cro_hos_keth"], "at": "keltetoe", "radius": 3,
                "text": "(Keth) Nézz be az „üres” keltetőkamrába", "marker": "Keltetőkamra"}],
    extra_trig=[{"id": "keltetoe_say", "on_objective": "keltetoe", "say": [say("Keth", "Nem készlet. Gyerekek. Megjegyzem az arcukat.")]}])
upd((C, "c5_a_belso_kapu"),
    replace={"irt": {"type": "guard", "at": "ter", "radius": 3, "seconds": 10, "text": "Keth olvassa ki az összekapcsolási mintát a csomópontnál",
                     "marker": "Csomópont"}})

# a szövetséges (bázis nélküli) csoportoknak is kell kezdőhely
S[("egsz_0_elojatek", "p0_masodik_lelegzet")]["layout"]["starts"].append("muhely")
S[("egsz_5_aureth", "a1_lefoglalt_szallitmany")]["layout"]["starts"].append("szallitmany")

if __name__ == "__main__":
    K.main()
