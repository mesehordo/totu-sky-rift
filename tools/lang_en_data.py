"""English names and descriptions of the game data (id -> (name, description[, tagline]))."""

FACTIONS = {
    "elves": ("Grey Elves", "The Cities of Stone and Star: stepped temple-cities bound together by the Council of the Nine "
              "Steps. Few, long-trained warriors with meteorite shields; buildings are summoned into the fields of the "
              "resonance obelisks.", "Power field • shields • self-raising temples"),
    "ursan": ("Ursans", "The Empire of the Northern Oaths: jarldoms bound by oath, longhalls mixed with yurts, barley "
              "farmsteads. They wear no helmets, at most a loincloth, and fight with great flat plank shields. Halls and "
              "towers take in garrisons (in an alarm the settlers hide inside too); their numbers are fed by farmsteads. "
              "Shield walls and ice-age war beasts.", "Garrisons • farmsteads • repair • shield wall"),
    "croftian": ("Croftians", "The Alien Choir: spores that came with the meteors have possessed the ant-like kethri people. "
                 "Every unit hatches in parallel from the Hive's larvae, Clawlings mutate into armoured beetles inside "
                 "cocoons, and a living enemy that falls on the deep may hatch as a new Clawling.",
                 "The deep • larvae • mutation • infestation"),
    "wullof": ("Wullof", "The Alliance of the Seven Roads: a dingo people of river towns, wandering communities and "
               "road-songs. Their camps – tipis too – can be packed up and pitched elsewhere; they are stronger as a pack, "
               "and every kill brings wood to the tribe.", "Nomad camps • pack strength • hunting spoils"),
    "mycel": ("Mycelians", "The people of the Deep Gardens: individual fungal persons bound together by an underground "
              "web of threads (the name is a working title). Their buildings only work when linked to a Heart Fungus by "
              "spore threads, but resources can be dropped at any of them; three Puffballs merge into a Giant Puffball.",
              "Spore-thread network • heart fungi • merging"),
    "imperium": ("Iron Empire", "The people of Aureth: Ordan, the Hand that Holds, carried those dying of the ether "
                 "sickness of the Skyfall into machine bodies. Real human souls in steel – no new members are born to "
                 "them, so every resource is a matter of life. Their numbers come from soul reactors; Fabricators "
                 "repair the machines, and soul callers raise a legion from the fallen.",
                 "Soul calling • machine repair • siege • soul reactors"),
    "vadon": ("The Wild", "Since the Bright Rain, ether-soaked creatures of the wild guard the crystal deposits and the "
              "treasures.", "Neutral monsters"),
}

