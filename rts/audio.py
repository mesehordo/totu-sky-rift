"""Zene és hangok.

ZENE – assets/music/*.mp3
  * a programindítástól és a menükben: menuzene.mp3 (ismétlődik);
  * csata indulásakor véletlen szám a mappából (a menuzene kivételével); ha véget ér, másik jön (nem ugyanaz);
  * küldetésnél, ha van saját zenéje (assets/music/kuldetesek/<küldetés-id>/*.mp3), csak abból válogat;
  * játék közben a szünetmenü „Zene” pontjában bármelyik szám kiválasztható; utána ismét véletlenszerű a sorrend.
  Új szám hozzáadásához elég bemásolni a mappába – nem kell semmit beállítani.

HANGOK – assets/sounds/… (ogg, wav vagy mp3; több változat: _1, _2, … – véletlenül váltakoznak)
  Keresési sorrend egy egység eseményére:
    units/<egység-id>/<esemény>     (pl. units/urs_pajzshordozo/select_2.ogg)
    factions/<faj-id>/<esemény>     (a nép közös hangja, pl. factions/ursan/death_1.ogg)
    common/<egységtípus>/<esemény>  (kozelharcos, lovesz, varazslo, munkas, orias, repulo, lovas, ostrom, landzsas)
    common/<esemény>
  Események: select (kijelölés), move (mozgásparancs), attack_order (támadásparancs), attack (csapás/lövés),
  death (halál), trained (elkészült), building_done (épület kész), under_attack (riasztás), ability (képesség).
  Ha nincs fájl, csend – a játék hangfájlok nélkül is fut.

A szimuláció a hangeseményeket a game.sound_events listába írja (nem befolyásolja a játékmenetet); a UI
minden képkockában kiüríti és lejátssza azokat, amelyek a játékos számára láthatók / fontosak.
"""
import os
import random

import pygame

from .data import ROOT

MUSIC_DIR = os.path.join(ROOT, "assets", "music")
SOUND_DIR = os.path.join(ROOT, "assets", "sounds")
MENU_TRACK = "menuzene.mp3"
EXTS = (".ogg", ".wav", ".mp3")
EVENTS = ("select", "move", "attack_order", "attack", "death", "trained", "building_done", "under_attack", "ability")


VOLUME_ROWS = (
    ("master", "Főhangerő"),
    ("music", "Zene"),
    ("units", "Egység- és hőshangok"),
    ("combat", "Harci effektek"),
    ("buildings", "Épülethangok"),
    ("interface", "Kezelőfelület és riasztások"),
    ("environment", "Környezeti hangok"),
)


def volume_setting(settings, category):
    default = {"master": 100, "music": 60}.get(category, settings.get("sfx_volume", 80))
    return max(0, min(100, settings.get(category + "_volume", default)))


