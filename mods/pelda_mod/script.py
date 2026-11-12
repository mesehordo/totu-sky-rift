"""Példa mod-script. Elérhető hookok: on_load(data), on_game_start(game), on_tick(game, dt)."""


def on_load(data):
    # adatszintű módosítás kódból: minden ursan egység +10% életerő
    for u in data.factions["ursan"]["units"].values():
        u["hp"] = int(u["hp"] * 1.1)


def on_game_start(game):
    for p in game.players:
        game.notify(p, "Jégkorszak mod aktív – a Vadak karámában Barlangi medve is nevelhető!", (160, 220, 255))
