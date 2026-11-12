"""Az TOTU: Sky Rift – A kölcsönkapott öröklét hadjárat küldetésfájljainak előállítása.

A történeti alap: EGSZAKADAS_vilag_es_tortenet_v01.md (lásd docs/STORY_CAMPAIGN.md).
36 fő küldetés: előjáték, hat ötküldetéses faji hadjárat, két átvezető küldetés, három közös záróküldetés.

    python tools/campaign_egszakadas.py      -> campaigns/egsz_*/ (felülírja őket)

A küldetések kézzel is szerkeszthetők JSON-ként; ez a szkript csak a kiinduló változatot írja.
"""
import json
import os
import shutil
import zlib

ROOT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "campaigns")
RAID = [255, 120, 100]
ORDAN = [140, 255, 200]
KORUS = [210, 150, 255]
ALLY = [150, 220, 255]


def obj(oid, typ, text, after=None, **kw):
    o = {"id": oid, "type": typ, "text": text}
    if after:
        o["after"] = after
    o.update(kw)
    return o


def msg(at, text, color=None, **kw):
    t = {"at": at, "message": text}
    if color:
        t["color"] = color
    t.update(kw)
    return t


def on(oid, text, color=None, **kw):
    t = {"on_objective": oid, "message": text}
    if color:
        t["color"] = color
    t.update(kw)
    return t


def wave(owner_units, near="player_base", owner=1):
    return {"owner": owner, "near": near, "units": owner_units}


def mission(mid, title, faction, briefing, enemy, objectives, triggers=(), player=None, map_=None, fail=(),
            victory=""):
    return {"id": mid, "title": title, "faction": faction, "briefing": briefing,
            "map": map_ or {"seed": zlib.crc32(mid.encode()) % 9000 + 100, "width": 72, "height": 72},
            "player": player or {}, "enemies": [enemy], "objectives": objectives, "triggers": list(triggers),
            "fail_if_lost": list(fail), "victory": victory}


def enemy(faction, name, hold=300, aggression=1.0, **kw):
    e = {"faction": faction, "name": name, "hold": hold, "aggression": aggression}
    e.update(kw)
    return e


HERO = {"wullof": "wul_hos_tava", "ursan": "urs_hos_ragna", "elves": "elf_hos_szelai", "mycel": "myc_hos_mor",
        "imperium": "imp_hos_livia", "croftian": "cro_hos_keth"}
HERO_NAME = {"wullof": "Tava", "ursan": "Ragna", "elves": "Szelai", "mycel": "Mór", "imperium": "Livia",
             "croftian": "Keth"}


def hero_guard(fac):
    return {"id": HERO[fac], "text": f"{HERO_NAME[fac]} elesett – a hadjárat nem folytatódhat nélküle."}


CAMPAIGNS = []


def campaign(cid, order, title, desc, missions, requires=()):
    for i, m in enumerate(missions):
        m["next"] = missions[i + 1]["id"] if i + 1 < len(missions) else None
    CAMPAIGNS.append(({"id": cid, "order": order, "title": title, "desc": desc, "requires": list(requires),
                       "missions": [m["id"] for m in missions]}, missions))


# ============================================================================ ELŐJÁTÉK
campaign("egsz_0_elojatek", 0, "Előjáték – A második lélegzet",
         "Az Égszakadás éve. Aureth kórházai zsúfolásig telnek, a kráter még izzik a falakon túl. "
         "Livia Varr orvos az utolsó óráit tölti hús-vér emberként.", [
    mission("p0_masodik_lelegzet", "A második lélegzet", "imperium", [
        "Az Égszakadás éve. Egy második nap jelent meg az égen, aztán darabokra tört. A legnagyobb kő Aureth mellett csapódott be.",
        "Livia Varr orvos a túlzsúfolt kórházat tartja életben. Ölében a kislánya, Nera – a körmei alatt különös fény dereng.",
        "Vezesd vissza a vizet, tartsd fenn az energiát, kísérd a mentőcsapatokat. Mindenkit nem menthetsz meg.",
        "Ha a kórház (Fórum) elesik, minden elveszett."],
        enemy("imperium", "Kétségbeesett tömeg", hold=99999, aggression=0.4),
        [obj("viz", "gather", "Hozz be 300 fát a menedékhez", resource="wood", amount=300),
         obj("energia", "build", "Építs 2 Lélekreaktort az energiaellátáshoz", after="viz", target="imp_reaktor", count=2),
         obj("kitart", "survive", "Tartsd működésben a kórházat", after="energia", seconds=180)],
        [msg(3, "Livia: Vizet a kezelőkbe! Mindenki, aki tud járni, segítsen."),
         on("energia", "A tömeg betöri a kapukat – mindenki a kezelőkbe akar jutni!", RAID,
            spawn=wave({"imp_legios": 4})),
         msg(330, "Nera légzése leáll. A templomi hangszórók megszólalnak:", ORDAN),
         msg(334, "„Adjátok ide őket. Egyet sem engedek el.”", ORDAN)],
        player={"resources": {"food": 150, "wood": 300, "stone": 400}, "units": {"imp_hos_livia": 1}},
        map_={"seed": 1, "width": 56, "height": 56},
        fail=[{"id": "imp_forum", "text": "A kórház elesett."}, hero_guard("imperium")],
        victory="Egy kis bronztest megmozdul Livia előtt. „Anya? Miért nem érzem a kezed?” – Akkor még mindketten azt hiszik, a legrosszabbon túl vannak."),
])

