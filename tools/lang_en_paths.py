"""Angol szövegek a fejlődési irányokhoz (tools/factions_paths.py) és a hozzájuk tartozó felülethez."""

DEFS = {
    "wul_csordavezer": ("Herd-leader Bull", "A huge bison bull: tramples buildings and sweeps everything aside in its charge."),
    "wul_groomi_fioka": ("Groomi Chick", "A small but nimble, bitey Groomi."),
    "wul_groomi_anya": ("Groomi Mother", "A giant Groomi hen: hatches chicks and bites hard."),
    "wul_totemfarago": ("Totem Carver", "A carver singing the Ancestors' song: nearby allies gain +2 armour."),
    "wul_alomjaro": ("Dreamwalker", "Walks the dreams of the ancestors: heals and calls spirit wolves."),
    "wul_totemkert": ("Totem Garden", "A garden of carved totems: provides Totem Carvers, Dreamwalkers and "
                                      "Termite-mound Giants."),
    "urs_medvealak": ("Bear Shapeshifter", "The Ursan turns fully into a bear: huge claws, a terrifying roar."),
    "urs_kurtos": ("War Hornblower", "The horn call stirs the warriors: +15% damage, +10% speed nearby."),
    "urs_runaorias": ("Rune Giant", "A stone giant awakened by runes: slow, but it smashes buildings."),
    "urs_verfogadalom": ("Hall of the Blood Oath", "Here the berserkers swear their blood oath."),
    "urs_runaszentely": ("Rune Sanctum", "A rune-stone circle with a forge: rune giants and siege sleds."),
    "cro_robbanoatka": ("Blast Mite", "A cheap, fast mite: rushes among the enemy and explodes."),
    "cro_vadaszkarmos": ("Hunter Clawling", "A nimble Clawling variant: pursues fast and rends."),
    "cro_ostromtaraj": ("Siege Crest", "A living siege engine: spits corrosive chitin spines far, especially at "
                                       "buildings."),
    "cro_asoferegg": ("Burrow Worm", "A worm that travels underground: hides burrowed and strikes from below."),
    "cro_rajkamra": ("Swarm Chamber", "A throbbing hatching chamber: blast mites and hunter clawlings (they hatch "
                                      "in the Hive)."),
    "cro_kitinkamra": ("Chitin Chamber", "An armour-growing chamber: armoured beetles, siege crests, burrow worms."),
    "myc_tetemgomba": ("Corpse Fungus", "A body risen from the fallen, woven through with fungus: slow, but it never "
                                        "gives up."),
    "myc_fagomba_orias": ("Bracket-fungus Giant", "A walking ancient tree fungus: guardian of the grove, binds with "
                                                  "its roots."),
    "myc_alomszovo": ("Dreamweaver", "A fungal mage weaving sleeping spores."),
    "myc_rothadokert": ("Rot Garden", "A fungus garden grown on decaying corpses: rotters and corpse fungi."),
    "myc_oriasliget": ("Giant Grove", "A grove of ancient tree fungi: bracket-fungus giants."),
    "elf_fopap": ("High Priest of the Sun", "The Sun's high priest: constantly heals those fighting around him."),
    "elf_jaguar": ("Jaguar Sniper", "A hunter in jaguar hide: shoots from the forest, deadly in ambush."),
    "elf_csillagjos": ("Star Seer", "Master of the observatories: calls down a meteor shower from afar."),
    "elf_naptemplom": ("Sun Temple", "A gilded stepped temple: high priests of the Sun and sun-scattering priests."),
    "imp_praetorianus": ("Praetorian", "An elite machine warrior in gilded armour: the backbone of the legion."),
    "imp_praetorium": ("Praetorium", "The legion's headquarters: praetorians and Centurio mechs."),
    "imp_lelekkut": ("Soul Well", "A well glowing with soul-light: permanent spirit legionaries and spirit colossi."),
}
ABILITIES = {
    "fiokakeltes": ("Hatch Chicks", "Two Groomi chicks hatch from her eggs for 45 seconds."),
    "harci_kurt": ("War Horn", "Horn call (one at a time): nearby allies +15% damage and +10% speed."),
    "medveuvoltes": ("Bear Roar", "A terrifying roar: damages and stuns nearby enemies."),
    "robbanas": ("Self-destruct", "Explodes: heavy damage nearby – the mite dies."),
    "alomspora": ("Dream Spores", "A sleeping spore cloud: nearby enemies fall asleep for 3 s."),
    "napfeny_aldas": ("Sunlight Blessing", "Automatically heals nearby wounded allies."),
    "meteorhivas": ("Call Meteors", "Meteor shower: 105 damage over 3 s in the area."),
    "lesallas": ("Ambush", "Ambush stance: cannot move, but +50% damage and +1 range."),
}
ASCENSION = ("Ascension", "The people choose a path of development (one of 3, permanently). Requirement: 3 completed "
                          "buildings besides the main building.")

