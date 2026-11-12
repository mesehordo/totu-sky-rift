"""Építési animációk a csempe-alapú „épületekhez”: Huk’ucha-tavacska és Vasbirodalom-alapozás.

A meglévő képekből rajzol (fű/föld/homok csempe, tavirózsás víz-csempe, márvány-lapkő, kő- és sziklakép,
porfelhő). Minden függvény egy n×n csempés terület képét adja egy adott haladásnál (p: 0…1), ráhagyással
(MARGIN csempeképpont minden oldalon), HD képpontban (T = egy csempe szélessége).

Változatok (a bemutatóhoz – a játék a kiválasztottat használja):
  tavacska: "asas"  – kiásás (sötét gödör nő, földkupacok), majd beszivárog a víz;
            "csempe"– csempénként: fű → sár → víz, egymás után;
            "iszap" – előbb a homokos part rajzolódik ki, majd iszapos víz tölti meg, és kitisztul.
  alapozás: "lapok" – lapkövek egyenként, hátulról előre, kis eséssel és porral;
            "kituzes" – kitűzés (karók, zsinór) → kavicságy → márvány átlós söprése;
            "kohalom" – kőhalom fogy, a márvány középről kifelé terül szét.
"""
import math

import numpy as np
import pygame

MARGIN = 0.5   # csempe


def _rng(seed):
    return np.random.default_rng(seed)


def _surf_from_alpha(alpha, color):
    """(w, h) alfa-tömb (0…1) -> egyszínű RGBA felület."""
    s = pygame.Surface(alpha.shape, pygame.SRCALPHA)
    s.fill(color)
    pa = pygame.surfarray.pixels_alpha(s)
    pa[...] = (np.clip(alpha, 0, 1) * 255).astype(np.uint8)
    del pa
    return s


def _masked(tex, alpha):
    """A textúra az alfa-maszkkal (a tex mérete = a maszk mérete)."""
    s = tex.copy()
    s.blit(_surf_from_alpha(alpha, (255, 255, 255)), (0, 0), special_flags=pygame.BLEND_RGBA_MULT)
    return s


def _tiled(tile, w, h):
    s = pygame.Surface((w, h), pygame.SRCALPHA)
    tw, th = tile.get_size()
    for y in range(0, h, th):
        for x in range(0, w, tw):
            s.blit(tile, (x, y))
    return s