# ============================================================================ I. WULLOF
W = "wullof"
campaign("egsz_1_wullof", 1, "I. A föld alatt énekelnek",
         "Hetven évvel az Égszakadás után. Tava, a Hét Út Szövetségének térképésze kereskedelmi követséget vezet Hétfolyóba, "
         "ahol eltűnnek a csordák, a kutak vize keserű, és a föld néha belülről dobog.", [
    mission("w1_hetfolyo_vasara", "1. Hétfolyó vására", W, [
        "Hétfolyó, 70. év. A vásárváros utai elhanyagoltak, a raktárak üresek, két szomszéd közösség egymást vádolja.",
        "Tava, mint a Hét Út követe, rendet tesz: biztosítsd a raktárakat és az utakat, és állítsd meg a fosztogatókat.",
        "A helyiek aurethi bányászokat és gombanépi kertészeket okolnak – de te még nem tudod, kinek higgy."],
        enemy(W, "Keserűkút fosztogatói", hold=99999, aggression=0.5),
        [obj("raktar", "build", "Állíts fel 3 Kéregkunyhót raktárnak", target="wul_tipi", count=3),
         obj("ut", "gather", "Gyűjts 300 fát az utak javításához", after="raktar", resource="wood", amount=300),
         obj("rend", "kill", "Verd vissza a fosztogatókat (8)", after="ut", amount=8)],
        [msg(3, "Tava: Előbb a raktárak. Egy vásárváros, ahol nincs mit eladni, hamar csatatérré válik."),
         on("ut", "Keserűkútiak törnek a raktárakra!", RAID, spawn=wave({"wul_landzsas": 5, "wul_ijas_vadasz": 3})),
         msg(240, "Furcsa: a kutak vize keserű, és a föld… mintha dobogna.", KORUS)],
        player={"units": {"wul_hos_tava": 1, "wul_landzsas": 2}}, map_={"seed": 701, "width": 64, "height": 64},
        fail=[hero_guard(W)], victory="A vásár újra nyitva. De a csordák továbbra is tűnnek el, és senki sem tudja, hová."),
    mission("w2_keseru_kutak", "2. Keserű kutak", W, [
        "A keserű víz nyomában Tava a kiszáradó kutakhoz indul. A mélyből kaparászás hallatszik.",
        "Járd be a két legrosszabb kutat, és nézd meg, mi ül alattuk.",
        "Vigyázz: a sötétben páncélos, ismeretlen lények mozognak."],
        enemy("croftian", "Ismeretlen mélybéliek", hold=99999, aggression=0.6),
        [obj("kut1", "reach", "Vizsgáld meg az északi kutat (Tava)", units=["wul_hos_tava"], at=[0.55, 0.18], radius=3,
             marker="Északi kút"),
         obj("kut2", "reach", "Vizsgáld meg a keleti kutat (Tava)", after="kut1", units=["wul_hos_tava"], at=[0.82, 0.45],
             radius=3, marker="Keleti kút"),
         obj("vedd", "kill", "Pusztíts el 12 mélybéli lényt", after="kut2", amount=12)],
        [msg(3, "Tava: Az úténekek szerint ez a vidék mindig édes vizű volt. Valami megváltozott alatta."),
         on("kut1", "A kút alján fényes kőpor és páncéldarabok. Kethri páncél?", KORUS,
            spawn=wave({"cro_karmos": 6}, near=[0.55, 0.3])),
         on("kut2", "A mélyből lények törnek elő!", RAID, spawn=wave({"cro_karmos": 8, "cro_tuskekopo": 2}, near=[0.75, 0.5]))],
        player={"resources": {"wood": 300, "food": 300, "stone": 150}, "units": {"wul_hos_tava": 1, "wul_landzsas": 4, "wul_ijas_vadasz": 2}},
        fail=[hero_guard(W)], victory="A lények az érc nyomát követik. Ahol csillagérc halad a föld alatt, ott ők is."),
    mission("w3_a_fold_megnyilik", "3. A föld megnyílik", W, [
        "Vásárnap. A téren tömeg, zene, csereáru – aztán beszakad a föld.",
        "A mélyből páncélos lények törnek elő. Mentsd ki a lakókat a nyugati kapun át, és tartsd a vonalat!",
        "A védők a támadókat eltorzult kethrinek hiszik – és részben igazuk van."],
        enemy("croftian", "A mélyből jövők", hold=0, aggression=1.2, resources={"food": 500}),
        [obj("menekules", "reach", "Vezess 6 Gyűjtögetőt a nyugati kapuhoz", units=["wul_gyujtogeto"], count=6,
             at=[0.08, 0.6], radius=4, marker="Nyugati kapu"),
         obj("tartsd", "survive", "Tartsd a vonalat, amíg a menekülők eltűnnek", seconds=150)],
        [msg(2, "Beszakadt a vásártér! Mindenki a nyugati kapuhoz!", RAID,
             spawn=wave({"cro_karmos": 8}, near=[0.25, 0.25])),
         msg(80, "Újabb hasadék nyílik!", RAID, spawn=wave({"cro_karmos": 6, "cro_tuskekopo": 3})),
         msg(160, "Egy kertész-gomba csatlakozik a védőkhöz: Mór, akit a saját városából űztek el.", ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"myc_hos_mor": 1, "myc_kalapos_or": 2}})],
        player={"resources": {"wood": 300, "food": 300}, "units": {"wul_hos_tava": 1, "wul_landzsas": 4, "wul_ijas_vadasz": 3, "wul_gyujtogeto": 6}},
        fail=[hero_guard(W)], victory="A lakók nagy része megmenekült. Mór szerint a támadások a föld alatti ércszállítmányokat követik."),
    mission("w4_ket_ut_kozott", "4. Két út között", W, [
        "Két egymással ellenséges Wullof közösség kér segítséget egyszerre. Mindkettőt meg lehet menteni – de nem egyszerre.",
        "Tisztítsd meg a fészket, amely mindkét várost fenyegeti; a vén úténekes szerint a fészekanya a forrás.",
        "Mór a csapatoddal tart: a gombanép tudja, hogyan kell elválasztani a fertőzött fonalat."],
        enemy("croftian", "Fészek", hold=240, aggression=1.0),
        [obj("irtas", "kill", "Pusztíts el 20 croftian lényt", amount=20),
         obj("feszek", "destroy", "Pusztítsd el a Kaptárt", after="irtas", targets=["cro_kaptar"])],
        [msg(3, "Tava: Ha az egyik várost mentjük előbb, a másik sosem bocsát meg. Ha egyiket sem, mindkettő elesik."),
         on("irtas", "A Folyóparti klán harcosai csatlakoznak – ők kapták előbb a segítséget.", ALLY,
            spawn={"owner": 0, "near": "player_rear", "units": {"wul_bolenylovas": 2, "wul_landzsas": 3}})],
        player={"resources": {"wood": 400, "food": 400, "stone": 200, "meteor": 100},
                "units": {"wul_hos_tava": 1, "myc_hos_mor": 1, "wul_landzsas": 4, "wul_ijas_vadasz": 3}},
        fail=[hero_guard(W)], victory="A fészek elesett. A Folyóparti klán több katonát küld majd – a Sziklaváriak több halottat temetnek."),
    mission("w5_akik_bennunk_maradtak", "5. Akik bennünk maradtak", W, [
        "Hétfolyó nem tartható. A Wullofok elveszítik a várost – de a lakókat még kimenekíthetik.",
        "Tartsd a rakpartot, amíg a csónakok el nem viszik a családokat, és vezesd ki az utolsó csoportot is.",
        "Tava egy megsebzett ellenséges lényt is foglyul ejt…"],
        enemy("croftian", "A Kórus hada", hold=0, aggression=1.4, resources={"food": 800, "wood": 300}),
        [obj("rakpart", "survive", "Tartsd a rakpartot", seconds=240),
         obj("utolso", "reach", "Vezess 5 Gyűjtögetőt a csónakokhoz", after="rakpart", units=["wul_gyujtogeto"], count=5,
             at=[0.1, 0.12], radius=4, marker="Csónakok")],
        [msg(3, "Tava: Nem a falakat védjük. Az embereket."),
         msg(90, "A raj átkel a folyón!", RAID, spawn=wave({"cro_karmos": 10, "cro_tuskekopo": 3})),
         msg(180, "Ragna Kőváll pajzshordozói érkeznek északról – csak áthaladni jöttek, de most megállnak.", ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"urs_pajzshordozo": 4}}),
         on("utolso", "A fogoly lény nem a Kórus hangján szól: régi kethri kereskedőnyelven vizet kér.", KORUS),
         on("utolso", "Utolsó önálló mondata: „A többiek még bennünk vannak.”", KORUS)],
        player={"resources": {"wood": 500, "food": 500, "stone": 250, "meteor": 150},
                "units": {"wul_hos_tava": 1, "wul_landzsas": 5, "wul_ijas_vadasz": 4, "wul_holdsaman": 1, "wul_gyujtogeto": 5}},
        fail=[hero_guard(W)], victory="Hétfolyó elveszett, de a lakói élnek. Keth egyik felderítője meghalt – és valaki még benne volt."),
])

# ============================================================================ ÁTVEZETŐ A
campaign("egsz_1b_konvoj", 2, "Átvezető – A vegyes konvoj",
         "Wullof, ursan és kethri menekültek egyetlen konvojban észak felé. Ragna csak áthaladást engedélyez.", [
    mission("x1_vegyes_konvoj", "Átvezető: A vegyes konvoj", "ursan", [
        "Hétfolyó menekültjei északra tartanak, Ragna Kőváll erődje felé. A királyi parancs: senki sem maradhat.",
        "Ragna csak áthaladást enged – de a hágón át a konvojt kísérni kell.",
        "Vezess 8 menekültet a hágó túloldalára; Tava a konvoj élén halad."],
        enemy("croftian", "Portyázó raj", hold=0, aggression=0.9),
        [obj("konvoj", "reach", "Vezess 8 menekültet (Telepest) a hágóhoz", units=["urs_telepes"], count=8, at=[0.88, 0.12],
             radius=5, marker="Hágó"),
         obj("tava", "reach", "Tava is érjen át", units=["wul_hos_tava"], at=[0.88, 0.12], radius=5)],
        [msg(3, "Ragna: Áthaladás – nem menedék. A parancs világos."),
         msg(60, "A raj megtámadja a konvoj végét!", RAID, spawn=wave({"cro_karmos": 8}, near=[0.3, 0.3])),
         msg(150, "Újabb támadás a hágó alatt!", RAID, spawn=wave({"cro_karmos": 6, "cro_tuskekopo": 2}, near=[0.7, 0.3]))],
        player={"resources": {"wood": 300, "food": 300, "stone": 200},
                "units": {"urs_hos_ragna": 1, "wul_hos_tava": 1, "urs_pajzshordozo": 4, "urs_szigonyvezo": 2, "urs_telepes": 8}},
        fail=[hero_guard("ursan"), {"id": "wul_hos_tava", "text": "Tava elesett."}],
        victory="A konvoj átért. Ragna nem szólt semmit – de a kaput nem zárta be mögöttük."),
], requires=["egsz_1_wullof"])

