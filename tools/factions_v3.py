"""A fajok 3. változata: valóban különböző fajok – minden egységnek legalább egy képessége, és
fajonként más a varázslatok „üzemanyaga”:

* szürke elfek – energia (mana); Fénylépés (teleport), obeliszkes beidézés,
* ursanok – nincs költség, csak újratöltés; rúnakövek, formációk, cölöpfal,
* croftianok – életerő (vérmágia, ``hp_cost``); Beásás (láthatatlan, gyorsan gyógyul),
* wullof – énekek (kapcsolható aurák, egyszerre egy) és újratöltés; erdei rejtőzés,
* micéliák – spóratöltet (``spores``: ölésből és a hálózatból); osztódás X ölés után,
* Vasbirodalom – energia csak a gép-papoknak; lélekhorgony: az elesett gép újraépíthető.

    python tools/factions_v3.py      (a factions_v2 és az economy_v3 után fut)
"""
import json
import os
import runpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


P = "passive"
AB = {
    # ---------------------------------------------------------------- közös passzívok
    "p_halozati_gyujtes": {"name": "Hálózati gyűjtés", "type": P, "icon": "network",
                           "desc": "A terhét bármelyik hálózatba kötött épületbe leadhatja."},
    "p_gepjavitas": {"name": "Gépjavítás és újratestesítés", "type": P, "icon": "repair",
                     "desc": "Jobb klikk sérült gépre: javítás. A lélekhorgonyokat is újraépíti."},
    "lelekhorgony": {"name": "Lélekhorgony", "type": P, "icon": "anchor",
                     "desc": "Ha a gép elpusztul, a lelke horgonyban marad 40 mp-ig – egy Fabrikátor újraépítheti."},
    "osztodas2": {"name": "Osztódás (2 ölés)", "type": P, "icon": "split", "split_kills": 2,
                  "desc": "Minden két legyőzött ellenség után kettéosztódik (mintha megette volna őket)."},
    "osztodas3": {"name": "Osztódás (3 ölés)", "type": P, "icon": "split", "split_kills": 3,
                  "desc": "Minden három legyőzött ellenség után kettéosztódik."},
    "osztodas4": {"name": "Osztódás (4 ölés)", "type": P, "icon": "split", "split_kills": 4,
                  "desc": "Minden négy legyőzött ellenség után kettéosztódik."},
    "erdei_rejtozes": {"name": "Erdei rejtőzés", "type": P, "icon": "forest",
                       "desc": "Fák mellett, álló helyzetben láthatatlan (lesállás), amíg nem támad; a közeli ellenség észreveszi."},
    "korus_szem": {"name": "A Kórus szeme", "type": P, "icon": "eye", "detect": 9,
                   "desc": "Felfedi a közeli rejtőzködő és beásott ellenségeket."},
    "jadetest": {"name": "Jádetest", "type": P, "icon": "shield",
                 "desc": "A varázslatok csak fele sebzést okoznak neki."},
    "szellemtest": {"name": "Szellemtest", "type": P, "icon": "ghost", "desc": "Idézett lény; idővel szertefoszlik."},
    "gubo_p": {"name": "Gubó", "type": P, "icon": "cocoon", "desc": "Mutálódik; ha elpusztul, az átalakulás elvész."},
    "taborkocsi": {"name": "Vándortábor", "type": P, "icon": "tent", "desc": "Felszedett tábor – bárhol letelepíthető."},
    "kristalypancel": {"name": "Kristálypáncél", "type": P, "icon": "shield", "desc": "Kemény kristályokkal benőtt páncél."},
    "falkavadasz": {"name": "Falkavadász", "type": P, "icon": "pack", "desc": "Falkában vadászik."},
    "runako_aura": {"name": "Rúnakő", "type": "aura", "icon": "rune", "radius": 5,
                    "modifiers": {"armor_add": 2, "regen_add": 1}, "desc": "A közeli szövetségesek páncélja és gyógyulása nő."},
    # ---------------------------------------------------------------- elfek (energia)
    "fenylepes": {"name": "Fénylépés", "type": "blink", "hotkey": "B", "energy": 40, "cooldown": 12, "range": 7,
                  "icon": "blink", "color": [120, 230, 255], "desc": "Fénnyé válik, és a kijelölt pontra lép (7 mező)."},
    "fenylepes_munkas": {"name": "Fénylépés", "type": "blink", "hotkey": "B", "energy": 30, "cooldown": 25, "range": 5,
                         "icon": "blink", "color": [120, 230, 255], "desc": "Menekülő teleport (5 mező)."},
    "jade_celzas": {"name": "Jáde-célzás", "type": "self_buff", "hotkey": "C", "energy": 30, "cooldown": 15,
                    "duration": 6, "modifiers": {"range_add": 2, "damage_mult": 1.2}, "icon": "aim",
                    "desc": "6 mp-ig +2 lőtáv és +20% sebzés."},
    "csillagdarda": {"name": "Csillagdárda", "type": "bolt", "hotkey": "D", "energy": 40, "cooldown": 10, "range": 8,
                     "damage": 45, "slow": 0.6, "duration": 1.5, "projectile": "spear", "icon": "spear",
                     "desc": "Célzott, csillagérc-hegyű dárda: 45 sebzés és lassítás."},
    "zuhanorepules": {"name": "Zuhanórepülés", "type": "nova", "hotkey": "Z", "energy": 50, "cooldown": 14,
                      "radius": 1.6, "damage": 40, "stun": 0.7, "color": [120, 230, 200], "icon": "dive",
                      "desc": "Lecsap a földre: a közeli ellenségek sebződnek és megtántorodnak."},
    "tollkorona_uvoltes": {"name": "Tollkorona-üvöltés", "type": "nova", "hotkey": "U", "energy": 60, "cooldown": 16,
                           "radius": 3.0, "damage": 20, "slow": 0.45, "duration": 3, "color": [240, 200, 90],
                           "icon": "roar", "desc": "Rémisztő üvöltés: a közeli ellenségek lelassulnak."},
    "napfokusz": {"name": "Napfókusz", "type": "toggle", "hotkey": "O", "icon": "deploy",
                  "modifiers": {"speed_mult": 0.0, "range_add": 3, "damage_mult": 1.2},
                  "desc": "Telepítés: nem mozog, de +3 lőtáv és +20% sebzés."},
    # ---------------------------------------------------------------- ursanok (újratöltés)
    "dardafal": {"name": "Dárdafal", "type": "toggle", "hotkey": "D", "icon": "spearwall",
                 "modifiers": {"speed_mult": 0.0, "damage_mult": 1.6, "armor_add": 2},
                 "desc": "Leszúrt dárdák: nem mozog, de +60% sebzés és +2 páncél."},
    "horgonyszigony": {"name": "Horgonyszigony", "type": "bolt", "hotkey": "G", "cooldown": 14, "range": 7,
                       "damage": 30, "slow": 0.0, "duration": 1.2, "projectile": "spear", "icon": "harpoon",
                       "desc": "Szigony egy ellenségbe: sebez és 1,2 mp-re megbénít."},
    "verszomj": {"name": "Vérszomj", "type": "self_buff", "hotkey": "V", "cooldown": 20, "duration": 7,
                 "modifiers": {"damage_mult": 1.5, "speed_mult": 1.2, "armor_add": -2}, "icon": "rage",
                 "desc": "7 mp-ig +50% sebzés, +20% sebesség, de -2 páncél."},
    "runako": {"name": "Rúnakő", "type": "summon", "hotkey": "K", "cooldown": 30, "unit": "urs_runako", "count": 1,
               "lifetime": 45, "icon": "rune", "desc": "Rúnakövet állít: a közeli szövetségesek páncélja és gyógyulása nő."},
    "zuhano_csapas": {"name": "Zuhanó csapás", "type": "bolt", "hotkey": "Z", "cooldown": 12, "range": 5, "damage": 35,
                      "slow": 0.5, "duration": 1.5, "icon": "dive", "desc": "A holló lecsap egy célpontra."},
    "taposas": {"name": "Taposás", "type": "nova", "hotkey": "T", "cooldown": 18, "radius": 2.2, "damage": 35,
                "stun": 1.0, "color": [200, 170, 120], "icon": "stomp", "desc": "A mamut a földbe dobbant: sebez és megbénít."},
    "telepites": {"name": "Telepítés", "type": "toggle", "hotkey": "O", "icon": "deploy",
                  "modifiers": {"speed_mult": 0.0, "range_add": 3, "damage_mult": 1.25},
                  "desc": "A szán lehorgonyoz: nem mozog, de +3 lőtáv és +25% sebzés."},
    # ---------------------------------------------------------------- croftianok (életerő)
    "beasas": {"name": "Beásás", "type": "toggle", "hotkey": "Q", "burrow": True, "icon": "burrow",
               "modifiers": {"speed_mult": 0.0, "regen_add": 4},
               "desc": "A föld alá ássa magát: láthatatlan (csak felfedő találja meg), gyorsan gyógyul, de nem támad."},
    "tuskezapor": {"name": "Tüskezápor", "type": "area", "hotkey": "Z", "hp_cost": 20, "cooldown": 12, "range": 7,
                   "radius": 1.6, "duration": 2, "dps": 22, "color": [220, 220, 180], "icon": "spines",
                   "desc": "Csonttüskék záporoznak a területre. Ára: 20 életerő."},
    "savtocsa": {"name": "Savtócsa", "type": "area", "hotkey": "S", "hp_cost": 30, "cooldown": 14, "range": 9,
                 "radius": 2.0, "duration": 6, "dps": 10, "slow": 0.7, "color": [150, 255, 90], "sprite": "oruletkod",
                 "icon": "acid", "desc": "Maró savtócsa: sebez és lassít. Ára: 30 életerő."},
    "ragadozo_zuhanas": {"name": "Ragadozó zuhanás", "type": "bolt", "hotkey": "Z", "hp_cost": 15, "cooldown": 9,
                         "range": 4, "damage": 45, "slow": 0.5, "duration": 1, "icon": "dive",
                         "desc": "Lecsap egy célpontra. Ára: 15 életerő."},
    "csapsopres": {"name": "Csápsöprés", "type": "nova", "hotkey": "C", "hp_cost": 40, "cooldown": 14, "radius": 2.4,
                   "damage": 55, "slow": 0.5, "duration": 2, "color": [170, 90, 160], "icon": "sweep",
                   "desc": "Körbecsapó csápok: nagy sebzés és lassítás. Ára: 40 életerő."},
    "kitinroham": {"name": "Kitinroham", "type": "self_buff", "hotkey": "R", "hp_cost": 20, "cooldown": 14,
                   "duration": 5, "modifiers": {"speed_mult": 1.6, "damage_mult": 1.3}, "icon": "charge",
                   "desc": "5 mp-es roham: +60% sebesség, +30% sebzés. Ára: 20 életerő."},
    # ---------------------------------------------------------------- wullof (énekek, újratöltés)
    "harci_enek": {"name": "Harci ének", "type": "chant", "hotkey": "E", "radius": 5, "icon": "song",
                   "modifiers": {"damage_mult": 1.08}, "desc": "Ének (egyszerre egy): a közeli szövetségesek +8% sebzés."},
    "vandor_enek": {"name": "Vándorének", "type": "chant", "hotkey": "W", "radius": 5, "icon": "song",
                    "modifiers": {"speed_mult": 1.10}, "desc": "Ének (egyszerre egy): a közeli szövetségesek +10% sebesség."},
    "osok_haragja": {"name": "Ősök haragja", "type": "chant", "hotkey": "H", "radius": 6, "icon": "song",
                     "modifiers": {"armor_add": 2}, "desc": "Ének: a közeli szövetségesek +2 páncél."},
    "tomahawk": {"name": "Tomahawkdobás", "type": "bolt", "hotkey": "T", "cooldown": 10, "range": 5, "damage": 30,
                 "slow": 0.6, "duration": 1.5, "projectile": "spear", "icon": "axe",
                 "desc": "Eldobott tomahawk: sebez és lassít."},
    "vadaszles": {"name": "Vadászles", "type": "toggle", "hotkey": "L", "icon": "deploy",
                  "modifiers": {"speed_mult": 0.0, "range_add": 2, "damage_mult": 1.2},
                  "desc": "Lesállás: nem mozog, de +2 lőtáv és +20% sebzés."},
    "villamcsapas": {"name": "Villámcsapás", "type": "nova", "hotkey": "V", "cooldown": 15, "radius": 2.0, "damage": 45,
                     "stun": 0.5, "color": [200, 220, 255], "icon": "lightning",
                     "desc": "A madár villámot sújt maga köré."},
    # ---------------------------------------------------------------- micéliák (spórák)
    "sporatuske": {"name": "Spóratüske", "type": "bolt", "hotkey": "T", "spores": 2, "cooldown": 6, "range": 7,
                   "damage": 45, "slow": 0.6, "duration": 2, "projectile": "acid", "icon": "spines",
                   "desc": "Mérgező tüske: sebez és lassít. Ára: 2 spóra."},
    "kalapfal": {"name": "Kalapfal", "type": "toggle", "hotkey": "K", "icon": "shield",
                 "modifiers": {"speed_mult": 0.5, "armor_add": 4, "ranged_taken_mult": 0.6},
                 "desc": "A kalap pajzzsá feszül: +4 páncél, -40% távolsági sebzés, de lassú."},
    "sporarobbanas": {"name": "Spórarobbanás", "type": "nova", "hotkey": "R", "spores": 3, "cooldown": 12, "radius": 2.6,
                      "damage": 60, "slow": 0.6, "duration": 2, "color": [180, 220, 120], "icon": "burst",
                      "desc": "Mérgező spórafelhő robban ki belőle. Ára: 3 spóra."},
    "sporaeso": {"name": "Spóraeső", "type": "area", "hotkey": "E", "spores": 3, "cooldown": 12, "range": 8,
                 "radius": 2.0, "duration": 4, "dps": 18, "color": [170, 200, 110], "sprite": "oruletkod", "icon": "rain",
                 "desc": "Spóraeső a területre. Ára: 3 spóra."},
    # ---------------------------------------------------------------- Vasbirodalom
    "testudo": {"name": "Testudo", "type": "toggle", "hotkey": "T", "icon": "shield",
                "modifiers": {"speed_mult": 0.6, "ranged_taken_mult": 0.45, "armor_add": 1},
                "desc": "Teknősalakzat: -55% távolsági sebzés, +1 páncél, de lassú."},
    "gyorstoltes_u": {"name": "Gyorstöltés", "type": "self_buff", "hotkey": "G", "cooldown": 18, "duration": 6,
                      "modifiers": {"damage_mult": 1.4}, "icon": "rage", "desc": "6 mp-ig +40% sebzés."},
    "geppuska_sorozat": {"name": "Gépágyú-sorozat", "type": "self_buff", "hotkey": "S", "cooldown": 20, "duration": 5,
                         "modifiers": {"damage_mult": 1.7, "speed_mult": 0.7}, "icon": "rage",
                         "desc": "5 mp-ig +70% sebzés, de lassabb."},
    "zuhanobomba": {"name": "Zuhanóbomba", "type": "nova", "hotkey": "B", "cooldown": 16, "radius": 1.8, "damage": 50,
                    "color": [255, 180, 90], "icon": "bomb", "desc": "Bombát old ki maga alá."},
    "ostrom_telepites": {"name": "Ostrom-telepítés", "type": "toggle", "hotkey": "O", "icon": "deploy",
                         "modifiers": {"speed_mult": 0.0, "range_add": 3, "damage_mult": 1.25},
                         "desc": "Lehorgonyoz: nem mozog, de +3 lőtáv és +25% sebzés."},
    # ---------------------------------------------------------------- vadon
    "eterloket": {"name": "Éterlöket", "type": "bolt", "energy": 40, "cooldown": 8, "range": 6, "damage": 40,
                  "slow": 0.5, "duration": 1.5, "projectile": "magic", "icon": "bolt", "desc": "Éterlövedék."},
    "foldrenges": {"name": "Földrengés", "type": "nova", "cooldown": 14, "radius": 2.2, "damage": 40, "stun": 1.0,
                   "color": [180, 160, 130], "icon": "stomp", "desc": "Megrengeti a földet maga körül."},
}

