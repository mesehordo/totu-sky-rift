"""A novellák önálló pályái: novellánként egy pálya, külön menüből (Egyjátékos → Novellák).

    python tools/novella_kuldetesek.py       -> novellak/<id>.json + novellak/index.json

Forrás: docs/tortenet/story/edited/novellak (ha van), különben drafts/novellak. Minden pálya a novella egy
összefüggő helyszínén, egy fő konfliktusára épül; a főszereplő névvel ellátott közönséges egység (a név nem ad
hősi tulajdonságot). A csendes novellákból nem harcos, hanem felfedező / gondoskodó / időre menő pálya lesz.
A „canon” mező jelzi, mi a novellából adaptált esemény és mi a játékhoz hozzáadott.
"""
import json
import zlib
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kuldetesek_atdolgozas import make_layout  # noqa: E402

OUT = os.path.join(ROOT, "novellak")
PEOPLE = {"01": ("elves", "Szürke elfek"), "02": ("ursan", "Ursanok"), "03": ("wullof", "Wullof"),
          "04": ("imperium", "Vasbirodalom"), "05": ("croftian", "Kethrik"), "06": ("ooni", "Ooni"),
          "07": ("hukucha", "Huk’ucha"), "08": ("mycel", "Micéliák"), "09": ("ursan", "Pinguu"),
          "10": ("ursan", "Ghoblin"), "11": ("imperium", "Mhonyo"), "12": ("elves", "Daresh"),
          "13": ("elves", "Shrissh-matah"), "14": ("ursan", "Tokla"), "15": ("mycel", "Galóca"),
          "16": ("elves", "Niil-elfek"), "17": ("elves", "Tűz-elfek"), "18": ("wullof", "Tendüür"),
          "19": ("elves", "Nila-uchana"), "20": ("ooni", "Zhan-zhama")}
THEME = {"09": "winter", "14": "winter", "02": "winter", "17": "wasteland", "13": "wasteland", "12": "wasteland",
         "07": "swamp", "16": "swamp", "19": "swamp", "03": "autumn"}

M = []


def L(who, text, portrait=""):
    return {"who": who, "text": text, "portrait": portrait}


def reach(i, at, text, after=None, units=None, tag=None, r=3, marker=None, optional=False, hidden=False):
    o = {"id": i, "type": "reach", "at": at, "radius": r, "text": text, "marker": marker or ""}
    if after:
        o["after"] = after
    if units:
        o["units"] = units
    if tag:
        o["tag"] = tag
    if optional:
        o["optional"] = True
    if hidden:
        o["hidden"] = True
    return o


def guard(i, at, sec, text, after=None, r=2, marker=None, optional=False):
    o = {"id": i, "type": "guard", "at": at, "radius": r, "seconds": sec, "text": text, "marker": marker or ""}
    if after:
        o["after"] = after
    if optional:
        o["optional"] = True
    return o


def escort(i, tag, at, text, after=None, mn=1, r=5, marker=None, hidden=False):
    o = {"id": i, "type": "escort", "tag": tag, "min": mn, "at": at, "radius": r, "text": text, "marker": marker or ""}
    if after:
        o["after"] = after
    if hidden:
        o["hidden"] = True
    return o


def survive(i, sec, text, after=None):
    o = {"id": i, "type": "survive", "seconds": sec, "text": text}
    if after:
        o["after"] = after
    return o


def deadline(t, obj, text):
    return {"at": t, "unless_done": obj, "fail": text}


def wave(i, near, to, units, every=60, delay=None, at=0, until=None, maxn=None, if_done=None):
    t = {"id": i, "every": every, "at": at, "spawn": {"owner": 1, "near": near, "to": to, "units": units}}
    for k, v in (("delay", delay), ("until", until), ("max", maxn), ("if_done", if_done)):
        if v is not None:
            t[k] = v
    return t


