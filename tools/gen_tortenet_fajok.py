"""Fajleírás a történetíráshoz: minden nép működése, egységei, épületei, hősei, kutatásai és mechanikái.

    python tools/gen_tortenet_fajok.py

Kimenet: docs/tortenet/FOFAJOK.md (a hét főfaj; modok nélkül) és docs/tortenet/SZOVETSEGES_NEPEK.md.
A számok és leírások a játék adataiból jönnek (mindig a mostani állapotot mutatják); a „Hogyan működik”
fejezetek kézzel írt összefoglalók – ha egy mechanika változik, itt is frissíteni kell.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from rts import data  # noqa: E402

OUT = os.path.join(ROOT, "docs", "tortenet")
MAIN = ["elves", "ursan", "croftian", "wullof", "imperium", "hukucha", "ooni"]

UTYPE = {"kozelharcos": "közelharcos", "landzsas": "lándzsás", "lovas": "lovas / hátas", "lovesz": "lövész",
         "munkas": "munkás", "orias": "óriás", "ostrom": "ostromgép", "repulo": "repülő", "varazslo": "varázsló"}

# ---------------------------------------------------------------------------------------------------------
# Kézzel írt összefoglalók: hogyan működik a nép a játékban, és mit jelent ez a történetben.
MECH = {
    "elves": [
        ("Energiamező", "Az épületek csak a Napobeliszkek energiamezejében működnek; az obeliszk egyben a létszámot "
                        "is adja. Ha egy obeliszk elesik, a mezején kívül rekedt épületek leállnak."),
        ("Maguktól épülő templomok", "A Kőfaragó csak lerakja az alapkövet, a templom magától nő ki a földből, a "
                                     "munkás pedig azonnal továbbmegy."),
        ("Beidézés", "Az új egységek nem az épületből lépnek ki, hanem a gyülekezőponthoz legközelebbi "
                     "Napobeliszknél jelennek meg."),
        ("Töltődő pajzs", "Minden elf egységnek pajzsa van, amely harcon kívül magától visszatöltődik. Kevés, "
                          "drága, de tartós sereg."),
        ("Áhítat", "A Napszentélyhez vagy Naptemplomhoz küldött Kőfaragók fohászkodnak, és Áhítatot gyűjtenek. "
                   "Ebből mennek a Nap csodái (Nap kegyelme, Napáldás, Napkitörés) és az Égi kegy (Napcsóva)."),
        ("Napfúró", "A bányára idézett fúrótorony magától is termel, és legfeljebb 3 Kőfaragó dolgozhat benne, "
                    "hordás nélkül."),
        ("Raptorok", "Az alapsereg gerince a Raptorlovas; a Páncélos raptor a nehéz változata."),
    ],
    "ursan": [
        ("Helyőrség", "A Nagyterem, a Hosszúház, a Harcosok háza és az Őrtorony egységeket fogad be: a bent lévők "
                      "védve vannak, és fejenként erősítik az épület lövéseit. Riadóra a telepesek is behúzódnak."),
        ("Telepesek", "A Telepes épít, gyűjt és javít; a Népfelkelés kutatással és a Fegyverbe! riadóval harcossá "
                      "válik. Az Esküfa-riadó (Égi kegy) az egész falut felfegyverzi 30 másodpercre."),
        ("Tanyák", "A létszámot a tanyák adják; a Kiadós tanya kutatás bővíti."),
        ("Pajzsfal és formációk", "Lassú, kemény gyalogság: Pajzsfal, Dárdafal, rúnakövek, cölöpfal. "
                                   "A varázslatok nem manából, hanem töltődési időből mennek."),
        ("Az ursanok", "Medveemberek: nem viselnek sisakot, nagy, lapos deszkapajzsuk és viking–jurta stílusú "
                       "épületeik vannak."),
    ],
    "croftian": [
        ("Mélység", "Az épületek a mélységet (élő talajt) terítik maguk köré, és csak azon építhetők. A mélységen "
                    "a croftianok gyógyulnak és gyorsabbak; a rajta álló fák lassan elrothadnak, és ebből fa folyik "
                    "a népnek (legfeljebb 6,5 fa/mp, kutatással 9)."),
        ("Lárvák", "Minden egység a Kaptár lárváiból kel ki, egyszerre többféle is."),
        ("Csápszolga", "A munkás nem hord nyersanyagot: beleolvad a Fertőzött tárnába, és onnan termel. "
                       "Mérgező tüskéket is szúr a földbe (lassítanak és mérgeznek minden nem croftian egységet)."),
        ("Beásás és lesből támadás", "A beásott egységek láthatatlanok és gyorsan gyógyulnak; csak felfedő "
                                     "(Szemhordozó, torony) vagy a nagyon közeli ellenség látja őket."),
        ("Létszám élő egységekből", "A létszámot a Szemhordozók adják – ha meghalnak, a raj is zsugorodik."),
        ("Mutáció és fertőzés", "A Karmos gubóban Páncélbogárrá mutálódik; a mélységen elesett élő ellenségből "
                                "Karmos kelhet ki. A varázslatok a saját életerőből mennek."),
    ],
    "wullof": [
        ("Nomád tábor", "Az épületek felszedhetők, vándorszekérre rakva máshová vihetők, és 8 mp alatt újra "
                        "felállnak. A Gyűjtögető belép az épülőbe, és belülről építi."),
        ("Falka", "Az együtt harcoló egységek erősebbek (közelben álló társanként +10%, legfeljebb +40%); a falka "
                  "kapcsolóval terelhető együtt."),
        ("Vadászzsákmány", "Minden ölés fát hoz; a csapolt fa hordás nélkül ad fát."),
        ("Erdei rejtőzés", "Fák mellett láthatatlanok, amíg nem támadnak."),
        ("Emuk", "A létszámot emuk adják, amelyeket Emukarámban tenyésztenek (karámonként legfeljebb 5)."),
        ("Ének és Hold", "A varázslatok énekből mennek; a Holdtölte kutatással 4 percenként 1 percig +15% sebzés. "
                         "A bölényre a Bölénylovaglás kutatás után ülhet fel a gyalogos."),
    ],
    "imperium": [
        ("Lélekhorgony", "Az elesett gépek lelke 40 mp-ig horgonyban marad: egy Fabrikátor az eredeti ár 35%-áért "
                         "újraépítheti. A népet „a halhatatlan légió” teszi."),
        ("Lélekidézés", "Az elesettek tetemeiből szellemlégiósok kelnek; az idézett szellemek nem foglalnak "
                        "létszámot."),
        ("Alapozás", "A Fabrikátor márvány lapkövet rak le; alapozáson az épület teljes életerővel áll, alapozás "
                     "nélkül csak félerővel, és lassan kopik."),
        ("Gépek és javítás", "A Fabrikátorok javítják a gépeket; a Kolosszus ostrom-módba telepíthető. Az Arató "
                             "lánctalpas gépként bányássza az aranyat és a meteoritot."),
        ("Reaktorok és szobrok", "A létszámot a lélekreaktorok adják, pajzs-zónát is vetítenek. A három "
                                 "isten-szobor (Ferrox, Arcanor, Umbrax) bónuszt ad, amíg áll."),
    ],
    "hukucha": [
        ("Tavacskák", "Minden épület tavacskára épül. A Tóásó ássa ki (ott kell állnia, amíg ás), és a "
                      "tavacskák összefüggő tavat alkotnak. A saját tavacskán a békák gyorsabbak."),
        ("Maguktól növő otthonok", "A Tóásó elindítja az építkezést és továbbmegy; az épület magától nő, és "
                                   "minél több tavacska veszi körül, annál gyorsabban (legfeljebb +60%)."),
        ("Tóének", "A Tóteremtő szentélyhez küldött Tóásók a Tóanyának énekelnek. Ebből mennek a tó csodái "
                   "(Áradás, Gyöngyérés, tavi lények hívása) és az Égi kegy (Árhullám)."),
        ("Idézés és ikrák", "A mágusok lábon járó tavi lényeket idéznek; az Ikrarakó csomóiból ebihalak kelnek ki."),
        ("Harcmodor", "Csúzli vagy tompa végű, kampós lapos kard, pajzs nélkül; a Kagylópajzsos a kivétel."),
    ],
    "ooni": [
        ("Lámpásfény", "A lámpások és épületek fénykörében az egységek gyógyulnak; a fény határozza meg, hol "
                       "építhet a nép."),
        ("Harci kedv", "Minden egységnek harci kedve van: sebesüléskor, társ vagy hős halálakor fogy, nyugalomban "
                       "visszatölt. Ha elfogy, az egység megtörik: gyengébben üt és menekül, amíg vissza nem tér a "
                       "bátorsága. A Taiko-dobos ritmusa visszaadja, a lámpásfény védi."),
        ("Dobos építés", "A dobos ritmusa a közelben gyorsítja az építkezést (+50%)."),
        ("Szakosodott gyűjtők", "A Favágó oni fát vág, a Tanuki aranyat és meteoritot hord; a Lámpásvivő épít."),
        ("Torii-kapuk", "A kapupár egyik felét lámpásfényben, a másikat bárhová lehet tenni, ahol látsz; a két kapu "
                        "között átkel a sereg."),
        ("Kőkutyák", "A Kőkutya-őr mellett álló épület páncélt kap (kutatással még többet)."),
    ],
    "mycel": [
        ("Spórafonal-hálózat", "Az épületek csak spórafonalakkal a Szívgombához kötve működnek, de bármelyikbe "
                               "leadható a nyersanyag; a hálózat minden épülete +1 létszámot ad."),
        ("Összeolvadás", "Három Pöfeteg Óriáspöfeteggé olvad."),
        ("Spóra", "A varázslatok spórából mennek."),
    ],
}

HOOKS = {
    "elves": "Fény, rend, áldozat: egy kicsi, de tökéletes nép, amelynek minden harcosa pótolhatatlan. A hit "
             "(Áhítat) szó szerint a munkából fakad – aki imádkozik, nem gyűjt.",
    "ursan": "Eskü, közösség, szívósság: a falu maga a hadsereg. A telepes és a harcos ugyanaz a medve – "
             "riadóra mindenki fegyvert ragad.",
    "croftian": "Éhség és terjeszkedés: a raj megemészti a tájat, a fák elrothadnak alatta. A munkás szó szerint "
                "feloldódik a bányában.",
    "wullof": "Vándorlás, falka, vadászat: a tábor sosem végleges, a zsákmány az élelem. A Hold ritmusa a harc ritmusa.",
    "imperium": "Halhatatlanság és ár: a gépek lelke visszatér, de minden újjászületésnek ára van. Rend, alap, "
                "márvány – ami alapozás nélkül épül, az lassan elkopik.",
    "hukucha": "A tó mint otthon és fegyver: a nép maga ássa ki a világát, és dalban kéri a Tóanya kegyét.",
    "ooni": "Bátorság és fény: a sötétben a harci kedv elfogy, a lámpás és a dob tartja egyben a sereget.",
    "mycel": "A hálózat mint emlékezet: egyetlen gomba sem él egyedül, a fonal köti össze őket.",
}


def esc(t):
    return str(t or "").replace("|", "/").replace("\n", " ").strip()


def cost(c):
    names = {"wood": "fa", "stone": "arany", "meteor": "meteorit", "faith": "áhítat"}
    return ", ".join(f"{v} {names.get(k, k)}" for k, v in (c or {}).items()) or "–"


def lore_sections():
    """FACTIONS_LORE.md: népnév -> kézzel írt háttér."""
    p = os.path.join(ROOT, "docs", "FACTIONS_LORE.md")
    out = {}
    if os.path.isfile(p):
        txt = open(p, encoding="utf-8").read()
        for m in re.finditer(r"^## (.+?)\n(.*?)(?=^## |\Z)", txt, re.S | re.M):
            out[m.group(1).split("–")[0].strip()] = m.group(2).strip()
    return out


def ability_line(d, aid):
    a = d.abilities.get(aid)
    if not a:
        return None
    return f"**{esc(a['name'])}** – {esc(a.get('desc'))}"


def unit_table(d, f, units, W):
    W("| Egység | Típus | Ár | Életerő / páncél | Támadás | Leírás |")
    W("|---|---|---|---|---|---|")
    for uid, u in units:
        at = u.get("attack") or {}
        atk = (f"{at.get('damage', 0)} sebzés, {('közel' if at.get('range', 0) < 1 else str(at.get('range')) + ' lőtáv')}"
               if at else "–")
        typ = UTYPE.get(u.get("utype"), u.get("utype") or "–")
        if u.get("flying"):
            typ += ", repül"
        if u.get("summoned"):
            typ += ", idézett"
        W(f"| {esc(u['name'])} | {typ} | {cost(u.get('cost'))} | {u.get('hp', '–')} / {u.get('armor', 0)} | {atk} | "
          f"{esc(u.get('desc'))} |")
    W("")


def abilities_block(d, units, W):
    seen = []
    for _, u in units:
        for a in (u.get("abilities") or []):
            if a not in seen:
                seen.append(a)
    lines = [ability_line(d, a) for a in seen]
    lines = [ln for ln in lines if ln]
    if lines:
        W("**Az egységek képességei**")
        W("")
        for ln in lines:
            W(f"- {ln}")
        W("")


def faction_doc(d, f, W, lore, ally=False):
    fid = f["id"]
    W(f"## {f['name']}" + (" (mod-nép)" if fid == "mycel" else ""))
    W("")
    W(f"*{esc(f.get('tagline'))}*")
    W("")
    W(esc(f.get("desc")))
    W("")
    lt = lore.get(f["name"])
    if lt:
        W("**Háttér (FACTIONS_LORE.md):**")
        W("")
        W(lt)
        W("")
    if fid in HOOKS:
        W(f"**A történet szempontjából:** {HOOKS[fid]}")
        W("")
    defs = d.defs
    if not ally:
        W("### Alapadatok")
        W("")
        W(f"- **Főépület:** {defs[f['hq']]['name']}  •  **Munkás:** {defs[f['worker']]['name']}  •  "
          f"**Létszám:** {esc(f.get('supply_desc'))}")
        sr = {"energy": "energia", "cooldown": "töltődési idő (nincs mana)", "hp": "saját életerő", "song": "ének",
              "spores": "spóra"}.get(f.get("spell_resource"), f.get("spell_resource") or "–")
        if f.get("spell_resource"):
            W(f"- **A varázslatok forrása:** {sr}")
        if f.get("faith"):
            fa = f["faith"]
            W(f"- **{fa.get('name')}:** {esc(fa.get('desc'))}")
        W("")
    if fid in MECH:
        W("### Hogyan működik")
        W("")
        for t, txt in MECH[fid]:
            W(f"- **{t}.** {txt}")
        W("")
    if ally and f.get("ally_pack"):
        W("### A szövetség")
        W("")
        W("A Diplomácia csarnokában köthető meg (egyszerre egy szövetséges nép). Ezután a saját bázison megépíthetők "
          "a nép épületei, és képezhetők az egységei:")
        W("")
        pk = f["ally_pack"]
        W("- **Épületek:** " + ", ".join(defs[b]["name"] for b in pk.get("buildings", []) if b in defs))
        W("- **Egységek:** " + ", ".join(defs[u]["name"] for u in pk.get("units", []) if u in defs))
        W("")
    # irányok
    if f.get("paths"):
        W("### Fejlődési irányok (Felemelkedéskor egyet kell választani, véglegesen)")
        W("")
        for pid, pth in f["paths"].items():
            extra = []
            if pth.get("building") in defs:
                extra.append("épület: " + defs[pth["building"]]["name"])
            us = [defs[u]["name"] for u in pth.get("units", []) if u in defs]
            if us:
                extra.append("egység: " + ", ".join(us))
            W(f"- **{esc(pth.get('name'))}** – *{esc(pth.get('tagline'))}*. {esc(pth.get('desc'))}"
              + (f" ({'; '.join(extra)})" if extra else ""))
        W("")
    units = list(f.get("units", {}).items())
    heroes = [(k, u) for k, u in units if u.get("hero")]
    normal = [(k, u) for k, u in units if not u.get("hero")]
    if heroes:
        W("### Hősök")
        W("")
        for k, u in heroes:
            W(f"#### {u['name']}")
            W("")
            W(esc(u.get("desc")))
            W("")
            for a in u.get("hero_abilities", []):
                ln = ability_line(d, a)
                if ln:
                    W(f"- {ln}")
            W("")
    if normal:
        W(f"### Egységek ({len(normal)})")
        W("")
        unit_table(d, f, normal, W)
        abilities_block(d, normal, W)
    blds = list(f.get("buildings", {}).items())
    if blds:
        W(f"### Épületek ({len(blds)})")
        W("")
        W("| Épület | Ár | Képez | Leírás |")
        W("|---|---|---|---|")
        for bid, b in blds:
            prod = ", ".join(defs[u]["name"] for u in (b.get("produces") or []) if u in defs) or "–"
            extra = []
            if b.get("garrison"):
                extra.append(f"helyőrség: {b['garrison']} fő")
            for pw in b.get("powers") or []:
                a = d.abilities.get(pw)
                if a:
                    extra.append(f"csoda: {a['name']} ({esc(a.get('desc'))})")
            desc = esc(b.get("desc")) + ((" " + "; ".join(extra) + ".") if extra else "")
            W(f"| {esc(b['name'])} | {cost(b.get('cost'))} | {prod} | {desc} |")
        W("")
    res = []
    for bid, b in blds:
        for rid in b.get("researches") or []:
            r = d.researches.get(rid)
            if r:
                res.append((r, b["name"]))
    if res:
        W(f"### Kutatások ({len(res)})")
        W("")
        W("| Kutatás | Hol | Hatás |")
        W("|---|---|---|")
        for r, where in res:
            W(f"| {esc(r['name'])} | {esc(where)} | {esc(r.get('desc'))} |")
        W("")


def common(d, W):
    from rts import tips
    W("## Közös szabályok (minden népre)")
    W("")
    W("- **Nyersanyagok:** fa, arany és meteorit. A meteorit ritka: a különleges egységek, a hősök és a "
      "legerősebb kutatások ára. Két népnek (Szürke elfek, Huk’ucha) saját hit-nyersanyaga is van (Áhítat, Tóének).")
    W("- **Felemelkedés:** amint a főépületen kívül 3 kész épület áll, a nép felemelkedik, és **végleg** választ "
      "három fejlődési irány közül. A hadjáratban nincs irányzár.")
    W("- **Hősök:** népenként 3 hős, egyszerre legfeljebb 3; szintet lépnek, 4 képességük van (a negyedik a "
      "végső), tárgyakat viselnek, elesve a főépületben újraéleszthetők.")
    W("- **Égi kegy:** a főépület gombjáról indítható, néphez kötött csapás vagy áldás (Napcsóva, Esküfa-riadó, "
      "Mélységi kitörés, Ősök hívása, Ordan villáma, Árhullám, Isteni szél); csak látható pontra.")
    W("- **Diplomácia csarnoka:** egy szövetséges nép választható; az ő épületei és egységei a saját bázison "
      "épülnek (lásd SZOVETSEGES_NEPEK.md).")
    W("- **Égkő:** lerakott meteoritszilánk; a fényében a varázslók erősebbek, a meteoritrokon népek gyógyulnak – "
      "mindenkire hat, és bárki lerombolhatja.")
    W("- **Erősség és gyengeség:** minden egységtípusnak van erőssége és gyengéje (pl. a lovasság a lövészek "
      "ellen erős, a lándzsa ellen gyenge); a sebzés- és páncélkutatásoknak három szintje van (a III. a "
      "Felemelkedés után).")
    W("")
    W("**A játékon belüli tippek a játékmenetről (a játék saját megfogalmazásában):**")
    W("")
    for t in tips.TIPS[12:]:  # az irányítási (billentyű-) tippek nélkül
        W(f"- {t}")
    W("")


def main():
    os.makedirs(OUT, exist_ok=True)
    d = data.load(enabled_mods=[])  # csak az alapjáték (a modok – pl. a játszható Micéliák – nem főfajok)
    lore = lore_sections()
    md = []
    W = md.append
    W("# A főfajok – működés, egységek, épületek, mechanikák")
    W("")
    W("*© Mesehordo – TOTU: Sky Rift. A fájlt a `python tools/gen_tortenet_fajok.py` állítja elő a játék adataiból "
      "(a számok mindig a mostani állapotot mutatják). Történetíráshoz: mit tud, hogyan harcol és mire épül "
      "minden nép.*")
    W("")
    W("Tartalom: " + " • ".join(d.factions[f]["name"] for f in MAIN))
    W("")
    common(d, W)
    for fid in MAIN:
        faction_doc(d, d.factions[fid], W, lore)
    open(os.path.join(OUT, "FOFAJOK.md"), "w", encoding="utf-8").write("\n".join(md) + "\n")
    md2 = []
    W = md2.append
    W("# A szövetséges népek – működés, egységek, épületek")
    W("")
    W("*© Mesehordo – TOTU: Sky Rift. A fájlt a `python tools/gen_tortenet_fajok.py` állítja elő a játék adataiból.*")
    W("")
    W("A szövetséges népek nem játszhatók önállóan: a főfajok a **Diplomácia csarnokában** köthetnek velük "
      "szövetséget (ára 150 arany és 50 meteorit), egyszerre egy néppel. Ezután a nép néhány épülete a saját "
      "bázison felépíthető, és ott képezhetők az egységei; a nép saját kutatásai is elérhetővé válnak.")
    W("")
    allies = d.allies()
    W("| Nép | Jelmondat | Épület | Egység |")
    W("|---|---|---|---|")
    for f in allies:
        W(f"| {f['name']} | {esc(f.get('tagline'))} | {len(f.get('buildings', {}))} | {len(f.get('units', {}))} |")
    W("")
    for f in allies:
        faction_doc(d, f, W, lore, ally=True)
    open(os.path.join(OUT, "SZOVETSEGES_NEPEK.md"), "w", encoding="utf-8").write("\n".join(md2) + "\n")
    print("docs/tortenet/FOFAJOK.md, docs/tortenet/SZOVETSEGES_NEPEK.md:",
          sum(len(x) for x in md) // 1000, "+", sum(len(x) for x in md2) // 1000, "ezer karakter")


if __name__ == "__main__":
    main()