PATHS = {
    "Bölény-csorda": "Bison Herd", "Kevés, drága, megállíthatatlan roham": "A few costly, unstoppable charges",
    "Groomi-fészek": "Groomi Nest", "Gyors portyázás, vadászat a gyengékre": "Fast raids, hunting the weak",
    "Ősök útja": "Way of the Ancestors", "Totemlánc, lassú és biztos előrenyomulás": "Totem chain, slow and sure advance",
    "Berserker-klán": "Berserker Clan", "Korai, nyers közelharci nyomás": "Early, raw melee pressure",
    "Rúnakovácsok": "Runesmiths", "Erőd, technológia, késői erő": "Fortress, technology, late-game power",
    "Tundra hívása": "Call of the Tundra",
    "Nagytestű vadak, kevés, de hatalmas egység": "Great beasts – few, but huge units",
    "Raj": "Swarm", "Olcsó tömeg, korai elárasztás": "Cheap masses, early flooding",
    "Páncél": "Carapace", "Lassú, szívós menetelés": "A slow, tough march",
    "Kórus": "Choir", "Irányítás és átállítás": "Control and conversion",
    "Rothadás": "Rot", "Felőrlő, méreggel nyújtott harc": "Grinding, poison-prolonged battle",
    "Óriásliget": "Giant Grove", "Terjeszkedés és területfoglalás": "Expansion and territory",
    "Álom": "Dream", "Csel, rejtőzés, rajtaütés": "Trickery, stealth, ambush",
    "Nap temploma": "Temple of the Sun", "Papokkal támogatott tömeghadsereg": "A mass army backed by priests",
    "Jaguár-harcosok": "Jaguar Warriors", "Vadászat, lesből támadás": "Hunting, attacking from ambush",
    "Csillagvizsgálók": "Stargazers", "Távolsági ostrom, lassú őrlés": "Long-range siege, slow grinding",
    "Légió": "Legion", "Fegyelmezett tömeg alakzatban": "A disciplined mass in formation",
    "Gépisten": "Machine God", "Kevés, hatalmas gép": "Few, enormous machines",
    "Lélek": "Soul", "Kifárasztó, élőhalott játék": "An attritional, undead game",
}
PATH_DESC = {
    "boleny": "You raise bison and your warriors mount them. Cavalry and beasts are faster and stronger; the Herd-leader "
              "Bull tramples buildings. Built for one decisive mid-game charge.",
    "groomi": "Red monster birds: Groomis heal on kills and run faster, the Groomi Mother hatches chicks. Their spit "
              "(acid, fire or frost) can be chosen once, permanently. Worker hunting, map control.",
    "osok": "You raise a Totem Garden: towers hit harder and see farther, buildings are tougher. Totem Carvers and "
            "Dreamwalkers support the Termite-mound Giants. A defensive, territorial game.",
    "berserker": "Blood oath: melee fighters grow stronger the more wounded they are (up to +60% damage). Berserkers, "
                 "shapeshifters who turn into bears, and war hornblowers.",
    "runa": "Rune-carved walls and towers: +3 armour for every building, +6 tower damage. Rune giants and siege sleds "
            "break the front once everything is in place.",
    "tundra": "Mammoths, sabre-tooth riders and storm ravens. Beasts and cavalry gain +60 health and faster "
              "regeneration. A slow, heavy siege.",
    "raj": "Larvae hatch 30% faster, swarm creatures move faster. Blast mites and hunter clawlings flood the enemy "
           "before they are ready.",
    "pancel": "+2 armour for every living unit. Clawlings can turn into armoured beetles; siege crests and tunnelling "
              "worms crack the defences.",
    "korus": "The Choir's voice: 15% of the enemy units you kill rise again on your side. Brain-eaters and tentacled "
             "horrors break up the enemy army.",
    "rothadas": "Your melee fighters poison, and 25% of fallen living foes rise as corpse-fungus zombies. Rotters lead "
                "the slowly spreading decay.",
    "liget": "Every connected building of the network gives +2 population, buildings gain +250 health. Walking "
             "bracket-fungus giants guard the grove.",
    "alom": "In the forest all your units are invisible while they neither move nor attack. Dream shamans and "
            "Dreamweavers paralyse the enemy with sleeping spores.",
    "nap": "All your living units regenerate constantly (+1 regeneration). High priests heal, sun-scattering priests "
           "burn, and quetzals strike from the sky.",
    "jaguar": "In the forest your infantry is invisible, melee fighters gain +2 damage. Jaguar snipers, raptor riders "
              "and feathered rexes.",
    "csillag": "+1 range for every ranged unit. Obsidian throwers and star seers call meteors from afar.",
    "legio": "Formation: infantry gain +6% damage for every nearby comrade (max. +30%) and +1 armour. Praetorians and "
             "Centurio mechs.",
    "gepisten": "Colossi and onager mechs with +150 health and +4 siege damage. Slow, costly, but unstoppable.",
    "lelek": "20% of the living enemies you kill rise as spirit legionaries on your side (for 40 s). Permanent spirit "
             "legionaries and spirit colossi can be trained at the Soul Well.",
}
UI = {
    "Felemelkedés – válassz irányt": "Ascension – choose a path",
    "A választás végleges: a másik két irány tartalma zárva marad.":
        "The choice is permanent: the other two paths stay locked.",
    "Esc: később (a főépület parancskártyáján újra megnyitható)":
        "Esc: later (can be reopened from the main building's command card)",
    "Irány választása": "Choose path",
    "Felemelkedés! Válaszd ki a néped fejlődési irányát.": "Ascension! Choose your people's path of development.",
    "kizárva": "excluded",
    "Irány (gép)": "Path (AI)", "Automatikus": "Automatic",
    "Hálózati játékban a sebességet a gazda állítja.": "In a network game the host sets the speed.",
    "A harcos a bölényhez indul – ha odaért, ültesd fel.": "The warrior heads to the bison – mount it when it arrives.",
    "Nincs felültethető wullof harcos a bölény közelében.": "No Wullof warrior near the bison to mount.",
}
PATTERNS = {
    "Épület: {0}": "Building: {0}",
    "Irány: {0}": "Path: {0}",
    "Fejlődési irány: {0} – megnyíltak az új épületek és egységek.":
        "Path of development: {0} – new buildings and units are unlocked.",
    "A Felemelkedéshez {0} kész épület kell (most {1}).": "Ascension needs {0} completed buildings (now {1}).",
    "Már ezt választottad: {0} – a többi nem fejleszthető.": "You already chose: {0} – the others cannot be researched.",
    "Minden {0} legfeljebb {1} {2} egységet tarthat el – építs még egyet.":
        "Each {0} can keep at most {1} {2} – build another one.",
    "több {0}": "more {0}",
    "Játéksebesség: x{0}": "Game speed: x{0}",
}