def nov(nid, title, pts, roads, units, objectives, intro, outro, victory, *, style="erdo", trig=(), fail=(), enemy=None,
        allies=None, layout_extra=None, canon_ada="", canon_uj=""):
    people = nid[4:6]
    fac, pname = PEOPLE[people]
    starts = [k for k in pts][:1] + [list(pts)[-1]] + ([list(pts)[1]] if allies else [])
    kw = dict(clumps=9)
    if style == "terasz":
        kw.update(ridges=[[[[6, 20], [58, 20]], 1], [[[6, 32], [58, 32]], 1]], clumps=5)
    elif style == "fjord":
        kw.update(water=[[[60, 10], 10], [[62, 40], 8]], clumps=7)
    elif style == "varos":
        kw.update(clumps=3)
    elif style == "barlang":
        kw.update(rocks=[[[14, 14], 6], [[50, 14], 6], [[16, 44], 5], [[48, 42], 6], [[32, 28], 3]], clumps=2)
    elif style == "hago":
        kw.update(ridges=[[[[0, 26], [26, 26]], 3], [[[38, 26], [64, 26]], 3]], clumps=5)
    elif style == "to":
        kw.update(water=[[[18, 18], 4], [[46, 18], 4], [[32, 40], 4]], clumps=6)
    elif style == "szurdok":
        kw.update(ridges=[[[[10, 4], [16, 50]], 3], [[[54, 4], [48, 50]], 3]], clumps=4)
    elif style == "jeg":
        kw.update(water=[[[64, 28], 14]], clumps=3)
    elif style == "dune":
        kw.update(rocks=[[[20, 14], 3], [[46, 40], 3]], clumps=2)
    elif style == "folyo":
        kw.update(river={"line": [[-1, 28], [20, 26], [44, 30], [65, 28]], "w": 4}, fords=[[[32, 22], [32, 34]]], clumps=6)
    elif style == "erdo":
        kw.update(clumps=16)
    if layout_extra:
        kw.update(layout_extra)
    lay = make_layout(zlib.crc32(nid.encode()) % 9000 + 100, [64, 56], pts, starts, roads=roads, **kw)
    lay["forest"][0] = {"border": 3}
    m = {"id": nid, "code": "NOV-" + nid[4:].upper(), "title": title, "faction": fac, "novella": True, "people": pname,
         "theme": THEME.get(people), "source": f"docs/tortenet/story/…/novellak/{people}_*.md – „{title}”",
         "canon": {"rogzitett": "A novella szereplői, helyszíne, ideje és fő eseménye.",
                   "adaptacio": canon_ada or "A novella fő konfliktusa egyetlen helyszínen, játszható feladatként.",
                   "uj_javaslat": canon_uj or "A pálya helyei, a feladat időkorlátja és a kísérő mellékszereplők."},
         "briefing": [b for b in intro.get("brief", [])],
         "layout": lay, "player": {"base": False, "resources": {"wood": 0, "stone": 0, "meteor": 0}, "units": units},
         "enemies": [enemy or {"faction": "croftian", "name": "–", "base": False, "ai": False, "units": []}],
         "objectives": objectives,
         "triggers": [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": intro["shots"]}}] + list(trig) +
                     [{"id": "zaras", "on_objective": next(o["id"] for o in reversed(objectives) if not o.get("optional")),
                       "cinematic": {"shots": outro}}],
         "fail_if_lost": list(fail), "victory": victory, "hero_level": 1, "enemy_hero_level": 1}
    if allies:
        m["allies"] = allies
    M.append(m)


def shot(cam, *lines):
    return {"camera": cam, "say": list(lines)}


def ally(fac, name, units):
    return [{"faction": fac, "name": name, "units": units}]


def crof(units, name="Néma raj"):
    return {"faction": "croftian", "name": name, "base": False, "ai": False, "units": units}


def U(i, at, name=None, tag=None, n=1):
    d = {"id": i, "at": at, "n": n}
    if name:
        d["name"] = name
    if tag:
        d["tag"] = tag
    return d


# ===================================================================== 01 Szürke elfek
P = "elf_kofarago"
nov("nov_01_1", "A repedt lépcső",
    {"lak": [32, 50], "lepcso1": [24, 38], "lepcso2": [40, 38], "lepcso3": [24, 26], "lepcso4": [40, 26], "kovesmester": [32, 12], "_": [60, 4]},
    [("lak", "lepcso1"), ("lepcso1", "lepcso2"), ("lepcso2", "lepcso4"), ("lepcso4", "lepcso3"), ("lepcso3", "kovesmester")],
    [U(P, "lak", "Ameyal", "ameyal")],
    [guard("f1", "lepcso1", 3, "Kopogtasd végig az alsó fokokat (bal)"), guard("f2", "lepcso2", 3, "Kopogtasd végig az alsó fokokat (jobb)"),
     guard("f3", "lepcso3", 3, "Kopogtasd végig a felső fokokat (bal)"), guard("f4", "lepcso4", 3, "Kopogtasd végig a felső fokokat (jobb)"),
     reach("jelent", "kovesmester", "Szólj a kőművesmesternek, mielőtt a tanács vizsgálója odaér", after=["f1", "f2", "f3", "f4"], marker="Kőművesmester")],
    {"brief": ["A 44. év, Tlaxoc, a Kilenc Lépcső déli ága. Ameyal családja négy nemzedék óta söpri a hatodik fordulót.",
               "Ma reggel repedést látott a köveken. Kopogtasd végig a fordulót, és szólj a kőművesmesternek, mielőtt a vizsgáló megérkezik."],
     "shots": [shot("lepcso2", L("Ameyal", "Kilencvenhat fok. Az egyik hangja más, mint tegnap.", "elf_kofarago"))]},
    [shot("lepcso3", L("Ameyal", "Holnaptól söprés előtt végigkopogtatom. Mind a kilencvenhatot.", "elf_kofarago"))],
    "A repedést időben jelentették. Ameyal azóta minden reggel végigkopogtatja a fordulót.",
    style="terasz", trig=[deadline(240, "jelent", "A tanács vizsgálója előbb ért a hatodik fordulóhoz.")],
    fail=[{"tag": "ameyal", "text": "Ameyal megsérült."}])

nov("nov_01_2", "A raptor, aki nem akart megjelenni",
    {"karam": [10, 46], "obeliszk1": [24, 34], "obeliszk2": [40, 24], "ocelotl": [16, 20], "szemle": [54, 10], "_": [60, 50]},
    [("karam", "obeliszk1"), ("obeliszk1", "obeliszk2"), ("obeliszk2", "szemle"), ("karam", "ocelotl")],
    [U("elf_raptor_lovas", "karam", "Csontos", "csontos"), U(P, "karam", "Tlilo", "tlilo")],
    [reach("ocelotl", "ocelotl", "Kérdezd meg Ocelotlt, a régi gondozót, miért nem akar Csontos megjelenni", tag="tlilo", marker="Ocelotl"),
     reach("o1", "obeliszk1", "Vezesd Csontost gyalog az első obeliszkig", after="ocelotl", tag="csontos", marker="Obeliszk"),
     reach("o2", "obeliszk2", "Tovább a második obeliszkig – gyalog, ahogy fiatalon járt", after="o1", tag="csontos", marker="Obeliszk"),
     reach("szemle", "szemle", "Érj be Csontossal a szemlére, a parancsnok elé", after="o2", tag="csontos", marker="Szemle")],
    {"brief": ["A 61. év, Napobeliszk-mező Huitzil határában. Csontos harmincegy éve szolgál; nem akar nyugdíjba menni, és nem hajlandó beidéződni.",
               "Tlilo a szemlére viszi – gyalog, ahogy az öreg raptor egykor járt."],
     "shots": [shot("karam", L("Tlilo", "Ha nem akarsz megjelenni, odasétálunk. Hosszú út, Csontos.", P))]},
    [shot("szemle", L("Tlilo", "Ott van. Gyalog jött. És megharapta a sisaktollát.", P))],
    "Csontos gyalog érkezett a szemlére. A parancsnok nevetett, és a tolla odalett.",
    style="erdo", fail=[{"tag": "csontos", "text": "Csontos elesett."}])

nov("nov_01_3", "A harmadik éjszaka",
    {"archivum": [32, 46], "kapu_ny": [6, 26], "kapu_e": [32, 4], "kapu_k": [58, 26], "rev_ut": [60, 50], "_": [2, 2]},
    [("archivum", "kapu_ny"), ("archivum", "kapu_e"), ("archivum", "kapu_k"), ("archivum", "rev_ut")],
    [U(P, "archivum", "Citlali", "citlali"), U("elf_raptor_lovas", "archivum", "Hírvivő", "hirvivo", 3)],
    [reach("l1", "kapu_ny", "Vidd el a tanács leveleit a nyugati kapuhoz", tag="hirvivo", marker="Nyugati kapu"),
     reach("l2", "kapu_e", "Vidd el a leveleket az északi kapuhoz", tag="hirvivo", marker="Északi kapu"),
     reach("l3", "kapu_k", "Vidd el a leveleket a keleti kapuhoz", tag="hirvivo", marker="Keleti kapu"),
     reach("l13", "rev_ut", "(A tizenharmadik levél) Küldd el Kettősrév felé is", tag="hirvivo", marker="Út Kettősrév felé", optional=True),
     reach("vissza", "archivum", "Citlali térjen vissza az archívumba a hajnal előtt", after=["l1", "l2", "l3"], tag="citlali", marker="Archívum")],
    {"brief": ["A 0. év, Kilenc Lépcső, a harmadik éjszaka. A második nap fent van, és darabokra hullhat.",
               "Citlali, archívumi inas, a tanács tizenkét levelét viszi a kapukhoz – és van egy tizenharmadik is, amelyet senki nem rendelt."],
     "shots": [shot("archivum", L("Citlali", "Vigyétek a népet a templomok alatti termekbe. Ennyi áll mindegyikben.", P))]},
    [shot("archivum", L("Citlali", "Soha nem tudom meg, odaért-e. De elküldtem.", P))],
    "Hajnalban a második nap széttört. A fennsíkra kevés kő esett.",
    style="terasz",
    trig=[{"at": 90, "message": "Lehullott egy kő – az északi út beomlott!", "color": [255, 160, 110],
           "grant": {"tag": "hirvivo", "abilities": ["vagta"], "text": "A raptorok megérzik a veszélyt: Vágta (képesség)."},
           "say": [L("Citlali", "Kerüljetek! És most már ne kíméljétek a raptorokat.", P)],
           "reshape": {"paint": [{"t": "rock", "area": [[32, 16], 2]}], "clear": [{"t": "dirt", "line": [[30, 22], [26, 14], [32, 6]], "w": 2}]}},
          deadline(260, "vissza", "Felkelt a nap – Citlali nem ért vissza az archívumba.")],
    fail=[{"tag": "citlali", "text": "Citlali megsérült."}], canon_uj="A beomló út a lehulló kövek miatt.")

# ===================================================================== 02 Ursanok
nov("nov_02_1", "A gabonakíséret",
    {"jarlsag": [32, 4], "tabor": [20, 26], "rev": [32, 52], "szoros": [40, 38], "_": [60, 30]},
    [("jarlsag", "tabor"), ("tabor", "szoros"), ("szoros", "rev")],
    [U("urs_hos_ragna", "jarlsag", "Sigvor", "sigvor"), U("urs_mammut", "jarlsag", "Gabonaszán", "gabona", 3), U("urs_pajzshordozo", "jarlsag", None, None, 4)],
    [reach("tabor", "tabor", "Érj a menekülttáborhoz az úton", tag="gabona", marker="Tábor"),
     escort("rev", "gabona", "rev", "Vidd le a gabonaszánokat a déli révig", after="tabor", mn=2, marker="Déli rév")],
    {"brief": ["Az 58. év, a déli út. Sigvor az Esküfa alatt esküdött: a gabonából útközben senkinek nem ad.",
               "Az úton Wullof menekültek táboroznak. Az eskü és az éhség ugyanazon az úton találkozik."],
     "shots": [shot("tabor", L("Sigvor", "Negyven szán gabona, egy eskü. És egy tábor az út mellett.", "urs_hos_ragna"))]},
    [shot("rev", L("Wullof útbíró", "Mi is tudunk esküt tenni.", "wul_gyujtogeto"))],
    "A gabona leért a révre. Az útbíró meghajolt, egész testtel, ahogy a Wullofok szoktak.",
    style="hago",
    trig=[{"id": "dontes", "on_objective": "tabor", "choice": {"title": "Az eskü", "text": "A táborban éhező Wullof családok. A szánokon a jarl gabonája.",
                                                               "options": [{"label": "Az eskü szerint továbbmegyünk", "flag": "nov021_eskü"},
                                                                           {"label": "Egy szánt itt hagyok – az eskümet a magam terhére szegem meg", "flag": "nov021_szan"}]}},
          wave("rabol", "_", "szoros", [{"id": "urs_berserker", "n": 3, "name": "Éhes rabló"}], every=70, delay=10, if_done="tabor", maxn=2)],
    enemy={"faction": "ursan", "name": "Rablók", "base": False, "ai": False, "units": []},
    fail=[{"tag": "sigvor", "text": "Sigvor elesett."}], canon_uj="A rablók rajtaütése; a döntés jelzője (csak a novellában).")

nov("nov_02_2", "A mamut, akinek magyarázat jár",
    {"karam": [14, 44], "repafold": [30, 30], "fjordpart": [50, 16], "fenyves": [20, 12], "_": [2, 2]},
    [("karam", "repafold"), ("repafold", "fjordpart"), ("fjordpart", "fenyves"), ("fenyves", "karam")],
    [U("urs_mammut", "karam", "Bundás", "bundas"), U("urs_telepes", "karam", "Ylva", "ylva")],
    [reach("repa", "repafold", "Vidd el Bundást a répaföldre", tag="bundas", marker="Répaföld"),
     reach("fjord", "fjordpart", "Aztán a fjordpartra, ahol fiatalon fürdött", tag="bundas", after="repa", marker="Fjordpart"),
     reach("fenyo", "fenyves", "A fenyveshez, ahol a nagyapád megtalálta", tag="bundas", after="fjord", marker="Fenyves"),
     reach("haza", "karam", "Ylva vezesse haza a karámba", tag="ylva", after="fenyo", marker="Karám")],
    {"brief": ["A 66. év, Fjordvár. Az öreg Bundást el kell altatni, és Ylvának kell megírnia a magyarázatot.",
               "Előtte még egyszer végigjárják a helyeit."],
     "shots": [shot("karam", L("Ylva", "Miért nekem kell megírnom? …Jó. Előbb sétálunk egyet.", "urs_telepes"))]},
    [shot("karam", L("Ylva", "Az utolsó sor: a sárgarépát mindig előbb ette meg.", "urs_telepes"))],
    "Ylva megírta a magyarázatot. Tizenegy sor, és mindegyik igaz.", style="fjord")

nov("nov_02_3", "Riadó dagasztás közben",
    {"sutohaz": [32, 34], "deli_kapu": [32, 52], "esku_fa": [20, 24], "eszak": [32, 4], "_": [60, 4]},
    [("sutohaz", "deli_kapu"), ("sutohaz", "esku_fa"), ("sutohaz", "eszak")],
    [U("urs_telepes", "sutohaz", "Gunnhild", "gunnhild"), U("urs_pajzshordozo", "sutohaz", None, None, 5), U("urs_szigonyvezo", "sutohaz", None, None, 3)],
    [guard("tészta", "sutohaz", 30, "Gunnhild dagasszon tovább a sütőházban", marker="Sütőház"),
     survive("riado", 150, "Tartsd a falut, amíg a riadó tart")],
    {"brief": ["A 45. év, Fjordvár sütőháza. Megkondult az Esküfa: riadó. Gunnhild keze a tésztában van.",
               "A telepeseknek fegyvert kellene ragadniuk. A kenyérnek viszont dagasztás kell."],
     "shots": [shot("sutohaz", L("Gunnhild", "Hány telepes van a déli kapunál? Elég. Én dagasztok.", "urs_telepes"))]},
    [shot("sutohaz", L("Parancsnok", "Legközelebb fegyvert ragadsz.", "urs_pajzshordozo"), L("Gunnhild", "Rendben.", "urs_telepes"))],
    "Másnap a falu minden háza kapott egy kenyeret. A parancsnoké volt a legnagyobb.", style="erdo",
    trig=[wave("raid", "eszak", "sutohaz", [{"id": "urs_berserker", "n": 2, "name": "Portyázó"}], every=45, delay=30, until="riado")],
    enemy={"faction": "ursan", "name": "Portyázók", "base": False, "ai": False, "units": []},
    fail=[{"tag": "gunnhild", "text": "Gunnhild megsérült – a kenyér odaégett."}])

# ===================================================================== 03 Wullof
nov("nov_03_1", "Áradás két szertartással",
    {"kelet": [44, 36], "nyugat": [20, 36], "felso": [32, 6], "rev": [32, 30], "_": [62, 54]},
    [("kelet", "rev"), ("nyugat", "rev"), ("rev", "felso")],
    [U("wul_gyujtogeto", "kelet", "Bilari", "bilari"), U("wul_holdsaman", "nyugat", "Ngurru", "ngurru")],
    [escort("k", "keleti", "felso", "Vezesd fel a keleti part családjait a felső teraszra", mn=3, marker="Felső terasz"),
     escort("ny", "nyugati", "felso", "Vezesd fel a nyugati part családjait is", mn=3, marker="Felső terasz")],
    {"brief": ["Az 52. év, Kettősrév. Két hagyomány, két part – és egy áradás, amely gyorsabban jön, mint más években.",
               "Vezesd fel mindkét part családjait a felső teraszra, mielőtt a víz elönti a révet."],
     "shots": [shot("rev", L("Bilari", "A víz már a rév harmadik fokán áll.", "wul_gyujtogeto"), L("Ngurru", "Akkor a szertartás várhat.", "wul_holdsaman"))]},
    [shot("felso", L("Ngurru", "Jövőre a kőkört a felső teraszon rakom.", "wul_holdsaman"), L("Bilari", "A kosaram meg alá kerül.", "wul_gyujtogeto"))],
    "Mindkét part családjai a felső teraszon nézték, ahogy a holdfény a vízre esik.", style="folyo",
    allies=ally("wullof", "Kettősrév", [U("wul_gyujtogeto", "kelet", None, "keleti", 4), U("wul_gyujtogeto", "nyugat", None, "nyugati", 4)]),
    trig=[{"on_enter": {"at": "kelet", "r": 5, "tag": "bilari"}, "join": "keleti"}, {"on_enter": {"at": "nyugat", "r": 5, "tag": "ngurru"}, "join": "nyugati"},
          {"at": 70, "message": "Árad a víz – az alsó teraszok elöntve.", "color": [140, 200, 255],
           "reshape": {"paint": [{"t": "water", "line": [[-1, 30], [20, 30], [44, 32], [65, 30]], "w": 8}],
                       "clear": [{"t": "dirt", "line": [[32, 20], [32, 40]], "w": 3}]}},
          {"at": 150, "message": "A víz tovább emelkedik!", "color": [140, 200, 255],
           "reshape": {"paint": [{"t": "water", "line": [[-1, 34], [20, 36], [44, 38], [65, 36]], "w": 10}],
                       "clear": [{"t": "dirt", "line": [[32, 18], [32, 46]], "w": 3}]}}],
    fail=[{"tag": "keleti", "text": "A keleti part családjai odavesztek."}, {"tag": "nyugati", "text": "A nyugati part családjai odavesztek."}])

nov("nov_03_2", "A Groomi köpése",
    {"tabor": [12, 44], "voros_szikla": [50, 12], "itato": [30, 30], "_": [60, 50]},
    [("tabor", "itato"), ("itato", "voros_szikla")],
    [U("wul_gyujtogeto", "tabor", "Djarra", "djarra"), U("wul_groomi_fioka", "tabor", "Piros", "piros")],
    [reach("itato", "itato", "Itasd meg Pirost az itatónál", tag="piros", marker="Itató"),
     reach("feszek", "voros_szikla", "Vidd Pirost a vörös szikla tövéhez, ahol a tojást találtad", tag="piros", after="itato", marker="Vörös szikla")],
    {"brief": ["A 69–70. év, Vörösszikla vadásztábora. A Groomi-fióka Djarra bundájának zsebében kelt ki.",
               "A család minden este veszekszik. Djarrának választania kell."],
     "shots": [shot("tabor", L("Djarra", "Piros, ne köpj. Kérlek. Legalább ma ne.", "wul_gyujtogeto"))]},
    [shot("voros_szikla", L("Djarra", "Nem tudom, jól választok-e. Csak azt tudom, hogy választok.", "wul_gyujtogeto"))],
    "Djarra választott. A vörös szikla tövében a fészek azóta sem üres.", style="dune",
    trig=[{"on_objective": "feszek", "choice": {"title": "Piros", "text": "A szabály szerint a vadon fiókája a vadoné. Piros a zsebedbe bújik.",
                                                "options": [{"label": "Itt hagyom a fészeknél", "flag": "nov032_vadon"},
                                                            {"label": "Hazaviszem, és vállalom a veszekedést", "flag": "nov032_haza"}]}}],
    fail=[{"tag": "piros", "text": "Piros megsérült."}])

nov("nov_03_3", "Holt út",
    {"falu": [8, 48], "jel1": [20, 36], "jel2": [30, 26], "perem": [36, 20], "tulso": [54, 20], "_": [60, 52]},
    [("falu", "jel1"), ("jel1", "jel2"), ("jel2", "perem")],
    [U("wul_gyujtogeto", "falu", "Ninnu, útbíró", "ninnu")],
    [reach("j1", "jel1", "Kövesd a holt út első jelét", tag="ninnu", marker="Ének-jel"),
     reach("j2", "jel2", "Kövesd a második jelet", tag="ninnu", after="j1", marker="Ének-jel"),
     reach("perem", "perem", "Érj a Peremjáró szélére", tag="ninnu", after="j2", marker="Perem"),
     reach("at", "tulso", "Kelj át a hídon, amíg varratfényben látszik", tag="ninnu", after="perem", marker="Túlsó perem"),
     reach("haza", "falu", "Vidd haza a jelet a határkőhöz", tag="ninnu", after="at", marker="Határkő")],
    {"brief": ["A 69. év, a Vörös Hát pereme. Az ének szerint a következő lépésnél le kellene zuhanni.",
               "Ninnu huszonhárom holt utat ismer. Ma éjjel varratfény van."],
     "shots": [shot("perem", L("Ninnu", "Itt a kőhíd, a fehér, a keskeny… Nincs itt semmi. Még.", "wul_gyujtogeto"))]},
    [shot("falu", L("Ninnu", "Aki erre jár, ne lépjen, amíg a híd villog.", "wul_gyujtogeto"))],
    "A határkőre új jel került: a Peremjáró, a híd, és mellé egy nyitott félkör, benne egy ponttal.", style="dune",
    layout_extra={"ridges": [[[[42, 0], [42, 56]], 6]]},
    trig=[{"on_objective": "perem", "message": "Varratfény! A szakadék fölött kőhíd rajzolódik ki.", "color": [180, 220, 255],
           "grant": {"tag": "ninnu", "abilities": ["vandor_enek"], "text": "Ninnu a holt út énekét dúdolja: Vándorének (képesség)."},
           "say": [L("Ninnu", "Az ének nem tévedett. Csak korán szólt.", "wul_gyujtogeto")],
           "reshape": {"clear": [{"t": "grass", "line": [[36, 20], [54, 20]], "w": 2}]}},
          {"on_objective": "at", "message": "A híd elhalványul mögötted.", "color": [180, 220, 255],
           "reshape": {"paint": [{"t": "rock", "line": [[40, 20], [44, 20]], "w": 2}],
                       "clear": [{"t": "dirt", "line": [[54, 20], [56, 40], [40, 50], [20, 50], [8, 48]], "w": 2}]}}],
    fail=[{"tag": "ninnu", "text": "Ninnu lezuhant."}])

# ===================================================================== 04 Vasbirodalom
nov("nov_04_1", "Az üres serlegek",
    {"konyha": [10, 30], "asztal1": [26, 20], "asztal2": [38, 20], "asztal3": [26, 38], "asztal4": [38, 38], "fohely": [52, 28], "_": [60, 52]},
    [("konyha", "asztal1"), ("asztal1", "asztal2"), ("asztal2", "fohely"), ("konyha", "asztal3"), ("asztal3", "asztal4"), ("asztal4", "fohely")],
    [U("imp_legios", "konyha", "Marcus Tullo, pincér", "marcus")],
    [guard("a1", "asztal1", 3, "Tölts az első asztalnál", marker="Asztal"), guard("a2", "asztal2", 3, "Tölts a második asztalnál", after="a1", marker="Asztal"),
     guard("a3", "asztal3", 3, "Tölts a harmadik asztalnál", after="a2", marker="Asztal"), guard("a4", "asztal4", 3, "Tölts a negyedik asztalnál", after="a3", marker="Asztal"),
     guard("fo", "fohely", 4, "Tölts az öreg jogásznak a főhelyen", after="a4", marker="Főhely")],
    {"brief": ["Az 51. év, Aureth, a Callidus-ház fogadóterme. Marcus harminckét éve tölt üres serlegekbe semmit, és jól csinálja.",
               "Ma este minden asztalnál tölteni kell – a vendég elhiszi, ha jól csinálod."],
     "shots": [shot("fohely", L("Marcus", "Egy tenyérnyi magasról, lassan, és vissza, amikor képzeletben megtelt.", "imp_legios"))]},
    [shot("fohely", L("Öreg jogász", "Mi nem tudunk inni. De emlékszünk rá. Az egészségedre.", "imp_szellemidezo"))],
    "Marcus hazavitte a maradék bort. Reggelre megecetesedett, és az illata napokig betöltötte a szobát.", style="varos",
    trig=[deadline(150, "fo", "A vendégek észrevették, hogy a serlegek üresek maradtak.")],
    layout_extra={"decor": [{"id": "imp_forum", "at": [52, 10]}, {"id": "imp_legiogyar", "at": [12, 10], "ruin": False}]})

nov("nov_04_2", "Ébresztés",
    {"raktar": [10, 12], "reaktor": [50, 40], "lista": [44, 34], "haz": [10, 48], "_": [60, 4]},
    [("raktar", "reaktor"), ("reaktor", "haz")],
    [U("imp_fabrikator", "reaktor", "Lucia Sabin", "lucia"), U("imp_fabrikator", "raktar", "Horgonyszállító", "horgony", 3)],
    [reach("lista", "lista", "Olvasd el a bronztáblát a reaktorcsarnok bejáratánál", tag="lucia", marker="Lista"),
     escort("horgonyok", "horgony", "reaktor", "Hozd át a negyven horgonyt a nyilvántartás raktárából", after="lista", mn=3, marker="Reaktor"),
     reach("haza", "haz", "Lucia menjen haza", tag="lucia", after="horgonyok", marker="Otthon")],
    {"brief": ["A 64. év, Aureth, a Pons Ferri negyed új Lélekreaktora. Negyven horgonyt ébresztenek fel ma, a régi rend szerint.",
               "Lucia nagyanyjának neve megint nincs a listán."],
     "shots": [shot("lista", L("Lucia", "Harmadszor olvasom el. Hátha rosszul láttam.", "imp_fabrikator"))]},
    [shot("haz", L("Lucia", "Hét lista áll már a polcon. Ez a nyolcadik.", "imp_fabrikator"))],
    "A negyven horgony felébredt. Lucia a polcra tette az aznapi listát, a többi mellé.", style="varos",
    trig=[{"on_objective": "lista", "choice": {"title": "A lista", "text": "A nyilvántartás jelölte ki a neveket. Egy hely cseréje nem látszana.",
                                               "options": [{"label": "A lista szerint", "flag": "nov042_lista"},
                                                           {"label": "Nem nyúlok hozzá – de kérvényt írok", "flag": "nov042_kerveny"}]}}],
    layout_extra={"decor": [{"id": "imp_reaktor", "at": [54, 46]}, {"id": "imp_ontode", "at": [16, 18], "ruin": True}]})

nov("nov_04_3", "Levelek Veternába",
    {"lakas": [10, 46], "hid": [24, 34], "piac": [40, 30], "szinhaz": [30, 14], "templom": [54, 10], "_": [60, 52]},
    [("lakas", "hid"), ("hid", "piac"), ("piac", "szinhaz"), ("szinhaz", "templom")],
    [U("imp_legios", "lakas", "Aulus", "aulus")],
    [reach("hid", "hid", "A híd, ahol a 12. évben az első levelet írta", tag="aulus", marker="Híd"),
     reach("piac", "piac", "A piac, ahol a pékség illata a régi otthont idézte", tag="aulus", after="hid", marker="Piac"),
     reach("szinhaz", "szinhaz", "A színház, ahová Veternával jártak", tag="aulus", after="piac", marker="Színház"),
     reach("templom", "templom", "Vidd a levelesládát Umbrax templomába", tag="aulus", after="szinhaz", marker="Umbrax temploma")],
    {"brief": ["Egy géptestű aurethi levelei a 12. és a 72. év között. A leveleket soha nem küldték el.",
               "Aulus még egyszer végigjárja a helyeket, amelyekről írt."],
     "shots": [shot("lakas", L("Aulus", "Veterna, nem tudok erről írni. Csak le akarom írni, hogy valaki tudja.", "imp_legios"))]},
    [shot("templom", L("Aulus", "Ha nem találod meg őket: semmi baj. Én megtaláltalak.", "imp_legios"))],
    "A ládát Umbrax templomában találták meg, a férfi halála után.", style="varos")

# ===================================================================== 05 Kethrik
nov("nov_05_1", "A semleges illat",
    {"bejarat": [32, 50], "pad1": [20, 30], "pad2": [44, 30], "kozep": [32, 24], "vizsgalo": [32, 6], "_": [60, 4]},
    [("bejarat", "kozep"), ("kozep", "pad1"), ("kozep", "pad2"), ("kozep", "vizsgalo")],
    [U("cro_csapszolga", "bejarat", "Vessa", "vessa")],
    [guard("p1", "pad1", 8, "Faragd ki a jelkő első felét", marker="Vésőpad"), guard("p2", "pad2", 8, "Faragd át a kölcsön jelét – a kő ne a tiéd legyen", after="p1", marker="Vésőpad"),
     reach("bead", "vizsgalo", "Add be a követ a vizsgáztatóknak", tag="vessa", after="p2", marker="Vizsgáztatók")],
    {"brief": ["A 28. év, Ithrass tanácsvárosa, a vizsgakamra. A semleges illat elnyomja a jelöltek saját illatát.",
               "Vessa köve még a saját megvetését hordja. Át kell faragnia, mielőtt a füstölő kiég."],
     "shots": [shot("kozep", L("Vessa", "A kő nem hazudhat. De nem is lehet az enyém.", "cro_csapszolga"))]},
    [shot("vizsgalo", L("Vessa", "Hazamegyek Qethrassba. És jövőre visszajövök.", "cro_csapszolga"))],
    "Vessa háromszor szagolta végig a feljegyzést. A következő évben visszajött.", style="barlang",
    trig=[deadline(120, "bead", "A semleges illat kiégett – a kő Vessa illatát hordja.")])

nov("nov_05_2", "Letét",
    {"kapu": [32, 4], "szint1": [18, 18], "szint2": [44, 30], "szint3": [24, 44], "letet": [40, 50], "_": [60, 54]},
    [("kapu", "szint1"), ("szint1", "szint2"), ("szint2", "szint3"), ("szint3", "letet")],
    [U("oon_nian_or", "kapu", "Suzu, könyvtáros", "suzu")],
    [escort("le", "arrth", "letet", "Vezesd le Arrth-t a legalsó, leghidegebb szintre", marker="Letétek"),
     guard("letet", "letet", 6, "Várd meg, amíg Arrth elhelyezi a letétet", after="le", marker="Letétek")],
    {"brief": ["A 49. év, Hét Lámpás kapuváros, a kaputartók könyvtára. A kethri letéteket a legalsó szinten tartják.",
               "Suzu, az öreg nian, levezeti Arrth-t, a kethri vendéget."],
     "shots": [shot("kapu", L("Suzu", "Lassan. A bal hátsó lábam nem szereti a lépcsőt. A kövek meg nem szeretik a sietést.", "oon_nian_or"))]},
    [shot("letet", L("Suzu", "Egy üres kő a névtelen neveknek. Nem tudom, jól tesszük-e.", "oon_nian_or"))],
    "Arrth letétje a legalsó szinten maradt. Suzu tudta, hová tartozik.", style="barlang",
    allies=ally("croftian", "Vendég", [U("cro_csapszolga", "kapu", "Arrth", "arrth")]),
    trig=[{"on_enter": {"at": "kapu", "r": 4, "tag": "suzu"}, "join": "arrth"}],
    fail=[{"tag": "arrth", "text": "Arrth megsérült."}])

nov("nov_05_3", "A hetedik nap",
    {"pad": [32, 30], "forras": [20, 40], "thurra": [44, 22], "ajto": [32, 52], "_": [60, 4]},
    [("ajto", "pad"), ("pad", "forras"), ("pad", "thurra")],
    [U("cro_csapszolga", "pad", "Ressth", "ressth")],
    [guard("hallgat", "pad", 45, "Ressth maradjon a padján, és hallgasson (a hat nap)", r=2, marker="Pad"),
     reach("felall", "thurra", "A hetedik napon állj fel, és szólj elsőként", tag="ressth", after="hallgat", marker="Középkő")],
    {"brief": ["A 15. év, Ithrass tanácsvárosa, a gőzkamra. Hat napja hallgat a tanács.",
               "Ressth mindig az öregre vár. Ma talán nem kell."],
     "shots": [shot("pad", L("Ressth", "Ha még egy napig hallgatok, megfulladok. Nem a gőztől.", "cro_csapszolga"))]},
    [shot("thurra", L("Ressth", "Felálltam. Az öreg ült. Talán ezt várta.", "cro_csapszolga"))],
    "A hetedik napon Ressth felállt, és a tanács meghallgatta.", style="barlang")

# ===================================================================== 06 Ooni
nov("nov_06_1", "Kötélkötés",
    {"okuma": [10, 40], "seiran": [54, 40], "kapu": [32, 16], "_": [60, 4]},
    [("okuma", "kapu"), ("seiran", "kapu")],
    [U("oon_oni_harcos", "okuma", "Hoka (Ōkuma)", "okuma"), U("oon_nian_or", "seiran", "Gensai (Seiran)", "seiran")],
    [reach("ok", "kapu", "Hoka érjen a kötélhez", tag="okuma", marker="Fogadalmi kötél"),
     reach("se", "kapu", "Gensai is érjen a kötélhez", tag="seiran", marker="Fogadalmi kötél"),
     guard("csomo", "kapu", 5, "Kössétek meg együtt az első csomót", after=["ok", "se"], marker="Kötél")],
    {"brief": ["Az 57. év, Kilenc Gerenda, a főkapu. A hosszú éjszaka utáni első hajnalon kötik a fogadalmi kötelet.",
               "Az Ōkuma és a Seiran család minden évben veszekszik, ki köti az első csomót."],
     "shots": [shot("kapu", L("Hoka", "Mi álltunk elsőként a kapuba.", "oon_oni_harcos"), L("Gensai", "Mi keltünk át elsőként.", "oon_nian_or"))]},
    [shot("kapu", L("Gensai", "Ez nem…", "oon_nian_or"), L("Hoka", "Jövőre.", "oon_oni_harcos"))],
    "Az első csomót idén együtt kötötték. Jövőre újra vitatkoznak.", style="hago")

nov("nov_06_2", "Amit a tanuki talált",
    {"lejarat": [32, 4], "erem": [14, 22], "tu": [50, 26], "szobor": [24, 46], "polc": [44, 8], "_": [60, 54]},
    [("lejarat", "erem"), ("lejarat", "tu"), ("erem", "szobor"), ("lejarat", "polc")],
    [U("oon_tanuki", "lejarat", "Pon", "pon")],
    [reach("e", "erem", "Keresd meg a régi érmét a nyugati tárnában", tag="pon", marker="Valami csillog"),
     reach("t", "tu", "A csonttűt a keleti omladéknál", tag="pon", marker="Valami csillog"),
     reach("s", "szobor", "Az agyagszobrot a mély tárnában", tag="pon", marker="Valami csillog"),
     reach("polc", "polc", "Tedd a leleteket a tanukik polcára", tag="pon", after=["e", "t", "s"], marker="A tanukik polca")],
    {"brief": ["A 63. év, a Szürkevölgy aranybánya. A tanukik a mélyben néha olyat találnak, ami nem arany és nem meteorit.",
               "Ma Pon a tárnákat járja."],
     "shots": [shot("lejarat", L("Pon", "A polc tele van. Mindig van még hely.", "oon_tanuki"))]},
    [shot("polc", L("Pon", "Lehet, hogy a régi világból valók. Lehet, hogy nem. A polcon jó helyük van.", "oon_tanuki"))],
    "Pon visszament a bányába. Egy hónap múlva egy régi csonttűt talált.", style="barlang",
    trig=[{"at": 80, "message": "Omlás a keleti tárnában!", "color": [255, 170, 110],
           "reshape": {"paint": [{"t": "rock", "line": [[40, 18], [44, 22]], "w": 2}], "clear": [{"t": "dirt", "line": [[32, 4], [46, 14], [50, 26]], "w": 2}]}}])

nov("nov_06_3", "Elvesztett ritmus",
    {"dobsor": [32, 30], "oreg": [10, 46], "kapu": [32, 12], "_": [60, 52]},
    [("dobsor", "oreg"), ("dobsor", "kapu")],
    [U("oon_dobos", "dobsor", "Ryo", "ryo"), U("oon_dobos", "dobsor", None, None, 5)],
    [reach("oreg", "oreg", "Ryo menjen el az öreg dobosmesterhez", tag="ryo", marker="Dobosmester"),
     guard("vissza", "dobsor", 8, "Ryo térjen vissza a dobjához, mielőtt a hajnal a kötélre esik", after="oreg", marker="Ryo dobja")],
    {"brief": ["A 30. év, Kilenc Gerenda, a hosszú éjszaka. Ryo keze megállt, és a dob elhallgatott.",
               "A többiek ritmusa nem tört meg. Ryo mégis hallja a csendet a saját dobja helyén."],
     "shots": [shot("dobsor", L("Ryo", "Egy ütésnyi csend. Mint egy lyuk.", "oon_dobos"))]},
    [shot("dobsor", L("Ryo", "Az utolsó ütés. Pontosan a helyén.", "oon_dobos"))],
    "Az utolsó ütést, amikor a hajnal első fénye a kötélre esett, Ryo ütötte a többiekkel együtt.", style="hago",
    trig=[deadline(150, "vissza", "Felkelt a nap – Ryo dobja üresen maradt.")])

# ===================================================================== 07 Huk’ucha
nov("nov_07_1", "Kinek a tava",
    {"gyules": [32, 46], "mobbra": [46, 14], "felso_gat": [18, 18], "regi_arok": [30, 22], "melyszem": [46, 18], "_": [62, 54]},
    [("gyules", "felso_gat"), ("gyules", "mobbra"), ("felso_gat", "regi_arok")],
    [U("huk_toidezo", "gyules", "Pirrek", "pirrek"), U("huk_toaso", "gyules", None, None, 2)],
    [reach("mobbra", "mobbra", "Beszélj Mobbrával a Mélyszem partján", tag="pirrek", marker="Mobbra"),
     guard("gat", "felso_gat", 6, "Nyisd meg a felső gátat", after="mobbra", marker="Felső gát"),
     guard("arok", "regi_arok", 6, "Vezesd a vizet a régi árkon a Mélyszembe", after="gat", marker="Régi árok")],
    {"brief": ["A 70. év, Nádtorok medence, a gátgyűlés. Tizenkét tó, tizenkét gazda – és mindenki egyszerre beszél.",
               "A víznek valahová mennie kell. Mobbra felajánlja a saját tavát."],
     "shots": [shot("gyules", L("Pirrek", "Három órája tart a gyűlés, és senki nem fejezett be egy mondatot.", "huk_toidezo"))]},
    [shot("melyszem", L("Pirrek", "A Mélyszem a legmélyebb. A kagylósor ott marad az alján.", "huk_toidezo"))],
    "A Mélyszem-tavat tizenegy év múlva énekelték meg újra. Pirrek énekelte.", style="to",
    trig=[{"on_objective": "arok", "reshape": {"paint": [{"t": "water", "line": [[30, 22], [40, 20], [46, 18]], "w": 2}]}}])

nov("nov_07_2", "Ikra a fűben",
    {"tabor": [8, 46], "c1": [16, 36], "c2": [24, 30], "c3": [32, 26], "c4": [40, 22], "c5": [48, 18], "c6": [56, 12], "_": [60, 52]},
    [("tabor", "c1"), ("c1", "c2"), ("c2", "c3"), ("c3", "c4"), ("c4", "c5"), ("c5", "c6")],
    [U("huk_ikrarako", "tabor", "Lillep", "lillep"), U("huk_toaso", "tabor", "Tirri", "tirri")],
    [reach("k1", "c1", "Jelöld meg kaviccsal az első ikracsomót", tag="tirri", marker="Ikra"), reach("k2", "c2", "A másodikat", tag="tirri", after="k1", marker="Ikra"),
     reach("k3", "c3", "A harmadikat", tag="tirri", after="k2", marker="Ikra"), reach("k4", "c4", "A negyediket", tag="tirri", after="k3", marker="Ikra"),
     reach("k5", "c5", "Az ötödiket", tag="tirri", after="k4", marker="Ikra"), reach("k6", "c6", "A hatodikat", tag="tirri", after="k5", marker="Ikra")],
    {"brief": ["A 71. év, a Tavirózsa-medencék nyugati széle. Lillep ikrarakó: láthatatlan csomókat rak a csapások mentén.",
               "Tirri, a kislány, rálépett egyre. Most kavicsokkal jelölik meg az összeset, amire Lillep emlékszik."],
     "shots": [shot("tabor", L("Tirri", "Miért harapnak az ebihalak?", "huk_toaso"), L("Lillep", "Mert rájuk léptél.", "huk_ikrarako"))]},
    [shot("c6", L("Lillep", "Kétszázkilencvenhárom kavics. Hét csomóra nem emlékszem.", "huk_ikrarako"))],
    "A háború végéig a nyugati csapás mentén kétszázkilencvenhárom kavics feküdt a fűben.", style="to",
    enemy=crof([], "Zavaros víz"), trig=[wave("zav", "_", "c3", [{"id": "cro_karmos", "n": 2, "name": "Zavaros víz"}], every=70, delay=40, maxn=3)],
    fail=[{"tag": "tirri", "text": "Tirri megsérült."}])

nov("nov_07_3", "Hamis Tóének",
    {"szentely": [40, 20], "asas": [22, 38], "_": [60, 52]},
    [("szentely", "asas")],
    [U("huk_toaso", "asas", "Brokk", "brokk")],
    [{"id": "to", "type": "ponds", "at": "asas", "radius": 5, "amount": 6, "text": "Áss egy tavacskát (6 csempe)", "marker": "Ásás"},
     guard("enek", "szentely", 10, "Énekelj a Tóteremtő szentélyben – akármilyen hamisan", after="to", marker="Szentély")],
    {"brief": ["A 40. év, Békanyál medence. Brokk a legjobb tóásó, és a leghamisabban énekel.",
               "A régi hit szerint a tavacska csak akkor marad meg, ha megéneklik."],
     "shots": [shot("szentely", L("Brokk", "Hét éve kérik, hogy ne énekeljek. Ma is fogok.", "huk_toaso"))]},
    [shot("szentely", L("Pap", "A rezonanciatábla… ugyanannyi Tóéneket mutat, mint bárki másé.", "huk_aradas_papja"))],
    "A Békanyál szentélye azóta a leghamisabb a medencék közt – és a legtöbb Tóéneket adja.", style="to",
    layout_extra={"decor": [{"id": "huk_toteremto", "at": [46, 14]}]})

# ===================================================================== 08 Micéliák
for nid, title, prot, brief, a, b, vic in (
        ("nov_08_1", "Elvágott ág", "Gyopár",
         ["A 68. év, a Csillámszurdok Mélykertje. A keleti ágat Gyopár és Nádi együtt ültették tizenhét éve.",
          "Nádi meghalt. Az ágban az marad, amit hátrahagyott – és az ágat el kell vágni, mert fertőzött."],
         "Egy tavaszi reggel illata. Ennyi maradt.", "Tavasszal új ágat ültetek. Egyedül.", "A keleti ág helyére Gyopár tavasszal új ágat ültetett."),
        ("nov_08_2", "Az emlékőr kalapja", "Pippi",
         ["A 73. év, a Mélykertek felszíni bejárata. Egy Huk’ucha gyerek három napig ült az Emlékőr lábánál.",
          "Az Emlékőr megmutatja, mire emlékszik a szurdok."],
         "Hét horpadás a kalapon. Mindegyik egy történet.", "Pippi elmondta. Mindet.", "Pippi meghallgatta az Emlékőrt, és elmondta a sajátját is."),
        ("nov_08_3", "Keserű olaj", "Hárs",
         ["A 60. év, a Vízmosás Mélykertje. A tanács arról dönt, befogadja-e a kiszáradt tavú Huk’ucha családokat.",
          "Hárs tudja, hogy veszíteni fog, és mégis feláll."],
         "Valaki keserű olajat kent a gyökerekre. Nem ismerem fel, kié.", "Hét hat ellen, igennel.", "A Huk’ucha családok a kert felszíni szélén telepedtek le.")):
    nov(nid, title, {"kert": [12, 44], "ag": [44, 20], "emlek": [30, 28], "kijarat": [52, 46], "_": [60, 4]},
        [("kert", "emlek"), ("emlek", "ag"), ("emlek", "kijarat")],
        [U("myc_sporaszedo" if nid != "nov_08_2" else "huk_toaso", "kert", prot, "fo"), U("myc_kalapos_or", "kert", None, None, 2)],
        [reach("e", "emlek", "Járd be a szurdok emlékhelyét", tag="fo", marker="Emlékhely"),
         guard("ag", "ag", 8, "Kapcsolódj a keleti ághoz, és figyelj" if nid == "nov_08_1" else "Hallgasd meg, ami itt történt", after="e", marker="Ág"),
         reach("vissza", "kijarat", "Térj vissza a kert peremére", tag="fo", after="ag", marker="Perem")],
        {"brief": brief, "shots": [shot("emlek", L(prot, a, "myc_sporaszedo" if nid != "nov_08_2" else "huk_toaso"))]},
        [shot("kijarat", L(prot, b, "myc_sporaszedo" if nid != "nov_08_2" else "huk_toaso"))], vic, style="szurdok",
        enemy=crof([], "Szivárgó fertőzés"),
        trig=[wave("fert", "_", "ag", [{"id": "cro_karmos", "n": 2, "name": "Szivárgó fertőzés"}], every=60, delay=30, maxn=3)] if nid == "nov_08_1" else [])

# ===================================================================== 09 Pinguu
nov("nov_09_1", "Hasoncsúszás",
    {"rajt": [6, 46], "j1": [20, 36], "j2": [34, 40], "j3": [44, 26], "cel": [56, 14], "nezok": [40, 50], "_": [60, 4]},
    [("rajt", "j1"), ("j1", "j2"), ("j2", "j3"), ("j3", "cel")],
    [U("pin_kajakos", "rajt", "Neeka", "neeka")],
    [reach("j1", "j1", "Csússz az első jelzőig", tag="neeka", marker="Jelző"), reach("j2", "j2", "A második jelzőig", tag="neeka", after="j1", marker="Jelző"),
     reach("j3", "j3", "A harmadikig", tag="neeka", after="j2", marker="Jelző"), reach("cel", "cel", "A célig, az ursan nézők szeme láttára", tag="neeka", after="j3", marker="Cél")],
    {"brief": ["A 60. év, Kőszirt partja Fjordvár közelében. A Pinguu hasoncsúszó-versenyt az ursanok minden tavasszal megnézik, mert nem hiszik el.",
               "Neeka idén nyerni akar."],
     "shots": [shot("rajt", L("Olvir (ursan)", "Ezt most komolyan? A hasukon?", "urs_pajzshordozo"))]},
    [shot("cel", L("Olvir (ursan)", "Ne mondd senkinek, hogy szurkoltam.", "urs_pajzshordozo"))],
    "Neeka visszacsúszott a nézőkhöz. Olvir megkérte, ne mondja el senkinek.", style="jeg",
    trig=[deadline(100, "cel", "Lejárt a futam ideje.")])

nov("nov_09_2", "A fésű",
    {"telep": [10, 44], "halaszok": [30, 34], "szikla": [50, 18], "_": [60, 52]},
    [("telep", "halaszok"), ("halaszok", "szikla")],
    [U("pin_arapaly_saman", "telep", "Atuuk, ár-apály sámán", "atuuk")],
    [reach("hal", "halaszok", "Kérdezd meg a fiatal halászokat a vízről", tag="atuuk", marker="Halászok"),
     guard("fesu", "szikla", 6, "Állj a szikla szélén, és dobd a tengerbe a csontfésűt", after="hal", marker="Szikla")],
    {"brief": ["A 39. év, Hosszú Sziklák telep. A halászat harmadik éve elmaradt.",
               "A régi kötelesség szerint a sámán csontfésűt dob a tengerbe. Atuuk tudja, mit kell tennie, és nem akarja."],
     "shots": [shot("szikla", L("Atuuk", "A halak akkor jönnek vissza, ha a víz lehűl. Ezt a halászok mondják.", "pin_arapaly_saman"))]},
    [shot("szikla", L("Atuuk", "Majd megtudom.", "pin_arapaly_saman"))],
    "A fésű lement a fenékre. Hogy ér-e le valakihez, Atuuk majd megtudja.", style="jeg")

nov("nov_09_3", "Ittu helye",
    {"szentely": [32, 30], "fa": [12, 12], "jeg": [52, 46], "_": [60, 4]},
    [("szentely", "fa"), ("szentely", "jeg")],
    [U("pin_hopajzsos", "szentely", "Nanuk, lángőr-inas", "nanuk")],
    [reach("fa", "fa", "Hozz tüzelőt a szentélyhez", tag="nanuk", marker="Tüzelő"),
     guard("lang", "szentely", 40, "Őrizd az Őrlángot éjjel", after="fa", marker="Őrláng")],
    {"brief": ["Az 50. év, Fehérperem, az Őrláng-szentély. A tűz bal oldalán Ittu helye: a gyerekek nem ülhetnek oda.",
               "Ma éjjel Tekkuna, a vén lángőr, Nanukra bízza a lángot."],
     "shots": [shot("szentely", L("Nanuk", "A kő meleg volt a bal oldalon. Csak azért ültem oda.", "pin_hopajzsos"))]},
    [shot("szentely", L("Nanuk", "Minden este megérintem a bal oldali követ. Meleg.", "pin_hopajzsos"))],
    "Ittu helye tavasszal megint üres lett. Nanuk azóta minden este megérinti.", style="jeg",
    trig=[deadline(150, "lang", "Az Őrláng kialudt.")])

# ===================================================================== 10 Ghoblin
nov("nov_10_1", "Kőcipó vendégnek",
    {"malom": [32, 30], "forras": [12, 14], "mezo": [52, 16], "vendeg": [52, 46], "_": [2, 54]},
    [("malom", "forras"), ("malom", "mezo"), ("malom", "vendeg")],
    [U("gho_koheju_oreg", "malom", "Bruk", "bruk")],
    [reach("viz", "forras", "Hozz lágy vizet a forrásból", tag="bruk", marker="Forrás"), reach("liszt", "mezo", "Hozz árpát a mezőről", tag="bruk", marker="Mező"),
     guard("sut", "malom", 10, "Süsd ki a kőcipót – olyat, amit egy ursan is megehet", after=["viz", "liszt"], marker="Malom"),
     reach("vendeg", "vendeg", "Vidd el a vendégnek", tag="bruk", after="sut", marker="Vendég")],
    {"brief": ["Az 55. év, Mohaköves falu, a Kőcipó malom. Bruk huszonöt éve süti a falu kőcipóját.",
               "Most valaki kérte, hogy süssön olyat, amit egy ursan is megehet. Saját maga."],
     "shots": [shot("malom", L("Bruk", "A kőcipóba kő kell. Ebbe most… kevesebb.", "gho_koheju_oreg"))]},
    [shot("vendeg", L("Bruk", "Tudom.", "gho_koheju_oreg"))], "A vendég megette. Bruk nem mondott többet.", style="hago")

nov("nov_10_2", "Aki nem akart felrobbanni",
    {"tarna": [32, 50], "f1": [16, 34], "f2": [48, 30], "f3": [32, 16], "kijarat": [8, 6], "_": [58, 4]},
    [("tarna", "f1"), ("tarna", "f2"), ("f1", "f3"), ("f2", "f3"), ("f3", "kijarat")],
    [U("gho_meszbomba", "tarna", "Krumm", "krumm"), U("gho_koharapo", "tarna", "Bányász", "banyasz", 4), U("gho_pajzskoves", "tarna", None, None, 2)],
    [reach("hall1", "f1", "Krumm hallgassa meg a nyugati falat", tag="krumm", marker="Repedés?"),
     reach("hall2", "f2", "A keleti falat", tag="krumm", marker="Repedés?"),
     escort("ki", "banyasz", "kijarat", "Vidd ki a bányászokat, mielőtt beomlik a tárna", after=["hall1", "hall2"], mn=3, marker="Kijárat")],
    {"brief": ["A 70. év, a Kőhíd tárna, az ursan határ alatt. Krumm mészbombás, és nem akar felrobbanni.",
               "A Néma raj a tárnában. Krumm hallja, ahogy a kő recseg."],
     "shots": [shot("tarna", L("Krumm", "Nem a robbanás a baj. A kő mondja, hogy menjünk.", "gho_meszbomba"))]},
    [shot("kijarat", L("Krumm", "Honnan tudtam? A kő mondta.", "gho_meszbomba"))],
    "A tárna beomlott – üresen. Krumm neve a bányászok falára került.", style="barlang", enemy=crof([]),
    trig=[wave("neme", "_", "f3", [{"id": "cro_karmos", "n": 3, "name": "Néma raj"}], every=55, delay=30, until="ki"),
          {"if_done": ["hall1", "hall2"], "at": 0, "say": [L("Krumm", "Mindkét fal recseg. Kifelé, most!", "gho_meszbomba")]},
          {"if_done": ["hall1", "hall2"], "every": 1, "delay": 70, "max": 1, "message": "A tárna beomlott!",
           "reshape": {"paint": [{"t": "rock", "area": [[32, 40], 6]}]}}],
    fail=[{"tag": "krumm", "text": "Krumm elesett."}, {"tag": "banyasz", "unless_done": "ki", "text": "A bányászok odavesztek."}])

nov("nov_10_3", "Krekk hallja",
    {"harang": [10, 46], "tarna": [32, 30], "d1": [44, 20], "d2": [52, 30], "d3": [40, 40], "_": [60, 4]},
    [("harang", "tarna"), ("tarna", "d1"), ("tarna", "d2"), ("tarna", "d3")],
    [U("gho_koharapo", "harang", "Durg", "durg")],
    [reach("le", "tarna", "Menj le a Kelet-tárnába – húsz év után először", tag="durg", marker="Kelet-tárna"),
     guard("d1", "d1", 6, "Dúcold alá a harmadik szint első járatát", after="le", marker="Dúc"),
     guard("d2", "d2", 6, "A másodikat", after="d1", marker="Dúc"), guard("d3", "d3", 6, "A harmadikat", after="d2", marker="Dúc")],
    {"brief": ["Az 51. év, Mohaköves, a Fogadalomharang csarnoka. Durg húsz éve fogadta meg, hogy soha többé nem dolgozik a Kelet-tárnában.",
               "A harmadik szint dúcai gyengék. Krekk hallja a követ."],
     "shots": [shot("harang", L("Durg", "Tudom, mit fogadtam. Tudom, mit szegek meg.", "gho_koharapo"))]},
    [shot("tarna", L("Durg", "Két fogadalom a kőben. Egy régi, egy új.", "gho_koharapo"))],
    "A Kelet-tárna dúcai azóta a legerősebbek a bányában.", style="barlang")

# ===================================================================== 11 Mhonyo
nov("nov_11_1", "A gólem szokása",
    {"udvar": [12, 44], "kut": [28, 30], "kohó": [44, 20], "fal": [54, 40], "_": [60, 4]},
    [("udvar", "kut"), ("kut", "kohó"), ("kohó", "fal"), ("fal", "udvar")],
    [U("imp_fabrikator", "udvar", "Aurethi mérnök", "mernok")],
    [reach("k", "kut", "Kövesd a gólemet a kútig", tag="mernok", marker="Gólem útja"), reach("h", "kohó", "A kohóig", tag="mernok", after="k", marker="Gólem útja"),
     reach("f", "fal", "A falig, ahol minden este megáll", tag="mernok", after="h", marker="Gólem útja")],
    {"brief": ["A 62. év, Hamuhegy kovácsfalu. Az aurethi mérnök negyedik napja ül a gólem előtt.",
               "Tikka mester szerint nincs benne lélek. A gólemnek mégis szokásai vannak."],
     "shots": [shot("udvar", L("Tikka mester", "Nincs benne lélek. Mondtam már.", "mho_kardkovacs"))]},
    [shot("fal", L("Aurethi mérnök", "Minden este itt áll meg. Ugyanott, ahol a régi kovács.", "imp_fabrikator"))],
    "Tikka nem szedte szét többé a gólem fejét.", style="varos",
    allies=ally("mhonyo", "Gólem", [{"id": "mho_fogaskerek_golem", "at": "udvar", "to": "fal", "attack": False}]))

nov("nov_11_2", "Parázséjszaka",
    {"szentely": [32, 28], "fujtato": [20, 40], "_": [60, 4]},
    [("szentely", "fujtato")],
    [U("mho_kardkovacs", "szentely", "Ressa, kovácsinas", "ressa")],
    [guard("ej", "szentely", 30, "Tartsd a parazsat a szentélyben", marker="Parázs"),
     guard("fuj", "fujtato", 5, "A parázs halványul – fújj bele, lassan", after="ej", marker="Fújtató"),
     guard("hajnal", "szentely", 20, "Tartsd hajnalig", after="fuj", marker="Parázs")],
    {"brief": ["A 45. év, Hamuhegy, a Parázsszentély. Egy éjszaka a parázzsal az ölben: a kovácsinas utolsó próbája.",
               "Ressa tizenkét évig tanult a kohó mellett."],
     "shots": [shot("szentely", L("Ressa", "Kisebb, mint gondoltam. És melegebb.", "mho_kardkovacs"))]},
    [shot("szentely", L("Ressa", "Hajnal. A parázs él.", "mho_kardkovacs"))], "Ressa aznap kovács lett.", style="barlang",
    trig=[deadline(130, "hajnal", "A parázs kialudt.")])

nov("nov_11_3", "A harmadik penge",
    {"muhely": [20, 40], "kohó": [36, 26], "inas": [50, 44], "_": [60, 4]},
    [("muhely", "kohó"), ("kohó", "inas")],
    [U("mho_kardkovacs", "muhely", "Tikka mester", "tikka")],
    [guard("kovacsol", "kohó", 12, "Kovácsold ki a harmadik pengét", marker="Kohó"),
     reach("ad", "inas", "Vidd az inashoz", tag="tikka", after="kovacsol", marker="Inas")],
    {"brief": ["A 73. év, Hamuhegy, Tikka műhelye. A harmadik pengét a szokás szerint eltörésre szánják.",
               "Tikka egy hét alatt készült el vele. Hogy mit tesz vele, nem kell elmondania senkinek."],
     "shots": [shot("kohó", L("Tikka", "Az első az enyém volt. A második odaadásra. Ez a harmadik.", "mho_kardkovacs"))]},
    [shot("inas", L("Tikka", "Ha egyszer egészen találkozunk, akkor a tiéd. Addig vigyázz rá.", "mho_kardkovacs"))],
    "Az inas a pengét nézte a kezében. Nem törték el.", style="varos")

# ===================================================================== 12 Daresh
nov("nov_12_1", "Az üres versszak",
    {"udvar": [32, 40], "e1": [14, 24], "e2": [32, 12], "e3": [50, 24], "_": [60, 52]},
    [("udvar", "e1"), ("udvar", "e2"), ("udvar", "e3")],
    [U("dar_rajenekes", "udvar", "Ivi", "ivi")],
    [reach("a", "e1", "Kérdezd meg a nyugati rajénekest", tag="ivi", marker="Rajénekes"), reach("b", "e2", "Az északit", tag="ivi", marker="Rajénekes"),
     reach("c", "e3", "A keletit", tag="ivi", marker="Rajénekes"), guard("enek", "udvar", 8, "Énekeld el a Szárnyudvaron", after=["a", "b", "c"], marker="Szárnyudvar")],
    {"brief": ["Az 59. év, a Vörösfal kaptár. A királynő versszakát csak dúdolni szabad. Ivi szavakat akar írni rá.",
               "Előbb megkérdezi a rajénekeseket."],
     "shots": [shot("udvar", L("Ivi", "Háromszáztizenkét versszak. Egy szavak nélkül.", "dar_rajenekes"))]},
    [shot("udvar", L("Ivi", "A kaptár hallgat. Először életemben hallom, milyen, amikor a raj nem énekel.", "dar_rajenekes"))],
    "A királynő versszaka üres maradt. Azt továbbra is dúdolják.", style="dune")

nov("nov_12_2", "Borostyán",
    {"muhely": [10, 46], "hatar": [40, 26], "sereg": [56, 10], "_": [60, 52]},
    [("muhely", "hatar"), ("hatar", "sereg")],
    [U("dar_gyantapajzsos", "muhely", "Zarra", "zarra"), U("dar_gyantapajzsos", "muhely", None, None, 4)],
    [reach("hatar", "hatar", "Indulj nyugatra a határig", tag="zarra", marker="Határ"),
     reach("sereg", "sereg", "Csatlakozz a szövetséges sereghez", tag="zarra", after="hatar", marker="Szövetséges sereg")],
    {"brief": ["A 70. év, a Mézvölgy kaptár. Zarra pajzsának közepén egy régi darázs ül a borostyánban.",
               "A gyantapajzsosokat nyugatra rendelték, a Néma raj ellen."],
     "shots": [shot("muhely", L("Zarra", "Harmadik napja nézem. Ő is engem.", "dar_gyantapajzsos"))]},
    [shot("sereg", L("Zarra", "Itt vagyunk. Mind a ketten.", "dar_gyantapajzsos"))], "Zarra a pajzzsal együtt megérkezett a sereghez.", style="dune",
    enemy=crof([]), trig=[wave("neme", "_", "hatar", [{"id": "cro_karmos", "n": 3, "name": "Néma raj"}], every=50, delay=20, until="sereg")],
    fail=[{"tag": "zarra", "text": "Zarra elesett."}])

nov("nov_12_3", "Hír a Néma rajról",
    {"hatar": [6, 6], "pihe1": [20, 20], "pihe2": [36, 32], "kaptar": [54, 46], "_": [2, 54]},
    [("hatar", "pihe1"), ("pihe1", "pihe2"), ("pihe2", "kaptar")],
    [U("dar_szarnyas_ijasz", "hatar", "Kessa, hírvivő", "kessa"), U("dar_szarnyas_ijasz", "hatar", "Kísérő", None, 2)],
    [reach("p1", "pihe1", "Pihenj meg az első szakadéknál", tag="kessa", marker="Pihenő"), reach("p2", "pihe2", "A másodiknál", tag="kessa", after="p1", marker="Pihenő"),
     reach("haza", "kaptar", "Érj a Vörösfal kaptár Szárnyudvarára", tag="kessa", after="p2", marker="Vörösfal"),
     guard("enek", "kaptar", 8, "Énekeld el a hírt a kaptárnak", after="haza", marker="Szárnyudvar")],
    {"brief": ["A 70. év. Kessa három nap alatt repült vissza a nyugati határról.", "A hírt egy rajének-versszakba szőtte. A kaptár nem akarja meghallani."],
     "shots": [shot("hatar", L("Kessa", "Szárnyak, bírjátok még három napig.", "dar_szarnyas_ijasz"))]},
    [shot("kaptar", L("Kessa", "A kaptár fele csatlakozott. Elég volt.", "dar_szarnyas_ijasz"))],
    "Két héttel később a Vörösfal kaptár a gyantapajzsosait nyugatra küldte.", style="dune",
    enemy=crof([]), trig=[{"on_objective": "p2", "grant": {"tag": "kessa", "abilities": ["dar_rajenek"], "text": "Kessa versszakba szőtte a hírt: Rajének (képesség)."},
                          "say": [L("Kessa", "Ha dalban viszem, a szárnyak is emlékeznek.", "dar_szarnyas_ijasz")]},
                         wave("uldoz", "_", "pihe1", [{"id": "cro_szarnyas_rem", "n": 1, "name": "Néma raj"}], every=55, delay=60, until="haza")],
    fail=[{"tag": "kessa", "text": "Kessa lezuhant."}])

# ===================================================================== 13 Shrissh-matah
nov("nov_13_1", "Két vonás",
    {"orhely": [32, 14], "kapu": [32, 26], "menekultek": [32, 50], "fal": [44, 10], "_": [60, 52]},
    [("menekultek", "kapu"), ("kapu", "orhely"), ("orhely", "fal")],
    [U("shr_runaszovo", "orhely", "Shrikka-ti", "shrikka"), U("shr_pajzsbogar", "kapu", None, None, 3)],
    [escort("be", "menekult", "orhely", "Engedd be a menekülőket a nyitott kapun", mn=3, marker="Őrhely"),
     guard("rovas", "fal", 6, "Vésd fel a falra az ellentmondás rúnáját", after="be", marker="Fal")],
    {"brief": ["A 65. év, a Tüskeorom őrhely. Tkassh nevét átvésték: „Nyitva hagyta.”", "Shrikka-ti tudja, miért hagyta nyitva."],
     "shots": [shot("kapu", L("Shrikka-ti", "Védte a falat. Két vonással lett belőle: nyitva hagyta.", "shr_runaszovo"))]},
    [shot("fal", L("Shrikka-ti", "Két rúna egymás mellett. Ha fúj a szél, mindkettő szól.", "shr_runaszovo"))],
    "A Tüskeorom falán ma is ott van a két rúna egymás mellett.", style="dune",
    allies=ally("shrissh_matah", "Menekülők", [U("shr_sziklafuto", "menekultek", None, "menekult", 4)]),
    trig=[{"on_enter": {"at": "menekultek", "r": 6}, "join": "menekult"},
          wave("uldozo", "_", "kapu", [{"id": "cro_karmos", "n": 3, "name": "Néma raj"}], every=50, delay=40, until="be")],
    enemy=crof([]), fail=[{"tag": "menekult", "unless_done": "be", "text": "A menekülők odavesztek."}])

nov("nov_13_2", "A szél neve",
    {"zuhanas": [10, 40], "bokor": [24, 30], "szikla": [40, 22], "runaszovo": [54, 10], "_": [60, 52]},
    [("zuhanas", "bokor"), ("bokor", "szikla"), ("szikla", "runaszovo")],
    [U("shr_tuszunyog", "zuhanas", "Vrissa", "vrissa")],
    [reach("b", "bokor", "Gyalogolj a tüskés bokorig", tag="vrissa", marker="Bokor"), reach("s", "szikla", "A sziklaárnyékig", tag="vrissa", after="b", marker="Árnyék"),
     reach("r", "runaszovo", "Érj el a Rúnaszövőhöz", tag="vrissa", after="s", marker="Rúnaszövő")],
    {"brief": ["A 48. év, a Száraz-fennsík. A szél elállt, és Vrissa, a tűszúnyog-felderítő, nem tudott leszállni – lezuhant.",
               "Gyalog kell átkelnie a fennsíkon."],
     "shots": [shot("zuhanas", L("Vrissa", "A szárnyam fáj. És itt egy gyík.", "shr_tuszunyog"))]},
    [shot("runaszovo", L("Vrissa", "A rúna egyenes lett. A heg világos.", "shr_tuszunyog"))], "A szárny széle hónapok alatt begyógyult.", style="dune",
    enemy={"faction": "vadon", "name": "Gyíkok", "base": False, "ai": False, "units": []} if False else crof([], "Gyíkok"),
    trig=[wave("gyik", "_", "szikla", [{"id": "cro_karmos", "n": 1, "name": "Fennsíki gyík"}], every=40, delay=20, maxn=3)],
    fail=[{"tag": "vrissa", "text": "Vrissa nem jutott el a Rúnaszövőhöz."}])

nov("nov_13_3", "Itatóhely",
    {"keserukut": [32, 28], "shr": [10, 44], "dar": [54, 12], "_": [60, 52]},
    [("shr", "keserukut"), ("dar", "keserukut")],
    [U("shr_sziklafuto", "shr", "Thikka", "thikka")],
    [reach("kut", "keserukut", "Thikka érjen a kúthoz", tag="thikka", marker="Keserűkút"),
     escort("darazs", "darazs", "keserukut", "A darázsgyerek is ihasson", after="kut", mn=1, r=3, marker="Kút"),
     guard("runa", "keserukut", 6, "Vésd alá a harmadik rúnát", after="darazs", marker="Kút")],
    {"brief": ["A 73. év, a Keserűkút, a Daresh és a Shrissh-matah határán.", "– Ez a mi kutunk. – Nem. A mienk."],
     "shots": [shot("keserukut", L("Thikka", "A mi nevünk rúna. A ti nevetek csak hang.", "shr_sziklafuto"))]},
    [shot("keserukut", L("Thikka", "Ferde lett. Azt jelenti: ma együtt ittunk.", "shr_sziklafuto"))], "A két rúna alatt ott a harmadik, ferdén.", style="dune",
    allies=ally("daresh", "Darázsgyerek", [U("dar_szarnyas_ijasz", "dar", "Darázsgyerek", "darazs")]),
    trig=[{"on_enter": {"at": "keserukut", "r": 4, "tag": "thikka"}, "join": "darazs"}])

# ===================================================================== 14 Tokla
nov("nov_14_1", "A legnagyobb hazug",
    {"tuz": [32, 30], "o1": [12, 14], "o2": [52, 14], "o3": [12, 46], "o4": [52, 46], "o5": [32, 6], "_": [60, 54]},
    [("tuz", "o1"), ("tuz", "o2"), ("tuz", "o3"), ("tuz", "o4"), ("tuz", "o5")],
    [U("tok_zuzmaraturpe", "tuz", "Fenna", "fenna")],
    [reach(f"o{i}", f"o{i}", f"Mondd meg a(z) {i}. óriás harcosnak, hogy a holdszilánk hazugság volt", tag="fenna", marker="Óriás harcos") for i in range(1, 6)],
    {"brief": ["Az 53. év, Kőszáj csarnok, a tél végi tűz. Fenna egész évben hazudott, hogy ő gyújthassa meg a Télkirály maszkját.",
               "Az egyik hazugsága nem volt ártalmatlan."],
     "shots": [shot("tuz", L("Nagyanyó", "A holdszilánk, Fenna.", "tok_harangjaro"))]},
    [shot("o5", L("Fenna", "Tudták már. Mégis megmondtam. Mind az ötnek.", "tok_zuzmaraturpe"))],
    "Fenna egyenként mind az öt harcosnak megmondta, hogy a holdszilánk hazugság volt.", style="jeg")

nov("nov_14_2", "Holdőr",
    {"ureg": [32, 20], "tabor": [32, 34], "hago": [32, 54], "_": [60, 4]},
    [("tabor", "ureg"), ("tabor", "hago")],
    [U("tok_fagyorias", "tabor", "Hrann", "hrann"), U("urs_pajzshordozo", "tabor", "Ketil", "ketil")],
    [survive("orseg", 120, "Őrizzétek együtt a holdszilánkos üreget")],
    {"brief": ["A 73. év, a holdszilánkos üreg a hágó fölött. Hrann és Ketil harmadik éve őrzik az üreget.",
               "A háborúban a hágó két oldalán álltak. Ma ugyanazon az oldalon."],
     "shots": [shot("ureg", L("Ketil", "Három éve. És még mindig nem értjük egymás nyelvét.", "urs_pajzshordozo"))]},
    [shot("tabor", L("Ketil", "Egy őré. Jó őr volt.", "urs_pajzshordozo"))], "Az üreg őrzése folytatódik. Ketil leült Hrann mellé a hóba.", style="jeg",
    enemy=crof([], "Maradék raj"), trig=[wave("maradek", "_", "ureg", [{"id": "cro_karmos", "n": 3, "name": "Maradék raj"}], every=40, delay=20, until="orseg")],
    fail=[{"tag": "hrann", "text": "Hrann elesett."}, {"tag": "ketil", "text": "Ketil elesett."}])

nov("nov_14_3", "Harangjárás",
    {"falu": [32, 34], "h1": [20, 26], "h2": [44, 26], "h3": [20, 42], "h4": [44, 42], "forras": [32, 6], "_": [60, 54]},
    [("falu", "h1"), ("falu", "h2"), ("falu", "h3"), ("falu", "h4"), ("falu", "forras")],
    [U("tok_harangjaro", "falu", "Görb", "gorb"), U("tok_harangjaro", "falu", None, None, 2)],
    [reach(f"h{i}", f"h{i}", f"Járd körbe harangszóval a(z) {i}. házat", marker="Beteg ház") for i in range(1, 5)] +
    [reach("viz", "forras", "Hozz tiszta vizet a hegyi forrásból – az asszony szerint a láz a víztől van", optional=True, marker="Forrás")],
    {"brief": ["Az 58. év, Csipkeudvar, Zhan-zhama falu a Tokla hágó alatt. A falu fele lázas.",
               "A Harangjárók lejöttek a hegyről."],
     "shots": [shot("falu", L("Zhan-zhama asszony", "Ez nem gyógyít. A láz a víz miatt van.", "zha_tolljos"))]},
    [shot("falu", L("Görb", "Egy kis faragvány a küszöbre. Egy tojás, kikelő madárral.", "tok_harangjaro"))],
    "A tizedik napon a láz megtört.", style="jeg")

# ===================================================================== 15 Galóca
nov("nov_15_1", "Harapásnyom",
    {"kert": [10, 46], "k1": [24, 30], "k2": [40, 30], "fogas": [54, 12], "_": [60, 52]},
    [("kert", "k1"), ("k1", "k2"), ("k2", "fogas")],
    [U("myc_sporaszedo", "kert", "Bodza, követ", "bodza")],
    [reach("k1", "k1", "Járd be az első határkövet", tag="bodza", marker="Határkő"), reach("k2", "k2", "A másodikat", tag="bodza", after="k1", marker="Határkő"),
     reach("fogas", "fogas", "Keresd meg Fogast, az őrparancsnokot", tag="bodza", after="k2", marker="Fogvár"),
     guard("var", "fogas", 10, "Várj. Ne csinálj semmit.", after="fogas", marker="Fogvár")],
    {"brief": ["A 74. év, a Csillámszurdok és a Fogvár Galóca-telep határa. Az ajtó előtt egy harapásnyomos gombafej.",
               "Bodza egy hónapja követ. Nem tudja, mit kell vele csinálni."],
     "shots": [shot("kert", L("Bodza", "Ez a Galóca-barátság jele. Azt hiszem.", "myc_sporaszedo"))]},
    [shot("fogas", L("Fogas", "Az a tiéd. Csinálj vele, amit akarsz.", "gal_fogaskalap"))], "Bodza nem csinált vele semmit. Fogas szerint ez volt a helyes.",
    style="erdo")

nov("nov_15_2", "A Tüskekert",
    {"belepo": [6, 30], "t1": [20, 20], "t2": [32, 40], "t3": [44, 18], "kapu": [56, 30], "_": [60, 54]},
    [("belepo", "t1"), ("t1", "t2"), ("t2", "t3"), ("t3", "kapu")],
    [U("imp_fabrikator", "belepo", "Livius", "livius")],
    [reach("ki", "kapu", "Találd meg a Tüskekert kapuját", tag="livius", marker="Kapu"),
     guard("var", "kapu", 15, "Ne menj be. Várj.", after="ki", marker="Kapu")],
    {"brief": ["A 67. év, a Fogvár Galóca-telep. A Fabrikátor nem tudta, hol van, és a tüskék a fejéig értek.",
               "Livius egyedül indult el. Most életében először nagyon megbánta."],
     "shots": [shot("belepo", L("Livius", "A térkép szerint az út egy erdőn át vezet. Arról nem szólt, hogy ki lakik benne.", "imp_fabrikator"))]},
    [shot("kapu", L("Galóca őr", "A kapun idegen nem jön be.", "gal_fogaskalap"))], "Livius kerülővel megtalálta az utat. A térképre azt írta: Ne menj be. Ha bementél, várj.",
    style="barlang", layout_extra={"rocks": [[[14, 14], 4], [[26, 30], 4], [[38, 26], 4], [[50, 40], 5], [[20, 44], 4], [[46, 6], 4]]},
    enemy={"faction": "galoca", "name": "Galóca őrök", "base": False, "ai": False,
           "units": [{"id": "gal_tuskebunkos", "n": 2, "at": [32, 30]}, {"id": "gal_tuskebunkos", "n": 2, "at": [44, 40]}]},
    fail=[{"tag": "livius", "text": "Livius nem jutott ki a Tüskekertből."}])

nov("nov_15_3", "Galóca mondák gyűjteménye",
    {"tabor": [10, 46], "g1": [20, 30], "g2": [36, 38], "g3": [44, 22], "g4": [30, 12], "g5": [54, 10], "_": [60, 54]},
    [("tabor", "g1"), ("g1", "g2"), ("g2", "g3"), ("g3", "g4"), ("g4", "g5")],
    [U("myc_sporaszedo", "tabor", "Csipkebogyó", "csipke")],
    [reach(f"g{i}", f"g{i}", f"Gyűjtsd össze a(z) {i}. mondát", tag="csipke", marker="Galóca") for i in range(1, 6)],
    {"brief": ["Csipkebogyó, micélia emlékkertész, jegyzetei a 63. évből.", "Gyűjteményt akar a Galóca népi mondáiból."],
     "shots": [shot("tabor", L("Csipkebogyó", "Megkérdezem, honnan jöttek. Biztos tudják.", "myc_sporaszedo"))]},
    [shot("g5", L("Csipkebogyó", "Öt monda, ebből kettő nem monda, egy hazugság. És három harapásnyom.", "myc_sporaszedo"))],
    "A gyűjteményt a tanácsnak beadta. A gombafejet megtartotta.", style="erdo",
    trig=[{"on_objective": f"g{i}", "say": [L("Galóca", t, "gal_fogaskalap")]} for i, t in
          ((1, "A földből."), (2, "Föld."), (3, "Volt egy erdő. Mindenki harapott. Mi jobban."), (4, "*harap*"), (5, "Most keletkezett."))])

# ===================================================================== 16 Niil-elfek
nov("nov_16_1", "Krokodilőr",
    {"medence": [20, 30], "lepcso": [32, 30], "folyo": [54, 30], "_": [60, 4]},
    [("medence", "lepcso"), ("lepcso", "folyo")],
    [U("nii_papiruszlandzsas", "lepcso", "Ineti", "ineti"), U("nii_krokodilor", "medence", "Merru-ka", "merru")],
    [reach("ki", "folyo", "Kísérd Merru-kát a nagy folyóig", tag="merru", marker="Folyó")],
    {"brief": ["Az 56. év, Papiruszkapu, az Árvízszentély medencéje. Az öreg krokodil, Merru-ka, nem akar kimenni a medencéből.",
               "Ineti három hónapja a gondozója."],
     "shots": [shot("medence", L("Ineti", "Száztíz év. A fejét a lépcsőre tette.", "nii_papiruszlandzsas"))]},
    [shot("folyo", L("Ineti", "Elment. Sokáig ültem a csónakban. Aztán visszaeveztem.", "nii_papiruszlandzsas"))],
    "Merru-ka a nagy folyóba úszott. Ineti visszaevezett.", style="folyo", fail=[{"tag": "merru", "text": "Merru-ka megsérült."}])

nov("nov_16_2", "A lótusz forog",
    {"szentely": [32, 40], "lepcso": [32, 30], "domb": [32, 6], "_": [60, 54]},
    [("szentely", "lepcso"), ("szentely", "domb")],
    [U("nii_arapalypap", "lepcso", "Nefri, árpapnő", "nefri")],
    [guard("lotusz", "lepcso", 5, "Tedd a lótuszt a vízre", marker="Lépcső"),
     escort("ki", "hivok", "domb", "A lótusz forog: vezesd fel a híveket a dombra", after="lotusz", mn=3, marker="Domb", hidden=False)],
    {"brief": ["A 70. év, Papiruszkapu, az áradás reggele. Ha a lótusz megáll és forog, a Lótuszsziget hívja vissza.",
               "Nefri tudja: az idén az ár korábban jön, és magasabban."],
     "shots": [shot("lepcso", L("Nefri", "Forog. Nem a sziget hív. Az örvény erősebb.", "nii_arapalypap"))]},
    [shot("domb", L("Nefri", "Az ároszlopra felkerül egy nyitott félkör.", "nii_arapalypap"))], "Az ároszlopon az idei forgás mellett ott a nyitott félkör.",
    style="folyo", allies=ally("niil_elf", "Hívők", [U("nii_lotuszpajzsos", "szentely", None, "hivok", 5)]),
    trig=[{"on_objective": "lotusz", "join": "hivok"},
          {"if_done": "lotusz", "every": 1, "delay": 40, "max": 1, "message": "Árad a folyó!", "color": [140, 200, 255],
           "reshape": {"paint": [{"t": "water", "line": [[-1, 32], [20, 30], [44, 34], [65, 32]], "w": 12}], "clear": [{"t": "dirt", "line": [[32, 20], [32, 10]], "w": 3}]}}],
    fail=[{"tag": "hivok", "unless_done": "ki", "text": "A hívők odavesztek az árban."}])

nov("nov_16_3", "Csónakos lakodalom",
    {"lepcso": [32, 12], "k1": [20, 30], "k2": [32, 34], "k3": [44, 30], "_": [60, 54]},
    [("lepcso", "k1"), ("lepcso", "k3")],
    [U("nii_papiruszlandzsas", "lepcso", "Csónakkészítő", "csonak"), U("nii_nadhajos_ijasz", "lepcso", None, None, 3)],
    [guard("c1", "k1", 6, "Kösd meg erősebben a nyugati csónakokat", marker="Kötés"), guard("c2", "k2", 6, "A középsőket", marker="Kötés"),
     guard("c3", "k3", 6, "A keletieket", marker="Kötés"), survive("kitart", 60, "Tartsd egyben a csónakszigetet, amíg az ár levonul", after=["c1", "c2", "c3"])],
    {"brief": ["A 44. év, Papiruszkapu. A lakodalmat harminchét csónakon tartják – és a víz délben emelkedni kezd.",
               "A csónakokat nem szabad szétoldani."],
     "shots": [shot("lepcso", L("Öreg árpapnő", "Nem oldjuk szét. Soha nem oldottuk.", "nii_arapalypap"))]},
    [shot("k2", L("Csónakkészítő", "Lakodalom. Nem oldották szét.", "nii_papiruszlandzsas"))], "Az ároszlopra felvésték: Lakodalom. Nem oldották szét.",
    style="folyo")

# ===================================================================== 17 Tűz-elfek
nov("nov_17_1", "A parázs mellett",
    {"kemence": [32, 30], "kapu": [32, 52], "ut": [32, 4], "_": [60, 4]},
    [("kemence", "kapu"), ("kemence", "ut")],
    [U("tuz_homokszablyas", "kemence", "Saruna", "saruna"), U("tuz_homokszablyas", "kemence", None, None, 5), U("tuz_parazsijasz", "kemence", None, None, 3), U("elf_tollas_harcos", "kemence", "A szürke elf zsoldos", "vendeg")],
    [survive("ejjel", 120, "A vendégjog szerint őrizd a parázskemence mellé ült vendéget reggelig")],
    {"brief": ["A 34. év, Vörösudvar erőd. A szürke elf zsoldos napnyugtakor a parázskemence mellé ült, mielőtt bárki megállíthatta.",
               "A dűnevidéki vendégjog szerint aki a parázs mellé ül, az vendég. Üldözői éjjel érkeznek."],
     "shots": [shot("kemence", L("Saruna", "Első hiba. Vagy első okos lépés. Attól függ, kit kérdezel.", "tuz_homokszablyas"))]},
    [shot("kemence", L("Saruna", "Hajnal. A vendégjog lejárt. Menj.", "tuz_homokszablyas"))], "A zsoldosról Saruna soha többé nem hallott.", style="dune",
    enemy={"faction": "elves", "name": "Üldözők", "base": False, "ai": False, "units": []},
    trig=[wave("uldozok", "ut", "kemence", [{"id": "elf_jade_ijasz", "n": 1, "name": "Üldöző"}, {"id": "elf_tollas_harcos", "n": 2, "name": "Üldöző"}],
               every=55, delay=45, until="ejjel")],
    fail=[{"tag": "vendeg", "text": "A vendéget elragadták – a vendégjog megtört."}])

nov("nov_17_2", "Ashrun kölcsöne",
    {"torony": [52, 12], "csata": [24, 34], "_": [6, 50]},
    [("csata", "torony")],
    [U("tuz_delibab_csalo", "csata", "Leyla", "leyla"), U("tuz_delibab_mas", "csata", "Mirra", "mirra"), U("tuz_homokszablyas", "csata", None, None, 3)],
    [escort("torony", "mirra", "torony", "Vidd a szét nem foszló délibáb-harcost, Mirrát a Délibábtoronyba", marker="Délibábtorony")],
    {"brief": ["A 62. év, Narhad erőd. Leyla tizenöt éve Délibáb-csaló. A délibáb-harcosok mindig szétfoszlanak.", "Mirra nem."],
     "shots": [shot("csata", L("Leyla", "Ashrun kölcsönözte őket. Vissza kell menniük. Miért nem mész?", "tuz_delibab_csalo"))]},
    [shot("torony", L("Leyla", "Mirra soha nem tudta meg, miért. Én sem.", "tuz_delibab_csalo"))], "Mirra a toronyban maradt. Nem foszlott szét.", style="dune",
    enemy=crof([]), trig=[{"on_enter": {"at": [38, 23], "r": 4, "tag": "mirra"},
                          "grant": {"tag": "mirra", "abilities": ["tuz_delibab"], "text": "Mirra maga is délibábot sző: Délibáb (képesség)."},
                          "say": [L("Leyla", "Ezt nem tőlem tanultad.", "tuz_delibab_csalo")]},
                         wave("raj", "_", "csata", [{"id": "cro_karmos", "n": 2, "name": "Néma raj"}], every=50, delay=40, until="torony")],
    fail=[{"tag": "mirra", "text": "Mirra szétfoszlott."}])

nov("nov_17_3", "Ciszternaőr",
    {"ciszterna": [32, 30], "karavan": [10, 46], "rud": [40, 24], "_": [60, 4]},
    [("ciszterna", "karavan"), ("ciszterna", "rud")],
    [U("tuz_hoszovo", "ciszterna", "Ahnar", "ahnar")],
    [guard("meres", "rud", 5, "Mérd meg a víz szintjét a jelzett rúddal", marker="Mérőrúd"),
     reach("karavan", "karavan", "Menj ki a karavánhoz", tag="ahnar", after="meres", marker="Karaván")],
    {"brief": ["A 20. év, a Tövisdomb ciszterna, az aszály harmadik éve. A víz három falunak kell egész nyárra.", "Napnyugtakor Wullof karaván érkezik."],
     "shots": [shot("ciszterna", L("Ahnar", "Hat nemzedék őrzi. És minden nap kevesebb.", "tuz_hoszovo"))]},
    [shot("ciszterna", L("Wullof kereskedő", "A víz, amit nem vendégként kaptunk, nem vendégség volt. Ez a só sem az.", "wul_gyujtogeto"))],
    "Ősszel egy Wullof kereskedő egy szekér sót hozott.", style="dune",
    trig=[{"on_objective": "karavan", "choice": {"title": "Víz", "text": "A karaván szomjas. A ciszterna félig üres.",
                                                 "options": [{"label": "Adok – a falvak egy hónapig takarékoskodnak", "flag": "nov173_ad"},
                                                             {"label": "Nem adhatok", "flag": "nov173_nem"}]}}])

# ===================================================================== 18 Tendüür
nov("nov_18_1", "Áthúzott név",
    {"liget": [12, 44], "ezustkereg": [40, 24], "forras": [54, 46], "_": [60, 4]},
    [("liget", "ezustkereg"), ("ezustkereg", "forras")],
    [U("ten_lampasor", "liget", "Yrie", "yrie")],
    [reach("fa", "ezustkereg", "Menj az Öreg Ezüstkéreghez", tag="yrie", marker="Öreg Ezüstkéreg"),
     reach("viz", "forras", "Hozz vizet a forrásból", tag="yrie", after="fa", marker="Forrás"),
     guard("ápol", "ezustkereg", 10, "Ülj a fa tövében", after="viz", marker="Öreg Ezüstkéreg")],
    {"brief": ["Az 50. év, a Lámpásliget. Yrie bal karján tizenhét név, és az első név fája haldoklik.", "A szokás szerint a halott fa nevét áthúzzák."],
     "shots": [shot("ezustkereg", L("Yrie", "Ha nem húzom át?", "ten_lampasor"))]},
    [shot("ezustkereg", L("Öreg tündér", "Majd lesz.", "ten_szelhimzo"))], "Yrie karján a név maradt, ahogy volt.", style="erdo",
    trig=[{"on_objective": "ápol", "choice": {"title": "A név", "text": "Az Öreg Ezüstkéreg haldoklik.",
                                             "options": [{"label": "Áthúzom", "flag": "nov181_at"}, {"label": "Még nem húzom át", "flag": "nov181_meg"}]}}])

nov("nov_18_2", "Aki nem kapaszkodott",
    {"falu": [10, 46], "tolgyor": [44, 20], "_": [60, 52]},
    [("falu", "tolgyor")],
    [U("ten_szelhimzo", "falu", "Fiel anyja", "anya")],
    [reach("oda", "tolgyor", "Menj a Tölgyőrök ligetébe, Fielért", tag="anya", marker="Tölgyőr"),
     guard("csend", "tolgyor", 10, "Ne mozdulj. Ne ébreszd fel a Tölgyőrt.", after="oda", marker="Tölgyőr"),
     escort("haza", "fiel", "falu", "Vidd haza Fielt", after="csend", marker="Falu")],
    {"brief": ["A 69. év, a Tölgyőrök ligete. Fiel elaludt egy Tölgyőr ölében.", "A monda szerint aki ott alszik, fává válik."],
     "shots": [shot("tolgyor", L("Fiel anyja", "Fiel. Ne mozdulj.", "ten_szelhimzo"))]},
    [shot("falu", L("Fiel", "Néha mégis leülök oda, egyedül.", "ten_lombijasz"))], "Fiel ma is ezt tanítja a gyerekeinek – és néha mégis leül.", style="erdo",
    allies=ally("tenduur", "Fiel", [U("ten_lombijasz", "tolgyor", "Fiel", "fiel"), U("ten_tolgyor", [46, 22], "Tölgyőr", None)]),
    trig=[{"on_objective": "csend", "join": "fiel"}])

nov("nov_18_3", "Lámpásgyújtás",
    {"liget": [32, 30], "l1": [12, 12], "l2": [52, 12], "l3": [56, 40], "l4": [10, 44], "elveszett": [40, 50], "_": [60, 4]},
    [("liget", "l1"), ("liget", "l2"), ("liget", "l3"), ("liget", "l4"), ("liget", "elveszett")],
    [U("ten_lampasor", "liget", "Ilvi", "ilvi")],
    [guard(f"l{i}", f"l{i}", 2, f"Gyújtsd meg a(z) {i}. lámpást a liget peremén", marker="Lámpás") for i in range(1, 5)] +
    [reach("talal", "elveszett", "Keresd meg az eltévedt aurethit", tag="ilvi", after=["l1", "l2", "l3", "l4"], marker="Fény a sötétben"),
     escort("ki", "aurethi", "liget", "Vezesd ki a gyökereken át – ne a gyökerekre lépjen", after="talal", marker="Ösvény")],
    {"brief": ["A 66. év, a Lámpásliget pereme. Ilvi lámpásgyújtó: napnyugtakor végigrepüli a peremet.", "Az aurethi a sötétben jött, és eltévedt."],
     "shots": [shot("liget", L("Ilvi", "A lámpások a fák szemei éjszakára.", "ten_lampasor"))]},
    [shot("liget", L("Ilvi", "Hozz egy lámpást. És ne a gyökerekre lépj.", "ten_lampasor"))], "Egy hónap múlva egy kis bronz lámpás lógott egy alacsony ágon.",
    style="erdo", allies=ally("imperium", "Eltévedt", [U("imp_fabrikator", "elveszett", "Eltévedt aurethi", "aurethi")]),
    trig=[{"on_objective": "talal", "join": "aurethi"}, deadline(200, "ki", "Teljesen besötétedett.")])

# ===================================================================== 19 Nila-uchana
nov("nov_19_1", "Makarán a folyón",
    {"atkelo": [8, 30], "a1": [24, 26], "a2": [40, 32], "a3": [56, 28], "_": [60, 52]},
    [("atkelo", "a1"), ("a1", "a2"), ("a2", "a3")],
    [U("nil_makara", "atkelo", "Riddhi makarája", "makara"), U("imp_legios", "atkelo", "Gnaeus Voro, hárfás", "gnaeus")],
    [reach(f"a{i}", f"a{i}", f"Állj meg a(z) {i}. hallgatóhelynél", tag="gnaeus", marker="Hallgatóhely", after=(f"a{i-1}" if i > 1 else None)) for i in range(1, 4)],
    {"brief": ["Az 54. év, a Lótuszlépcsők alatti folyóág. Az aurethi zenész fülével hall.", "Riddhi körbeviszi a folyón."],
     "shots": [shot("atkelo", L("Riddhi", "Most tedd a kezed a vízbe.", "nil_makara_idezo"))]},
    [shot("a3", L("Gnaeus", "A makarának add át, hogy köszönöm.", "imp_legios"))], "Riddhi átadta. A makara a víz alatt felelt.", style="folyo")

nov("nov_19_2", "Rossz név",
    {"szentely": [32, 30], "h1": [12, 12], "h2": [52, 14], "h3": [14, 46], "haz": [50, 46], "_": [60, 4]},
    [("szentely", "h1"), ("szentely", "h2"), ("szentely", "h3"), ("szentely", "haz")],
    [U("nil_szornykoto", "szentely", "Ankit", "ankit")],
    [reach("h1", "h1", "Keresd a kutyát a lótuszudvarban", tag="ankit", marker="Nyom"), reach("h2", "h2", "A piacon", tag="ankit", marker="Nyom"),
     reach("h3", "h3", "A folyóparton", tag="ankit", marker="Nyom"), reach("haz", "haz", "Menj haza", tag="ankit", after=["h1", "h2", "h3"], marker="Otthon")],
    {"brief": ["A 61. év, a Kék Isten szentélye. Valaki egy nevet kiabált, és a papok egyszerre megfordultak.", "Ankit kutyája elszökött."],
     "shots": [shot("szentely", L("Ankit", "A kutyám. Elszökött.", "nil_szornykoto"))]},
    [shot("haz", L("Ankit", "Hazajött. Magától.", "nil_szornykoto"))], "A fiatalabb papok jelnek tartották. Az idősebbek szerint a kutyák mindig hazajönnek.", style="to")

nov("nov_19_3", "A nága változata",
    {"haz": [10, 46], "medence": [44, 20], "_": [60, 52]},
    [("haz", "medence")],
    [U("nil_szornykoto", "haz", "Lakshi", "lakshi")],
    [reach("part", "medence", "Ülj le a Makara-medence partján", tag="lakshi", marker="Medence"),
     guard("hallgat", "medence", 10, "Hallgasd meg a nága változatát", after="part", marker="Nága"),
     reach("haza", "haz", "Menj haza, és mondd el anyádnak", tag="lakshi", after="hallgat", marker="Otthon")],
    {"brief": ["Az 50. év, a Makara-medence partja. A gyerekeknek nem szabad a nágák közelébe menniük.", "– Akarod hallani az igazat? – kérdezte a nága."],
     "shots": [shot("medence", L("Nága", "Akarod hallani az igazat?", "nil_naga"))]},
    [shot("haz", L("Lakshi", "A nagyanyám is hallotta. Ő azt gondolta, a nága még mindig vár, hogy valaki elhiggye.", "nil_szornykoto"))],
    "Lakshi elmondta az anyjának. Az anyja elgondolkodott.", style="to")

# ===================================================================== 20 Zhan-zhama
nov("nov_20_1", "A kakaslovas udvarol",
    {"nadfedel": [8, 30], "hid": [32, 30], "tojasko": [56, 30], "tyuk": [40, 12], "_": [60, 52]},
    [("nadfedel", "hid"), ("hid", "tojasko"), ("hid", "tyuk")],
    [U("zha_kakaslovas", "nadfedel", "Bakku és Vihar", "bakku")],
    [reach("hid", "hid", "Lovagolj át a hídon", tag="bakku", marker="Híd"),
     reach("mimbu", "tojasko", "Érj Mimbu házához Tojáskőben", tag="bakku", after="hid", marker="Mimbu")],
    {"brief": ["A 47. év, Nádfedél és Tojáskő között. Bakku harmadszor megy át a hídon udvarolni.", "Vihar, a kakasa, eddig mindent elrontott."],
     "shots": [shot("hid", L("Bakku", "Vihar, ma kérlek… csak ma.", "zha_kakaslovas"))]},
    [shot("tojasko", L("Bakku", "Ma nem rontott el semmit.", "zha_kakaslovas"))], "Vihar a kis fehér tyúknak kukorékolt. Bakku Mimbunak.", style="folyo")

nov("nov_20_2", "Az idegen tojás",
    {"szentely": [32, 30], "piac": [12, 12], "harcos": [52, 46], "_": [60, 4]},
    [("szentely", "piac"), ("szentely", "harcos")],
    [U("zha_tolljos", "szentely", "Tuku-tuk, Tolljós", "tuku")],
    [reach("hal", "piac", "Hozz halpépet a piacról a fiókának", tag="tuku", marker="Piac"),
     reach("harcos", "harcos", "Keresd meg a Pinguu harcost", tag="tuku", after="hal", marker="Pinguu harcos"),
     escort("fioka", "fioka", "harcos", "Vidd a fiókát a harcoshoz", after="harcos", marker="Pinguu harcos")],
    {"brief": ["A 66. év, Fehértoll, a Keltetőszentély. Reggel huszonhárom tojás volt. Este huszonnégy.", "A nagy tojásból egy pingvinfióka kelt ki."],
     "shots": [shot("szentely", L("Tuku-tuk", "Csipog. Rám néz, és csipog.", "zha_tolljos"))]},
    [shot("harcos", L("Pinguu harcos", "A halpép borzalmas volt. Köszönöm.", "pin_hopajzsos"))], "Tuku-tuk nem felelt. A szemében könnyek voltak.", style="erdo",
    allies=ally("pinguu", "Fióka", [U("pin_kajakos", "szentely", "Pingvinfióka", "fioka")]),
    trig=[{"on_objective": "harcos", "join": "fioka"}])

nov("nov_20_3", "Ki csalta elő a hajnalt",
    {"vasarter": [32, 30], "tojas": [20, 28], "dob": [44, 28], "_": [60, 4]},
    [("tojas", "vasarter"), ("dob", "vasarter")],
    [U("zha_csirkepasztor", "tojas", "Tukki, tojásárus", "tukki"), U("oon_dobos", "dob", "Daiki, dobárus", "daiki")],
    [reach("t", "vasarter", "Tukki menjen a vásártér közepére", tag="tukki", marker="Vásártér"),
     reach("d", "vasarter", "Daiki is", tag="daiki", marker="Vásártér"),
     guard("mondak", "vasarter", 8, "Mondjátok el együtt a régi mondát", after=["t", "d"], marker="Vásártér")],
    {"brief": ["Az 59. év, Hét Lámpás, a tavaszi vásár. A vita a tojásárus és a dobárus pultja között kezdődött.", "A kakas csalta elő a hajnalt – vagy a dob?"],
     "shots": [shot("vasarter", L("Tukki", "A kakas! Kuk-Tuha csalta elő.", "zha_csirkepasztor"), L("Daiki", "A dob.", "oon_dobos"))]},
    [shot("vasarter", L("Daiki", "A kakas a dob tetején ült.", "oon_dobos"))], "A krónika szerint a régi mondában a kakas a dob tetején ült.", style="varos")


def main():
    os.makedirs(OUT, exist_ok=True)
    idx = []
    for m in M:
        with open(os.path.join(OUT, m["id"] + ".json"), "w", encoding="utf-8") as f:
            json.dump(m, f, ensure_ascii=False, indent=1)
        idx.append({"id": m["id"], "title": m["title"], "people": m["people"], "teaser": m["briefing"][0] if m["briefing"] else ""})
    with open(os.path.join(OUT, "index.json"), "w", encoding="utf-8") as f:
        json.dump({"missions": idx}, f, ensure_ascii=False, indent=1)
    print(f"novellak/: {len(M)} pálya")


if __name__ == "__main__":
    main()
