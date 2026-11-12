"""Az Ooni (IV/b) és a Huk’ucha (IV/c) hadjárat küldetései – a hadjárat-útmutató szerint.

    python tools/kuldetesek_ooni_huk.py

Forrás: docs/tortenet/story/edited/01_vilag_es_fotortenet.md (8. fejezet IV. és V. hadjárat; 11. fejezet:
a küldetéscímek és a „Három kapu”, „A szabad folyóág” helyzet), nepek/06_ooni.md, nepek/07_hukucha.md,
characters/szereplok.md (Sairen, Relluk). Minden pálya „canon” mezője jelöli, mi rögzített, mi adaptáció és mi új.
Sairen és Relluk kampányszereplők: névvel ellátott közönséges egységként jelennek meg (a név nem ad hősi
tulajdonságot); mellettük a nép játékbeli hősei harcolnak.
"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SAIREN = {"id": "oon_oni_harcos", "name": "Sairen, kapuőr", "tag": "sairen"}
RELLUK = {"id": "huk_toidezo", "name": "Relluk, gátmester", "tag": "relluk"}


def say(who, text, portrait, to=None, urgent=False):
    d = {"who": who, "text": text, "portrait": portrait}
    if to:
        d["to"] = to
    if urgent:
        d["urgent"] = True
    return d


def S(text, to=None, urgent=False):
    return say("Sairen", text, "oon_oni_harcos", to, urgent)


def R(text, to=None, urgent=False):
    return say("Relluk", text, "huk_toidezo", to, urgent)


def wave(tid, near, to, units, every=60, delay=None, at=0, until=None, maxn=None, if_done=None, warn=None):
    t = {"id": tid, "every": every, "at": at, "spawn": {"owner": 1, "near": near, "to": to, "units": units}}
    if delay is not None:
        t["delay"] = delay
    if until:
        t["until"] = until
    if maxn:
        t["max"] = maxn
    if if_done:
        t["if_done"] = if_done
    if warn:
        t["say_if_done"] = warn
    return t


def lamp(n, kind="cro_karmos", name="Lámpástalan"):
    return [{"id": kind, "n": n, "name": name, "spread": 0.6}]


def zav(n, kind="cro_karmos"):
    return [{"id": kind, "n": n, "name": "Zavaros víz", "spread": 0.6}]


def base_mission(mid, code, title, faction, briefing, layout, player, objectives, triggers, fail, victory, nxt,
                 enemies=None, allies=None, canon=None, variants=None, marks=None, hero_level=5):
    m = {"id": mid, "code": code, "title": title, "faction": faction,
         "source": "docs/tortenet/story/edited/01_vilag_es_fotortenet.md – 8. és 11. fejezet",
         "canon": canon or {}, "briefing": briefing, "layout": layout, "player": player,
         "enemies": enemies or [{"faction": "croftian", "name": "A Kórus", "base": False, "ai": False, "units": []}],
         "objectives": objectives, "triggers": triggers, "fail_if_lost": fail, "victory": victory,
         "next": nxt, "hero_level": hero_level, "enemy_hero_level": hero_level, "creeps": False}
    if allies:
        m["allies"] = allies
    if variants:
        m["victory_variants"] = variants
    if marks:
        m["marks"] = marks
    return m


# =================================================================== OONI – Akiket átengedünk
OONI_RES = [{"kind": "stone", "at": "arany1"}, {"kind": "stone", "at": "arany2"}, {"kind": "meteor", "at": "meteor1"}]

o1 = base_mission(
    "o1_a_bezart_hago", "OON-01", "1. A bezárt hágó", "ooni",
    ["A hetvenedik év nyarán fertőzött kereskedőket találtak az alsó átkelőnél. Az Ooni kapunaplók Lámpástalanoknak "
     "hívják őket: ismerős hangokat utánoznak.",
     "Sairen lezárja a régi kapukat. Emelj két kapuőrző tornyot a hágó két oldalán, és tartsd meg a kaput éjszakára.",
     "A késve érkezőkről neked kell döntened."],
    {"size": [60, 54], "seed": 4101, "points": {
        "varos": [30, 44], "lampas": [30, 33], "kapu": [30, 24], "kapu_ny": [26, 28], "kapu_k": [34, 28],
        "tulpart": [30, 6], "orveny_ny": [12, 8], "orveny_k": [48, 8], "arany1": [20, 44], "arany2": [41, 41],
        "meteor1": [36, 49], "erdo_ny": [10, 40], "erdo_k": [50, 44], "torii": [30, 20]},
     "paint": [{"t": "rock", "line": [[-1, 24], [26, 24]], "w": 5}, {"t": "rock", "line": [[34, 24], [61, 24]], "w": 5},
               {"t": "rock", "area": [[8, 30], 5]}, {"t": "rock", "area": [[52, 31], 5]}],
     "clear": [{"t": "dirt", "line": ["tulpart", "kapu", "lampas", "varos"], "w": 3},
               {"t": "dirt", "line": ["orveny_ny", [20, 14], "kapu"], "w": 2},
               {"t": "dirt", "line": ["orveny_k", [40, 14], "kapu"], "w": 2}],
     "forest": [{"border": 3}, {"area": ["erdo_ny", 7], "density": 0.8}, {"area": ["erdo_k", 6], "density": 0.8},
                {"area": [[14, 16], 5], "density": 0.5}, {"area": [[46, 16], 5], "density": 0.5}],
     "resources": OONI_RES, "starts": ["varos", "tulpart", [30, 10]],
     "decor": [{"id": "oon_torii", "at": "torii", "ruin": True}],
     "ambient": [{"id": "oon_tanuki", "n": 3, "at": [24, 47], "mode": "idle", "shelter": "varos"}]},
    {"base": True, "resources": {"wood": 500, "stone": 300, "meteor": 80},
     "units": [dict(SAIREN, at="lampas"), {"id": "oon_hos_shiro", "at": "lampas"}, {"id": "oon_oni_harcos", "n": 4, "at": "lampas"},
               {"id": "oon_kohajito", "n": 2, "at": "lampas"}],
     "buildings": [{"id": "oon_papirlampas", "at": "lampas"}]},
    [{"id": "o1_torony_ny", "type": "build_at", "target": "oon_kapuorzo_torony", "at": "kapu_ny", "radius": 4,
      "text": "Építs kapuőrző tornyot a hágó nyugati oldalán", "marker": "Nyugati torony"},
     {"id": "o1_torony_k", "type": "build_at", "target": "oon_kapuorzo_torony", "at": "kapu_k", "radius": 4,
      "text": "Építs kapuőrző tornyot a hágó keleti oldalán", "marker": "Keleti torony"},
     {"id": "o1_ejszaka", "type": "survive", "after": ["o1_torony_ny", "o1_torony_k"], "seconds": 200,
      "text": "Tartsd a lezárt kaput reggelig"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "kapu", "say": [S("A kötél ma este nem kap új csomót. Aki az alsó átkelőn jön, kint marad reggelig.")]},
        {"camera": "tulpart", "say": [say("Shiro, a Kapuőr", "Ismerős hangon szólnak. Ezért nem nyitunk.", "oon_hos_shiro", "Sairen")]},
        {"camera": "lampas", "say": [S("Két torony a hágó két oldalán. Addig senki nem alszik.")]}]}},
     {"id": "kereskedok", "if_done": ["o1_torony_ny", "o1_torony_k"], "at": 0, "say": [
         say("Kapuőr", "Kereskedők a túlparton! Tanukik, szekérrel. Azt mondják, egész nap jöttek.", "oon_nian_or", "Sairen", True)],
      "spawn": {"owner": 2, "near": "tulpart", "to": [30, 21], "tag": "kereskedo",
                "units": [{"id": "oon_tanuki", "n": 4, "spread": 0.6}]}},
     {"id": "dontes", "if_done": ["o1_torony_ny", "o1_torony_k"], "on_enter": {"at": [30, 21], "r": 4, "tag": "kereskedo", "any": True},
      "choice": {"title": "A késve érkezők",
                 "text": "Négy tanuki-kereskedő áll a lezárt kapu előtt. Nem lehet tudni, melyikük ép. Ha átengeded őket, "
                         "a városban kell szemmel tartani őket. Ha nem, a hegyoldalon töltik az éjszakát.",
                 "options": [
                     {"label": "Átengedem őket, és a lámpásnál vesztegzárban tartom",
                      "hint": "Csatlakoznak hozzád. Ha valamelyikük fertőzött, a városon belül kell megállítani.",
                      "flag": "o1_atengedte", "join": "kereskedo",
                      "say": [S("Nyissátok egy arasznyira. Egyenként, lámpással a kezükben.")]},
                     {"label": "Zárva marad a kapu",
                      "hint": "A kereskedők kint maradnak. A kapu biztonságos, de a kapunaplóba bekerül a nevük.",
                      "flag": "o1_lezarta",
                      "say": [S("Reggel kinyitjuk. Addig a kötél a miénk, a hegyoldal az övék."),
                              say("Tanuki kereskedő", "Akkor reggel. Ha lesz még reggel.", "oon_tanuki", "Sairen")]}]}},
     {"id": "fertozott", "if_flag": "o1_atengedte", "if_done": "o1_torony_ny", "every": 1, "delay": 70, "max": 1,
      "message": "Az egyik kereskedő hangja megváltozik a vesztegzárban!", "color": [255, 140, 110],
      "spawn": {"owner": 1, "near": "lampas", "to": "varos", "units": lamp(3)}},
     wave("w_kozep", "tulpart", "kapu", lamp(4), every=45, delay=25, if_done=["o1_torony_ny", "o1_torony_k"], until="o1_ejszaka"),
     wave("w_ny", "orveny_ny", "kapu_ny", lamp(3) + [{"id": "cro_tuskekopo", "n": 1, "name": "Lámpástalan"}], every=70, delay=60,
          if_done=["o1_torony_ny", "o1_torony_k"], until="o1_ejszaka"),
     wave("w_k", "orveny_k", "kapu_k", lamp(3) + [{"id": "cro_tuskekopo", "n": 1, "name": "Lámpástalan"}], every=70, delay=95,
          if_done=["o1_torony_ny", "o1_torony_k"], until="o1_ejszaka"),
     wave("w_korai", "tulpart", "kapu", lamp(2), every=80, delay=50, maxn=2, until="o1_torony_k"),
     {"id": "hajnal", "on_objective": "o1_ejszaka", "cinematic": {"shots": [
         {"camera": "kapu", "say": [S("Hajnal. A kötél kap egy csomót: azoké, akiket ma nem engedtünk át.")]}]}}],
    [{"id": "oon_nian_csarnok", "text": "A Nian csarnok elesett – a kapuváros nem tartható."},
     {"tag": "sairen", "text": "Sairen elesett – a kapuőrség parancsnok nélkül maradt."}],
    "A hágó kitartott reggelig. A kapuvárosokban kijárási tilalmat rendelnek el, Aureth pedig a katonai útvonal "
    "mielőbbi megnyitását kéri.",
    "o2_fogadalmi_kovek",
    canon={"rogzitett": "Fertőzött kereskedők az alsó átkelőnél (Lámpástalanok); Sairen lezárja a régi kapukat; kijárási tilalom.",
           "adaptacio": "A kapuőrző tornyok és az éjszakai kitartás; a késve érkezők döntése (Sairen „kit enged át” döntése).",
           "uj_javaslat": "A vesztegzár következménye és a döntés jelzői: o1_atengedte / o1_lezarta."},
    variants=[{"if_flag": "o1_atengedte", "text": "A vesztegzárban egy kereskedő Lámpástalanná vált – a többiek túlélték, és tartoznak a kapunak."},
              {"if_flag": "o1_lezarta", "text": "A kint rekedt kereskedők sorsa ismeretlen. A nevük a kapunaplóba került."}],
    allies=[{"faction": "ooni", "name": "Kereskedők", "units": []}])

o2 = base_mission(
    "o2_fogadalmi_kovek", "OON-02", "2. Fogadalmi kövek", "ooni",
    ["A kaputartók feljegyzései szerint a hegyben különböző bolygók egykori kőzetei érnek össze. A fogadalmi kövek a "
     "régi határokat jelölik.",
     "Sairen kis csapattal indul a mozgó hegyoldalba: olvasd le mindhárom kő jelét, mielőtt a hegy újra megmozdul.",
     "Karasu, a tengu bölcs a magasból figyel."],
    {"size": [64, 56], "seed": 4202, "points": {
        "indulas": [8, 48], "ko1": [18, 36], "ko2": [40, 40], "ko3": [46, 14], "kapuvaros": [58, 6],
        "omlas_a": [28, 30], "omlas_b": [36, 30], "jarat_a": [30, 44], "jarat_b": [44, 26], "banyasz": [24, 16],
        "feszek": [52, 30]},
     "paint": [{"t": "rock", "area": [[30, 18], 9]}, {"t": "rock", "area": [[12, 22], 6]},
               {"t": "rock", "area": [[56, 40], 6]}, {"t": "rock", "line": [[0, 28], [22, 28]], "w": 3}],
     "clear": [{"t": "dirt", "line": ["indulas", "ko1", [24, 32], "omlas_a", "omlas_b", "ko2"], "w": 2},
               {"t": "dirt", "line": ["ko2", [46, 32], "jarat_b", "ko3", "kapuvaros"], "w": 2},
               {"t": "dirt", "line": ["ko1", [16, 30], [18, 24], "banyasz", [36, 8], "ko3"], "w": 2}],
     "forest": [{"border": 2}, {"area": [[10, 40], 4], "density": 0.6}, {"area": [[50, 50], 5], "density": 0.7}],
     "resources": [], "starts": ["indulas", "feszek", "banyasz"],
     "decor": [{"id": "oon_torii", "at": "kapuvaros"}]},
    {"base": False, "units": [dict(SAIREN, at="indulas"), {"id": "oon_hos_karasu", "at": "indulas"},
                              {"id": "oon_oni_harcos", "n": 4, "at": "indulas"}, {"id": "oon_kohajito", "n": 3, "at": "indulas"}]},
    [{"id": "o2_ko1", "type": "reach", "at": "ko1", "radius": 3, "text": "Olvasd le az első fogadalmi kő jelét", "marker": "Első kő"},
     {"id": "o2_ko2", "type": "reach", "at": "ko2", "radius": 3, "after": "o2_ko1", "text": "Olvasd le a második kő jelét", "marker": "Második kő"},
     {"id": "o2_ko3", "type": "reach", "at": "ko3", "radius": 3, "after": "o2_ko2", "text": "Olvasd le a harmadik kő jelét", "marker": "Harmadik kő"},
     {"id": "o2_banyasz", "type": "escort", "optional": True, "activate_near": {"at": "banyasz", "r": 7}, "tag": "banyasz",
      "min": 1, "at": "kapuvaros", "radius": 5, "text": "Vidd a rekedt tanuki-bányászokat a kapuvárosba", "marker": "Kapuváros"},
     {"id": "o2_haza", "type": "reach", "after": "o2_ko3", "units": ["oon_oni_harcos"], "tag": "sairen", "at": "kapuvaros",
      "radius": 5, "text": "Vidd vissza Sairent a feljegyzésekkel a kapuvárosba", "marker": "Kapuváros"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "ko1", "say": [S("A kaputartók szerint ez a hegy háromféle kőből van. Három kő, három jel.")]},
        {"camera": "kapuvaros", "say": [say("Karasu, a tengu bölcs", "Fentről nézve a hegy olyan, mint egy rosszul varrt köpeny. Siessetek.", "oon_hos_karasu", "Sairen")]}]}},
     {"id": "ko1_jel", "on_objective": "o2_ko1", "say": [S("Vörös kő, fekete erezettel. Ilyen nincs a mi hegyeinkben."),
                                                          say("Karasu, a tengu bölcs", "Az én népem tollai között van ilyen színű por. Régről.", "oon_hos_karasu")]},
     {"id": "omlas", "on_objective": "o2_ko2", "say": [S("Ez itt kék, rétegesen hasad. Mintha egy másik világ alja lenne."),
                                                        say("Karasu, a tengu bölcs", "A hegy mozdul! Az út mögöttetek bezárul!", "oon_hos_karasu", None, True)],
      "reshape": {"paint": [{"t": "rock", "line": ["omlas_a", "omlas_b"], "w": 3}],
                  "clear": [{"t": "dirt", "line": ["ko2", "jarat_a"], "w": 2}]},
      "spawn": {"owner": 1, "near": "jarat_b", "to": "ko2", "units": lamp(4)}},
     {"id": "ko3_jel", "on_objective": "o2_ko3", "say": [S("Fehér, üveges kő. Mind a három külön világ volt egyszer. A varrat itt fut.")]},
     {"id": "banyasz_csatl", "on_enter": {"at": "banyasz", "r": 5}, "join": "banyasz",
      "say": [say("Tanuki-bányász", "Három napja nem találjuk a lejáratot. A hegy átrendezte a tárnát.", "oon_tanuki", "Sairen")]},
     wave("w_feszek", "feszek", "ko2", lamp(3), every=55, delay=40, until="o2_ko3"),
     wave("w_hazaut", "feszek", "jarat_b", lamp(4) + [{"id": "cro_tuskekopo", "n": 1, "name": "Lámpástalan"}], every=50, delay=10,
          if_done="o2_ko3", until="o2_haza"),
     {"id": "zaras", "on_objective": "o2_haza", "cinematic": {"shots": [
         {"camera": "kapuvaros", "say": [S("Három jel, három régi világ. Szelai mérései ugyanide mutatnak.")]}]}}],
    [{"tag": "sairen", "text": "Sairen elesett – a feljegyzések elvesztek."}],
    "A három kő jele bekerült a kapunaplóba: ugyanabban a hegyben három egykori bolygó kőzete ér össze.",
    "o3_harom_kapu",
    canon={"rogzitett": "A kaputartók feljegyzései különböző bolygók kőzeteit írják le ugyanabban a hegyben.",
           "adaptacio": "A három fogadalmi kő leolvasása és az omlás a mozgó hegyoldalban.",
           "uj_javaslat": "A rekedt tanuki-bányászok mellékcélja."},
    allies=[{"faction": "ooni", "name": "Bányászok", "units": [{"id": "oon_tanuki", "n": 3, "at": "banyasz", "tag": "banyasz"}]}])

o3 = base_mission(
    "o3_harom_kapu", "OON-03", "3. Három kapu", "ooni",
    ["A Három kapu egyik szárnyát Ooni őrség, a másikat aurethi műszak felügyeli. Az Örök Hajnal próbaárama menet "
     "közben eltolhatja a szerelőjáratot.",
     "Két csapatot vezetsz: Sairen a nyugati, Cassia Vell aurethi mérnök a keleti kijáratot méri be.",
     "Az egyezség próbája: a veszélyben is elfogadjátok egymás helyszíni döntését."],
    {"size": [64, 56], "seed": 4303, "points": {
        "ny_indul": [8, 46], "k_indul": [56, 46], "kij_ny": [12, 12], "kij_k": [52, 12], "kozep": [32, 28],
        "kapu": [32, 8], "vez_ny": [24, 20], "vez_k": [40, 20], "regi_jarat": [32, 18], "uj_ny": [26, 12], "uj_k": [38, 12],
        "melyseg": [32, 50]},
     "paint": [{"t": "rock", "area": [[32, 38], 7]}, {"t": "rock", "line": [[20, 4], [26, 16], [24, 26]], "w": 3},
               {"t": "rock", "line": [[44, 4], [38, 16], [40, 26]], "w": 3}, {"t": "rock", "line": [[28, 14], [36, 14]], "w": 2}],
     "clear": [{"t": "dirt", "line": ["ny_indul", [10, 30], "kij_ny"], "w": 2}, {"t": "dirt", "line": ["k_indul", [54, 30], "kij_k"], "w": 2},
               {"t": "dirt", "line": ["kij_ny", "vez_ny", "kozep", "vez_k", "kij_k"], "w": 2},
               {"t": "dirt", "line": ["kozep", "regi_jarat", [32, 13], "kapu"], "w": 3},
               {"t": "dirt", "line": ["melyseg", [22, 44], [16, 36], [10, 30]], "w": 2},
               {"t": "dirt", "line": ["melyseg", [42, 44], [48, 36], [54, 30]], "w": 2}],
     "forest": [{"border": 2}, {"area": [[18, 50], 4], "density": 0.6}, {"area": [[46, 50], 4], "density": 0.6}],
     "resources": [], "starts": ["ny_indul", "melyseg"],
     "decor": [{"id": "oon_torii", "at": "kapu"}, {"id": "imp_reaktor", "at": [46, 8], "ruin": True},
               {"id": "oon_papirlampas", "at": [18, 8]}]},
    {"base": False, "units": [dict(SAIREN, at="ny_indul"), {"id": "oon_hos_shiro", "at": "ny_indul", "tag": "ooni"},
                              {"id": "oon_oni_harcos", "n": 3, "at": "ny_indul", "tag": "ooni"},
                              {"id": "imp_fabrikator", "at": "k_indul", "name": "Cassia Vell, aurethi mérnök", "tag": "cassia"},
                              {"id": "imp_legios", "n": 4, "at": "k_indul", "tag": "aureth"}]},
    [{"id": "o3_ny", "type": "reach", "tag": "sairen", "at": "kij_ny", "radius": 3, "text": "Sairen mérje be a nyugati kijáratot", "marker": "Nyugati kijárat"},
     {"id": "o3_k", "type": "reach", "tag": "cassia", "at": "kij_k", "radius": 3, "text": "Cassia Vell mérje be a keleti kijáratot", "marker": "Keleti kijárat"},
     {"id": "o3_vez_ny", "type": "guard", "hidden": True, "at": "vez_ny", "radius": 2, "seconds": 6, "text": "Tarts őrt a nyugati vezérlőnél", "marker": "Nyugati vezérlő"},
     {"id": "o3_vez_k", "type": "guard", "hidden": True, "at": "vez_k", "radius": 2, "seconds": 6, "text": "Tarts őrt a keleti vezérlőnél", "marker": "Keleti vezérlő"},
     {"id": "o3_kapu", "type": "escort", "after": ["o3_vez_ny", "o3_vez_k"], "tag": "cassia", "min": 1, "at": "kapu", "radius": 4,
      "text": "Vezessétek át Cassia Vellt az újranyitott kapun", "marker": "Három kapu"},
     {"id": "o3_sairen_kapu", "type": "reach", "after": ["o3_vez_ny", "o3_vez_k"], "tag": "sairen", "at": "kapu", "radius": 4,
      "text": "Sairen is érjen a kapuhoz"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "kij_ny", "say": [S("Mi a nyugati szárnyat mérjük. Ti a keletit. A kapu csak akkor nyílik, ha mindkettő stimmel.", "Cassia Vell")]},
        {"camera": "kij_k", "say": [say("Cassia Vell", "A vezérlőnk szerint minden a régi helyén van. Megnézzük, igaz-e.", "imp_fabrikator", "Sairen")]}]}},
     {"id": "proba_aram", "if_done": ["o3_ny", "o3_k"], "at": 0, "activate": ["o3_vez_ny", "o3_vez_k"],
      "reshape": {"paint": [{"t": "rock", "line": [[32, 22], [32, 15]], "w": 3}],
                  "clear": [{"t": "dirt", "line": ["vez_ny", "uj_ny", [30, 9], "kapu"], "w": 2},
                            {"t": "dirt", "line": ["vez_k", "uj_k", [34, 9], "kapu"], "w": 2}]},
      "cinematic": {"shots": [
          {"camera": "regi_jarat", "say": [say("Cassia Vell", "Áramlökés a hegyben… A vezérlő még mindig a régi végállást mutatja.", "imp_fabrikator", "Sairen", True)]},
          {"camera": "kozep", "say": [S("A régi járat bezárult. A kövek két új utat nyitottak – kétfelől."),
                                      say("Cassia Vell", "Akkor mindkét vezérlőt kézzel állítjuk. Te a nyugatit, én a keletit.", "imp_fabrikator")]}]}},
     {"id": "egyezseg", "on_objective": "o3_vez_k", "if_done": "o3_vez_ny",
      "say": [S("A te méréseddel nyitunk. Az én kötelem tartja."), say("Cassia Vell", "Elfogadom. Nyitás!", "imp_fabrikator")]},
     wave("w_ny", "melyseg", [10, 30], lamp(3), every=60, delay=35, until="o3_kapu"),
     wave("w_k", "melyseg", [54, 30], lamp(3), every=60, delay=65, until="o3_kapu"),
     wave("w_kozep", "melyseg", "kozep", lamp(4) + [{"id": "cro_tuskekopo", "n": 1, "name": "Lámpástalan"}], every=50, delay=5,
          if_done=["o3_ny", "o3_k"], until="o3_kapu"),
     {"id": "zaras", "on_objective": "o3_sairen_kapu", "if_done": "o3_kapu", "cinematic": {"shots": [
         {"camera": "kapu", "say": [S("A Három kapu újra nyitva. A kötél két színű csomót kap: egyet nekünk, egyet Aurethnek.")]}]}}],
    [{"tag": "sairen", "text": "Sairen elesett – a nyugati mérés elveszett."},
     {"tag": "cassia", "text": "Cassia Vell elesett – a keleti vezérlőt senki sem állítja át."}],
    "Az Ooni és az aurethi őrség két mérés alapján nyitotta újra a Három kaput. Az egyezség kiállta a próbát.",
    "o4_a_vilagvarrat",
    canon={"rogzitett": "A Három kapu: az egyik szárny Ooni, a másik aurethi felügyelet alatt; a próbaáram eltolja a szerelőjáratot, "
                        "az egyik vezérlő a régi végállást jelenti; két mérés alapján nyitják újra.",
           "adaptacio": "A két csapat külön kijáratot mér be, majd kézzel állítják a vezérlőket.",
           "uj_javaslat": "Cassia Vell, aurethi mérnök (névvel ellátott Fabrikátor)."})

o4 = base_mission(
    "o4_a_vilagvarrat", "OON-04", "4. A világvarrat", "ooni",
    ["Szelai mérései igazolják: az aurethi próbaterhelés a világvarraton át jutott el a fertőzött bányákig.",
     "Sairen a közös őrség előtt teríti ki a varrattérképet. Építs három kapuőrző tornyot a varrat mentén, és "
     "pusztítsd el a két fészket, amely a varraton át terjeszkedik.",
     "A tornyok sora lesz a közös szerelőjárat őrláncának első szakasza."],
    {"size": [64, 58], "seed": 4404, "points": {
        "varos": [10, 48], "t1": [22, 36], "t2": [32, 26], "t3": [42, 16], "f1": [52, 40], "f2": [50, 8],
        "arany1": [6, 38], "arany2": [20, 52], "meteor1": [28, 44], "kelet": [58, 30]},
     "paint": [{"t": "rock", "line": [[14, 46], [24, 34], [34, 24], [44, 14], [54, 4]], "w": 1},
               {"t": "rock", "area": [[40, 46], 6]}, {"t": "rock", "area": [[18, 14], 6]}],
     "clear": [{"t": "dirt", "line": ["varos", "t1", "t2", "t3", "f2"], "w": 3},
               {"t": "dirt", "line": ["t1", [38, 38], "f1"], "w": 2}, {"t": "dirt", "line": ["t2", "kelet"], "w": 2}],
     "forest": [{"border": 3}, {"area": [[52, 50], 6], "density": 0.8}, {"area": [[8, 22], 6], "density": 0.8},
                {"area": [[30, 52], 4], "density": 0.6}],
     "resources": [{"kind": "stone", "at": "arany1"}, {"kind": "stone", "at": "arany2"}, {"kind": "meteor", "at": "meteor1"}],
     "starts": ["varos", "kelet"]},
    {"base": True, "resources": {"wood": 600, "stone": 400, "meteor": 120},
     "units": [dict(SAIREN, at="varos"), {"id": "oon_hos_akahana", "at": "varos"}, {"id": "oon_oni_harcos", "n": 4, "at": "varos"},
               {"id": "oon_kohajito", "n": 2, "at": "varos"}],
     "buildings": [{"id": "oon_papirlampas", "at": [18, 40]}]},
    [{"id": "o4_t1", "type": "build_at", "target": "oon_kapuorzo_torony", "at": "t1", "radius": 4, "text": "Kapuőrző torony a varrat első pontján", "marker": "1."},
     {"id": "o4_t2", "type": "build_at", "target": "oon_kapuorzo_torony", "at": "t2", "radius": 4, "after": "o4_t1", "text": "Kapuőrző torony a varrat második pontján", "marker": "2."},
     {"id": "o4_t3", "type": "build_at", "target": "oon_kapuorzo_torony", "at": "t3", "radius": 4, "after": "o4_t2", "text": "Kapuőrző torony a varrat harmadik pontján", "marker": "3."},
     {"id": "o4_feszek", "type": "destroy", "targets": ["feszek"], "text": "Pusztítsd el a varraton terjeszkedő két fészket"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "t2", "say": [S("Itt fut a varrat. Két kőzet, két régi világ, és a kettő között a fertőzés útja.")]},
        {"camera": "f1", "say": [say("Akahana, a Vörös Buzogány", "Ott fészkelnek. Mondd a szót, és szétverem.", "oon_hos_akahana", "Sairen")]},
        {"camera": "varos", "say": [S("Előbb a tornyok. A fészkek nem mennek el.")]}]}},
     {"id": "szelai", "on_objective": "o4_t2", "say": [
         say("Szelai (üzenet)", "A méréseim egyeznek a tiédekkel. A próbaterhelés a varraton át jutott a bányákig.", "elf_hos_szelai", "Sairen"),
         S("Akkor a varrat a közös út. És közös a felelősség is.")]},
     wave("w_f1", "f1", "t1", lamp(3), every=60, delay=70, until="o4_feszek"),
     wave("w_f2", "f2", "t3", lamp(3) + [{"id": "cro_savkopo", "n": 1, "name": "Lámpástalan"}], every=75, delay=120, until="o4_feszek"),
     {"id": "zaras", "on_objective": "o4_feszek", "if_done": "o4_t3", "say": [S("A varrat őrláncot kapott. A szerelőjárat mellé őrséget adunk.")]}],
    [{"id": "oon_nian_csarnok", "text": "A Nian csarnok elesett."}, {"tag": "sairen", "text": "Sairen elesett."}],
    "A varrat mentén három kapuőrző torony áll. A közös szerelőjárat Ooni őrséget kap.",
    "o5_a_nyitott_atjaro",
    canon={"rogzitett": "Szelai mérései igazolják a próbaterhelés útját a varraton át; Sairen kiteríti a varrattérképet; közös szerelőjárat és őrség.",
           "adaptacio": "A három torony és a két fészek.", "uj_javaslat": "–"},
    enemies=[{"faction": "croftian", "name": "A Kórus", "base": False, "ai": False,
              "buildings": [{"id": "cro_kaptar", "at": "f1", "tag": "feszek"}, {"id": "cro_kaptar", "at": "f2", "tag": "feszek"}],
              "units": [{"id": "cro_karmos", "n": 3, "at": "f1", "name": "Lámpástalan"}, {"id": "cro_karmos", "n": 3, "at": "f2", "name": "Lámpástalan"}]}])

o5 = base_mission(
    "o5_a_nyitott_atjaro", "OON-05", "5. A nyitott átjáró", "ooni",
    ["Az egyik kaput külön átjáróvá alakítják a civileknek, hogy a hadsereg későbbi visszavonulása se zárhassa rájuk az utat.",
     "Vezesd át a hátsó város lakóit a civil kapun, miközben a hadsereg kitart, aztán vonulj vissza te is.",
     "A hegy közben omladozik: a katonai út le fog zárulni."],
    {"size": [64, 56], "seed": 4505, "points": {
        "varos": [32, 44], "hatso": [10, 10], "civil_kapu": [20, 30], "kijarat": [8, 50], "katonai_ut": [44, 30],
        "eszak": [40, 4], "kelet": [60, 18], "omlas1": [44, 34], "omlas2": [40, 26], "arany1": [40, 50], "meteor1": [24, 50]},
     "paint": [{"t": "rock", "line": [[0, 30], [18, 30]], "w": 3}, {"t": "rock", "line": [[22, 30], [40, 30]], "w": 3},
               {"t": "rock", "line": [[48, 30], [64, 30]], "w": 3}],
     "clear": [{"t": "dirt", "line": ["hatso", [14, 20], "civil_kapu", [16, 40], "kijarat"], "w": 2},
               {"t": "dirt", "line": ["eszak", "omlas2", "katonai_ut", "omlas1", "varos"], "w": 3},
               {"t": "dirt", "line": ["varos", [20, 46], "kijarat"], "w": 2}, {"t": "dirt", "line": ["kelet", [50, 20], "omlas2"], "w": 2}],
     "forest": [{"border": 2}, {"area": [[52, 46], 6], "density": 0.7}, {"area": [[30, 14], 6], "density": 0.6}],
     "resources": [{"kind": "stone", "at": "arany1"}, {"kind": "meteor", "at": "meteor1"}],
     "starts": ["varos", "eszak", "hatso"], "decor": [{"id": "oon_torii", "at": "civil_kapu"}, {"id": "oon_lakohaz", "at": [6, 6]},
                                                      {"id": "oon_lakohaz", "at": [14, 6], "ruin": True}]},
    {"base": True, "resources": {"wood": 700, "stone": 450, "meteor": 150},
     "units": [dict(SAIREN, at="varos"), {"id": "oon_hos_shiro", "at": "varos"}, {"id": "oon_hos_akahana", "at": "varos"},
               {"id": "oon_oni_harcos", "n": 6, "at": "varos"}, {"id": "oon_kohajito", "n": 3, "at": "varos"},
               {"id": "oon_nian_or", "n": 2, "at": "katonai_ut"}]},
    [{"id": "o5_civil1", "type": "escort", "tag": "civil1", "min": 3, "at": "kijarat", "radius": 5,
      "text": "Vezesd az első civil csoportot a civil kapun át a kijárathoz", "marker": "Kijárat"},
     {"id": "o5_civil2", "type": "escort", "hidden": True, "tag": "civil2", "min": 3, "at": "kijarat", "radius": 5,
      "text": "Vezesd a második civil csoportot is a kijárathoz", "marker": "Kijárat"},
     {"id": "o5_visszavonulas", "type": "reach", "after": ["o5_civil1", "o5_civil2"], "tag": "sairen", "at": "kijarat", "radius": 5,
      "text": "Vonuljon vissza Sairen is a kijárathoz"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "civil_kapu", "say": [S("Ez a kapu a civileké. Ha mi elesünk, ez akkor is nyitva marad.")]},
        {"camera": "hatso", "say": [say("Shiro, a Kapuőr", "A hátsó városban még várnak. Két csoportban hozzuk őket.", "oon_hos_shiro", "Sairen")]}]}},
     {"id": "civil1_csatl", "on_enter": {"at": "hatso", "r": 6}, "join": "civil1",
      "say": [say("Lámpásvivő", "Minden lámpást hozunk. Nem hagyunk sötétet magunk után.", "oon_lampasvivo", "Sairen")]},
     {"id": "civil2_jon", "on_objective": "o5_civil1", "activate": "o5_civil2",
      "say": [say("Shiro, a Kapuőr", "A második csoport a hátsó város tetején vár. Gyorsan, a hegy recseg!", "oon_hos_shiro", None, True)],
      "spawn": {"owner": 2, "near": "hatso", "tag": "civil2", "units": [{"id": "oon_lampasvivo", "n": 3}, {"id": "oon_tanuki", "n": 3}]}},
     {"id": "civil2_csatl", "if_done": "o5_civil1", "on_enter": {"at": "hatso", "r": 6}, "join": "civil2"},
     {"id": "omlas", "at": 150, "message": "Omlás! A katonai út a szorosban lezárult.", "color": [255, 160, 110],
      "reshape": {"paint": [{"t": "rock", "line": ["omlas2", "omlas1"], "w": 4}]},
      "say": [S("A katonai út elveszett. A civil kapu áll. Ezért építettük külön.", None, True)]},
     wave("w_eszak", "eszak", "katonai_ut", lamp(4), every=55, delay=30, until="o5_visszavonulas"),
     wave("w_kelet", "kelet", "varos", lamp(3) + [{"id": "cro_tuskekopo", "n": 1, "name": "Lámpástalan"}], every=65, delay=80, until="o5_visszavonulas"),
     wave("w_civil", "eszak", "civil_kapu", lamp(3), every=70, delay=170, until="o5_visszavonulas"),
     {"id": "zaras", "on_objective": "o5_visszavonulas", "cinematic": {"shots": [
         {"camera": "kijarat", "say": [S("Mindenki átért. A kötélre ma nem csomó kerül, hanem egy új szál: a civil kapué.")]}]}}],
    [{"tag": "sairen", "text": "Sairen elesett."},
     {"tag": "civil1", "unless_done": "o5_civil1", "text": "Az első civil csoport odaveszett."},
     {"tag": "civil2", "unless_done": "o5_civil2", "text": "A második civil csoport odaveszett."}],
    "A civilek a külön kapun át biztonságba jutottak. A hadsereg visszavonult, de a civil átjáró nyitva maradt.",
    "",
    canon={"rogzitett": "Az egyik kaput külön civil átjáróvá alakítják, hogy a visszavonulás se zárhassa rájuk az utat.",
           "adaptacio": "Két civil csoport kimenekítése, a katonai út omlása.", "uj_javaslat": "–"},
    allies=[{"faction": "ooni", "name": "A hátsó város", "units": [{"id": "oon_lampasvivo", "n": 3, "at": "hatso", "tag": "civil1"},
                                                                  {"id": "oon_tanuki", "n": 2, "at": "hatso", "tag": "civil1"}]}])

# =================================================================== HUK’UCHA – Külön tavak, közös folyó
HUK_RES = [{"kind": "stone", "at": "arany1"}, {"kind": "stone", "at": "arany2"}, {"kind": "meteor", "at": "meteor1"}]

h1 = base_mission(
    "h1_a_keseru_ar", "HUK-01", "1. A keserű ár", "hukucha",
    ["A Fekete Tükör felől szennyezett víz érkezik a folyón. Több Huk’ucha telepet fenyeget.",
     "Relluk külön lezárható medencékre osztja az árteret. Áss tavacskákat, és zárd le az első gátat – de a "
     "lezárt víz valahová máshová fordul.",
     "A parton elszaporodik a Zavaros Víz: a gátmesterek így hívják a fertőzött lényeket."],
    {"size": [64, 56], "seed": 5101, "points": {
        "telep": [20, 40], "folyo_be": [32, 0], "gat1": [32, 20], "uveghaz1": [12, 30], "uveghaz2": [16, 24],
        "kikoto": [48, 38], "arany1": [8, 48], "arany2": [30, 50], "meteor1": [40, 50], "zavar1": [56, 6], "zavar2": [6, 6]},
     "paint": [{"t": "water", "line": ["folyo_be", [32, 16], [30, 28], [40, 34], [48, 42], [64, 46]], "w": 3},
               {"t": "rock", "area": [[46, 20], 4]}],
     "clear": [{"t": "dirt", "line": [[28, 20], [36, 20]], "w": 1}, {"t": "dirt", "line": ["telep", "uveghaz1"], "w": 2},
               {"t": "dirt", "line": ["telep", [26, 34], [36, 38], [44, 42]], "w": 2}],
     "forest": [{"border": 3}, {"area": [[10, 12], 5], "density": 0.7}, {"area": [[54, 24], 5], "density": 0.7},
                {"area": [[22, 10], 4], "density": 0.5}],
     "resources": HUK_RES, "starts": ["telep", "zavar1", "kikoto"]},
    {"base": True, "resources": {"wood": 500, "stone": 300, "meteor": 80},
     "units": [dict(RELLUK, at="telep"), {"id": "huk_hos_kvarra", "at": "telep"}, {"id": "huk_kampokardos", "n": 4, "at": "telep"},
               {"id": "huk_csuzlis", "n": 3, "at": "telep"}],
     "buildings": [{"id": "huk_gyongybuborek", "at": "uveghaz1", "tag": "uveghaz"},
                   {"id": "huk_gyongybuborek", "at": "uveghaz2", "tag": "uveghaz"}]},
    [{"id": "h1_tavak", "type": "ponds", "amount": 12, "text": "Áss ki legalább 12 csempényi tavacskát"},
     {"id": "h1_gat", "type": "guard", "after": "h1_tavak", "at": "gat1", "radius": 2, "seconds": 6,
      "text": "Relluk zárja le az első gátat", "marker": "Első gát"},
     {"id": "h1_kitart", "type": "survive", "after": "h1_gat", "seconds": 150, "text": "Védd a telepet, amíg az ár levonul"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "folyo_be", "say": [R("Nézd a színét. Barna, mint a tea, csak keserű. Három tavat már megfertőzött fentebb.")]},
        {"camera": "telep", "say": [say("Kvarra, a Tóanya", "Előbb a saját tavaink. Áss, Relluk, és számolj közben, ahogy szoktál.", "huk_hos_kvarra", "Relluk")]}]}},
     {"id": "gat_dontes", "on_objective": "h1_gat", "choice": {
         "title": "Az első gát",
         "text": "A gát lezárásával a víz nem éri el az üvegházakat – de a kikötő felé fordul, ahol menekülők várnak. "
                 "Ha nyitva hagyod, a kikötő megmarad, az üvegházak elázhatnak.",
         "options": [
             {"label": "Lezárom: az üvegházak maradnak", "flag": "h1_uveghaz",
              "hint": "Az üvegházak megmenekülnek; a kikötőből a menekülőket a következő küldetésben kell kimenteni.",
              "reshape": {"paint": [{"t": "water", "line": [[40, 34], [46, 30], [52, 34], "kikoto"], "w": 3}]},
              "say": [R("Lezárva. A víz a kikötőnek fordul. Ezt holnap ki kell fizetnünk.")]},
             {"label": "Nyitva hagyom: a kikötő marad", "flag": "h1_kikoto",
              "hint": "A menekülők kikötője száraz marad; az üvegházakat elönti a keserű víz.",
              "reshape": {"paint": [{"t": "water", "area": [[14, 27], 4]}]},
              "message": "A keserű víz elönti az üvegházakat.", "color": [255, 160, 110],
              "say": [R("Nyitva marad. Az üvegházak… újraépítjük őket. A menekülőket nem lehet.")]}]}},
     wave("w_eszak", "zavar1", "gat1", zav(3), every=60, delay=60, until="h1_kitart"),
     wave("w_nyugat", "zavar2", "uveghaz2", zav(3), every=75, delay=110, until="h1_kitart"),
     wave("w_ar", "zavar1", "telep", zav(4) + [{"id": "cro_tuskekopo", "n": 1, "name": "Zavaros víz"}], every=40, delay=5,
          if_done="h1_gat", until="h1_kitart"),
     {"id": "zaras", "on_objective": "h1_kitart", "say": [R("Kilenc medence, egy gát, és egy döntés, amit még sokáig fogok számolni.")]}],
    [{"id": "huk_korallcsarnok", "text": "A Korallcsarnok elesett."}, {"tag": "relluk", "text": "Relluk elesett – a gátakat senki sem kezeli."}],
    "A keserű ár levonult. A medencék külön zárhatók – de minden lezárt víz máshová folyik.",
    "h2_buborekok_a_parton",
    canon={"rogzitett": "Relluk kisebb, külön lezárható medencékre osztja az árteret; az első gát lezárása megóvja az üvegházakat, "
                        "de a víz a menekülők kikötője felé fordul; Zavaros Víz.",
           "adaptacio": "A gát döntés-ablakként; a tavacskák ásása.", "uj_javaslat": "A döntés jelzői: h1_uveghaz / h1_kikoto."},
    variants=[{"if_flag": "h1_uveghaz", "text": "Az üvegházak állnak. A kikötőt elöntötte a víz – a menekülők a parton várnak."},
              {"if_flag": "h1_kikoto", "text": "A kikötő száraz maradt. Az üvegházak helyén keserű tó áll."}])

h2 = base_mission(
    "h2_buborekok_a_parton", "HUK-02", "2. Buborékok a parton", "hukucha",
    ["A menekülőknek új medence és stég kell. Relluk a parton új tavat jelöl ki.",
     "Áss új medencét a part mellé, emelj Árvízkaput, és hozd át a menekülőket a régi kikötőből.",
     "A Zavaros Víz a parton tanyázik."],
    {"size": [64, 56], "seed": 5202, "points": {
        "telep": [14, 42], "uj_medence": [34, 40], "kapu": [42, 36], "regi_kikoto": [54, 14], "arany1": [6, 30],
        "arany2": [24, 52], "meteor1": [44, 52], "zavar1": [60, 40], "zavar2": [30, 4]},
     "paint": [{"t": "water", "line": [[64, 24], [52, 26], [44, 30], [34, 28], [20, 30], [-1, 26]], "w": 3},
               {"t": "water", "area": ["regi_kikoto", 4]}],
     "clear": [{"t": "dirt", "line": [[26, 31], [26, 25]], "w": 2}, {"t": "dirt", "line": ["telep", "uj_medence", "kapu"], "w": 2},
               {"t": "dirt", "line": [[26, 24], [40, 18], [50, 18]], "w": 2}],
     "forest": [{"border": 3}, {"area": [[12, 12], 6], "density": 0.75}, {"area": [[56, 50], 5], "density": 0.7}],
     "resources": HUK_RES, "starts": ["telep", "zavar1", [50, 18]],
     "decor": [{"id": "huk_gyongybuborek", "at": [58, 8], "ruin": True}]},
    {"base": True, "resources": {"wood": 600, "stone": 350, "meteor": 100},
     "units": [dict(RELLUK, at="telep"), {"id": "huk_hos_iriss", "at": "telep"}, {"id": "huk_kampokardos", "n": 4, "at": "telep"},
               {"id": "huk_csuzlis", "n": 3, "at": "telep"}],
     "units_if": [{"if_flag": "h1_uveghaz", "units": {"huk_kagylopajzsos": 2}}]},
    [{"id": "h2_medence", "type": "ponds", "at": "uj_medence", "radius": 5, "amount": 10, "text": "Áss új medencét a part mellé (10 csempe)", "marker": "Új medence"},
     {"id": "h2_kapu", "type": "build_at", "target": "huk_arvizkapu", "at": "kapu", "radius": 5, "after": "h2_medence",
      "text": "Emelj Árvízkaput az új medence mellé", "marker": "Árvízkapu"},
     {"id": "h2_menekultek", "type": "escort", "after": "h2_kapu", "tag": "menekult", "min": 4, "at": "uj_medence", "radius": 6,
      "text": "Hozd át a menekülőket a régi kikötőből az új medencéhez", "marker": "Új medence"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "regi_kikoto", "say": [R("A régi kikötő. Ott várnak. Ha jól számolok, tizenegyen. Ha rosszul, többen.")]},
        {"camera": "uj_medence", "say": [say("Iriss Szivárványgyöngy", "Itt a part szilárd. Ide áshatsz.", "huk_hos_iriss", "Relluk")]}]}},
     {"id": "elonto", "if_flag": "h1_uveghaz", "at": 2, "say": [R("A kikötőt mi árasztottuk el, amikor lezártuk a gátat. Most mi hozzuk ki őket.")]},
     {"id": "menekult_csatl", "if_done": "h2_kapu", "on_enter": {"at": "regi_kikoto", "r": 7}, "join": "menekult",
      "say": [say("Wullof menekült", "Azt mondták, a békák tavat ásnak nekünk. Nem hittük el.", "wul_gyujtogeto", "Relluk")]},
     wave("w_part", "zavar1", "kapu", zav(3), every=60, delay=80, until="h2_menekultek"),
     wave("w_eszak", "zavar2", "regi_kikoto", zav(3), every=70, delay=20, if_done="h2_kapu", until="h2_menekultek"),
     {"id": "zaras", "on_objective": "h2_menekultek", "say": [R("Buborékokat fújunk nekik is. Egy tó, két nép. Számoljuk tovább.")]}],
    [{"tag": "relluk", "text": "Relluk elesett."}, {"tag": "menekult", "unless_done": "h2_menekultek", "text": "A menekülők odavesztek."},
     {"id": "huk_korallcsarnok", "text": "A Korallcsarnok elesett."}],
    "Az új medence mellett stégek épültek. A menekülők a buborékházak között kaptak helyet.",
    "h3_az_ures_medence",
    canon={"rogzitett": "Új medence ásása és stégek a menekülőknek, miközben a parton elszaporodik a Zavaros Víz.",
           "adaptacio": "Az Árvízkapu mint stég-épület; a menekülők kísérése.", "uj_javaslat": "A Wullof menekültek és Iriss szerepe."},
    allies=[{"faction": "wullof", "name": "Menekülők", "units": [
        {"id": "wul_gyujtogeto", "n": 6, "at": "regi_kikoto", "tag": "menekult", "spread": 0.6},
        {"id": "wul_landzsas", "n": 2, "at": "regi_kikoto", "tag": "menekult", "spread": 0.6}]}])

h3 = base_mission(
    "h3_az_ures_medence", "HUK-03", "3. Az üres medence", "hukucha",
    ["Fentebb egy régi kőmedence áll szárazon. Ha a zsilipje működik, oda lehetne vezetni a keserű vizet.",
     "Relluk kis csapattal indul: járd be a medencét, olvasd le a régi vízjeleket, és indítsd el a zsilipet.",
     "A medence alján a Zavaros Víz fészkel."],
    {"size": [60, 56], "seed": 5303, "points": {
        "indulas": [30, 52], "perem": [30, 40], "jel1": [16, 28], "jel2": [44, 26], "jel3": [30, 14], "zsilip": [30, 6],
        "feszek": [30, 24], "kijarat": [52, 50]},
     "paint": [{"t": "rock", "area": [[30, 24], 15], "rough": 0.3}],
     "clear": [{"t": "grass", "area": [[30, 24], 12], "rough": 0.3},
               {"t": "dirt", "line": ["indulas", "perem", "jel1", "jel3", "zsilip"], "w": 2},
               {"t": "dirt", "line": ["perem", "jel2", "jel3"], "w": 2}, {"t": "dirt", "line": ["indulas", "kijarat"], "w": 2}],
     "forest": [{"border": 2}, {"area": [[8, 48], 5], "density": 0.7}, {"area": [[52, 10], 5], "density": 0.7}],
     "resources": [], "starts": ["indulas", "feszek"]},
    {"base": False, "units": [dict(RELLUK, at="indulas"), {"id": "huk_hos_iriss", "at": "indulas"},
                              {"id": "huk_kampokardos", "n": 4, "at": "indulas"}, {"id": "huk_csuzlis", "n": 3, "at": "indulas"},
                              {"id": "huk_teknos_mozsar", "n": 1, "at": "indulas"}]},
    [{"id": "h3_jel1", "type": "reach", "at": "jel1", "radius": 3, "text": "Olvasd le a nyugati vízjelet", "marker": "Vízjel"},
     {"id": "h3_jel2", "type": "reach", "at": "jel2", "radius": 3, "text": "Olvasd le a keleti vízjelet", "marker": "Vízjel"},
     {"id": "h3_jel3", "type": "reach", "at": "jel3", "radius": 3, "after": ["h3_jel1", "h3_jel2"], "text": "Olvasd le a medence fenekén a kagylósort", "marker": "Kagylósor"},
     {"id": "h3_zsilip", "type": "guard", "after": "h3_jel3", "at": "zsilip", "radius": 2, "seconds": 8, "text": "Relluk indítsa el a zsilipet", "marker": "Zsilip"},
     {"id": "h3_ki", "type": "reach", "after": "h3_zsilip", "tag": "relluk", "at": "kijarat", "radius": 4, "text": "Vonulj ki a medencéből, mielőtt megtelik", "marker": "Kijárat"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "perem", "say": [R("Kőből van, nem iszapból. Ezt nem mi ástuk. Ez régebbi, mint a tavaink.")]},
        {"camera": "feszek", "say": [say("Iriss Szivárványgyöngy", "És lakik benne valami. Valami, ami nem szereti a vizet.", "huk_hos_iriss", "Relluk")]}]}},
     {"id": "j1", "on_objective": "h3_jel1", "say": [R("Vízjel, tizenkét arasz. Valaha tele volt.")]},
     {"id": "j2", "on_objective": "h3_jel2", "say": [R("Ugyanaz a magasság a túloldalon. Ez egy part volt. Egy régi világ partja.")]},
     {"id": "j3", "on_objective": "h3_jel3", "say": [R("Kagylósor. Ilyen kagyló nincs a mi folyóinkban. A víz ide szivárgott a próbaterheléskor.")]},
     {"id": "zsilip_nyit", "on_objective": "h3_zsilip", "message": "A zsilip megnyílt – a medence telni kezd!", "color": [140, 210, 255],
      "reshape": {"paint": [{"t": "water", "area": [[30, 20], 6]}]},
      "say": [R("Fut! Most kifelé, mielőtt bennünket is beszámol a víz.", None, True)]},
     wave("w_fenek", "feszek", "perem", zav(3), every=50, delay=30, until="h3_zsilip"),
     wave("w_kifele", "feszek", "kijarat", zav(4), every=45, delay=8, if_done="h3_zsilip", until="h3_ki")],
    [{"tag": "relluk", "text": "Relluk elesett."}],
    "A régi kőmedence megtelt: egy egykori világ partvonala ma a keserű vizet fogja fel.",
    "h4_a_tavak_orei",
    canon={"rogzitett": "Az üres kőmedence fenekén régi kagylósor: a megmentett töredék egykori partvonala; a víz a próbaterheléskor ebbe a rétegbe szivárgott.",
           "adaptacio": "A felfedezés a zárókép előtti küldetésbe került, a zsilip megnyitásával.", "uj_javaslat": "A vízjelek leolvasása."},
    enemies=[{"faction": "croftian", "name": "Zavaros víz", "base": False, "ai": False,
              "units": [{"id": "cro_karmos", "n": 4, "at": "feszek", "name": "Zavaros víz"}]}])

h4 = base_mission(
    "h4_a_tavak_orei", "HUK-04", "4. A tavak őrei", "hukucha",
    ["Három lakott tó áll a folyó mentén. A Zavaros Víz most mindhármat egyszerre támadja.",
     "Tartsd meg mindhárom tó buborékházait, amíg a gátmesterek elkészülnek a zsilipekkel.",
     "Minden tónak saját őrsége lesz – ahogy a finálé túlfolyóinak is."],
    {"size": [64, 56], "seed": 5404, "points": {
        "telep": [32, 46], "to1": [12, 30], "to2": [32, 24], "to3": [52, 30], "z1": [6, 4], "z2": [32, 2], "z3": [58, 4],
        "arany1": [22, 50], "arany2": [44, 50], "meteor1": [32, 54]},
     "paint": [{"t": "water", "area": [[12, 24], 3]}, {"t": "water", "area": [[32, 18], 3]}, {"t": "water", "area": [[52, 24], 3]}],
     "clear": [{"t": "dirt", "line": ["telep", "to1", "z1"], "w": 2}, {"t": "dirt", "line": ["telep", "to2", "z2"], "w": 2},
               {"t": "dirt", "line": ["telep", "to3", "z3"], "w": 2}],
     "forest": [{"border": 3}, {"area": [[22, 12], 5], "density": 0.7}, {"area": [[42, 12], 5], "density": 0.7}],
     "resources": HUK_RES, "starts": ["telep", "z2"]},
    {"base": True, "resources": {"wood": 800, "stone": 500, "meteor": 150},
     "units": [dict(RELLUK, at="telep"), {"id": "huk_hos_kvarra", "at": "telep"}, {"id": "huk_hos_bulgok", "at": "telep"},
               {"id": "huk_kampokardos", "n": 5, "at": "telep"}, {"id": "huk_csuzlis", "n": 4, "at": "telep"}],
     "buildings": [{"id": "huk_gyongybuborek", "at": "to1", "tag": "to1"}, {"id": "huk_gyongybuborek", "at": "to2", "tag": "to2"},
                   {"id": "huk_gyongybuborek", "at": "to3", "tag": "to3"},
                   {"id": "huk_korallor", "at": [16, 33]}, {"id": "huk_korallor", "at": [48, 33]}]},
    [{"id": "h4_kitart", "type": "survive", "seconds": 300, "text": "Tartsd meg mindhárom tavat, amíg a zsilipek elkészülnek"},
     {"id": "h4_orseg", "type": "build_at", "optional": True, "target": "huk_korallor", "at": "to2", "radius": 6,
      "text": "Emelj Korallőrt a középső tónál", "marker": "Korallőr"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "to1", "say": [R("Nádas-tó, Kagylós-tó, Gyöngyös-tó. Három tó, háromféle iszap, három őrség.")]},
        {"camera": "z2", "say": [say("Bul’gok Kampókirály", "Jönnek. Hadd jöjjenek. Kampóval fogadjuk őket.", "huk_hos_bulgok", "Relluk")]}]}},
     wave("w1", "z1", "to1", zav(3), every=55, delay=60, until="h4_kitart"),
     wave("w2", "z2", "to2", zav(3), every=55, delay=80, until="h4_kitart"),
     wave("w3", "z3", "to3", zav(3), every=55, delay=100, until="h4_kitart"),
     wave("w_nagy", "z2", "telep", zav(5) + [{"id": "cro_tuskekopo", "n": 2, "name": "Zavaros víz"}], every=1, delay=220, maxn=1, until="h4_kitart"),
     {"id": "zaras", "on_objective": "h4_kitart", "say": [R("Mindhárom tó áll. Mindegyiknek saját zsilipje lesz, és saját őrsége.")]}],
    [{"tag": "to1", "text": "A Nádas-tó buborékházai elestek."}, {"tag": "to2", "text": "A Kagylós-tó buborékházai elestek."},
     {"tag": "to3", "text": "A Gyöngyös-tó buborékházai elestek."}, {"tag": "relluk", "text": "Relluk elesett."}],
    "Mindhárom tó kitartott. A gátmesterek tavanként külön zsilipet és őrséget jelölnek ki.",
    "h5_a_szabad_folyoag",
    canon={"rogzitett": "Külön működtetett vízzárak; a túlfolyók egy-egy helyi őrséghez kerülnek.", "adaptacio": "A három tó egyidejű védelme.",
           "uj_javaslat": "A tavak nevei (Nádas-, Kagylós-, Gyöngyös-tó)."})

h5 = base_mission(
    "h5_a_szabad_folyoag", "HUK-05", "5. A szabad folyóág", "hukucha",
    ["Megérkezik a nagy áradás. Több gátat kell egymás után nyitni, hogy a víz a lakott tavak helyett üres kőmedencékbe fusson.",
     "A sorrend dönt: előbb a felső kőmedence, aztán az alsó. A Nádas-tó zsilipjéhez nem szabad nyúlni.",
     "Ez lesz a finálé hűtő- és elvezetőrendszerének mintája."],
    {"size": [64, 56], "seed": 5505, "points": {
        "telep": [14, 46], "zsilip_a": [24, 20], "zsilip_b": [38, 34], "zsilip_c": [46, 22], "medence_a": [14, 10],
        "medence_c": [54, 10], "nadas": [44, 46], "folyo": [32, 2], "z1": [62, 30], "z2": [4, 20], "arany1": [6, 36],
        "meteor1": [26, 52], "kijarat": [32, 52]},
     "paint": [{"t": "water", "line": ["folyo", [32, 14], [32, 28]], "w": 3}, {"t": "water", "area": ["nadas", 4]},
               {"t": "rock", "area": ["medence_a", 5], "rough": 0.2}, {"t": "rock", "area": ["medence_c", 5], "rough": 0.2}],
     "clear": [{"t": "grass", "area": ["medence_a", 3]}, {"t": "grass", "area": ["medence_c", 3]},
               {"t": "dirt", "line": ["telep", "zsilip_a"], "w": 2}, {"t": "dirt", "line": ["telep", [28, 40], "zsilip_b"], "w": 2},
               {"t": "dirt", "line": ["zsilip_b", "zsilip_c"], "w": 2}, {"t": "dirt", "line": [[30, 30], [34, 30]], "w": 1},
               {"t": "dirt", "line": ["telep", "kijarat"], "w": 2}],
     "forest": [{"border": 3}, {"area": [[20, 32], 4], "density": 0.6}, {"area": [[56, 40], 5], "density": 0.7}],
     "resources": [{"kind": "stone", "at": "arany1"}, {"kind": "meteor", "at": "meteor1"}],
     "starts": ["telep", "z1", "nadas"],
     "decor": [{"id": "huk_gyongybuborek", "at": [48, 50]}, {"id": "huk_gyongybuborek", "at": [40, 52]}]},
    {"base": True, "resources": {"wood": 800, "stone": 500, "meteor": 150},
     "units": [dict(RELLUK, at="telep"), {"id": "huk_hos_kvarra", "at": "telep"}, {"id": "huk_hos_iriss", "at": "telep"},
               {"id": "huk_kampokardos", "n": 5, "at": "telep"}, {"id": "huk_csuzlis", "n": 4, "at": "telep"},
               {"id": "huk_teknos_mozsar", "n": 2, "at": "telep"}]},
    [{"id": "h5_a", "type": "guard", "at": "zsilip_a", "radius": 2, "seconds": 8, "text": "Nyisd meg a felső kőmedence zsilipjét", "marker": "Felső zsilip"},
     {"id": "h5_c", "type": "guard", "after": "h5_a", "at": "zsilip_c", "radius": 2, "seconds": 8, "text": "Nyisd meg az alsó kőmedence zsilipjét", "marker": "Alsó zsilip"},
     {"id": "h5_ar", "type": "survive", "after": "h5_c", "seconds": 120, "text": "Tarts ki, amíg az áradás a kőmedencékbe fut"},
     {"id": "h5_ki", "type": "reach", "after": "h5_ar", "tag": "relluk", "at": "kijarat", "radius": 4,
      "text": "Relluk vezesse a csónakokat a hajózható kijárathoz", "marker": "Kijárat"}],
    [{"id": "nyitas", "at": 0.2, "cinematic": {"shots": [
        {"camera": "folyo", "say": [R("Itt jön. Négyszer annyi víz, mint tavaly. Ha jól számolok. Most az egyszer jól kell számolnom.")]},
        {"camera": "zsilip_b", "say": [R("Ez a Nádas-tó zsilipje. Ha ezt nyitjuk, a víz a tóba fut, ahol laknak. Ehhez senki nem nyúl.")]},
        {"camera": "telep", "say": [say("Kvarra, a Tóanya", "Előbb a felső, aztán az alsó. A víz mindig lefelé tanul.", "huk_hos_kvarra", "Relluk")]}]}},
     {"id": "csapda", "on_enter": {"at": "zsilip_b", "r": 1.5}, "unless_done": "h5_ar",
      "say": [say("Kvarra, a Tóanya", "Ne! Az a Nádas-tó!", "huk_hos_kvarra", "Relluk", True)],
      "fail": "A Nádas-tó zsilipje megnyílt – az áradás a lakott tóba zúdult."},
     {"id": "a_nyit", "on_objective": "h5_a", "message": "A felső kőmedence telik.", "color": [140, 210, 255],
      "reshape": {"paint": [{"t": "water", "area": ["medence_a", 3]}, {"t": "water", "line": [[32, 14], [22, 14], "medence_a"], "w": 2}]},
      "say": [R("Egy. A víz megtalálta a régi partot.")]},
     {"id": "c_nyit", "on_objective": "h5_c", "message": "Az alsó kőmedence telik.", "color": [140, 210, 255],
      "reshape": {"paint": [{"t": "water", "area": ["medence_c", 3]}, {"t": "water", "line": [[32, 16], [44, 14], "medence_c"], "w": 2}]},
      "say": [R("Kettő. A Nádas-tó száraz. Ezt most nem kell holnap kifizetnünk.")]},
     wave("w_kelet", "z1", "zsilip_c", zav(4), every=55, delay=40, until="h5_ar"),
     wave("w_nyugat", "z2", "zsilip_a", zav(3), every=60, delay=70, until="h5_ar"),
     wave("w_ar", "z1", "telep", zav(5) + [{"id": "cro_tuskekopo", "n": 1, "name": "Zavaros víz"}], every=40, delay=10,
          if_done="h5_c", until="h5_ar"),
     {"id": "zaras", "on_objective": "h5_ki", "cinematic": {"shots": [
         {"camera": "medence_a", "say": [R("Három túlfolyó a Fekete Tükörhöz. Mindegyik egy helyi őrségé. Külön is működnek.")]},
         {"camera": "kijarat", "say": [say("Kvarra, a Tóanya", "Külön tavak, közös folyó. Ahogy mindig mondtam.", "huk_hos_kvarra", "Relluk")]}]}}],
    [{"tag": "relluk", "text": "Relluk elesett."}, {"id": "huk_korallcsarnok", "text": "A Korallcsarnok elesett."}],
    "Az áradás az üres kőmedencékbe futott. A Huk’ucha hajózható kijáratot nyitott a Fekete Tükörnél dolgozó csapatoknak.",
    "",
    canon={"rogzitett": "Több gátat kell egymás után nyitni, hogy az áradás a lakott tavak helyett üres kőmedencékbe fusson; "
                        "hajózható kijárat; három túlfolyó a helyi őrségeknek.",
           "adaptacio": "A zsilipek sorrendje és a lakott tó zsilipje mint tiltott választás.", "uj_javaslat": "–"})


def write(cid, order, title, desc, requires, missions):
    d = os.path.join(ROOT, "campaigns", cid)
    os.makedirs(d, exist_ok=True)
    for m in missions:
        with open(os.path.join(d, m["id"] + ".json"), "w", encoding="utf-8") as f:
            json.dump(m, f, ensure_ascii=False, indent=1)
    camp = {"id": cid, "order": order, "title": title, "desc": desc, "requires": requires, "missions": [m["id"] for m in missions]}
    with open(os.path.join(d, "campaign.json"), "w", encoding="utf-8") as f:
        json.dump(camp, f, ensure_ascii=False, indent=1)


if __name__ == "__main__":
    # a pályák egymásra mutatnak: az utolsó küldetés "next" mezője üres
    for seq in ([o1, o2, o3, o4, o5], [h1, h2, h3, h4, h5]):
        for a, b in zip(seq, seq[1:]):
            a["next"] = b["id"]
        seq[-1].pop("next", None)
    write("egsz_4c_ooni", 5.3, "IV/b. Akiket átengedünk",
          "Tava küldöttsége a hegyek alatt keres utat a Fekete Tükör felé. Sairen, az Ooni kapuőr lezárja a régi kapukat – "
          "és el kell döntenie, kiket enged át, amikor a megrepedt hegyek megmozdulnak.", ["egsz_4_gombanep"], [o1, o2, o3, o4, o5])
    write("egsz_4d_hukucha", 5.6, "IV/c. Külön tavak, közös folyó",
          "A Fekete Tükörtől érkező szennyezett víz Huk’ucha telepeket fenyeget. Relluk gátmester külön zárható medencékre "
          "osztja az árteret – és minden lezárt víz máshová folyik.", ["egsz_4c_ooni"], [h1, h2, h3, h4, h5])
    print("kész: egsz_4c_ooni, egsz_4d_hukucha")
