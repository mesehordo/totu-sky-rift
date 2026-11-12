"""Szövetséges népek (docs/UJ_FAJOK_TERV.md) és a Diplomácia csarnoka.

    python tools/allies.py      (a factions_v3.py láncában fut, a balance_tune előtt)

Minden szövetséges nép: 3 épület (2 egységet gyárt, 1 fejleszt), 5 egység, 3 fejlesztés, saját képességek.
A nép a data/factions/ally_<id>.json fájlba kerül ("ally": true – nem játszható, a faj-listában nem szerepel).
A főfajok kapnak egy Diplomácia csarnokát: ott választható EGY szövetséges nép, véglegesen (Game.cmd_choose_ally).
A Micéliák a régi micélia-faj sprite-jait használják ("sprite" álnév); a régi faj rejtve marad a hadjárathoz.
Abszolút értékeket ír, többször futtatva is ugyanazt adja.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FDIR = os.path.join(ROOT, "data", "factions")
ABP = os.path.join(ROOT, "data", "abilities.json")

# --- egység-sablonok (szerep szerint)
ROLE = {
    "light": dict(hp=110, armor=1, speed=2.7, sight=7, radius=0.38, supply=1, time=14, cost={"wood": 50, "stone": 15},
                  shape="warrior", attack=dict(damage=9, range=0.3, cooldown=1.0, targets=["ground"]),
                  tags=["infantry", "melee", "organic"]),
    "melee": dict(hp=165, armor=2, speed=2.3, sight=7, radius=0.42, supply=2, time=18, cost={"wood": 65, "stone": 25},
                  shape="shieldbearer", attack=dict(damage=12, range=0.3, cooldown=1.2, targets=["ground"]),
                  tags=["infantry", "melee", "organic", "armored"]),
    "ranged": dict(hp=85, armor=0, speed=2.5, sight=8, radius=0.38, supply=2, time=20, cost={"wood": 70, "stone": 25},
                   shape="archer", attack=dict(damage=12, range=5.5, cooldown=1.5, targets=["ground", "air"],
                                               projectile="arrow"), tags=["infantry", "ranged", "organic"]),
    "caster": dict(hp=80, armor=0, speed=2.4, sight=9, radius=0.37, supply=2, time=26, energy=150,
                   cost={"wood": 70, "stone": 25, "meteor": 50}, shape="caster",
                   attack=dict(damage=6, range=4, cooldown=1.6, targets=["ground", "air"], projectile="magic"),
                   tags=["caster", "ranged", "organic"]),
    "cavalry": dict(hp=300, armor=2, speed=3.0, sight=8, radius=0.52, supply=3, time=26, cost={"wood": 150, "stone": 50},
                    shape="rider", attack=dict(damage=16, range=0.4, cooldown=1.2, targets=["ground"]),
                    tags=["cavalry", "melee", "organic"]),
    "heavy": dict(hp=520, armor=3, speed=1.9, sight=8, radius=0.6, supply=4, time=36,
                  cost={"wood": 180, "stone": 90, "meteor": 40}, shape="golem",
                  attack=dict(damage=26, range=0.5, cooldown=1.8, targets=["ground"]), tags=["large", "melee", "organic"]),
    "flyer": dict(hp=120, armor=0, speed=3.8, sight=10, radius=0.45, supply=2, time=24, flying=True,
                  cost={"wood": 110, "stone": 35, "meteor": 40}, shape="flyer",
                  attack=dict(damage=10, range=3.5, cooldown=1.1, targets=["ground", "air"], projectile="arrow"),
                  tags=["flyer"]),
    "siege": dict(hp=280, armor=2, speed=1.8, sight=9, radius=0.6, supply=3, time=32, cost={"wood": 200, "stone": 100},
                  shape="artillery", attack=dict(damage=38, range=7.5, cooldown=3.2, targets=["ground"], projectile="boulder",
                                                  splash=0.9, bonus={"building": 35}), tags=["siege", "large", "mech"]),
}

# --- képesség-sablonok
def heal(name, amount=10):
    return dict(name=name, type="autoheal", cooldown=0.8, range=5, amount=amount, icon="heal")


def rush(name, speed=1.6, dmg=1.3, cd=20, dur=4, icon="rage"):
    return dict(name=name, type="self_buff", cooldown=cd, duration=dur, modifiers={"speed_mult": speed, "damage_mult": dmg},
                icon=icon)


def guard(name, armor=3, speed=0.6, icon="shield"):
    return dict(name=name, type="toggle", modifiers={"speed_mult": speed, "armor_add": armor}, icon=icon)


def stomp(name, dmg=35, radius=2.2, stun=1.0, cd=18, color=(200, 170, 120), icon="stomp", slow=None):
    d = dict(name=name, type="nova", cooldown=cd, radius=radius, damage=dmg, stun=stun, color=list(color), icon=icon)
    if slow:
        d.update(slow=slow, duration=2)
    return d


def chant(name, mods, radius=5, icon="song"):
    return dict(name=name, type="chant", radius=radius, icon=icon, modifiers=mods)


def storm(name, dps=18, radius=1.8, dur=4, energy=70, color=(255, 190, 60), sprite="napvihar", slow=None, icon="burst"):
    d = dict(name=name, type="area", energy=energy, cooldown=8, range=8, radius=radius, duration=dur, dps=dps,
             color=list(color), sprite=sprite, icon=icon)
    if slow:
        d["slow"] = slow
    return d


def bolt(name, dmg=45, energy=40, cd=10, slow=None, proj="spear", icon="spear"):
    d = dict(name=name, type="bolt", energy=energy, cooldown=cd, range=8, damage=dmg, projectile=proj, icon=icon)
    if slow:
        d.update(slow=slow, duration=1.5)
    return d


def summon(name, unit, count=1, life=40, cd=35, icon="summon"):
    return dict(name=name, type="summon", cooldown=cd, unit=unit, count=count, lifetime=life, icon=icon)


# --- a népek (id, előtag, név, jelmondat, leírás, stílus, színek)
PEOPLES = []


def people(pid, pre, name, tagline, desc, style, color, accent, b_prod1, b_prod2, b_upg, units, researches, ab):
    PEOPLES.append(dict(pid=pid, pre=pre, name=name, tagline=tagline, desc=desc, style=style, color=color, accent=accent,
                        b1=b_prod1, b2=b_prod2, b3=b_upg, units=units, res=researches, ab=ab))


# egység: (id-utótag, név, szerep, leírás, [képességek], {felülírások}, épület 1|2)
people("pinguu", "pin", "Pinguu", "Jég, csont és vastag bőrök",
       "A sarkvidéki pingvinharcosok jégből, csontból és vastag bőrökből építenek menedéket. A hómezőkön kis csapatokban "
       "mozognak, és olyan útvonalakon is átvezetik szövetségeseiket, ahol mások eltévednének.",
       "nordic", (40, 44, 56), (120, 200, 230),
       ("jegcsarnok", "Jégcsarnok", "Jégdárdásokat, hópajzsosokat és rozmárlovasokat képez."),
       ("arapalymuhely", "Árapályműhely", "Ár-apály sámánokat és kajakos portyázókat képez."),
       ("orlangszentely", "Őrláng-szentély", "A pinguu fejlesztések szentélye."),
       [("jegdardas", "Jégdárdás", "ranged", "Jéghegyű dárdát hajít; lassítja a célpontot.", [],
         {"attack": {"projectile": "spear", "damage": 11, "slow": 0.8}}, 1),
        ("hopajzsos", "Hópajzsos", "melee", "Jégpajzsa mögött falat alkot.", ["pin_jegfal"], {"hp": 185}, 1),
        ("rozmarlovas", "Rozmárlovas", "cavalry", "Rozmárháton tör át a hómezőn.", ["pin_csuszas"], {"speed": 2.7, "hp": 340}, 1),
        ("arapaly_saman", "Ár-apály sámán", "caster", "Gyógyít, és jeges hullámot zúdít az ellenségre.",
         ["pin_jeggyogyitas", "pin_jeghullam"], {}, 2),
        ("kajakos", "Kajakos portyázó", "light", "Gyors portyázó; vízen át is utat talál a szövetségeseknek.",
         ["pin_osveny"], {"speed": 3.2, "attack": {"range": 3.5, "projectile": "spear"}, "tags": ["infantry", "ranged", "organic"]}, 2)],
       [("osvenyvezetok", "Ösvényvezetők", "+10% sebesség minden egységnek.", [{"stat": "speed_pct", "tags": ["all"], "add": 10}]),
        ("jegpancel", "Jégpáncél", "+1 páncél a pinguu egységeknek.", [{"stat": "armor", "tags": ["ally_pinguu"], "add": 1}]),
        ("hidegtures", "Hidegtűrés", "+30 életerő a pinguu egységeknek.", [{"stat": "hp", "tags": ["ally_pinguu"], "add": 30}])],
       {"pin_jegfal": guard("Jégfal", 3, 0.5), "pin_csuszas": rush("Hasoncsúszás", 1.9, 1.15, 18),
        "pin_jeggyogyitas": heal("Jeges gyógyítás", 9),
        "pin_jeghullam": storm("Jeges hullám", 14, 2.0, 4, 70, (150, 210, 255), "lelegzet", slow=0.5),
        "pin_osveny": chant("Ösvényvezetés", {"speed_mult": 1.12}, 5, "aura")})

people("ghoblin", "gho", "Ghoblin", "Kőnyűvő manók",
       "A kőnyűvő manók a hegyoldalból fejtik ki falvaik alapjait, és alacsony, meredek tetejű házaikat faragott kővel "
       "erősítik meg. Úgy ismerik a kő repedéseit, ahogy más népek a fa erezetét.",
       "nordic", (110, 120, 104), (200, 60, 50),
       ("ragoko_banya", "Rágókő bánya", "Kőharapókat és sziklagörgetőket képez."),
       ("kocipo_malom", "Kőcipó malom", "Pajzsköveseket, mészköpőket és kőhéjú öregeket képez."),
       ("fogadalomharang", "Fogadalomharang", "A ghoblin fejlesztések harangja."),
       [("koharapo", "Kőharapó", "light", "Csákányos manó; a köveket is megrágja (épületek ellen erős).", [],
         {"attack": {"bonus": {"building": 8}}}, 1),
        ("sziklagorgeto", "Sziklagörgető", "siege", "Óriási sziklát görget a falakra. Felállítás: lövés előtt 2 mp-ig áll fel (utána +1 lőtáv).", [], {"deploy": {"time": 2, "pack": 1, "range_add": 1}}, 1),
        ("pajzskoves", "Pajzsköves", "melee", "Kőlap-pajzzsal és kőbunkóval harcol.", ["gho_kofal"], {"hp": 190, "armor": 3}, 2),
        ("meszkopo", "Mészköpő", "ranged", "Égetett mészgolyót dob; a találat elvakít (lassít).", [],
         {"attack": {"projectile": "acid", "range": 5, "slow": 0.7}}, 2),
        ("koheju_oreg", "Kőhéjú öreg", "heavy", "Kővé keményedett bőrű vén manó.", ["gho_kodobbantas"], {}, 2),
        # Ötletdoboz Ö7: olcsó öngyilkos egység – lassú, de az épületeket rombolja, és a mészköd elvakít
        ("meszbomba", "Mészbombás manó", "light", "Égetett mésszel teli kosarat cipel: az ellenség közé lép, és "
         "felrobban – az épületeket is rombolja, a mészköd pedig elvakít (lassít).", ["gho_meszrobbanas"],
         {"hp": 70, "armor": 0, "speed": 2.0, "supply": 1, "time": 12, "cost": {"wood": 40, "stone": 25},
          "attack": {"damage": 4, "range": 0.3, "cooldown": 1.0, "targets": ["ground"]}}, 2)],
       [("kobor", "Kőbőr", "+1 páncél a ghoblin egységeknek.", [{"stat": "armor", "tags": ["ally_ghoblin"], "add": 1}]),
        ("repedesismeret", "Repedésismeret", "+3 sebzés a ghoblin egységeknek.", [{"stat": "damage", "tags": ["ally_ghoblin"], "add": 3}]),
        ("meszvakitas", "Mészvakítás", "+1 lőtáv a távolsági egységeknek.", [{"stat": "range", "tags": ["ranged"], "add": 1}])],
       {"gho_kofal": guard("Kőfal", 3, 0.5), "gho_kodobbantas": stomp("Kődobbantás", 40, 2.4, 1.2, 18),
        "gho_meszrobbanas": {"name": "Mészrobbanás", "type": "nova", "hotkey": "R", "cooldown": 1, "radius": 1.8,
                             "damage": 40, "building_damage": 120, "slow": 0.5, "duration": 4, "self_destruct": True,
                             "color": [235, 235, 215], "icon": "bomb",
                             "desc": "Felrobban: a közeli ellenségeket megsebzi és elvakítja (lassítja), az "
                                     "épületeket erősen rombolja – a manó elpusztul."}})

people("mhonyo", "mho", "Mhonyo", "Kardkovács manók",
       "A kardkovács manók műhelyeiben a pengék mellett fogaskerekek, emelők és finom szerkezetek is készülnek. Mestereik "
       "egy kard súlypontját éppolyan gondosan állítják be, mint egy gép legapróbb alkatrészét.",
       "imperial", (40, 44, 70), (220, 60, 40),
       ("szelpenge_udvar", "Szélpenge udvar", "Hajnalpengéket és szerkezet-íjászokat képez."),
       ("parazsszentely", "Parázsszentély", "Szikraőröket, kardkovácsokat és fogaskerék-gólemeket képez."),
       ("harompenge_kovacsda", "Hárompenge kovácsda", "A mhonyo fejlesztések kovácsműhelye."),
       [("hajnalpenge", "Hajnalpenge", "light", "Villámgyors párbajozó; kétszer csap.", ["mho_szelvagas"],
         {"attack": {"damage": 8, "cooldown": 0.7}, "speed": 3.0}, 1),
        ("szerkezet_ijasz", "Szerkezet-íjász", "ranged", "Felhúzós számszeríj; hosszú lőtáv.", [],
         {"attack": {"range": 6.5, "damage": 14, "cooldown": 1.8}}, 1),
        ("szikraor", "Szikraőr", "melee", "Izzó pengéjű, nehézpajzsos őr.", ["mho_szikrapajzs"], {"hp": 180}, 2),
        ("kardkovacs", "Kardkovács", "caster", "Javítja és élezi a társak fegyverét.", ["mho_elezes", "mho_javitas"],
         {"attack": {"range": 0.3, "damage": 10, "projectile": None}}, 2),
        ("fogaskerek_golem", "Fogaskerék-gólem", "heavy", "Kovácsolt gép-óriás.", ["mho_gozkitores"],
         {"tags": ["large", "melee", "mech"]}, 2)],
       [("kiegyensulyozott_penge", "Kiegyensúlyozott penge", "+2 sebzés a közelharcosoknak.",
         [{"stat": "damage", "tags": ["melee"], "add": 2}]),
        ("szikrapancel", "Szikrapáncél", "+1 páncél a mhonyo egységeknek.", [{"stat": "armor", "tags": ["ally_mhonyo"], "add": 1}]),
        ("gepjavitas", "Gépjavítás", "+60 életerő a gépeknek.", [{"stat": "hp", "tags": ["mech"], "add": 60}])],
       {"mho_szelvagas": rush("Szélvágás", 1.5, 1.4, 16), "mho_szikrapajzs": guard("Szikrapajzs", 3, 0.6),
        "mho_elezes": chant("Élezés", {"damage_mult": 1.1}, 5), "mho_javitas": heal("Foltozás", 8),
        "mho_gozkitores": stomp("Gőzkitörés", 38, 2.3, 0.8, 16, (230, 230, 230))})

people("daresh", "dar", "Daresh", "A darázsnép",
       "A darázsnép napmeleg vidékeken emel kaptárakat, amelyeket fonott díszek, festett minták és okkerszínű falak tesznek "
       "felismerhetővé. Szárnyas őreik magasból figyelik a környéket, és gyors rajokban csapnak le.",
       "tribal", (190, 120, 40), (40, 110, 110),
       ("agyagkaptar", "Agyagkaptár", "Fullánclándzsásokat és gyantapajzsosokat képez."),
       ("szarnyudvar", "Szárnyudvar", "Rajénekeseket, darázsrajt és szárnyas íjászokat képez."),
       ("gyantamuhely", "Gyantaműhely", "A daresh fejlesztések műhelye."),
       [("fullanclandzsas", "Fullánclándzsás", "light", "Mérges fullánkú lándzsával döf.", [],
         {"attack": {"poison": [3, 4], "range": 0.6}}, 1),
        ("gyantapajzsos", "Gyantapajzsos", "melee", "Borostyán-gyantapajzzsal véd.", ["dar_gyantafal"], {"hp": 185}, 1),
        ("rajenekes", "Rajénekes", "caster", "Énekével gyorsítja a rajt, és gyógyít.", ["dar_rajenek", "dar_mezgyogyitas"], {}, 2),
        ("darazsraj", "Darázsraj", "flyer", "Repülő darázsraj; gyorsan csap le.", [], {"attack": {"range": 1.5, "damage": 8,
                                                                                                   "cooldown": 0.7}}, 2),
        ("szarnyas_ijasz", "Szárnyas íjász", "flyer", "A magasból nyilaz.", [], {"attack": {"range": 5, "damage": 12}}, 2)],
       [("gyantapancel", "Gyantapáncél", "+1 páncél a daresh egységeknek.", [{"stat": "armor", "tags": ["ally_daresh"], "add": 1}]),
        ("meregfullank", "Méregfullánk", "+2 sebzés a daresh egységeknek.", [{"stat": "damage", "tags": ["ally_daresh"], "add": 2}]),
        ("rajzas", "Rajzás", "+15% sebesség a repülőknek.", [{"stat": "speed_pct", "tags": ["flyer"], "add": 15}])],
       {"dar_gyantafal": guard("Gyantafal", 3, 0.6), "dar_rajenek": chant("Rajének", {"speed_mult": 1.12, "damage_mult": 1.05}),
        "dar_mezgyogyitas": heal("Mézgyógyítás", 9)})

people("tokla", "tok", "Tokla", "Fagyóriások és fagytörpék",
       "A fagyóriások és fagytörpék közös népét messziről felismerni faragott, busószerű maszkjairól. A törpék műhelyei a hegy "
       "gyomrában húzódnak, az óriások pedig a havas hágókat és a telepek kapuit őrzik.",
       "nordic", (226, 218, 200), (180, 40, 40),
       ("maszkcsur", "Maszkcsűr", "Fagyóriásokat, harangjárókat és hóvihar-busókat képez."),
       ("harangkapu", "Harangkapu", "Zúzmaratörpéket és jégvető törpéket képez."),
       ("zuzmaramuhely", "Zúzmaraműhely", "A tokla fejlesztések műhelye."),
       [("fagyorias", "Fagyóriás", "heavy", "Jégbalták óriása; csapásai lelassítanak.", ["tok_fagydobbantas"],
         {"attack": {"slow": 0.7}}, 1),
        ("harangjaro", "Harangjáró", "caster", "Harangszóval bátorít, és elűzi a gonoszt.", ["tok_harangszo", "tok_rontasuzes"], {}, 1),
        ("hovihar_buso", "Hóvihar-busó", "caster", "Hóvihart kavar az ellenségre.", ["tok_hovihar"], {}, 1),
        ("zuzmaraturpe", "Zúzmaratörpe", "melee", "Jégbaltás, kerek pajzsos törpe.", [], {"radius": 0.36, "speed": 2.4}, 2),
        ("jegveto_turpe", "Jégvető törpe", "ranged", "Jégtömböket hajít.", [], {"attack": {"projectile": "boulder", "slow": 0.8}}, 2)],
       [("busomaszk", "Busómaszk", "+2 sebzés a tokla egységeknek.", [{"stat": "damage", "tags": ["ally_tokla"], "add": 2}]),
        ("fagyos_fegyver", "Fagyos fegyver", "+1 páncél a tokla egységeknek.", [{"stat": "armor", "tags": ["ally_tokla"], "add": 1}]),
        ("hagoorseg", "Hágóőrség", "+60 életerő a nagy testű egységeknek.", [{"stat": "hp", "tags": ["large"], "add": 60}])],
       {"tok_fagydobbantas": stomp("Fagydobbantás", 36, 2.4, 1.0, 18, (180, 220, 255), slow=0.5),
        "tok_harangszo": chant("Harangszó", {"damage_mult": 1.08, "armor_add": 1}),
        "tok_rontasuzes": heal("Rontásűzés", 8),
        "tok_hovihar": storm("Hóvihar", 14, 2.2, 5, 75, (200, 230, 255), "lelegzet", slow=0.4)})

people("galoca", "gal", "Galóca", "Tüskés kalapú gombaharcosok",
       "A Galócák tüskés kalapú, fogas gombaharcosok, akik a sűrű növényzetből rontanak a betolakodókra. Telepeik peremén már a "
       "földből előbújó tüskék is jelzik, meddig tart a türelmük.",
       "fungal", (190, 40, 40), (240, 220, 120),
       ("harapofeszek", "Harapófészek", "Fogaskalapokat, tüskebunkósokat és óriásgalócákat képez."),
       ("sporakurt", "Spórakürt", "Spóraköpőket és gyökérgáncsolókat képez."),
       ("tuskekert", "Tüskekert", "A galóca fejlesztések kertje; tüskéi sebzik a közelébe érő ellenséget."),
       [("fogaskalap", "Fogaskalap", "light", "Fogas kalapú, mohó gombaharcos.", ["gal_harapas"], {"hp": 120}, 1),
        ("tuskebunkos", "Tüskebunkós", "melee", "Két tüskés bunkóval csapkod.", [], {"attack": {"damage": 14}}, 1),
        ("oriasgaloca", "Óriásgalóca", "heavy", "Hatalmas, fogas gombaóriás.", ["gal_kalaptaposas"], {}, 1),
        ("sporakopo", "Spóraköpő", "ranged", "Mérges spórafelhőt köp.", [], {"attack": {"projectile": "acid", "poison": [3, 4]}}, 2),
        ("gyokergancsolo", "Gyökérgáncsoló", "caster", "Gyökerekkel köti meg az ellenséget.", ["gal_gyokergancs"], {}, 2)],
       [("tuskes_talaj", "Tüskés talaj", "+1 páncél a galóca egységeknek.", [{"stat": "armor", "tags": ["ally_galoca"], "add": 1}]),
        ("merges_spora", "Mérges spóra", "+2 sebzés a galóca egységeknek.", [{"stat": "damage", "tags": ["ally_galoca"], "add": 2}]),
        ("gyors_burjanzas", "Gyors burjánzás", "+1 életerő-visszatöltés másodpercenként.", [{"stat": "regen", "tags": ["ally_galoca"], "add": 1}])],
       {"gal_harapas": rush("Lesből harapás", 1.6, 1.35, 18), "gal_kalaptaposas": stomp("Kalaptaposás", 40, 2.3, 1.0, 18, (200, 60, 50)),
        "gal_gyokergancs": storm("Gyökérgáncs", 6, 2.0, 4, 60, (120, 200, 120), "gyokerkotes", slow=0.1)})

people("niil_elf", "nii", "Niil-elfek", "A folyók elfjei",
       "A vízi elfek oszlopos szentélyei és faragott kőkapui a folyók és csatornák rendjéhez igazodnak. A víz számukra útvonal, "
       "településeik tengelye és szertartásaik központja.",
       "aztec", (40, 150, 140), (230, 190, 70),
       ("lotuszkikoto", "Lótuszkikötő", "Papiruszlándzsásokat és nádhajós íjászokat képez."),
       ("arvizszentely", "Árvízszentély", "Lótuszpajzsosokat, árapálypapokat és krokodilőröket képez."),
       ("nadpalota", "Nádpalota", "A niil-elf fejlesztések palotája."),
       [("papiruszlandzsas", "Papiruszlándzsás", "light", "Nádpajzsos, hosszú lándzsás harcos.", [], {"attack": {"range": 0.7}}, 1),
        ("nadhajos_ijasz", "Nádhajós íjász", "ranged", "Gyorsan mozgó, nádcsónakos íjász.", [], {"speed": 2.9}, 1),
        ("lotuszpajzsos", "Lótuszpajzsos", "melee", "Aranylótusz-pajzzsal véd.", ["nii_lotuszfal"], {"hp": 180}, 2),
        ("arapalypap", "Árapálypap", "caster", "Gyógyít, és árhullámot zúdít.", ["nii_aldas", "nii_arhullam"], {}, 2),
        ("krokodilor", "Krokodilőr", "heavy", "Megszelídített óriáskrokodil.", ["nii_harapas"], {"speed": 2.1}, 2)],
       [("csatornarend", "Csatornarend", "+10% sebesség minden egységnek.", [{"stat": "speed_pct", "tags": ["all"], "add": 10}]),
        ("lotuszaldas", "Lótuszáldás", "+40 életerő a niil-elf egységeknek.", [{"stat": "hp", "tags": ["ally_niil_elf"], "add": 40}]),
        ("aradas", "Áradás", "+2 sebzés a niil-elf egységeknek.", [{"stat": "damage", "tags": ["ally_niil_elf"], "add": 2}])],
       {"nii_lotuszfal": guard("Lótuszfal", 3, 0.6), "nii_aldas": heal("Árapály-áldás", 10),
        "nii_arhullam": storm("Árhullám", 16, 2.2, 3, 70, (110, 200, 230), "lelegzet", slow=0.5),
        "nii_harapas": stomp("Halálforgás", 42, 1.6, 1.2, 16, (90, 140, 90))})

people("tuz_elf", "tuz", "Tűz-elfek", "A dűnék elfjei",
       "A dűnék elfjei vörös tetők alatt, árnyékos udvarok köré építik telepeiket. Ismerik a homokviharok járását, és a tűz "
       "mágiáját éppúgy használják a műhelyeikben, mint a harcban.",
       "imperial", (170, 60, 40), (240, 170, 60),
       ("dunepavilon", "Dűnepavilon", "Homokszablyásokat, parázsíjászokat és homokvihar-táncosokat képez."),
       ("delibabtorony", "Délibábtorony", "Hőszövőket és délibáb-csalókat képez."),
       ("parazskemence", "Parázskemence", "A tűz-elf fejlesztések kemencéje."),
       [("homokszablyas", "Homokszablyás", "light", "Ívelt szablyával vág.", ["tuz_homokroham"], {"attack": {"damage": 10}}, 1),
        ("parazsijasz", "Parázsíjász", "ranged", "Izzó hegyű nyilakat lő.", [], {"attack": {"damage": 13}}, 1),
        ("homokviharTancos", "Homokvihar-táncos", "light", "Pörgő pengés táncos; körben sebez.", ["tuz_pengetanc"], {"speed": 3.0}, 1),
        ("hoszovo", "Hőszövő", "caster", "Tűzfalat sző az ellenségre.", ["tuz_tuzfal"], {}, 2),
        ("delibab_csalo", "Délibáb-csaló", "caster", "Délibáb-másokat idéz, hogy megtévessze az ellent.", ["tuz_delibab"], {}, 2)],
       [("izzo_nyilhegy", "Izzó nyílhegy", "+2 sebzés a távolsági egységeknek.", [{"stat": "damage", "tags": ["ranged"], "add": 2}]),
        ("homokvihar", "Homokvihar", "+15% sebesség a tűz-elf egységeknek.", [{"stat": "speed_pct", "tags": ["ally_tuz_elf"], "add": 15}]),
        ("tuzallo_pancel", "Tűzálló páncél", "+1 páncél a tűz-elf egységeknek.", [{"stat": "armor", "tags": ["ally_tuz_elf"], "add": 1}])],
       {"tuz_homokroham": rush("Homokroham", 1.7, 1.25, 18), "tuz_pengetanc": stomp("Pengetánc", 30, 1.8, 0, 12, (240, 200, 120), "sweep"),
        "tuz_tuzfal": storm("Tűzfal", 24, 1.6, 4, 75, (255, 140, 40), "napvihar"),
        "tuz_delibab": summon("Délibáb", "tuz_delibab_mas", 2, 20, 30)})

people("tenduur", "ten", "Tendüür", "Szárnyas erdei tündérek",
       "A szárnyas erdei tündérek élő fák ágai között laknak, ösvényeiket függő lámpások jelzik. Harcosaik testén tetovált minták "
       "futnak, és a lombkoronán át közelítik meg azokat, akik veszélyeztetik a ligeteiket.",
       "tribal", (70, 110, 50), (240, 200, 90),
       ("lampasfa", "Lámpásfa", "Lámpásőröket és szélhímzőket képez."),
       ("tovisudvar", "Tövisudvar", "Tövisvívókat, lombíjászokat és tölgyőröket képez."),
       ("fenyfeszek", "Fényfészek", "A tendüür fejlesztések fészke."),
       [("lampasor", "Lámpásőr", "caster", "Lámpása fénye gyógyít, és bevilágít a ködbe.", ["ten_lampasfeny"], {"sight": 11}, 1),
        ("szelhimzo", "Szélhímző", "caster", "Levélörvényt kavar.", ["ten_levelorveny"], {}, 1),
        ("tovisvivo", "Tövisvívó", "light", "Tövispengével vív, tündérszárnyon szökell.", ["ten_szokkenes"], {"speed": 3.1}, 2),
        ("lombijasz", "Lombíjász", "ranged", "A lombok közül nyilaz.", [], {"attack": {"range": 6}}, 2),
        ("tolgyor", "Tölgyőr", "heavy", "Élő, mozgó fa-óriás.", ["ten_gyokerdobbantas"], {"speed": 1.7, "hp": 600}, 2)],
       [("lombsuhanas", "Lombsuhanás", "+15% sebesség a tendüür egységeknek.", [{"stat": "speed_pct", "tags": ["ally_tenduur"], "add": 15}]),
        ("tetovalt_vedelem", "Tetovált védelem", "+1 páncél a tendüür egységeknek.", [{"stat": "armor", "tags": ["ally_tenduur"], "add": 1}]),
        ("lampasfeny", "Lámpásfény", "+1 látótáv minden egységnek.", [{"stat": "sight", "tags": ["all"], "add": 1}])],
       {"ten_lampasfeny": heal("Lámpásfény", 9), "ten_levelorveny": storm("Levélörvény", 16, 2.0, 4, 70, (180, 230, 120), "gyokerkotes"),
        "ten_szokkenes": rush("Szökkenés", 1.8, 1.2, 16), "ten_gyokerdobbantas": stomp("Gyökérdobbantás", 38, 2.4, 1.2, 18, (120, 90, 60))})

people("nila_uchana", "nil", "Nila-uchana", "Lótuszudvarok és szörnyidézők",
       "A kék bőrű, látható külső fül nélküli nép lótuszos udvarokat és díszes szentélyeket emel. A nága, a makara és a rakshasa "
       "náluk a telepek faragványain és a csatatéren is megjelenik, idézett lényként vagy szövetségesként.",
       "aztec", (60, 70, 160), (240, 160, 40),
       ("szornyek_csarnoka", "Szörnyek csarnoka", "Szörnykötőket és rakshasákat képez."),
       ("makara_medence", "Makara-medence", "Makara-idézőket, nága szövetségeseket és makarákat képez."),
       ("lotuszudvar", "Lótuszudvar", "A nila-uchana fejlesztések udvara."),
       [("szornykoto", "Szörnykötő", "melee", "Láncos bottal harcol; rakshasát hív maga mellé.", ["nil_rakshasa_hivas"], {}, 1),
        ("rakshasa", "Rakshasa", "heavy", "Megkötött, vörös szőrű démoni fenevad.", ["nil_tombolas"], {"speed": 2.4}, 1),
        ("makara_idezo", "Makara-idéző", "caster", "Vízörvényt kavar, és gyógyít.", ["nil_orveny", "nil_lotuszgyogyitas"], {}, 2),
        ("naga", "Nága szövetséges", "ranged", "Kígyótestű lándzsás; mérgező dárdát vet.", [],
         {"hp": 140, "attack": {"projectile": "spear", "poison": [3, 4]}}, 2),
        ("makara", "Makara", "cavalry", "Elefántfejű vízi szörny.", ["nil_makara_roham"], {"hp": 360}, 2)],
       [("kotes_ereje", "Kötés ereje", "+3 sebzés a nagy testű egységeknek.", [{"stat": "damage", "tags": ["large"], "add": 3}]),
        ("naga_pikkely", "Nága-pikkely", "+1 páncél a nila-uchana egységeknek.", [{"stat": "armor", "tags": ["ally_nila_uchana"], "add": 1}]),
        ("lotuszszentseg", "Lótuszszentség", "+40 életerő a nila-uchana egységeknek.", [{"stat": "hp", "tags": ["ally_nila_uchana"], "add": 40}])],
       {"nil_rakshasa_hivas": summon("Rakshasa hívása", "nil_rakshasa_kotott", 1, 30, 45),
        "nil_tombolas": stomp("Tombolás", 40, 2.2, 0.8, 16, (200, 40, 40)),
        "nil_orveny": storm("Vízörvény", 16, 2.0, 4, 70, (90, 200, 220), "lelegzet", slow=0.5),
        "nil_lotuszgyogyitas": heal("Lótuszgyógyítás", 10), "nil_makara_roham": rush("Makara-roham", 1.6, 1.3, 20)})

people("zhan_zhama", "zha", "Zhan-zhama", "Apró nép, nagy kakasok",
       "A sötétbőrű, apró termetű nép mellett egy közönséges tyúk is nagynak látszik, egy kakas pedig már hátasnak is megfelel. "
       "Csirkepásztoraik és keltetőik köré épül a telep élete, a Tolljósok pedig madárszellemeket idéznek.",
       "tribal", (190, 60, 40), (240, 200, 80),
       ("feszekhaz", "Fészekház", "Csirkepásztorokat és tojásdobálókat képez."),
       ("kakasudvar", "Kakasudvar", "Kakaslovasokat, tolljósokat és madárszellemeket képez."),
       ("keltetoszentely", "Keltetőszentély", "A zhan-zhama fejlesztések szentélye."),
       [("csirkepasztor", "Csirkepásztor", "light", "Olcsó, gyors harcos – tyúkjai csipkedik az ellent.", [],
         {"cost": {"wood": 35, "stone": 10}, "hp": 80, "time": 10, "radius": 0.32}, 1),
        ("tojasdobalo", "Tojásdobáló", "ranged", "Kemény tojásokat hajít.", [], {"cost": {"wood": 50, "stone": 15}, "hp": 65,
                                                                            "attack": {"projectile": "boulder", "damage": 10}}, 1),
        ("kakaslovas", "Kakaslovas", "cavalry", "Lándzsás, kakasháton száguldó lovas.", ["zha_kukorekolas"],
         {"hp": 230, "speed": 3.4, "cost": {"wood": 110, "stone": 35}}, 2),
        ("tolljos", "Tolljós", "caster", "Madárszellemeket idéz, és gyógyít.", ["zha_madarszellem_hivas", "zha_tollgyogyitas"], {}, 2),
        ("madarszellem", "Madárszellem", "flyer", "Arany fényből szőtt madárszellem.", [], {"hp": 90}, 2)],
       [("gyors_keltetes", "Gyors keltetés", "+10% sebesség minden egységnek.", [{"stat": "speed_pct", "tags": ["all"], "add": 10}]),
        ("kakaskukorekolas", "Kakaskukorékolás", "+2 sebzés a zhan-zhama egységeknek.",
         [{"stat": "damage", "tags": ["ally_zhan_zhama"], "add": 2}]),
        ("tollszellem", "Tollszellem", "+30 életerő a zhan-zhama egységeknek.", [{"stat": "hp", "tags": ["ally_zhan_zhama"], "add": 30}])],
       {"zha_kukorekolas": chant("Kukorékolás", {"damage_mult": 1.1}, 5),
        "zha_madarszellem_hivas": summon("Madárszellem hívása", "zha_madarszellem", 2, 30, 35),
        "zha_tollgyogyitas": heal("Tollgyógyítás", 9)})

people("shrissh_matah", "shr", "Shrissh-matah", "Tollas rovarok rúnavárai",
       "A száraz sziklavidék tollas rovarai tüskékkel és rúnákkal díszített termeszvárakban fészkelnek. A tűszúnyogok a levegőből "
       "portyáznak, a molnárka formájú Sziklafutók kampós lábakkal kapaszkodnak a köveken, a Rúnaszövők pedig jeleket vésnek a "
       "kaptárak agyagfalába.",
       "organic", (170, 110, 50), (120, 230, 180),
       ("tollvar", "Tollvár", "Rúnaszövőket, sziklafutókat és pajzsbogarakat képez."),
       ("tuskekelteto", "Tüskekeltető", "Tűszúnyogokat és tüskelövőket képez."),
       ("runaszellozo", "Rúnaszellőző", "A shrissh-matah fejlesztések rúnatornya."),
       [("runaszovo", "Rúnaszövő", "caster", "Rúnákat vés: védő jelet és robbanó rúnát.", ["shr_vedorunak", "shr_robbanorunak"], {}, 1),
        ("sziklafuto", "Sziklafutó", "light", "Kampós lábakkal száguld a köveken át.", ["shr_kapaszkodas"], {"speed": 3.3}, 1),
        ("pajzsbogar", "Pajzsbogár", "melee", "Tollas, vastag páncélú bogár.", [], {"hp": 200, "armor": 3}, 1),
        ("tuszunyog", "Tűszúnyog", "flyer", "Tűhegyes ormányú repülő portyázó.", [], {"attack": {"range": 0.8, "damage": 12}}, 2),
        ("tuskelovo", "Tüskelövő", "ranged", "Tollas tüskéket lő.", [], {"attack": {"damage": 13}}, 2)],
       [("runavesest", "Rúnavésés", "+2 sebzés a shrissh-matah egységeknek.", [{"stat": "damage", "tags": ["ally_shrissh_matah"], "add": 2}]),
        ("tollas_pancel", "Tollas páncél", "+1 páncél a shrissh-matah egységeknek.", [{"stat": "armor", "tags": ["ally_shrissh_matah"], "add": 1}]),
        ("szikla_kapaszkodas", "Szikla-kapaszkodás", "+15% sebesség a shrissh-matah egységeknek.",
         [{"stat": "speed_pct", "tags": ["ally_shrissh_matah"], "add": 15}])],
       {"shr_vedorunak": chant("Védőrúnák", {"armor_add": 2}, 5, "rune"),
        "shr_robbanorunak": bolt("Robbanó rúna", 50, 45, 10, icon="rune"),
        "shr_kapaszkodas": rush("Kapaszkodás", 1.7, 1.15, 16)})

# --- idézett (nem képezhető) egységek
SUMMONED = {
    "tuz_delibab_mas": ("tuz_elf", "Délibáb-más", "light", "Rövid életű délibáb-harcos.", {"hp": 40, "attack": {"damage": 5}}),
    "nil_rakshasa_kotott": ("nila_uchana", "Megkötött rakshasa", "heavy", "Rövid ideig a szörnykötő mellett harcol.",
                            {"hp": 260}),
}

# Micéliák: a régi micélia-faj sprite-jaival
MICELIA = dict(
    pid="micelia", pre="mic", name="Micéliák", tagline="Gyökérlábú gombanép",
    desc="Ezek a gyökérlábú gombalények élő fonalakkal kötik össze telepeiket, amelyek szívében egy óriási gomba lüktet. Míg a "
         "spóraszedők készleteket gyűjtenek és a pöfetegek őrzik a telepet, az Emlékfa gyökerei között régi társaik történetei "
         "pihennek.", style="fungal", color=(150, 120, 80), accent=(80, 230, 205))

DIPLO = {  # főfajonként: (azonosító, a főfaj stílusa)
    "elves": "elf_diplomacia", "ursan": "urs_diplomacia", "croftian": "cro_diplomacia", "wullof": "wul_diplomacia",
    "imperium": "imp_diplomacia", "hukucha": "huk_diplomacia", "ooni": "oon_diplomacia",
}
ALLY_COST = {"stone": 150, "meteor": 50}   # a szövetség megkötésének ára
AI_ALLY_PREF = {"elves": ["niil_elf", "tuz_elf", "tenduur"], "ursan": ["pinguu", "tokla", "ghoblin"],
                "croftian": ["daresh", "shrissh_matah", "galoca"], "wullof": ["zhan_zhama", "daresh", "tenduur"],
                "imperium": ["mhonyo", "ghoblin", "tuz_elf"], "hukucha": ["niil_elf", "nila_uchana", "pinguu"],
                "ooni": ["tokla", "mhonyo", "nila_uchana"]}


def rd(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def wr(p, d):
    with open(p, "w", encoding="utf-8") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=2) + "\n")


def make_unit(pid, uid, name, role, desc, abilities, over):
    import copy
    u = copy.deepcopy(ROLE[role])
    u["name"], u["desc"] = name, desc
    for k, v in over.items():
        if k == "attack" and isinstance(v, dict):
            for ak, av in v.items():
                if av is None:
                    u["attack"].pop(ak, None)
                else:
                    u["attack"][ak] = av
        else:
            u[k] = v
    u["tags"] = sorted(set(u.get("tags", [])) | {f"ally_{pid}", "ally"})
    if abilities:
        u["abilities"] = list(abilities)
    u["ally"] = pid
    return u


def build_people(P, abilities):
    pid, pre = P["pid"], P["pre"]
    units, buildings, researches = {}, {}, {}
    b_ids = [f"{pre}_{P['b1'][0]}", f"{pre}_{P['b2'][0]}", f"{pre}_{P['b3'][0]}"]
    prod = {b_ids[0]: [], b_ids[1]: []}
    for suf, name, role, desc, abl, over, bno in P["units"]:
        uid = f"{pre}_{suf}".lower()
        units[uid] = make_unit(pid, uid, name, role, desc, abl, over)
        prod[b_ids[bno - 1]].append(uid)
    for sid, (p2, name, role, desc, over) in SUMMONED.items():
        if p2 == pid:
            u = make_unit(pid, sid, name, role, desc, [], over)
            u["summoned"] = True
            u["cost"] = {}
            units[sid] = u
    for rsuf, name, desc, eff in P["res"]:
        rid = f"{pre}_{rsuf}"
        researches[rid] = {"name": name, "desc": desc, "cost": {"wood": 120, "stone": 120, "meteor": 40}, "time": 55,
                           "effects": eff, "ally": pid}
    for i, (suf, name, desc) in enumerate((P["b1"], P["b2"], P["b3"])):
        bid = b_ids[i]
        b = {"name": name, "desc": desc, "cost": {"wood": 160, "stone": 80}, "time": 38, "size": [3, 3], "hp": 850,
             "armor": 1, "sight": 7, "shape": "barracks" if i < 2 else "tech", "ally": pid}
        if i < 2:
            b["produces"] = prod[bid]
        else:
            b["researches"] = list(researches)
            b["cost"] = {"wood": 150, "stone": 120}
        if i == 1:
            b["requires"] = [b_ids[0]]
        if i == 2:
            b["requires"] = [b_ids[0]]
        buildings[bid] = b
    for k, v in P["ab"].items():
        abilities[k] = dict(v, desc=v.get("desc", ""), ally=pid)
    d = {"id": pid, "ally": True, "order": 101 + PEOPLES.index(P), "name": P["name"], "tagline": P["tagline"], "desc": P["desc"],
         "style": P["style"], "color": list(P["color"]), "accent": list(P["accent"]), "mechanic": "standard",
         "units": units, "buildings": buildings, "researches": researches,
         "ally_pack": {"buildings": b_ids, "units": [u for u in units if not units[u].get("summoned")]},
         "ai": {"build": b_ids[:2] + [b_ids[2]], "army": {u: (3 if units[u].get("supply", 1) <= 2 else 2)
                                                         for u in units if not units[u].get("summoned")},
                "research": list(researches)}}
    return d


def build_micelia(myc, abilities):
    """A Micéliák szövetséges népként: a régi micélia-sprite-ok álnévként (sprite), új, ally azonosítókkal."""
    pid = "micelia"
    src_u = myc["units"]

    def unit(uid, src, name=None, desc=None, over=None, abl=None):
        import copy
        u = copy.deepcopy(src_u[src])
        for k in ("path", "hero", "hero_abilities", "requires", "ability_path", "ability_research"):
            u.pop(k, None)
        if name:
            u["name"] = name
        if desc:
            u["desc"] = desc
        u.update(over or {})
        u["abilities"] = abl if abl is not None else [a for a in u.get("abilities", []) if a in abilities and
                                                       not abilities[a].get("spores")]
        u["sprite"] = src
        u["tags"] = sorted(set(u.get("tags", [])) | {"ally_micelia", "ally"})
        u["ally"] = pid
        return u
    units = {
        "mic_sporaszedo": unit("mic_sporaszedo", "myc_sporaszedo", desc="Gyűjtögető gombanép: készleteket hord a telepre."),
        "mic_pofeteg": unit("mic_pofeteg", "myc_pofeteg", desc="A telep pöfeteg-őre."),
        "mic_fonalhalos": unit("mic_fonalhalos", "myc_alomszovo", "Fonalhálós", "Élő fonalakkal köti meg és gyógyítja a társakat.",
                               {"energy": 150}, ["mic_fonalgyogyitas", "mic_fonalhalo"]),
        "mic_tuskegomba_lovesz": unit("mic_tuskegomba_lovesz", "myc_tuskegomba_lovesz"),
        "mic_emlekor": unit("mic_emlekor", "myc_kalapos_or", "Emlékőr", "Az Emlékfa kalapos őre; a régi társak emlékét védi."),
    }
    abilities["mic_fonalgyogyitas"] = dict(heal("Fonalgyógyítás", 10), desc="", ally=pid)
    abilities["mic_fonalhalo"] = dict(storm("Fonalháló", 6, 2.0, 4, 60, (120, 200, 120), "gyokerkotes", slow=0.1), desc="", ally=pid)
    researches = {
        "mic_osok_emleke": {"name": "Ősök emléke", "desc": "+30 életerő a micélia egységeknek.", "cost": {"wood": 120, "stone": 120,
                                                                                                         "meteor": 40}, "time": 55,
                            "effects": [{"stat": "hp", "tags": ["ally_micelia"], "add": 30}], "ally": pid},
        "mic_sporafonal": {"name": "Spórafonal", "desc": "+10% sebesség minden egységnek.", "cost": {"wood": 120, "stone": 120,
                                                                                                   "meteor": 40}, "time": 55,
                           "effects": [{"stat": "speed_pct", "tags": ["all"], "add": 10}], "ally": pid},
        "mic_szivdobbanas": {"name": "Szívdobbanás", "desc": "+1 életerő-visszatöltés másodpercenként a micélia egységeknek.",
                             "cost": {"wood": 120, "stone": 120, "meteor": 40}, "time": 55,
                             "effects": [{"stat": "regen", "tags": ["ally_micelia"], "add": 1}], "ally": pid},
    }
    buildings = {
        "mic_termotest": {"name": "Termőtest", "desc": "Spóraszedőket, pöfetegeket és fonalhálósokat képez.",
                          "cost": {"wood": 160, "stone": 80}, "time": 38, "size": [3, 3], "hp": 850, "armor": 1, "sight": 7,
                          "shape": "barracks", "produces": ["mic_sporaszedo", "mic_pofeteg", "mic_fonalhalos"],
                          "sprite": "myc_termotest", "ally": pid},
        "mic_szivgomba": {"name": "Szívgomba", "desc": "Tüskegomba-lövészeket és emlékőröket képez; a telep lüktető szíve.",
                          "cost": {"wood": 160, "stone": 80}, "time": 38, "size": [2, 2], "hp": 850, "armor": 1, "sight": 8,
                          "shape": "barracks", "produces": ["mic_tuskegomba_lovesz", "mic_emlekor"], "requires": ["mic_termotest"],
                          "sprite": "myc_szivgomba", "ally": pid},
        "mic_emlekfa": {"name": "Emlékfa", "desc": "A micélia fejlesztések fája; gyökerei között a régi társak történetei pihennek.",
                        "cost": {"wood": 150, "stone": 120}, "time": 38, "size": [3, 3], "hp": 850, "armor": 1, "sight": 7,
                        "shape": "tech", "researches": list(researches), "requires": ["mic_termotest"],
                        "sprite": "myc_emlekfa", "ally": pid},
    }
    return {"id": pid, "ally": True, "order": 100, "name": MICELIA["name"], "tagline": MICELIA["tagline"],
            "desc": MICELIA["desc"], "style": "fungal", "color": list(MICELIA["color"]), "accent": list(MICELIA["accent"]),
            "mechanic": "standard", "units": units, "buildings": buildings, "researches": researches,
            "ally_pack": {"buildings": list(buildings), "units": list(units)},
            "ai": {"build": list(buildings), "army": {"mic_pofeteg": 3, "mic_tuskegomba_lovesz": 3, "mic_emlekor": 2,
                                                      "mic_fonalhalos": 1}, "research": list(researches)}}


def auto_desc(a):
    """Rövid, egységes leírás egy képesség adataiból."""
    t = a.get("type")
    mods = a.get("modifiers", {})
    names = {"speed": "sebesség", "damage": "sebzés", "armor": "páncél", "range": "lőtáv", "regen": "gyógyulás"}
    m = []
    for k, v in mods.items():
        nm = names.get(k.rsplit("_", 1)[0], k)
        m.append(f"{nm} {(v - 1) * 100:+.0f}%" if k.endswith("_mult") else f"{nm} {v:+g}")
    ms = ", ".join(m)
    if t == "autoheal":
        return f"Automatikusan gyógyítja a közeli sebesült szövetségeseket ({a.get('amount', 0)} életerő)."
    if t == "area":
        s = f"Területre ható csapás: {a.get('duration', 0):g} mp alatt {a.get('dps', 0) * a.get('duration', 0):g} sebzés"
        if a.get("slow"):
            s += ", és lassít"
        return s + "."
    if t == "toggle":
        return f"Be- és kikapcsolható állás: {ms}."
    if t == "self_buff":
        return f"{a.get('duration', 0):g} mp-re: {ms}."
    if t == "summon":
        return f"Lényt idéz {a.get('lifetime', 30):g} mp-re."
    if t == "nova":
        return f"Csapás maga körül: {a.get('damage', 0):g} sebzés" + (", és megbénít." if a.get("stun") else ".")
    if t in ("aura", "passive"):
        return f"Állandó hatás: {ms}." if ms else "Állandó hatás."
    if t == "chant":
        return f"Ének: a közeli szövetségesek – {ms}." if ms else "Ének a közeli szövetségeseknek."
    if t == "blink":
        return f"Gyors ugrás a kijelölt pontra ({a.get('range', 0):g} csempe)."
    return ms + "." if ms else "Különleges képesség."


def main():
    abilities = rd(ABP)
    for k in [k for k, v in abilities.items() if isinstance(v, dict) and v.get("ally")]:
        del abilities[k]
    # régi kimenetek törlése (idempotencia)
    for fn in os.listdir(FDIR):
        if fn.startswith("ally_") and fn.endswith(".json"):
            os.remove(os.path.join(FDIR, fn))
    myc_p = os.path.join(FDIR, "mycel.json")
    myc = rd(myc_p)
    myc["hidden"] = True          # a Micéliák csak szövetséges nép; a régi faj a hadjárathoz marad meg, rejtve
    wr(myc_p, myc)
    out = [build_micelia(myc, abilities)] + [build_people(P, abilities) for P in PEOPLES]
    for d in out:
        wr(os.path.join(FDIR, f"ally_{d['id']}.json"), d)
    for v in abilities.values():  # a szövetséges képességek leírása az adataikból (a súgóablakhoz)
        if isinstance(v, dict) and v.get("ally") and not v.get("desc"):
            v["desc"] = auto_desc(v)
    wr(ABP, abilities)
    # Diplomácia csarnoka a főfajoknál
    for fid, bid in DIPLO.items():
        p = os.path.join(FDIR, fid + ".json")
        if not os.path.isfile(p):
            continue
        d = rd(p)
        d["buildings"][bid] = {"name": "Diplomácia csarnoka",
                               "desc": "Itt köthetsz szövetséget egy kisebb néppel (egyszer, véglegesen). A szövetség után "
                                       "a nép épületei a munkásaid építési listáján jelennek meg.",
                               "cost": {"wood": 180, "stone": 120}, "time": 45, "size": [3, 3], "hp": 900, "armor": 1,
                               "sight": 8, "shape": "tech", "diplomacy": True}
        d["ally_cost"] = dict(ALLY_COST)
        ai = d.setdefault("ai", {})
        bo = [b for b in ai.get("build_order", []) if b != bid]
        bo.insert(min(len(bo), 6), bid)
        ai["build_order"] = bo
        ai["ally_pref"] = AI_ALLY_PREF.get(fid, [])
        wr(p, d)
    print(f"szövetséges népek: {len(out)} nép, Diplomácia csarnoka {sum(os.path.isfile(os.path.join(FDIR, f + '.json')) for f in DIPLO)} főfajnál")


if __name__ == "__main__":
    main()
