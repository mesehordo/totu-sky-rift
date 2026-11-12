"""„Mire jó / Mit tud / Adatok” – a grafikai lista és az adatlapok játékbeli ismertetője minden elemhez.

info(d, kategória, azonosító) -> (mire_jó, mit_tud, adatok)   (d = rts.data.load())
"""

RES_HU = {"wood": "fa", "stone": "arany", "meteor": "meteorit"}


def cost_txt(c):
    return ", ".join(f"{v} {RES_HU.get(k, k)}" for k, v in (c or {}).items()) or "ingyenes"


def _abilities(d, ids):
    out = []
    for a in ids:
        ab = d.abilities.get(a)
        if ab:
            out.append(f"{ab.get('name', a)} – {ab.get('desc', '')}".strip(" –"))
    return out


def _name(d, did):
    return d.defs.get(did, {}).get("name", did)


def unit_info(d, u):
    role = u.get("desc", "")
    tags = u.get("tags", [])
    kind = []
    if u.get("hero"):
        kind.append("hős (szintet lép, tárgyakat visel, 4 képesség)")
    if "worker" in tags:
        kind.append("munkás (gyűjt, épít, javít)")
    if u.get("flying"):
        kind.append("repülő")
    if "livestock" in tags:
        kind.append("jószág – nem harcol")
    if u.get("unit_supply"):
        kind.append(f"+{u['unit_supply']} létszámot ad")
    f = d.factions.get(u.get("faction"), {})
    pth = f.get("paths", {}).get(u.get("path"), {}).get("name")
    if pth:
        kind.append(f"csak a(z) „{pth}” fejlődési irányban")
    prod = [b["name"] for b in f.get("buildings", {}).values() if u["id"] in b.get("produces", ())]
    if prod:
        kind.append("képzi: " + ", ".join(prod))
    elif not u.get("hero"):
        kind.append("nem képezhető közvetlenül (idézett, átalakulással vagy felültetéssel jön létre)")
    can = _abilities(d, u.get("abilities", []) + u.get("hero_abilities", []))
    a = u.get("attack")
    st = [f"ár: {cost_txt(u.get('cost'))}", f"létszám: {u.get('supply', 1):g}", f"életerő: {u.get('hp')}",
          f"páncél: {u.get('armor', 0)}", f"sebesség: {u.get('speed', 0)}", f"látótáv: {u.get('sight', 0)}"]
    if a:
        tg = "föld+lég" if "air" in a.get("targets", []) else "föld"
        st.append(f"sebzés: {a.get('damage')} ({'közelharc' if a.get('range', 0) < 1 else 'táv ' + str(a['range'])}, "
                  f"{tg}, {a.get('cooldown')} mp)")
    else:
        st.append("nem támad")
    return (role + (" (" + "; ".join(kind) + ")" if kind else "")), " | ".join(can), "; ".join(st)


def building_info(d, b):
    role = b.get("desc", "")
    can = []
    if b.get("produces"):
        can.append("képez: " + ", ".join(_name(d, x) for x in b["produces"]))
    if b.get("researches"):
        can.append("kutat: " + ", ".join(d.researches.get(x, {}).get("name", x) for x in b["researches"]))
    if b.get("supply"):
        can.append(f"+{b['supply']} létszám")
    if b.get("dropoff"):
        can.append("leadóhely: " + ", ".join(RES_HU.get(x, x) for x in b["dropoff"]))
    if b.get("attack"):
        a = b["attack"]
        can.append(f"torony: {a.get('damage')} sebzés, {a.get('range')} táv")
    if b.get("packable"):
        can.append("felszedhető és máshol letelepíthető")
    if b.get("garrison"):
        can.append(f"helyőrség: {b['garrison']} egység fér be")
    if b.get("altar"):
        can.append("hősidéző")
    f = d.factions.get(b.get("faction"), {})
    pth = f.get("paths", {}).get(b.get("path"), {}).get("name")
    if pth:
        can.append(f"csak a(z) „{pth}” fejlődési irányban")
    if b.get("requires"):
        can.append("feltétel: " + ", ".join(_name(d, x) for x in b["requires"]))
    st = [f"ár: {cost_txt(b.get('cost'))}", f"építési idő: {b.get('time')} mp", f"életerő: {b.get('hp')}",
          f"páncél: {b.get('armor', 0)}", f"alapterület: {b['size'][0]}×{b['size'][1]} csempe"]
    return role, " | ".join(can), "; ".join(st)


def info(d, cat, ident):
    if cat in ("Egység", "Egység-ikon", "Arckép") and ident in d.defs:
        r, c, s = unit_info(d, d.defs[ident])
        if cat == "Egység-ikon":
            return "Az egység ikonja a képzés-menüben és a kijelölésnél. " + r, c, s
        if cat == "Arckép":
            return "Az egység arcképe a kijelölő panelen (hősnél a bal felső sarokban is). " + r, c, s
        return r, c, s
    if cat in ("Épület", "Épület-ikon") and ident in d.defs:
        r, c, s = building_info(d, d.defs[ident])
        return ("Az épület ikonja az építés-menüben és a kijelölésnél. " + r if cat == "Épület-ikon" else r), c, s
    if cat == "Képesség-ikon":
        ab = d.abilities.get(ident, {})
        owners = [u["name"] for u in d.defs.values() if u.get("kind") == "unit"
                  and ident in u.get("abilities", []) + u.get("hero_abilities", [])]
        st = []
        for k, lab in (("cooldown", "újratöltés"), ("energy", "energia"), ("range", "táv"), ("radius", "sugár"),
                       ("damage", "sebzés"), ("duration", "időtartam")):
            if ab.get(k):
                st.append(f"{lab}: {ab[k]}")
        return (f"Képesség a parancskártyán ({ab.get('hotkey', '')} billentyű). " + ab.get("desc", ""),
                "kinek van: " + ", ".join(owners), "; ".join(st))
    if cat == "Kutatás-ikon":
        r = d.researches.get(ident, {})
        where = [b["name"] for b in d.defs.values() if b.get("kind") == "building" and ident in b.get("researches", ())]
        return ("Fejlesztés: " + r.get("desc", ""), "hol: " + ", ".join(where),
                f"ár: {cost_txt(r.get('cost'))}; idő: {r.get('time')} mp" + ("; kizáró (csak egy választható)"
                                                                            if r.get("exclusive") else ""))
    if cat == "Tárgy-ikon":
        it = d.items.get(ident, {})
        return "Hős-tárgy a leltárban: " + it.get("desc", ""), "", ""
    if cat == "Címer":
        f = d.factions.get(ident, {})
        return f"A(z) {f.get('name', ident)} faj jelképe: menük, beállítóképernyő, zászlók.", f.get("tagline", ""), ""
    if cat == "Irány-címer":
        fid, _, pid = ident.partition("_")
        pd = d.factions.get(fid, {}).get("paths", {}).get(pid, {})
        return ("Fejlődési irány jelképe a Felemelkedés-ablak kártyáján. " + pd.get("desc", ""),
                pd.get("tagline", ""), "")
    if cat == "Nyersanyag":
        g = d.game["resources"].get(ident, {})
        return (f"Gyűjthető nyersanyag: {g.get('name', ident)}.", "a munkások gyűjtik",
                f"egy forduló: {g.get('carry', '')}; gyűjtési idő: {g.get('gather_time', '')} mp")
    return "", "", ""
