"""docs/FACTIONS.md előállítása a játékadatokból (egységek, hősök, képességek, mechanikák).

    python tools/gen_faction_doc.py

A kézzel írt háttérszövegek a docs/FACTIONS_LORE.md-ben vannak; ez a fájl a végére kerül.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from rts import data  # noqa: E402

MECH = {
    "elves": "**Kevés, de tökéletes.** Elit, drága egységek töltődő pajzzsal; Fénylépés (teleport) a harcosoknál. "
             "Az új egységek a gyülekezőponthoz legközelebbi Napobeliszknél jelennek meg (beidézés). "
             "**Energiamező és pajzs.** Az épületek csak Napobeliszk mezőjében működnek; a Kőfaragó lerakja az alapot, "
             "a templom magától nő ki, a munkás azonnal továbbmegy. Minden egységnek töltődő pajzsa van.",
    "ursan": "**A fal és az eskü.** Lassú, kemény gyalogság, formációk (Pajzsfal, Dárdafal), rúnakövek, cölöpfal. "
             "**Helyőrség és tanyák.** A Nagyterem, a Hosszúház, a Harcosok háza és az Őrtorony helyőrséget fogad: a bent "
             "lévők védve vannak, és mindegyikük erősíti az épület lövéseit (riadóra a telepesek is behúzódnak). A Telepes "
             "végig építi és javítja az épületeket. Az ursanok nem viselnek sisakot, legfeljebb ágyékkötőt; nagy, lapos "
             "deszkapajzsuk és viking–jurta épületeik vannak.",
    "croftian": "**A raj.** Olcsó, gyors egységek, lesből támadás: a Beásással láthatatlanok és gyorsan gyógyulnak "
                "(csak felfedő – Szemhordozó, torony – vagy a nagyon közeli ellenség látja őket). "
                "**Mélység, lárvák, mutáció, fertőzés.** Minden egység a Kaptár lárváiból kel ki, párhuzamosan; a mélységen "
                "gyógyulnak és gyorsabbak. A Karmos gubóban Páncélbogárrá mutálódik; a mélységen elesett élő ellenségből "
                "Karmos kelhet ki.",
    "wullof": "**Üss és fuss.** Gyors egységek, erdei rejtőzés (fák mellett láthatatlanok, amíg nem támadnak), énekek. "
              "**Nomád táborok, falka, vadászzsákmány.** Az épületek (a Kéregkunyhók és az Ősök köre is) felszedhetők és "
              "vándorszekéren máshová vihetők; az együtt harcoló falka erősebb; minden ölés zsákmányt (fát) hoz.",
    "mycel": "**A növekvő hálózat.** Osztódás: a harcos egységek X legyőzött ellenség után kettéosztódnak (a pöttyök "
             "az egység fölött mutatják, mennyit „evett”); a varázslatok spórából mennek. **Spórafonal-hálózat.** Az épületeket fonalak kötik össze; bármelyik összekötött épület leadóhely; a kicsi "
             "Szívgomba sokszor megépíthető, és egyben védőépület. A pöfetegek összeolvadhatnak.",
    "imperium": "**A halhatatlan légió.** Az elesett gépek lelke 40 mp-ig lélekhorgonyban marad: egy Fabrikátor az eredeti "
                "ár 35%-áért újraépítheti. **Lélekidézés, gépjavítás, ostrom.** Az elesettek tetemeiből szellemlégiósok kelnek; a gépeket a "
                "Fabrikátorok javítják; a Kolosszus ostrom-módba telepíthető. Az idézett szellemek nem foglalnak létszámot.",
}


def cost(c, rd):
    return ", ".join(f"{v} {rd[k]['name'].lower()}" for k, v in c.items()) or "–"


def main():
    d = data.load(use_mods=False)
    rd = d.game["resources"]
    ab = d.abilities
    out = ["# A fajok (2. változat)", "",
           "*© 2026 Mesehordo – a fajok világa és leírásai: minden jog fenntartva (lásd [LICENSE.md](../LICENSE.md)). "
           "Ez a fájl a játékadatokból készül: `python tools/gen_faction_doc.py`.*", "",
           "## Közös szabályok", "",
           "* **Nyersanyagok:** fa, arany és meteorit (étel nincs). A meteorit ritka – a magasabb tech és a hősök kapuja.",
           "* **Létszám:** minden egység létszámot foglal (a nagy vadak és gépek többet, pl. mamut 6), a korlát "
           f"legfeljebb {d.game['max_supply']}. Hogy mi növeli, az fajonként más (lásd a táblázatot).",
           "* **Hősök:** minden faj saját hősidéző épületéből **3 féle hőst** hívhat, egyszerre legfeljebb hármat. "
           "A hős 10 szintig fejlődik, szintenként képességpontot kap; 4 képessége van (3 háromszintű + egy végső "
           "a 6. szinttől), és 6 tárgyat visel. Az elesett hős az oltárban újraéleszthető.", "",
           "* **Felemelkedés és fejlődési irányok:** amint a főépületen kívül 3 kész épületed van, a nép "
           "felemelkedik – ingyen, magától. Egy ablakban **végleg** választasz a faj 3 iránya közül: mindegyik saját épületet, egységeket és egy szabályt nyit meg, a másik "
           "kettő zárva marad. A hadjáratban nincs irányzár.", "",
           "| Faj | Egyedi mechanika | Létszám nő | Hősidéző |", "|---|---|---|---|"]
    for f in d.faction_list():
        alt = [b for b, v in f["buildings"].items() if v.get("altar")]
        out.append(f"| {f['name']} | {f.get('tagline', '')} | {f.get('supply_desc', '')} | "
                   f"{f['buildings'][alt[0]]['name'] if alt else '–'} |")
    for f in d.faction_list():
        res = {"energy": "energia (mana)", "cooldown": "nincs ára – csak újratöltés", "hp": "életerő (vérmágia)",
               "song": "énekek (kapcsolható aurák, egyszerre egy) és újratöltés", "spores": "spóratöltet (ölésből és a "
               "hálózat közelében)"}.get(f.get("spell_resource"), "energia")
        out += ["", f"## {f['name']}", "", MECH.get(f["id"], f.get("desc", "")), "",
                f"*Létszám:* {f.get('supply_desc', '')}.  *A varázslatok ára:* {res}.", "", "### Egységek", "",
                "| Egység | Költség | Létszám | Képességek | Leírás |", "|---|---|---|---|---|"]
        for uid, u in f["units"].items():
            if u.get("hero"):
                continue
            made = any(uid in b.get("produces", ()) for b in f["buildings"].values()) or u.get("requires")
            pth = f.get("paths", {}).get(u.get("path"), {}).get("name")
            note = ("" if made else " *(idézett / mutációval)*") + (f" *[{pth}]*" if pth else "")
            abl = ", ".join(ab[a]["name"] + (" (passzív)" if ab[a].get("type") in ("passive", "aura") else "")
                            for a in u.get("abilities", []) if a in ab)
            out.append(f"| {u['name']}{note} | {cost(u.get('cost', {}), rd)} | {u.get('supply', 1):g} | {abl} | "
                       f"{u.get('desc', '')} |")
        if f.get("paths"):
            out += ["", "### Fejlődési irányok (Felemelkedés után, egyet végleg)", "",
                    "| Irány | Stílus | Épület | Egységek | Szabály |", "|---|---|---|---|---|"]
            for pid, pd in f["paths"].items():
                bn = f["buildings"].get(pd["building"], {}).get("name", pd["building"])
                un = ", ".join(f["units"][u]["name"] for u in pd["units"] if u in f["units"])
                out.append(f"| **{pd['name']}** | {pd.get('tagline', '')} | {bn} | {un} | {pd.get('desc', '')} |")
        out += ["", "### Hősök", ""]
        for uid, u in f["units"].items():
            if not u.get("hero"):
                continue
            out.append(f"**{u['name']}** – {u.get('desc', '')}")
            out.append("")
            for a in u.get("hero_abilities", []):
                x = ab[a]
                tag = " *(végső, 6. szinttől)*" if x.get("ultimate") else ""
                out.append(f"* **{x['name']}** ({x.get('hotkey', '')}){tag}: {x.get('desc', '')}")
            out.append("")
        out += ["### Épületek", "", "| Épület | Költség | Szerep |", "|---|---|---|"]
        for bid, b in f["buildings"].items():
            out.append(f"| {b['name']} | {cost(b.get('cost', {}), rd)} | {b.get('desc', '')} |")
    lore = os.path.join(ROOT, "docs", "FACTIONS_LORE.md")
    if os.path.isfile(lore):
        with open(lore, encoding="utf-8") as fh:
            out += ["", "---", "", fh.read()]
    with open(os.path.join(ROOT, "docs", "FACTIONS.md"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")
    print("docs/FACTIONS.md kész")


if __name__ == "__main__":
    main()