DEFS = {
    "cro_csapszolga": ("Tendril Thrall", "Worker. Turns into the building it makes (is consumed)."),
    "cro_karmos": ("Clawling", "Cheap, fast swarm unit. Two hatch from one larva."),
    "cro_tuskekopo": ("Spine Spitter", "Spits bone spines at ground and air targets."),
    "cro_savkopo": ("Acid Spitter", "Living artillery. Long-range acid sacs, splash damage, bonus against buildings."),
    "cro_agyfalo": ("Mind Eater", "Psionic horror. Unleashes the mist of madness on the enemy."),
    "cro_szarnyas_rem": ("Winged Dread", "A flying predator on membrane wings."),
    "cro_csapos_horror": ("Tendril Horror", "Giant of the deep. Its tendrils crush several foes at once."),
    "cro_hos_keth": ("Keth", "The memory-keeper of a possessed kethri city; a command node of the Choir – yet the old "
                     "self has not vanished."),
    "cro_pancelbogar": ("Armoured Beetle", "A Clawling mutated into an armoured assault beast. Only created by mutation."),
    "cro_guboj": ("Cocoon", "A mutating unit – a vulnerable but armoured shell."),
    "cro_szemhordozo": ("Eye Bearer", "A floating eye-sac: sees far, and carries the Choir's numbers (+8)."),
    "cro_hos_vor": ("Vor-Uthul", "The Master of Mutation: hatches larvae from his own body and grows chitin armour."),
    "cro_hos_ssarha": ("Ssarha, Voice of the Swarm", "The voice of the Choir: binds the swarm into one will and stitches "
                       "its wounds."),
    "cro_kaptar": ("Hive", "Living main building. Raises larvae (max. 3); every unit hatches here – in parallel."),
    "cro_tapgomba": ("Feeding Fungus", "A nourishing fungus colony: spreads the deep far and heals the swarm."),
    "cro_emeszto": ("Digestion Chamber", "Drop-off for every resource."),
    "cro_ikrafeszek": ("Spawning Nest", "Enables Clawlings."),
    "cro_tuskeverem": ("Spine Pit", "Enables Spine Spitters and Acid Spitters."),
    "cro_agyszentely": ("Brain Sanctum", "Enables the Mind Eater and the Tendril Horror."),
    "cro_csontcsucs": ("Bone Spire", "Enables Winged Dreads."),
    "cro_tuskes_gyoker": ("Spined Root", "Defensive building against ground and air targets."),
    "kethri_tojaskamra": ("Kethri Egg Chamber", "Uninfected kethri eggs – the next generation. Protect it!"),
    "cro_korus_kut": ("Choir Well", "A throbbing well of sound: from here speak the Choir's heroes – Keth, Vor-Uthul "
                      "and Ssarha."),
    "elf_kofarago": ("Stonecarver", "Worker. Summons the buildings, then goes back to gathering."),
    "elf_tollas_harcos": ("Feathered Warrior", "Elite infantry fighting with an obsidian-edged club."),
    "elf_jade_ijasz": ("Jade Archer", "Ranged unit, shoots ground and air targets."),
    "elf_raptor_lovas": ("Raptor Rider", "Fast rider on a tailless feathered lizard. Bonus against ranged units."),
    "elf_napszoro": ("Sunscatter Priest", "Meteorite mage. Calls a sun storm upon the enemy."),
    "elf_quetzal": ("Quetzal", "Feathered flying dinosaur. Strikes at everything."),
    "elf_tollas_rex": ("Crested Rex", "A giant dinosaur with a feather crown, archers on its back. Area bite."),
    "elf_hos_szelai": ("Szelai", "A resonance architect made a commander by fate. Builds and breaks networks; calls a "
                       "sun storm."),
    "elf_csillagdardas": ("Star Spear", "Throws star-ore tipped spears – long reach, hits air targets too."),
    "elf_obszidian_veto": ("Obsidian Hurler", "The stonecarvers' siege engine: hurls blocks of obsidian, especially "
                           "strong against buildings."),
    "elf_jadeor": ("Jade Guardian", "A jade statue brought to life (summoned)."),
    "elf_hos_itzcoatl": ("Itzcoatl", "The feathered warlord of the Nine Steps. A melee fighter at the head of the army."),
    "elf_hos_xochi": ("Xochi", "Master of resonance: awakens jade guardians and heals."),
    "elf_napszentely": ("Sun Sanctuary", "Stepped pyramid, main building. Drop-off, trains Stonecarvers."),
    "elf_obeliszk": ("Sun Obelisk", "+8 population. Radiates a power field – elf buildings only work inside it."),
    "elf_harcos_csarnok": ("Hall of Warriors", "Trains Feathered Warriors and Jade Archers."),
    "elf_obszidian_muhely": ("Obsidian Workshop", "Weapon and shield upgrades; builds Star Spears and Obsidian Hurlers."),
    "elf_raptor_karam": ("Raptor Pen", "Raises Raptor Riders and Crested Rexes."),
    "elf_csillagvizsgalo": ("Observatory", "Trains Sunscatter Priests and studies the wisdom of the stars."),
    "elf_tollfeszek": ("Feather Nest Pyramid", "Raises Quetzals."),
    "elf_jade_oszlop": ("Jade Pillar", "Defensive tower against ground and air targets."),
    "elf_napkapu": ("Sun Gate", "Hero-summoning temple gate: Szelai, Itzcoatl and Xochi."),
    "imp_fabrikator": ("Fabricator", "Spider-like worker machine. Builds, gathers, and repairs buildings and machines "
                       "(right-click a damaged machine)."),
    "imp_legios": ("Legionary", "A legionary soul locked in a small walker body, with scutum and gladius."),
    "imp_ballistarius": ("Ballistarius", "Small machine with a shoulder-mounted crossbow."),
    "imp_szellemidezo": ("Soul Caller", "Soul-saving machine priest. Rescues soul anchors from the corpses of the "
                         "battlefield and raises ghost legionaries."),
    "imp_centurio": ("Centurion Mech", "Medium walker mech with a rotary cannon."),
    "imp_aquila": ("Aquila Drone", "An eagle-shaped flying machine."),
    "imp_kolosszus": ("Colossus", "Giant siege mech. In siege mode it cannot move, but shoots further and harder."),
    "imp_szellemlegios": ("Ghost Legionary", "The soul of a fallen warrior, bound to the Soul Caller's will."),
    "imp_szellemkolosszus": ("Ghost Colossus", "The soul of a fallen giant."),
    "imp_hos_livia": ("Livia Varr", "Once a physician, now an officer of the legion that rescues soul anchors."),
    "imp_hos_nera": ("Nera", "Livia's daughter: put into a machine body as a child; seventy years of experience, but "
                     "by imperial law still a minor."),
    "imp_onager": ("Onager Mech", "Four-legged throwing engine: long range, against buildings and groups."),
    "imp_hos_cassius": ("Legate Cassius Ferrum", "Commander of the Seventh Legion in a heavy machine body: charge, "
                        "discipline and the wrath of Ordan."),
    "imp_forum": ("Forum", "Colonnaded square with a steaming engine house. Drop-off, builds Fabricators."),
    "imp_reaktor": ("Soul Reactor", "+10 population – gives soul energy to the machines."),
    "imp_legiogyar": ("Legion Works", "Assembles Legionaries and Ballistarii."),
    "imp_ontode": ("Foundry", "Casts Centurion Mechs, Onagers and Colossi."),
    "imp_gepisten_templom": ("Temple of Ordan", "Shrine of the network of Ordan, the Hand that Holds. Ordains Soul "
                             "Callers, researches upgrades."),
    "imp_hangar": ("Hangar", "Builds Aquila Drones."),
    "imp_aquila_torony": ("Aquila Tower", "Eagle watchtower with a crossbow."),
    "imp_panteon": ("Pantheon", "The hall of the rescued heroes: Livia, Nera and Cassius."),
    "myc_sporaszedo": ("Spore Gatherer", "Worker. Can drop its load at any building linked to the network."),
    "myc_pofeteg": ("Puffball", "Cheap, hopping fungus creature. A poison spore cloud bursts out when it dies. Three "
                    "merge into a Giant Puffball."),
    "myc_tuskegomba_lovesz": ("Spinecap Shooter", "Fires poisonous spore spines at ground and air targets."),
    "myc_kalapos_or": ("Capped Guard", "An armoured fungus warrior with a thick cap."),
    "myc_alomsaman": ("Dreamcap Shaman", "Binds an area with roots and paralyses the enemy."),
    "myc_oriaspofeteg": ("Giant Puffball", "A giant merged from three Puffballs. A huge spore blast when it dies."),
    "myc_lebego_pofeteg": ("Floating Puffball", "A gas-filled floating fungus body; scatters spore rain."),
    "myc_hos_mor": ("Mór", "A young memory-gardener carrying fragments of a damaged memory garden inside him. Heals and "
                    "studies infection."),
    "myc_rothaszto": ("Rotter", "A decaying fungus body: rots its foes in melee and releases a poison spore cloud when "
                      "it dies."),
    "myc_hos_gyokeranya": ("The Root Mother", "Body of the ancient mycelium: puts down roots, binds, and shakes the "
                           "earth."),
    "myc_hos_sporin": ("Sporin the Decayer", "Wanderer of decay: its spines rot, its cloak is a spore cloud."),
    "myc_szivgomba": ("Heart Fungus", "Heart of the network: a small main building and defensive tower in one. +6 "
                      "population (and +1 for every linked building of the network), raises Spore Gatherers, shoots "
                      "spores. Any number can be built."),
    "myc_sporafonal": ("Spore Thread", "Network node: links buildings to the Heart Fungus."),
    "myc_termotest": ("Fruiting Body", "Grows Puffballs, Rotters and Capped Guards."),
    "myc_tuskegomba_telep": ("Spinecap Colony", "Grows Spinecap Shooters."),
    "myc_gyokerbarlang": ("Root Cave", "Upgrades deep within the network."),
    "myc_alomkert": ("Dream Garden", "Raises Dreamcap Shamans."),
    "myc_lebegobolcso": ("Floating Cradle", "Raises Floating Puffballs."),
    "myc_emlekfa": ("Memory Tree", "The tree of forgotten names: calls Mór, the Root Mother and Sporin."),
    "urs_telepes": ("Settler", "Worker. Builds (stays until it is done), repairs, gathers."),
    "urs_pajzshordozo": ("Shieldbearer", "Backbone of the Ursan army. Shield wall: absorbs most ranged damage."),
    "urs_szigonyvezo": ("Harpooner", "Ranged, bonus against large targets; hits air targets too."),
    "urs_berserker": ("Bearskin Berserker", "Fast, hard-hitting melee fighter."),
    "urs_runapap": ("Rune Priest", "Healer. Automatically patches up wounded warriors."),
    "urs_kardfogu": ("Sabretooth Rider", "Fast warrior riding a sabretooth tiger. Charge ability."),
    "urs_viharhollo": ("Storm Raven", "Giant raven. Scout and anti-air."),
    "urs_mammut": ("War Mammoth", "Woolly mammoth with stone throwers on its back. Splash damage, bonus against "
                   "buildings."),
    "urs_hos_ragna": ("Ragna Stoneshoulder", "Commander of a border fort. Her shield protects the one beside her first."),
    "urs_dardaor": ("Spear Guard", "Defender with a long spear – excellent against cavalry and large beasts."),
    "urs_ostromszan": ("Siege Sled", "A stone thrower mounted on a sled, against buildings."),
    "urs_kardfogu_tigris": ("Sabretooth Tiger", "A beast answering Bjarka's call (summoned)."),
    "urs_hos_hjalmar": ("Hjalmar Frostseer", "A seer keeping the memory of the ice age: frost, storm and slowing."),
    "urs_hos_bjarka": ("Bjarka, Lord of Beasts", "Hunter and beast tamer: calls sabretooth tigers and mammoths to battle."),
    "urs_nagyterem": ("Great Hall", "Carved longhall, main building. Drop-off, trains Settlers."),
    "urs_hosszuhaz": ("Longhouse", "Fortified longhouse: garrison of 4, the settlers' refuge in an alarm."),
    "urs_fatelep": ("Lumber Yard", "Drop-off for wood, gold and meteorite."),
    "urs_harcosok_haza": ("House of Warriors", "Trains Shieldbearers, Spear Guards, Harpooners and Berserkers."),
    "urs_runakovacs": ("Rune Forge", "Trains Rune Priests and Siege Sleds, upgrades weapons and armour."),
    "urs_vadak_karama": ("Beast Pen", "Raises Sabretooth Riders and War Mammoths."),
    "urs_sasszirt": ("Eagle Crag", "Raises Storm Ravens."),
    "urs_ortorony": ("Watchtower", "Wood-and-stone watchtower with archers."),
    "urs_eskufa": ("Oath Tree", "An ancient ash under which the jarls swear their oaths: Ragna, Hjalmar and Bjarka."),
    "urs_tanya": ("Farmstead", "Barley field, sheds and hay barns: the jarldom lives off it. +10 population."),
    "vad_kristalybogar": ("Crystal Beetle", "A beetle grown huge from star-ore; crystals grow on its shell."),
    "vad_hamufarkas": ("Ash Wolf", "A pack beast living in the ash of the impact."),
    "vad_eterliderc": ("Ether Wisp", "A freely wandering knot of ether – once perhaps a soul."),
    "vad_kotroll": ("Stone Troll", "A mountain giant with crystals grown into its skin."),
    "vad_ercgolem": ("Star-ore Golem", "A living lump of ore that set out from the rim of the Black Mirror. Lord of the "
                     "wild."),
    "wul_gyujtogeto": ("Forager", "Worker. Gathers, builds, repairs."),
    "wul_landzsas": ("Spear Warrior", "Fast melee fighter with a stone-tipped spear. Dangerous in a pack."),
    "wul_ijas_vadasz": ("Bow Hunter", "Ranged hunter, against ground and air targets."),
    "wul_holdsaman": ("Moon Shaman", "Heals, and calls spirit wolves from the moonlight."),
    "wul_szellemfarkas": ("Spirit Wolf", "A spirit dingo called from the moonlight. Lives for half a minute."),
    "wul_bolenylovas": ("Bison Rider", "Heavy cavalry charging on bison-back."),
    "wul_mennydorgesmadar": ("Thunderbird", "Sacred storm bird; its lightning also strikes around the target."),
    "wul_totemorias": ("Totem Giant", "A totem pole come to life – slow, but it grinds buildings to dust."),
    "wul_vandorszeker": ("Travois", "A packed-up camp. Right button: pitch it elsewhere."),
    "wul_hos_tava": ("Tava", "River cartographer and envoy of the Alliance of the Seven Roads. Opens roads and scouts."),
    "wul_tomahawkos": ("Tomahawk Raider", "A fast raider: the terror of workers and archers."),
    "wul_hos_varnu": ("Varnu the Moon Shaman", "Speaks with the spirits of the ancestors: heals, calls mist and a "
                      "spirit pack."),
    "wul_hos_kurrin": ("Warchief Kurrin", "A warchief riding a war bison: tramples, roars, and calls the warriors of "
                       "the Seven Roads."),
    "wul_tanacssator": ("Council Tent", "The tribe's great tent. Drop-off, raises Foragers. Can be packed up."),
    "wul_tipi": ("Tipi", "Hide tent: +8 population. Can be packed up and carried elsewhere on a travois."),
    "wul_harcosok_tuze": ("Warriors' Fire", "Trains Spear Warriors, Tomahawk Raiders and Bow Hunters. Can be packed up."),
    "wul_bolenykaram": ("Bison Corral", "Provides Bison Riders and Totem Giants. Can be packed up."),
    "wul_samansator": ("Shaman Tent", "Trains Moon Shamans, researches upgrades. Can be packed up."),
    "wul_mennydorges_szikla": ("Thunder Rock", "The storm birds' nest on the sacred rock."),
    "wul_szellemtotem": ("Spirit Totem", "Carved totem pole: guards the camp with spirit arrows."),
    "wul_osok_kore": ("Circle of Ancestors", "A circle of carved posts: calls Tava, Varnu and Kurrin. Can be packed up."),
}

