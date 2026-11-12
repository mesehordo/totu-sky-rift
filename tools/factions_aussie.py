"""Wullof – ausztrál karakter: bumeráng, emuk, kéregkunyhók, termeszvárak, eukaliptusz.

    python tools/factions_aussie.py      (a factions_v3.py végén automatikusan is lefut)

Az azonosítók (id-k) változatlanok, így a hadjárat, a pályák és a mentések nem törnek el;
csak a nevek, leírások, a lövedékek és néhány képesség változik.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

UNITS = {
    "wul_gyujtogeto": ("Gyűjtögető", "Munkás. Gyűjt, épít, javít; fonott háti kosárban hordja a zsákmányt."),
    "wul_landzsas": ("Lándzsás harcos", "Gyors, kovakőhegyű lándzsás, keskeny festett hárítópajzzsal. Falkában veszélyes."),
    "wul_ijas_vadasz": ("Bumerángvető", "Távolsági vadász: a bumeráng visszatérő íven a közeli ellenségeket is "
                                        "eltalálja. Földi és légi célpontokra."),
    "wul_tomahawkos": ("Dárdavető portyázó", "Gyors portyázó dárdavetővel (woomera): a munkások és a "
                                             "távolsági egységek réme."),
    "wul_bolenylovas": ("Bölénylovas", "Bölényen ülő wullof harcos: erős roham. Le is szállhat."),
    "wul_mennydorgesmadar": ("Viharkakadu", "Óriás viharkakadu; villámai a célpont környékét is sújtják."),
    "wul_totemorias": ("Termeszvár-óriás", "Életre kelt vörös termeszvár – lassú, de az épületeket porrá zúzza."),
    "wul_vandorszeker": ("Vándorbatyu", "Felszedett tábor, rudakra kötve. Jobb gomb: Letelepítés máshol."),
    "wul_hos_kurrin": ("Kurrin Harcfőnök", "Harci bölényen lovagló harcfőnök: tapos, üvölt, és a Hét Út "
                                          "harcosait hívja."),
}
BUILDINGS = {
    "wul_tanacssator": ("Tanácskunyhó", "A törzs nagy kéregkunyhója. Leadóhely, Gyűjtögetőket nevel. Felszedhető."),
    "wul_tipi": ("Kéregkunyhó", "Eukaliptuszkéreg-kunyhó: +8 létszám. Felszedhető és vándorbatyuban máshová vihető."),
    "wul_harcosok_tuze": ("Harcosok tüze", "Lándzsásokat, dárdavetőket és bumerángvetőket képez. Felszedhető."),
    "wul_bolenykaram": ("Emukarám", "Emulovasokat és termeszvár-óriásokat ad. Felszedhető."),
    "wul_samansator": ("Sámánkunyhó", "Holdsámánokat képez, fejlesztéseket kutat. Felszedhető."),
    "wul_mennydorges_szikla": ("Vörös szikla", "A viharkakaduk fészke a pusztai vörös sziklán."),
    "wul_szellemtotem": ("Eukaliptusz-őrfa", "Kilátós őrfa: a fenti vadászok bumerángokkal védik a tábort."),
}
RESEARCH = {
    "wul_obszidian_landzsa": ("Kovakő lándzsahegyek", "+2 sebzés a közelharci egységeknek."),
    "wul_holdfeny_ijak": ("Vasfa bumerángok", "+2 sebzés a távolsági egységeknek."),
    "wul_bolenybor": ("Kengurubőr vértek", "+1 páncél minden élő egységnek."),
}
ABILITIES = {
    "tomahawk": {"name": "Dárdavetés", "projectile": "spear", "icon": "spear",
                 "desc": "Dárdavetővel (woomera) hajított dárda: messzire repül, sebez és lassít.", "range": 6},
    "bumerang": {"name": "Bumeráng", "type": "bolt", "hotkey": "B", "cooldown": 9, "range": 6, "damage": 24,
                 "bounces": 2, "projectile": "boomerang", "icon": "bolt",
                 "desc": "Visszatérő bumeráng: a célpont után még két közeli ellenséget eltalál (csökkenő sebzéssel)."},
    "h_kur_taposas": {"name": "Bölénytaposás", "desc": "A harci bölény a földbe dobbant: sebez és megbénít."},
    "h_tava_madar": {"desc": "Végső: két viharkakadu érkezik 45 másodpercre."},
}
EN = {
    "wul_gyujtogeto": ("Gatherer", "Worker. Gathers, builds, repairs; carries the haul in a woven back basket."),
    "wul_landzsas": ("Spear Warrior", "A fast warrior with a flint spear and a narrow painted parrying shield. "
                                      "Dangerous in a pack."),
    "wul_ijas_vadasz": ("Boomerang Thrower", "A ranged hunter: the boomerang's returning arc also strikes nearby "
                                             "enemies. Hits ground and air."),
    "wul_tomahawkos": ("Spear-thrower Raider", "A fast raider with a woomera: the terror of workers and ranged units."),
    "wul_mennydorgesmadar": ("Storm Cockatoo", "A giant storm cockatoo; its lightning also strikes around the target."),
    "wul_totemorias": ("Termite-mound Giant", "A red termite mound come to life – slow, but it grinds buildings to dust."),
    "wul_vandorszeker": ("Bundle Camp", "A packed-up camp tied to poles. Right-click: settle elsewhere."),
    "wul_hos_kurrin": ("Warchief Kurrin", "A warchief riding a war bison: tramples, howls, and calls the warriors "
                                              "of the Seven Roads."),
    "wul_emu": ("Emu", "Breeding emu: +6 population. At most 5 per Emu Pen."),
    "wul_boleny": ("Bison", "A strong but slow bison. A Wullof foot warrior can be mounted on it."),
    "wul_bolenylovas": ("Bison Rider", "A Wullof warrior riding a bison: a mighty charge. Can also dismount."),
    "wul_groomi": ("Groomi", "A red monster bird: a fast, biting ground hunter. In the Groomi Nest it can learn acid, "
                             "fire or frost spit (only one)."),
    "wul_bolenyakol": ("Bison Corral", "Provides Bison and Termite-mound Giants. Can be packed up."),
    "wul_groomi_feszek": ("Groomi Nest", "Raises Groomis; their spit is chosen here."),
    "wul_savkopes": ("Acid Spit", "Groomis can spit corrosive, poisonous acid. Only one spit can be chosen: once you "
                                  "research this, the other two can never be researched."),
    "wul_tuzkopes": ("Fire Spit", "Groomis can spit a jet of flame. Only one spit can be chosen: once you research "
                                  "this, the other two can never be researched."),
    "wul_fagykopes": ("Frost Spit", "Groomis can spit icy slime. Only one spit can be chosen: once you research this, "
                                    "the other two can never be researched."),
    "riado_futas": ("Panic Run", "The emu panics and runs: +80% speed for 4 s."),
    "felultetes": ("Mount", "The nearest Wullof foot warrior mounts the bison: together they become a Bison Rider."),
    "leszallas": ("Dismount", "The rider dismounts: a separate bison and foot warrior again."),
    "savkopes": ("Acid Spit", "Corrosive acid: 8 damage, then 6 poison damage per second for 6 s."),
    "tuzkopes": ("Fire Spit", "A jet of flame: 26 damage to the target, half to those around it."),
    "fagykopes": ("Frost Spit", "Icy spit: 10 damage, and the target freezes for 2.5 s (cannot move or attack)."),
    "wul_tanacssator": ("Council Hut", "The tribe's great bark hut. Drop-off point, trains Gatherers. Can be packed up."),
    "wul_tipi": ("Bark Hut", "Eucalyptus-bark hut: trains Spear Warriors. Can be packed up."),
    "wul_harcosok_tuze": ("Warriors' Fire", "Trains spear-throwers and boomerang throwers. Can be packed up."),
    "wul_bolenykaram": ("Emu Pen", "Breeds emus (at most 5 per pen). Only emus raise the population limit. "
                                   "Can be packed up."),
    "wul_samansator": ("Shaman's Hut", "Trains Moon Shamans and researches upgrades. Can be packed up."),
    "wul_mennydorges_szikla": ("Red Rock", "The storm cockatoos nest on the red rock of the plains."),
    "wul_szellemtotem": ("Eucalyptus Lookout", "A lookout tree: the hunters above guard the camp with boomerangs."),
    "wul_obszidian_landzsa": ("Flint Spearheads", "+2 damage for melee units."),
    "wul_holdfeny_ijak": ("Ironwood Boomerangs", "+2 damage for ranged units."),
    "wul_bolenybor": ("Kangaroo-hide Armour", "+1 armour for every living unit."),
    "tomahawk": ("Spear Throw", "A spear hurled with a woomera: flies far, damages and slows."),
    "bumerang": ("Boomerang", "A returning boomerang: after the target it strikes two more nearby enemies "
                              "(with falling damage)."),
    "h_kur_taposas": ("Bison Stomp", "The war bison stamps the ground: damages and stuns."),
    "h_tava_madar": ("Call of the Seven Roads", "Ultimate: two Storm Cockatoos arrive for 45 seconds."),
}
FACTION_EN = ("The Alliance of the Seven Roads: a dingo people of river towns, wandering communities and road-songs "
              "on the red plains and in the eucalyptus forests. Their camps can be packed up and pitched elsewhere; "
              "emu herds provide the population, they are stronger as a pack, and every kill brings wood to the tribe.")


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


NEW_ABILITIES = {
    "riado_futas": {"name": "Riadófutás", "type": "self_buff", "hotkey": "F", "cooldown": 25, "duration": 4,
                    "modifiers": {"speed_mult": 1.8}, "icon": "rage",
                    "desc": "Az emu megriad és elszalad: 4 mp-ig +80% sebesség."},
    "felultetes": {"name": "Felültetés", "type": "mount", "hotkey": "U", "range": 3, "into": "wul_bolenylovas",
                   "riders": ["wul_landzsas", "wul_tomahawkos", "wul_ijas_vadasz"], "icon": "merge",
                   "desc": "A legközelebbi wullof gyalogos harcos felül a bölényre: Bölénylovas lesz belőlük."},
    "leszallas": {"name": "Leszállás", "type": "dismount", "hotkey": "U", "mount": "wul_boleny",
                  "rider": "wul_landzsas", "icon": "split",
                  "desc": "A lovas leszáll: újra külön bölény és gyalogos harcos."},
    "savkopes": {"name": "Savköpés", "type": "bolt", "hotkey": "Q", "cooldown": 10, "range": 5, "damage": 8,
                 "poison": [6, 6], "projectile": "acid", "icon": "bolt",
                 "desc": "Maró sav: 8 sebzés, majd 6 mp-en át másodpercenként 6 méregsebzés."},
    "tuzkopes": {"name": "Tűzköpés", "type": "bolt", "hotkey": "Q", "cooldown": 10, "range": 4.5, "damage": 26,
                 "splash": 1.6, "projectile": "fire", "icon": "burst",
                 "desc": "Lángcsóva: 26 sebzés a célpontra, fele a körülötte állókra."},
    "fagykopes": {"name": "Fagyköpés", "type": "bolt", "hotkey": "Q", "cooldown": 12, "range": 5, "damage": 10,
                  "slow": 0, "duration": 2.5, "projectile": "frost", "icon": "bolt",
                  "desc": "Jeges köpet: 10 sebzés, és a célpont 2,5 mp-re megfagy (nem mozog, nem támad)."},
}
SPIT_DESC = "Csak egy köpésfajta választható: ha ezt kifejleszted, a másik kettő többé nem fejleszthető."


def _herds_and_beasts(d, ab):
    """Emucsordák (létszám), kunyhó → alap harcos, barakk → többi gyalogos, bölény + felültetés, Groomi + köpések."""
    U, B, R = d["units"], d["buildings"], d.setdefault("researches", {})
    ab.update(NEW_ABILITIES)
    # --- emuk: csak ők adnak létszámot; Emukarámonként legfeljebb 5
    U["wul_emu"] = {"name": "Emu", "desc": "Tenyészemu: +6 létszám. Emukarámonként legfeljebb 5 tartható.",
                    "cost": {"wood": 60}, "supply": 0, "unit_supply": 6, "time": 15, "hp": 110, "armor": 0,
                    "speed": 3.2, "sight": 7, "radius": 0.4, "shape": "emu", "tags": ["organic", "livestock"],
                    "abilities": ["riado_futas"], "per_building": {"building": "wul_bolenykaram", "max": 5}}
    B["wul_bolenykaram"].update({"name": "Emukarám", "desc": "Emukat tenyészt (karámonként legfeljebb 5). Csak az "
                                 "emuk növelik a létszámot. Felszedhető.", "produces": ["wul_emu"],
                                 "cost": {"wood": 150}, "time": 30, "supply": 0})
    B["wul_bolenykaram"].pop("requires", None)
    d["supply_unit"] = "wul_emu"
    d["supply_building"] = "wul_bolenykaram"
    d["supply_desc"] = "Emuk (Emukarámban tenyészthetők, karámonként legfeljebb 5)"
    d["start_units"] = {"wul_gyujtogeto": 5, "wul_emu": 2}
    B["wul_tanacssator"]["supply"] = 0
    # --- kunyhó: alap harcos; barakk: a többi gyalogos
    B["wul_tipi"].update({"supply": 0, "produces": ["wul_landzsas"],
                          "desc": "Eukaliptuszkéreg-kunyhó: Lándzsás harcosokat képez. Felszedhető."})
    B["wul_harcosok_tuze"].update({"produces": ["wul_tomahawkos", "wul_ijas_vadasz"], "requires": ["wul_tipi"],
                                   "desc": "Dárdavetőket és bumerángvetőket képez. Felszedhető."})
    # --- bölény és felültetés
    U["wul_boleny"] = {"name": "Bölény", "desc": "Erős, de lassú bölény. Felültethető rá egy wullof gyalogos harcos.",
                       "cost": {"wood": 110, "stone": 40}, "supply": 1, "time": 20, "hp": 260, "armor": 2,
                       "speed": 2.7, "sight": 6, "radius": 0.55, "shape": "bison",
                       "attack": {"damage": 9, "range": 0.4, "cooldown": 1.4, "targets": ["ground"]},
                       "tags": ["organic", "beast", "melee"], "abilities": ["felultetes"]}
    U["wul_bolenylovas"].update({"name": "Bölénylovas", "desc": "Bölényen ülő wullof harcos: erős roham. Le is "
                                 "szállhat.", "hp": 320, "armor": 2, "speed": 3.0,
                                 "abilities": ["roham", "leszallas"]})
    B["wul_bolenyakol"] = {"name": "Bölénykarám", "desc": "Bölényeket és termeszvár-óriásokat ad. Felszedhető.",
                           "cost": {"wood": 220, "stone": 90}, "time": 45, "size": [3, 3], "hp": 850, "armor": 1,
                           "sight": 7, "packable": True, "produces": ["wul_boleny", "wul_totemorias"],
                           "requires": ["wul_harcosok_tuze"], "shape": "bisonpen"}
    # --- Groomi: piros szörnymadár, a fészekben választható köpéssel
    U["wul_groomi"] = {"name": "Groomi", "desc": "Piros szörnymadár: gyors, harapós földi vadász. A Groomi-fészekben "
                       "sav-, tűz- vagy fagyköpést tanulhat (csak egyet).",
                       "cost": {"wood": 85, "meteor": 25}, "supply": 2, "time": 22, "hp": 160, "armor": 1,
                       "speed": 3.3, "sight": 7, "radius": 0.45, "shape": "groomi",
                       "attack": {"damage": 13, "range": 0.4, "cooldown": 1.0, "targets": ["ground"]},
                       "tags": ["organic", "beast", "melee"],
                       "abilities": ["roham", "savkopes", "tuzkopes", "fagykopes"],
                       "ability_research": {"savkopes": "wul_savkopes", "tuzkopes": "wul_tuzkopes",
                                            "fagykopes": "wul_fagykopes"}}
    B["wul_groomi_feszek"] = {"name": "Groomi-fészek", "desc": "Groomikat nevel; itt választható ki a köpésük.",
                              "cost": {"wood": 200, "meteor": 60}, "time": 45, "size": [3, 3], "hp": 800,
                              "armor": 1, "sight": 7, "packable": True, "produces": ["wul_groomi"],
                              "researches": ["wul_savkopes", "wul_tuzkopes", "wul_fagykopes"],
                              "requires": ["wul_harcosok_tuze"], "shape": "groominest"}
    for rid, nm, what in (("wul_savkopes", "Savköpés", "maró, mérgező savat"),
                          ("wul_tuzkopes", "Tűzköpés", "lángcsóvát"), ("wul_fagykopes", "Fagyköpés", "jeges köpetet")):
        R[rid] = {"name": nm, "desc": f"A Groomik {what} köphetnek. {SPIT_DESC}", "cost": {"wood": 100, "meteor": 100},
                  "time": 50, "effects": [], "exclusive": "groomi_kopes"}
    # --- gépi ellenfél
    ai = d["ai"]
    ai["build_order"] = ["wul_bolenykaram", "wul_tipi", "wul_harcosok_tuze", "wul_tipi", "wul_osok_kore",
                         "wul_samansator", "wul_groomi_feszek", "wul_bolenyakol", "wul_szellemtotem",
                         "wul_mennydorges_szikla"]
    ai["army"] = {"wul_landzsas": 4, "wul_ijas_vadasz": 3, "wul_holdsaman": 1, "wul_boleny": 2,
                  "wul_mennydorgesmadar": 1, "wul_totemorias": 1, "wul_tomahawkos": 2, "wul_groomi": 2}
    ro = [r for r in ai.get("research_order", []) if r not in ("wul_savkopes", "wul_tuzkopes", "wul_fagykopes")]
    ai["research_order"] = ro[:1] + ["wul_tuzkopes"] + ro[1:]


def main():
    ap = os.path.join(ROOT, "data", "abilities.json")
    ab = rd(ap)
    for k, ch in ABILITIES.items():
        ab.setdefault(k, {}).update(ch)
    wr(ap, ab)
    fp = os.path.join(ROOT, "data", "factions", "wullof.json")
    d = rd(fp)
    U, B, R = d["units"], d["buildings"], d.get("researches", {})
    for k, (n, ds) in UNITS.items():
        if k in U:
            U[k]["name"], U[k]["desc"] = n, ds
    for k, (n, ds) in BUILDINGS.items():
        if k in B:
            B[k]["name"], B[k]["desc"] = n, ds
    for k, (n, ds) in RESEARCH.items():
        if k in R:
            R[k]["name"], R[k]["desc"] = n, ds
    hunter = U.get("wul_ijas_vadasz")
    if hunter:
        hunter["attack"]["projectile"] = "boomerang"
        if "bumerang" not in hunter["abilities"]:
            hunter["abilities"].insert(0, "bumerang")
    for k in ("wul_hos_tava",):  # Tava is bumeránggal harcol
        if k in U and U[k].get("attack", {}).get("projectile"):
            U[k]["attack"]["projectile"] = "boomerang"
    if "wul_szellemtotem" in B and B["wul_szellemtotem"].get("attack"):
        B["wul_szellemtotem"]["attack"]["projectile"] = "boomerang"
    d["desc"] = ("A Hét Út Szövetsége: dingónép folyóvárosokkal, vándorló közösségekkel és útének-dalokkal a vörös "
                 "pusztán és az eukaliptuszerdőkben. A táboraik felszedhetők és máshol letelepíthetők; a létszámot "
                 "az emucsordák adják, falkában erősebbek, és minden zsákmány fát hoz a törzsnek.")
    _herds_and_beasts(d, ab)
    wr(fp, d)
    wr(ap, ab)
    print("wullof: ausztrál nevek, bumeráng, emuk")


if __name__ == "__main__":
    main()