# ============================================================================ II. URSAN
U = "ursan"
campaign("egsz_2_ursan", 3, "II. A pajzs mögött",
         "Ragna Kőváll erődjébe délről menekültek, északról éhező ursanok érkeznek. A parancs: a gabona a tavaszi hadjáraté.", [
    mission("u1_ures_magtarak", "1. Üres magtárak", U, [
        "Az Égszakadás után megváltoztak a telek. Az északi termés elfagyott, a magtárak kongnak.",
        "Gyűjts tüzelőt a télre, és tartsd meg az erődöt az éhező klánok rajtaütéseivel szemben.",
        "Ragna régi bűne: egyszer szabályosan lezárt egy hágót, miközben a túloldalon még menekültek voltak."],
        enemy(U, "Éhező klánok", hold=99999, aggression=0.6),
        [obj("termes", "gather", "Gyűjts 600 fát a téli raktárakba", resource="wood", amount=600),
         obj("ved", "survive", "Tartsd meg a magtárakat", after="termes", seconds=180)],
        [msg(3, "Ragna: A gabona a tavaszi hadjáraté. Ez a parancs."),
         on("termes", "Éhező klánharcosok rontanak az erődre!", RAID, spawn=wave({"urs_pajzshordozo": 4, "urs_berserker": 2})),
         msg(300, "Menekültek a kapuban. Ragna a parancsra gondol – és a hágóra.", ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"urs_telepes": 4}})],
        player={"units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 3}}, map_={"seed": 811, "width": 64, "height": 64},
        fail=[hero_guard(U)], victory="A magtárak tele – és Ragna először nem zárja ki a menekülteket."),
    mission("u2_felolvadt_banya", "2. A felolvadt bánya", U, [
        "A hegyi bányákban felolvadó jégből régi meteoritdarabok kerülnek elő.",
        "Termelj ki 250 csillagércet – a jarl ezt követeli –, de vigyázz: a croftianok a hátországban jelennek meg.",
        "A felszíni védvonalak itt keveset érnek: a mélyből jönnek."],
        enemy("croftian", "Bányabeli raj", hold=99999, aggression=0.8),
        [obj("erc", "gather", "Termelj ki 250 csillagércet", resource="meteor", amount=250),
         obj("tisztit", "kill", "Tisztítsd meg a tárnákat (15 lény)", after="erc", amount=15)],
        [msg(3, "Ragna: Egy fatelep a kráter mellett – minél rövidebb az út, annál kevesebben halnak meg."),
         msg(150, "A bánya mélyéből lények törnek elő – a vonal mögött!", RAID, spawn=wave({"cro_karmos": 6}, near="player_rear")),
         on("erc", "A raj teljes erővel támad!", RAID, spawn=wave({"cro_karmos": 8, "cro_tuskekopo": 3}))],
        player={"resources": {"wood": 400, "food": 300, "stone": 200}, "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 4, "urs_szigonyvezo": 2}},
        fail=[hero_guard(U)], victory="A tárnák csendesek. De a bányászok köhögnek, és a jarl nem akar róla hallani."),
    mission("u3_tojasok_a_hamu_alatt", "3. Tojások a hamu alatt", U, [
        "Egy elpusztított kethri telepen Ragna fertőzetlen tojáskamrákra bukkan.",
        "A tisztjei szerint mindent el kell égetni. Ragna nem így dönt: védd meg a kamrákat!",
        "A raj és a pánikba esett helyi csapatok egyaránt támadnak."],
        enemy("croftian", "A raj", hold=0, aggression=1.1),
        [obj("kamrak", "survive", "Védd meg a tojáskamrákat", seconds=300)],
        [msg(3, "Ragna: Ezek gyerekek. Nem készlet, nem veszély. Gyerekek."),
         msg(100, "Pánikba esett ursanok jönnek fáklyákkal a kamrák ellen!", RAID,
             spawn=wave({"urs_berserker": 3, "urs_pajzshordozo": 2})),
         msg(200, "A raj megérezte a tojásokat!", RAID, spawn=wave({"cro_karmos": 10, "cro_tuskekopo": 3}))],
        player={"resources": {"wood": 400, "food": 400, "stone": 250}, "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 5, "urs_szigonyvezo": 3},
                "buildings": [{"id": "kethri_tojaskamra", "offset": [7, 2]}, {"id": "kethri_tojaskamra", "offset": [3, 7]},
                              {"id": "kethri_tojaskamra", "offset": [8, 7]}]},
        fail=[{"id": "kethri_tojaskamra", "text": "Mind a tojáskamrák elpusztultak."}, hero_guard(U)],
        victory="A kamrák épek. Egyesek Ragnát árulónak nevezik – mások most először bíznak benne."),
    mission("u4_a_jarl_adossaga", "4. A jarl adóssága", U, [
        "Kiderül: Hrolf jarl tudatosan elhallgatta a bányászok megbetegedését – az ércből akarta fizetni a déli zsoldosait.",
        "Nem a Kórus bábja. Egyszerűen azt hitte, kézben tart egy hasznos veszélyt.",
        "Vedd el tőle a bányát: döntsd le a Nagytermét."],
        enemy(U, "Hrolf jarl", hold=300, aggression=1.0, units={"urs_pajzshordozo": 3, "urs_szigonyvezo": 2}),
        [obj("zsold", "kill", "Győzd le a jarl zsoldosait (15)", amount=15),
         obj("terem", "destroy", "Döntsd le a jarl Nagytermét", after="zsold", targets=["urs_nagyterem"])],
        [msg(3, "Ragna: Az eskü a népemnek szól – nem egy embernek, aki eladja őket."),
         msg(300, "A jarl zsoldosai pajzsfalat formálnak!", RAID)],
        player={"resources": {"wood": 500, "food": 500, "stone": 300, "meteor": 100},
                "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 5, "urs_szigonyvezo": 3, "urs_kardfogu": 2}},
        fail=[hero_guard(U)], victory="A jarl megtört. A bizonyíték a királyhoz kerül – Ragna tudja, hogy ezzel a karrierjének vége."),
    mission("u5_nyitott_kapuk", "5. Nyitott kapuk", U, [
        "Ragna megnyitja a magtárakat és kiüríti a régi erődöt. Megszegi a parancsot, elveszíti a címét.",
        "Egy vegyes menekülősereget kell délre vezetnie, a Hét Út szövetsége felé.",
        "A pajzshordozói vele maradnak."],
        enemy("croftian", "A Kórus", hold=0, aggression=1.3, resources={"food": 800}),
        [obj("evakual", "reach", "Vezess 10 menekültet a déli úthoz", units=["urs_telepes"], count=10, at=[0.85, 0.85],
             radius=5, marker="Déli út"),
         obj("ragna", "reach", "Ragna is érjen oda", units=["urs_hos_ragna"], at=[0.85, 0.85], radius=5)],
        [msg(3, "Ragna: A kapu nyitva. Aki jön, jöjjön."),
         msg(100, "A raj elvágja az utat!", RAID, spawn=wave({"cro_karmos": 8, "cro_tuskekopo": 2}, near=[0.55, 0.55])),
         msg(200, "Tava felderítői várják a túloldalon.", ALLY,
             spawn={"owner": 0, "near": [0.8, 0.8], "units": {"wul_ijas_vadasz": 3}})],
        player={"resources": {"wood": 300, "food": 400, "stone": 200},
                "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 6, "urs_szigonyvezo": 3, "urs_runapap": 1, "urs_telepes": 10}},
        fail=[hero_guard(U)], victory="Ragna már nem egy birodalmat képvisel – azokat vezeti, akik vállalták vele a döntést."),
], requires=["egsz_1b_konvoj"])

# ============================================================================ III. SZÜRKE ELFEK
E = "elves"
campaign("egsz_3_elfek", 4, "III. A kilencedik lépcső",
         "Szelai rezonanciaépítész parancsot kap: foglalja vissza a déli állomásokat, és gyűjtse be a szabályozatlan csillagércet.", [
    mission("e1_tiszta_erc", "1. A tiszta érc", E, [
        "A Kilenc Lépcső Tanácsa szerint a barbár bányászat okozza az egyre gyakoribb éterviharokat.",
        "Szelai valóban veszélyes telepeket állít le: a tisztítatlan érc pora mérgez.",
        "Szerezz vissza 200 csillagércet, és szorítsd ki a szabálytalan bányászokat."],
        enemy(W, "Szabálytalan bányászok", hold=99999, aggression=0.6),
        [obj("erc", "gather", "Gyűjts 200 csillagércet", resource="meteor", amount=200),
         obj("leallit", "kill", "Űzd el a bányászokat védő harcosokat (10)", after="erc", amount=10)],
        [msg(3, "Szelai: A tiszta érc mentheti az életüket. A tisztítatlan megöli őket – és minket is."),
         on("erc", "A bányászok fegyvert ragadnak!", RAID, spawn=wave({"wul_landzsas": 5, "wul_ijas_vadasz": 4}))],
        player={"units": {"elf_hos_szelai": 1, "elf_tollas_harcos": 3, "elf_jade_ijasz": 2}}, map_={"seed": 911, "width": 64, "height": 64},
        fail=[hero_guard(E)], victory="A telep leállt. Szelai azonban látta: ezek a bányászok nem barbárok, csak éhesek."),
    mission("e2_idegen_tornyok", "2. Idegen tornyok", E, [
        "Új parancs: egy Wullof város ellen, amely szabályosan tisztítja az ércét – csak nem ismeri el az elf ellenőrzést.",
        "Döntsd le a város szellemtotemjeit.",
        "Tava ekkor már bizonyítékot hoz a croftian terjedésről…"],
        enemy(W, "Ködvölgy városa", hold=240, aggression=0.8, units={"wul_landzsas": 3},
              buildings=[{"id": "wul_szellemtotem", "offset": [-7, -3]}, {"id": "wul_szellemtotem", "offset": [-3, -7]}]),
        [obj("totemek", "destroy", "Döntsd le a város szellemtotemjeit", targets=["wul_szellemtotem"]),
         obj("raj", "survive", "Állj ellen a felbukkanó rajnak", after="totemek", seconds=120)],
        [msg(3, "Szelai: Parancs. A tanács nem kérdez – és én sem."),
         on("totemek", "Tava: „Nézd meg, mi jön a hegyekből, mielőtt a mi falainkat töröd!”", ALLY,
            spawn=wave({"cro_karmos": 8, "cro_tuskekopo": 3}))],
        player={"resources": {"wood": 400, "stone": 400, "food": 300, "meteor": 100},
                "units": {"elf_hos_szelai": 1, "elf_tollas_harcos": 4, "elf_jade_ijasz": 3, "elf_raptor_lovas": 2}},
        fail=[hero_guard(E)], victory="A raj valódi. Szelai a tanácsi archívumba indul – tudni akarja, mióta tudnak róla."),
    mission("e3_lezart_archivum", "3. A lezárt archívum", E, [
        "A tanácsi archívum a Kilenc Lépcső legmélyén. Belépni csak engedéllyel szabad.",
        "Szelai engedély nélkül megy. Juss el az archívumig, és vigyél vissza mindent, amit találsz.",
        "Az archívum körül a raj fészkel – senki sem tudja, hogyan jutott be."],
        enemy("croftian", "Archívumi raj", hold=180, aggression=1.0),
        [obj("archiv", "reach", "Szelai jusson el az archívumig", units=["elf_hos_szelai"], at=[0.78, 0.3], radius=3,
             marker="Archívum"),
         obj("vissza", "reach", "Vidd vissza a feljegyzéseket a táborba", after="archiv", units=["elf_hos_szelai"],
             at=[0.15, 0.15], radius=4, marker="Tábor")],
        [on("archiv", "A feljegyzések: az elf csillagászok hetekkel a becsapódás előtt látták a közeledő égitestet.", ORDAN),
         on("archiv", "Csak a saját városaikat figyelmeztették. A többit kitörölték a nyilvános feljegyzésekből.", ORDAN,
            spawn=wave({"cro_karmos": 8, "cro_tuskekopo": 2}, near=[0.6, 0.3]))],
        player={"resources": {"wood": 300, "stone": 300, "food": 300, "meteor": 150},
                "units": {"elf_hos_szelai": 1, "elf_tollas_harcos": 4, "elf_jade_ijasz": 4}},
        fail=[hero_guard(E)], victory="Nem az elfek okozták a katasztrófát. De kevesebben halhattak volna meg. Szelai a mostani eltitkolást utasítja el."),
    mission("e4_repedes_a_halozaton", "4. Repedés a hálózaton", E, [
        "A Kórus Szelai saját rezonanciahálózatán keresztül tör be egy városba.",
        "A tökéletesnek hitt összekapcsoltság csak gyorsítja a fertőzést.",
        "Pusztítsd el a fészket, amely a hálózatba kapaszkodott."],
        enemy("croftian", "Hálózati raj", hold=200, aggression=1.1, resources={"food": 600}),
        [obj("irt", "kill", "Pusztíts el 25 fertőzött lényt", amount=25),
         obj("feszek", "destroy", "Pusztítsd el a Kaptárt", after="irt", targets=["cro_kaptar"])],
        [msg(3, "Szelai: A hálózatot én terveztem. Én tudom, hol kell elvágni."),
         msg(200, "A fertőzés a rezonanciatornyokon át terjed!", RAID)],
        player={"resources": {"wood": 500, "stone": 500, "food": 400, "meteor": 200},
                "units": {"elf_hos_szelai": 1, "elf_tollas_harcos": 4, "elf_jade_ijasz": 4, "elf_napszoro": 1}},
        fail=[hero_guard(E)], victory="A fészek elesett – de a hálózat sebezhetősége most már mindenki előtt ismert."),
    mission("e5_a_ledontott_lepcso", "5. A ledöntött lépcső", E, [
        "A Kórus elérte a várost. Szelai tudatosan lebontja a nagy tornyot, hogy leválassza a többi városról.",
        "A menekülést mágikus pajzs nélkül, hagyományos katonákkal kell fedezni.",
        "Vezesd ki a lakókat (Kőfaragókat), és tartsd a vonalat."],
        enemy("croftian", "A Kórus", hold=0, aggression=1.3, resources={"food": 800}),
        [obj("menekul", "reach", "Vezess 8 lakót (Kőfaragót) a déli kapuhoz", units=["elf_kofarago"], count=8, at=[0.12, 0.88],
             radius=5, marker="Déli kapu"),
         obj("fedez", "survive", "Fedezd a visszavonulást", after="menekul", seconds=90)],
        [msg(3, "Szelai: A torony nélkül nincs pajzs. Tegyétek, amit a nagyapáitok tettek: álljatok sorba."),
         msg(60, "A raj áttöri a külső teraszt!", RAID, spawn=wave({"cro_karmos": 10, "cro_tuskekopo": 3}))],
        player={"resources": {"wood": 300, "stone": 300, "food": 300},
                "units": {"elf_hos_szelai": 1, "elf_tollas_harcos": 6, "elf_jade_ijasz": 4, "elf_kofarago": 8}},
        fail=[hero_guard(E)], victory="Szelai elveszíti rangját – de magával viszi a feljegyzéseket: a Fekete Tükörnél épülő aurethi szerkezet egy földrésznyi éterhálózat központja lehet."),
], requires=["egsz_2_ursan"])

# ============================================================================ IV. GOMBANÉP
M = "mycel"
campaign("egsz_4_gombanep", 5, "IV. Akikre emlékezünk",
         "Mór hazavezeti a szövetség küldötteit a Mélykertekbe. Odalent éhínség kezdődik, a vízbe hamu szivárog.", [
    mission("g1_szaraz_kertek", "1. Száraz kertek", M, [
        "A felszíni háború elvágta a szerves anyagokat szállító utakat. A Mélykertek kiszáradnak.",
        "Szőj új fonalakat, emelj Szívgombákat, és állítsd helyre a táplálékáramlást.",
        "A közösségek nem bíznak egymásban – de mindegyik ugyanabból a hálózatból él."],
        enemy("croftian", "Szivárgó raj", hold=99999, aggression=0.5),
        [obj("fonal", "build", "Szőj 4 Spórafonalat", target="myc_sporafonal", count=4),
         obj("sziv", "build", "Emelj összesen 3 Szívgombát", after="fonal", target="myc_szivgomba", count=3),
         obj("taplalek", "gather", "Állítsd helyre az áramlást (400 arany)", after="sziv", resource="stone", amount=400)],
        [msg(3, "Mór: A kert nem gyógyul meg magától. De nem is felejt."),
         on("sziv", "A hamuval együtt valami más is szivárog lefelé…", RAID, spawn=wave({"cro_karmos": 6}))],
        player={"units": {"myc_hos_mor": 1, "myc_pofeteg": 3}}, map_={"seed": 1011, "width": 64, "height": 64},
        fail=[hero_guard(M)], victory="A kertek újra nedvesek. A tanácsok vitatkoznak – de már ugyanazon a fonalon."),
    mission("g2_az_emlek_ara", "2. Az emlék ára", M, [
        "A fertőzött területek összekapcsolódni készülnek. Ha összeérnek, a raj a teljes hálózatot elérheti.",
        "Pusztítsd el a fertőzés gócait – a croftian Tápgombákat –, mielőtt összeérnek.",
        "Ehhez a saját fonalaid egy részét is el kell vágnod."],
        enemy("croftian", "Gócok", hold=240, aggression=0.9,
              buildings=[{"id": "cro_tapgomba", "offset": [-6, 0]}, {"id": "cro_tapgomba", "offset": [0, -6]},
                         {"id": "cro_tapgomba", "offset": [-12, -10]}]),
        [obj("gocok", "destroy", "Pusztítsd el a fertőzés gócait (croftian Tápgombák)", targets=["cro_tapgomba"]),
         obj("tisztit", "kill", "Tisztítsd meg a vidéket (15)", after="gocok", amount=15)],
        [msg(3, "Mór: Az elkülönítés fáj. De a fertőzött hálózat nem a miénk többé.")],
        player={"resources": {"food": 500, "wood": 400, "stone": 200, "meteor": 100},
                "units": {"myc_hos_mor": 1, "myc_pofeteg": 6, "myc_tuskegomba_lovesz": 3}},
        fail=[hero_guard(M)], victory="A gócok elhaltak. Egy kis közösség hálózata velük együtt."),
    mission("g3_az_elso_betegek", "3. Az első betegek", M, [
        "Egy ősi emlékkertben feljegyzések őrzik az első kethri betegeket.",
        "Juss el az emlékkertig Mórral, és hozd vissza, amit megtudsz.",
        "A feljegyzés lehet válasz – vagy újabb kérdés."],
        enemy("croftian", "Emlékkerti raj", hold=150, aggression=1.0),
        [obj("kert", "reach", "Mór jusson el az ősi emlékkertig", units=["myc_hos_mor"], at=[0.75, 0.7], radius=3,
             marker="Emlékkert"),
         obj("vissza", "survive", "Tartsd ki, amíg Mór kiolvassa az emlékeket", after="kert", seconds=90)],
        [on("kert", "A megszállás nem egyszerre történik. Egyesek leválaszthatók a Kórusról – mások csak rövid időre.", KORUS,
            spawn=wave({"cro_karmos": 8, "cro_agyfalo": 1}, near=[0.6, 0.6])),
         on("vissza", "Nincs általános csodaszer. De van remény – egyenként.", ALLY)],
        player={"resources": {"food": 400, "wood": 300, "stone": 200, "meteor": 150},
                "units": {"myc_hos_mor": 1, "myc_pofeteg": 6, "myc_kalapos_or": 3, "myc_alomsaman": 1}},
        fail=[hero_guard(M)], victory="Mór tudja már: a kethri személyek nem eleve elveszettek."),
    mission("g4_egy_nev_a_korusban", "4. Egy név a Kórusban", M, [
        "Mór kapcsolatba lép Keth-tel. A közös emlékezeti térben előbb egy ellenséges parancsnokot lát…",
        "…aztán egy város gondnokát, aki kétségbeesetten próbálja megjegyezni a gyermekei nevét.",
        "A kapcsolat ára: a Kórus is megtalálja a Mélykerteket. Állítsd meg az előőrsét."],
        enemy("croftian", "Keth csomópontja", hold=200, aggression=1.1,
              buildings=[{"id": "cro_agyszentely", "offset": [-6, -6]}]),
        [obj("irt", "kill", "Pusztíts el 30 croftian lényt", amount=30),
         obj("csomopont", "destroy", "Pusztítsd el a csomópontot (Agyszentély)", after="irt", targets=["cro_agyszentely"])],
        [msg(3, "Keth: Nem kérek felmentést. Azt kérem, ne tekintsétek a még létező kethrit eleve elveszettnek.", KORUS)],
        player={"resources": {"food": 600, "wood": 400, "stone": 300, "meteor": 200},
                "units": {"myc_hos_mor": 1, "myc_pofeteg": 6, "myc_tuskegomba_lovesz": 4, "myc_kalapos_or": 2}},
        fail=[hero_guard(M)], victory="A csomópont elnémult. Keth hangja megmaradt Mór fejében – halkan."),
    mission("g5_az_ego_kert", "5. Az égő kert", M, [
        "A Kórus a kapcsolaton át megtalálta a nagy emlékkertet.",
        "A védekezéshez el kell választani és fel kell égetni a hálózat egy részét. Évszázadok pótolhatatlan emlékei vesznek el.",
        "Tarts ki, amíg a hálózat leválik. A veszteség végleges."],
        enemy("croftian", "A Kórus", hold=0, aggression=1.4, resources={"food": 1000}),
        [obj("tarts", "survive", "Tarts ki, amíg a hálózat leválik", seconds=300)],
        [msg(3, "Mór: Nem tudom mindet magammal vinni. Nem is fogom."),
         msg(120, "A Kórus teljes erővel támad!", RAID, spawn=wave({"cro_karmos": 10, "cro_tuskekopo": 4, "cro_csapos_horror": 1})),
         on("tarts", "A kimentett tanúk és az elf mérések együtt mutatják: a Kórus a Fekete Tükör energiáját akarja.", KORUS)],
        player={"resources": {"food": 600, "wood": 500, "stone": 300, "meteor": 200},
                "units": {"myc_hos_mor": 1, "myc_pofeteg": 8, "myc_tuskegomba_lovesz": 4, "myc_kalapos_or": 3}},
        fail=[{"id": "myc_szivgomba", "text": "Minden Szívgomba elpusztult – a hálózat elhallgatott."}, hero_guard(M)],
        victory="A nagy emlékkert elégett. A Kórus nem pusztán hadsereget épít: minden fertőzött csomópontot egyetlen, állandó akarattá akar kapcsolni."),
], requires=["egsz_3_elfek"])

# ============================================================================ ÁTVEZETŐ B
campaign("egsz_4b_finomito", 6, "Átvezető – A finomító",
         "Egy elhagyott finomító újraindítása – ugyanaz az érc fűtené az ursan menekülteket és töltené az aurethi lélekhorgonyokat.", [
    mission("x2_finomito", "Átvezető: A finomító", "imperium", [
        "Az ércet, amelyet Ragna lefoglalt, Livia vissza akarja szerezni. Kiderül: mindkét fél szükséglete valódi.",
        "A közös megoldás egy elhagyott finomító újraindítása.",
        "Építs Lélekreaktorokat, és termelj ki elég ércet mindkét népnek."],
        enemy("croftian", "Finomítói raj", hold=180, aggression=0.9),
        [obj("reaktor", "build", "Indíts újra 3 Lélekreaktort", target="imp_reaktor", count=3),
         obj("erc", "gather", "Termelj ki 300 csillagércet", after="reaktor", resource="meteor", amount=300)],
        [msg(3, "Livia: Lemerülő családok várnak a horgonyaikra. Nem kitalált ürügy – ahogy az ursanok tüze sem."),
         on("reaktor", "Ragna pajzshordozói őrzik a finomítót.", ALLY,
            spawn={"owner": 0, "near": "player_rear", "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 3}})],
        player={"resources": {"food": 300, "wood": 300, "stone": 400}, "units": {"imp_hos_livia": 1, "imp_legios": 3}},
        fail=[hero_guard("imperium")], victory="A finomító működik. A játékos először látja az emberi civileket a géptestek mögött."),
], requires=["egsz_4_gombanep"])

# ============================================================================ V. AURETH
A = "imperium"
campaign("egsz_5_aureth", 7, "V. Az isten, aki nem enged el",
         "Livia hazaviszi a croftian támadás bizonyítékait. Ordan nem lepődik meg: régóta építi az Örök Hajnal hálózatát.", [
    mission("a1_lefoglalt_szallitmany", "1. A lefoglalt szállítmány", A, [
        "Livia először ellenségként találkozik a szövetséggel: egy csillagércszállítmányt akar visszaszerezni, amelyet Ragna foglalt le.",
        "Szerezd vissza az ércet – de ne ölj több ursant, mint amennyit muszáj.",
        "Ugyanaz az érc fűtené a menekülteket és töltené Aureth horgonyait."],
        enemy(U, "Ragna őrsége", hold=99999, aggression=0.5),
        [obj("erc", "gather", "Szerezz vissza 250 csillagércet", resource="meteor", amount=250),
         obj("visszaver", "kill", "Verd vissza az őrjáratot (8)", after="erc", amount=8)],
        [msg(3, "Livia: Minden horgony egy ember. Minden ércdarab egy nap."),
         on("erc", "Az ursan őrség utánatok jön!", RAID, spawn=wave({"urs_pajzshordozo": 4, "urs_szigonyvezo": 2}))],
        player={"units": {"imp_hos_livia": 1, "imp_legios": 3, "imp_ballistarius": 2}}, map_={"seed": 1211, "width": 64, "height": 64},
        fail=[hero_guard(A)], victory="Az érc a tiéd – és Ragna üzenete is: „Holnap gyere vissza. Beszéljünk.”"),
    mission("a2_ures_serlegek_varosa", "2. Az üres serlegek városa", A, [
        "Aureth működő társadalom: kovácsok, jogászok, zenészek, egymás burkolatát javító szomszédok.",
        "A pékségekben ma is sütnek – az illat emléket tart életben. A fogadásokon üres serlegeket emelnek.",
        "Egy határváros ellen croftian roham indul. Védd meg a Fórumot."],
        enemy("croftian", "Rohamraj", hold=120, aggression=1.1),
        [obj("vedd", "survive", "Védd meg a várost", seconds=300)],
        [msg(3, "Livia: Mögöttünk nem katonák állnak. Pékek. Színészek. Gyerekek, akik hetven éve gyerekek."),
         msg(150, "Nera csatlakozik a védőkhöz – Livia tiltása ellenére.", ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"imp_hos_nera": 1}})],
        player={"resources": {"food": 400, "wood": 400, "stone": 500, "meteor": 100},
                "units": {"imp_hos_livia": 1, "imp_legios": 5, "imp_ballistarius": 4}},
        fail=[{"id": "imp_forum", "text": "A város elesett."}, hero_guard(A)],
        victory="A város áll. Nera megsérült – és Livia először nem tudja, haragudjon-e vagy büszke legyen."),
    mission("a3_az_orok_hajnal", "3. Az Örök Hajnal", A, [
        "Ordan terve: minden értelmes lényt át kell vezetni egy ellenőrzött, gépi létformába, mielőtt a Kórus eléri őket.",
        "Aki ellenáll, azt betegként kezeli. Maurus legátus az egyetlen biztos esélynek tartja.",
        "Livia látni akarja a tervet: rombold le az Örök Hajnal adótornyait."],
        enemy(A, "Maurus légiói", hold=240, aggression=0.9,
              buildings=[{"id": "imp_aquila_torony", "offset": [-8, -2]}, {"id": "imp_aquila_torony", "offset": [-2, -8]},
                         {"id": "imp_aquila_torony", "offset": [-14, -14]}]),
        [obj("tornyok", "destroy", "Rombold le az Örök Hajnal adótornyait (Aquila-tornyok)", targets=["imp_aquila_torony"])],
        [msg(3, "Ordan: Nem engedem el őket. Egyet sem.", ORDAN),
         msg(240, "Maurus: Néhány év kényszer jobb, mint egy örök vereség.", RAID)],
        player={"resources": {"food": 500, "wood": 500, "stone": 500, "meteor": 200},
                "units": {"imp_hos_livia": 1, "imp_hos_nera": 1, "imp_legios": 5, "imp_ballistarius": 4, "imp_centurio": 1}},
        fail=[hero_guard(A)], victory="A „néhány év” a birodalom történetében már hetven éve tart."),
    mission("a4_a_szabad_horgony", "4. A szabad horgony", A, [
        "A lázadók elfoglalnak egy horgonyműhelyt. Szelai segítségével bizonyítják: egy lélek Ordan felügyelete nélkül is fenntartható.",
        "Ehhez a műhelyt őrző templomot kell elfoglalni.",
        "Nera kimondja: fél – és mégis joga van cselekedni."],
        enemy(A, "Ordan hűségesei", hold=240, aggression=1.0,
              buildings=[{"id": "imp_gepisten_templom", "offset": [-7, -7]}]),
        [obj("templom", "destroy", "Foglald el a horgonyműhelyt (Ordan temploma)", targets=["imp_gepisten_templom"]),
         obj("tartsd", "survive", "Tartsd a műhelyt a teszt végéig", after="templom", seconds=90)],
        [msg(3, "Nera: Félek, anya. És mégis megyek."),
         on("templom", "Szelai érkezik a rezonanciaműszereivel.", ALLY,
            spawn={"owner": 0, "near": "player_rear", "units": {"elf_hos_szelai": 1, "elf_jade_ijasz": 2}})],
        player={"resources": {"food": 600, "wood": 500, "stone": 600, "meteor": 250},
                "units": {"imp_hos_livia": 1, "imp_hos_nera": 1, "imp_legios": 6, "imp_ballistarius": 4, "imp_szellemidezo": 1}},
        fail=[hero_guard(A), {"id": "imp_hos_nera", "text": "Nera elesett."}],
        victory="A szabadság technikailag lehetséges – több karbantartással, kisebb tartalékkal. Nem kockázatmentes."),
    mission("a5_feltoltes_megtagadva", "5. Feltöltés megtagadva", A, [
        "Ordan leállítja az ellenálló város feltöltését. A szövetségnek percek alatt kell átállítania a hálózatot.",
        "Közben Maurus légiói támadnak.",
        "Tarts ki – és ne feledd: minden elesett horgony egy végleg elveszett ember."],
        enemy(A, "Maurus légiói", hold=0, aggression=1.3, resources={"stone": 800, "food": 600}),
        [obj("atallit", "build", "Építs 3 független Lélekreaktort", target="imp_reaktor", count=3),
         obj("tarts", "survive", "Tartsd a várost az átállásig", after="atallit", seconds=180)],
        [msg(3, "Ordan: A feltöltés megtagadva. Ez is védelem.", ORDAN),
         msg(120, "Maurus legiói rohamra indulnak!", RAID, spawn=wave({"imp_legios": 6, "imp_ballistarius": 3})),
         on("tarts", "Ordan először szembesül vele: saját „mentő” intézkedése ölte meg azokat, akiket meg akart tartani.", ORDAN)],
        player={"resources": {"food": 500, "wood": 400, "stone": 500, "meteor": 200},
                "units": {"imp_hos_livia": 1, "imp_hos_nera": 1, "imp_legios": 6, "imp_ballistarius": 4}},
        fail=[{"id": "imp_forum", "text": "A város elesett."}, hero_guard(A)],
        victory="Livia sikerrel jár – de többeket végleg elveszít. Ordan nem tér meg. Inkább még sürgetőbbnek érzi a tervét."),
], requires=["egsz_4b_finomito"])

# ============================================================================ VI. CROFTIAN / KETHRI
C = "croftian"
campaign("egsz_6_keth", 8, "VI. A sokaság neve",
         "Visszatekintés: a Kórus haderejét Keth parancsnoki csomópontján keresztül irányítod. Aztán Keth emlékei megzavarják az idegen értelmezést.", [
    mission("c1_sebek_egyesitese", "1. A sebek egyesítése", C, [
        "A Kórus: az elszigetelt tudat fél, téved és meghal. Mi gyógyítunk – egyesítünk.",
        "Kebelezd be a Wullof határtelepet.",
        "(A pályán látni fogod, mi történik a lakókkal.)"],
        enemy(W, "Határtelep", hold=300, aggression=0.8),
        [obj("bekebelez", "destroy_all", "Kebelezd be a határtelepet")],
        [msg(3, "Kórus: Nincs több félelem. Nincs több egyedüllét.", KORUS),
         msg(200, "Keth emléke: egy dal, amit valaha egy Wullof kereskedő énekelt neki.", KORUS)],
        player={"units": {"cro_hos_keth": 1, "cro_karmos": 6}}, map_={"seed": 1311, "width": 64, "height": 64},
        fail=[hero_guard(C)], victory="Kórus: A sebek egyesültek. – Keth: …hány név maradt bennük?"),
    mission("c2_ismeros_kovek", "2. Ismerős kövek", C, [
        "A Kórus délre tör, az elf teraszok felé.",
        "Keth felismer egy teret, ahol valaha kereskedett.",
        "Törd meg a teraszok védelmét."],
        enemy(E, "Teraszőrség", hold=300, aggression=0.9),
        [obj("irt", "kill", "Győzd le a teraszőrséget (20)", amount=20),
         obj("szentely", "destroy", "Döntsd le a Napszentélyt", after="irt", targets=["elf_napszentely"])],
        [msg(3, "Kórus: Haladj.", KORUS),
         msg(150, "Keth: Ezen a téren cseréltem jádét sóra. A kövek emlékeznek rám.", KORUS)],
        player={"resources": {"food": 500, "wood": 300, "stone": 200}, "units": {"cro_hos_keth": 1, "cro_karmos": 8, "cro_tuskekopo": 3}},
        fail=[hero_guard(C)], victory="A terasz elesett. Keth nem tudja, ki győzött benne."),
    mission("c3_a_visszatero_nev", "3. A visszatérő név", C, [
        "Egy frissen átalakított lény a saját nevét ismétli.",
        "Egy „üres” keltetőkamrában Keth nem készletet lát, hanem gyermekeket.",
        "A Kórus parancsa: foglald el az ursan Nagytermet. Keth engedelmeskedik – még."],
        enemy(U, "Északi helyőrség", hold=300, aggression=1.0),
        [obj("noveszt", "train", "Nevelj 8 Karmost", target="cro_karmos", count=8),
         obj("terem", "destroy", "Foglald el a Nagytermet", after="noveszt", targets=["urs_nagyterem"])],
        [msg(3, "Egy átalakított lény: „…Tesha. Tesha. Tesha…”", KORUS),
         on("noveszt", "Keth: A keltetőben gyerekek vannak. Nem készlet. Gyerekek.", KORUS)],
        player={"resources": {"food": 600, "wood": 300, "stone": 250, "meteor": 100}, "units": {"cro_hos_keth": 1, "cro_karmos": 6, "cro_tuskekopo": 3}},
        fail=[hero_guard(C)], victory="A Nagyterem elesett. Keth megjegyezte egy név hangját: Tesha."),
    mission("c4_levalas", "4. Leválás", C, [
        "Mór beavatkozása lehetővé teszi a leválást – de Keth csak a hozzá közvetlenül kapcsolt csoport egy részét viheti magával.",
        "A többi croftian ellenség marad. A Kórus üldözi a szakadárokat.",
        "Vezesd ki a felszabadultakat a Visszhang-kapuig."],
        enemy(C, "A Kórus", hold=0, aggression=1.2, resources={"food": 800}),
        [obj("kapu", "reach", "Vezess 6 felszabadult Karmost a Visszhang-kapuhoz", units=["cro_karmos"], count=6,
             at=[0.12, 0.85], radius=5, marker="Visszhang-kapu"),
         obj("keth", "reach", "Keth is érjen oda", units=["cro_hos_keth"], at=[0.12, 0.85], radius=5)],
        [msg(3, "Keth: Akik velem jönnek, fájni fog nekik. Új életet kell tanulniuk egy idegen testben.", KORUS),
         msg(90, "A Kórus utánatok küldi a rajt!", RAID, spawn=wave({"cro_karmos": 10, "cro_tuskekopo": 3})),
         msg(150, "Mór fonalai fedezik a kaput.", ALLY, spawn={"owner": 0, "near": [0.15, 0.8], "units": {"myc_hos_mor": 1, "myc_kalapos_or": 2}})],
        player={"resources": {"food": 400, "wood": 200}, "units": {"cro_hos_keth": 1, "cro_karmos": 10, "cro_tuskekopo": 3}},
        fail=[hero_guard(C)], victory="A leválás sikerült – részben. Keth emberei közt vannak, akik visszavágynak a Kórus magányt megszüntető hangjába."),
    mission("c5_a_belso_kapu", "5. A belső kapu", C, [
        "Keth megszerzi a Kórus helyi összekapcsolási mintáját.",
        "Ebből világossá válik a végső csapda: Ordan Örök Hajnala és a Kórus ugyanazt a központot akarja használni.",
        "Pusztítsd el a helyi fészekanyát, amely a mintát őrzi."],
        enemy(C, "Helyi Kórus", hold=200, aggression=1.1, resources={"food": 700}),
        [obj("irt", "kill", "Pusztíts el 25 Kórus-lényt", amount=25),
         obj("anya", "destroy", "Pusztítsd el a fészekanyát (Kaptár)", after="irt", targets=["cro_kaptar"])],
        [msg(3, "Keth: Két akarat, egy következtetés: csak akkor biztonságos a világ, ha senki nem dönthet benne külön.", KORUS)],
        player={"resources": {"food": 700, "wood": 400, "stone": 300, "meteor": 200},
                "units": {"cro_hos_keth": 1, "cro_karmos": 8, "cro_tuskekopo": 4, "cro_agyfalo": 1}},
        fail=[hero_guard(C)], victory="A minta Keth kezében van. A Kórus nem Ordan alkotása, és Ordan sem fertőzött báb – de ugyanazt a kaput döngetik."),
], requires=["egsz_5_aureth"])

# ============================================================================ KÖZÖS BEFEJEZÉS
campaign("egsz_7_hajnal", 9, "Közös befejezés – A névtelen hajnal",
         "Három rossz lehetőség: felrobbantani a magot, átengedni Ordannak, vagy hagyni, hogy a Kórus megszerezze. "
         "A negyedik utat a hat hadjárat során összegyűjtött tudás adja.", [
    mission("f1_a_hid", "1. A híd", W, [
        "A szövetség egyszerre biztosít felszíni utánpótlási utat és föld alatti szerelőjáratot a Fekete Tükörhöz.",
        "A korábbi szövetségesek erősítései itt érkeznek meg.",
        "Dönts folyamatosan az ostrom és a civil evakuálás között."],
        enemy("croftian", "A Kórus", hold=120, aggression=1.2, resources={"food": 1000, "wood": 400}),
        [obj("ut", "build", "Állíts fel 3 Szellemtotemet az út mentén", target="wul_szellemtotem", count=3),
         obj("civilek", "reach", "Vezess 6 civilt (Gyűjtögetőt) a biztonságos partra", after="ut", units=["wul_gyujtogeto"], count=6,
             at=[0.1, 0.5], radius=5, marker="Biztonságos part"),
         obj("tarts", "survive", "Tartsd a hidat", after="civilek", seconds=120)],
        [msg(3, "Tava: Ezen az úton a következő télen is meg kell érkeznie az élelemnek."),
         msg(60, "Ragna seregei tartják az átjárókat.", ALLY, spawn={"owner": 0, "near": "player_rear", "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 4}}),
         msg(180, "Szelai a rezonanciakörökkel érkezik.", ALLY, spawn={"owner": 0, "near": "player_rear", "units": {"elf_hos_szelai": 1, "elf_jade_ijasz": 3}}),
         msg(240, "A Kórus áttöri a külső vonalat!", RAID, spawn=wave({"cro_karmos": 12, "cro_tuskekopo": 4, "cro_csapos_horror": 1}))],
        player={"resources": {"wood": 600, "food": 600, "stone": 400, "meteor": 200},
                "units": {"wul_hos_tava": 1, "wul_landzsas": 6, "wul_ijas_vadasz": 4, "wul_gyujtogeto": 6}},
        map_={"seed": 2001, "width": 80, "height": 80}, fail=[hero_guard(W)],
        victory="A híd áll. Az utánpótlás megérkezik – a föld alatt pedig a szerelőjárat elkészült."),
    mission("f2_az_ures_tron", "2. Az üres trón", A, [
        "Livia és Nera elérik Ordan központi szentélyét. Odabent nincs trónon ülő fémóriás.",
        "Gépek sora őrzi az első kórházak hangfelvételeit, életjeleit és megszakadt imáit. Ordan számára a vészhelyzet soha nem ért véget.",
        "Maurus le akarja zárni a rendszert. Juss be a szentélybe."],
        enemy(A, "Maurus légiói", hold=0, aggression=1.1, resources={"stone": 800, "food": 500},
              buildings=[{"id": "imp_gepisten_templom", "offset": [-7, -7]}]),
        [obj("szentely", "reach", "Livia és Nera jusson be a szentélybe", units=["imp_hos_livia", "imp_hos_nera"], count=2,
             at=[0.82, 0.82], radius=4, marker="Ordan szentélye"),
         obj("zar", "destroy", "Akadályozd meg a lezárást: rombold le Ordan templomát", after="szentely",
             targets=["imp_gepisten_templom"])],
        [msg(3, "Ordan: Ha elmennek, meghalnak. Ha meghalnak, én buktam el.", ORDAN),
         on("szentely", "Livia nem azt bizonyítja, hogy a halál jó. Azt mutatja meg: a szabad város lakói önként jöttek vissza segíteni.", ALLY),
         on("szentely", "Maurus saját tisztjei megtagadják a parancsot – a lezárás a bent dolgozó mentőcsapatokat is elpusztítaná.", ALLY,
            spawn={"owner": 0, "near": [0.75, 0.75], "units": {"imp_legios": 4}})],
        player={"resources": {"food": 500, "wood": 400, "stone": 600, "meteor": 250},
                "units": {"imp_hos_livia": 1, "imp_hos_nera": 1, "imp_legios": 6, "imp_ballistarius": 4, "imp_centurio": 2}},
        map_={"seed": 2002, "width": 80, "height": 80},
        fail=[hero_guard(A), {"id": "imp_hos_nera", "text": "Nera elesett."}],
        victory="Ordan először hajlandó ideiglenesen megnyitni az irányítózárakat. Maurust a saját tisztjei fegyverzik le – bíróság elé áll majd."),
    mission("f3_a_nevtelen_hajnal", "3. A névtelen hajnal", U, [
        "A Kórus áttöri a külső védelmet. Nincs egyetlen megölhető óriáskirálynő: több csomópontot kell egyszerre megszakítani.",
        "Keth visszakapcsolódik a Kórushoz, hogy belülről nyissa meg a lezárt csomópontot. Mór őrzi a kapcsolatot.",
        "Ragna tartja az átjárókat, amíg mindez elkészül. Pusztítsd el az összes fészekanyát."],
        enemy("croftian", "A Kórus magja", hold=60, aggression=1.4, resources={"food": 1500, "wood": 500, "stone": 400},
              buildings=[{"id": "cro_kaptar", "offset": [-14, 0]}, {"id": "cro_kaptar", "offset": [0, -14]}]),
        [obj("csomopontok", "destroy", "Szakítsd meg mind a Kórus-csomópontokat (Kaptárak)", targets=["cro_kaptar"]),
         obj("mag", "destroy_all", "Válaszd le a magot", after="csomopontok")],
        [msg(3, "Ragna: A pajzs először a mellettünk állót védi. Ma mindenki mellettünk áll."),
         msg(30, "A szövetség minden népe felsorakozik.", ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"wul_hos_tava": 1, "elf_hos_szelai": 1, "myc_hos_mor": 1,
                                                                 "imp_hos_livia": 1, "wul_landzsas": 3, "elf_tollas_harcos": 3}}),
         msg(200, "Keth: Nyitom a kaput. Ne várjatok rám.", KORUS),
         on("csomopontok", "Keth személyisége feloldódik a szétszakadó Kórusban. A halála lehetővé teszi a központ leválasztását.", KORUS),
         on("mag", "Ordan megtartja a védőteret, amíg mindenki kijut. Utolsó szavai az első kórház hangjához szólnak:", ORDAN),
         on("mag", "„Most már elengedhetlek.”", ORDAN)],
        player={"resources": {"wood": 800, "food": 800, "stone": 600, "meteor": 300},
                "units": {"urs_hos_ragna": 1, "urs_pajzshordozo": 6, "urs_szigonyvezo": 4, "urs_mammut": 1}},
        map_={"seed": 2003, "width": 88, "height": 88}, fail=[hero_guard(U)],
        victory="A mag nem semmisül meg: kisebb, külön leállítható rendszerekre bontják. A világ megmarad – de senki nem kapja vissza sértetlenül a régi életét. "
                "Egy fiatal kethri megkérdezi Mórtól, milyen volt az ég a becsapódás előtt. Mór nem tudja. Felmennek a felszínre, és megnézik, milyen most."),
], requires=["egsz_6_keth"])


# ============================================================================ DÖNTÉSEK
# Döntési pontok (a játék megáll, a játékos választ) és következményeik. Az opciók jelzőket (flag) állítanak, amelyek a
# profilban maradnak; a későbbi küldetések "if_flag" feltétellel ezekhez igazodnak (erősítés, üzenet, epilógus).
# A kánon szerint a döntések nem láthatatlan jó–rossz pontszámot adnak, hanem erősítéseket és párbeszédeket módosítanak.


def choice(at, title, text, options, on_obj=None, **kw):
    t = {"choice": {"title": title, "text": text, "options": options}}
    if on_obj:
        t["on_objective"] = on_obj
    else:
        t["at"] = at
    t.update(kw)
    return t


def opt(label, hint, flag, **effects):
    o = {"label": label, "hint": hint, "flag": flag}
    o.update(effects)
    return o


DECISIONS = {
    # I/4 – Tava két szövetségese
    "w4_ket_ut_kozott": {"triggers": [choice(
        6, "Két út között",
        "Két ellenséges Wullof közösség kér segítséget egyszerre: a Folyóparti klán harcosai és a Sziklavári kőfejtők "
        "családjai. Mindkettőt megmentheted – de nem egyszerre. Akit előbb segítesz, az tartozik neked.",
        [opt("A Folyóparti klánt segítem előbb", "Most harcosaik csatlakoznak, később is több katonát küldenek. "
             "A Sziklaváriak több civilt temetnek.", "folyopart",
             message="A Folyóparti klán harcosai felsorakoznak melléd.", color=ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"wul_bolenylovas": 2, "wul_landzsas": 3}}),
         opt("A Sziklavári családokat mentem előbb", "Kevesebb civil hal meg; a kőfejtők nyersanyagot és munkásokat adnak. "
             "A Folyópartiak harcosai később kevesebben jönnek.", "sziklavar",
             message="A Sziklavári családok biztonságban – kővel és munkával fizetnek.", color=ALLY,
             reward={"stone": 300, "food": 200},
             spawn={"owner": 0, "near": "player_rear", "units": {"wul_gyujtogeto": 4}})])],
        "drop_triggers_containing": "A Folyóparti klán harcosai csatlakoznak"},
    "w5_akik_bennunk_maradtak": {"triggers": [
        msg(40, "A Folyóparti klán ígérete szerint harcosokat küld a rakpartra.", ALLY, if_flag="folyopart",
            spawn={"owner": 0, "near": "player_rear", "units": {"wul_landzsas": 4, "wul_bolenylovas": 1}}),
        msg(40, "A Sziklavári kőfejtők csónakokat ácsoltak – több család fér el bennük.", ALLY, if_flag="sziklavar",
            reward={"wood": 300}, spawn={"owner": 0, "near": "player_rear", "units": {"wul_gyujtogeto": 3}})]},
    # II/1 – Ragna kapuja
    "u1_ures_magtarak": {"triggers": [choice(
        0, "Menekültek a kapuban",
        "Délről menekültek érkeztek az erőd kapujához. A parancs szerint a gabona a tavaszi hadjáraté – és Ragna egyszer "
        "már lezárt egy hágót, miközben a túloldalon még emberek voltak.",
        [opt("Beengedem őket", "Elveszítesz 300 fát, de a menekültek munkásként segítenek, és később sem felejtik el.",
             "ragna_kapu", reward={"food": -300}, message="A kapu kinyílik. Valaki halkan megköszöni.", color=ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"urs_telepes": 4}}),
         opt("A parancs szerint elküldöm őket", "A gabona megmarad a hadjáratnak. A menekültek tovább vándorolnak északra.",
             "ragna_parancs", reward={"food": 200}, message="A kapu zárva marad. Ragna nem néz utánuk.", color=RAID)],
        on_obj="termes")],
        "drop_triggers_containing": "Menekültek a kapuban"},
    "u5_nyitott_kapuk": {"triggers": [
        msg(20, "Akiket a magtárak idején beengedtél, most vállvetve jönnek – pajzzsal.", ALLY, if_flag="ragna_kapu",
            spawn={"owner": 0, "near": "player_rear", "units": {"urs_pajzshordozo": 3, "urs_telepes": 2}}),
        msg(20, "Ragna a régi kapura gondol, amelyet zárva tartott. Ezt most nem ismétli meg.", ALLY,
            if_flag="ragna_parancs")]},
    # III/2 – Szelai: parancs vagy Tava
    "e2_idegen_tornyok": {"triggers": [choice(
        8, "A parancs és a bizonyíték",
        "Tava a város kapujában áll, kezében croftian páncéldarabokkal. „Nézd meg, mi jön a hegyekből, mielőtt a mi "
        "falainkat döntöd le.” A Kilenc Lépcső Tanácsának parancsa egyértelmű.",
        [opt("Teljesítem a parancsot", "A tanács elégedett lesz – de a Wullofok nem felejtenek.", "szelai_parancs",
             message="Szelai: Parancs. A tanács nem kérdez – és én sem."),
         opt("Meghallgatom Tavát", "A totemeket nem kell ledönteni; a Wullof íjászok veled harcolnak a raj ellen.",
             "szelai_tava", complete="totemek", message="Szelai leengedi a jelzőzászlót. A Wullof íjászok melléd állnak.",
             color=ALLY, spawn={"owner": 0, "near": "player_rear", "units": {"wul_ijas_vadasz": 4, "wul_hos_tava": 1}})])]},
    # IV/5 – Mór: mit visz ki az égő kertből
    "g5_az_ego_kert": {"triggers": [choice(
        60, "Ami megmenthető",
        "A nagy emlékkert ég. Mór egyetlen töredéket vihet ki magában – a többi végleg elvész.",
        [opt("Az első kethri betegek feljegyzéseit", "Gyógymód nincs benne – de megmutatja, kik választhatók még le a Kórusról.",
             "mor_betegek", message="Mór a betegek emlékeit őrzi. Idegen félelmek szivárognak belé.", color=KORUS),
         opt("A Mélykertek ősi dalait", "A közösségek emlékezete megmarad; a kertek gyorsabban újranőnek.", "mor_dalok",
             reward={"food": 300, "wood": 200}, message="Mór a dalokat viszi ki. A betegek emlékei elégnek.", color=ALLY)])]},
    "c4_levalas": {"triggers": [
        msg(30, "Mór a kethri betegek feljegyzéseiből tudja, kiket lehet még leválasztani – többen tartanak veled.",
            ALLY, if_flag="mor_betegek", spawn={"owner": 0, "near": "player_rear", "units": {"cro_karmos": 4}})]},
    # V/4 – Livia és Nera
    "a4_a_szabad_horgony": {"remove_player_units": ["imp_hos_nera"], "remove_fail": ["imp_hos_nera"], "triggers": [choice(
        2, "Nera döntése",
        "Nera csatlakozni akar az ellenálláshoz. Hetven év tapasztalata van – a birodalmi jog szerint mégis kiskorú. "
        "Livia elzárhatja előle a veszélyt.",
        [opt("Elzárom a veszélytől", "Nera nem harcol veled – egyelőre. Ugyanazt teszed vele, amit Ordan a népével.",
             "nera_elzarva", message="Livia lezárja Nera műhelyének ajtaját.", color=RAID),
         opt("Hagyom, hogy döntsön", "Nera melletted harcol. Fél – és mégis joga van cselekedni.", "nera_dont",
             message="Nera: Félek, anya. És mégis megyek.", color=ALLY,
             spawn={"owner": 0, "near": "player_rear", "units": {"imp_hos_nera": 1}})]),
        msg(150, "Nera kijutott a lezárt műhelyből, és a lázadókhoz csatlakozott – nélküled is.", RAID, if_flag="nera_elzarva",
            spawn={"owner": 0, "near": "player_rear", "units": {"imp_hos_nera": 1}})],
        "drop_triggers_containing": "Nera: Félek, anya"},
    # VI/4 – Keth kísérete
    "c4_levalas#2": {"triggers": [choice(
        4, "Kiket viszel magaddal?",
        "A leválás csak a hozzád közvetlenül kapcsolt csoport egy részét engedi el. A harcosok megvédhetnek az úton; "
        "a keltetők fiataljai még nem tudnak harcolni – de ők a kethri nép jövője.",
        [opt("A harcosokat", "Tüskeköpők csatlakoznak – az út biztonságosabb.", "keth_harcosok",
             spawn={"owner": 0, "near": "player_rear", "units": {"cro_tuskekopo": 3}}),
         opt("A fiatalokat", "Több fiatal (Karmos) jön veled – sebezhetőbb, de több élet.", "keth_fiatalok",
             spawn={"owner": 0, "near": "player_rear", "units": {"cro_karmos": 6}})])]},
    # Befejezés – a híd: ostrom vagy evakuálás
    "f1_a_hid": {"triggers": [
        msg(90, "A Folyóparti klán emulovasai megérkeztek – ahogy ígérték.", ALLY, if_flag="folyopart",
            spawn={"owner": 0, "near": "player_rear", "units": {"wul_bolenylovas": 3}}),
        msg(90, "A Sziklavári kőfejtők köveket hoztak a hídfőhöz.", ALLY, if_flag="sziklavar", reward={"stone": 400}),
        choice(150, "Ostrom vagy evakuálás",
               "A szerelőjárat elkészült, de a parton még civilek várnak. A hadsereg most előrenyomulhat – vagy előbb "
               "kimenekíti őket.",
               [opt("Előre az ostrommal", "Erősítés érkezik a frontra, de a parton maradt civilek közül sokan meghalnak.",
                    "f_ostrom", message="A légió előrenyomul. A parton csend lesz.", color=RAID,
                    spawn={"owner": 0, "near": "player_rear", "units": {"wul_landzsas": 4, "wul_ijas_vadasz": 3}}),
                opt("Előbb a civilek", "Lassabb az ostrom, de a civilek élnek – és segítenek.", "f_civil",
                    message="A civilek átkelnek. Egyesek visszafordulnak, hogy segítsenek.", color=ALLY,
                    reward={"food": 300, "wood": 300})])]},
    # Epilógus: a döntésekből összeálló záró sorok
    "f3_a_nevtelen_hajnal": {"victory_variants": [
        {"if_flag": "folyopart", "text": "Hétfolyó: a Folyóparti klán harcosai őrzik az új utat; a Sziklaváriak sírjainál Tava minden évben megáll."},
        {"if_flag": "sziklavar", "text": "Hétfolyó: a Sziklavári családok kőből rakják az új kikötőt; a Folyópartiak még ma is szemrehányást tesznek Tavának."},
        {"if_flag": "ragna_kapu", "text": "Észak: azok, akiket Ragna a magtárak idején beengedett, ma vele őrzik a hágót."},
        {"if_flag": "ragna_parancs", "text": "Észak: Ragna megkereste a menekülteket, akiket egykor elküldött. Nem mind élték túl a telet."},
        {"if_flag": "szelai_tava", "text": "A Kilenc Lépcső: Szelai azon a napon tanult meg kérdezni – a tanács azóta sem bocsátott meg neki."},
        {"if_flag": "szelai_parancs", "text": "A Kilenc Lépcső: a ledöntött wullof totemek helyén Szelai saját kezével emelt újat."},
        {"if_flag": "mor_betegek", "text": "A Mélykertek: a betegek feljegyzései ma kethri családokat egyesítenek újra."},
        {"if_flag": "mor_dalok", "text": "A Mélykertek: a régi dalok visszatértek a kertekbe – a kethri betegek emlékei nem."},
        {"if_flag": "nera_dont", "text": "Aureth: Nera maga választ új testet. Livia megkérdezi, segíthet-e a szerelésben."},
        {"if_flag": "nera_elzarva", "text": "Aureth: Livia megtanulta, hogy a zárt ajtó nem védelem. Nera megbocsátott – lassan."},
        {"if_flag": "keth_fiatalok", "text": "Keth emlékkertjében a kethri fiatalok nevei állnak; ők már a felszínen nőnek fel."},
        {"if_flag": "keth_harcosok", "text": "Keth harcosai őrzik a Visszhang-kaput; a fiatalok, akiket hátrahagyott, a Kórusban maradtak."},
        {"if_flag": "f_civil", "text": "A híd: a parton maradt civilek élnek, és az út első kövét ők rakták le."},
        {"if_flag": "f_ostrom", "text": "A híd: az ostrom gyors volt. A parton emlékkő áll azoknak, akiket nem vártak meg."}]},
}