RESEARCH = {
    "cro_inas_labak": ("Sinewy Legs", "+30% speed for Clawlings."),
    "cro_savas_tuskek": ("Acid Spines", "+2 damage for ranged units."),
    "cro_kitinpancel": ("Chitin Armour", "+1 armour for every unit."),
    "cro_eletnedv": ("Life Sap", "+1 health regeneration per second."),
    "elf_obszidian_penge": ("Obsidian Blades", "+2 damage for melee units."),
    "elf_jade_hegyek": ("Jade-tipped Arrows", "+2 damage for ranged units."),
    "elf_napegi_pajzs": ("Sunsky Shield", "+30 shield for every elf unit."),
    "elf_csillagfeny": ("Starlight Sight", "+1 range for ranged units, +1 sight for everyone."),
    "imp_legiopengek": ("Legion Blades", "+2 damage for melee units."),
    "imp_gyorstoltes": ("Quick Loading", "+2 damage for ranged units."),
    "imp_edzett_acel": ("Tempered Steel", "+1 armour for every machine."),
    "imp_megerositett_vaz": ("Reinforced Frame", "+30 health for every machine."),
    "myc_meregspora": ("Poison Spore", "+2 damage for ranged units."),
    "myc_kemenykalap": ("Hard Cap", "+1 armour for every unit."),
    "myc_gyors_micelium": ("Swift Mycelium", "+15% speed for every unit."),
    "myc_dus_spora": ("Rich Spore", "+20 health for every fungus unit."),
    "urs_runapengek": ("Rune Blades", "+2 damage for melee units."),
    "urs_vasszigony": ("Iron-tipped Harpoons", "+2 damage for ranged units."),
    "urs_medvebunda": ("Bear-pelt Armour", "+1 armour for every living unit."),
    "urs_telegi_edzes": ("Winter Training", "+25 health for infantry and cavalry."),
    "wul_obszidian_landzsa": ("Obsidian Spearheads", "+2 damage for melee units."),
    "wul_holdfeny_ijak": ("Moonlight Bows", "+2 damage for ranged units."),
    "wul_bolenybor": ("Bison-hide Armour", "+1 armour for every living unit."),
    "wul_falkaszellem": ("Spirit of the Pack", "+10% speed for every unit."),
}