def _tiled_var(tiles, w, h, T, seed=0):
    s = pygame.Surface((w, h), pygame.SRCALPHA)
    k = 0
    for y in range(0, h, T):
        for x in range(0, w, T):
            t = tiles[(x // T * 7 + y // T * 13 + seed) % len(tiles)]
            if t.get_width() != T:
                t = pygame.transform.smoothscale(t, (T, T))
            s.blit(t, (x, y))
            k += 1
    return s


def _shape(W, H, cx, cy, rx, ry, rough=0.08, seed=1, square=2.6):
    """Lekerekített, kicsit szabálytalan folt (szuperellipszis) 0…1 lágy széllel; rx/ry: félszélesség."""
    xs = (np.arange(W)[:, None] - cx) / max(rx, 1e-3)
    ys = (np.arange(H)[None, :] - cy) / max(ry, 1e-3)
    ang = np.arctan2(ys, xs)
    r = (np.abs(xs) ** square + np.abs(ys) ** square) ** (1 / square)
    g = _rng(seed)
    wob = sum(g.uniform(-1, 1) * np.sin(k * ang + g.uniform(0, 6.3)) for k in (2, 3, 5)) * rough / 3
    d = r - (1 + wob)
    return np.clip(0.5 - d * min(rx, ry) / 3.0, 0, 1)


def pond_final_layers(tex, n, T):
    """A kész tavacska rétegei (homokos part, víz, hab) – ugyanaz a forma, mint a játékbeli kész tavacskánál."""
    M = int(T * MARGIN)
    S = n * T
    W = H = S + 2 * M
    c = W / 2
    v_out = _shape(W, H, c, c, S / 2 + T * 0.18, S / 2 + T * 0.18, 0.06, 3)
    v_in = _shape(W, H, c, c, S / 2 - T * 0.10, S / 2 - T * 0.10, 0.06, 3)
    return W, H, M, v_out, v_in


def draw_pond(tex, n, T, p, variant="asas", seed=0):
    W, H, M, v_out, v_in = pond_final_layers(tex, n, T)
    S = n * T
    c = W / 2
    out = pygame.Surface((W, H), pygame.SRCALPHA)
    sand = _tiled_var(tex["sand"], W, H, T, seed)
    dirt = _tiled_var(tex["dirt"], W, H, T, seed)
    water = _tiled_var(tex["pond"], W, H, T, seed)
    mud = dirt.copy()
    mud.fill((92, 78, 66, 255), special_flags=pygame.BLEND_RGBA_MULT)

    def finish(level_sand, level_water, foam=1.0, murk=0.0):
        out.blit(_masked(sand, v_out * level_sand), (0, 0))
        if level_water > 0:
            w = water
            if murk > 0:
                w = water.copy()
                tint = pygame.Surface((W, H), pygame.SRCALPHA)
                tint.fill((120, 92, 52, int(200 * murk)))
                w.blit(tint, (0, 0))
            out.blit(_masked(w, v_in * level_water), (0, 0))
        if foam > 0:
            ring = np.clip(1 - np.abs(v_in - 0.5) / 0.35, 0, 1) * 0.7 * foam
            out.blit(_surf_from_alpha(ring, (232, 246, 242)), (0, 0))

    if variant == "asas":
        dig = min(1.0, p / 0.55)
        fill = max(0.0, (p - 0.55) / 0.45)
        r = (S / 2 + T * 0.18) * (0.25 + 0.75 * dig)
        hole = _shape(W, H, c, c, r, r, 0.12, 5 + seed)
        if fill < 1:
            out.blit(_masked(dirt, np.clip(hole * 1.2, 0, 1) * (1 - fill)), (0, 0))          # kiásott föld
            dark = np.clip(_shape(W, H, c, c, r * 0.72, r * 0.72, 0.15, 6 + seed), 0, 1)
            out.blit(_surf_from_alpha(dark * 0.45 * (1 - fill), (40, 28, 20)), (0, 0))      # mélyülő gödör
        out.blit(_masked(sand, v_out * fill), (0, 0))
        if fill > 0:
            rw = (S / 2 - T * 0.10) * (0.35 + 0.65 * fill)
            wat = np.minimum(_shape(W, H, c, c, rw, rw, 0.1, 9 + seed), v_in)
            out.blit(_masked(water, wat), (0, 0))
            ring = np.clip(1 - np.abs(wat - 0.5) / 0.35, 0, 1) * 0.7
            out.blit(_surf_from_alpha(ring, (232, 246, 242)), (0, 0))
        # földkupacok a gödör szélén (a kiásott föld), a feltöltéskor elfogynak
        piles = int(round(5 * dig * (1 - fill)))
        g = _rng(40 + seed)
        angs = g.uniform(0, 6.28, 6)
        for k in range(piles):
            a = angs[k]
            px, py = c + math.cos(a) * (r + T * 0.12), c + math.sin(a) * (r * 0.9 + T * 0.10)
            pw = int(T * (0.35 + 0.2 * dig))
            mnd = _shape(pw, int(pw * 0.7), pw / 2, pw * 0.35, pw / 2, pw * 0.33, 0.2, 50 + k, 2)
            ps = _masked(pygame.transform.smoothscale(tex["dirt"][k % len(tex["dirt"])], (pw, int(pw * 0.7))), mnd)
            sh = _surf_from_alpha(mnd * 0.35, (20, 14, 10))
            out.blit(sh, (px - pw / 2 + 3, py - pw * 0.35 + 3))
            out.blit(ps, (px - pw / 2, py - pw * 0.35))
        return out, M

    if variant == "csempe":
        cells = [(x, y) for y in range(n) for x in range(n)]
        cells.sort(key=lambda t: (t[0] + t[1], t[1]))
        k = len(cells)
        full_sand = np.zeros((W, H), np.float32)
        full_water = np.zeros((W, H), np.float32)
        full_mud = np.zeros((W, H), np.float32)
        for i, (x, y) in enumerate(cells):
            q = np.clip(p * k - i * 0.75, 0, 1.5) / 1.5      # a cellák kicsit átfedve haladnak
            if q <= 0:
                continue
            cx, cy = M + x * T + T / 2, M + y * T + T / 2
            cell = _shape(W, H, cx, cy, T * 0.62, T * 0.62, 0.1, 70 + i, 3.0)
            full_mud = np.maximum(full_mud, cell * min(1, q / 0.5))
            if q > 0.5:
                full_water = np.maximum(full_water, cell * min(1, (q - 0.5) / 0.5))
        if p >= 0.999:
            finish(1, 1)
            return out, M
        full_sand = np.minimum(v_out, full_mud * 1.1)
        out.blit(_masked(sand, full_sand), (0, 0))
        out.blit(_masked(mud, np.minimum(full_mud, v_in) * (1 - full_water)), (0, 0))
        out.blit(_masked(water, np.minimum(full_water, v_in)), (0, 0))
        return out, M

    if variant == "iszap":
        shore = min(1.0, p / 0.3)
        fill = np.clip((p - 0.3) / 0.35, 0, 1)
        clear = np.clip((p - 0.65) / 0.35, 0, 1)
        # a part kirajzolódik (körbefutó homoksáv)
        xs = np.arange(W)[:, None] - c
        ys = np.arange(H)[None, :] - c
        ang = (np.arctan2(ys, xs) + math.pi) / (2 * math.pi)
        sweep = np.clip((shore * 1.08 - ang) / 0.08, 0, 1)
        rim = np.clip(v_out - v_in * 0.85, 0, 1) * sweep
        out.blit(_masked(sand, np.maximum(rim, v_out * fill)), (0, 0))
        if fill > 0:
            rw = (S / 2 - T * 0.10) * (0.3 + 0.7 * fill)
            wat = np.minimum(_shape(W, H, c, c, rw, rw, 0.14, 11 + seed), v_in)
            w = water.copy()
            tint = pygame.Surface((W, H), pygame.SRCALPHA)
            tint.fill((122, 96, 58, int(215 * (1 - clear))))
            w.blit(tint, (0, 0))
            out.blit(_masked(w, wat), (0, 0))
            ring = np.clip(1 - np.abs(wat - 0.5) / 0.35, 0, 1) * 0.7 * clear
            out.blit(_surf_from_alpha(ring, (232, 246, 242)), (0, 0))
        return out, M
    finish(1, 1)
    return out, M


def draw_foundation(tex, n, T, p, variant="lapok", seed=0, t=0.0):
    M = int(T * MARGIN)
    S = n * T
    W = H = S + 2 * M
    out = pygame.Surface((W, H), pygame.SRCALPHA)
    marble = tex["marble"]
    if marble.get_width() != T:
        marble = pygame.transform.smoothscale(marble, (T, T))
    gravel = _tiled_var(tex["sand"], S, S, T, seed + 3)
    gravel.fill((168, 160, 150, 255), special_flags=pygame.BLEND_RGBA_MULT)

    if variant == "lapok":
        sub = 1                                  # csempénként egy lapkő (a kész alapozás is így néz ki)
        st = T // sub
        slabs = [(x, y) for y in range(n * sub) for x in range(n * sub)]
        k = len(slabs)
        bed = min(1.0, p / 0.12)
        out.blit(_masked(gravel, np.full((S, S), bed * 0.9, np.float32)), (M, M))
        for i, (x, y) in enumerate(slabs):
            q = np.clip((p - 0.1) / 0.9 * k - i, 0, 1)     # 0…1 az adott lapkő lerakása
            if q <= 0:
                continue
            piece = marble.subsurface(((x % sub) * st, (y % sub) * st, st, st)).copy()
            pygame.draw.rect(piece, (138, 132, 124), (0, 0, st, st), 1)          # saját fuga
            pygame.draw.line(piece, (236, 232, 224), (1, 1), (st - 2, 1))
            pygame.draw.line(piece, (236, 232, 224), (1, 1), (1, st - 2))
            drop = (1 - q) ** 2 * st * 0.9
            a = int(255 * min(1, q * 2.5))
            piece.set_alpha(a)
            px, py = M + x * st, M + y * st
            if q < 1:
                sh = pygame.Surface((st, st), pygame.SRCALPHA)
                sh.fill((0, 0, 0, int(70 * q)))
                out.blit(sh, (px + 2, py + 2))
            out.blit(piece, (px, py - drop))
            if 0.85 < q < 1 and "dust" in tex:              # por a lerakáskor
                d = tex["dust"][min(len(tex["dust"]) - 1, int((q - 0.85) / 0.15 * len(tex["dust"])))]
                d = pygame.transform.smoothscale(d, (st, st))
                out.blit(d, (px, py + st * 0.2))
        return out, M

    if variant == "kituzes":
        stake = min(1.0, p / 0.18)
        grav = np.clip((p - 0.18) / 0.3, 0, 1)
        lay = np.clip((p - 0.48) / 0.52, 0, 1)
        # kavicságy (a bal felső saroktól terül)
        if grav > 0:
            xs = np.arange(S)[:, None] / S
            ys = np.arange(S)[None, :] / S
            gm = np.clip((grav * 2.1 - (xs + ys)) / 0.25, 0, 1)
            out.blit(_masked(gravel, gm), (M, M))
        # márvány átlós söprés, fényes élsávval
        if lay > 0:
            mt = _tiled(marble, S, S)
            xs = np.arange(S)[:, None] / S
            ys = np.arange(S)[None, :] / S
            mm = np.clip((lay * 2.2 - (xs + ys)) / 0.12, 0, 1)
            out.blit(_masked(mt, mm), (M, M))
            edge = np.clip(1 - np.abs(lay * 2.2 - (xs + ys) - 0.06) / 0.06, 0, 1) * (1 - lay ** 4)
            out.blit(_surf_from_alpha(edge * 0.6, (255, 250, 230)), (M, M))
        # karók a sarkokban és zsinór közöttük (a végén eltűnnek)
        if lay < 1:
            fade = 1 - lay
            cs = [(M, M), (M + S, M), (M + S, M + S), (M, M + S)]
            col = (200, 60, 40, int(230 * fade))
            ln = pygame.Surface((W, H), pygame.SRCALPHA)
            for i in range(4):
                a, b = cs[i], cs[(i + 1) % 4]
                q = np.clip(stake * 4 - i, 0, 1)
                if q > 0:
                    pygame.draw.line(ln, col, (a[0], a[1] - 6), (a[0] + (b[0] - a[0]) * q, a[1] - 6 + (b[1] - a[1]) * q), 2)
            for (x, y) in cs:
                pygame.draw.line(ln, (90, 64, 40, int(255 * fade)), (x, y), (x, y - 12), 3)
                pygame.draw.circle(ln, (240, 240, 230, int(255 * fade)), (x, y - 12), 2)
            out.blit(ln, (0, 0))
        return out, M

    if variant == "kohalom":
        spread = np.clip((p - 0.08) / 0.92, 0, 1)
        if spread > 0:
            mt = _tiled(marble, S, S)
            c = S / 2
            r = spread * S * 0.78
            xs = np.arange(S)[:, None] - c
            ys = np.arange(S)[None, :] - c
            dd = np.maximum(np.abs(xs), np.abs(ys)) * 0.55 + np.hypot(xs, ys) * 0.45
            mm = np.clip((r - dd) / (T * 0.15), 0, 1) if spread < 1 else np.ones((S, S))
            out.blit(_masked(mt, mm), (M, M))
        pile = 1 - spread                     # a kőhalom fogy
        if pile > 0.02:   # sziklákból rakott halom; ahogy fogy, egyre kevesebb és kisebb kő
            rocks = tex["rocks"]
            spots = [(0, 0, 1.0), (-0.22, 0.08, 0.8), (0.22, 0.1, 0.8), (-0.08, -0.14, 0.7), (0.12, -0.1, 0.65)]
            cnt = max(1, int(math.ceil(len(spots) * pile)))
            for k, (ox, oy, sc) in enumerate(spots[:cnt][::-1]):
                rk = rocks[k % len(rocks)]
                z = sc * (0.55 + 0.45 * pile) * T * 0.9 / rk.get_width()
                img = pygame.transform.smoothscale(rk, (max(1, int(rk.get_width() * z)), max(1, int(rk.get_height() * z))))
                out.blit(img, (W / 2 + ox * S - img.get_width() / 2, H / 2 + oy * S - img.get_height() * 0.75))
        if "dust" in tex and 0.05 < p < 0.98:
            d = tex["dust"][int(t * 12) % len(tex["dust"])]
            d = pygame.transform.smoothscale(d, (T, T))
            out.blit(d, (W / 2 - T / 2, H / 2 - T * 0.2))
        return out, M
    out.blit(_tiled(marble, S, S), (M, M))
    return out, M
