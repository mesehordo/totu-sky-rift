"""A 20 csata- és többjátékos-pálya leírása -> data/maps/*.json.

    python tools/make_maps.py
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "data", "maps")

MAPS = [
    # id, játékos, méret, elrendezés, téma, hu név, en név, hu leírás, en leírás, jellemzők
    ("ket_part", 2, 72, "ring", "forest", "Két part", "Two Shores",
     "Kis, gyors pálya: egy folyó választja el a bázisokat, három gázlóval.",
     "Small, fast map: a river splits the bases, with three fords.",
     {"river": {"fords": 3, "width": 1.8}, "lakes": [1, 2, 3], "ridges": [2, 4, 7], "forests": 10}),
    ("hasadek", 2, 80, "ring", "wasteland", "Hasadék", "The Rift",
     "Középen sziklás hasadék, két szűk hágóval – a hágók ura a pálya ura.",
     "A rocky rift in the middle with two narrow passes – hold the passes, hold the map.",
     {"river": {"terrain": "rock", "fords": 2, "width": 2.2, "inner": 0}, "ridges": [3, 5, 9], "lakes": [0, 0, 0],
      "forests": 9, "dirt": 22}),
    ("kraterfold", 2, 88, "ring", "wasteland", "Kráterföld", "Crater Fields",
     "Nyílt síkság, a közepén gazdag meteorkráterrel.",
     "Open plains with a rich meteor crater in the middle.",
     {"crater": {"radius": 0.12, "meteors": 4}, "lakes": [1, 2, 3], "ridges": [3, 4, 8], "forests": 12}),
    ("osi_erdo", 2, 96, "ring", "forest", "Ősi erdő", "Elder Forest",
     "Sűrű rengeteg, keskeny ösvények – lesállásokhoz és rajtaütésekhez.",
     "A dense old forest with narrow trails – made for ambushes and raids.",
     {"forests": 34, "lakes": [1, 2, 3], "ridges": [2, 4, 6]}),
    ("jeghid", 2, 96, "ring", "winter", "Jéghíd", "Ice Bridge",
     "Befagyott tó a pálya közepén; a partok mentén és a szárazföldi hidakon át lehet támadni.",
     "A frozen lake in the centre; attack along the shores or across the land bridges.",
     {"lakes": [4, 3, 5.5], "ridges": [2, 4, 7], "forests": 14}),
    ("harmas_folyo", 3, 104, "ring", "forest", "Hármas folyó", "Three Rivers",
     "Három folyó három szektorra osztja a pályát; minden ágon két gázló.",
     "Three rivers cut the map into three sectors, each with two fords.",
     {"river": {"fords": 2}, "lakes": [1, 2, 3], "forests": 14}),
    ("haromko", 3, 96, "ring", "wasteland", "Háromkő", "Three Stones",
     "Három sziklatorony a közép körül; a meteor a háromszög közepén.",
     "Three rock towers around the centre; the meteor sits in the middle of the triangle.",
     {"river": {"terrain": "rock", "fords": 1, "width": 2.0, "inner": 10}, "ridges": [3, 6, 10],
      "crater": {"radius": 0.07, "meteors": 3}, "forests": 12}),
    ("negy_sarok", 4, 112, "corners", "forest", "Négy sarok", "Four Corners",
     "Klasszikus négyes: a sarkokban bázisok, középen nyílt csatatér kráterrel.",
     "A classic for four: bases in the corners, an open battlefield and crater in the middle.",
     {"crater": {"radius": 0.09, "meteors": 4}, "lakes": [1, 2, 3.5], "forests": 14}),
    ("keresztut", 4, 104, "corners", "forest", "Keresztút", "Crossroads",
     "Négy tó között kereszt alakú utak – a kereszteződésben a meteor.",
     "Cross-shaped roads between four lakes – the meteor waits at the crossing.",
     {"lakes": [2, 4, 6], "forests": 12, "dirt": 24}),
    ("gyuruhegy", 4, 120, "ring", "winter", "Gyűrűhegy", "Ring Mountain",
     "Sziklagyűrű öleli a pálya kincses közepét, négy kapuval.",
     "A ring of cliffs guards the rich centre, with four gates.",
     {"ring_wall": {"radius": 0.17, "gates": 4}, "crater": {"radius": 0.08, "meteors": 4}, "forests": 14,
      "start_angle": 45}),
    ("tavak_videke", 4, 112, "ring", "swamp", "Tavak vidéke", "Lakelands",
     "Apró tavak és gázlók szabdalt vidéke; sok kis csata, sok kerülőút.",
     "Land broken by many small lakes; many skirmishes, many detours.",
     {"lakes": [6, 2, 4], "forests": 12, "start_angle": 45}),
    ("fent_es_lent", 4, 112, "rows", "forest", "Fent és lent", "North versus South",
     "Csapatpálya (2–2): az északi és a déli szövetség között folyó, gázlókkal.",
     "Team map (2v2): a river with fords between the northern and southern alliances.",
     {"river": {"fords": 3, "width": 2.0}, "forests": 16, "lakes": [1, 2, 3]}),
    ("csillag", 5, 128, "ring", "wasteland", "Csillag", "Star",
     "Öt sziklakar ágazik a középből – öt völgy, öt bázis.",
     "Five rocky arms branch out from the centre – five valleys, five bases.",
     {"river": {"terrain": "rock", "fords": 2, "width": 1.8, "inner": 9}, "ridges": [2, 6, 10],
      "crater": {"radius": 0.07, "meteors": 5}, "forests": 12}),
    ("hatszog", 6, 128, "ring", "forest", "Hatszög", "Hexagon",
     "Hat bázis körben, a szomszédok között patakok – szabad mindenki-mindenki ellen.",
     "Six bases in a circle with streams between neighbours – a true free-for-all.",
     {"river": {"fords": 2, "width": 1.3}, "forests": 12, "crater": {"radius": 0.06, "meteors": 6}}),
    ("mocsarvidek", 6, 128, "rows", "swamp", "Mocsárvidék", "Marshlands",
     "Csapatpálya (3–3): ingoványos középföld, sok kis tó.",
     "Team map (3v3): a boggy middle ground full of small pools.",
     {"lakes": [5, 2, 3.5], "forests": 16}),
    ("het_ut", 7, 136, "ring", "forest", "Hét Út", "Seven Roads",
     "Hét bázis, hét út a középső kráter felé – a wullof dalok szerint itt találkoznak a vándorok.",
     "Seven bases, seven roads leading to the central crater.",
     {"crater": {"radius": 0.08, "meteors": 7}, "ridges": [2, 6, 10], "forests": 12, "dirt": 20}),
    ("nyolc_tuz", 8, 144, "ring", "forest", "Nyolc tűz", "Eight Fires",
     "Nyolc tábortűz egy nagy körben – a legnagyobb mindenki-mindenki ellen pálya.",
     "Eight campfires in a great circle – the biggest free-for-all map.",
     {"crater": {"radius": 0.08, "meteors": 8}, "forests": 10, "lakes": [1, 2, 3]}),
    ("csatamezo", 8, 144, "rows", "wasteland", "Csatamező", "Battlefield",
     "Csapatpálya (4–4): két hosszú vonal, középen nyílt csatatér.",
     "Team map (4v4): two long lines facing each other across an open battlefield.",
     {"forests": 16, "ridges": [4, 4, 8], "lakes": [1, 2, 3]}),
    ("egszakadas_kratere", 8, 152, "ring", "wasteland", "Az Égszakadás krátere", "The Skyfall Crater",
     "Óriáskráter a világ közepén, tele csillagérccel; sziklafal védi, nyolc kapuval.",
     "A giant crater full of star-ore at the heart of the world; guarded by cliffs with eight gates.",
     {"ring_wall": {"radius": 0.15, "gates": 8}, "crater": {"radius": 0.13, "meteors": 8}, "forests": 10}),
    ("labirintus", 4, 120, "ring", "winter", "Labirintus", "Labyrinth",
     "Sziklafalak útvesztője – a felderítés itt életet ment.",
     "A maze of cliff walls – scouting saves lives here.",
     {"river": [{"terrain": "rock", "fords": 3, "width": 1.6, "inner": 0},
                {"terrain": "rock", "fords": 2, "width": 1.4, "inner": 12, "turn": 0.785}],
      "ring_wall": {"radius": 0.24, "gates": 4}, "ridges": [6, 6, 11], "forests": 10, "start_angle": 45}),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    for i, (mid, pl, size, layout, theme, hu, en, dhu, den, feats) in enumerate(MAPS):
        md = {"id": mid, "order": i, "players": pl, "size": size, "layout": layout, "theme": theme,
              "name": {"hu": hu, "en": en}, "desc": {"hu": dhu, "en": den}, "features": feats}
        with open(os.path.join(OUT, mid + ".json"), "w", encoding="utf-8") as f:
            json.dump(md, f, ensure_ascii=False, indent=1)
    print(len(MAPS), "pálya ->", OUT)


if __name__ == "__main__":
    main()