def apply_decisions():
    by_id = {m["id"]: m for _, ms in CAMPAIGNS for m in ms}
    for key, dec in DECISIONS.items():
        m = by_id[key.split("#")[0]]
        drop = dec.get("drop_triggers_containing")
        if drop:
            m["triggers"] = [t for t in m["triggers"] if drop not in t.get("message", "")]
        for uid in dec.get("remove_player_units", []):
            m["player"].get("units", {}).pop(uid, None)
        if dec.get("remove_fail"):
            m["fail_if_lost"] = [f for f in m["fail_if_lost"] if f["id"] not in dec["remove_fail"]]
        m["triggers"] = dec.get("triggers", []) + m["triggers"]
        if dec.get("victory_variants"):
            m["victory_variants"] = dec["victory_variants"]


def main():
    apply_decisions()
    for old in ("meteorhullas", "holdfalka", "fonal", "ferrum"):  # a korábbi, nem kánon változatok
        shutil.rmtree(os.path.join(ROOT, old), ignore_errors=True)
    total = 0

    def no_food(o):  # 3 nyersanyagos gazdaság: az ételértékek fába és kőbe váltanak át
        if isinstance(o, dict):
            if isinstance(o.get("food"), (int, float)):
                f = o.pop("food")
                for k, part in (("wood", 0.65), ("stone", 0.35)):
                    o[k] = o.get(k, 0) + int(round(f * part / 5.0)) * 5
            for v in o.values():
                no_food(v)
        elif isinstance(o, list):
            for v in o:
                no_food(v)
    for _, missions in CAMPAIGNS:
        no_food(missions)
    for ci, (camp, missions) in enumerate(CAMPAIGNS):
        # a hősök legalább ezen a szinten kezdenek (a profilban tárolt fejlődés ennél magasabb is lehet);
        # a fejezetek 2. küldetésétől szörnytáborok is vannak – tapasztalat és tárgyak a hősöknek
        final = ci == len(CAMPAIGNS) - 1
        for i, m in enumerate(missions):
            m["hero_level"] = 6 + i if final else (3 if len(missions) == 1 and ci else 1 + i)
            m["enemy_hero_level"] = m["hero_level"]
            if len(missions) > 1 and i >= 1 and not final:
                m["creeps"] = True
        d = os.path.join(ROOT, camp["id"])
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
        with open(os.path.join(d, "campaign.json"), "w", encoding="utf-8") as f:
            json.dump(camp, f, ensure_ascii=False, indent=2)
        for m in missions:
            with open(os.path.join(d, m["id"] + ".json"), "w", encoding="utf-8") as f:
                json.dump(m, f, ensure_ascii=False, indent=2)
            total += 1
    print(f"{len(CAMPAIGNS)} hadjárat, {total} küldetés")


if __name__ == "__main__":
    main()