ABILITIES = {
    "pajzsfal": ("Shield Wall", "Locked shields: -60% ranged damage, +2 armour, but half speed."),
    "napvihar": ("Sun Storm", "A rain of glowing meteor dust: 88 damage over 4 s to every enemy in the area."),
    "oruletkod": ("Mist of Madness", "A mind-rending spore cloud: 50% slow and continuous damage for 6 s."),
    "gyogyitas": ("Rune Healing", "Automatically heals nearby wounded living allies."),
    "vagta": ("Gallop", "The lizard rider is 80% faster for 4 s."),
    "roham": ("Charge", "+70% speed and +30% damage for 4 s."),
    "holdgyogyitas": ("Moonlight Healing", "Automatically heals nearby wounded living allies."),
    "szellemfarkasok": ("Spirit Wolves", "Summons two spirit dingoes for 30 seconds."),
    "gyokerkotes": ("Root Binding", "Roots wrap around the enemy: for 4 s it can barely move."),
    "osszeolvadas": ("Merge", "Three nearby Puffballs merge into a single Giant Puffball."),
    "lelekidezes": ("Soul Calling", "Raises a ghost legionary (a ghost colossus from a large corpse) from a nearby corpse."),
    "ostrommod": ("Siege Mode", "The Colossus settles: it cannot move, but +3 range and +25% damage."),
    "h_tava_nyil": ("Tracking Arrow", "Tava's aimed shot: high damage and a slow on one enemy."),
    "h_tava_utenek": ("Road Song", "Nearby allies move faster (and defend better)."),
    "h_tava_farkasok": ("Spirit Wolves", "Summons spirit dingoes."),
    "h_tava_madar": ("Call of the Seven Roads", "Ultimate: two Thunderbirds arrive for 45 seconds."),
    "h_ragna_pajzsfal": ("Shield Wall", "The shield protects the one beside her first: less ranged damage, more armour."),
    "h_ragna_esku": ("Weight of the Oath", "Nearby allies gain armour."),
    "h_ragna_roham": ("Shield Charge", "Ragna rushes an enemy: damages it and stuns it briefly."),
    "h_ragna_hosszutel": ("Long Winter", "Ultimate: a freezing storm that slows and damages."),
    "h_szelai_napvihar": ("Sun Storm", "A rain of glowing meteor dust."),
    "h_szelai_rezonancia": ("Resonance", "Nearby allies deal more damage."),
    "h_szelai_levalasztas": ("Severance", "A resonance strike that tears a target out of its network."),
    "h_szelai_csillagorok": ("Guardians of the Nine Steps", "Ultimate: four feathered warrior spirits."),
    "h_mor_gyogyspora": ("Healing Spore", "Automatically heals nearby wounded living allies."),
    "h_mor_gyokerkotes": ("Root Binding", "Roots wrap around the enemy."),
    "h_mor_emlekkert": ("Memory Garden", "Nearby allies regenerate faster."),
    "h_mor_ebredes": ("Awakening of the Memory Garden", "Ultimate: two Giant Puffballs sprout."),
    "h_livia_lelek": ("Soul Rescue", "Raises a ghost legionary from a nearby corpse."),
    "h_livia_horgony": ("Anchor Repair", "Automatically repairs nearby damaged machines."),
    "h_livia_legio": ("Discipline of the Legion", "Nearby allies deal more damage."),
    "h_livia_lelegzet": ("Second Breath", "Ultimate: every ally in the area is healed."),
    "h_keth_kod": ("Mist of Madness", "A mind-rending spore cloud."),
    "h_keth_korushang": ("Choir Voice", "Nearby allies are faster and regenerate."),
    "h_keth_csap": ("Tendril Lash", "An acid strike on one target."),
    "h_keth_sokasag": ("The Multitude", "Ultimate: six Clawlings hatch."),
    "h_nera_toltes": ("Quick Charge", "More damage for a short time."),
    "h_nera_loves": ("Aimed Shot", "A powerful long-range shot."),
    "h_nera_batorsag": ("Courage", "„I am afraid – and I go anyway.” Nearby allies gain armour."),
    "h_nera_horgony": ("Free Anchor", "Ultimate: Nera's freed soul becomes a fearsome force for 10 seconds."),
    "h_itz_darda": ("Obsidian Spear", "A thrown obsidian spear: high damage, slow."),
    "h_itz_csapas": ("Earthshaker Strike", "The warlord strikes the ground: damages and stuns those around him."),
    "h_itz_jelenlet": ("Presence of the Warlord", "Nearby allies gain armour and damage."),
    "h_itz_kukulkan": ("Form of Quetzlaru", "Ultimate: the power of the feathered serpent god – double damage, +6 armour."),
    "h_xochi_hullam": ("Resonance Wave", "A jade-coloured vibration tears the area apart."),
    "h_xochi_harmonia": ("Harmony", "Nearby allies recover health and shields faster."),
    "h_xochi_jadeor": ("Jade Guardian", "Awakens a living jade statue that fights for its caller."),
    "h_xochi_napkorona": ("Sun Crown", "Ultimate: every ally in the area is healed."),
    "h_hja_szilank": ("Ice Shard", "A freezing shard: damages and slows heavily."),
    "h_hja_vihar": ("Blizzard", "An icy storm strikes the area."),
    "h_hja_bunda": ("Winter Pelts", "Nearby allies gain armour and regeneration."),
    "h_hja_fagyott_ido": ("Frozen Time", "Ultimate: every nearby enemy freezes into ice."),
    "h_bja_medve": ("Call of the Sabretooth", "Calls a sabretooth tiger from the mountains."),
    "h_bja_falka": ("Strength of the Pack", "Nearby allies (and beasts) deal more damage."),
    "h_bja_szigony": ("Hunting Harpoon", "Hurls a harpoon into an enemy: damages it and stuns it briefly."),
    "h_bja_mamutcsorda": ("Mammoth Herd", "Ultimate: two war mammoths join for 40 seconds."),
    "h_vor_sugar": ("Acid Beam", "A corrosive beam of acid."),
    "h_vor_pancel": ("Mutant Armour", "Chitin plates grow on the master's body."),
    "h_vor_larvak": ("Larva Wave", "Hatches Clawlings from his own body."),
    "h_vor_robbanas": ("Chitin Burst", "Ultimate: chitin shards burst out around the master."),
    "h_ssa_felho": ("Spore Cloud", "A choking spore cloud."),
    "h_ssa_rajtudat": ("Swarm Mind", "The swarm moves with one will: faster and stronger."),
    "h_ssa_oltes": ("Flesh Stitch", "Automatically stitches up nearby wounded living allies."),
    "h_ssa_remszarnyak": ("Dread Wings", "Ultimate: four winged dreads arrive."),
    "h_var_holdfeny": ("Moonlight", "Automatically heals nearby wounded allies."),
    "h_var_kod": ("Mist of the Ancestors", "The spirits of the ancestors wrap around the enemy like mist."),
    "h_var_falka": ("Spirit Pack", "Calls a pack of spirit dingoes."),
    "h_var_telihold": ("Full Moon", "Ultimate: full-moon light heals the area."),
    "h_kur_taposas": ("Bison Stomp", "The war bison stamps the ground: damages and stuns."),
    "h_kur_uvoltes": ("War Howl", "Nearby allies deal more damage."),
    "h_kur_verfalka": ("Blood Pack", "Hunting frenzy: faster and stronger for a short time."),
    "h_kur_het_ut": ("Warriors of the Seven Roads", "Ultimate: five spear warriors arrive from the Seven Roads."),
    "h_gya_halo": ("Root Web", "Roots bind the enemy."),
    "h_gya_melygyoker": ("Deep Root", "Puts down roots: slow, but almost indestructible."),
    "h_gya_talaj": ("Nourishing Soil", "Nearby allies gain regeneration and armour."),
    "h_gya_rengés": ("Root Quake", "Ultimate: roots bursting from below stun everyone."),
    "h_spo_tuske": ("Decaying Spine", "A rotting spore spine."),
    "h_spo_rothadas": ("Rot", "Everything rots in the area."),
    "h_spo_kopeny": ("Spore Cloak", "Wraps itself in a spore cloud: faster, and shots miss."),
    "h_spo_raj": ("Puffball Swarm", "Ultimate: six puffballs sprout – exploding spore sacs."),
    "h_cas_roham": ("Legion Charge", "The legate's machine body surges forward and scatters the enemy."),
    "h_cas_fegyelem": ("Iron Discipline", "Nearby allies gain armour."),
    "h_cas_legio": ("Mech Legion", "Orders legionaries to the front."),
    "h_cas_harag": ("Wrath of Ordan", "Ultimate: Ordan's soul fills the legate's machine."),
    "atvaltozas": ("Mutation: Armoured Beetle", "The Clawling shuts itself in a cocoon and turns into an armoured beetle."),
    "beszallas": ("Garrison", "The selected units enter the nearest garrison building; they are protected there, and "
                  "every unit inside strengthens the building's shots."),
}

