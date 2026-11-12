"""Builds data/lang/en.json (English) from the UI dictionary below and tools/lang_en_data.py.

    python tools/lang_en.py

"ui": Hungarian UI text -> English (exact); "patterns": Hungarian text with {0}, {1} slots -> English
template (the slot values are translated too, e.g. unit names); "data": id -> name/desc/tagline.
The campaign texts are in tools/lang_en_campaign.py.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from tools import lang_en_data as D  # noqa: E402

UI = {
    "Pixelnagyítás (retró)": "Pixel zoom (retro)",
    "Nem mozog": "Immobile", "Passzív képesség": "Passive ability", "Mozgás": "Move", "Állj": "Stop", "Tart": "Hold",
    "Lélekhorgony – javítsd Fabrikátorral (ára: az eredeti 35%-a)": "Soul anchor – repair it with a Fabricator (costs 35% of the original)",
    "         jobb klikk az ikonon → visszavonás.  B / N / U: fülek váltása":
        "         right-click an icon → cancel.  B / N / U: switch tabs",
    "         leltár: bal klikk = használat, jobb klikk = eldobás.  F2/F3/F4: 1./2./3. hős kijelölése":
        "         inventory: left-click = use, right-click = drop.  F2/F3/F4: select hero 1/2/3",
    "+ / - : játéksebesség   P: szünet   F1: súgó   Esc: mégse / menü": "+ / - : game speed   P: pause   F1: help   Esc: cancel / menu",
    "A + klikk: támadó mozgás   S: állj   H: helyben tartás   Shift: parancssor":
        "A + click: attack-move   S: stop   H: hold position   Shift: queue orders",
    "A KÜLDETÉS ELBUKOTT": "MISSION FAILED",
    "A beállítások a profilhoz mentődnek.": "Settings are saved to the profile.",
    "A gazda IP-címe:": "Host IP address:",
    "A gazda a játékot létrehozva lesz az 1. játékos; a többiek a helyi hálózaton megkeresik, vagy IP-címmel csatlakoznak (TCP 47810, UDP 47811).":
        "The host creates the game and becomes player 1; the others find it on the local network or join by IP "
        "address (TCP 47810, UDP 47811).",
    "A gazda bezárta a játékot.": "The host closed the game.",
    "A gyártósor megtelt.": "The production queue is full.",
    "A játék megtelt.": "The game is full.",
    "A képesség még töltődik.": "The ability is still recharging.",
    "A mutációhoz nincs elég létszám.": "Not enough population for the mutation.",
    "Add meg a gazda IP-címét.": "Enter the host's IP address.",
    "Az életerő már teljes.": "Health is already full.",
    "Az építési hely elérhetetlen.": "The building site cannot be reached.",
    "Az épület nincs energiamezőben!": "The building is not in a power field!",
    "Az összeolvadáshoz közel kell lenniük – odamennek.": "They must be close to merge – moving together.",
    "BETÖLTÉS": "LOAD", "BEÁLLÍTÁSOK": "SETTINGS", "Betöltés": "Load", "Beállítások": "Settings",
    "Bal klikk / húzás: kijelölés  (Shift: hozzáadás, dupla klikk: azonos típus)":
        "Left-click / drag: select  (Shift: add, double-click: same type)",
    "Bal klikk: lerakás • Shift: többször": "Left-click: place • Shift: several",
    "Bal: kiképzés (Shift: 5×) • Jobb: visszavonás": "Left: train (Shift: 5×) • Right: cancel",
    "Betöltési hiba:": "Load error:",
    "Biztosan törlöd?": "Delete it for sure?",
    "Csapatjátékhoz legalább két különböző csapat kell.": "A team game needs at least two different teams.",
    "Csatlakozás": "Join",
    "Ctrl+1..9: csoport mentése, 1..9: csoport kijelölése (dupla: odaugrás)":
        "Ctrl+1..9: save group, 1..9: select group (twice: jump there)",
    "CÉLOK": "OBJECTIVES", "Célok": "Objectives", "Címer (kedvenc faj):": "Emblem (favourite people):",
    "Döntés": "Decision", "EGYSÉGEK": "UNITS", "EGYÉNI CSATA": "CUSTOM BATTLE", "Egyéni csata": "Custom battle",
    "Elveszett, amit meg kellett védeni!": "What had to be protected is lost!",
    "Energiamező": "Power field", "Enter / Esc: vissza a menübe": "Enter / Esc: back to the menu", "Erdő": "Forest",
    "Esc / P: folytatás": "Esc / P: continue",
    "Ez a képesség már a legmagasabb szinten van.": "This ability is already at its highest level.",
    "Ez a mentés a játék egy másik verziójával készült.": "This save was made with a different version of the game.",
    "F1: súgó  •  F5/F9: gyorsmentés/-betöltés": "F1: help  •  F5/F9: quick save/load",
    "F1: súgó": "F1: help", "F5/F9: gyorsmentés/-betöltés": "F5/F9: quick save/load",
    "Jobb klikk: gyülekezőpont": "Right-click: rally point",
    "FPS-kijelzés": "Show FPS", "Felbontás": "Resolution", "Felderített térkép": "Explored map",
    "Felszedés (F)": "Pack up (F)", "Folytatás": "Continue", "GYŐZELEM!": "VICTORY!", "Gyorsmentés": "Quicksave",
    "Gyorsmentés kész (F9: betöltés).": "Quicksave done (F9: load).", "Gyógyítás": "Healing",
    "Gyógyítás/mp": "Healing/s", "Gyűjt": "Gathering", "Gyűjtött: ": "Gathered: ", "Gép %d": "Computer %d",
    "Gép – könnyű": "Computer – easy", "Gép – közepes": "Computer – normal", "Gép – nehéz": "Computer – hard",
    "Gép-1": "Computer-1", "Gép-2": "Computer-2", "Gépi ellenfél": "Computer opponent",
    "Gépi ellenfél:": "Computer opponent:", "Görgetés sebessége": "Scroll speed", "Hadvezér": "Warlord",
    "Hiányzik: ": "Missing: ", "Hős – szintet lép, képességeket tanul, tárgyakat visel":
        "Hero – gains levels, learns abilities, carries items",
    "Hősök: bal felső portré (dupla: odaugrás), Tanulás (+N) = képességpont, jobb klikk a tárgyra = felvétel":
        "Heroes: top-left portrait (twice: jump there), Learn (+N) = skill point, right-click an item = pick up",
    "INDÍTÁS": "START", "IRÁNYÍTÁS": "CONTROLS", "Időtartam": "Duration", "Javít": "Repairing",
    "Jobb klikk: eldobás": "Right-click: drop", "Jobb klikk: gyülekezőpont": "Right-click: rally point",
    "Jobb klikk: mozgás / támadás / gyűjtés / segít építeni / javít / gyülekezőpont":
        "Right-click: move / attack / gather / help build / repair / rally point",
    "Játék létrehozása": "Create game", "Játékok keresése a helyi hálózaton": "Search for games on the local network",
    "Játékos": "Player", "Játékos (hálózat)": "Player (network)", "Játéksebesség": "Game speed",
    "Játéktípus": "Game type", "KAMPÁNY": "CAMPAIGN", "Kampány": "Campaign",
    "Kapcsolódva. Jelezd, ha kész vagy (KÉSZ gomb).": "Connected. Signal when you are ready (READY button).",
    "Kevés": "Low", "Kezdőhely": "Start", "Kilépés": "Quit", "Kilépés a játékból": "Quit the game",
    "Kilépés a menübe": "Quit to menu", "Kilépéshez nyomd meg újra az Esc-et.": "Press Esc again to quit.",
    "Kreátor: Mesehordo": "Creator: Mesehordo", "KÉSZ": "READY", "KÉSZÍTŐK": "CREDITS",
    "KÜLDETÉS TELJESÍTVE": "MISSION ACCOMPLISHED", "Képernyőkép:": "Screenshot:",
    "Képernyőszéli görgetés": "Edge scrolling",
    "Képességek: a panel gombjai vagy a jelzett gyorsbillentyű (F, T, V, W, R, M, O ...)":
        "Abilities: the panel buttons or the shown hotkey (F, T, V, W, R, M, O ...)",
    "Készítők": "Credits", "Kétkezes építés": "Two-handed building", "Köd": "Fog", "Könnyű": "Easy",
    "Közepes": "Normal", "Lassú": "Slow", "Legalább két résztvevő kell.": "At least two participants are needed.",
    "Legfeljebb három hősöd lehet egyszerre.": "You can have at most three heroes at once.",
    "Letelepítés (L)": "Pitch camp (L)", "Létrehozás": "Create", "Létszám": "Population", "MENTÉS": "SAVE",
    "Megszakadt a kapcsolat a gazdával.": "The connection to the host was lost.", "Megtelt a leltár.": "Inventory full.",
    "Mentés": "Save", "Mentés neve:": "Save name:",
    "Minden tartalom moddolható – lásd docs/MODDING.md": "All content is moddable – see docs/MODDING.md",
    "Mocsár": "Swamp", "Motor: Python + pygame-ce  •  Sprite-generátor: numpy + Pillow":
        "Engine: Python + pygame-ce  •  Sprite generator: numpy + Pillow",
    "Még nincs gyorsmentés.": "No quicksave yet.", "Még nincs mentés ebben a profilban.": "No saves in this profile yet.",
    "Mégse": "Cancel", "Mélység és lárvák": "The deep and larvae", "NEM KÉSZ": "NOT READY", "Nehéz": "Hard",
    "Nehézség (csata)": "Difficulty (battle)", "Nehézség:": "Difficulty:", "Nem minden játékos kész.": "Not every player is ready.",
    "Nem sikerült csatlakozni: {e}": "Could not connect: {e}", "Nem sikerült megnyitni a portot: {e}": "Could not open the port: {e}",
    "Nincs célpont.": "No target.", "Nincs elég energia, vagy töltődik.": "Not enough energy, or recharging.",
    "Nincs elég energia.": "Not enough energy.", "Nincs feltámasztható tetem a közelben.": "No corpse to raise nearby.",
    "Nincs köd": "No fog", "Nincs megfelelő (működő) épület.": "No suitable (working) building.",
    "Nincs megfelelő épület.": "No suitable building.", "Nincs szabad hely a helyőrségben.": "No free place in the garrison.",
    "Nincs szabad lárva.": "No free larva.", "Nincs szabad munkás.": "No free worker.", "Normál": "Normal",
    "Nyilak / képernyőszél / középső gomb: kamera   Szóköz: bázis / utolsó riasztás":
        "Arrows / screen edge / middle button: camera   Space: base / last alert",
    "Névtelen": "Nameless", "Növelése: ": "Increased by: ", "Profil törlése": "Delete profile", "Pusztaság": "Wasteland",
    "Pálya": "Map", "Pálya szerint": "By map", "Q: kilépés a menübe": "Q: quit to menu", "Résztvevő": "Participant",
    "SZÜNET": "PAUSED", "Sebesség": "Speed", "Sebzés": "Damage", "Sebzés/mp": "Damage/s",
    "Sidebar: épület-ikon → lerakás (Shift: többször), egység-ikon → kiképzés,":
        "Sidebar: building icon → place (Shift: several), unit icon → train,",
    "Spórafonal-hálózat": "Spore-thread network", "Sugár": "Radius", "Szinkronhiba a gépek között!": "Desync between the computers!",
    "Szín": "Colour", "Szörnytáborok": "Monster camps", "Szükséges: ": "Requires: ", "Talált játékok: {n}": "Games found: {n}",
    "Tartja a helyét": "Holding position", "Teljes képernyő": "Fullscreen", "TÖBBJÁTÉKOS – LAN": "MULTIPLAYER – LAN",
    "Táj": "Landscape", "Támad": "Attacking", "Támad (A)": "Attack (A)", "Támadás ér!": "We are under attack!",
    "Támadó menet": "Attack-moving", "Tél": "Winter", "Többjátékos (LAN)": "Multiplayer (LAN)", "Törlés": "Delete",
    "VERESÉG": "DEFEAT", "Varázsol": "Casting",
    "Világ és történet: docs/EGSZAKADAS_vilag_es_tortenet_v01.md": "World and story: docs/EGSZAKADAS_vilag_es_tortenet_v01.md",
    "Várakozás a többi játékosra…": "Waiting for the other players…", "Végső képesség (6. szinttől)": "Ultimate (from level 6)",
    "Véletlen": "Random",
    "Wullof: F = tábor felszedése, L = letelepítés   Vasbirodalom: jobb klikk sérült gépre = javítás":
        "Wullof: F = pack up camp, L = pitch camp   Iron Empire: right-click a damaged machine = repair",
    "ZÁRVA": "LOCKED", "Zárt": "Closed", "a térképen kívül": "outside the map", "csak mélységre építhető": "only on the deep",
    "farönk": "log", "felderítetlen terület": "unexplored area", "görgess a további pályákért": "scroll for more maps",
    "hiányzik: ": "missing: ", "klikk: visszavonás": "click: cancel", "kész": "done", "közönséges": "common",
    "ritka": "rare", "ereklye": "artifact", "nem építhető terep": "unbuildable terrain",
    "nincs energiamező (Napobeliszk kell)": "no power field (a Sun Obelisk is needed)",
    "nincs spórafonal-kapcsolat (építs Spórafonalat)": "no spore-thread link (build a Spore Thread)",
    "termelő épület": "production building", "v0.3 • moddolható": "v0.3 • moddable", "v0.3 • moddolható • ": "v0.3 • moddable • ",
    "{n} játékos • {s}×{s} • {t}": "{n} players • {s}×{s} • {t}",
    "© 2026 Mesehordo – a világ és a történet minden joga fenntartva": "© 2026 Mesehordo – all rights to the world and story reserved",
    "Állj (S)": "Stop (S)", "TOTU: Sky Rift": "TOTU: Sky Rift", "TOTU: Sky Rift – A kölcsönkapott öröklét": "TOTU: Sky Rift – The Borrowed Eternity",
    "A kölcsönkapott öröklét": "The Borrowed Eternity",
    "ÉPÜLETEK": "BUILDINGS", "Épít": "Building", "Építeni megy": "Going to build", "Új profil": "New profile",
    "Újrakezdés": "Restart", "Újratöltés": "Cooldown", "épület": "building", "épületekkel": "with buildings",
    "Be": "On", "CSATA": "BATTLE", "Csapat": "Team", "Csapatok": "Teams", "Darab": "Count", "FEJLESZT.": "UPGRADES",
    "Faj": "People", "Fent kontra lent": "Top vs bottom", "Gyors": "Fast",
    "Ihlet: Warcraft II, StarCraft, Armies of Exigo, Command & Conquer": "Inspiration: Warcraft II, StarCraft, Armies of Exigo, Command & Conquer",
    "Ki": "Off", "Mindenki mindenki ellen": "Free for all", "Mozog": "Moving", "NINCS ENERGIA!": "NO POWER!",
    "Nyersanyag": "Resources", "Nyitott": "Open", "PROFILOK": "PROFILES", "Profil": "Profile", "Profil neve:": "Profile name:",
    "Sok": "High", "Tart (H)": "Hold (H)", "Vissza": "Back", "Vissza (Esc)": "Back (Esc)", "foglalt hely": "occupied",
    "#": "#", "A neved:": "Your name:", "A gazda IP-címe": "Host IP address", "Gép": "Computer", "Nyelv": "Language",
    "Magyar": "Magyar", "English": "English", "Felvétel": "Pick up", "Tanulás": "Learn",
    "Életerő": "Health", "Pajzs": "Shield", "Energia": "Energy", "Tapasztalat": "Experience", "max.": "max.",
    "Hős": "Hero", "Fajok": "Peoples", "Szint": "Level", "Hősök": "Heroes", "Leltár": "Inventory",
    "Gépi ellenfél nehézsége": "Computer difficulty", "a gép a gép ellen": "computer vs computer",
    "Elesett hős": "Fallen hero", "Elkészült": "Completed", "kampány": "campaign",
    "Hadjáratok": "Campaigns", "Küldetések": "Missions", "Eligazítás": "Briefing", "Indítás": "Start",
    "Teljesítve": "Completed", "Zárolva": "Locked", "Csaták": "Battles", "Győzelem": "Victories",
    "Vereség": "Defeats", "Statisztika": "Statistics", "Profilok": "Profiles", "Mentések": "Saves",
    "Gyorsbillentyű": "Hotkey", "Igen": "Yes", "Nem": "No", "Kevés (0,6×)": "Low (0.6×)",
    "Kiképzés": "Train", "Fejlesztés": "Upgrade", "Visszavonás": "Cancel", "Jutalom": "Reward",
}

PATTERNS = {
    "• {0}": "• {0}",
    "Sebesség {0}": "Speed {0}", "Sebzés {0}%": "Damage {0}%", "Páncél {0}": "Armour {0}",
    "Kapott távolsági sebzés {0}": "Ranged damage taken {0}", "Lőtáv {0}": "Range {0}", "Gyógyulás {0}": "Regeneration {0}",
    "Ára: {0} életerő": "Cost: {0} health", "Ára: {0} spóra": "Cost: {0} spores", "Spórák: {0}/6": "Spores: {0}/6",
    "Táplálék: {0}/{1} (osztódásig)": "Meals: {0}/{1} (until division)", "Osztódott: {0}": "Divided: {0}",
    "Újratestesült: {0}": "Re-embodied: {0}",
    "Fa {0}": "Wood {0}", "Kő {0}": "Gold {0}", "Arany {0}": "Gold {0}", "Meteorit {0}": "Meteorite {0}",
    "+{0} létszám": "+{0} population",
    "Csaták: {0}   Győzelmek: {1}   Vereségek: {2}": "Battles: {0}   Victories: {1}   Defeats: {2}",
    "Csaták: {0}  Győzelem: {1}": "Battles: {0}  Victories: {1}",
    "Döntés: {0}": "Decision: {0}", "Egység kész: {0}": "Unit ready: {0}",
    "Ehhez {0}. szintű hős kell.": "This needs a level {0} hero.",
    "Elesett: {0} – a hősidéző épületben újraéleszthető.": "Fallen: {0} – can be revived at the hero altar.",
    "Elkészült: {0}": "Completed: {0}", "Elérted a létszámkorlátot – növeld: {0}": "Population limit reached – increase it: {0}",
    "Energia {0}/{1}": "Energy {0}/{1}", "Gazda – port {0}. Csatlakozott játékosok: {1}.": "Host – port {0}. Players joined: {1}.",
    "Helyőrség {0}/{1}": "Garrison {0}/{1}", "Idő: {0} mp": "Time: {0} s", "Játékidő: {0} ó {1} p": "Play time: {0} h {1} m",
    "Játékidő: {0}:{1}": "Play time: {0}:{1}",
    "Kiképzett egységek: {0}   Megölt ellenség: {1}   Veszteség: {2}": "Units trained: {0}   Enemies killed: {1}   Losses: {2}",
    "Kiürítés ({0})": "Unload ({0})", "Készlet: {0}": "Amount: {0}", "Lárvák: {0}/{1}": "Larvae: {0}/{1}",
    "Létrehozva: {0}": "Created: {0}", "Létszám: {0}": "Population: {0}", "Megölt ellenség: {0}": "Enemies killed: {0}",
    "Mutáció kész: {0}": "Mutation complete: {0}", "Mutáció: {0} – {1} mp": "Mutation: {0} – {1} s",
    "Méret: {0}×{1}": "Size: {0}×{1}", "Nem lehet letelepíteni: {0}": "Cannot pitch camp: {0}",
    "Nem lehet építeni: {0}": "Cannot build: {0}", "Nincs elég nyersanyag: {0}": "Not enough resources: {0}",
    "Pajzs {0}/{1}": "Shield {0}/{1}", "Páncél {0}": "Armour {0}", "Seb. {0}": "Spd. {0}", "Sebzés {0}": "Damage {0}",
    "Szállít: {0} {1}": "Carrying: {0} {1}", "Szükséges hősszint: {0}": "Required hero level: {0}",
    "Tanulás (+{0}) T": "Learn (+{0}) T", "Tapasztalat {0}": "Experience {0}", "Teljesített küldetések: {0}": "Missions completed: {0}",
    "Teljesítve: {0}": "Completed: {0}", "Táv {0}": "Range {0}", "görgetés ▲▼  {0}/{1}": "scroll ▲▼  {0}/{1}",
    "{0} egység": "{0} units", "{0} elérte a(z) {1}. szintet!": "{0} reached level {1}!",
    "{0} kilépett – a gép veszi át.": "{0} left – the computer takes over.", "{0} kilépett.": "{0} left.",
    "{0} csatlakozott.": "{0} joined.", "{0} már a seregedben van.": "{0} is already in your army.",
    "{0} vereséget szenvedett!": "{0} has been defeated!", "{0} – tanulás: {1}. szint": "{0} – learn: level {1}",
    "{0}. csoport mentve": "Group {0} saved", "{0}. szintű hős": "Level {0} hero", "{0}: automatikus képesség.": "{0}: automatic ability.",
    "{0}: legalább {1} egység kell.": "{0}: at least {1} units are needed.", "{0}: {1} csata, {2} győzelem": "{0}: {1} battles, {2} victories",
    "Életerő {0}/{1}": "Health {0}/{1}", "Épül: {0}%": "Building: {0}%", "Új cél: {0}": "New objective: {0}",
    "Újraélesztés – {0}. szint": "Revive – level {0}", "{0} megtanulta: {1} ({2}. szint)": "{0} learned: {1} (level {2})",
    "{0} ({1}. szint)": "{0} (level {1})", "{0} (aura)": "{0} (aura)", "{0} ({1})": "{0} ({1})",
    "{0} {1}/{2}": "{0} {1}/{2}", "Energia: {0}": "Energy: {0}", "Csatlakozott: {0}": "Joined: {0}",
    "Gép {0}": "Computer {0}", "Tapasztalat max.": "Experience max.", "{0}: {1}": "{0}: {1}", "{0} – {1}": "{0} – {1}",
    "Nem lehet építeni: {0}": "Cannot build: {0}", "Növelése: {0}": "Increased by: {0}",
    "Hiányzik: {0}": "Missing: {0}", "Szükséges: {0}": "Requires: {0}", "Gyűjtött: {0}": "Gathered: {0}",
    "{0} – Szint {1}": "{0} – Level {1}",
}


def to_regex(t):
    parts = re.split(r"(\{\d+\})", t)
    rx = "".join("(.+?)" if re.fullmatch(r"\{\d+\}", p) else re.escape(p) for p in parts)
    return "^" + rx + "$"


def main():
    sys.path.insert(0, ROOT)
    from rts import data
    gd = data.load(use_mods=False)
    ui = dict(UI)
    out_data = {}

    def add(did, hu, en, field):
        if hu and en:
            ui.setdefault(hu, en)
        out_data.setdefault(did, {})[field] = en

    for fid, t in D.FACTIONS.items():
        f = gd.factions.get(fid, {})
        add(fid, f.get("name"), t[0], "name")
        add(fid, f.get("desc"), t[1], "desc")
        add(fid, f.get("tagline"), t[2], "tagline")
    for group, src in ((D.DEFS, gd.defs), (D.RESEARCH, gd.researches), (D.ABILITIES, gd.abilities),
                       (D.ITEMS, gd.items)):
        for did, (n, ds) in group.items():
            v = src.get(did, {})
            add(did, v.get("name"), n, "name")
            add(did, v.get("desc"), ds, "desc")
    from tools import lang_en_paths as LP  # fejlődési irányok: nevek, jelmondatok, leírások
    ui.update(LP.UI)
    ui.update(LP.PATHS)
    for fid, f in gd.factions.items():
        for pid, pd in f.get("paths", {}).items():
            if pid in LP.PATH_DESC:
                ui[pd["desc"]] = LP.PATH_DESC[pid]
    PATTERNS.update(LP.PATTERNS)
    for k, n in D.RESOURCES.items():
        add(k, gd.game["resources"].get(k, {}).get("name"), n, "name")
    missing = [k for k in gd.defs if k not in D.DEFS] + [k for k in gd.abilities if k not in D.ABILITIES]
    if missing:
        print("fordítás nélkül:", missing)
    pats = sorted(PATTERNS.items(), key=lambda kv: -len(kv[0]))
    try:  # hadjárat
        from tools import lang_en_campaign as LC
        ui.update(LC.TEXTS)
    except ImportError:
        pass
    out = {"ui": ui, "patterns": [[to_regex(k), v] for k, v in pats], "data": out_data}
    os.makedirs(os.path.join(ROOT, "data", "lang"), exist_ok=True)
    with open(os.path.join(ROOT, "data", "lang", "en.json"), "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=0)
    print(f"en.json: {len(ui)} szöveg, {len(pats)} minta, {len(out_data)} adat")


if __name__ == "__main__":
    main()
