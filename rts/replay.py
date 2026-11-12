"""Parancsnapló és visszajátszás (replay).

A felvétel a kezdőállapot JSON-mentéséből (``rts/snapshot.py``) és a játékosparancsok tick szerinti listájából áll;
a parancsok kódolása ugyanaz, mint a LAN-on (``rts.net.enc`` / ``rts.net.execute``). Közben 10 mp-enként
állapothash is kerül a fájlba, így a visszajátszás minden ponton ellenőrizhető.

Fájl: gzip JSON, ``{"format": "totu-replay", "schema": 1, "start": <snapshot>, "mod_ids": [...],
"commands": [{"t": tick, "p": játékos, "c": [név, [arg...], {kw}]}...], "hashes": [[tick, hash]...],
"end_tick": n, "complete": bool}``.
A ``t`` tick az a ``Game.tick`` érték, amelynél a parancs érvénybe lép: a visszajátszás a következő ``update``
előtt hajtja végre (ugyanúgy, mint élő játékban, ahol a parancs két szimulációs lépés között érkezik).

Korlát (1. változat): egyéni és gépi csatákra érvényes. Kampányban a döntés-ablakok és az átvezetők valós idejű
léptetése nincs a naplóban.
"""
import gzip
import json
import os
import random

from . import snapshot
from .net import enc, execute, state_hash

FORMAT = "totu-replay"
SCHEMA = 1
HASH_EVERY = 300  # tick


class Recorder:
    def __init__(self, game):
        self.g = game
        self.start = snapshot.dump({"game": game, "rng": random.getstate()}, game.data)
        self.mod_ids = list(game.data.mod_ids)
        self.commands = []
        self.hashes = []
        self.complete = True

    def issue(self, pid, name, args, kw=None):
        """Egy játékosparancs naplózása (a végrehajtás a hívó dolga). Nem kódolható paraméternél a felvétel
        hiányosnak jelölődik, a játék megy tovább."""
        try:
            entry = [name, [enc(a) for a in args]]
            if kw:
                entry.append({k: enc(v) for k, v in kw.items()})
        except (TypeError, RecursionError):
            self.complete = False
            return
        self.commands.append({"t": self.g.tick, "p": pid, "c": entry})

    def after_tick(self):
        if self.g.tick % HASH_EVERY == 0:
            self.hashes.append([self.g.tick, state_hash(self.g)])

    def to_doc(self):
        return {"format": FORMAT, "schema": SCHEMA, "start": self.start, "mod_ids": self.mod_ids,
                "commands": self.commands, "hashes": self.hashes, "end_tick": self.g.tick,
                "complete": self.complete}

    def save(self, path):
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        text = json.dumps(self.to_doc(), ensure_ascii=False, separators=(",", ":"))
        tmp = path + ".tmp"
        with open(tmp, "wb") as f:
            f.write(gzip.compress(text.encode("utf-8"), compresslevel=6))
        os.replace(tmp, path)


def read(path):
    with open(path, "rb") as f:
        doc = json.loads(gzip.decompress(f.read()).decode("utf-8"))
    if not isinstance(doc, dict) or doc.get("format") != FORMAT:
        raise ValueError("Ez nem TOTU-visszajátszás.")
    if doc.get("schema") != SCHEMA:
        raise ValueError(f"Ismeretlen visszajátszás-séma: {doc.get('schema')}")
    return doc


def start_game(doc, data):
    if list(data.mod_ids) != list(doc.get("mod_ids", [])):
        raise ValueError(f"Eltér a modlista. Felvétel: {', '.join(doc.get('mod_ids', [])) or 'alapjáték'}; "
                         f"aktív: {', '.join(data.mod_ids) or 'alapjáték'}.")
    root = snapshot.load(doc["start"], data)
    random.setstate(root["rng"])
    return root["game"]


class Player:
    """Visszajátszás lépésenként (a megjelenítő vagy az ellenőrző hívja)."""

    def __init__(self, doc, data):
        self.doc = doc
        self.g = start_game(doc, data)
        self.cmds = sorted(doc.get("commands", []), key=lambda c: c["t"])
        self.ci = 0
        self.expected = {t: h for t, h in doc.get("hashes", [])}
        self.mismatch = None
        self.checked = 0

    @property
    def done(self):
        return self.g.tick >= self.doc.get("end_tick", 0)

    def step(self, dt):
        g = self.g
        while self.ci < len(self.cmds) and self.cmds[self.ci]["t"] <= g.tick:
            c = self.cmds[self.ci]
            execute(g, c["p"], [c["c"]])
            self.ci += 1
        g.update(dt)
        exp = self.expected.get(g.tick)
        if exp is not None:
            self.checked += 1
            if self.mismatch is None and state_hash(g) != exp:
                self.mismatch = g.tick


def verify(path, data):
    """A teljes felvétel lejátszása grafika nélkül. Vissza: (rendben?, ellenőrzött hash-ek, első eltérés tickje)."""
    doc = read(path)
    pl = Player(doc, data)
    dt = 1 / data.game["sim_hz"]
    while not pl.done:
        pl.step(dt)
    return pl.mismatch is None, pl.checked, pl.mismatch