class Audio:
    def __init__(self, settings=None):
        self.ok = False
        self.settings = settings if settings is not None else {}
        self.mode = None          # "menu" | "game"
        self.current = None
        self.game_music_exhausted = False
        self.bad_game_tracks = set()
        self.cache = {}
        self.active_channels = []
        self.cooldown = {}        # (kulcs) -> következő megengedett idő (ms)
        self.rng = random.Random()
        try:
            if not pygame.mixer.get_init():
                pygame.mixer.init(44100, -16, 2, 512)
            pygame.mixer.set_num_channels(24)
            self.ok = True
        except pygame.error as e:  # nincs hangeszköz: néma mód
            print("[hang] nem indítható:", e)

    # ---- hangerő ----
    def category_volume(self, category):
        if not (self.settings.get("master_enabled", True) and self.settings.get(category + "_enabled", True)):
            return 0.0
        return volume_setting(self.settings, "master") * volume_setting(self.settings, category) / 10000.0

    def music_vol(self):
        return self.category_volume("music")

    def environment_available(self):
        return any(self._load(path) is not None for path in self._find("common/environment")) if self.ok else False

    def apply_volume(self):
        if not self.ok:
            return
        pygame.mixer.music.set_volume(self.music_vol())
        self._prune_channels()
        for channel, _, category, gain in self.active_channels:
            channel.set_volume(self.category_volume(category) * gain)

    def _prune_channels(self):
        self.active_channels = [
            item for item in self.active_channels if item[0].get_busy() and item[0].get_sound() is item[1]
        ]

    # ---- zene ----
    def tracks(self):
        """Az általános számok (a fő mappa .mp3-jai, relatív névvel)."""
        if not os.path.isdir(MUSIC_DIR):
            return []
        return sorted(f for f in os.listdir(MUSIC_DIR) if f.lower().endswith(".mp3"))

    def mission_tracks(self, mission_id):
        d = os.path.join(MUSIC_DIR, "kuldetesek", mission_id or "")
        if not mission_id or not os.path.isdir(d):
            return []
        return sorted(f"kuldetesek/{mission_id}/{f}" for f in os.listdir(d) if f.lower().endswith(".mp3"))

    def all_tracks(self):
        """Minden szám a választóhoz: (megjelenő név, relatív útvonal) – előbb a küldetés saját zenéi."""
        out = [(os.path.splitext(os.path.basename(t))[0], t) for t in self.mission_tracks(self.mission)]
        out += [(os.path.splitext(t)[0], t) for t in self.tracks() if t.lower() != MENU_TRACK]
        return out

    def choose(self, rel):
        """Kézi választás (szünetmenü): ez a szám szól; utána folytatódik a véletlen sorrend."""
        self.mode = "game"
        if self._play(rel):
            self.game_music_exhausted = False
            self.bad_game_tracks.discard(rel)

    def _play(self, name, loops=0):
        if not self.ok:
            return False
        p = os.path.join(MUSIC_DIR, *name.split("/"))
        if not os.path.isfile(p):
            return False
        try:
            pygame.mixer.music.load(p)
            pygame.mixer.music.set_volume(self.music_vol())
            pygame.mixer.music.play(loops, fade_ms=1200)
            self.current = name
            return True
        except pygame.error as e:
            print("[zene] nem játszható:", name, e)
            return False

    def menu_music(self):
        """A menük zenéje (menuzene.mp3, ismétlődve). Ha már ez szól, nem kezdi újra."""
        if self.mode == "menu" and self.current == MENU_TRACK and self.ok and pygame.mixer.music.get_busy():
            return
        self.mode = "menu"
        if not self._play(MENU_TRACK, loops=-1):
            self.current = None

    mission = None

    def game_music(self, mission_id=None):
        """Csata indulása: véletlen szám – a küldetés saját zenéiből, ha vannak, különben a fő mappából."""
        self.mode = "game"
        self.mission = mission_id
        self.game_music_exhausted = False
        self.bad_game_tracks.clear()
        self._next_game_track()

    def _next_game_track(self):
        if not self.ok:
            return
        for tracks in (self.mission_tracks(self.mission), self.tracks()):
            pool = [t for t in tracks if t.lower() != MENU_TRACK and t not in self.bad_game_tracks]
            self.rng.shuffle(pool)
            if len(pool) > 1 and self.current in pool:
                pool.remove(self.current)
                pool.append(self.current)
            for track in pool:
                if self._play(track):
                    return
                self.bad_game_tracks.add(track)
        pygame.mixer.music.fadeout(800)
        self.current = None
        self.game_music_exhausted = True

    def update(self):
        """Minden képkockában: ha a csata zenéje véget ért, jön a következő."""
        if (self.ok and self.mode == "game" and not self.game_music_exhausted
                and not pygame.mixer.music.get_busy()):
            self._next_game_track()

    # ---- hangeffektek ----
    def _find(self, rel_noext):
        """[(útvonal)] az adott névhez: <név>.ext vagy <név>_N.ext."""
        key = rel_noext
        if key in self.cache:
            return self.cache[key]
        d, base = os.path.split(os.path.join(SOUND_DIR, rel_noext))
        out = []
        if os.path.isdir(d):
            for f in sorted(os.listdir(d)):
                stem, ext = os.path.splitext(f)
                if ext.lower() in EXTS and (stem == base or (stem.startswith(base + "_") and stem[len(base) + 1:].isdigit())):
                    out.append(os.path.join(d, f))
        self.cache[key] = out
        return out

    def _load(self, path):
        k = ("snd", path)
        if k not in self.cache:
            try:
                self.cache[k] = pygame.mixer.Sound(path)
            except pygame.error:
                self.cache[k] = None
        return self.cache[k]

    def unit_sound(self, ev, defn, faction_id=None):
        """A legspecifikusabb létező hangfájl egy egység eseményéhez (vagy None)."""
        cands = []
        if defn is not None:
            cands.append(f"units/{defn['id']}/{ev}")
        if faction_id:
            cands.append(f"factions/{faction_id}/{ev}")
        if defn is not None and defn.get("utype"):
            cands.append(f"common/{defn['utype']}/{ev}")
        cands.append(f"common/{ev}")
        for c in cands:
            files = self._find(c)
            if files:
                return self.rng.choice(files)
        return None

    def play(self, ev, defn=None, faction_id=None, vol=1.0, throttle_ms=0, key=None, building=False):
        if ev == "environment":
            category = "environment"
        elif ev == "under_attack" or ev.startswith("ui_"):
            category = "interface"
        elif ev in ("building_done", "gate_arrival", "building_pack", "building_unpack") or (
                building and ev in ("select", "death")):
            category = "buildings"
        elif ev in ("select", "move", "attack_order", "trained", "death"):
            category = "units"
        else:
            category = "combat"
        volume = self.category_volume(category) * vol
        if not self.ok or volume <= 0:
            return
        now = pygame.time.get_ticks()
        k = key or (ev, defn["id"] if defn else None)
        if throttle_ms and self.cooldown.get(k, 0) > now:
            return
        path = self.unit_sound(ev, defn, faction_id)
        if path is None:
            return
        snd = self._load(path)
        if snd is None:
            return
        self._prune_channels()
        ch = snd.play()
        if ch is not None:
            ch.set_volume(volume)
            self.active_channels.append((ch, snd, category, vol))
        if throttle_ms:
            self.cooldown[k] = now + throttle_ms


_AUDIO = [None]


def get(settings=None):
    if _AUDIO[0] is None:
        _AUDIO[0] = Audio(settings)
    elif settings is not None and _AUDIO[0].settings is not settings:
        _AUDIO[0].settings = settings
        _AUDIO[0].apply_volume()
    return _AUDIO[0]