# egység -> képességek (a meglévők mellé)
ADD = {
    "elf_kofarago": ["fenylepes_munkas"], "elf_tollas_harcos": ["fenylepes"], "elf_jade_ijasz": ["jade_celzas"],
    "elf_csillagdardas": ["csillagdarda"], "elf_quetzal": ["zuhanorepules"], "elf_tollas_rex": ["tollkorona_uvoltes"],
    "elf_obszidian_veto": ["napfokusz"], "elf_jadeor": ["jadetest"], "elf_raptor_lovas": ["fenylepes"],
    "urs_dardaor": ["dardafal"], "urs_szigonyvezo": ["horgonyszigony"], "urs_berserker": ["verszomj"],
    "urs_runapap": ["runako"], "urs_viharhollo": ["zuhano_csapas"], "urs_mammut": ["taposas"],
    "urs_ostromszan": ["telepites"], "urs_kardfogu_tigris": ["szellemtest"],
    "cro_csapszolga": ["beasas"], "cro_karmos": ["beasas"], "cro_tuskekopo": ["tuskezapor", "beasas"],
    "cro_savkopo": ["savtocsa"], "cro_szarnyas_rem": ["ragadozo_zuhanas"], "cro_csapos_horror": ["csapsopres"],
    "cro_pancelbogar": ["kitinroham", "beasas"], "cro_guboj": ["gubo_p"], "cro_szemhordozo": ["korus_szem"],
    "wul_gyujtogeto": ["erdei_rejtozes"], "wul_landzsas": ["harci_enek", "erdei_rejtozes"],
    "wul_tomahawkos": ["tomahawk", "vandor_enek", "erdei_rejtozes"], "wul_ijas_vadasz": ["vadaszles", "erdei_rejtozes"],
    "wul_mennydorgesmadar": ["villamcsapas"], "wul_totemorias": ["osok_haragja"], "wul_szellemfarkas": ["szellemtest"],
    "wul_vandorszeker": ["taborkocsi"], "wul_holdsaman": ["vandor_enek"],
    "myc_sporaszedo": ["p_halozati_gyujtes"], "myc_pofeteg": ["osztodas3"], "myc_tuskegomba_lovesz": ["sporatuske", "osztodas4"],
    "myc_kalapos_or": ["kalapfal", "osztodas4"], "myc_rothaszto": ["osztodas4"], "myc_alomsaman": [],
    "myc_oriaspofeteg": ["sporarobbanas"], "myc_lebego_pofeteg": ["sporaeso"],
    "imp_fabrikator": ["p_gepjavitas"], "imp_legios": ["testudo", "lelekhorgony"],
    "imp_ballistarius": ["gyorstoltes_u", "lelekhorgony"], "imp_centurio": ["geppuska_sorozat", "lelekhorgony"],
    "imp_aquila": ["zuhanobomba", "lelekhorgony"], "imp_kolosszus": ["lelekhorgony"],
    "imp_onager": ["ostrom_telepites", "lelekhorgony"], "imp_szellemlegios": ["szellemtest"],
    "imp_szellemkolosszus": ["szellemtest"],
    "vad_kristalybogar": ["kristalypancel"], "vad_hamufarkas": ["falkavadasz"], "vad_eterliderc": ["eterloket"],
    "vad_kotroll": ["foldrenges"], "vad_ercgolem": ["foldrenges", "kristalypancel"],
}

