"""Új alapfrakció-mechanikák és fajok (docs/UJ_FAJOK_TERV.md).

    python tools/factions_new.py      (a factions_v3.py láncában fut, az allies.py előtt)

* Vasbirodalom – alapozás (Dune 2000-szerű): 2×2-es márvány-lapkő; ami teljesen rajta áll, teljes életerővel épül fel,
  ami nem, az fél életerővel (Fabrikátorral javítható).
Abszolút értékeket ír, többször futtatva is ugyanazt adja.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


def imperium_foundation():
    p = os.path.join(FDIR, "imperium.json")
    d = rd(p)
    # Ötletdoboz Ö3: a lapkövet a Fabrikátor rakja le; Nagy alapozás (4×4) kutatással; alapozás nélkül kopik
    d["foundation"] = {"size": 2, "cost": {"stone": 10}, "unfounded_hp": 0.5,
                       "decay": {"pct_per_s": 0.002, "min": 0.5}}
    B = d["buildings"]
    B["imp_alapozas"] = {
        "name": "Alapozás", "desc": "Márvány-lapkő (2×2): a Fabrikátor 3 mp alatt rakja le, a mozgást nem akadályozza. "
                                    "Ami teljes egészében alapozáson áll, teljes életerővel épül fel; ami nem, az fél "
                                    "életerővel, és lassan kopik is (fél életerőig).",
        "cost": {"stone": 10}, "time": 3, "size": [2, 2], "hp": 1, "armor": 0, "sight": 0, "shape": "foundation",
        "foundation_tile": True, "no_sprite": True}
    B["imp_nagy_alapozas"] = {
        "name": "Nagy alapozás", "desc": "Márvány-lapkő (4×4): csempénként olcsóbb, a Fabrikátor 6 mp alatt rakja le. "
                                         "(A Nagy alapozás kutatás kell hozzá az Öntödében.)",
        "cost": {"stone": 30}, "time": 6, "size": [4, 4], "hp": 1, "armor": 0, "sight": 0, "shape": "foundation",
        "foundation_tile": True, "no_sprite": True, "requires_research": ["imp_nagy_alapozas"]}
    if "imp_nagy_alapozas" not in d["buildings"].get("imp_ontode", {}).get("researches", []) or True:
        on = B["imp_ontode"]
        on["researches"] = [x for x in (on.get("researches") or []) if x != "imp_nagy_alapozas"] + ["imp_nagy_alapozas"]
    d.setdefault("researches", {})["imp_nagy_alapozas"] = {
        "name": "Nagy alapozás", "desc": "A Fabrikátorok 4×4-es márvány-lapkövet is lerakhatnak (30 arany, 6 mp): "
                                         "csempénként olcsóbb. A meglévő lapkövek nem változnak.",
        "cost": {"stone": 100, "meteor": 40}, "time": 40, "effects": []}
    for bid, b in B.items():  # a falaknak és az alapozásnak nem kell alapozás
        if b.get("shape") in ("wall",) or b.get("foundation_tile"):
            b["foundation_free"] = True
    ai = d.setdefault("ai", {})
    ai["foundation"] = True
    wr(p, d)


def builder_styles(abilities):
    """Ötletdoboz Ö16: minden faj másképp épít (docs/OTLETDOBOZ_TERV.md).
    stay = ott marad, több munkás gyorsít (Ursan) · stay_single = ott marad, egy épületen egy dolgozik ·
    enter = beköltözik az épületbe (Wullof) · consume = beleolvad (Croftian) · leave = elindítja és elmegy (elf, Huk’ucha)."""
    abilities["tulhajtas"] = {
        "name": "Túlhajtás", "type": "overdrive", "hotkey": "T", "cooldown": 20, "self_hp": 0.4, "share": 0.5,
        "color": [255, 170, 80], "icon": "rage",
        "desc": "A Fabrikátor túlhajtja magát: annak az épületnek, amelyen dolgozik, azonnal elkészül a hátralévő "
                "építés fele (javításnál a hiányzó életerő fele). Ára a Fabrikátor életerejének 40%-a – bele is "
                "pusztulhat."}
    sets = {
        "wullof": ({"builder": "enter"}, "wul_gyujtogeto",
                   "Munkás. Gyűjt, javít; építéskor beköltözik a félkész kunyhóba, ott védett, és csak készen jön ki. "
                   "Fonott háti kosárban hordja a zsákmányt."),
        "imperium": ({"builder": "stay_single"}, "imp_fabrikator",
                     "Pókszerű munkásgép. Épít (egy épületen egyszerre csak egy Fabrikátor dolgozhat – Túlhajtással "
                     "gyorsíthat), gyűjt, alapoz, és javítja az épületeket és a gépeket."),
        "hukucha": ({"builder": "leave", "pond_build_bonus": 0.6}, "huk_toaso",
                    "Munkás: tavacskát ás (2×2 – erre építhet a nép), gyűjt, és elindítja az építkezést. Az épület "
                    "magától nő; minél több tavacska veszi körül, annál gyorsabban (legfeljebb +60%)."),
        "ooni": ({"builder": "stay_single", "build_boost": {"unit": "oon_dobos", "radius": 4, "mult": 1.5}},
                 "oon_lampasvivo",
                 "Kis oni munkás: papírlámpást állít (a nép csak lámpásfényben építhet), gyűjt, és végig ott marad az "
                 "építkezésen. Több munkás nem gyorsít, de egy közeli Taiko-dobos ritmusa +50%-kal igen."),
    }
    for fid, (keys, worker, desc) in sets.items():
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        d.pop("build_boost", None)
        d.pop("pond_build_bonus", None)
        d.update(keys)
        w = d["units"][worker]
        w["desc"] = desc
        if fid == "imperium":
            abl = w.setdefault("abilities", [])
            if "tulhajtas" not in abl:
                abl.append("tulhajtas")
        wr(p, d)


def gatherer_split():
    """Ötletdoboz Ö12 + Ö13: külön építő és külön gyűjtő.
    Vasbirodalom: a Fabrikátor épít és fát hord; az aranyat és a meteoritot a nagy rakományú Arató viszi
    (Fórumhoz vagy a Finomítóba, ahol a két kizáró Arató-fejlesztés közül választani kell).
    Ooni: a Lámpásvivő csak épít; fát a Favágó oni, aranyat és meteoritot az álcázott Tanuki-bányász gyűjt.
    Egységmezők: gathers (mit gyűjthet), builds (építhet-e), carry (egyedi rakomány), mine_stealth, find_items."""
    p = os.path.join(FDIR, "imperium.json")
    d = rd(p)
    U, B, R = d["units"], d["buildings"], d.setdefault("researches", {})
    fab = U["imp_fabrikator"]
    fab["gathers"] = ["wood"]
    fab["desc"] = ("Pókszerű munkásgép. Épít (egy épületen egyszerre csak egy Fabrikátor dolgozhat – Túlhajtással "
                   "gyorsíthat), alapoz, fát hord, és javítja az épületeket és a gépeket. Aranyat és meteoritot az "
                   "Arató gyűjt.")
    U["imp_arato"] = unit("Arató", "Lánctalpas gyűjtőgép: aranyat és meteoritot bányászik, egyszerre nagy rakománnyal "
                                   "(30 arany vagy 20 meteorit). Lassú és drága – egy kilőtt Arató egy teljes rakomány.",
                          {"wood": 100, "stone": 50}, 2, 30, 300, 3, 2.1, 7, 0.55, "worker", worker=True, builds=False,
                          gathers=["stone", "meteor"], carry={"stone": 30, "meteor": 20},
                          tags=["worker", "mech", "large"])
    hq = B["imp_forum"]
    hq["produces"] = [x for x in hq.get("produces", []) if x != "imp_arato"] + ["imp_arato"]
    B["imp_finomito"] = bld("Finomító", "Lerakóhely aranyhoz és meteorithoz – érdemes a bányák mellé építeni. Itt "
                                       "választható az Arató fejlesztése: gyorsabb hajtómű VAGY nagyobb tartály.",
                            {"wood": 100, "stone": 40}, 25, (2, 2), 600, "workshop", dropoff=["stone", "meteor"],
                            researches=["imp_hajtomu", "imp_tartaly"])
    R["imp_hajtomu"] = {"name": "Gyorsított hajtómű", "desc": "+25% sebesség az Aratóknak. (Kizárja a Bővített "
                        "tartályt.)", "cost": {"stone": 100, "meteor": 40}, "time": 45, "exclusive": "arato_fejlesztes",
                        "effects": [{"stat": "speed_pct", "tags": ["imp_arato"], "add": 25}]}
    R["imp_tartaly"] = {"name": "Bővített tartály", "desc": "+50% rakomány az Aratóknak. (Kizárja a Gyorsított "
                        "hajtóművet.)", "cost": {"stone": 100, "meteor": 40}, "time": 45, "exclusive": "arato_fejlesztes",
                        "effects": [{"stat": "carry_pct", "tags": ["imp_arato"], "add": 50}]}
    d["start_units"] = {"imp_fabrikator": 4, "imp_arato": 1}
    d["gatherers"] = {"stone": "imp_arato", "meteor": "imp_arato"}
    ai = d.setdefault("ai", {})
    ai["gatherer_share"] = {"imp_arato": 3}  # egy Arató ennyi munkásnyi bevételt ér
    bo = [x for x in ai.get("build_order", []) if x != "imp_finomito"]
    if "imp_legiogyar" in bo:
        bo.insert(bo.index("imp_legiogyar") + 1, "imp_finomito")
    ai["build_order"] = bo
    ro = [x for x in ai.get("research_order", []) if x != "imp_tartaly"]
    ai["research_order"] = ["imp_tartaly"] + ro
    wr(p, d)

    p = os.path.join(FDIR, "ooni.json")
    d = rd(p)
    U, B = d["units"], d["buildings"]
    lv = U["oon_lampasvivo"]
    lv["gathers"] = []
    lv["desc"] = ("Kis oni munkás: papírlámpást állít (a nép csak lámpásfényben építhet), épít, és végig ott marad az "
                  "építkezésen. Nem gyűjt. Több munkás nem gyorsít, de egy közeli Taiko-dobos ritmusa +50%-kal igen.")
    U["oon_favago"] = unit("Favágó oni", "Erős, széles vállú oni baltával: csak fát gyűjt, de egyszerre sokat (12). "
                                         "Harcban gyenge.", {"wood": 55}, 1, 13, 95, 0, 2.4, 7, 0.4, "worker",
                           melee(6, 1.4), worker=True, builds=False, gathers=["wood"], carry={"wood": 12},
                           tags=["worker", "organic"])
    U["oon_tanuki"] = unit("Tanuki-bányász", "Alakváltó tanuki: aranyat és meteoritot bányászik. A bányák közelében, "
                                             "amíg nem harcol, álcázva dolgozik; néha egy-egy tárgyat is talál a "
                                             "mélyben (a leadóhelynél hagyja).", {"wood": 50, "stone": 10}, 1, 13, 55,
                           0, 2.7, 7, 0.32, "worker", melee(4, 1.4), worker=True, builds=False,
                           gathers=["stone", "meteor"], mine_stealth=2,
                           find_items={"chance": 0.02, "pool": ["gyogyfozet", "eterfozet", "csizma", "regengyuru"]},
                           tags=["worker", "organic"])
    hq = B["oon_nian_csarnok"]
    hq["produces"] = ["oon_lampasvivo", "oon_favago", "oon_tanuki"]
    hq["desc"] = ("Márványoszlopok, ívelt cseréptető, faragott nian-őrzők. Leadóhely; Lámpásvivőket, Favágó onikat és "
                  "Tanuki-bányászokat nevel; fénye körül építhet a nép.")
    # Taiko-dobos: alapegység az Edzőudvarból; a Vörös buzogány irány erősebb dobokat ad
    dob = U["oon_dobos"]
    dob.pop("path", None)
    dob["desc"] = "Harci dobjával bátorítja az onikat, és ritmusával +50%-kal gyorsítja a közeli építkezést."
    eu = B["oon_edzoudvar"]
    eu["produces"] = [x for x in eu["produces"] if x != "oon_dobos"] + ["oon_dobos"]
    eu["desc"] = "Oni harcosokat, kőhajítókat, nian őrzőket és taiko-dobosokat képez."
    vd = B["oon_voros_dojo"]
    vd["produces"] = [x for x in vd["produces"] if x != "oon_dobos"]
    vd["desc"] = "Tomboló onikat képez; a dódzsó dobjai erősebbek."
    pv = d["paths"]["voros"]
    pv["units"] = [x for x in pv["units"] if x != "oon_dobos"]
    pv["desc"] = ("Tomboló onik és harci dobok: a közelharcosok erősebbek és gyorsabbak, a Taiko-dobosok szívósabbak, és "
                  "ritmusuk +75%-kal gyorsítja az építkezést. Korai, kemény nyomás.")
    eff = [e for e in pv.get("effects", []) if e.get("tags") != ["oon_dobos"]]
    pv["effects"] = eff + [{"stat": "hp", "tags": ["oon_dobos"], "add": 60}]
    pv.setdefault("rules", {})["build_boost_mult"] = 1.75
    d["start_units"] = {"oon_lampasvivo": 2, "oon_favago": 2, "oon_tanuki": 1}
    d["gatherers"] = {"wood": "oon_favago", "stone": "oon_tanuki", "meteor": "oon_tanuki"}
    ai = d.setdefault("ai", {})
    ai["builders"] = 3
    wr(p, d)


def otletdoboz_batch2(abilities):
    """Ötletdoboz Ö4–Ö11 (docs/OTLETDOBOZ_TERV.md)."""
    def edit(fid, fn):
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        fn(d)
        wr(p, d)

    def add_res(d, bid, rid):
        b = d["buildings"][bid]
        b["researches"] = [x for x in b.get("researches", []) if x != rid] + [rid]

    # Ö4 Napfúró – Szürke elfek
    def elves(d):
        d["buildings"]["elf_napfuro"] = bld(
            "Napfúró", "A bányára (arany vagy meteorit) idézett fúrótorony: magától is termel, és legfeljebb 3 Kőfaragó "
            "dolgozhat benne, hordás nélkül (jobb klikk a Napfúróra). Ha lerombolják, a bent lévők sérülten kiugranak.",
            {"wood": 120, "stone": 40}, 30, (2, 2), 700, "tower", on_resource=["stone", "meteor"],
            mine={"passive": 0.4, "slots": 3}, no_power=True)
        d.setdefault("ai", {})["mine_building"] = "elf_napfuro"
    edit("elves", elves)

    # Ö5 Fertőzött tárna + Ö11 Rothasztás – Croftianok
    def croftian(d):
        d["buildings"]["cro_tarna"] = bld(
            "Fertőzött tárna", "A bányára (arany vagy meteorit) növesztett tárna. A Csápszolgák beleolvadnak (jobb klikk "
            "a tárnára, legfeljebb 5): mindegyik végleg egy munkásnyi hozamot ad, hordás nélkül. Ha a tárna elpusztul "
            "vagy a bánya kimerül, a beolvasztott szolgák is elvesznek.", {"wood": 75}, 25, (2, 2), 500, "hive",
            on_resource=["stone", "meteor"], mine={"passive": 0, "slots": 5, "consume": True}, creep_radius=3,
            no_creep=True)
        w = d["units"]["cro_csapszolga"]
        w["gathers"] = []
        w["desc"] = ("Munkás. Épületté alakul át, vagy beleolvad egy Fertőzött tárnába (elfogy). Nem gyűjt: a fát a "
                     "mélység rothasztja el, az aranyat és a meteoritot a tárnák adják.")
        d["creep_rot"] = {"rate": 0.06, "hq_mult": 2.5, "hq_radius": 9}
        d.setdefault("ai", {})["mine_building"] = "cro_tarna"
    edit("croftian", croftian)

    # Ö10 Kéregcsapolás + zsákmány-arany – Wullof
    def wullof(d):
        w = d["units"]["wul_gyujtogeto"]
        w["tap_wood"] = {"amount": 8, "time": 10, "drain": 0.5}
        w["desc"] = ("Munkás. Fát csapol: a fához kötődik, és hordás nélkül, lassan, folyamatosan jön a fa (egy fán egy "
                     "csapoló). Aranyat hord, javít; építéskor beköltözik a félkész kunyhóba.")
        d.setdefault("researches", {})["wul_zsakmanyszerzes"] = {
            "name": "Zsákmányszerzés", "desc": "A vadászzsákmány (ölésből) aranyat is hoz: a fa-zsákmány felét.",
            "cost": {"wood": 100, "stone": 60}, "time": 45, "effects": [], "rules": {"bounty_gold": 0.5}}
        add_res(d, "wul_tanacssator", "wul_zsakmanyszerzes")
    edit("wullof", wullof)

    # Ö8 Népfelkelés + Fegyverbe! – Ursanok
    abilities["fegyverbe"] = {
        "name": "Fegyverbe!", "type": "self_buff", "hotkey": "F", "cooldown": 90, "duration": 45, "drop_work": True,
        "modifiers": {"damage_mult": 1.6, "armor_add": 2, "speed_mult": 1.15}, "icon": "rage",
        "desc": "Riadó: a telepesek 45 mp-re fegyvert ragadnak (erősebbek, gyorsabbak, páncélosabbak), de közben nem "
                "dolgoznak; utána visszamennek a munkájukhoz."}

    def ursan(d):
        d.setdefault("researches", {})["urs_nepfelkeles"] = {
            "name": "Népfelkelés", "desc": "A telepesek harcosként is megállják a helyüket: +5 sebzés, +40 életerő, "
            "+1 páncél; és megnyílik a „Fegyverbe!” riadó.", "cost": {"wood": 50, "stone": 75}, "time": 40,
            "effects": [{"stat": "damage", "tags": ["urs_telepes"], "add": 5},
                        {"stat": "hp", "tags": ["urs_telepes"], "add": 40},
                        {"stat": "armor", "tags": ["urs_telepes"], "add": 1}]}
        add_res(d, "urs_nagyterem", "urs_nepfelkeles")
        w = d["units"]["urs_telepes"]
        abl = [a for a in w.get("abilities", []) if a != "fegyverbe"] + ["fegyverbe"]
        w["abilities"] = abl
        w.setdefault("ability_research", {})["fegyverbe"] = "urs_nepfelkeles"
    edit("ursan", ursan)

    # Ö6 Torii-kapupár – Ooni
    def ooni(d):
        d["buildings"]["oon_torii"] = bld(
            "Torii-kapu", "Kapupár egyik fele. Az elsőt lámpásfényben, a másodikat bárhová teheted, ahol épp látsz; a "
            "két kapu között az egységek átkelnek (jobb klikk a kapura, 2 mp, utána 8 mp töltődés). Kis lámpásfényt is ad.",
            {"wood": 120, "stone": 60}, 30, (2, 2), 600, "gate", torii=True, light_radius=3, light_heal=1)
        d.setdefault("researches", {})["oon_kettos_kapu"] = {
            "name": "Kettős kapu", "desc": "Két Torii-kapupár is építhető.", "cost": {"stone": 120, "meteor": 60},
            "time": 50, "effects": [], "rules": {"torii_pairs": 2}}
        add_res(d, "oon_lampaskert", "oon_kettos_kapu")
    edit("ooni", ooni)

    # Ö9 Ikrarakó – Huk’ucha
    abilities["ikrarakas"] = {
        "name": "Ikrarakás", "type": "lay_mine", "hotkey": "R", "cooldown": 3, "energy": 25, "unit": "huk_ikra",
        "max": 12, "icon": "summon",
        "desc": "Láthatatlan ikra-csomót rak le (egyszerre legfeljebb 12). Ha ellenség lép rá, 3 harapós ebihal kel ki "
                "belőle (saját tavacskán 5), és 10 mp-ig harcolnak."}

    def huk(d):
        U = d["units"]
        U["huk_ikrarako"] = unit("Ikrarakó", "Fürge kis béka-dajka: láthatatlan ikra-csomókat rak le, amelyekből "
                                             "rálépéskor harapós ebihalak kelnek ki.", {"wood": 60, "stone": 20}, 1, 18, 60,
                                 0, 3.2, 8, 0.32, "caster", None, ["ikrarakas"], tags=["organic"], energy=100)
        U["huk_ikra"] = unit("Ikra-csomó", "Láthatatlan ikra: ha ellenség lép rá, ebihalak kelnek ki belőle.", {}, 0, 0,
                             30, 0, 0.0, 3, 0.25, "cocoon", None, tags=["organic", "mine"], summoned=True,
                             always_hidden=True, no_corpse=True,
                             mine_trigger={"radius": 1.0, "spawn": "huk_ebihal", "count": 3, "pond_count": 5,
                                           "lifetime": 10})
        U["huk_ebihal"] = unit("Ebihal", "Harapós ebihal az ikrából; rövid ideig harcol.", {}, 0, 0, 35, 0, 3.4, 5, 0.22,
                               "bug", melee(6, 0.8), tags=["organic", "melee", "light"], summoned=True)
        nb = d["buildings"]["huk_nadbarakk"]
        nb["produces"] = [x for x in nb["produces"] if x != "huk_ikrarako"] + ["huk_ikrarako"]
        ai = d.setdefault("ai", {})
        ai.setdefault("army", {})["huk_ikrarako"] = 1
    edit("hukucha", huk)


def szoborpark():
    """Ötletdoboz Ö14: a Vasbirodalom Szoborparkja (fejlesztő épület) és az öreg istenek szobrai (bónuszt adó, kis
    épületek: a körzetükben, és csak amíg állnak). Ferrox – acél, Arcanor – mágia, Umbrax – túlvilág."""
    p = os.path.join(FDIR, "imperium.json")
    d = rd(p)
    B, R = d["buildings"], d.setdefault("researches", {})
    gods = [
        ("ferrox", "Ferrox-szobor", "Ferrox, az acél ura: kalapácsos, öntöttvas-szakállú alak. A körzetében (6 csempe) "
         "a gépek és az épületek +2 páncélt kapnak – amíg a szobor áll.",
         {"radius": 6, "tags": ["mech"], "modifiers": {"armor_add": 2}, "building_armor": 2},
         [("imp_ferrox_kalapacsa", "Ferrox kalapácsa", "+2 sebzés a közelharcos egységeknek.", {"stone": 150}, 50,
           [{"stat": "damage", "tags": ["melee"], "add": 2}], None),
          ("imp_ontottvas_burok", "Öntöttvas burok", "+60 életerő a nagy gépeknek.", {"stone": 175}, 55,
           [{"stat": "hp", "tags": ["large"], "add": 60}], None)]),
        ("arcanor", "Arcanor-szobor", "Arcanor, a mágia ura: csillagköpenyes alak, lebegő gömbbel. A körzetében "
         "(6 csempe) a távolsági egységek +1 lőtávot kapnak – amíg a szobor áll.",
         {"radius": 6, "tags": ["ranged"], "modifiers": {"range_add": 1}},
         [("imp_arcanor_csillagai", "Arcanor csillagai", "+25 energia a varázslóknak.", {"stone": 150}, 50,
           [{"stat": "energy", "tags": ["caster"], "add": 25}], None),
          ("imp_csillagterkep", "Csillagtérkép", "+2 látótáv minden egységnek.", {"stone": 125}, 45,
           [{"stat": "sight", "tags": ["all"], "add": 2}], None)]),
        ("umbrax", "Umbrax-szobor", "Umbrax, a túlvilág kapusa: fátylas alak lámpással és kulccsal. A körzetében "
         "(6 csempe) az egységek +1 életerőt töltenek vissza másodpercenként – amíg a szobor áll.",
         {"radius": 6, "modifiers": {"regen_add": 1.0}},
         [("imp_umbrax_fatyla", "Umbrax fátyla", "+15% sebesség a szellem-egységeknek.", {"stone": 150}, 50,
           [{"stat": "speed_pct", "tags": ["spirit"], "add": 15}], None),
          ("imp_lelekkapu", "Lélekkapu", "Az elesett ellenségek 8% eséllyel szellemlégiósként kelnek fel a te oldaladon "
           "(30 mp-re).", {"stone": 200}, 60, [],
           {"raise_from_kills": {"unit": "imp_szellemlegios", "chance": 0.08, "lifetime": 30, "color": [150, 200, 255]}})]),
    ]
    rids = []
    for gid, name, desc, aura, res in gods:
        sid = f"imp_szobor_{gid}"
        B[sid] = bld(name, desc, {"wood": 60, "stone": 80}, 20, (1, 1), 400, "statue", armor=3, aura=aura,
                     requires=["imp_szoborpark"])
        for rid, rn, rdsc, cost, t, eff, rules in res:
            R[rid] = {"name": rn, "desc": rdsc + f" (Kell hozzá: {name}.)", "cost": cost, "time": t, "effects": eff,
                      "requires": [sid]}
            if rules:
                R[rid]["rules"] = rules
            rids.append(rid)
    B["imp_szoborpark"] = bld("Szoborpark", "Az öreg istenek – Ferrox, Arcanor és Umbrax – szoborparkja. Itt kutathatók "
                              "az isteni fejlesztések, aranyért; mindegyik csak akkor, ha az adott isten szobra áll.",
                              {"wood": 150, "stone": 100}, 40, (3, 3), 900, "temple", researches=rids,
                              requires=["imp_legiogyar"])
    ai = d.setdefault("ai", {})
    bo = [x for x in ai.get("build_order", []) if not x.startswith("imp_szobor")]
    at = bo.index("imp_gepisten_templom") if "imp_gepisten_templom" in bo else len(bo)
    bo[at:at] = ["imp_szoborpark", "imp_szobor_ferrox", "imp_szobor_umbrax"]
    ai["build_order"] = bo
    ro = [x for x in ai.get("research_order", []) if x not in rids]
    ai["research_order"] = ro + ["imp_ferrox_kalapacsa", "imp_ontottvas_burok"]
    wr(p, d)


EGKO_PRE = {"elves": "elf", "ursan": "urs", "croftian": "cro", "wullof": "wul", "imperium": "imp", "hukucha": "huk",
            "ooni": "oon"}


def meteor_rift(abilities):
    """Ötletdoboz Ö21–Ö23: Égkő-fény (minden mágia a meteoritból ered), új pajzsok, Égkő."""
    gp = os.path.join(ROOT, "data", "game.json")
    g = rd(gp)
    g["rift"] = {"radius": 5, "energy_mult": 1.5, "mage_range": 1}
    g["upkeep"] = {"tiers": [[80, 1.0], [140, 0.7], [200, 0.4]]}  # Ö1 Hadtáp: létszám -> aranybevétel
    wr(gp, g)

    def edit(fid, fn):
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        fn(d)
        wr(p, d)

    def elves(d):  # erős rokonság: csillagkultusz
        d["rift"] = {"energy_mult": 2.0, "spell_mult": 1.25, "shield_regen_mult": 2.0}

    def imperium(d):  # erős rokonság: lélekreaktor, lélekenergia
        d["rift"] = {"regen": {"tags": ["mech"], "add": 1}}
        d["shield_zone"] = "reactor"
        for u in d["units"].values():
            if "mech" in u.get("tags", []):
                u["shield_slot"] = True
        d["buildings"]["imp_reaktor"]["shield_reactor"] = True
        d.setdefault("researches", {})["imp_reaktorpajzs"] = {
            "name": "Reaktorpajzs", "desc": "A gépek 30 pajzsot kapnak; a pajzs csak Lélekreaktor (6 csempe), "
            "meteorit vagy Égkő közelében töltődik.", "cost": {"stone": 125, "meteor": 50}, "time": 50,
            "effects": [{"stat": "shield", "tags": ["mech"], "add": 30}]}
        t = d["buildings"]["imp_gepisten_templom"]
        t["researches"] = [x for x in t.get("researches", []) if x != "imp_reaktorpajzs"] + ["imp_reaktorpajzs"]
        ai = d.setdefault("ai", {})
        ai["research_order"] = [x for x in ai.get("research_order", []) if x != "imp_reaktorpajzs"] + ["imp_reaktorpajzs"]

    def croftian(d):  # közepes rokonság: a mélység rokona
        d["rift"] = {"regen": {"units": ["cro_agyfalo", "cro_csapos_horror"], "add": 2}}

    def hukucha(d):  # Gyöngyház-pajzs: a Gyöngy irány egységei, csak saját tavacskán töltődik
        for uid in ("huk_gyongyveto", "huk_kagylopajzsos"):
            u = d["units"][uid]
            u["shield"] = 40
            u["shield_zone"] = "pond"

    def ursan(d):  # Rúnapajzs: csak 10 mp harc nélkül töltődik
        for uid, u in d["units"].items():
            if uid == "urs_runaorias" or u.get("hero"):
                u["shield"] = 60 if uid == "urs_runaorias" else 40
                u["shield_delay"] = 10

    for fid, fn in (("elves", elves), ("imperium", imperium), ("croftian", croftian), ("hukucha", hukucha),
                    ("ursan", ursan)):
        edit(fid, fn)
    # Ooni Védőfény: a Lámpáspap ideiglenes pajzsot ad
    abilities["oon_vedofeny"] = {
        "name": "Védőfény", "type": "ward", "hotkey": "V", "cooldown": 18, "energy": 50, "radius": 5, "amount": 50,
        "duration": 10, "color": [255, 230, 150], "icon": "aura",
        "desc": "A lámpás fénye védőburkot von a közeli társakra: 10 mp-ig 50 sebzést nyel el."}
    # Égkő: minden főfaj építője lerakhatja; bárkinek ad, bárki lerombolhatja
    for fid, pre in EGKO_PRE.items():
        def add(d, pre=pre):
            d["buildings"][f"{pre}_egko"] = bld(
                "Égkő", "Lerakott meteoritszilánk: Égkő-fényt áraszt (5 csempe), mint egy meteorit – a "
                "mágiahasználók erősebbek, a meteoritrokon népek gyógyulnak. MINDENKINEK hat, és bárki lerombolhatja.",
                {"meteor": 75}, 15, (1, 1), 300, "egko", rift_source=True, no_power=True, no_creep=True, no_pond=True,
                no_light=True, foundation_free=True)
            if pre in ("elf", "imp"):
                ai = d.setdefault("ai", {})
                bo = [x for x in ai.get("build_order", []) if x != f"{pre}_egko"]
                ai["build_order"] = bo + [f"{pre}_egko"]
        edit(fid, add)


def dow_mechanics(abilities):
    """Ötletdoboz Ö15 (Dawn of War-mechanikák): Harci kedv (Ooni), Falka-kiképzés (Wullof), Békaugrás (Huk’ucha),
    Felállítás (a nagy ostromgépek)."""
    def edit(fid, fn):
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        fn(d)
        wr(p, d)

    def ooni(d):
        d["morale"] = {"max": 100, "hurt": 60, "ally_death": 10, "hero_death": 25, "regen": 2,
                       "drum": {"unit": "oon_dobos", "radius": 4, "regen": 8}, "light_min": 30, "break_until": 50,
                       "broken": {"damage_mult": 0.7, "speed_mult": 1.2}}
    edit("ooni", ooni)

    def wullof(d):
        U, B = d["units"], d["buildings"]
        hz = B["wul_harcosok_tuze"]
        ids = []
        for base in ("wul_tomahawkos", "wul_ijas_vadasz"):
            u = U[base]
            fid = base + "_falka"
            f = {k: v for k, v in u.items() if k not in ("abilities", "hero")}
            f.update({"name": u["name"] + "-falka (3)",
                      "desc": f"Három {u['name']} egyszerre, olcsóbban. (Falkaharc kutatás kell hozzá.)",
                      "cost": {k: int(round(v * 2.6)) for k, v in u["cost"].items()},
                      "time": int(round(u.get("time", 20) * 2)), "supply": u.get("supply", 1) * 3,
                      "spawn_group": {"unit": base, "count": 3}, "requires_research": ["wul_falkaharc"],
                      "sprite": base})
            U[fid] = f
            ids.append(fid)
        hz["produces"] = [x for x in hz["produces"] if not x.endswith("_falka")] + ids
        hz["researches"] = [x for x in hz.get("researches", []) if x != "wul_falkaharc"] + ["wul_falkaharc"]
        d.setdefault("researches", {})["wul_falkaharc"] = {
            "name": "Falkaharc", "desc": "A Tomahawkosok és az Íjas vadászok falkában (hármasával, olcsóbban) is "
            "kiképezhetők.", "cost": {"wood": 120, "stone": 60}, "time": 45, "effects": []}
        ai = d.setdefault("ai", {})
        ai["research_order"] = ["wul_falkaharc"] + [x for x in ai.get("research_order", []) if x != "wul_falkaharc"]
    edit("wullof", wullof)

    abilities["bekaugras"] = {
        "name": "Békaugrás", "type": "blink", "hotkey": "J", "cooldown": 12, "range": 2.5,
        "range_research": {"huk_ugrolabak": 5}, "land_damage": 10, "land_radius": 1.2, "color": [130, 220, 150],
        "icon": "blink", "desc": "A béka nagyot ugrik a kijelölt pontra (vízen és akadályon át is): 2,5 csempe, az "
                                 "Ugrólábak kutatással 5. Landoláskor megsebzi a közeli ellenséget."}

    def huk(d):
        no_jump = {"huk_tavi_leny", "huk_gotelovas", "huk_szitakoto", "huk_teknos_mozsar", "huk_gyongyorias",
                   "huk_ikra", "huk_ebihal"}
        for uid, u in d["units"].items():
            abl = [a for a in u.get("abilities", []) if a != "bekaugras"]
            if uid not in no_jump:
                abl.append("bekaugras")
            if abl:
                u["abilities"] = abl
        r = d["researches"]["huk_ugrolabak"]
        r["desc"] = "+10% sebesség minden egységnek, és a Békaugrás 5 csempére nő (2,5 helyett)."
    edit("hukucha", huk)

    deploy = {"time": 2, "pack": 1, "range_add": 1}
    for fid, uid in (("elves", "elf_obszidian_veto"), ("imperium", "imp_onager"), ("croftian", "cro_ostromtaraj"),
                     ("hukucha", "huk_teknos_mozsar"), ("ursan", "urs_ostromszan")):
        def dep(d, uid=uid):
            u = d["units"][uid]
            u["deploy"] = dict(deploy)
            if "Felállítás" not in u.get("desc", ""):
                u["desc"] = u.get("desc", "") + " Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv), " \
                                               "mozgás előtt 1 mp-ig csukódik össze."
        edit(fid, dep)


def rift_guards():
    """Ötletdoboz Ö2: őrállás a Hasadékkövön – fajonként saját név és képesség. Duplázza a kő hozamát, lassan lő,
    és amíg áll, a kő nem foglalható el."""
    common = dict(on_point=True, attack={"damage": 8, "range": 6, "cooldown": 2.0, "targets": ["ground", "air"],
                                         "projectile": "arrow"},
                  foundation_free=True, no_power=True, no_creep=True, no_pond=True, no_light=True, sight=9)
    guards = {
        "elves": ("elf_naposzlop", "Naposzlop", "Napkorongos oszlop a Hasadékkövön; energiamezőt is ad, így a kő "
                  "mellett is építhetsz.", {"power_radius": 6}),
        "ursan": ("urs_jelzotuz", "Jelzőtűz", "Őrtűz a Hasadékkövön: messzire lát, és két harcos helyőrségként "
                  "védheti.", {"garrison": 2, "sight": 12}),
        "croftian": ("cro_dudor", "Lüktető dudor", "Hús-dudor a Hasadékkövön: mélységet terjeszt maga körül.",
                     {"creep_radius": 5}),
        "wullof": ("wul_totemcolop", "Totemcölöp", "Totemcölöp a Hasadékkövön: felszedhető, és egy másik kövön "
                   "újra leverhető.", {"packable": True}),
        "imperium": ("imp_jelzooszlop", "Jelzőoszlop", "Réz jelzőoszlop a Hasadékkövön: a gépek Reaktorpajzsa a "
                     "közelében is töltődik.", {"shield_reactor": True}),
        "hukucha": ("huk_gyongyboja", "Gyöngybója", "Gyöngyház bója a Hasadékkövön: elkészültekor tavacskát nyit a kő "
                    "körül.", {"pond_ring": 1}),
        "ooni": ("oon_kolampas", "Kőlámpás", "Kőlámpás a Hasadékkövön: lámpásfényt ad, így a kő mellett is "
                 "építhetsz.", {"light_radius": 5, "light_heal": 2}),
    }
    for fid, (bid, name, desc, extra) in guards.items():  # Ö2 elvetve (2026-11-05): az őrállások törlése
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        d["buildings"].pop(bid, None)
        for k in ("build_order",):
            if k in d.get("ai", {}):
                d["ai"][k] = [x for x in d["ai"][k] if x != bid]
        wr(p, d)
        continue
        d = rd(p)
        b = bld(name, desc + " Duplázza a kő meteorit-hozamát, lassan lő, és amíg áll, a követ nem lehet elfoglalni. "
                "Csak saját, elfoglalt Hasadékkőre építhető.", {"wood": 75, "stone": 25}, 25, (1, 1), 500, "tower",
                armor=2, **common)
        b.update(extra)
        d["buildings"][bid] = b
        wr(p, d)


def ahitat(abilities):
    """Ötletdoboz Ö17: Áhítat (Szürke elfek) és Tóének (Huk’ucha). A szentélyhez küldött munkás fohászkodik/énekel,
    és külön nyersanyagot termel (fejenként 0,3/mp, egyszerre legfeljebb 8). Képességekre, épületgombokra és
    kutatásokra költhető."""
    gp = os.path.join(ROOT, "data", "game.json")
    g = rd(gp)
    g["resources"]["faith"] = {"name": "Áhítat", "color": [255, 214, 120], "gather_time": 1, "carry": 0,
                               "node_amount": 0, "special": True}
    wr(gp, g)
    faith = {"rate": 0.3, "max": 8}

    # ---- Szürke elfek: Áhítat
    p = os.path.join(FDIR, "elves.json")
    d = rd(p)
    d["faith"] = dict(faith, name="Áhítat", verb="fohászkodik",
                      desc="A Napszentélyhez vagy Naptemplomhoz küldött Kőfaragók fohászkodnak (jobb klikk a szentélyre).")
    B, U, R = d["buildings"], d["units"], d["researches"]
    B["elf_napszentely"]["pray_slots"] = 4
    B["elf_naptemplom"]["pray_slots"] = 4
    B["elf_napszentely"]["researches"] = [x for x in B["elf_napszentely"].get("researches", [])
                                          if x != "elf_arany_fohasz"] + ["elf_arany_fohasz"]
    B["elf_naptemplom"]["researches"] = [x for x in B["elf_naptemplom"].get("researches", [])
                                         if x not in ("elf_napszemu_papok", "elf_egi_kegy")] + \
        ["elf_napszemu_papok", "elf_egi_kegy"]
    B["elf_napfuro"]["powers"] = ["napaldas"]
    for uid, ab in (("elf_napszoro", "napkitores"), ("elf_fopap", "nap_kegyelme")):
        U[uid]["abilities"] = [a for a in U[uid].get("abilities", []) if a != ab] + [ab]
    R["elf_arany_fohasz"] = {"name": "Arany fohász", "desc": "A fohászkodó Kőfaragók 0,4 Áhítatot adnak "
                             "másodpercenként (0,3 helyett).", "cost": {"faith": 120}, "time": 40, "effects": [],
                             "rules": {"faith_rate": 0.4}}
    R["elf_napszemu_papok"] = {"name": "Napszemű papok", "desc": "A Napszóró papok energiája 50%-kal gyorsabban "
                               "töltődik.", "cost": {"faith": 100, "stone": 100}, "time": 45, "effects": [],
                               "rules": {"energy_regen_ids": {"elf_napszoro": 1.5}}}
    eff = []
    for uid, u in sorted(U.items()):
        if u.get("hp") and not u.get("summoned") and u.get("cost"):
            eff.append({"stat": "hp", "tags": [uid], "add": int(round(u["hp"] * 0.1))})
    R["elf_egi_kegy"] = {"name": "Égi kegy", "desc": "A Nap megáldja a népet: minden egység +10% életerőt kap.",
                         "cost": {"faith": 200}, "time": 60, "effects": eff}
    ai = d.setdefault("ai", {})
    ro = [x for x in ai.get("research_order", []) if x not in ("elf_arany_fohasz", "elf_napszemu_papok", "elf_egi_kegy")]
    ai["research_order"] = ro + ["elf_arany_fohasz", "elf_egi_kegy", "elf_napszemu_papok"]
    wr(p, d)
    nv = abilities["napvihar"]
    abilities["napkitores"] = {
        "name": "Napkitörés", "type": "area", "hotkey": "K", "energy": 50, "faith": 40, "cooldown": 20,
        "range": nv.get("range", 8), "radius": round(nv.get("radius", 1.8) * 1.5, 2), "duration": nv.get("duration", 4),
        "dps": round(nv.get("dps", 22) * 1.5, 1), "color": [255, 150, 40], "sprite": nv.get("sprite", "napvihar"),
        "icon": "burst", "desc": "A Nap haragja: erősebb Napvihar (másfélszeres sebzés, másfélszeres terület). "
                                 "Ára 50 energia és 40 Áhítat."}
    abilities["nap_kegyelme"] = {
        "name": "Nap kegyelme", "type": "nova", "hotkey": "K", "faith": 30, "cooldown": 45, "radius": 6,
        "damage": 0, "heal": 80, "color": [255, 230, 140], "icon": "heal",
        "desc": "6 csempés körben azonnal 80 életerőt ad minden saját élőlénynek. Ára 30 Áhítat, 45 mp töltődés."}
    abilities["napaldas"] = {
        "name": "Napáldás", "type": "bpower", "effect": "mine_boost", "mult": 1.5, "duration": 30, "cooldown": 60,
        "faith": 25, "hotkey": "N", "icon": "rage",
        "desc": "A Napfúró 30 mp-re 50%-kal többet termel. Ára 25 Áhítat, 60 mp töltődés."}

    # ---- Huk’ucha: Tóének
    p = os.path.join(FDIR, "hukucha.json")
    d = rd(p)
    d["faith"] = dict(faith, name="Tóének", verb="énekel",
                      desc="A Tóteremtő szentélyhez küldött Tóásók a Tóanyának énekelnek (jobb klikk a szentélyre).")
    d.setdefault("resource_names", {})["faith"] = "Tóének"
    B, U, R = d["buildings"], d["units"], d["researches"]
    B["huk_toteremto"]["pray_slots"] = 4
    B["huk_toteremto"]["researches"] = [x for x in B["huk_toteremto"].get("researches", [])
                                        if x not in ("huk_toanya_korusa", "huk_iszapos_bor", "huk_mely_viz")] + \
        ["huk_toanya_korusa", "huk_iszapos_bor", "huk_mely_viz"]
    B["huk_toteremto"]["powers"] = ["aradas"]
    for bid in ("huk_gyongymuhely", "huk_gyongykert"):
        B[bid]["powers"] = ["gyongyeres"]
    t = U["huk_tavi_leny"]
    big = dict(t)
    big.update({"name": "Nagy tavi lény", "desc": "A Tóének erejével megidézett, nagyobb víztest; rövid ideig harcol.",
                "hp": int(round(t["hp"] * 1.5)), "radius": round(t.get("radius", 0.5) * 1.15, 2),
                "attack": dict(t["attack"], damage=int(round(t["attack"]["damage"] * 1.5))), "sprite": "huk_tavi_leny"})
    U["huk_nagy_tavi_leny"] = big
    go = U["huk_gyongyorias"]
    gi = {k: v for k, v in go.items() if k not in ("path", "requires", "requires_research", "abilities")}
    gi.update({"name": "Megidézett gyöngyóriás", "desc": "A Mélység hangjára előjövő gyöngyóriás; 45 mp-ig harcol.",
               "cost": {}, "supply": 0, "time": 1, "summoned": True, "sprite": "huk_gyongyorias",
               "tags": sorted(set(go.get("tags", [])) | {"summoned"})})
    U["huk_gyongyorias_idezett"] = gi
    for uid, ab in (("huk_toidezo", "huk_nagy_tavi_leny_hivas"), ("huk_melysegi_hivo", "huk_melyseg_hangja")):
        U[uid]["abilities"] = [a for a in U[uid].get("abilities", []) if a != ab] + [ab]
    R["huk_toanya_korusa"] = {"name": "Tóanya kórusa", "desc": "Az éneklő Tóásók 0,4 Tóéneket adnak másodpercenként "
                              "(0,3 helyett).", "cost": {"faith": 120}, "time": 40, "effects": [],
                              "rules": {"faith_rate": 0.4}}
    R["huk_iszapos_bor"] = {"name": "Iszapos bőr", "desc": "Minden béka +1 páncélt kap.",
                            "cost": {"faith": 150, "stone": 80}, "time": 45,
                            "effects": [{"stat": "armor", "tags": ["organic"], "add": 1}]}
    R["huk_mely_viz"] = {"name": "Mély víz", "desc": "A megidézett lények 30%-kal tovább maradnak.",
                         "cost": {"faith": 100}, "time": 40, "effects": [], "rules": {"summon_life_mult": 1.3}}
    ai = d.setdefault("ai", {})
    ro = [x for x in ai.get("research_order", []) if x not in ("huk_toanya_korusa", "huk_iszapos_bor", "huk_mely_viz")]
    ai["research_order"] = ro + ["huk_toanya_korusa", "huk_iszapos_bor", "huk_mely_viz"]
    wr(p, d)
    abilities["huk_nagy_tavi_leny_hivas"] = {
        "name": "Nagy tavi lény", "type": "summon", "hotkey": "N", "faith": 35, "cooldown": 30,
        "unit": "huk_nagy_tavi_leny", "count": 1, "lifetime": 60, "icon": "summon",
        "desc": "Erősebb tavi lényt idéz 60 mp-re (+50% életerő és sebzés). Ára 35 Tóének."}
    abilities["huk_melyseg_hangja"] = {
        "name": "Mélység hangja", "type": "summon", "hotkey": "N", "faith": 60, "energy": 50, "cooldown": 60,
        "unit": "huk_gyongyorias_idezett", "count": 1, "lifetime": 45, "icon": "summon",
        "desc": "Egy gyöngyóriást hív elő a mélyből 45 mp-re. Ára 60 Tóének és 50 energia."}
    abilities["aradas"] = {
        "name": "Áradás", "type": "bpower", "effect": "flood", "duration": 30, "cooldown": 90, "faith": 30,
        "heal": 3, "hotkey": "A", "icon": "heal",
        "desc": "30 mp-re minden saját tavacska egy csempével kitágul, és a rajtuk álló saját egységek "
                "másodpercenként 3 életerőt kapnak. Ára 30 Tóének, 90 mp töltődés."}
    abilities["gyongyeres"] = {
        "name": "Gyöngyérés", "type": "bpower", "effect": "hasten", "mult": 2.0, "duration": 30, "cooldown": 60,
        "faith": 25, "hotkey": "G", "icon": "rage",
        "desc": "Az épület 30 mp-re kétszer gyorsabban kutat és képez. Ára 25 Tóének, 60 mp töltődés."}


def wullof_rework(abilities):
    """Wullof egyszerűsítés (a főfaj-áttekintés után):
    - a Dárdavető portyázó kikerül (szinte azonos a Lándzsással); a dárdavetése a Lándzsásé lesz;
    - a falka nem külön egység, hanem kapcsoló (Falkaharc után): a Lándzsás és a Bumerángvető hármasával jön;
    - a Bölénylovas nem képezhető: a Bölénylovaglás kutatás után a gyalogos felülhet a bölényre;
    - az Álomjáró saját képességeket kap (a Holdsámán másolata volt)."""
    p = os.path.join(FDIR, "wullof.json")
    d = rd(p)
    U, B, R = d["units"], d["buildings"], d.setdefault("researches", {})
    # Dárdavető portyázó ki (a régi mentések miatt a leírása marad, rejtve)
    U["wul_tomahawkos"]["hidden"] = True
    U.pop("wul_tomahawkos_falka", None)
    for b in B.values():
        if b.get("produces"):
            b["produces"] = [x for x in b["produces"] if x not in ("wul_tomahawkos", "wul_tomahawkos_falka")]
    la = U["wul_landzsas"]
    la["abilities"] = [a for a in la.get("abilities", []) if a != "tomahawk"] + ["tomahawk"]
    la["desc"] = ("Gyors, kovakőhegyű lándzsás, keskeny festett hárítópajzzsal; dárdavetővel (woomera) messzire is "
                  "hajít. Falkában erős.")
    # falka: a Lándzsás és a Bumerángvető (a falka-változat rejtett, a kapcsoló választja)
    base = la
    f = {k: v for k, v in base.items() if k not in ("abilities", "hero")}
    f.update({"name": base["name"] + "-falka (3)", "desc": f"Három {base['name']} egyszerre, olcsóbban.",
              "cost": {k: int(round(v * 2.6)) for k, v in base["cost"].items()},
              "time": int(round(base.get("time", 18) * 2)), "supply": base.get("supply", 2) * 3,
              "spawn_group": {"unit": "wul_landzsas", "count": 3}, "requires_research": ["wul_falkaharc"],
              "sprite": "wul_landzsas", "hidden": True})
    U["wul_landzsas_falka"] = f
    if "wul_ijas_vadasz_falka" in U:
        U["wul_ijas_vadasz_falka"]["hidden"] = True
    for bid, b in B.items():
        pr = b.get("produces") or []
        if "wul_landzsas" in pr and "wul_landzsas_falka" not in pr:
            pr.append("wul_landzsas_falka")
    R["wul_falkaharc"]["desc"] = ("A Lándzsás harcosok és a Bumerángvetők falkában (hármasával, olcsóbban) is "
                                  "képezhetők: a képző épületen a „Falkában képez” kapcsolóval.")
    # Bölénylovaglás
    bo = U["wul_boleny"]
    bo["ability_research"] = {"felultetes": "wul_bolenylovaglas"}
    ak = B["wul_bolenyakol"]
    ak["researches"] = [x for x in (ak.get("researches") or []) if x != "wul_bolenylovaglas"] + ["wul_bolenylovaglas"]
    R["wul_bolenylovaglas"] = {"name": "Bölénylovaglás", "desc": "A wullof gyalogos harcosok felülhetnek a bölényre "
                               "(a Bölény Felültetés gombja): Bölénylovas lesz belőlük.",
                               "cost": {"wood": 100, "stone": 80}, "time": 40, "effects": []}
    # Álomjáró: saját képességek
    aj = U["wul_alomjaro"]
    aj["abilities"] = ["alomkod", "alomseta"]
    aj["desc"] = "Az ősök álmait járja: álomködöt borít az ellenségre, és az álmok útján ugrik egyik helyről a másikra."
    # gépi ellenfél
    ai = d.setdefault("ai", {})
    army = dict(ai.get("army", {}))
    tw = army.pop("wul_tomahawkos", 0)
    army["wul_landzsas"] = army.get("wul_landzsas", 0) + tw
    ai["army"] = army
    ro = ai.setdefault("research_order", [])
    if "wul_bolenylovaglas" not in ro:
        ro.append("wul_bolenylovaglas")
    wr(p, d)
    abilities["felultetes"]["riders"] = [r for r in abilities["felultetes"].get("riders", []) if r != "wul_tomahawkos"]
    abilities["alomkod"] = {
        "name": "Álomköd", "type": "area", "hotkey": "W", "energy": 60, "cooldown": 12, "range": 7, "radius": 2.5,
        "duration": 5, "dps": 3, "slow": 0.35, "color": [170, 150, 230], "sprite": "oruletkod", "icon": "burst",
        "desc": "Álomködöt borít egy területre 5 mp-re: az ellenség benne nagyon lelassul (–65%) és kicsit sebződik."}
    abilities["alomseta"] = {
        "name": "Álomséta", "type": "blink", "hotkey": "E", "cooldown": 18, "range": 7, "color": [190, 170, 255],
        "icon": "blink", "desc": "Az álmok útján a kijelölt pontra lép (legfeljebb 7 csempe)."}


def research_tiers_and_extras(abilities):
    """(1) Szintes alapfejlesztések: minden főfaj sebzés- és páncélkutatása kap II. és III. szintet (egyre drágább;
    a III. csak a Felemelkedés után). (2) A katalógusból kiválasztott fajjellemző fejlesztések."""
    MAIN = ("elves", "ursan", "croftian", "wullof", "imperium", "hukucha", "ooni")
    for fid in MAIN:
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        R, B = d.setdefault("researches", {}), d["buildings"]
        ro = d.setdefault("ai", {}).setdefault("research_order", [])
        base_ids = [rid for rid, r in list(R.items())
                    if not rid.endswith(("_2", "_3")) and len(r.get("effects", [])) == 1
                    and r["effects"][0]["stat"] in ("damage", "armor") and not r.get("rules")
                    and r.get("cost") and "faith" not in r.get("cost", {})
                    and any(rid in (b.get("researches") or []) for b in B.values())]
        for rid in base_ids:
            r = R[rid]
            prev = rid
            for lvl, mult, roman in ((2, 1.6, "II."), (3, 2.4, "III.")):
                nid = f"{rid}_{lvl}"
                cost = {k: int(round(v * mult / 5) * 5) for k, v in r["cost"].items()}
                if lvl == 3:
                    cost["meteor"] = cost.get("meteor", 0) + 50
                nr = {"name": f"{r['name']} {roman}", "desc": r["desc"] + f" ({roman} szint" +
                      ("; a Felemelkedés után)" if lvl == 3 else ")"),
                      "cost": cost, "time": int(r.get("time", 40) * (1 + 0.25 * (lvl - 1))),
                      "effects": [dict(r["effects"][0])], "requires_research": [prev]}
                if lvl == 3:
                    nr["requires_ascended"] = True
                R[nid] = nr
                for b in B.values():
                    rs = b.get("researches") or []
                    if rid in rs and nid not in rs:
                        rs.insert(rs.index(prev) + 1, nid)
                if nid not in ro:
                    ro.append(nid)
                prev = nid
        wr(p, d)

    def add(fid, bid, rid, name, desc, cost, time, effects=(), rules=None, unlock=None):
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        b = d["buildings"][bid]
        b["researches"] = [x for x in (b.get("researches") or []) if x != rid] + [rid]
        r = {"name": name, "desc": desc, "cost": cost, "time": time, "effects": list(effects)}
        if rules:
            r["rules"] = rules
        d.setdefault("researches", {})[rid] = r
        for uid, ab in (unlock or {}).items():  # kutatáshoz kötött képesség
            u = d["units"][uid]
            u["abilities"] = [a for a in u.get("abilities", []) if a != ab] + [ab]
            u.setdefault("ability_research", {})[ab] = rid
        ro = d.setdefault("ai", {}).setdefault("research_order", [])
        if rid not in ro:
            ro.append(rid)
        wr(p, d)
    # elfek
    add("elves", "elf_napszentely", "elf_rezonancia", "Rezonancia", "A Napobeliszkek energiamezeje 2 csempével nagyobb.",
        {"stone": 100, "meteor": 50}, 40, rules={"power_add": 2})
    abilities["napfolt"] = {"name": "Napfolt", "type": "self_buff", "hotkey": "F", "cooldown": 40, "duration": 6,
                            "no_weak": True, "icon": "shield",
                            "desc": "Napfénybe öltözik: 6 mp-ig semmilyen egységtípus ellen nem gyenge."}
    add("elves", "elf_harcos_csarnok", "elf_napfolt", "Napfolt", "A Tollas harcosok megkapják a Napfolt képességet: 6 mp-ig "
        "semmilyen egységtípus ellen nem gyengék.", {"wood": 80, "stone": 100}, 40, unlock={"elf_tollas_harcos": "napfolt"})
    # ursanok
    add("ursan", "urs_runakovacs", "urs_tuskes_pajzsfal", "Tüskés pajzsfal",
        "A Pajzshordozók nem gyengék a közelharcosok és az ostromgépek ellen.", {"wood": 100, "stone": 100}, 40,
        rules={"no_weak": {"urs_pajzshordozo": ["kozelharcos", "ostrom"]}})
    # wullof
    add("wullof", "wul_samansator", "wul_nyomolvasas", "Ősi nyomolvasás",
        "A wullof egységek 8 csempén belül felfedik a láthatatlan ellenséget.", {"wood": 80, "stone": 80}, 35,
        rules={"detect_all": 8})
    add("wullof", "wul_samansator", "wul_holdtolte", "Holdtölte",
        "Minden 4. percben 1 percig telihold van: a wullof egységek 15%-kal nagyobbat sebeznek.",
        {"wood": 100, "stone": 80, "meteor": 40}, 45, rules={"moon": {"period": 240, "dur": 60, "mult": 1.15}})
    # vasbirodalom
    abilities["goztulnyomas"] = {"name": "Gőztúlnyomás", "type": "self_buff", "hotkey": "G", "cooldown": 30,
                                 "duration": 6, "modifiers": {"speed_mult": 1.4}, "after_slow": 3, "icon": "rage",
                                 "desc": "6 mp-re +40% sebesség; utána 3 mp-ig lassabb, amíg a kazán lehűl."}
    p = os.path.join(FDIR, "imperium.json")
    d = rd(p)
    mechs = {uid: "goztulnyomas" for uid, u in d["units"].items()
             if "mech" in u.get("tags", []) and not u.get("worker") and not u.get("hero") and not u.get("summoned")
             and u.get("speed", 0) > 0}
    add("imperium", "imp_ontode", "imp_goztulnyomas", "Gőztúlnyomás",
        "A gépek megkapják a Gőztúlnyomás képességet: 6 mp-re +40% sebesség, utána 3 mp-ig lassabbak.",
        {"wood": 80, "stone": 120}, 40, unlock=mechs)
    add("imperium", "imp_ontode", "imp_szikrafogo", "Szikrafogó vértek", "A gépek nem gyengék a varázslók ellen.",
        {"stone": 140, "meteor": 40}, 40, rules={"no_weak": {"mech": ["varazslo"]}})
    # huk'ucha
    abilities["hejba"] = {"name": "Héjba húzódás", "type": "self_buff", "hotkey": "H", "cooldown": 35, "duration": 8,
                          "no_weak": True, "modifiers": {"speed_mult": 0.6}, "icon": "shield",
                          "desc": "Behúzódik a kagylóhéjba: 8 mp-ig semmilyen egységtípus ellen nem gyenge, de lassabb."}
    add("hukucha", "huk_gyongykert", "huk_hejba", "Héjba húzódás", "A Kagylópajzsosok megkapják a Héjba húzódás "
        "képességet: 8 mp-ig semmi ellen nem gyengék.", {"wood": 80, "stone": 100}, 35,
        unlock={"huk_kagylopajzsos": "hejba"})
    add("hukucha", "huk_gyongymuhely", "huk_tavak_utja", "Tavak útja",
        "A saját tavacskán álló békák 20%-kal nagyobbat sebeznek.", {"wood": 100, "stone": 100}, 40,
        rules={"pond_dmg": 1.2})
    # ooni
    add("ooni", "oon_kapuszentely", "oon_kokutya_huseg", "Kőkutya-hűség",
        "A Kőkutya-őr mellett álló (2,5 csempén belüli) épület +2 páncélt kap.", {"wood": 80, "stone": 100}, 35,
        rules={"kokutya_guard": 2})
    add("ooni", "oon_lampaskert", "oon_lampas_magia", "Lámpás-mágia",
        "A lámpásfényben álló varázslók 20%-kal erősebbek.", {"wood": 80, "stone": 80, "meteor": 60}, 40,
        rules={"lantern_magic": 1.2})


def research_pack():
    """Kutatás-bővítés (a főfaj-áttekintés után): minden faj tud sebzést és páncélt fejleszteni, plusz fajonként
    jellegzetes fejlesztések (Ursan, Croftian, Ooni). A Croftian Emésztőkamra (a Csápszolga már nem hord, így
    leadóhelyként haszontalan lett) a fejlesztések háza."""
    def add(fid, bid, rid, name, desc, cost, time, effects=(), rules=None):
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        b = d["buildings"][bid]
        b["researches"] = [x for x in (b.get("researches") or []) if x != rid] + [rid]
        r = {"name": name, "desc": desc, "cost": cost, "time": time, "effects": list(effects)}
        if rules:
            r["rules"] = rules
        d.setdefault("researches", {})[rid] = r
        ro = d.setdefault("ai", {}).setdefault("research_order", [])
        if rid not in ro:
            ro.append(rid)
        wr(p, d)
    # --- hiányzó alap sebzés / páncél
    add("elves", "elf_obszidian_muhely", "elf_obszidian_vert", "Obszidián vértezet",
        "+1 páncél minden élő elf egységnek.", {"wood": 65, "stone": 150}, 40,
        [{"stat": "armor", "tags": ["organic"], "add": 1}])
    add("croftian", "cro_emeszto", "cro_eles_karmok", "Éles karmok", "+2 sebzés a közelharci egységeknek.",
        {"wood": 120, "stone": 80}, 35, [{"stat": "damage", "tags": ["melee"], "add": 2}])
    add("hukucha", "huk_gyongymuhely", "huk_kampohegy", "Kampóhegy", "+2 sebzés a közelharci egységeknek.",
        {"wood": 100, "stone": 100}, 35, [{"stat": "damage", "tags": ["melee"], "add": 2}])
    add("ooni", "oon_lampaskert", "oon_kotel", "Fonott hajítókötél", "+2 sebzés a távolsági egységeknek.",
        {"wood": 100, "stone": 100}, 35, [{"stat": "damage", "tags": ["ranged"], "add": 2}])
    # --- Ursanok
    add("ursan", "urs_runakovacs", "urs_runapajzs", "Rúnapajzs",
        "Rúnák az épületeken: minden épület +2 páncélt kap, és lassan magától javul (+1 életerő/mp).",
        {"wood": 100, "stone": 120}, 40,
        [{"stat": "armor", "tags": ["building"], "add": 2}, {"stat": "regen", "tags": ["building"], "add": 1}])
    add("ursan", "urs_nagyterem", "urs_kiados_tanya", "Kiadós tanya", "A Tanyák +4 létszámot adnak (8 helyett 12).",
        {"wood": 120, "stone": 60}, 40, rules={"supply_add": {"urs_tanya": 4}})
    add("ursan", "urs_eskufa", "urs_medve_osztön", "Medve-ösztön",
        "Vérszag: a 40% életerő alá sebesült ursan egységek 20%-kal nagyobbat sebeznek.",
        {"wood": 80, "stone": 120, "meteor": 40}, 45, rules={"low_hp_rage": {"below": 0.4, "mult": 1.2}})
    add("ursan", "urs_nagyterem", "urs_gyors_helyorseg", "Lőréses falak",
        "A helyőrségbe húzódott egységek erősebben lőnek ki (+7 sebzés fejenként a +4 helyett).",
        {"wood": 100, "stone": 80}, 35, rules={"garrison_dmg": 7})
    # --- Croftianok (Emésztőkamra)
    add("croftian", "cro_emeszto", "cro_gyors_larva", "Gyors lárvázás",
        "A lárvák 25%-kal gyorsabban kelnek, és a Kaptár +1 lárvát tarthat.", {"wood": 120, "stone": 100}, 40,
        rules={"larva_mult": 1.25, "larva_add": 1})
    add("croftian", "cro_emeszto", "cro_melyebb_rothasztas", "Mélyebb rothasztás",
        "A mélységen álló fák 50%-kal gyorsabban rothadnak, és a rothasztás felső határa 6,5-ről 9 fa/mp-re nő.",
        {"wood": 80, "stone": 120}, 40, rules={"rot_mult": 1.5, "rot_cap": 9.0})
    add("croftian", "cro_emeszto", "cro_benito_mereg", "Bénító méreg",
        "A Mérgező tüskék kétszer erősebben mérgeznek és lassítanak, a távolsági egységek lövései pedig mérgeznek "
        "(4 sebzés/mp, 3 mp).", {"wood": 100, "stone": 100, "meteor": 40}, 45,
        rules={"spike_mult": 2.0, "poison_attacks": {"tags": ["ranged"], "dps": 4, "dur": 3}})
    add("croftian", "cro_emeszto", "cro_melysegi_gyogyulas", "Mélységi gyógyulás",
        "A saját mélységen álló egységek kétszer gyorsabban gyógyulnak.", {"wood": 80, "stone": 80, "meteor": 60}, 40,
        rules={"creep_regen_mult": 2.0})
    # --- Ooni
    add("ooni", "oon_papirlampas", "oon_messzefeny", "Messzefény",
        "+2 csempe lámpásfény minden lámpásnál: több helyen lehet építeni, és több helyen gyógyulnak az egységek.",
        {"wood": 120, "stone": 80}, 40, rules={"light_add": 2})
    add("ooni", "oon_edzoudvar", "oon_harci_dobok", "Harci dobok",
        "A harci kedv 40%-kal lassabban fogy, a Taiko-dobos kétszer gyorsabban tölti.", {"wood": 100, "stone": 100}, 40,
        rules={"morale_hurt_mult": 0.6, "drum_mult": 2.0})
    add("ooni", "oon_edzoudvar", "oon_verszomj", "Oni vérszomj",
        "Az oni harcosok (Oni harcos, Tomboló oni, Ō-oni) ölésenként 20 életerőt kapnak vissza.",
        {"wood": 80, "stone": 120, "meteor": 40}, 45,
        rules={"kill_heal": {"ids": ["oon_oni_harcos", "oon_berserker_oni", "oon_o_oni", "oon_kohajito"], "hp": 20}})
    add("ooni", "oon_tengu_feszek", "oon_tengu_tollak", "Tengu-tollak",
        "A repülő egységek +1 lőtávot és +10% sebességet kapnak.", {"wood": 120, "stone": 80, "meteor": 40}, 40,
        [{"stat": "range", "tags": ["flyer"], "add": 1}, {"stat": "speed_pct", "tags": ["flyer"], "add": 10}])
    # az Emésztőkamra új szerepe
    p = os.path.join(FDIR, "croftian.json")
    d = rd(p)
    d["buildings"]["cro_emeszto"]["desc"] = ("Emésztőkamra: a mélység lassú bendője. Itt fejleszthető a lárvázás, a "
                                             "rothasztás, a méreg és a gyógyulás; nyersanyag leadására is jó.")
    wr(p, d)


def elf_core_raptor():
    """Elf alapsereg (a főfaj-áttekintés után): a Raptorlovas a Felemelkedés előtt is képezhető a Harcosok csarnokában.
    A Jaguár-irány helyette a Páncélos raptort kapja (több életerő és páncél, erdőben is lopakodik)."""
    p = os.path.join(FDIR, "elves.json")
    d = rd(p)
    U, B = d["units"], d["buildings"]
    r = U["elf_raptor_lovas"]
    r.pop("path", None)
    r["requires"] = ["elf_obszidian_muhely"]
    hc = B["elf_harcos_csarnok"]
    hc["produces"] = [x for x in hc["produces"] if x != "elf_raptor_lovas"] + ["elf_raptor_lovas"]
    pr = dict(r)
    pr.pop("requires", None)
    pr.update({"name": "Páncélos raptor", "desc": "Obszidiánlemezes harci raptor jaguár-harcossal: lassabb, de jóval "
                                                  "tartósabb a Raptorlovasnál. Bónusz a távolsági egységek ellen.",
               "cost": {"wood": 110, "stone": 55}, "time": 26, "hp": 210, "shield": 70, "armor": 3, "speed": 3.4,
               "attack": dict(r["attack"], damage=17), "path": "jaguar", "sprite": "elf_raptor_lovas"})
    U["elf_pancelos_raptor"] = pr
    rk = B["elf_raptor_karam"]
    rk["produces"] = ["elf_pancelos_raptor" if x == "elf_raptor_lovas" else x for x in rk["produces"]]
    if "elf_pancelos_raptor" not in rk["produces"]:
        rk["produces"].insert(0, "elf_pancelos_raptor")
    jp = d["paths"]["jaguar"]
    jp["units"] = ["elf_pancelos_raptor" if x == "elf_raptor_lovas" else x for x in jp["units"]]
    jp["desc"] = ("Az erdőben a gyalogosaid láthatatlanok, a közelharcosok +2 sebzést kapnak. Jaguár-orvlövészek, "
                  "páncélos raptorok és tollas rexek.")
    ai = d.setdefault("ai", {})
    army = dict(ai.get("army", {}))
    army["elf_raptor_lovas"] = 2
    ai["army"] = army
    jag = ai.setdefault("paths", {}).setdefault("jaguar", {})
    ja = {("elf_pancelos_raptor" if k == "elf_raptor_lovas" else k): v for k, v in jag.get("army", {}).items()}
    jag["army"] = ja
    wr(p, d)


def croftian_spikes():
    """Croftian korai bázisvédelem: a Csápszolga mérgező tüskéket rak le (1×1, a Tüskemező kutatással 2×2).
    Át lehet rajtuk menni, de minden nem croftian egységet lassítanak és mérgeznek (a szövetségeseket is)."""
    p = os.path.join(FDIR, "croftian.json")
    d = rd(p)
    B, R = d["buildings"], d.setdefault("researches", {})
    B["cro_merges_tuskek"] = {
        "name": "Mérgező tüskék", "cost": {"wood": 20, "stone": 10}, "time": 2, "size": [1, 1], "hp": 1, "armor": 0,
        "sight": 0, "shape": "spikes", "spike_tile": True, "no_sprite": True, "foundation_free": True,
        "desc": "A Csápszolga a földbe szúrja (2 mp, nem olvad bele). Át lehet rajta menni, de minden nem croftian "
                "egységet lassít (–40%) és mérgez (4 sebzés/mp, a szövetségeseket is). Legfeljebb 16 mező. "
                "A Tüskemező kutatással 2×2-es."}
    tv = B["cro_tuskeverem"]
    tv["researches"] = [x for x in (tv.get("researches") or []) if x != "cro_tuskemezo"] + ["cro_tuskemezo"]
    R["cro_tuskemezo"] = {"name": "Tüskemező", "desc": "A Mérgező tüskék 2×2-es mezőre kerülnek (ugyanazért az árért), "
                                                      "és legfeljebb 24 mező rakható.",
                          "cost": {"wood": 100, "stone": 75}, "time": 35, "effects": [],
                          "rules": {"spike_size": 2, "spike_max": 24}}
    ai = d.setdefault("ai", {})
    ai["spikes"] = True
    wr(p, d)


def egi_kegy(abilities):
    """Ötletdoboz Ö19: Égi kegy – fajonként egy isteni csapás a főépület gombjáról, a térkép bármely látható pontjára
    (vagy célpont nélkül), hosszú töltődéssel; ára a faj saját mérőjéből."""
    P = {
        "elves": ("gp_napcsova", {
            "name": "Napcsóva", "target": True, "faith": 150, "cooldown": 180, "effect": "area", "radius": 2.5,
            "duration": 8, "dps": 25, "reveal": 6, "color": [255, 220, 120], "icon": "burst",
            "desc": "Izzó fényoszlop a kijelölt pontra: 8 mp alatt 200 sebzés a körzetben, és 10 mp-re felfedi a "
                    "környéket. Ára 150 Áhítat, 3 perc töltődés."}),
        "hukucha": ("gp_arhullam", {
            "name": "Árhullám", "target": True, "faith": 120, "cooldown": 150, "effect": "area", "radius": 3,
            "duration": 4, "dps": 15, "slow": 0.5, "color": [110, 200, 255], "icon": "burst",
            "desc": "A Tóanya hulláma elönti a kijelölt pontot: 4 mp alatt 60 sebzés, és felére lassítja az "
                    "ellenséget. Ára 120 Tóének, 2,5 perc töltődés."}),
        "ursan": ("gp_eskufa_riado", {
            "name": "Esküfa-riadó", "target": False, "cooldown": 240, "effect": "levy", "buff": "fegyverbe",
            "duration": 30, "icon": "rage",
            "desc": "Megkondul az Esküfa: a falu minden telepese 30 mp-re fegyvert ragad (a Fegyverbe! hatása, "
                    "kutatás nélkül is), utána visszamegy dolgozni. 4 perc töltődés."}),
        "croftian": ("gp_melysegi_kitores", {
            "name": "Mélységi kitörés", "target": True, "hq_hp": 250, "cooldown": 180, "effect": "area",
            "radius": 2.5, "duration": 3, "dps": 40, "slow": 0.6, "color": [150, 255, 120], "icon": "burst",
            "desc": "Tüskék törnek elő a mélyből a kijelölt ponton: 3 mp alatt 120 sebzés és lassítás. Ára a "
                    "Kaptár 250 életereje, 3 perc töltődés."}),
        "wullof": ("gp_osok_hivasa", {
            "name": "Ősök hívása", "target": True, "cooldown": 180, "effect": "summon", "unit": "wul_szellemfarkas",
            "count": 3, "lifetime": 40, "icon": "summon",
            "desc": "Három szellemfarkas jelenik meg a kijelölt ponton 40 mp-re. 3 perc töltődés."}),
        "imperium": ("gp_ordan_villama", {
            "name": "Ordan villáma", "target": True, "meteor": 100, "cooldown": 150, "effect": "area", "radius": 2,
            "duration": 1, "dps": 160, "buildings": True, "color": [120, 255, 190], "icon": "bolt",
            "desc": "Ordan villáma csap le a kijelölt pontra: azonnal 160 sebzés egységeknek és épületeknek is. "
                    "Ára 100 meteorit, 2,5 perc töltődés."}),
        "ooni": ("gp_isteni_szel", {
            "name": "Isteni szél", "target": True, "cooldown": 150, "effect": "wind", "radius": 4, "duration": 8,
            "slow": 0.5, "buff": "gp_isteni_szel_buff", "color": [200, 240, 255], "icon": "burst",
            "desc": "Isteni szél söpör végig a kijelölt ponton: 8 mp-re felére lassítja az ellenséget, a saját "
                    "egységek pedig 20%-kal gyorsabbak. 2,5 perc töltődés."}),
    }
    abilities["gp_isteni_szel_buff"] = {"name": "Isteni szél", "type": "passive", "modifiers": {"speed_mult": 1.2},
                                        "icon": "speed", "desc": "Az Isteni szél hátszele: +20% sebesség."}
    for fid, (aid, ab) in P.items():
        ab = dict(ab, type="gpower")
        abilities[aid] = ab
        p = os.path.join(FDIR, fid + ".json")
        d = rd(p)
        hq = d["buildings"][d["hq"]]
        hq["powers"] = [x for x in hq.get("powers", []) if x != aid] + [aid]
        wr(p, d)


def hero_ab(name, typ, desc, levels, icon, **kw):
    d = dict(name=name, type=typ, desc=desc, levels=levels, icon=icon)
    d.update(kw)
    return d


def unit(name, desc, cost, supply, time, hp, armor, speed, sight, radius, shape, attack=None, abilities=(), tags=(), **kw):
    u = {"name": name, "desc": desc, "cost": cost, "supply": supply, "time": time, "hp": hp, "armor": armor, "speed": speed,
         "sight": sight, "radius": radius, "shape": shape, "tags": list(tags)}
    if attack:
        u["attack"] = attack
    if abilities:
        u["abilities"] = list(abilities)
    u.update(kw)
    return u


def bld(name, desc, cost, time, size, hp, shape, **kw):
    b = {"name": name, "desc": desc, "cost": cost, "time": time, "size": list(size), "hp": hp, "armor": 1, "sight": 7,
         "shape": shape}
    b.update(kw)
    return b


def melee(d, cd=1.2, r=0.3):
    return {"damage": d, "range": r, "cooldown": cd, "targets": ["ground"]}


def ranged(d, r, cd=1.5, proj="arrow", air=True, **kw):
    a = {"damage": d, "range": r, "cooldown": cd, "targets": ["ground", "air"] if air else ["ground"], "projectile": proj}
    a.update(kw)
    return a


# ---------------------------------------------------------------- Huk'ucha (békanép)
def hukucha(abilities):
    U, B, R = {}, {}, {}
    U["huk_toaso"] = unit("Tóásó", "Munkás: tavacskát ás (2×2 – erre építhet a nép), gyűjt, épít.", {"wood": 50}, 1, 12, 45,
                          0, 2.8, 7, 0.36, "worker", melee(4, 1.4), worker=True, tags=["worker", "organic"])
    U["huk_csuzlis"] = unit("Csúzlis", "Gyöngygolyós csúzlival lő; pajzsa nincs.", {"wood": 60, "stone": 20}, 1, 16, 70, 0,
                            2.8, 8, 0.36, "archer", ranged(10, 5.5, 1.3, "boulder"), tags=["infantry", "ranged", "organic"])
    U["huk_kampokardos"] = unit("Kampókardos", "Tompa végű, kampós lapos karddal harcol – a kampóval magához rántja "
                                "az ellenfelet.", {"wood": 65, "stone": 20}, 2, 18, 150, 1, 2.6, 7, 0.4, "warrior",
                                melee(13, 1.1), ["huk_horog"], tags=["infantry", "melee", "organic"])
    U["huk_toidezo"] = unit("Tóidéző", "Mágus: lábon járó tavi lényt idéz, és gyógyít.", {"wood": 70, "stone": 25,
                                                                                         "meteor": 50}, 2, 26, 80, 0,
                            2.5, 9, 0.37, "caster", ranged(6, 4, 1.6, "magic"), ["huk_tavi_leny_hivas", "huk_tavgyogyitas"],
                            tags=["caster", "ranged", "organic"], energy=150)
    U["huk_tavi_leny"] = unit("Tavi lény", "Lábon járó, megidézett víztest; rövid ideig harcol.", {}, 0, 1, 220, 1, 2.4, 7,
                              0.5, "golem", melee(14, 1.3), tags=["summoned", "melee", "large"], summoned=True)
    U["huk_gotelovas"] = unit("Gőtelovas", "Óriásgőte hátán száguldó portyázó.", {"wood": 140, "stone": 45}, 3, 24, 280, 2,
                              3.1, 8, 0.52, "rider", melee(15, 1.1, 0.4), ["huk_gotevagta"],
                              tags=["cavalry", "melee", "organic"], requires=["huk_gyongymuhely"])
    U["huk_szitakoto"] = unit("Szitakötő-felderítő", "Gyors, repülő felderítő; gyenge, de messzire lát.",
                              {"wood": 70, "stone": 20}, 1, 16, 70, 0, 4.2, 12, 0.4, "flyer", ranged(5, 2.5, 1.0),
                              flying=True, tags=["flyer"])
    U["huk_teknos_mozsar"] = unit("Teknőshátas mozsár", "Óriásteknős hátán gyöngyágyú – épületek ellen.",
                                  {"wood": 210, "stone": 110}, 3, 32, 330, 3, 1.6, 9, 0.62, "artillery",
                                  ranged(38, 7.5, 3.2, "boulder", air=False, splash=0.9, bonus={"building": 35}),
                                  tags=["siege", "large", "organic"], requires=["huk_gyongymuhely"])
    # hősök
    hero_cost = {"wood": 210, "meteor": 40, "stone": 70}
    U["huk_hos_kvarra"] = unit("Kvarra, a Tóanya", "Az öreg tóanya: tavat fakaszt, és vízből gyúrt lényeket kelt életre.",
                               hero_cost, 5, 40, 230, 0, 2.6, 10, 0.4, "caster", ranged(14, 5, 1.4, "magic"),
                               tags=["hero", "ranged", "organic"], hero=True, energy=200,
                               hero_abilities=["h_kva_arasztas", "h_kva_forras", "h_kva_lenyek", "h_kva_ozonviz"],
                               growth={"hp": 26, "damage": 2, "armor": 0.2, "energy": 20}, requires=["huk_toteremto"])
    U["huk_hos_bulgok"] = unit("Bul’gok Kampókirály", "A kampókardosok királya: rántás, ugrás, tavi tombolás.", hero_cost, 5,
                               40, 420, 2, 2.9, 9, 0.46, "warrior", melee(24, 1.1), tags=["hero", "melee", "organic"],
                               hero=True, energy=120,
                               hero_abilities=["h_bul_kampo", "h_bul_ugras", "h_bul_brekeges", "h_bul_kiraly"],
                               growth={"hp": 40, "damage": 3, "armor": 0.3, "energy": 10}, requires=["huk_toteremto"])
    U["huk_hos_iriss"] = unit("Iriss Szivárványgyöngy", "Gyöngyhalász-hercegnő: gyógyít, és szivárványfénnyel véd.",
                              hero_cost, 5, 40, 280, 1, 2.8, 10, 0.4, "archer", ranged(16, 6, 1.2, "magic"),
                              tags=["hero", "ranged", "organic"], hero=True, energy=180,
                              hero_abilities=["h_iri_gyongy", "h_iri_szivarvany", "h_iri_aldas", "h_iri_gyongykapu"],
                              growth={"hp": 30, "damage": 3, "armor": 0.2, "energy": 15}, requires=["huk_toteremto"])
    # épületek
    B["huk_korallcsarnok"] = bld("Korallcsarnok", "A békanép csarnoka korallból és üvegbuborékokból. Leadóhely, Tóásókat nevel.",
                                 {"wood": 360, "stone": 40}, 70, (4, 4), 1400, "hq", supply=10,
                                 dropoff=["wood", "stone", "meteor"], produces=["huk_toaso"], sight=10)
    B["huk_tavacska"] = bld("Tavacska", "Tavirózsás vízfolt (2×2): a Tóásó ott állva 4 mp alatt ássa ki. A Huk’ucha csak tavacskára építhet; a "
                            "szomszédos tavacskák egybefüggő tóvá olvadnak. A tavon a békanép gyorsabb, mindenki más lassabb.",
                            {"wood": 15}, 0, (2, 2), 1, "pond", pond_tile=True, no_sprite=True)
    B["huk_gyongybuborek"] = bld("Gyöngybuborék-otthon", "Lakható üvegbuborék a tavon: +8 létszám.", {"wood": 90, "stone": 10},
                                 25, (2, 2), 450, "house", supply=8)
    B["huk_nadbarakk"] = bld("Nádbarakk", "Csúzlisokat, kampókardosokat és szitakötő-felderítőket képez.",
                             {"wood": 150, "stone": 40}, 35, (3, 3), 850, "barracks",
                             produces=["huk_csuzlis", "huk_kampokardos", "huk_szitakoto"])
    B["huk_toteremto"] = bld("Tóteremtő szentély", "Tóidézőket és hősöket képez.", {"wood": 160, "stone": 80, "meteor": 40}, 45,
                             (3, 3), 850, "temple", produces=["huk_toidezo", "huk_hos_kvarra", "huk_hos_bulgok", "huk_hos_iriss"],
                             requires=["huk_nadbarakk"])
    B["huk_gyongymuhely"] = bld("Gyöngyműhely", "A békanép fejlesztései: gyöngy, kagylóhéj, tavirózsa.",
                                {"wood": 150, "stone": 120}, 45, (3, 3), 800, "tech",
                                researches=["huk_gyongygolyo", "huk_kagylohej", "huk_tavirozsa", "huk_ugrolabak"],
                                requires=["huk_nadbarakk"])
    B["huk_goteistallo"] = bld("Gőteistálló", "Gőtelovasokat és teknőshátas mozsarakat nevel.", {"wood": 180, "stone": 90}, 40,
                               (3, 3), 900, "pen", produces=["huk_gotelovas", "huk_teknos_mozsar"],
                               requires=["huk_gyongymuhely"])
    B["huk_korallor"] = bld("Korallőr", "Korall-védőtorony gyöngyhajítóval.", {"wood": 100, "stone": 60}, 30, (2, 2), 600,
                            "tower", attack=ranged(12, 7, 1.2, "boulder"), detect=7)
    R["huk_gyongygolyo"] = {"name": "Gyöngygolyó", "desc": "+2 sebzés a távolsági egységeknek.", "cost": {"wood": 100, "stone": 100},
                            "time": 60, "effects": [{"stat": "damage", "tags": ["ranged"], "add": 2}]}
    R["huk_kagylohej"] = {"name": "Kagylóhéj-vért", "desc": "+1 páncél az élő egységeknek.", "cost": {"wood": 100, "stone": 120},
                          "time": 60, "effects": [{"stat": "armor", "tags": ["organic"], "add": 1}]}
    R["huk_tavirozsa"] = {"name": "Tavirózsa-tea", "desc": "+1 életerő-visszatöltés másodpercenként.",
                          "cost": {"wood": 120, "stone": 80, "meteor": 40}, "time": 60,
                          "effects": [{"stat": "regen", "tags": ["all"], "add": 1}]}
    R["huk_ugrolabak"] = {"name": "Ugrólábak", "desc": "+10% sebesség minden egységnek.", "cost": {"wood": 120, "stone": 100},
                          "time": 60, "effects": [{"stat": "speed_pct", "tags": ["all"], "add": 10}]}
    # fejlődési irányok
    U["huk_aradas_papja"] = unit("Áradás papja", "Árhullámot zúdít, és a tavat kiterjeszti.", {"wood": 80, "stone": 30,
                                                                                             "meteor": 60}, 2, 28, 90, 0,
                                 2.5, 9, 0.38, "caster", ranged(7, 4.5, 1.6, "magic"), ["huk_arhullam"],
                                 tags=["caster", "ranged", "organic"], energy=180, path="arviz")
    U["huk_iszapbeka"] = unit("Iszapbéka", "Hatalmas, iszapos varangy: elnyeli a csapásokat.", {"wood": 150, "stone": 70},
                              4, 30, 520, 3, 2.0, 7, 0.6, "golem", melee(22, 1.6, 0.5), ["huk_iszapcsapas"],
                              tags=["large", "melee", "organic"], path="arviz")
    U["huk_gyongyveto"] = unit("Gyöngyvető", "Mesterlövész: igazgyöngyöt lő, hosszú lőtávval.", {"wood": 90, "stone": 50}, 2, 24,
                               90, 0, 2.6, 10, 0.38, "archer", ranged(20, 7.5, 2.0, "boulder"),
                               tags=["infantry", "ranged", "organic"], path="gyongy")
    U["huk_kagylopajzsos"] = unit("Kagylópajzsos", "Óriáskagyló-teknőt hord – a nép egyetlen pajzsos harcosa.",
                                  {"wood": 90, "stone": 50}, 2, 22, 240, 4, 2.2, 7, 0.44, "shieldbearer", melee(11, 1.2),
                                  ["huk_kagylofal"], tags=["infantry", "melee", "organic", "armored"], path="gyongy")
    U["huk_gyongyorias"] = unit("Gyöngyóriás", "A mélység gyöngyhéjú óriása.", {"wood": 220, "stone": 120, "meteor": 60}, 5, 40,
                                760, 4, 1.8, 8, 0.7, "golem", melee(34, 1.8, 0.6), ["huk_melysegdobbantas"],
                                tags=["large", "melee", "organic"], path="melyseg")
    U["huk_melysegi_hivo"] = unit("Mélységi hívó", "Két tavi lényt hív egyszerre a mélyből.", {"wood": 90, "stone": 40,
                                                                                             "meteor": 70}, 2, 28, 95, 0,
                                  2.5, 9, 0.38, "caster", ranged(6, 4, 1.6, "magic"), ["huk_melysegi_hivas"],
                                  tags=["caster", "ranged", "organic"], energy=200, path="melyseg")
    B["huk_arvizkapu"] = bld("Árvízkapu", "Áradás papjait és iszapbékákat nevel.", {"wood": 180, "stone": 100}, 40, (3, 3), 900,
                             "temple", produces=["huk_aradas_papja", "huk_iszapbeka"], path="arviz")
    B["huk_gyongykert"] = bld("Gyöngykagyló-kert", "Gyöngyvetőket és kagylópajzsosokat nevel.", {"wood": 180, "stone": 100},
                              40, (3, 3), 900, "barracks", produces=["huk_gyongyveto", "huk_kagylopajzsos"], path="gyongy")
    B["huk_melyseg_kapuja"] = bld("A mélység kapuja", "Gyöngyóriásokat és mélységi hívókat nevel.",
                                  {"wood": 200, "stone": 120, "meteor": 40}, 45, (3, 3), 950, "temple",
                                  produces=["huk_gyongyorias", "huk_melysegi_hivo"], path="melyseg")
    paths = {
        "arviz": {"name": "Árvíz", "tagline": "A tó mindent elnyel",
                  "desc": "A tavacskák 3×3-asok lesznek, az egész nép gyorsabb; az Áradás papjai árhullámot zúdítanak, az "
                          "iszapbékák a frontot tartják. Terjeszkedő, mozgékony játék a vízen.",
                  "building": "huk_arvizkapu", "units": ["huk_aradas_papja", "huk_iszapbeka"],
                  "effects": [{"stat": "speed_pct", "tags": ["all"], "add": 10}], "rules": {"pond_size": 3}},
        "gyongy": {"name": "Gyöngy", "tagline": "Pontos lövés, kemény héj",
                   "desc": "Mesterlövész gyöngyvetők és kagylópajzsos fal; a távolsági egységek erősebbek. Védekező, "
                           "lőállásból építkező játék.",
                   "building": "huk_gyongykert", "units": ["huk_gyongyveto", "huk_kagylopajzsos"],
                   "effects": [{"stat": "damage", "tags": ["ranged"], "add": 3}, {"stat": "range", "tags": ["ranged"],
                                                                                   "add": 1}]},
        "melyseg": {"name": "Mélység", "tagline": "A tó mélyének óriásai",
                    "desc": "Gyöngyóriások és tömeges idézés; az idézett lények tovább élnek. Kevés, de hatalmas egység.",
                    "building": "huk_melyseg_kapuja", "units": ["huk_gyongyorias", "huk_melysegi_hivo"],
                    "effects": [{"stat": "hp", "tags": ["large"], "add": 80}]},
    }
    abilities.update({
        "huk_horog": dict(name="Horog", type="bolt", energy=0, cooldown=12, range=4, damage=20, slow=0, duration=1.0,
                          projectile="spear", icon="harpoon", desc="A kampó a célpontba akad: sebez és egy pillanatra megbénít."),
        "huk_tavi_leny_hivas": dict(name="Tavi lény", type="summon", cooldown=30, unit="huk_tavi_leny", count=1, lifetime=40,
                                    icon="summon", desc="Lábon járó víztestet idéz 40 másodpercre."),
        "huk_tavgyogyitas": dict(name="Tóvíz-gyógyítás", type="autoheal", cooldown=0.8, range=5, amount=9, icon="heal",
                                 desc="Gyógyítja a közeli sebesülteket."),
        "huk_gotevagta": dict(name="Gőtevágta", type="self_buff", cooldown=20, duration=4,
                              modifiers={"speed_mult": 1.8, "damage_mult": 1.2}, icon="charge", desc="Rövid, sebes roham."),
        "huk_arhullam": dict(name="Árhullám", type="area", energy=75, cooldown=8, range=8, radius=2.2, duration=3, dps=18,
                             slow=0.5, color=[110, 200, 230], sprite="lelegzet", icon="burst",
                             desc="Árhullám: sebez és lelassít."),
        "huk_iszapcsapas": dict(name="Iszapcsapás", type="nova", cooldown=16, radius=2.2, damage=36, stun=0.8,
                                color=[110, 100, 60], icon="stomp", desc="Iszapot fröccsent: körben sebez és megbénít."),
        "huk_kagylofal": dict(name="Kagylófal", type="toggle", modifiers={"speed_mult": 0.5, "armor_add": 3,
                                                                          "ranged_taken_mult": 0.5}, icon="shield",
                              desc="Teknőjébe húzódik: erős védelem, lassú mozgás."),
        "huk_melysegdobbantas": dict(name="Mélységdobbantás", type="nova", cooldown=18, radius=2.6, damage=48, stun=1.2,
                                     color=[90, 170, 200], icon="stomp", desc="Földrengető dobbantás."),
        "huk_melysegi_hivas": dict(name="Mélységi hívás", type="summon", energy=90, cooldown=30, unit="huk_tavi_leny",
                                   count=2, lifetime=60, icon="summon", desc="Két tavi lényt hív 60 másodpercre."),
        "h_kva_arasztas": hero_ab("Árasztás", "area", "Vízzel árasztja el a területet: sebez és lassít.",
                                  [{"dps": 18}, {"dps": 28}, {"dps": 38}], "burst", energy=70, cooldown=10, range=8, radius=2.2,
                                  duration=4, slow=0.5, color=[110, 200, 230], sprite="lelegzet", hotkey="Q"),
        "h_kva_forras": hero_ab("Gyógyforrás", "aura", "A közeli szövetségesek gyorsabban gyógyulnak.",
                                [{"modifiers": {"regen_add": 1}}, {"modifiers": {"regen_add": 2}}, {"modifiers": {"regen_add": 3}}],
                                "aura", radius=6, hotkey="W"),
        "h_kva_lenyek": hero_ab("Vízlények", "summon", "Tavi lényeket kelt életre.", [{"count": 1}, {"count": 2}, {"count": 3}],
                                "summon", energy=75, cooldown=25, unit="huk_tavi_leny", lifetime=40, hotkey="E"),
        "h_kva_ozonviz": hero_ab("Özönvíz", "area", "Végső: hatalmas áradás a csatatéren.", [{}], "burst", ultimate=True,
                                 energy=150, cooldown=90, range=9, radius=4, duration=6, dps=40, slow=0.4,
                                 color=[90, 170, 230], sprite="lelegzet", hotkey="R"),
        "h_bul_kampo": hero_ab("Királyi kampó", "bolt", "Megrántja és megbénítja a célpontot.",
                               [{"damage": 60}, {"damage": 100}, {"damage": 140}], "harpoon", energy=50, cooldown=9, range=6,
                               slow=0, duration=1.5, projectile="spear", hotkey="Q"),
        "h_bul_ugras": hero_ab("Békaugrás", "nova", "Nagyot ugrik és lecsap: körben sebez.",
                               [{"damage": 50}, {"damage": 85}, {"damage": 120}], "stomp", energy=60, cooldown=12, radius=2.4,
                               stun=0.8, color=[120, 200, 120], hotkey="W"),
        "h_bul_brekeges": hero_ab("Harci brekegés", "aura", "A közeli szövetségesek erősebben csapnak.",
                                  [{"modifiers": {"damage_mult": 1.08}}, {"modifiers": {"damage_mult": 1.14}},
                                   {"modifiers": {"damage_mult": 1.2}}], "aura", radius=6, hotkey="E"),
        "h_bul_kiraly": hero_ab("A Tó Királya", "self_buff", "Végső: óriássá nő, sebzése és páncélja megnő.", [{}], "rage",
                                ultimate=True, energy=100, cooldown=90, duration=15,
                                modifiers={"damage_mult": 1.6, "armor_add": 4}, hotkey="R"),
        "h_iri_gyongy": hero_ab("Igazgyöngy-lövés", "bolt", "Ragyogó gyöngy: nagy sebzés egy célpontra.",
                                [{"damage": 70}, {"damage": 110}, {"damage": 150}], "bolt", energy=45, cooldown=8, range=8,
                                projectile="magic", hotkey="Q"),
        "h_iri_szivarvany": hero_ab("Szivárványpajzs", "aura", "A közeli szövetségesek páncélt kapnak.",
                                    [{"modifiers": {"armor_add": 1}}, {"modifiers": {"armor_add": 2}}, {"modifiers": {"armor_add": 3}}],
                                    "aura", radius=6, hotkey="W"),
        "h_iri_aldas": hero_ab("Tavi áldás", "autoheal", "Gyógyítja a közeli sebesülteket.",
                               [{"amount": 10}, {"amount": 16}, {"amount": 22}], "heal", cooldown=0.8, range=6, hotkey="E"),
        "h_iri_gyongykapu": hero_ab("Gyöngykapu", "nova", "Végső: gyöngyfény-robbanás – sebez és megbénít.", [{}], "burst",
                                    ultimate=True, energy=150, cooldown=90, radius=4, damage=120, stun=2.0,
                                    color=[240, 220, 255], hotkey="R"),
    })
    return {
        "id": "hukucha", "order": 7, "name": "Huk’ucha", "tagline": "A békanép tavai",
        "desc": "A békanép maga hozza létre tavirózsás tavacskáit, amelyekre vízinövényekből, korallból és lakható "
                "üvegbuborékokból épít otthonokat. Harcosaik csúzlival vagy tompa végű, apró kampóban végződő lapos karddal "
                "küzdenek, pajzs nélkül. Mágusaik lábon járó tavi lényeket idéznek.",
        "mechanic": "pond", "style": "tribal", "color": [70, 150, 80], "accent": [120, 230, 210],
        "hq": "huk_korallcsarnok", "worker": "huk_toaso", "supply_building": "huk_gyongybuborek",
        "start_units": {"huk_toaso": 5}, "start_gather": "wood", "supply_desc": "Gyöngybuborék-otthonokkal",
        "pond": {"size": 2, "cost": {"wood": 15}}, "ascension": {"buildings": 3},
        "units": U, "buildings": B, "researches": R, "paths": paths,
        "ai": {"workers": 18, "gather_ratio": {"wood": 0.5, "stone": 0.36, "meteor": 0.14}, "pond": True,
               "build_order": ["huk_gyongybuborek", "huk_nadbarakk", "huk_gyongybuborek", "huk_toteremto", "huk_nadbarakk",
                               "huk_gyongymuhely", "huk_korallor", "huk_goteistallo", "huk_gyongybuborek"],
               "army": {"huk_csuzlis": 4, "huk_kampokardos": 3, "huk_toidezo": 1, "huk_gotelovas": 2, "huk_teknos_mozsar": 1},
               "research_order": ["huk_gyongygolyo", "huk_kagylohej", "huk_ugrolabak", "huk_tavirozsa"],
               "attack_supply": 18, "path_pref": ["arviz", "gyongy", "melyseg"],
               "paths": {"arviz": {"build": ["huk_arvizkapu"], "army": {"huk_aradas_papja": 1, "huk_iszapbeka": 2}},
                         "gyongy": {"build": ["huk_gyongykert"], "army": {"huk_gyongyveto": 3, "huk_kagylopajzsos": 3}},
                         "melyseg": {"build": ["huk_melyseg_kapuja"], "army": {"huk_gyongyorias": 1,
                                                                                "huk_melysegi_hivo": 1}}},
               "heroes": ["huk_hos_kvarra", "huk_hos_bulgok", "huk_hos_iriss"]},
    }


# ---------------------------------------------------------------- Ooni (onik és nianok)
def ooni(abilities):
    U, B, R = {}, {}, {}
    U["oon_lampasvivo"] = unit("Lámpásvivő", "Kis oni munkás: papírlámpást állít (a nép csak lámpásfényben építhet), gyűjt, épít.",
                               {"wood": 50}, 1, 12, 55, 0, 2.6, 7, 0.37, "worker", melee(5, 1.4), worker=True,
                               tags=["worker", "organic"])
    U["oon_oni_harcos"] = unit("Oni harcos", "Vörös bőrű, szarvas oni, vasbuzogánnyal.", {"wood": 70, "stone": 25}, 2, 18, 190,
                               2, 2.4, 7, 0.44, "warrior", melee(14, 1.3), ["oon_buzogany_roham"],
                               tags=["infantry", "melee", "organic"])
    U["oon_kohajito"] = unit("Kőhajító oni", "Sziklákat hajít az ellenségre.", {"wood": 75, "stone": 30}, 2, 20, 120, 1, 2.3, 8,
                             0.42, "archer", ranged(14, 5.5, 1.7, "boulder", air=False), tags=["infantry", "ranged", "organic"])
    U["oon_nian_or"] = unit("Nian őrző", "Négylábú, zöld sörényű őrző. Kapuőrként egy épület mellé állhat: az épület "
                            "páncélt kap, a nian helyben marad és védi.", {"wood": 120, "stone": 50}, 3, 26, 380, 3, 2.6, 8,
                            0.56, "beast", melee(17, 1.3, 0.4), ["oon_kapuorzes"], tags=["beast", "melee", "organic", "large"])
    U["oon_tengu"] = unit("Tengu követ", "Szárnyas, hollótollú követ: legyezőjével szélvihart kavar.",
                          {"wood": 100, "stone": 35, "meteor": 50}, 2, 26, 110, 0, 3.6, 11, 0.44, "flyer",
                          ranged(9, 4, 1.3, "magic"), ["oon_szelvihar"], flying=True, energy=150,
                          tags=["flyer", "caster"])
    U["oon_kitsune"] = unit("Kitsune-maszkos", "Rókamaszkos mágus: illúzió-másokat szőve megtéveszti az ellent, és gyógyít.",
                            {"wood": 75, "stone": 25, "meteor": 60}, 2, 26, 85, 0, 2.5, 9, 0.37, "caster",
                            ranged(6, 4, 1.6, "magic"), ["oon_illuzio", "oon_lampasgyogyitas"],
                            tags=["caster", "ranged", "organic"], energy=150)
    U["oon_illuzio_mas"] = unit("Illúzió-más", "Rövid életű rókaszellem-másolat.", {}, 0, 1, 50, 0, 2.6, 7, 0.4, "warrior",
                                melee(5, 1.2), tags=["summoned", "melee"], summoned=True)
    U["oon_kapudongeto"] = unit("Kapudöngető", "Faltörő kos, onik tolják – épületek ellen.", {"wood": 200, "stone": 100}, 3, 30,
                                420, 3, 1.8, 7, 0.6, "artillery", melee(45, 2.6, 0.5),
                                tags=["siege", "large", "mech"], requires=["oon_lampaskert"])
    U["oon_o_oni"] = unit("Ō-oni", "Óriás oni, a hegyek ura.", {"wood": 220, "stone": 110, "meteor": 50}, 5, 40, 780, 4, 2.0, 8,
                          0.72, "golem", melee(34, 1.8, 0.6), ["oon_oriasdobbantas"], tags=["large", "melee", "organic"],
                          requires=["oon_lampaskert"])
    hero_cost = {"wood": 210, "meteor": 40, "stone": 70}
    U["oon_hos_akahana"] = unit("Akahana, a Vörös Buzogány", "Az onik vezére: rohamoz és földet rengeti.", hero_cost, 5, 40, 460,
                                3, 2.7, 9, 0.48, "warrior", melee(26, 1.2), tags=["hero", "melee", "organic"], hero=True,
                                energy=120, hero_abilities=["h_aka_roham", "h_aka_rengetes", "h_aka_harag", "h_aka_vorosvihar"],
                                growth={"hp": 42, "damage": 3, "armor": 0.3, "energy": 10}, requires=["oon_meselenyek"])
    U["oon_hos_shiro"] = unit("Shiro, a Kapuőr", "A fehér nian: védi a kapukat és a társait.", hero_cost, 5, 40, 520, 4, 2.6, 9,
                              0.56, "beast", melee(20, 1.2, 0.4), tags=["hero", "melee", "organic", "beast"], hero=True,
                              energy=150, hero_abilities=["h_shi_pajzs", "h_shi_uvoltes", "h_shi_kapu", "h_shi_oroktolany"],
                              growth={"hp": 48, "damage": 2, "armor": 0.4, "energy": 12}, requires=["oon_meselenyek"])
    U["oon_hos_karasu"] = unit("Karasu, a tengu bölcs", "Az öreg tengu: szélvihart és villámot hív.", hero_cost, 5, 40, 260, 1,
                               3.6, 12, 0.44, "flyer", ranged(16, 5, 1.2, "magic"), tags=["hero", "flyer", "caster"], hero=True,
                               flying=True, energy=200,
                               hero_abilities=["h_kar_villam", "h_kar_szel", "h_kar_tollak", "h_kar_tajfun"],
                               growth={"hp": 26, "damage": 3, "armor": 0.2, "energy": 20}, requires=["oon_meselenyek"])
    B["oon_nian_csarnok"] = bld("Nian csarnok", "Márványoszlopok, ívelt cseréptető, faragott nian-őrzők. Leadóhely, "
                                "Lámpásvivőket nevel; fénye körül építhet a nép.", {"wood": 360, "stone": 60}, 70, (4, 4), 1500,
                                "hq", supply=10, dropoff=["wood", "stone", "meteor"], produces=["oon_lampasvivo"],
                                light_radius=8, light_heal=2, sight=10)
    B["oon_papirlampas"] = bld("Papírlámpás", "Olcsó lámpás: fénye bevilágít, tágítja az építési területet, és az Ooni "
                               "egységek gyógyulnak benne.", {"wood": 25}, 6, (1, 1), 120, "lantern", lantern=True,
                               light_radius=6, light_heal=2, sight=8, foundation_free=True)
    B["oon_lakohaz"] = bld("Oni lakóház", "Cseréptetős oni ház: +8 létszám.", {"wood": 90, "stone": 15}, 25, (2, 2), 500, "house",
                           supply=8)
    B["oon_edzoudvar"] = bld("Edzőudvar", "Oni harcosokat és kőhajítókat képez.", {"wood": 150, "stone": 50}, 35, (3, 3), 900,
                             "barracks", produces=["oon_oni_harcos", "oon_kohajito", "oon_nian_or"])
    B["oon_meselenyek"] = bld("Meselények szentélye", "Kitsune-maszkosokat és hősöket képez.",
                              {"wood": 160, "stone": 80, "meteor": 40}, 45, (3, 3), 850, "temple",
                              produces=["oon_kitsune", "oon_hos_akahana", "oon_hos_shiro", "oon_hos_karasu"],
                              requires=["oon_edzoudvar"])
    B["oon_tengu_feszek"] = bld("Tengu-fészek", "Tengu követeket nevel.", {"wood": 160, "stone": 70}, 40, (3, 3), 850, "tower",
                                produces=["oon_tengu"], requires=["oon_edzoudvar"])
    B["oon_lampaskert"] = bld("Lámpáskert", "Az Ooni fejlesztései és ostromszíne.", {"wood": 150, "stone": 120}, 45, (3, 3), 800,
                              "tech", researches=["oon_vasbuzogany", "oon_lakkozott_vert", "oon_lampasolaj", "oon_szelparna"],
                              produces=["oon_kapudongeto", "oon_o_oni"], requires=["oon_edzoudvar"])
    B["oon_kapuorzo_torony"] = bld("Kapuőrző torony", "Faragott nian-szoborral díszített őrtorony.", {"wood": 110, "stone": 60},
                                   30, (2, 2), 650, "tower", attack=ranged(13, 7, 1.2, "arrow"), detect=7)
    R["oon_vasbuzogany"] = {"name": "Vasbuzogány", "desc": "+2 sebzés a közelharcosoknak.", "cost": {"wood": 100, "stone": 100},
                            "time": 60, "effects": [{"stat": "damage", "tags": ["melee"], "add": 2}]}
    R["oon_lakkozott_vert"] = {"name": "Lakkozott vért", "desc": "+1 páncél az élő egységeknek.",
                               "cost": {"wood": 100, "stone": 120}, "time": 60,
                               "effects": [{"stat": "armor", "tags": ["organic"], "add": 1}]}
    R["oon_lampasolaj"] = {"name": "Lámpásolaj", "desc": "+1 látótáv minden egységnek.", "cost": {"wood": 120, "stone": 80},
                           "time": 50, "effects": [{"stat": "sight", "tags": ["all"], "add": 1}]}
    R["oon_szelparna"] = {"name": "Szélpárna", "desc": "+15% sebesség a repülőknek.", "cost": {"wood": 120, "stone": 80,
                                                                                            "meteor": 40}, "time": 60,
                          "effects": [{"stat": "speed_pct", "tags": ["flyer"], "add": 15}]}
    # irányok
    U["oon_berserker_oni"] = unit("Tomboló oni", "Kétbuzogányos, megszállott oni.", {"wood": 110, "stone": 40}, 3, 24, 260, 2,
                                  2.7, 7, 0.46, "warrior", melee(20, 1.0), ["oon_tombolas"],
                                  tags=["infantry", "melee", "organic"], path="voros")
    U["oon_dobos"] = unit("Taiko-dobos", "Harci dobjával bátorítja az onikat.", {"wood": 80, "stone": 30, "meteor": 40}, 2, 24, 120,
                          1, 2.5, 8, 0.4, "caster", melee(8, 1.4), ["oon_taiko"], tags=["infantry", "organic"], path="voros")
    U["oon_kokutya"] = unit("Kőkutya-őr", "Élő kőoroszlán – a kapuk legkeményebb őre.", {"wood": 150, "stone": 90}, 4, 30, 620, 5,
                            2.0, 8, 0.6, "beast", melee(18, 1.4, 0.4), ["oon_kapuorzes"],
                            tags=["beast", "melee", "large", "mech"], path="kapu")
    U["oon_lampaspap"] = unit("Lámpáspap", "A lámpások papja: gyógyít, és védőfényt von a társakra.",
                              {"wood": 80, "stone": 30, "meteor": 60}, 2, 26, 95, 0, 2.5, 10, 0.38, "caster",
                              ranged(6, 4, 1.6, "magic"), ["oon_vedofeny", "oon_lampasgyogyitas"],
                              tags=["caster", "ranged", "organic"], energy=180, path="kapu")
    U["oon_holloharcos"] = unit("Hollótengu-harcos", "Kardos tengu, a magasból csap le.", {"wood": 120, "stone": 45,
                                                                                         "meteor": 40}, 3, 26, 170, 1, 3.8, 10,
                                0.46, "flyer", ranged(14, 1.5, 1.0, "arrow"), flying=True, tags=["flyer"], path="tengu")
    U["oon_viharhivo"] = unit("Viharhívó tengu", "Villámot és tájfunt hív.", {"wood": 100, "stone": 40, "meteor": 80}, 3, 30,
                              120, 0, 3.4, 11, 0.44, "flyer", ranged(10, 4.5, 1.4, "magic"), ["oon_villamcsapas"],
                              flying=True, energy=200, tags=["flyer", "caster"], path="tengu")
    B["oon_voros_dojo"] = bld("Vörös dódzsó", "Tomboló onikat és taiko-dobosokat képez.", {"wood": 180, "stone": 100}, 40, (3, 3),
                              950, "barracks", produces=["oon_berserker_oni", "oon_dobos"], path="voros")
    B["oon_kapuszentely"] = bld("Kapuszentély", "Kőkutya-őröket és lámpáspapokat nevel.", {"wood": 180, "stone": 120}, 40, (3, 3),
                                1000, "temple", produces=["oon_kokutya", "oon_lampaspap"], path="kapu")
    B["oon_szelcsucs"] = bld("Szélcsúcs", "Hollótengu-harcosokat és viharhívókat nevel.", {"wood": 180, "stone": 100,
                                                                                         "meteor": 40}, 45, (3, 3), 900,
                             "tower", produces=["oon_holloharcos", "oon_viharhivo"], path="tengu")
    paths = {
        "voros": {"name": "Vörös buzogány", "tagline": "Oni roham", "desc": "Tomboló onik és harci dobok; a közelharcosok "
                  "erősebbek és gyorsabbak. Korai, kemény nyomás.", "building": "oon_voros_dojo",
                  "units": ["oon_berserker_oni", "oon_dobos"],
                  "effects": [{"stat": "damage", "tags": ["melee"], "add": 3}, {"stat": "speed_pct", "tags": ["infantry"],
                                                                                  "add": 10}]},
        "kapu": {"name": "Kapuőrzők", "tagline": "A nian-kapuk erőssége", "desc": "Kőkutya-őrök és lámpáspapok; az épületek "
                 "és a nagy testű őrzők erősebbek. Lassú, áttörhetetlen védelem.", "building": "oon_kapuszentely",
                 "units": ["oon_kokutya", "oon_lampaspap"],
                 "effects": [{"stat": "armor", "tags": ["building"], "add": 2}, {"stat": "hp", "tags": ["beast"], "add": 80}]},
        "tengu": {"name": "Tengu szél", "tagline": "Légierő és vihar", "desc": "Hollótengu-harcosok és viharhívók; a repülők "
                  "gyorsabbak. Mozgékony, légi portyázó játék.", "building": "oon_szelcsucs",
                  "units": ["oon_holloharcos", "oon_viharhivo"],
                  "effects": [{"stat": "speed_pct", "tags": ["flyer"], "add": 15}, {"stat": "damage", "tags": ["flyer"],
                                                                                      "add": 2}]},
    }
    abilities.update({
        "oon_buzogany_roham": dict(name="Buzogányroham", type="self_buff", cooldown=20, duration=4,
                                   modifiers={"speed_mult": 1.5, "damage_mult": 1.3}, icon="rage", desc="Rövid, kemény roham."),
        "oon_kapuorzes": dict(name="Kapuőrzés", type="toggle", modifiers={"speed_mult": 0.0, "armor_add": 2}, icon="shield",
                              desc="Kapuőrként megáll egy épület mellett: nem mozdul, az épület +3 páncélt kap (legfeljebb +6)."),
        "oon_szelvihar": dict(name="Szélvihar", type="area", energy=70, cooldown=8, range=8, radius=2.0, duration=4, dps=14,
                              slow=0.5, color=[200, 230, 255], sprite="lelegzet", icon="burst",
                              desc="Legyezővel kavart szélvihar: sebez és lassít."),
        "oon_illuzio": dict(name="Rókaillúzió", type="summon", cooldown=30, unit="oon_illuzio_mas", count=2, lifetime=20,
                            icon="summon", desc="Két rókaszellem-másolatot sző 20 másodpercre."),
        "oon_lampasgyogyitas": dict(name="Lámpásfény-gyógyítás", type="autoheal", cooldown=0.8, range=5, amount=9, icon="heal",
                                    desc="Gyógyítja a közeli sebesülteket."),
        "oon_oriasdobbantas": dict(name="Óriásdobbantás", type="nova", cooldown=18, radius=2.6, damage=46, stun=1.2,
                                   color=[200, 80, 60], icon="stomp", desc="Földrengető dobbantás."),
        "oon_tombolas": dict(name="Tombolás", type="self_buff", cooldown=22, duration=6,
                             modifiers={"damage_mult": 1.5, "armor_add": -1}, icon="rage", desc="Tombol: erősebben üt, védtelenebb."),
        "oon_taiko": dict(name="Taiko-ritmus", type="chant", radius=6, modifiers={"damage_mult": 1.1, "speed_mult": 1.06},
                          icon="song", desc="Harci dob: a közeli társak erősebben és gyorsabban harcolnak."),
        "oon_vedofeny": dict(name="Védőfény", type="chant", radius=5, modifiers={"armor_add": 2}, icon="aura",
                             desc="A közeli társak +2 páncélt kapnak."),
        "oon_villamcsapas": dict(name="Villámcsapás", type="bolt", energy=50, cooldown=9, range=8, damage=60, slow=0,
                                 duration=0.8, projectile="magic", icon="lightning", desc="Villám: nagy sebzés, rövid bénulás."),
        "h_aka_roham": hero_ab("Vörös roham", "bolt", "Nekiront a célpontnak: sebez és megbénít.",
                               [{"damage": 60}, {"damage": 100}, {"damage": 140}], "charge", energy=50, cooldown=9, range=6,
                               slow=0, duration=1.2, projectile="spear", hotkey="Q"),
        "h_aka_rengetes": hero_ab("Földrengetés", "nova", "Buzogányával a földbe sújt: körben sebez.",
                                  [{"damage": 50}, {"damage": 85}, {"damage": 120}], "stomp", energy=60, cooldown=12, radius=2.6,
                                  stun=1.0, color=[200, 80, 60], hotkey="W"),
        "h_aka_harag": hero_ab("Oni harag", "aura", "A közeli onik erősebben csapnak.",
                               [{"modifiers": {"damage_mult": 1.08}}, {"modifiers": {"damage_mult": 1.14}},
                                {"modifiers": {"damage_mult": 1.2}}], "aura", radius=6, hotkey="E"),
        "h_aka_vorosvihar": hero_ab("Vörös vihar", "self_buff", "Végső: tomboló vörös vihar – sebzése és sebessége megnő.",
                                    [{}], "rage", ultimate=True, energy=100, cooldown=90, duration=15,
                                    modifiers={"damage_mult": 1.6, "speed_mult": 1.3}, hotkey="R"),
        "h_shi_pajzs": hero_ab("Nian-pajzs", "aura", "A közeli társak páncélt kapnak.",
                               [{"modifiers": {"armor_add": 1}}, {"modifiers": {"armor_add": 2}}, {"modifiers": {"armor_add": 3}}],
                               "aura", radius=6, hotkey="Q"),
        "h_shi_uvoltes": hero_ab("Nian-üvöltés", "nova", "Rettentő üvöltés: körben sebez és megbénít.",
                                 [{"damage": 40}, {"damage": 70}, {"damage": 100}], "roar", energy=60, cooldown=14, radius=3,
                                 stun=1.2, color=[120, 220, 140], hotkey="W"),
        "h_shi_kapu": hero_ab("Őrkapu", "autoheal", "Gyógyítja a közeli sebesülteket.",
                              [{"amount": 10}, {"amount": 16}, {"amount": 22}], "heal", cooldown=0.8, range=6, hotkey="E"),
        "h_shi_oroktolany": hero_ab("Az őrök hívása", "summon", "Végső: két kőkutya-őrt hív.", [{}], "summon", ultimate=True,
                                    energy=150, cooldown=120, unit="oon_kokutya", count=2, lifetime=45, hotkey="R"),
        "h_kar_villam": hero_ab("Tengu-villám", "bolt", "Villám egy célpontra.",
                                [{"damage": 70}, {"damage": 110}, {"damage": 150}], "lightning", energy=45, cooldown=8, range=8,
                                projectile="magic", hotkey="Q"),
        "h_kar_szel": hero_ab("Szélörvény", "area", "Szélörvény: sebez és lassít.", [{"dps": 16}, {"dps": 26}, {"dps": 36}],
                              "burst", energy=70, cooldown=10, range=8, radius=2.2, duration=4, slow=0.5,
                              color=[200, 230, 255], sprite="lelegzet", hotkey="W"),
        "h_kar_tollak": hero_ab("Hollótollak", "aura", "A közeli repülők gyorsabbak.",
                                [{"modifiers": {"speed_mult": 1.08}}, {"modifiers": {"speed_mult": 1.14}},
                                 {"modifiers": {"speed_mult": 1.2}}], "aura", radius=7, hotkey="E"),
        "h_kar_tajfun": hero_ab("Tájfun", "area", "Végső: tájfun söpör végig a csatatéren.", [{}], "burst", ultimate=True,
                                energy=150, cooldown=90, range=9, radius=4, duration=6, dps=40, slow=0.4,
                                color=[200, 230, 255], sprite="lelegzet", hotkey="R"),
    })
    return {
        "id": "ooni", "order": 8, "name": "Ooni", "tagline": "Onik, nianok és tengu követek",
        "desc": "Az onik és nianok közösségében a buzogányos harcosok mellett négylábú őrzők és szárnyas tengu követek is "
                "szolgálnak. Csarnokaik márványoszlopai fölé ívelt cseréptetők borulnak, kapuiknál faragott őrzők és "
                "papírlámpások állnak.",
        "mechanic": "lantern", "style": "imperial", "color": [170, 40, 40], "accent": [80, 200, 140],
        "hq": "oon_nian_csarnok", "worker": "oon_lampasvivo", "supply_building": "oon_lakohaz",
        "start_units": {"oon_lampasvivo": 5}, "start_gather": "wood", "supply_desc": "Oni lakóházakkal",
        "ascension": {"buildings": 3}, "units": U, "buildings": B, "researches": R, "paths": paths,
        "ai": {"workers": 18, "gather_ratio": {"wood": 0.5, "stone": 0.36, "meteor": 0.14}, "lantern": "oon_papirlampas",
               "build_order": ["oon_lakohaz", "oon_papirlampas", "oon_edzoudvar", "oon_papirlampas", "oon_lakohaz",
                               "oon_meselenyek", "oon_edzoudvar", "oon_lampaskert", "oon_papirlampas", "oon_tengu_feszek",
                               "oon_kapuorzo_torony", "oon_lakohaz"],
               "army": {"oon_oni_harcos": 4, "oon_kohajito": 3, "oon_nian_or": 1, "oon_kitsune": 1, "oon_tengu": 1,
                        "oon_kapudongeto": 1},
               "research_order": ["oon_vasbuzogany", "oon_lakkozott_vert", "oon_lampasolaj", "oon_szelparna"],
               "attack_supply": 18, "path_pref": ["voros", "kapu", "tengu"],
               "paths": {"voros": {"build": ["oon_voros_dojo"], "army": {"oon_berserker_oni": 3, "oon_dobos": 1}},
                         "kapu": {"build": ["oon_kapuszentely"], "army": {"oon_kokutya": 1, "oon_lampaspap": 1}},
                         "tengu": {"build": ["oon_szelcsucs"], "army": {"oon_holloharcos": 3, "oon_viharhivo": 1}}},
               "heroes": ["oon_hos_akahana", "oon_hos_shiro", "oon_hos_karasu"]},
    }


def main():
    imperium_foundation()
    abp = os.path.join(ROOT, "data", "abilities.json")
    abilities = rd(abp)
    for maker in (hukucha, ooni):
        d = maker(abilities)
        for k, v in d["units"].items():
            if v.get("path") and v.get("requires") is None:
                pass
        wr(os.path.join(FDIR, d["id"] + ".json"), d)
    builder_styles(abilities)
    gatherer_split()
    otletdoboz_batch2(abilities)
    szoborpark()
    meteor_rift(abilities)
    dow_mechanics(abilities)
    rift_guards()
    ahitat(abilities)
    egi_kegy(abilities)
    croftian_spikes()
    elf_core_raptor()
    research_pack()
    wullof_rework(abilities)
    research_tiers_and_extras(abilities)
    wr(abp, abilities)
    gp = os.path.join(ROOT, "data", "game.json")
    g = rd(gp)
    g["pond_speed"] = {"own": 1.3, "other": 0.7}
    wr(gp, g)
    print("új mechanikák: Vasbirodalom-alapozás; új fajok: Huk’ucha (tavacskák), Ooni (lámpások, kapuőrzők)")


if __name__ == "__main__":
    main()
