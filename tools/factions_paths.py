"""Felemelkedés és fajonként 3 fejlődési irány.

    python tools/factions_paths.py      (a factions_v3.py végén automatikusan lefut)

Minden fajnál: amint a főépületen kívül 3 kész épület áll, a nép magától, ingyen „felemelkedik”, és a játékos
egy ablakban végleg választ a 3 irány közül. Az irány saját épületet, egységeket és egy szabályt nyit meg; a többi irány
tartalma zárva marad. A hadjáratban nincs irányzár. Az azonosítók stabilak (újrafuttatható).
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")



def U(name, desc, cost, hp, atk=None, supply=2, time=22, armor=1, speed=2.8, sight=7, radius=0.42, shape="warrior",
      tags=("organic",), abilities=(), **kw):
    d = {"name": name, "desc": desc, "cost": cost, "supply": supply, "time": time, "hp": hp, "armor": armor,
         "speed": speed, "sight": sight, "radius": radius, "shape": shape, "tags": list(tags),
         "abilities": list(abilities)}
    if atk:
        d["attack"] = atk
    d.update(kw)
    return d


def A(dmg, rng=0.4, cd=1.1, targets=("ground",), **kw):
    d = {"damage": dmg, "range": rng, "cooldown": cd, "targets": list(targets)}
    d.update(kw)
    return d


def B(name, desc, cost, produces=(), size=(3, 3), hp=850, shape="barracks", time=45, **kw):
    d = {"name": name, "desc": desc, "cost": cost, "time": time, "size": list(size), "hp": hp, "armor": 1, "sight": 7,
         "produces": list(produces), "shape": shape}
    d.update(kw)
    return d


NEW_ABILITIES = {
    "fiokakeltes": {"name": "Fiókakeltetés", "type": "summon", "hotkey": "W", "cooldown": 35,
                    "unit": "wul_groomi_fioka", "count": 2, "lifetime": 45, "icon": "summon",
                    "desc": "Két Groomi-fióka kel ki a tojásaiból 45 másodpercre."},
    "harci_kurt": {"name": "Harci kürt", "type": "chant", "hotkey": "E", "radius": 6, "icon": "song",
                   "modifiers": {"damage_mult": 1.15, "speed_mult": 1.1},
                   "desc": "Kürtszó (egyszerre egy): a közeli szövetségesek +15% sebzés és +10% sebesség."},
    "medveuvoltes": {"name": "Medveüvöltés", "type": "nova", "hotkey": "R", "cooldown": 16, "radius": 2.6,
                     "damage": 30, "stun": 1.2, "color": [200, 140, 90], "icon": "stomp",
                     "desc": "Rettentő üvöltés: sebez és megbénítja a közeli ellenséget."},
    "robbanas": {"name": "Önrobbantás", "type": "nova", "hotkey": "R", "cooldown": 1, "radius": 2.0, "damage": 70,
                 "self_destruct": True, "color": [150, 255, 120], "icon": "bomb",
                 "desc": "Felrobban: nagy sebzés a közelben – az atka elpusztul."},
    "alomspora": {"name": "Álomspóra", "type": "nova", "hotkey": "R", "cooldown": 20, "radius": 2.8, "damage": 10,
                  "stun": 3.0, "color": [160, 140, 255], "icon": "burst",
                  "desc": "Altató spórafelhő: a közeli ellenség 3 mp-re elalszik."},
    "napfeny_aldas": {"name": "Napfény-áldás", "type": "autoheal", "hotkey": "G", "cooldown": 0.6, "range": 6,
                      "amount": 14, "icon": "heal", "desc": "Automatikusan gyógyítja a közeli sebesült szövetségeseket."},
    "meteorhivas": {"name": "Meteorhívás", "type": "area", "hotkey": "T", "range": 9, "radius": 2.0, "duration": 3,
                    "dps": 35, "cooldown": 22, "color": [255, 150, 60], "sprite": "napvihar", "icon": "burst",
                    "desc": "Meteorzápor: 3 mp alatt 105 sebzés a területen."},
    "lesallas": {"name": "Les", "type": "toggle", "hotkey": "L", "icon": "deploy",
                 "modifiers": {"speed_mult": 0.0, "damage_mult": 1.5, "range_add": 1},
                 "desc": "Lesállás: nem mozog, de +50% sebzés és +1 lőtáv."},
}

# ----------------------------------------------------------------------------- irányok
# fid -> {path_id: {name, tagline, desc, building, units, rules, effects}}
PATHS = {
    "wullof": {
        "boleny": {
            "name": "Bölény-csorda", "tagline": "Kevés, drága, megállíthatatlan roham",
            "desc": "Bölényeket nevelsz, és a harcosaid felülnek rájuk. A lovasság és a vadállatok gyorsabbak és "
                    "erősebbek; a Csordavezér bika épületeket tipor. Középjátékban egyetlen, döntő rohamra épít.",
            "building": "wul_bolenyakol", "units": ["wul_boleny", "wul_bolenylovas", "wul_csordavezer"],
            "effects": [{"stat": "speed_pct", "tags": ["cavalry", "beast"], "add": 15},
                        {"stat": "damage", "tags": ["cavalry"], "add": 3}]},
        "groomi": {
            "name": "Groomi-fészek", "tagline": "Gyors portyázás, vadászat a gyengékre",
            "desc": "Piros szörnymadarak: a Groomik öléskor gyógyulnak és gyorsabbak, a Groomi-anya fiókákat "
                    "keltet. A köpésük (sav, tűz vagy fagy) egyszer, végleg választható. Munkásvadászat, térképuralom.",
            "building": "wul_groomi_feszek", "units": ["wul_groomi", "wul_groomi_fioka", "wul_groomi_anya"],
            "rules": {"heal_on_kill": {"tags": ["beast"], "amount": 35}},
            "effects": [{"stat": "speed_pct", "tags": ["beast"], "add": 10}]},
        "osok": {
            "name": "Ősök útja", "tagline": "Totemlánc, lassú és biztos előrenyomulás",
            "desc": "Totemkertet emelsz: a tornyok erősebbek és messzebbre látnak, az épületek szívósabbak. "
                    "Totemfaragók és Álomjárók támogatják a termeszvár-óriásokat. Védekező, területfoglaló játék.",
            "building": "wul_totemkert", "units": ["wul_totemfarago", "wul_alomjaro", "wul_totemorias"],
            "effects": [{"stat": "damage", "tags": ["tower"], "add": 6}, {"stat": "armor", "tags": ["building"], "add": 2},
                        {"stat": "sight", "tags": ["tower"], "add": 3}]},
    },
    "ursan": {
        "berserker": {
            "name": "Berserker-klán", "tagline": "Korai, nyers közelharci nyomás",
            "desc": "Vérfogadalom: a közelharcosok annál erősebbek, minél sebesültebbek (+60% sebzésig). "
                    "Berserkerek, medvévé változó alakváltók és harci kürtösök.",
            "building": "urs_verfogadalom", "units": ["urs_berserker", "urs_medvealak", "urs_kurtos"],
            "rules": {"rage": {"tags": ["melee"], "max": 0.6}}},
        "runa": {
            "name": "Rúnakovácsok", "tagline": "Erőd, technológia, késői erő",
            "desc": "Rúnákkal vésett falak és tornyok: +3 páncél minden épületnek, +6 toronysebzés. "
                    "Rúnaóriások és ostromszánok törik át a frontot, amikor már minden megvan.",
            "building": "urs_runaszentely", "units": ["urs_runaorias", "urs_ostromszan"],
            "effects": [{"stat": "armor", "tags": ["building"], "add": 3}, {"stat": "damage", "tags": ["tower"], "add": 6}]},
        "tundra": {
            "name": "Tundra hívása", "tagline": "Nagytestű vadak, kevés, de hatalmas egység",
            "desc": "Mamutok, kardfogú lovasok és viharhollók. A vadállatok és a lovasság +60 életerőt és gyorsabb "
                    "regenerációt kapnak. Lassú, nehéz ostrom.",
            "building": "urs_vadak_karama", "extra_buildings": ["urs_sasszirt"],
            "units": ["urs_kardfogu", "urs_mammut", "urs_viharhollo"],
            "effects": [{"stat": "hp", "tags": ["beast", "cavalry"], "add": 60},
                        {"stat": "regen", "tags": ["beast", "cavalry"], "add": 1}]},
    },
    "croftian": {
        "raj": {
            "name": "Raj", "tagline": "Olcsó tömeg, korai elárasztás",
            "desc": "A lárvák 30%-kal gyorsabban kelnek ki, a rajlények gyorsabbak. Robbanó atkák és vadászkarmosok "
                    "öntik el az ellenfelet, mielőtt felkészülne.",
            "building": "cro_rajkamra", "units": ["cro_robbanoatka", "cro_vadaszkarmos"],
            "rules": {"train_time_mult": {"mult": 0.7}},
            "effects": [{"stat": "speed_pct", "tags": ["swarm", "light"], "add": 12}]},
        "pancel": {
            "name": "Páncél", "tagline": "Lassú, szívós menetelés",
            "desc": "+2 páncél minden élő egységnek. A Karmosok páncélbogárrá alakulhatnak; ostromtaréjak és "
                    "alagutat ásó férgek törik fel a védelmet.",
            "building": "cro_kitinkamra", "units": ["cro_pancelbogar", "cro_ostromtaraj", "cro_asoferegg"],
            "effects": [{"stat": "armor", "tags": ["organic"], "add": 2}]},
        "korus": {
            "name": "Kórus", "tagline": "Irányítás és átállítás",
            "desc": "A Kórus hangja: a megölt ellenséges egységek 15%-a a te oldaladon kel fel. Agyfalók és csápos "
                    "horrorok bomlasztják az ellenséges sereget.",
            "building": "cro_agyszentely", "units": ["cro_agyfalo", "cro_csapos_horror"],
            "rules": {"convert_kills": {"chance": 0.15, "hp": 0.35}}},
    },
    "mycel": {
        "rothadas": {
            "name": "Rothadás", "tagline": "Felőrlő, méreggel nyújtott harc",
            "desc": "A közelharcosaid mérgeznek, és az elesett élőkből 25% eséllyel tetemgomba-zombi kel. "
                    "Rothasztók vezetik a lassan terjedő pusztulást.",
            "building": "myc_rothadokert", "units": ["myc_rothaszto", "myc_tetemgomba"],
            "rules": {"poison_attacks": {"tags": ["melee"], "dps": 3, "dur": 4},
                      "raise_from_kills": {"unit": "myc_tetemgomba", "chance": 0.25, "lifetime": 45,
                                           "color": [170, 200, 120]}}},
        "liget": {
            "name": "Óriásliget", "tagline": "Terjeszkedés és területfoglalás",
            "desc": "A hálózat minden összekötött épülete +2 létszámot ad, az épületek +250 életerőt kapnak. "
                    "Járó fagomba-óriások védik a ligetet.",
            "building": "myc_oriasliget", "units": ["myc_fagomba_orias"],
            "rules": {"network_supply_add": 2},
            "effects": [{"stat": "hp", "tags": ["building"], "add": 250}]},
        "alom": {
            "name": "Álom", "tagline": "Csel, rejtőzés, rajtaütés",
            "desc": "Az erdőben minden egységed láthatatlan, amíg nem mozog és nem támad. Álomsámánok és "
                    "Álomszövők altató spórákkal bénítják meg az ellenfelet.",
            "building": "myc_alomkert", "units": ["myc_alomsaman", "myc_alomszovo"],
            "rules": {"forest_stealth": {"tags": ["fungal"]}}},
    },
    "elves": {
        "nap": {
            "name": "Nap temploma", "tagline": "Papokkal támogatott tömeghadsereg",
            "desc": "Minden élő egységed folyamatosan gyógyul (+1 regeneráció). Főpapok gyógyítanak, napszóró papok "
                    "égetnek, és a quetzalok az égből csapnak le.",
            "building": "elf_naptemplom", "extra_buildings": ["elf_tollfeszek"],
            "units": ["elf_fopap", "elf_napszoro", "elf_quetzal"],
            "effects": [{"stat": "regen", "tags": ["organic"], "add": 1}]},
        "jaguar": {
            "name": "Jaguár-harcosok", "tagline": "Vadászat, lesből támadás",
            "desc": "Az erdőben a gyalogosaid láthatatlanok, a közelharcosok +2 sebzést kapnak. Jaguár-orvlövészek, "
                    "raptorlovasok és tollas rexek.",
            "building": "elf_raptor_karam", "units": ["elf_jaguar", "elf_raptor_lovas", "elf_tollas_rex"],
            "rules": {"forest_stealth": {"tags": ["infantry"]}},
            "effects": [{"stat": "damage", "tags": ["melee"], "add": 2}]},
        "csillag": {
            "name": "Csillagvizsgálók", "tagline": "Távolsági ostrom, lassú őrlés",
            "desc": "+1 lőtáv minden távolsági egységnek. Obszidián-vetők és csillagjósok meteorokat hívnak "
                    "messziről.",
            "building": "elf_csillagvizsgalo", "units": ["elf_obszidian_veto", "elf_csillagjos"],
            "effects": [{"stat": "range", "tags": ["ranged", "siege"], "add": 1}]},
    },
    "imperium": {
        "legio": {
            "name": "Légió", "tagline": "Fegyelmezett tömeg alakzatban",
            "desc": "Alakzat: a gyalogosok minden közeli bajtársért +6% sebzést kapnak (max. +30%), és +1 páncélt. "
                    "Praetoriánusok és Centurio-mechák.",
            "building": "imp_praetorium", "units": ["imp_praetorianus", "imp_centurio"],
            "rules": {"pack_bonus": {"radius": 4, "per": 0.06, "max": 0.3, "tags": ["infantry"]}},
            "effects": [{"stat": "armor", "tags": ["infantry"], "add": 1}]},
        "gepisten": {
            "name": "Gépisten", "tagline": "Kevés, hatalmas gép",
            "desc": "Kolosszusok és onager-mechák, +150 életerővel és +4 ostromsebzéssel. Lassú, drága, de "
                    "megállíthatatlan.",
            "building": "imp_ontode", "units": ["imp_onager", "imp_kolosszus"],
            "effects": [{"stat": "hp", "tags": ["imp_onager", "imp_kolosszus"], "add": 150},
                        {"stat": "damage", "tags": ["siege", "imp_kolosszus"], "add": 4}]},
        "lelek": {
            "name": "Lélek", "tagline": "Kifárasztó, élőhalott játék",
            "desc": "A megölt élő ellenségek 20%-a szellemlégiósként kel fel nálad (40 mp-re). A Lélekkútban "
                    "állandó szellemlégiósok és szellemkolosszusok képezhetők.",
            "building": "imp_lelekkut", "units": ["imp_szellemlegios", "imp_szellemkolosszus"],
            "rules": {"raise_from_kills": {"unit": "imp_szellemlegios", "chance": 0.2, "lifetime": 40,
                                           "color": [120, 255, 190]}}},
    },
}

# ----------------------------------------------------------------------------- új egységek / épületek
NEW_UNITS = {
    "wullof": {
        "wul_csordavezer": U("Csordavezér bika", "Hatalmas bölénybika: épületeket tipor, rohamában mindent elsöpör.",
                             {"wood": 220, "stone": 90, "meteor": 40}, 650, A(26, 0.5, 1.4, bonus={"building": 25}),
                             supply=4, time=34, armor=3, speed=2.6, radius=0.75, shape="bison",
                             tags=("organic", "beast", "melee", "large"), abilities=["taposas", "roham"]),
        "wul_groomi_fioka": U("Groomi-fióka", "Kicsi, de fürge és harapós Groomi.", {"wood": 40}, 70, A(6, 0.35, 0.8),
                              supply=1, time=10, armor=0, speed=3.5, radius=0.3, shape="groomi",
                              tags=("organic", "beast", "melee", "light"), abilities=["roham"]),
        "wul_groomi_anya": U("Groomi-anya", "Óriás Groomi-tojó: fiókákat keltet, és keményen harap.",
                             {"wood": 160, "meteor": 60}, 420, A(20, 0.45, 1.2), supply=4, time=32, armor=2,
                             speed=2.7, radius=0.65, shape="groomi", tags=("organic", "beast", "melee", "large"),
                             abilities=["fiokakeltes", "roham"]),
        "wul_totemfarago": U("Totemfaragó", "Ősök-dalát éneklő faragó: a közeli szövetségesek +2 páncélt kapnak.",
                             {"wood": 90, "meteor": 30}, 110, A(7, 0.4, 1.2), supply=2, time=20, armor=0,
                             tags=("caster", "organic"), abilities=["osok_haragja", "erdei_rejtozes"]),
        "wul_alomjaro": U("Álomjáró", "Az ősök álmait járja: gyógyít, és szellemfarkasokat hív.",
                          {"wood": 80, "meteor": 70}, 100, A(9, 5, 1.4, targets=("ground", "air"), projectile="arrow"),
                          supply=2, time=24, armor=0, shape="caster", tags=("caster", "ranged", "organic"),
                          abilities=["holdgyogyitas", "szellemfarkasok"]),
    },
    "ursan": {
        "urs_medvealak": U("Medvealakváltó", "Az ursan teljesen medvévé változik: hatalmas karmok, rettentő üvöltés.",
                           {"wood": 150, "stone": 60, "meteor": 40}, 520, A(24, 0.5, 1.2), supply=4, time=30,
                           armor=2, speed=2.9, radius=0.7, tags=("organic", "melee", "large"),
                           abilities=["medveuvoltes", "verszomj"]),
        "urs_kurtos": U("Harci kürtös", "Kürtszava felszítja a harcosokat: +15% sebzés, +10% sebesség a közelben.",
                        {"wood": 80, "stone": 40}, 160, A(9, 0.4, 1.2), supply=2, time=20,
                        tags=("infantry", "organic"), abilities=["harci_kurt", "beszallas"]),
        "urs_runaorias": U("Rúnaóriás", "Rúnákkal életre keltett kőóriás: lassú, de épületeket zúz.",
                           {"stone": 220, "meteor": 80}, 760, A(30, 0.5, 1.6, bonus={"building": 20}), supply=5,
                           time=38, armor=5, speed=2.0, radius=0.75, shape="golem", tags=("melee", "large", "mech"),
                           abilities=["taposas"]),
    },
    "croftian": {
        "cro_robbanoatka": U("Robbanó atka", "Olcsó, gyors atka: az ellenség közé rohan és felrobban.",
                             {"wood": 30, "stone": 15}, 40, A(4, 0.3, 1.0), supply=1, time=8, armor=0, speed=3.6,
                             radius=0.3, shape="bug", tags=("organic", "melee", "light", "swarm"),
                             abilities=["robbanas"], death_burst={"radius": 1.6, "damage": 35}),
        "cro_vadaszkarmos": U("Vadászkarmos", "Fürge karmos-változat: gyorsan üldöz és szaggat.",
                              {"wood": 45, "stone": 25}, 90, A(9, 0.35, 0.8), supply=1, time=12, armor=0, speed=3.8,
                              radius=0.35, shape="bug", tags=("organic", "melee", "light", "swarm"),
                              abilities=["beasas"]),
        "cro_ostromtaraj": U("Ostromtaréj", "Élő ostromgép: messzire köpi a maró kitintüskéket, épületekre különösen.",
                             {"wood": 120, "stone": 120, "meteor": 40}, 360,
                             A(30, 8, 3.2, projectile="acid", splash=1.2, bonus={"building": 30}), supply=4, time=32,
                             armor=3, speed=2.0, radius=0.7, shape="artillery", tags=("organic", "ranged", "large",
                                                                                        "siege"), abilities=["beasas"]),
        "cro_asoferegg": U("Ásóféreg", "A föld alatt közlekedő féreg: beásva rejtőzik, és alulról támad.",
                           {"wood": 90, "stone": 70}, 260, A(16, 0.4, 1.1), supply=2, time=22, armor=2, speed=3.0,
                           radius=0.5, shape="spitter", tags=("organic", "melee", "armored"), abilities=["beasas"]),
    },
    "mycel": {
        "myc_tetemgomba": U("Tetemgomba", "Az elesettekből kelt, gombával átszőtt test: lassú, de nem adja fel.",
                            {"wood": 50, "stone": 20}, 120, A(9, 0.35, 1.2), supply=1, time=12, armor=0, speed=2.4,
                            tags=("organic", "melee", "fungal"), abilities=["osztodas4"]),
        "myc_fagomba_orias": U("Fagomba-óriás", "Járó, ősi fagomba: a liget védelmezője, gyökereivel megköt.",
                               {"wood": 200, "stone": 100, "meteor": 40}, 700, A(24, 0.5, 1.5), supply=5, time=36,
                               armor=3, speed=2.0, radius=0.75, shape="golem",
                               tags=("organic", "melee", "large", "fungal"), abilities=["gyokerkotes"]),
        "myc_alomszovo": U("Álomszövő", "Altató spórákat szövő gomba-mágus.", {"wood": 90, "meteor": 70}, 110,
                           A(9, 5, 1.4, targets=("ground", "air"), projectile="acid"), supply=2, time=24, armor=0,
                           shape="mushroom_caster", tags=("caster", "ranged", "organic", "fungal"),
                           abilities=["alomspora"]),
    },
    "elves": {
        "elf_fopap": U("Nap főpapja", "A Nap főpapja: folyamatosan gyógyítja a körülötte harcolókat.",
                       {"wood": 80, "stone": 40, "meteor": 60}, 120, A(8, 5, 1.4, targets=("ground", "air"),
                                                                       projectile="arrow"),
                       supply=2, time=24, armor=0, shape="caster", tags=("caster", "ranged", "organic"),
                       abilities=["napfeny_aldas"]),
        "elf_jaguar": U("Jaguár-orvlövész", "Jaguárbőrös vadász: az erdőből lő, lesállásban halálos.",
                        {"wood": 90, "stone": 30}, 100, A(14, 6, 1.4, targets=("ground", "air"), projectile="arrow"),
                        supply=2, time=20, armor=0, speed=3.1, shape="archer", tags=("infantry", "ranged", "organic"),
                        abilities=["lesallas"]),
        "elf_csillagjos": U("Csillagjós", "A csillagvizsgálók mestere: meteorzáport hív le messziről.",
                            {"wood": 90, "stone": 30, "meteor": 90}, 110, A(9, 6, 1.5, targets=("ground", "air"),
                                                                             projectile="arrow"),
                            supply=2, time=26, armor=0, shape="caster", tags=("caster", "ranged", "organic"),
                            abilities=["meteorhivas"]),
    },
    "imperium": {
        "imp_praetorianus": U("Praetoriánus", "Elit gépharcos aranyozott páncélban: a légió gerince.",
                              {"stone": 100, "meteor": 40}, 260, A(16, 0.4, 1.0), supply=3, time=26, armor=4,
                              speed=2.7, radius=0.45, shape="legionary", tags=("infantry", "melee", "mech", "armored"),
                              abilities=["testudo", "lelekhorgony"]),
    },
}

NEW_BUILDINGS = {
    "wullof": {"wul_totemkert": B("Totemkert", "Faragott totemek kertje: Totemfaragókat, Álomjárókat és "
                                  "termeszvár-óriásokat ad.", {"wood": 220, "meteor": 60},
                                  ["wul_totemfarago", "wul_alomjaro", "wul_totemorias"], shape="tech", packable=True)},
    "ursan": {"urs_verfogadalom": B("Vérfogadalom-csarnok", "Itt teszik le a berserkerek a vérfogadalmat.",
                                    {"wood": 220, "stone": 80}, ["urs_berserker", "urs_medvealak", "urs_kurtos"]),
              "urs_runaszentely": B("Rúnaszentély", "Rúnakőkör kovácsműhellyel: rúnaóriások és ostromszánok.",
                                    {"stone": 240, "meteor": 60}, ["urs_runaorias", "urs_ostromszan"], shape="tech")},
    "croftian": {"cro_rajkamra": B("Rajkamra", "Lüktető keltetőkamra: robbanó atkák és vadászkarmosok (a Kaptárban "
                                   "kelnek ki).", {"wood": 150, "stone": 80}, shape="barracks"),
                 "cro_kitinkamra": B("Kitinkamra", "Páncélnövesztő kamra: páncélbogarak, ostromtaréjak, ásóférgek.",
                                     {"wood": 160, "stone": 140}, shape="pen")},
    "mycel": {"myc_rothadokert": B("Rothadókert", "Bomló tetemeken nőtt gombakert: rothasztók és tetemgombák.",
                                   {"wood": 180, "stone": 60}, ["myc_rothaszto", "myc_tetemgomba"], shape="barracks"),
              "myc_oriasliget": B("Óriásliget", "Ősi fagombák ligete: fagomba-óriások.", {"wood": 240, "stone": 100},
                                  ["myc_fagomba_orias"], shape="tech")},
    "elves": {"elf_naptemplom": B("Naptemplom", "Aranyozott lépcsős templom: a Nap főpapjai és napszóró papok.",
                                  {"wood": 160, "stone": 160, "meteor": 40}, ["elf_fopap", "elf_napszoro"],
                                  shape="tech")},
    "imperium": {"imp_praetorium": B("Praetorium", "A légió parancsnoksága: praetoriánusok és Centurio-mechák.",
                                     {"stone": 220, "meteor": 60}, ["imp_praetorianus", "imp_centurio"]),
                 "imp_lelekkut": B("Lélekkút", "Lélekfénnyel izzó kút: állandó szellemlégiósok és szellemkolosszusok.",
                                   {"stone": 180, "meteor": 120}, ["imp_szellemlegios", "imp_szellemkolosszus"],
                                   shape="tech")},
}

# meglévő épületek termelése (az irányba átkerült egységek kikerülnek az alapépületekből)
PRODUCES = {
    "wullof": {"wul_bolenyakol": ["wul_boleny", "wul_csordavezer"], "wul_groomi_feszek": ["wul_groomi", "wul_groomi_anya"]},
    "ursan": {"urs_harcosok_haza": ["urs_pajzshordozo", "urs_dardaor", "urs_szigonyvezo"],
              "urs_runakovacs": ["urs_runapap"]},
    "croftian": {"cro_kaptar": ["cro_csapszolga", "cro_szemhordozo", "cro_karmos", "cro_tuskekopo", "cro_savkopo",
                                "cro_szarnyas_rem", "cro_agyfalo", "cro_csapos_horror", "cro_robbanoatka",
                                "cro_vadaszkarmos", "cro_ostromtaraj", "cro_asoferegg"]},
    "mycel": {"myc_termotest": ["myc_pofeteg", "myc_kalapos_or"]},
    "elves": {"elf_obszidian_muhely": ["elf_csillagdardas"], "elf_csillagvizsgalo": ["elf_obszidian_veto", "elf_csillagjos"]},
    "imperium": {"imp_ontode": ["imp_onager", "imp_kolosszus"]},
}
# Croftian: a Kaptárban kelnek, az irány-épület csak feltétel
REQUIRES = {"cro_robbanoatka": ["cro_rajkamra"], "cro_vadaszkarmos": ["cro_rajkamra"],
            "cro_ostromtaraj": ["cro_kitinkamra"], "cro_asoferegg": ["cro_kitinkamra"],
            "cro_agyfalo": ["cro_agyszentely"], "cro_csapos_horror": ["cro_agyszentely"]}
# meglévő épületek, amelyek egy irányhoz kerülnek, és a feltételeik
BLD_REQ = {"elf_tollfeszek": ["elf_naptemplom"], "imp_hangar": ["imp_legiogyar"], "myc_lebegobolcso": ["myc_termotest"],
           "myc_alomkert": ["myc_gyokerbarlang"], "elf_csillagvizsgalo": ["elf_harcos_csarnok"],
           "elf_raptor_karam": ["elf_harcos_csarnok"], "urs_vadak_karama": ["urs_harcosok_haza"],
           "urs_sasszirt": ["urs_vadak_karama"], "cro_agyszentely": ["cro_ikrafeszek"], "imp_ontode": ["imp_legiogyar"],
           "wul_bolenyakol": ["wul_harcosok_tuze"], "wul_groomi_feszek": ["wul_harcosok_tuze"]}
# trénelhetővé tett, korábban csak idézett egységek
TRAINABLE = {"imp_szellemlegios": ({"stone": 60, "meteor": 30}, 2, 18),
             "imp_szellemkolosszus": ({"stone": 160, "meteor": 120}, 5, 36)}
# a croftian átváltozás (Karmos → gubó → páncélbogár) csak a Páncél-irányban
ABILITY_PATH = {"cro_karmos": {"atvaltozas": "pancel"}}

AI = {
    "wullof": {"pref": ["groomi", "boleny", "osok"],
               "paths": {"boleny": {"build": ["wul_bolenyakol"], "army": {"wul_boleny": 3, "wul_csordavezer": 1}},
                         "groomi": {"build": ["wul_groomi_feszek"], "army": {"wul_groomi": 4, "wul_groomi_anya": 1},
                                    "research": ["wul_tuzkopes"]},
                         "osok": {"build": ["wul_totemkert", "wul_szellemtotem"],
                                  "army": {"wul_totemfarago": 1, "wul_alomjaro": 1, "wul_totemorias": 2}}}},
    "ursan": {"pref": ["berserker", "tundra", "runa"],
              "paths": {"berserker": {"build": ["urs_verfogadalom"],
                                      "army": {"urs_berserker": 4, "urs_medvealak": 1, "urs_kurtos": 1}},
                        "runa": {"build": ["urs_runaszentely", "urs_ortorony"], "army": {"urs_runaorias": 2,
                                                                                         "urs_ostromszan": 1}},
                        "tundra": {"build": ["urs_vadak_karama", "urs_sasszirt"],
                                   "army": {"urs_kardfogu": 2, "urs_mammut": 1, "urs_viharhollo": 1}}}},
    "croftian": {"pref": ["raj", "korus", "pancel"],
                 "paths": {"raj": {"build": ["cro_rajkamra"], "army": {"cro_robbanoatka": 3, "cro_vadaszkarmos": 4}},
                           "pancel": {"build": ["cro_kitinkamra"], "army": {"cro_ostromtaraj": 1, "cro_asoferegg": 2}},
                           "korus": {"build": ["cro_agyszentely"], "army": {"cro_agyfalo": 1, "cro_csapos_horror": 2}}}},
    "mycel": {"pref": ["rothadas", "liget", "alom"],
              "paths": {"rothadas": {"build": ["myc_rothadokert"], "army": {"myc_rothaszto": 3, "myc_tetemgomba": 3}},
                        "liget": {"build": ["myc_oriasliget"], "army": {"myc_fagomba_orias": 2}},
                        "alom": {"build": ["myc_alomkert"], "army": {"myc_alomsaman": 1, "myc_alomszovo": 2}}}},
    "elves": {"pref": ["nap", "csillag", "jaguar"],
              "paths": {"nap": {"build": ["elf_naptemplom", "elf_tollfeszek"],
                                "army": {"elf_fopap": 1, "elf_napszoro": 1, "elf_quetzal": 1}},
                        "jaguar": {"build": ["elf_raptor_karam"],
                                   "army": {"elf_jaguar": 3, "elf_raptor_lovas": 2, "elf_tollas_rex": 1}},
                        "csillag": {"build": ["elf_csillagvizsgalo"], "army": {"elf_obszidian_veto": 2,
                                                                               "elf_csillagjos": 1}}}},
    "imperium": {"pref": ["legio", "gepisten", "lelek"],
                 "paths": {"legio": {"build": ["imp_praetorium"], "army": {"imp_praetorianus": 3, "imp_centurio": 1}},
                           "gepisten": {"build": ["imp_ontode"], "army": {"imp_onager": 1, "imp_kolosszus": 1}},
                           "lelek": {"build": ["imp_lelekkut"], "army": {"imp_szellemlegios": 4,
                                                                         "imp_szellemkolosszus": 1}}}},
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
    ab.update(NEW_ABILITIES)
    wr(ap, ab)
    for fid, paths in PATHS.items():
        fp = os.path.join(FDIR, fid + ".json")
        d = rd(fp)
        Us, Bs, Rs = d["units"], d["buildings"], d.setdefault("researches", {})
        Us.update(NEW_UNITS.get(fid, {}))
        Bs.update(NEW_BUILDINGS.get(fid, {}))
        for bid, prod in PRODUCES.get(fid, {}).items():
            Bs[bid]["produces"] = list(prod)
        for uid, req in REQUIRES.items():
            if uid in Us:
                Us[uid]["requires"] = list(req)
        for bid, req in BLD_REQ.items():
            if bid in Bs:
                Bs[bid]["requires"] = list(req)
        for uid, (cost, sup, t) in TRAINABLE.items():
            if uid in Us:
                Us[uid].update({"cost": cost, "supply": sup, "time": t})
        for uid, m in ABILITY_PATH.items():
            if uid in Us:
                Us[uid]["ability_path"] = m
        out = {}
        for pid, pd in paths.items():
            for bid in [pd["building"]] + pd.get("extra_buildings", []):
                Bs[bid]["path"] = pid
            for uid in pd["units"]:
                Us[uid]["path"] = pid
            out[pid] = {k: v for k, v in pd.items()}
        d["paths"] = out
        d["ascension"] = {"buildings": 3}  # automatikus: 3 kész épület után ingyen, csak választani kell
        rid = fid + "_felemelkedes"  # régebbi változat kutatása – eltávolítjuk
        Rs.pop(rid, None)
        hq = Bs[d["hq"]]
        hq["researches"] = [r for r in hq.get("researches", []) if r != rid]
        ai = d.setdefault("ai", {})
        ai["path_pref"] = AI[fid]["pref"]
        ai["paths"] = AI[fid]["paths"]
        locked = {u for pd in paths.values() for u in pd["units"]}
        ai["army"] = {k: v for k, v in ai.get("army", {}).items() if k not in locked}
        lockedb = {b for pd in paths.values() for b in [pd["building"]] + pd.get("extra_buildings", [])}
        ai["build_order"] = [b for b in ai.get("build_order", []) if b not in lockedb]
        ro = [r for r in ai.get("research_order", []) if r != rid and not r.endswith("kopes")]
        ai["research_order"] = ro
        wr(fp, d)
    print("irányok: 6 faj × 3 fejlődési irány, Felemelkedés")


if __name__ == "__main__":
    main()