ITEMS = {
    "gyogyfozet": ("Healing Draught", "Instantly restores 250 health."),
    "eterfozet": ("Ether Draught", "Instantly restores 100 energy."),
    "emlekkonyv": ("Book of Memories", "Pages of a lost memory garden: +300 experience."),
    "erotom": ("Tome of Strength", "Permanently +2 damage."),
    "eletertom": ("Tome of Vitality", "Permanently +60 health."),
    "csizma": ("Wanderer's Boots", "+15% movement speed."),
    "vedogyuru": ("Ring of Protection", "+2 armour."),
    "karomkesztyu": ("Claw Gauntlet", "+5 damage."),
    "eletamulett": ("Amulet of Vitality", "+150 health."),
    "regengyuru": ("Ring of Renewal", "+2 health regeneration."),
    "eterko": ("Ether Stone", "+60 energy, +1 energy regeneration."),
    "hosipajzs": ("Heroic Shield", "+4 armour, +100 health."),
    "ercpenge": ("Star-ore Blade", "A blade forged from the sky stone: +12 damage."),
    "arnykopeny": ("Shadow Cloak", "+12% speed, +2 armour, +1 regeneration."),
    "orokhajnal": ("Shard of the Eternal Dawn", "A fragment of Ordan's network: +200 health, +100 energy, +6 damage."),
}

