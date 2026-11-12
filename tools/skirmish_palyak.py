"""A csatapályák átdolgozása a skirmish-pályaszerkesztési útmutató szerint.

    python tools/skirmish_palyak.py      -> data/maps/*.json (frissítve) + docs/SKIRMISH_PALYALAPOK.md

Pályánként: a jellegzetes taktikai döntés a leírásban, pályalap (zónák, felállások), a tájhoz illő semleges
táborok (szintenként egy nép: pl. téli pályán Pinguu és Tokla, a vadon lényeivel nem keverve), díszletépületek
(tereptárgy), ártalmatlan háttérszereplők és – ahol a döntéshez kell – őrzött, gazdag bővítés.
A pozíciók a kezdőhelyhez képest értendők (d: a közép felé, s: oldalra; „pair”: két szomszéd között kifelé;
„centre”: a középtől), és minden kezdőhelynél ugyanúgy ismétlődnek.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = os.path.join(ROOT, "data", "maps")

PEOPLE_NAME = {"vadon": "Vadon", "pinguu": "Pinguu", "tokla": "Tokla", "ghoblin": "Ghoblin", "mhonyo": "Mhonyo",
               "daresh": "Daresh", "galoca": "Galóca", "niil_elf": "Niil-elf", "tuz_elf": "Tűz-elf",
               "tenduur": "Tendüür", "nila_uchana": "Nila-uchana", "zhan_zhama": "Zhan-zhama",
               "shrissh_matah": "Shrissh-matah"}


def D(i, at="base", d=0, s=0, ruin=False):
    o = {"id": i, "at": at, "d": d, "s": s}
    if ruin:
        o["ruin"] = True
    return o


def A(i, at="base", d=0, s=0, n=1, path=(), shelter=None, mode=None):
    o = {"id": i, "at": at, "d": d, "s": s, "n": n}
    if path:
        o["path"] = [{"at": p[0], "d": p[1], "s": p[2]} for p in path]
    if shelter:
        o["shelter"] = {"at": shelter[0], "d": shelter[1], "s": shelter[2]}
    if mode:
        o["mode"] = mode
    return o


SPEC = {
    "ket_part": dict(
        desc="Kis, gyors pálya: egy folyó választja el a bázisokat, három gázlóval. Döntés: a középső gázlót "
             "tartod, vagy a szélsőkön kerülsz a Zhan-zhama tanyák mellett?",
        dontes="Három gázló: a középső a legrövidebb, a két szélső kerülő a csirketanyák mellett visz el.",
        camps={"1": "zhan_zhama", "2": "zhan_zhama"},
        decor=[D("zha_feszekhaz", "base", 4, 12), D("zha_kakasudvar", "pair", 9, 7)],
        ambient=[A("zha_csirkepasztor", "base", 6, 12, n=2, mode="idle", shelter=("base", 4, 12))]),
    "hasadek": dict(
        desc="Középen sziklás hasadék, két szűk hágóval. Döntés: mindkét hágót őrzöd, vagy az egyiket elzárod, "
             "és a másikon mész át teljes erővel?",
        dontes="Két szűk hágó a hasadékon: a hágók ura a pálya ura; a peremen Tűz-elf tábor őrzi a kerülőt.",
        camps={"1": "tuz_elf", "2": "tuz_elf"},
        decor=[D("tuz_delibabtorony", "pair", 11, 8, ruin=True), D("tuz_dunepavilon", "base", 3, -12)],
        ambient=[A("tuz_homokszablyas", "base", 5, -12, n=2, mode="idle", shelter=("base", 3, -12))]),
    "kraterfold": dict(
        desc="Nyílt síkság, a közepén gazdag meteorkráterrel. Döntés: a kráter gyors elfoglalása, vagy a "
             "daresh-táborok őrizte, gazdag oldalsó lelőhelyek?",
        dontes="A kráter mindenkinek egyformán közel van; az oldalsó gazdag meteorhelyet Daresh-tábor őrzi.",
        rich="meteor",
        camps={"1": "daresh", "2": "daresh", "3": "daresh", "4": "daresh"},
        decor=[D("dar_agyagkaptar", "pair", 20, 0), D("dar_szarnyudvar", "base", 5, 13)],
        ambient=[A("dar_fullanclandzsas", "base", 6, 13, n=3, path=[("base", 6, 13), ("pair", 16, 0)],
                   shelter=("base", 5, 13))]),
    "osi_erdo": dict(
        desc="Sűrű rengeteg, keskeny ösvények – lesállásokhoz és rajtaütésekhez. Döntés: utat vágsz az erdőn "
             "át, vagy a Tendüür-ligetek mellett kerülsz?",
        dontes="Sűrű erdő keskeny ösvényekkel: a favágás egyben útépítés; a ligeteket Tendüür-táborok őrzik.",
        camps={"1": "tenduur", "2": "tenduur", "3": "tenduur", "4": "tenduur"},
        decor=[D("ten_lampasfa", "pair", 12, 0), D("ten_tovisudvar", "base", 4, -12)],
        ambient=[A("ten_lampasor", "pair", 10, 2, n=2, mode="idle", shelter=("pair", 12, 0))]),
    "jeghid": dict(
        desc="Befagyott tó a pálya közepén; a partok mentén és a szárazföldi hidakon át lehet támadni. A "
             "partokon Pinguu és Tokla táborok – a mélyben a vadon lényei.",
        dontes="A befagyott tó két oldalán vezet az út: a rövidebb a Pinguu-, a hosszabb a Tokla-tábor mellett.",
        camps={"1": "pinguu", "2": "tokla"},
        decor=[D("pin_jegcsarnok", "base", 5, 13), D("tok_maszkcsur", "base", 6, -13), D("pin_orlangszentely", "pair", 12, 7)],
        ambient=[A("pin_kajakos", "base", 7, 13, n=2, mode="idle", shelter=("base", 5, 13)),
                 A("tok_harangjaro", "base", 8, -13, n=1, path=[("base", 8, -13), ("pair", 9, 0)],
                   shelter=("base", 6, -13))]),
    "harmas_folyo": dict(
        desc="Három folyó három szektorra osztja a pályát; minden ágon két gázló. Döntés: a szomszéd felé "
             "nyitott, Galóca őrizte gazdag lelőhely, vagy a biztos, de szegényebb saját szektor?",
        dontes="Mindhárom szektor határán gazdag, őrzött lelőhely: aki elviszi, két gázlóra is figyelnie kell.",
        rich=True,
        camps={"1": "galoca", "2": "galoca"},
        decor=[D("gal_harapofeszek", "pair", 6, 6), D("gal_tuskekert", "base", 4, 12)],
        ambient=[A("gal_fogaskalap", "base", 6, 12, n=2, mode="idle", shelter=("base", 4, 12))]),
    "haromko": dict(
        desc="Három sziklatorony a közép körül; a meteor a háromszög közepén. Döntés: a középső meteor, vagy a "
             "tornyok tövében, Shrissh-matah rúnavárak között húzódó kerülő?",
        dontes="Egy gázló minden sziklakaron: a közép nyílt, a kerülők a rúnavárak alatt szűkek.",
        camps={"1": "shrissh_matah", "2": "shrissh_matah", "3": "shrissh_matah"},
        decor=[D("shr_tollvar", "pair", 14, 7), D("shr_runaszellozo", "base", 4, 12, ruin=True)],
        ambient=[A("shr_sziklafuto", "pair", 12, 4, n=2, mode="idle", shelter=("pair", 14, 7))]),
    "negy_sarok": dict(
        desc="Klasszikus négyes: a sarkokban bázisok, középen nyílt csatatér kráterrel. Döntés: korai "
             "terjeszkedés a kakasudvarok felé, vagy azonnal a kráter?",
        dontes="Nyílt közép, peremi bővítések: a két szomszéd között Ghoblin-bánya, a bázis mellett Zhan-zhama tanya.",
        camps={"1": "zhan_zhama", "2": "ghoblin"},
        decor=[D("zha_kakasudvar", "base", 4, 12), D("gho_ragoko_banya", "pair", 6, 0)],
        ambient=[A("zha_csirkepasztor", "base", 6, 12, n=2, mode="idle", shelter=("base", 4, 12)),
                 A("gho_koharapo", "pair", 4, 2, n=1, mode="idle", shelter=("pair", 6, 0))]),
    "keresztut": dict(
        desc="Négy tó között kereszt alakú utak – a kereszteződésben a meteor. A tavak között Mhonyo "
             "kovácsműhelyek; a gazdag lelőhely a szomszéd felé nyitott.",
        dontes="Kereszt alakú utak: a közép a leggyorsabb, a gazdag peremi lelőhelyet Mhonyo-tábor őrzi.",
        rich=True,
        camps={"1": "mhonyo", "2": "mhonyo", "3": "mhonyo"},
        decor=[D("mho_harompenge_kovacsda", "base", 5, -12), D("mho_parazsszentely", "base", 4, 12)],
        ambient=[A("mho_kardkovacs", "base", 7, -12, n=2, mode="idle", shelter=("base", 5, -12)),
                 A("mho_hajnalpenge", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
    "gyuruhegy": dict(
        desc="Sziklagyűrű öleli a pálya kincses közepét, négy kapuval. A gyűrű előtt Tokla harangkapuk. "
             "Döntés: befelé a kincsért, vagy kint, a szomszéd bővítései felé?",
        dontes="A gyűrű négy kapuja szűkület: aki a belsőt tartja, mind a négy kaput figyelnie kell.",
        camps={"1": "tokla", "2": "tokla", "3": "tokla"},
        decor=[D("tok_harangkapu", "pair", 4, 0), D("tok_zuzmaramuhely", "base", 4, -12)],
        ambient=[A("tok_harangjaro", "base", 6, -12, n=2, path=[("base", 6, -12), ("pair", 6, 3)],
                   shelter=("base", 4, -12))]),
    "tavak_videke": dict(
        desc="Apró tavak és gázlók szabdalt vidéke; sok kis csata, sok kerülőút. A tóparton Nila-uchana és "
             "Niil-elf táborok, a szomszédok között gazdag lelőhely.",
        dontes="Sok kis tó: rövid, de szűk gázlók és hosszabb, nyílt kerülők; a gazdag helyek a szomszéd felé nyitottak.",
        rich=True,
        camps={"1": "nila_uchana", "2": "niil_elf"},
        decor=[D("nil_lotuszudvar", "base", 4, 12), D("nii_lotuszkikoto", "pair", 12, 0)],
        ambient=[A("nii_nadhajos_ijasz", "pair", 10, 2, n=2, mode="idle", shelter=("pair", 12, 0)),
                 A("nil_naga", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
    "fent_es_lent": dict(
        desc="Csapatpálya (2–2): az északi és a déli szövetség között folyó, gázlókkal. A gázlóknál "
             "Niil-elf révészek.",
        dontes="Három gázló a folyón: a csapattárs gázlója a közös védelem, a középső a közös támadás útja.",
        camps={"1": "niil_elf", "2": "niil_elf"},
        decor=[D("nii_arvizszentely", "base", 4, 12)],
        ambient=[A("nii_papiruszlandzsas", "base", 6, 12, n=2, mode="idle", shelter=("base", 4, 12))]),
    "csillag": dict(
        desc="Öt sziklakar ágazik a középből – öt völgy, öt bázis. A karok tövében Ghoblin-bányák.",
        dontes="Öt völgy, sziklakarokkal elválasztva: két gázló vezet a szomszédhoz, a közép nyílt kráter.",
        camps={"1": "ghoblin", "2": "ghoblin", "3": "mhonyo"},
        decor=[D("gho_ragoko_banya", "base", 4, 12), D("gho_kocipo_malom", "pair", 14, 7)],
        ambient=[A("gho_koharapo", "base", 6, 12, n=2, mode="idle", shelter=("base", 4, 12))]),
    "hatszog": dict(
        desc="Hat bázis körben, a szomszédok között patakok – szabad mindenki-mindenki ellen. A patakok "
             "mentén gazdag, de két szomszéd által is elérhető lelőhelyek.",
        dontes="Mindenkinek két szomszédja van: a köztes gazdag hely jutalom, de két irányból is támadható.",
        rich=True,
        camps={"1": "tenduur", "2": "galoca"},
        decor=[D("ten_lampasfa", "base", 4, 12)],
        ambient=[A("ten_lampasor", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
    "mocsarvidek": dict(
        desc="Csapatpálya (3–3): ingoványos középföld, sok kis tó. A tavak között Nila-uchana és Niil-elf "
             "telepek.",
        dontes="Ingoványos közép, sok kis tóval: a csapat a széleken egyesülhet, a közép lassú és szétszabdalt.",
        camps={"1": "nila_uchana", "2": "niil_elf", "3": "nila_uchana"},
        decor=[D("nil_makara_medence", "base", 6, 11), D("nii_nadpalota", "base", 4, -12)],
        ambient=[A("nil_szornykoto", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
    "het_ut": dict(
        desc="Hét bázis, hét út a középső kráter felé – a wullof dalok szerint itt találkoznak a vándorok. Az "
             "utak mentén szellemtotemek.",
        dontes="Hét egyforma út a kráterhez: a középre mindenki egyszerre ér; a szomszéd felé a tábor az akadály.",
        camps={"1": "zhan_zhama", "2": "tenduur"},
        decor=[D("wul_szellemtotem", "base", 12, 3), D("wul_osok_kore", "pair", 12, 0, ruin=True)],
        ambient=[A("wul_gyujtogeto", "base", 10, 4, n=1, path=[("base", 10, 4), ("centre", 14, 2)],
                   shelter=("base", 12, 3))]),
    "nyolc_tuz": dict(
        desc="Nyolc tábortűz egy nagy körben – a legnagyobb mindenki-mindenki ellen pálya. A szomszédok "
             "között gazdag, őrzött lelőhelyek.",
        dontes="Nyolc tábortűz: a köztes gazdag lelőhelyekért két szomszéd verseng, a közép mindenkié.",
        rich=True,
        camps={"1": "ghoblin", "2": "zhan_zhama"},
        decor=[D("wul_harcosok_tuze", "base", 5, 12)],
        ambient=[A("zha_csirkepasztor", "base", 7, 12, n=1, mode="idle", shelter=("base", 5, 12))]),
    "csatamezo": dict(
        desc="Csapatpálya (4–4): két hosszú vonal, középen nyílt csatatér. A régi csaták romjai között "
             "Tűz-elf és Daresh táborok.",
        dontes="Nyílt csatatér: aki a romok fedezékét és a szélső táborokat birtokolja, oldalba kaphatja a frontot.",
        camps={"1": "tuz_elf", "2": "daresh"},
        decor=[D("elf_obeliszk", "base", 10, 6, ruin=True), D("imp_aquila_torony", "base", 11, -9, ruin=True)],
        ambient=[]),
    "egszakadas_kratere": dict(
        desc="Óriáskráter a világ közepén, tele csillagérccel; sziklafal védi, nyolc kapuval. A fal előtt "
             "Mhonyo és Shrissh-matah őrhelyek.",
        dontes="Nyolc kapu a kráterfalon: aki bent terjeszkedik, gazdag, de minden kaput védenie kell.",
        camps={"1": "mhonyo", "2": "shrissh_matah"},
        decor=[D("elf_napkapu", "pair", 6, 0, ruin=True), D("mho_szelpenge_udvar", "base", 4, 12)],
        ambient=[A("mho_szikraor", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
    "labirintus": dict(
        desc="Sziklafalak útvesztője – a felderítés itt életet ment. A zugokban Pinguu és Tokla táborok.",
        dontes="Útvesztő: a rövid út szűk és őrzött, a hosszú kerülő nyitott – előbb nézz, aztán lépj.",
        camps={"1": "pinguu", "2": "tokla", "3": "pinguu"},
        decor=[D("pin_orlangszentely", "base", 4, 12), D("tok_harangkapu", "pair", 10, 7, ruin=True)],
        ambient=[A("pin_hopajzsos", "base", 6, 12, n=1, mode="idle", shelter=("base", 4, 12))]),
}


def main():
    doc = ["# Csatapályák – pályalapok", "",
           "A `tools/skirmish_palyak.py` állítja elő. Pályánként: a jellegzetes döntés, felállások, a semleges "
           "táborok népei szintenként, díszletek és háttérszereplők. A táborok népe szintenként egyféle (nem "
           "keveredik a vadon lényeivel); a zsákmány a vadon azonos szintű táboráé (a hős felveheti).", ""]
    maps = []
    for fn in sorted(os.listdir(MAPS)):
        p = os.path.join(MAPS, fn)
        with open(p, encoding="utf-8") as f:
            md = json.load(f)
        mid = fn[:-5]
        sp = SPEC.get(mid)
        if not sp:
            continue
        md["desc"]["hu"] = sp["desc"]
        md["camps"] = sp["camps"]
        md["decor"] = sp["decor"]
        md["ambient"] = sp["ambient"]
        if sp.get("rich"):
            md["features"]["rich"] = sp["rich"]
        else:
            md["features"].pop("rich", None)
        lay = md.get("layout", "ring")
        n = md["players"]
        felall = ("csapat " + f"{n // 2}–{n // 2}") if lay == "rows" else \
            ("1v1" if n == 2 else f"mindenki-mindenki ellen ({n}), kevesebb játékossal is")
        md["plan"] = {"dontes": sp["dontes"], "felallasok": felall,
                      "taborok": {k: PEOPLE_NAME[v] for k, v in sorted(sp["camps"].items())}}
        with open(p, "w", encoding="utf-8") as f:
            json.dump(md, f, ensure_ascii=False, indent=1)
        maps.append((md.get("order", 99), mid, md, sp))
    for _, mid, md, sp in sorted(maps):
        camps = ", ".join(f"{k}: {PEOPLE_NAME[sp['camps'].get(k, 'vadon')]}" for k in "12345")
        doc += [f"## {md['name']['hu']} (`{mid}`)", "",
                f"- **Méret, játékosok, elrendezés:** {md['size']}×{md['size']}, {md['players']} fő, {md.get('layout', 'ring')}, "
                f"táj: {md.get('theme', 'forest')}",
                f"- **Felállások:** {md['plan']['felallasok']}",
                f"- **Döntés:** {sp['dontes']}",
                f"- **Táborok szintenként (1 könnyű … 5 főnök):** {camps}",
                f"- **Gazdag, őrzött bővítés:** {'igen' if sp.get('rich') else 'nincs'}",
                f"- **Díszlet:** {', '.join(d['id'] + (' (rom)' if d.get('ruin') else '') for d in sp['decor']) or '–'}",
                f"- **Háttérszereplők:** {', '.join(a['id'] + ' ×' + str(a['n']) for a in sp['ambient']) or '–'}", ""]
    with open(os.path.join(ROOT, "docs", "SKIRMISH_PALYALAPOK.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(doc) + "\n")
    print(f"{len(maps)} pálya frissítve")


if __name__ == "__main__":
    main()