# meglévő képességek költségmodellje fajonként
COST = {
    "gyogyitas": {"energy": 0},          # ursan: nincs mana
    "roham": {"energy": 0},
    "holdgyogyitas": {"energy": 0, "cooldown": 1.0},
    "szellemfarkasok": {"energy": 0, "cooldown": 30},
    "oruletkod": {"energy": 0, "hp_cost": 25, "cooldown": 14},
    "gyokerkotes": {"energy": 0, "spores": 3, "cooldown": 10},
}


def main():
    ap = os.path.join(ROOT, "data", "abilities.json")
    abil = rd(ap)
    abil.update(AB)
    for k, ch in COST.items():
        if k in abil:
            abil[k].update(ch)
            if not abil[k].get("energy"):
                abil[k].pop("energy", None)
    icons = {"toggle": "shield", "area": "burst", "summon": "summon", "autoheal": "heal", "self_buff": "rage",
             "raise": "ghost", "merge": "merge", "morph": "cocoon", "garrison": "house", "aura": "aura", "bolt": "bolt",
             "nova": "burst"}
    for v in abil.values():
        v.setdefault("icon", icons.get(v.get("type"), "bolt"))
    wr(ap, abil)

    for fn in sorted(os.listdir(FDIR)):
        p = os.path.join(FDIR, fn)
        d = rd(p)
        U, B = d.get("units", {}), d.get("buildings", {})
        fid = d.get("id")
        for uid, extra in ADD.items():
            if uid in U:
                lst = U[uid].setdefault("abilities", [])
                lst[:] = [a for a in lst if not a.startswith("osztodas") or a == "osszeolvadas" or a in extra]
                for a in extra:
                    if a not in lst:
                        lst.append(a)
                if any(abil[a].get("energy") for a in lst):  # energiát csak, akinek kell
                    U[uid]["energy"] = max(U[uid].get("energy", 0), 100)
        if fid == "elves":
            d["warp_in"] = True
            d["spell_resource"] = "energy"
        elif fid == "ursan":
            d["spell_resource"] = "cooldown"
            U["urs_runako"] = {"name": "Rúnakő", "desc": "Rúnapap által állított kő (idézett). Aurája véd és gyógyít.",
                               "cost": {}, "supply": 0, "time": 1, "hp": 220, "armor": 4, "speed": 0, "sight": 5,
                               "radius": 0.4, "shape": "golem", "abilities": ["runako_aura"], "no_corpse": True,
                               "tags": ["structure", "summoned"]}
            B["urs_colopfal"] = {"name": "Cölöpfal", "desc": "Hegyes karókból álló falszakasz – elzárja az utat. "
                                 "Olcsó, gyorsan épül; a telepesek javítják.",
                                 "cost": {"wood": 20}, "time": 8, "size": [1, 1], "hp": 350, "armor": 4, "sight": 3,
                                 "shape": "wall", "requires": [], "wall": True}
            for k, v in U.items():  # ursan egységek: nincs mana
                if not v.get("hero"):
                    v.pop("energy", None)
        elif fid == "croftian":
            d["spell_resource"] = "hp"
            d["detect_units"] = ["cro_szemhordozo"]
        elif fid == "wullof":
            d["spell_resource"] = "song"
            d["bounty"] = 0.2
            d["forest_stealth"] = True
            for k, v in U.items():
                if not v.get("hero"):
                    v.pop("energy", None)
            # balansz (v5: 100% győzelem): olcsó tömeg helyett drágább harcosok és sátrak
            for k, v in (U.items() if not d.get("balance_v6") else ()):  # csak egyszer (a lánc újrafuttatható)
                if v.get("cost") and k != "wul_gyujtogeto" and not v.get("hero"):
                    v["cost"] = {r: int(round(c * 1.18 / 5) * 5) for r, c in v["cost"].items()}
            if "wul_tipi" in B:
                B["wul_tipi"]["cost"] = {"wood": 85}
        elif fid == "mycel":
            d["spell_resource"] = "spores"
            for k, v in U.items():
                if not v.get("hero"):
                    v.pop("energy", None)
            for k in (("myc_tuskegomba_lovesz", "myc_kalapos_or", "myc_oriaspofeteg")  # balansz (v5: 80%)
                      if not d.get("balance_v6") else ()):
                if k in U and U[k].get("cost"):
                    U[k]["cost"] = {r: int(round(c * 1.12 / 5) * 5) for r, c in U[k]["cost"].items()}
        elif fid == "imperium":
            d["spell_resource"] = "energy"
            d["soul_anchor"] = "imp_lelekhorgony"
            U["imp_lelekhorgony"] = {"name": "Lélekhorgony", "desc": "Egy elesett gép lelke. Egy Fabrikátor 40 mp-en "
                                     "belül újraépítheti (javítással) – különben a lélek elillan.",
                                     "cost": {}, "supply": 0, "time": 1, "hp": 400, "armor": 0, "speed": 0, "sight": 4,
                                     "radius": 0.35, "shape": "ghost", "abilities": [], "no_corpse": True,
                                     "tags": ["mech", "anchor"], "lifetime": 40}
            U["imp_lelekhorgony"]["abilities"] = ["lelekhorgony"]
            for k in ("imp_legios", "imp_ballistarius", "imp_centurio", "imp_aquila", "imp_onager", "imp_kolosszus"):
                if k in U:
                    U[k].pop("energy", None)
        # tornyok: felfedők (a beásott / rejtőző egységeket is látják)
        for v in B.values():
            if v.get("attack") and not v.get("detect"):
                v["detect"] = 7
        if fid in ("wullof", "mycel"):
            d["balance_v6"] = True
        wr(p, d)
    # ursan cölöpfal a Telepes építési listáján (a lista a faj "buildings" kulcsa szerint) – megvan
    print("fajok v3: képességek minden egységnek, faji varázslatrendszerek")
    runpy.run_path(os.path.join(ROOT, "tools", "factions_aussie.py"), run_name="__main__")
    runpy.run_path(os.path.join(ROOT, "tools", "factions_paths.py"), run_name="__main__")
    runpy.run_path(os.path.join(ROOT, "tools", "factions_new.py"), run_name="__main__")
    runpy.run_path(os.path.join(ROOT, "tools", "allies.py"), run_name="__main__")
    runpy.run_path(os.path.join(ROOT, "tools", "egysegtipusok.py"), run_name="__apply__")  # Ö24 egységtípusok
    runpy.run_path(os.path.join(ROOT, "tools", "balance_tune.py"), run_name="__main__")


if __name__ == "__main__":
    main()