RESOURCES = {"wood": "Wood", "stone": "Gold", "meteor": "Meteorite"}

DEFS.update({
    "imp_lelekhorgony": ("Soul Anchor", "The soul of a fallen machine. A Fabricator can rebuild it within 40 s (by "
                         "repairing) – otherwise the soul fades."),
    "urs_runako": ("Rune Stone", "A stone raised by a Rune Priest (summoned). Its aura protects and heals."),
    "urs_colopfal": ("Palisade", "A wall section of sharpened stakes – blocks the way. Cheap and quick to build; settlers "
                     "repair it."),
})
ABILITIES.update({
    "p_halozati_gyujtes": ("Network Gathering", "Can drop its load at any building linked to the network."),
    "p_gepjavitas": ("Machine Repair and Re-embodiment", "Right-click a damaged machine: repair. Also rebuilds soul anchors."),
    "lelekhorgony": ("Soul Anchor", "When the machine is destroyed, its soul stays in an anchor for 40 s – a Fabricator can "
                     "rebuild it."),
    "osztodas2": ("Division (2 kills)", "After every two defeated enemies it divides in two (as if it had eaten them)."),
    "osztodas3": ("Division (3 kills)", "After every three defeated enemies it divides in two."),
    "osztodas4": ("Division (4 kills)", "After every four defeated enemies it divides in two."),
    "erdei_rejtozes": ("Forest Stealth", "Invisible next to trees until it attacks for 3 s (nearby enemies notice it)."),
    "korus_szem": ("Eye of the Choir", "Reveals nearby hidden and burrowed enemies."),
    "jadetest": ("Jade Body", "Spells deal only half damage to it."),
    "szellemtest": ("Spirit Body", "A summoned being; fades away in time."),
    "gubo_p": ("Cocoon", "Mutating; if it is destroyed, the transformation is lost."),
    "taborkocsi": ("Wandering Camp", "A packed-up camp – can be pitched anywhere."),
    "kristalypancel": ("Crystal Armour", "Armour overgrown with hard crystals."),
    "falkavadasz": ("Pack Hunter", "Hunts in a pack."),
    "runako_aura": ("Rune Stone", "Nearby allies gain armour and regeneration."),
    "fenylepes": ("Light Step", "Turns into light and steps to the chosen point (7 tiles)."),
    "fenylepes_munkas": ("Light Step", "An escape teleport (5 tiles)."),
    "jade_celzas": ("Jade Aim", "+2 range and +20% damage for 6 s."),
    "csillagdarda": ("Star Spear", "An aimed, star-ore tipped spear: 45 damage and a slow."),
    "zuhanorepules": ("Dive", "Swoops to the ground: nearby enemies are damaged and stagger."),
    "tollkorona_uvoltes": ("Crest Roar", "A terrifying roar: nearby enemies slow down."),
    "napfokusz": ("Sun Focus", "Deploy: cannot move, but +3 range and +20% damage."),
    "dardafal": ("Spear Wall", "Planted spears: cannot move, but +60% damage and +2 armour."),
    "horgonyszigony": ("Anchor Harpoon", "A harpoon into an enemy: damages and stuns it for 1.2 s."),
    "verszomj": ("Bloodlust", "+50% damage and +20% speed for 7 s, but -2 armour."),
    "runako": ("Rune Stone", "Raises a rune stone: nearby allies gain armour and regeneration."),
    "zuhano_csapas": ("Diving Strike", "The raven swoops on a target."),
    "taposas": ("Trample", "The mammoth stamps the ground: damages and stuns."),
    "telepites": ("Deploy", "The sled anchors: cannot move, but +3 range and +25% damage."),
    "beasas": ("Burrow", "Digs itself underground: invisible (only detectors find it), heals fast, but cannot attack."),
    "tuskezapor": ("Spine Volley", "Bone spines rain on the area. Costs 20 health."),
    "savtocsa": ("Acid Pool", "A corrosive acid pool: damages and slows. Costs 30 health."),
    "ragadozo_zuhanas": ("Predator's Dive", "Swoops on a target. Costs 15 health."),
    "csapsopres": ("Tendril Sweep", "Sweeping tendrils: heavy damage and a slow. Costs 40 health."),
    "kitinroham": ("Chitin Charge", "A 5 s charge: +60% speed, +30% damage. Costs 20 health."),
    "harci_enek": ("War Song", "Song (one at a time): nearby allies +12% damage."),
    "vandor_enek": ("Road Song", "Song (one at a time): nearby allies +15% speed."),
    "osok_haragja": ("Wrath of the Ancestors", "Song: nearby allies +2 armour."),
    "tomahawk": ("Tomahawk Throw", "A thrown tomahawk: damages and slows."),
    "vadaszles": ("Hunter's Blind", "Ambush stance: cannot move, but +2 range and +20% damage."),
    "villamcsapas": ("Lightning Strike", "The bird calls down lightning around itself."),
    "sporatuske": ("Spore Spine", "A poisonous spine: damages and slows. Costs 2 spores."),
    "kalapfal": ("Cap Wall", "The cap stiffens into a shield: +4 armour, -40% ranged damage, but slow."),
    "sporarobbanas": ("Spore Burst", "A poison spore cloud bursts out of it. Costs 3 spores."),
    "sporaeso": ("Spore Rain", "Spore rain on the area. Costs 3 spores."),
    "testudo": ("Testudo", "Tortoise formation: -55% ranged damage, +1 armour, but slow."),
    "gyorstoltes_u": ("Quick Loading", "+40% damage for 6 s."),
    "geppuska_sorozat": ("Cannon Burst", "+70% damage for 5 s, but slower."),
    "zuhanobomba": ("Dive Bomb", "Drops a bomb beneath itself."),
    "ostrom_telepites": ("Siege Deploy", "Anchors: cannot move, but +3 range and +25% damage."),
    "eterloket": ("Ether Blast", "An ether bolt."),
    "foldrenges": ("Earthquake", "Shakes the ground around itself."),
})


# Wullof – ausztrál karakter (tools/factions_aussie.py): a nevek ott vannak, itt csak bekötjük őket
def _aussie():
    import importlib.util
    import os
    spec = importlib.util.spec_from_file_location(
        "factions_aussie", os.path.join(os.path.dirname(os.path.abspath(__file__)), "factions_aussie.py"))
    A = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(A)
    for k, v in A.EN.items():
        tgt = (RESEARCH if (k in RESEARCH or k.startswith("wul_") and k.endswith("kopes")) else
               ABILITIES if (k in ABILITIES or k in A.ABILITIES or k in A.NEW_ABILITIES) else DEFS)
        tgt[k] = v
    f = FACTIONS["wullof"]
    FACTIONS["wullof"] = (f[0], A.FACTION_EN) + tuple(f[2:])


_aussie()


def _paths():
    from tools import lang_en_paths as LP
    DEFS.update(LP.DEFS)
    ABILITIES.update(LP.ABILITIES)


_paths()
