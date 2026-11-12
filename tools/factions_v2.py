"""A fajok 2. változata: egyedi fajmechanikák, új egységek, hősidéző épületek és fajonként 3 hős.

    python tools/factions_v2.py

Idempotens: a data/factions/*.json és a data/abilities.json fájlokat frissíti (a meglévő
azonosítók megmaradnak, így a hadjárat küldetései érvényesek maradnak).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")


def load(fid):
    with open(os.path.join(FDIR, fid + ".json"), encoding="utf-8") as f:
        return json.load(f)


def save(fid, d):
    with open(os.path.join(FDIR, fid + ".json"), "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


def hero(name, desc, hp, armor, speed, dmg, rng, cd, tags, abilities, growth, sight=9, radius=0.45, energy=150,
         proj=None, targets=("ground",), shape="warrior", altar=None, **kw):
    atk = {"damage": dmg, "range": rng, "cooldown": cd, "targets": list(targets)}
    if proj:
        atk["projectile"] = proj
    d = {"name": name, "desc": desc, "hp": hp, "armor": armor, "speed": speed, "sight": sight, "radius": radius,
         "attack": atk, "abilities": [], "tags": ["hero"] + list(tags), "cost": {"food": 200, "wood": 80, "meteor": 40},
         "supply": 5, "time": 45, "hero": True, "shape": shape, "hero_abilities": abilities, "growth": growth,
         "energy": energy, "requires": [altar] if altar else []}
    d.update(kw)
    return d


def altar(name, desc, heroes, shape="altar", **kw):
    d = {"name": name, "desc": desc, "cost": {"wood": 180, "stone": 100}, "time": 45, "size": [3, 3], "hp": 900,
         "armor": 2, "sight": 8, "produces": heroes, "shape": shape, "altar": True}
    d.update(kw)
    return d


def lv(*levels):
    return [dict(x) for x in levels]


# ============================================================================ KÉPESSÉGEK
AB = {
    # --- szürke elfek
    "h_itz_darda": {"name": "Obszidián-dárda", "type": "bolt", "hotkey": "Q", "range": 7, "cooldown": 8, "energy": 50,
                    "slow": 0.5, "duration": 2, "projectile": "spear",
                    "levels": lv({"damage": 80}, {"damage": 125}, {"damage": 170}),
                    "desc": "Hajított obszidián-dárda: nagy sebzés, lassítás."},
    "h_itz_csapas": {"name": "Földrengető csapás", "type": "nova", "hotkey": "W", "cooldown": 9, "energy": 70,
                     "color": [255, 200, 90], "levels": lv({"radius": 2.2, "damage": 45, "stun": 1.0},
                                                          {"radius": 2.5, "damage": 70, "stun": 1.5},
                                                          {"radius": 2.8, "damage": 95, "stun": 2.0}),
                     "desc": "A hadúr a földbe sújt: a körülötte állókat megsebzi és megbénítja."},
    "h_itz_jelenlet": {"name": "A hadúr jelenléte", "type": "aura", "hotkey": "E", "radius": 7,
                       "levels": lv({"modifiers": {"armor_add": 1, "damage_mult": 1.04}},
                                    {"modifiers": {"armor_add": 2, "damage_mult": 1.08}},
                                    {"modifiers": {"armor_add": 3, "damage_mult": 1.12}}),
                       "desc": "A közeli szövetségesek páncélja és sebzése nő."},
    "h_itz_kukulkan": {"name": "Quetzlaru alakja", "type": "self_buff", "hotkey": "R", "ultimate": True, "cooldown": 110,
                       "duration": 15, "modifiers": {"damage_mult": 2.0, "armor_add": 6, "speed_mult": 1.2},
                       "levels": lv({}), "desc": "Végső képesség: a tollas kígyóisten ereje – kétszeres sebzés, +6 páncél."},
    "h_xochi_hullam": {"name": "Rezonanciahullám", "type": "area", "hotkey": "Q", "energy": 75, "range": 8,
                       "radius": 2.0, "duration": 3, "color": [80, 230, 220], "sprite": "napvihar",
                       "levels": lv({"dps": 26}, {"dps": 36}, {"dps": 46}),
                       "desc": "Jádeszínű rezgés tépi szét a területet."},
    "h_xochi_harmonia": {"name": "Harmónia", "type": "aura", "hotkey": "W", "radius": 7,
                         "levels": lv({"modifiers": {"regen_add": 1}}, {"modifiers": {"regen_add": 2}},
                                      {"modifiers": {"regen_add": 3}}),
                         "desc": "A közeli szövetségesek életereje és pajzsa gyorsabban töltődik."},
    "h_xochi_jadeor": {"name": "Jádeőr", "type": "summon", "hotkey": "E", "energy": 80, "cooldown": 25,
                       "unit": "elf_jadeor", "lifetime": 45, "levels": lv({"count": 1}, {"count": 2}, {"count": 3}),
                       "desc": "Élő jádeszobrot ébreszt, amely a hívójáért harcol."},
    "h_xochi_napkorona": {"name": "Napkorona", "type": "area", "hotkey": "R", "ultimate": True, "energy": 150,
                          "cooldown": 110, "range": 8, "radius": 4, "duration": 6, "heal": 32, "color": [255, 230, 140],
                          "sprite": "lelegzet", "levels": lv({}), "desc": "Végső képesség: a területen minden szövetséges gyógyul."},
    # --- ursanok
    "h_hja_szilank": {"name": "Jégszilánk", "type": "bolt", "hotkey": "Q", "range": 7, "cooldown": 7, "energy": 50,
                      "slow": 0.35, "duration": 3, "projectile": "magic",
                      "levels": lv({"damage": 75}, {"damage": 115}, {"damage": 155}),
                      "desc": "Fagyos szilánk: sebez és erősen lassít."},
    "h_hja_vihar": {"name": "Hóvihar", "type": "area", "hotkey": "W", "energy": 80, "range": 8, "radius": 2.4,
                    "duration": 5, "slow": 0.5, "color": [190, 230, 255], "sprite": "hosszutel",
                    "levels": lv({"dps": 16}, {"dps": 24}, {"dps": 32}), "desc": "Jeges vihar csap le a területre."},
    "h_hja_bunda": {"name": "Téli bundák", "type": "aura", "hotkey": "E", "radius": 7,
                    "levels": lv({"modifiers": {"armor_add": 1, "regen_add": 0.5}},
                                 {"modifiers": {"armor_add": 2, "regen_add": 1}},
                                 {"modifiers": {"armor_add": 2, "regen_add": 2}}),
                    "desc": "A közeli szövetségesek páncélja és regenerációja nő."},
    "h_hja_fagyott_ido": {"name": "Megfagyott idő", "type": "nova", "hotkey": "R", "ultimate": True, "energy": 150,
                          "cooldown": 110, "color": [170, 220, 255], "radius": 5, "damage": 120, "stun": 3.5,
                          "levels": lv({}), "desc": "Végső képesség: minden közeli ellenség jégbe dermed."},
    "h_bja_medve": {"name": "Kardfogú hívása", "type": "summon", "hotkey": "Q", "energy": 75, "cooldown": 22,
                    "unit": "urs_kardfogu_tigris", "lifetime": 45, "levels": lv({"count": 1}, {"count": 2}, {"count": 3}),
                    "desc": "Kardfogú tigrist hív a hegyekből."},
    "h_bja_falka": {"name": "A falka ereje", "type": "aura", "hotkey": "W", "radius": 7,
                    "levels": lv({"modifiers": {"damage_mult": 1.08}}, {"modifiers": {"damage_mult": 1.14}},
                                 {"modifiers": {"damage_mult": 1.2, "speed_mult": 1.05}}),
                    "desc": "A közeli szövetségesek (és vadak) sebzése nő."},
    "h_bja_szigony": {"name": "Vadászszigony", "type": "bolt", "hotkey": "E", "range": 6, "cooldown": 9, "energy": 55,
                      "slow": 0.0, "projectile": "spear",
                      "levels": lv({"damage": 60, "duration": 1.2}, {"damage": 95, "duration": 1.7},
                                   {"damage": 130, "duration": 2.2}),
                      "desc": "Szigonyt vet egy ellenségbe: sebzi és rövid időre megbénítja."},
    "h_bja_mamutcsorda": {"name": "Mamutcsorda", "type": "summon", "hotkey": "R", "ultimate": True, "energy": 150,
                          "cooldown": 120, "unit": "urs_mammut", "count": 2, "lifetime": 40, "levels": lv({}),
                          "desc": "Végső képesség: két harci mamut csatlakozik 40 másodpercre."},
    # --- croftianok
    "h_vor_sugar": {"name": "Savsugár", "type": "bolt", "hotkey": "Q", "range": 6, "cooldown": 7, "energy": 50,
                    "slow": 0.5, "duration": 2, "projectile": "acid",
                    "levels": lv({"damage": 80}, {"damage": 120}, {"damage": 160}), "desc": "Maró savsugár."},
    "h_vor_pancel": {"name": "Mutáns páncél", "type": "self_buff", "hotkey": "W", "cooldown": 20,
                     "levels": lv({"duration": 8, "modifiers": {"armor_add": 4, "ranged_taken_mult": 0.7}},
                                  {"duration": 10, "modifiers": {"armor_add": 6, "ranged_taken_mult": 0.6}},
                                  {"duration": 12, "modifiers": {"armor_add": 8, "ranged_taken_mult": 0.5}}),
                     "desc": "Kitinlemezek nőnek a mester testén."},
    "h_vor_larvak": {"name": "Lárvahullám", "type": "summon", "hotkey": "E", "energy": 75, "cooldown": 20,
                     "unit": "cro_karmos", "lifetime": 40, "levels": lv({"count": 2}, {"count": 3}, {"count": 4}),
                     "desc": "Karmosokat költ ki a saját testéből."},
    "h_vor_robbanas": {"name": "Kitinrobbanás", "type": "nova", "hotkey": "R", "ultimate": True, "energy": 150,
                       "cooldown": 110, "color": [150, 255, 120], "radius": 4.5, "damage": 220, "stun": 1.0,
                       "levels": lv({}), "desc": "Végső képesség: kitinszilánkok repesznek szét a mester körül."},
    "h_ssa_felho": {"name": "Spórafelhő", "type": "area", "hotkey": "Q", "energy": 70, "range": 8, "radius": 2.3,
                    "duration": 6, "slow": 0.6, "color": [170, 90, 220], "sprite": "oruletkod",
                    "levels": lv({"dps": 9}, {"dps": 13}, {"dps": 17}), "desc": "Fojtó spórafelhő."},
    "h_ssa_rajtudat": {"name": "Rajtudat", "type": "aura", "hotkey": "W", "radius": 8,
                       "levels": lv({"modifiers": {"speed_mult": 1.08, "damage_mult": 1.05}},
                                    {"modifiers": {"speed_mult": 1.12, "damage_mult": 1.1}},
                                    {"modifiers": {"speed_mult": 1.16, "damage_mult": 1.15}}),
                       "desc": "A raj egy akarattal mozog: gyorsabb és erősebb."},
    "h_ssa_oltes": {"name": "Hús-öltés", "type": "autoheal", "hotkey": "E", "energy": 5, "cooldown": 0.8, "range": 6,
                    "levels": lv({"amount": 12}, {"amount": 18}, {"amount": 24}),
                    "desc": "Automatikusan összefoltozza a közeli sérült élő szövetségeseket."},
    "h_ssa_remszarnyak": {"name": "Rémszárnyak", "type": "summon", "hotkey": "R", "ultimate": True, "energy": 150,
                          "cooldown": 110, "unit": "cro_szarnyas_rem", "count": 4, "lifetime": 45, "levels": lv({}),
                          "desc": "Végső képesség: négy szárnyas rém érkezik."},
    # --- wullof
    "h_var_holdfeny": {"name": "Holdfény", "type": "autoheal", "hotkey": "Q", "energy": 5, "cooldown": 0.8, "range": 6,
                       "levels": lv({"amount": 12}, {"amount": 18}, {"amount": 24}),
                       "desc": "Automatikusan gyógyítja a közeli sérült szövetségeseket."},
    "h_var_kod": {"name": "Ősök köde", "type": "area", "hotkey": "W", "energy": 70, "range": 8, "radius": 2.4,
                  "duration": 6, "slow": 0.5, "color": [200, 220, 255], "sprite": "oruletkod",
                  "levels": lv({"dps": 8}, {"dps": 12}, {"dps": 16}),
                  "desc": "Az ősök szellemei ködként fonják körül az ellenséget."},
    "h_var_falka": {"name": "Szellemfalka", "type": "summon", "hotkey": "E", "energy": 75, "cooldown": 20,
                    "unit": "wul_szellemfarkas", "lifetime": 40, "levels": lv({"count": 2}, {"count": 3}, {"count": 4}),
                    "desc": "Szellem-dingók falkáját hívja."},
    "h_var_telihold": {"name": "Telihold", "type": "area", "hotkey": "R", "ultimate": True, "energy": 150,
                       "cooldown": 110, "range": 8, "radius": 4.5, "duration": 6, "heal": 30, "color": [220, 235, 255],
                       "sprite": "lelegzet", "levels": lv({}), "desc": "Végső képesség: telihold-fény gyógyít a területen."},
    "h_kur_taposas": {"name": "Bölénytaposás", "type": "nova", "hotkey": "Q", "cooldown": 9, "energy": 65,
                      "color": [210, 170, 110], "levels": lv({"radius": 2.2, "damage": 50, "stun": 1.0},
                                                             {"radius": 2.5, "damage": 75, "stun": 1.5},
                                                             {"radius": 2.8, "damage": 100, "stun": 2.0}),
                      "desc": "A harci bölény a földbe dobbant: sebez és megbénít."},
    "h_kur_uvoltes": {"name": "Harci üvöltés", "type": "aura", "hotkey": "W", "radius": 7,
                      "levels": lv({"modifiers": {"damage_mult": 1.08}}, {"modifiers": {"damage_mult": 1.14}},
                                   {"modifiers": {"damage_mult": 1.2}}),
                      "desc": "A közeli szövetségesek sebzése nő."},
    "h_kur_verfalka": {"name": "Vérfalka", "type": "self_buff", "hotkey": "E", "cooldown": 18,
                       "levels": lv({"duration": 6, "modifiers": {"damage_mult": 1.3, "speed_mult": 1.2}},
                                    {"duration": 7, "modifiers": {"damage_mult": 1.45, "speed_mult": 1.25}},
                                    {"duration": 8, "modifiers": {"damage_mult": 1.6, "speed_mult": 1.3}}),
                       "desc": "Vadászláz: rövid ideig gyorsabb és erősebb."},
    "h_kur_het_ut": {"name": "A Hét Út harcosai", "type": "summon", "hotkey": "R", "ultimate": True, "energy": 150,
                     "cooldown": 110, "unit": "wul_landzsas", "count": 5, "lifetime": 45, "levels": lv({}),
                     "desc": "Végső képesség: öt lándzsás harcos érkezik a Hét Útról."},
    # --- gombanép
    "h_gya_halo": {"name": "Gyökérháló", "type": "area", "hotkey": "Q", "energy": 70, "range": 8, "radius": 2.5,
                   "duration": 5, "slow": 0.15, "color": [140, 200, 110], "sprite": "gyokerkotes",
                   "levels": lv({"dps": 6}, {"dps": 9}, {"dps": 12}), "desc": "Gyökerek fonják gúzsba az ellenséget."},
    "h_gya_melygyoker": {"name": "Mélygyökér", "type": "toggle", "hotkey": "W",
                         "levels": lv({"modifiers": {"armor_add": 4, "speed_mult": 0.3, "regen_add": 3}},
                                      {"modifiers": {"armor_add": 6, "speed_mult": 0.3, "regen_add": 5}},
                                      {"modifiers": {"armor_add": 8, "speed_mult": 0.3, "regen_add": 7}}),
                         "desc": "Gyökeret ereszt: lassú, de szinte elpusztíthatatlan."},
    "h_gya_talaj": {"name": "Tápláló talaj", "type": "aura", "hotkey": "E", "radius": 7,
                    "levels": lv({"modifiers": {"regen_add": 1, "armor_add": 1}},
                                 {"modifiers": {"regen_add": 2, "armor_add": 1}},
                                 {"modifiers": {"regen_add": 3, "armor_add": 2}}),
                    "desc": "A közeli szövetségesek regenerációja és páncélja nő."},
    "h_gya_rengés": {"name": "Gyökérrengés", "type": "nova", "hotkey": "R", "ultimate": True, "energy": 150,
                     "cooldown": 110, "color": [150, 120, 80], "radius": 5, "damage": 150, "stun": 2.5,
                     "levels": lv({}), "desc": "Végső képesség: a föld alól feltörő gyökerek mindenkit megbénítanak."},
    "h_spo_tuske": {"name": "Bomlasztó tüske", "type": "bolt", "hotkey": "Q", "range": 7, "cooldown": 7, "energy": 50,
                    "slow": 0.5, "duration": 3, "projectile": "acid",
                    "levels": lv({"damage": 75}, {"damage": 115}, {"damage": 155}), "desc": "Rothasztó spóratüske."},
    "h_spo_rothadas": {"name": "Rothadás", "type": "area", "hotkey": "W", "energy": 75, "range": 8, "radius": 2.0,
                       "duration": 5, "color": [150, 170, 70], "sprite": "oruletkod",
                       "levels": lv({"dps": 22}, {"dps": 32}, {"dps": 42}), "desc": "Minden elrothad a területen."},
    "h_spo_kopeny": {"name": "Spóraköpeny", "type": "self_buff", "hotkey": "E", "cooldown": 18,
                     "levels": lv({"duration": 6, "modifiers": {"speed_mult": 1.3, "ranged_taken_mult": 0.6}},
                              {"duration": 8, "modifiers": {"speed_mult": 1.4, "ranged_taken_mult": 0.5}},
                              {"duration": 10, "modifiers": {"speed_mult": 1.5, "ranged_taken_mult": 0.4}}),
                     "desc": "Spórafelhőbe burkolózik: gyorsabb, és a lövések elvétik."},
    "h_spo_raj": {"name": "Pöfeteg-raj", "type": "summon", "hotkey": "R", "ultimate": True, "energy": 150,
                  "cooldown": 110, "unit": "myc_pofeteg", "count": 6, "lifetime": 45, "levels": lv({}),
                  "desc": "Végső képesség: hat pöfeteg sarjad – robbanó spórazsákok."},
    # --- vasbirodalom
    "h_cas_roham": {"name": "Légió-roham", "type": "nova", "hotkey": "Q", "cooldown": 9, "energy": 65,
                    "color": [255, 190, 120], "levels": lv({"radius": 2.2, "damage": 45, "stun": 1.0},
                                                          {"radius": 2.5, "damage": 70, "stun": 1.5},
                                                          {"radius": 2.8, "damage": 95, "stun": 2.0}),
                    "desc": "A legátus gépteste előrelendül, és szétszórja az ellenséget."},
    "h_cas_fegyelem": {"name": "Vasfegyelem", "type": "aura", "hotkey": "W", "radius": 7,
                       "levels": lv({"modifiers": {"armor_add": 1}}, {"modifiers": {"armor_add": 2}},
                                    {"modifiers": {"armor_add": 3, "damage_mult": 1.05}}),
                       "desc": "A közeli szövetségesek páncélja nő."},
    "h_cas_legio": {"name": "Mechalégió", "type": "summon", "hotkey": "E", "energy": 80, "cooldown": 22,
                    "unit": "imp_legios", "lifetime": 40, "levels": lv({"count": 2}, {"count": 3}, {"count": 4}),
                    "desc": "Légiósokat rendel a frontra."},
    "h_cas_harag": {"name": "Ordan haragja", "type": "self_buff", "hotkey": "R", "ultimate": True, "cooldown": 110,
                    "duration": 15, "modifiers": {"damage_mult": 2.0, "armor_add": 8}, "levels": lv({}),
                    "desc": "Végső képesség: Ordan lelke tölti meg a legátus gépét."},
    # --- egységképességek
    "atvaltozas": {"name": "Mutáció: Páncélbogár", "type": "morph", "hotkey": "M", "into": "cro_pancelbogar",
                   "via": "cro_guboj", "time": 18, "cost": {"food": 75, "stone": 50, "meteor": 25},
                   "desc": "A Karmos gubóba zárkózik, és páncélos bogárrá alakul."},
    "beszallas": {"name": "Helyőrség", "type": "garrison", "hotkey": "G",
                  "desc": "A kijelölt egységek beszállnak a legközelebbi helyőrség-épületbe; ott védve vannak, és "
                          "minden beszállt egység erősíti az épület lövéseit."},
}

# ============================================================================ FAJOK
F = {f: load(f) for f in ("elves", "ursan", "croftian", "wullof", "mycel", "imperium")}

# ---------------------------------------------------------------- szürke elfek: önépítő templomok
e = F["elves"]
e["tagline"] = "Energiamező • pajzs • önmagától épülő templomok"
e["self_build"] = True
e["units"]["elf_csillagdardas"] = {
    "name": "Csillagdárdás", "desc": "Csillagérc-hegyű dárdákat hajít – messzire hord, légi célpontokra is.",
    "cost": {"food": 70, "wood": 30, "meteor": 15}, "supply": 2, "time": 20, "hp": 85, "shield": 30, "armor": 0,
    "speed": 2.7, "sight": 8, "radius": 0.36, "shape": "archer",
    "attack": {"damage": 13, "range": 6, "cooldown": 1.5, "targets": ["ground", "air"], "projectile": "spear",
               "bonus": {"flyer": 8}},
    "requires": ["elf_obszidian_muhely"], "tags": ["infantry", "ranged", "organic"]}
e["units"]["elf_obszidian_veto"] = {
    "name": "Obszidián-vető", "desc": "Kőfaragók ostromgépe: obszidiántömböket hajít, épületek ellen különösen erős.",
    "cost": {"food": 120, "wood": 120, "stone": 80}, "supply": 4, "time": 36, "hp": 260, "shield": 60, "armor": 1,
    "speed": 1.8, "sight": 9, "radius": 0.6, "shape": "artillery",
    "attack": {"damage": 34, "range": 8, "cooldown": 3.0, "targets": ["ground"], "projectile": "boulder", "splash": 1.0,
               "bonus": {"building": 30}},
    "requires": ["elf_obszidian_muhely"], "tags": ["siege", "large", "mech"]}
e["units"]["elf_jadeor"] = {
    "name": "Jádeőr", "desc": "Életre keltett jádeszobor (idézett).", "cost": {}, "supply": 0, "time": 1,
    "hp": 320, "shield": 60, "armor": 3, "speed": 2.2, "sight": 7, "radius": 0.48, "shape": "golem",
    "attack": {"damage": 18, "range": 0.4, "cooldown": 1.3, "targets": ["ground"]}, "no_corpse": True,
    "tags": ["melee", "large", "summoned"]}
e["units"]["elf_hos_itzcoatl"] = hero(
    "Itzcoatl", "A Kilenc Lépcső tollas hadura. Közelharcos, a sereg élén áll.", 520, 4, 2.7, 24, 0.45, 1.15,
    ["melee", "organic"], ["h_itz_darda", "h_itz_csapas", "h_itz_jelenlet", "h_itz_kukulkan"],
    {"hp": 60, "damage": 3.5, "armor": 0.5, "energy": 10}, altar="elf_napkapu", shield=60)
e["units"]["elf_hos_xochi"] = hero(
    "Xochi", "A rezonancia mestere: jádeőröket ébreszt és gyógyít.", 330, 1, 2.7, 15, 6, 1.4,
    ["caster", "ranged", "organic"], ["h_xochi_hullam", "h_xochi_harmonia", "h_xochi_jadeor", "h_xochi_napkorona"],
    {"hp": 32, "damage": 2, "armor": 0.3, "energy": 20}, altar="elf_napkapu", proj="magic", targets=("ground", "air"),
    shield=120, energy=200)
e["buildings"]["elf_napkapu"] = altar("Napkapu", "Hősidéző templomkapu: Szelai, Itzcoatl és Xochi.",
                                      ["elf_hos_szelai", "elf_hos_itzcoatl", "elf_hos_xochi"])
e["buildings"]["elf_obszidian_muhely"]["produces"] = ["elf_csillagdardas", "elf_obszidian_veto"]
e["units"]["elf_hos_szelai"]["requires"] = ["elf_napkapu"]

# ---------------------------------------------------------------- ursanok: helyőrség + tanyák
u = F["ursan"]
u["tagline"] = "Helyőrség • tanyák • javítás • pajzsfal"
u["desc"] = ("Az Északi Eskük Birodalma: esküvel összekötött jarlságok, jurtákkal kevert hosszúcsarnokok, árpaföldes "
             "tanyák. Sisakot nem hordanak, legfeljebb ágyékkötőt; nagy, lapos deszkapajzzsal harcolnak. A csarnokok és "
             "tornyok helyőrséget fogadnak (riadókor a telepesek is behúzódnak), a létszámot a tanyák adják. Pajzsfal és "
             "jégkorszaki harci vadak.")
u["units"]["urs_dardaor"] = {
    "name": "Dárdaőr", "desc": "Hosszú dárdás védő – lovasság és nagy vadak ellen kiváló.",
    "cost": {"food": 55, "wood": 35}, "supply": 2, "time": 18, "hp": 130, "armor": 2, "speed": 2.4, "sight": 7,
    "radius": 0.4, "shape": "warrior",
    "attack": {"damage": 10, "range": 1.0, "cooldown": 1.3, "targets": ["ground"], "bonus": {"cavalry": 14, "large": 10}},
    "tags": ["infantry", "melee", "organic", "armored"], "abilities": ["beszallas"]}
u["units"]["urs_ostromszan"] = {
    "name": "Ostromszán", "desc": "Szánra szerelt kőhajító, épületek ellen.", "cost": {"food": 100, "wood": 150, "stone": 75},
    "supply": 3, "time": 32, "hp": 300, "armor": 2, "speed": 1.8, "sight": 9, "radius": 0.6, "shape": "artillery",
    "attack": {"damage": 40, "range": 8, "cooldown": 3.2, "targets": ["ground"], "projectile": "boulder", "splash": 0.9,
               "bonus": {"building": 40}},
    "requires": ["urs_runakovacs"], "tags": ["siege", "large", "mech"]}
u["units"]["urs_kardfogu_tigris"] = {
    "name": "Kardfogú tigris", "desc": "Bjarka hívására érkező vadállat (idézett).", "cost": {}, "supply": 0, "time": 1,
    "hp": 260, "armor": 1, "speed": 3.4, "sight": 8, "radius": 0.5, "shape": "beast",
    "attack": {"damage": 16, "range": 0.4, "cooldown": 0.9, "targets": ["ground"]}, "tags": ["beast", "melee", "organic"]}
u["units"]["urs_hos_hjalmar"] = hero(
    "Hjalmar Fagyjós", "A jégkorszak emlékét őrző jós: fagy, vihar és lassítás.", 340, 1, 2.6, 15, 6, 1.5,
    ["caster", "ranged", "organic"], ["h_hja_szilank", "h_hja_vihar", "h_hja_bunda", "h_hja_fagyott_ido"],
    {"hp": 34, "damage": 2, "armor": 0.3, "energy": 20}, altar="urs_eskufa", proj="magic", targets=("ground", "air"),
    energy=200)
u["units"]["urs_hos_bjarka"] = hero(
    "Bjarka, a Vadak Ura", "Vadász és vadidomár: kardfogú tigriseket és mamutokat hív harcba.", 430, 2, 3.0, 19, 4, 1.2,
    ["ranged", "organic"], ["h_bja_medve", "h_bja_falka", "h_bja_szigony", "h_bja_mamutcsorda"],
    {"hp": 48, "damage": 3, "armor": 0.4, "energy": 14}, altar="urs_eskufa", proj="spear")
u["buildings"]["urs_eskufa"] = altar("Esküfa", "Ősi kőris, amely alatt a jarlok esküt tesznek: Ragna, Hjalmar és Bjarka.",
                                     ["urs_hos_ragna", "urs_hos_hjalmar", "urs_hos_bjarka"], requires=["urs_harcosok_haza"])
u["buildings"]["urs_tanya"] = {
    "name": "Tanya", "desc": "Árpaföld és ólak: munkás nélkül, magától termel élelmet (+0,6/mp). Ellátmányt is ad.",
    "cost": {"wood": 120, "stone": 30}, "time": 30, "size": [3, 3], "hp": 500, "armor": 0, "sight": 6, "supply": 4,
    "income": {"food": 0.6}, "shape": "farm"}
u["buildings"]["urs_harcosok_haza"]["produces"] = ["urs_pajzshordozo", "urs_dardaor", "urs_szigonyvezo", "urs_berserker"]
u["buildings"]["urs_runakovacs"]["produces"] = ["urs_runapap", "urs_ostromszan"]
u["buildings"]["urs_nagyterem"]["garrison"] = 6
u["buildings"]["urs_ortorony"]["garrison"] = 4
u["buildings"]["urs_harcosok_haza"]["garrison"] = 4
for uid in ("urs_telepes", "urs_pajzshordozo", "urs_szigonyvezo", "urs_berserker", "urs_runapap"):
    abl = u["units"][uid].setdefault("abilities", [])
    if "beszallas" not in abl:
        abl.append("beszallas")
u["units"]["urs_hos_ragna"]["requires"] = ["urs_eskufa"]
u["units"]["urs_hos_ragna"].pop("sprite", None)

# ---------------------------------------------------------------- croftianok: mutáció + fertőzés
c = F["croftian"]
c["tagline"] = "Mélység • lárvák • mutáció • fertőzés"
c["desc"] = ("Az Idegen Kórus: a meteorokkal érkezett spórák megszállták a hangyaszerű kethri népet. A Kaptár lárváiból "
             "minden egység párhuzamosan kel ki, a Karmosok gubóban páncélos bogárrá mutálódnak, és a mélységen elesett "
             "élő ellenségből új Karmos kelhet ki.")
c["infest"] = {"unit": "cro_karmos", "chance": 0.3, "delay": 6}
c["units"]["cro_pancelbogar"] = {
    "name": "Páncélbogár", "desc": "Karmosból mutálódott, páncélos rohamállat. Csak mutációval jön létre.",
    "cost": {"food": 75, "stone": 50, "meteor": 25}, "supply": 2, "time": 18, "hp": 340, "armor": 4, "speed": 2.6,
    "sight": 7, "radius": 0.6, "shape": "beast", "attack": {"damage": 18, "range": 0.4, "cooldown": 1.2,
                                                           "targets": ["ground"], "bonus": {"building": 10}},
    "tags": ["melee", "organic", "large", "armored"]}
c["units"]["cro_guboj"] = {
    "name": "Gubó", "desc": "Mutálódó egység – sérülékeny, de páncélos burok.", "cost": {}, "supply": 0, "time": 1,
    "hp": 200, "armor": 6, "speed": 0, "sight": 4, "radius": 0.45, "shape": "cocoon", "tags": ["organic", "cocoon"],
    "no_corpse": True}
c["units"]["cro_szemhordozo"] = {
    "name": "Szemhordozó", "desc": "Lebegő szemzsák: messzire lát, és a Kórus ellátmányát hordozza (+6).",
    "cost": {"food": 80, "meteor": 20}, "supply": 0, "time": 20, "hp": 160, "armor": 1, "speed": 1.8, "sight": 12,
    "radius": 0.5, "flying": True, "shape": "floater", "unit_supply": 6, "tags": ["flyer", "organic"]}
c["units"]["cro_karmos"].setdefault("abilities", [])
if "atvaltozas" not in c["units"]["cro_karmos"]["abilities"]:
    c["units"]["cro_karmos"]["abilities"].append("atvaltozas")
c["units"]["cro_hos_vor"] = hero(
    "Vor-Uthul", "A Mutáció Mestere: a saját testéből költ lárvákat, kitinpáncélt növeszt.", 480, 3, 2.6, 21, 0.45, 1.2,
    ["melee", "organic"], ["h_vor_sugar", "h_vor_pancel", "h_vor_larvak", "h_vor_robbanas"],
    {"hp": 55, "damage": 3, "armor": 0.5, "energy": 12}, altar="cro_korus_kut", radius=0.5)
c["units"]["cro_hos_ssarha"] = hero(
    "Ssarha, a Raj Hangja", "A Kórus hangja: a rajt egy akaratba fogja, és összeölti a sebeket.", 340, 1, 2.8, 14, 5.5,
    1.4, ["caster", "ranged", "organic"], ["h_ssa_felho", "h_ssa_rajtudat", "h_ssa_oltes", "h_ssa_remszarnyak"],
    {"hp": 34, "damage": 2, "armor": 0.3, "energy": 20}, altar="cro_korus_kut", proj="acid", targets=("ground", "air"),
    energy=200)
c["buildings"]["cro_korus_kut"] = altar("Kórus-kút", "Lüktető hangkút: innen szólnak a Kórus hősei – Keth, Vor-Uthul és Ssarha.",
                                        ["cro_hos_keth", "cro_hos_vor", "cro_hos_ssarha"], shape="altar",
                                        cost={"food": 150, "stone": 100})
c["buildings"]["cro_kaptar"]["produces"] = [x for x in c["buildings"]["cro_kaptar"]["produces"] if not x.startswith("cro_hos")]
if "cro_szemhordozo" not in c["buildings"]["cro_kaptar"]["produces"]:
    c["buildings"]["cro_kaptar"]["produces"].insert(1, "cro_szemhordozo")
c["units"]["cro_hos_keth"]["requires"] = ["cro_korus_kut"]

# ---------------------------------------------------------------- wullof: nomád táborok + falka + vadászat
w = F["wullof"]
w["tagline"] = "Nomád táborok • falkaerő • vadászzsákmány"
w["desc"] = ("A Hét Út Szövetsége: dingónép folyóvárosokkal, vándorló közösségekkel és úténekekkel. A táboraik – a tipik "
             "is – felszedhetők és máshol letelepíthetők; falkában erősebbek, és minden zsákmány fát hoz a törzsnek.")
w["units"]["wul_tomahawkos"] = {
    "name": "Tomahawkos portyázó", "desc": "Gyors portyázó: a munkások és az íjászok réme.",
    "cost": {"food": 60, "wood": 25}, "supply": 2, "time": 16, "hp": 95, "armor": 0, "speed": 3.4, "sight": 8,
    "radius": 0.38, "shape": "warrior",
    "attack": {"damage": 11, "range": 0.4, "cooldown": 1.0, "targets": ["ground"], "bonus": {"worker": 8, "ranged": 4}},
    "tags": ["infantry", "melee", "organic", "light"]}
w["units"]["wul_hos_varnu"] = hero(
    "Varnu, a Holdsámán", "Az ősök szellemeivel beszél: gyógyít, ködöt és szellemfalkát hív.", 330, 1, 2.7, 14, 5.5, 1.4,
    ["caster", "ranged", "organic"], ["h_var_holdfeny", "h_var_kod", "h_var_falka", "h_var_telihold"],
    {"hp": 32, "damage": 2, "armor": 0.3, "energy": 20}, altar="wul_osok_kore", proj="magic", targets=("ground", "air"),
    energy=200)
w["units"]["wul_hos_kurrin"] = hero(
    "Kurrin Harcfőnök", "Harci bölényen lovagló harcfőnök: tapos, üvölt, és a Hét Út harcosait hívja.", 560, 3, 3.2, 23,
    0.5, 1.15, ["melee", "organic", "cavalry"], ["h_kur_taposas", "h_kur_uvoltes", "h_kur_verfalka", "h_kur_het_ut"],
    {"hp": 62, "damage": 3.5, "armor": 0.5, "energy": 10}, altar="wul_osok_kore", radius=0.55, shape="rider")
w["buildings"]["wul_osok_kore"] = altar("Ősök köre", "Faragott oszlopok köre: Tava, Varnu és Kurrin hívása. Felszedhető.",
                                        ["wul_hos_tava", "wul_hos_varnu", "wul_hos_kurrin"], packable=True)
w["buildings"]["wul_harcosok_tuze"]["produces"] = ["wul_landzsas", "wul_tomahawkos", "wul_ijas_vadasz"]
w["buildings"]["wul_tanacssator"]["produces"] = [x for x in w["buildings"]["wul_tanacssator"]["produces"] if "hos" not in x]
w["units"]["wul_hos_tava"]["requires"] = ["wul_osok_kore"]
w["units"]["wul_hos_tava"].pop("sprite", None)

# ---------------------------------------------------------------- gombanép
m = F["mycel"]
m["tagline"] = "Spórafonal-hálózat • szívgombák • összeolvadás"
m["units"]["myc_rothaszto"] = {
    "name": "Rothasztó", "desc": "Bomló gombatest: közelharcban rothaszt, halálakor mérgező spórafelhőt ereget.",
    "cost": {"food": 70, "wood": 30}, "supply": 2, "time": 20, "hp": 150, "armor": 1, "speed": 2.4, "sight": 7,
    "radius": 0.42, "shape": "warrior", "attack": {"damage": 12, "range": 0.4, "cooldown": 1.2, "targets": ["ground"],
                                                    "bonus": {"armored": 6}},
    "death_burst": {"radius": 1.8, "damage": 40}, "tags": ["infantry", "melee", "organic"]}
m["units"]["myc_hos_gyokeranya"] = hero(
    "A Gyökéranya", "Az ősi micélium teste: gyökeret ereszt, gúzsba köt, és megrengeti a földet.", 600, 4, 2.3, 20, 0.5,
    1.3, ["melee", "organic", "large"], ["h_gya_halo", "h_gya_melygyoker", "h_gya_talaj", "h_gya_rengés"],
    {"hp": 70, "damage": 3, "armor": 0.6, "energy": 12}, altar="myc_emlekfa", radius=0.55)
m["units"]["myc_hos_sporin"] = hero(
    "Sporin, a Bomlasztó", "A bomlás vándora: tüskéi rothasztanak, köpenye spórafelhő.", 360, 1, 3.0, 18, 5, 1.2,
    ["ranged", "organic"], ["h_spo_tuske", "h_spo_rothadas", "h_spo_kopeny", "h_spo_raj"],
    {"hp": 38, "damage": 3, "armor": 0.3, "energy": 16}, altar="myc_emlekfa", proj="acid", targets=("ground", "air"))
m["buildings"]["myc_emlekfa"] = altar("Emlékfa", "Az elfeledett nevek fája: Mór, a Gyökéranya és Sporin hívása.",
                                      ["myc_hos_mor", "myc_hos_gyokeranya", "myc_hos_sporin"], cost={"wood": 150, "food": 100})
m["buildings"]["myc_termotest"]["produces"] = ["myc_pofeteg", "myc_rothaszto", "myc_kalapos_or"]
m["buildings"]["myc_szivgomba"]["produces"] = [x for x in m["buildings"]["myc_szivgomba"]["produces"] if "hos" not in x]
m["units"]["myc_hos_mor"]["requires"] = ["myc_emlekfa"]
m["units"]["myc_hos_mor"].pop("sprite", None)

# ---------------------------------------------------------------- vasbirodalom
i = F["imperium"]
i["tagline"] = "Lélekidézés • gépjavítás • ostrom • lélekreaktorok"
i["desc"] = ("Aureth népe: az Égszakadás éterbetegségében haldokló embereket Ordan, a Kéz, amely Megtart, géptestekbe "
             "vezette át. Valódi emberi szellemek acélban – nem születnek új tagjaik, ezért minden nyersanyag számukra "
             "életfeltétel. Létszámukat a lélekreaktorok adják; a Fabrikátorok javítják a gépeket, a szellemidézők az "
             "elesettekből légiót támasztanak.")
i["units"]["imp_onager"] = {
    "name": "Onager-mecha", "desc": "Négylábú hajítógép: messzire hord, épületek és csoportok ellen.",
    "cost": {"food": 110, "stone": 140, "meteor": 40}, "supply": 4, "time": 34, "hp": 320, "armor": 2, "speed": 1.9,
    "sight": 9, "radius": 0.62, "shape": "mech",
    "attack": {"damage": 36, "range": 8.5, "cooldown": 3.0, "targets": ["ground"], "projectile": "boulder", "splash": 1.2,
               "bonus": {"building": 30}},
    "requires": ["imp_ontode"], "tags": ["siege", "large", "mech"]}
i["units"]["imp_hos_cassius"] = hero(
    "Cassius Ferrum legátus", "A Hetedik Légió parancsnoka nehéz géptestben: roham, fegyelem és Ordan haragja.", 600, 5,
    2.4, 24, 0.5, 1.25, ["melee", "mech", "armored", "large"], ["h_cas_roham", "h_cas_fegyelem", "h_cas_legio", "h_cas_harag"],
    {"hp": 65, "damage": 3.5, "armor": 0.6, "energy": 10}, altar="imp_panteon", radius=0.55, shape="mech")
i["units"]["imp_hos_nera"].update({"cost": {"food": 200, "wood": 80, "meteor": 40}, "time": 45, "supply": 5,
                                   "requires": ["imp_panteon"]})
i["buildings"]["imp_panteon"] = altar("Panteon", "A megmentett hősök csarnoka: Livia, Nera és Cassius.",
                                      ["imp_hos_livia", "imp_hos_nera", "imp_hos_cassius"], cost={"stone": 180, "wood": 100})
i["buildings"]["imp_ontode"]["produces"] = ["imp_centurio", "imp_onager", "imp_kolosszus"]
i["buildings"]["imp_forum"]["produces"] = [x for x in i["buildings"]["imp_forum"]["produces"] if "hos" not in x]
i["units"]["imp_hos_livia"]["requires"] = ["imp_panteon"]
i["units"]["imp_hos_livia"].pop("sprite", None)
i["units"]["imp_hos_nera"].pop("sprite", None)

# a hős ne a főépületben legyen; gépi ellenfél: oltár az építési sorrendbe, az új egységek a seregbe
ALT = {"elves": "elf_napkapu", "ursan": "urs_eskufa", "croftian": "cro_korus_kut", "wullof": "wul_osok_kore",
       "mycel": "myc_emlekfa", "imperium": "imp_panteon"}
NEWARMY = {"elves": {"elf_csillagdardas": 2, "elf_obszidian_veto": 1}, "ursan": {"urs_dardaor": 2, "urs_ostromszan": 1},
           "croftian": {}, "wullof": {"wul_tomahawkos": 2}, "mycel": {"myc_rothaszto": 2}, "imperium": {"imp_onager": 1}}
for fid, d in F.items():
    hq = d["buildings"][d["hq"]]
    hq["produces"] = [x for x in hq.get("produces", []) if not d["units"].get(x, {}).get("hero")]
    for uid, ud in d["units"].items():  # a hősök már nem kölcsönzik más egység sprite-ját
        if ud.get("hero"):
            ud.pop("sprite", None)
    ai = d["ai"]
    bo = ai["build_order"]
    if ALT[fid] not in bo:
        bo.insert(min(3, len(bo)), ALT[fid])
    if fid == "ursan" and "urs_tanya" not in bo:
        bo.insert(2, "urs_tanya")
        bo.append("urs_tanya")
    ai["army"].update(NEWARMY[fid])
    save(fid, d)

ap = os.path.join(ROOT, "data", "abilities.json")
with open(ap, encoding="utf-8") as f:
    abil = json.load(f)
abil.update(AB)
with open(ap, "w", encoding="utf-8") as f:
    f.write(json.dumps(abil, ensure_ascii=False, indent=2) + "\n")
print("fajok frissítve:", ", ".join(F))
# a 3 nyersanyagos gazdaság (étel nélkül) és a fajonkénti létszám-mechanikák ráépülnek
import runpy  # noqa: E402
runpy.run_path(os.path.join(ROOT, "tools", "economy_v3.py"))
