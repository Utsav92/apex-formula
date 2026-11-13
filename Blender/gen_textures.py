"""Procedural PBR-ish colour textures for APEX Formula (original, generated; no third-party images).

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_textures.py
Out:  Blender/export/textures/T_*.png  (all tileable unless noted)
"""
import os

import bpy
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export", "textures")
os.makedirs(OUT, exist_ok=True)


def vnoise(w, h, cells_x, cells_y, rng):
    """Tileable bilinear value noise in [0,1], size (h, w)."""
    g = rng.random((cells_y, cells_x))
    xs = np.linspace(0, cells_x, w, endpoint=False)[None, :]
    ys = np.linspace(0, cells_y, h, endpoint=False)[:, None]
    x0 = xs.astype(int); y0 = ys.astype(int)
    fx = xs - x0; fy = ys - y0
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy)
    x1 = (x0 + 1) % cells_x; y1 = (y0 + 1) % cells_y
    return (g[y0, x0] * (1 - fx) * (1 - fy) + g[y0, x1] * fx * (1 - fy)
            + g[y1, x0] * (1 - fx) * fy + g[y1, x1] * fx * fy)


def fbm(w, h, base, octaves, rng):
    total = np.zeros((h, w)); amp = 1.0; norm = 0.0
    for o in range(octaves):
        total += amp * vnoise(w, h, base * 2 ** o, base * 2 ** o if w == h else max(1, int(base * 2 ** o * h / w)), rng)
        norm += amp; amp *= 0.5
    return total / norm


def save(name, rgb):
    """rgb float array (h, w, 3) in 0..1; stored as sRGB PNG."""
    h, w, _ = rgb.shape
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(rgb, 0, 1)
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = "sRGB"
    # Blender stores rows bottom-to-top.
    img.pixels.foreach_set(rgba[::-1].ravel())
    img.filepath_raw = os.path.join(OUT, name + ".png")
    img.file_format = "PNG"
    img.save()
    print("TEXTURE", name, w, h)


rng = np.random.default_rng(7)
S = 1024

# Asphalt: U across the road (0..1), V along it (tiles every ~14 m). Edge lines + a rubbered racing line.
a = 0.20 + 0.07 * fbm(S, S, 8, 5, rng) + 0.07 * (rng.random((S, S)) - 0.5)
a += (rng.random((S, S)) > 0.992) * 0.18                                # bright aggregate specks
a -= (rng.random((S, S)) > 0.996) * 0.08                                # dark pits
u = np.linspace(0, 1, S)[None, :]
a *= 1 - 0.28 * np.exp(-((u - 0.5) / 0.14) ** 2)                         # darker, rubbered centre line
for x0, x1 in ((0.025, 0.040), (0.960, 0.975)):                           # white edge lines
    m = ((u > x0) & (u < x1)).astype(float) * (0.78 + 0.1 * fbm(S, S, 16, 3, rng))
    a = a * (1 - m) + m * 0.82
save("T_Asphalt", np.dstack([a * 1.0, a * 1.0, a * 1.03]))

# Grass: mottled green with blade-scale grain.
g = fbm(S, S, 6, 6, rng); fine = rng.random((S, S))
gr = 0.06 + 0.11 * g + 0.03 * fine
save("T_Grass", np.dstack([gr * 0.55, gr * 1.9, gr * 0.45]))

# Grass macro variation (low frequency), used to break up tiling.
mv = 0.65 + 0.7 * fbm(256, 256, 3, 4, rng)
save("T_GrassMacro", np.dstack([mv, mv, mv]))

# Kerb: red/white bars along V (square-ish tiles), slight wear.
k = np.zeros((S, S, 3)); v = np.linspace(0, 1, S)[:, None] * np.ones((1, S))
red = np.array([0.72, 0.04, 0.05]); white = np.array([0.86, 0.86, 0.84])
bar = (v < 0.5)[..., None]
k = np.where(bar, red, white) * (0.9 + 0.12 * fbm(S, S, 12, 4, rng))[..., None]
save("T_Kerb", k)

# Barrier: 8 m long panels (U) over a 1.2 m wall (V): white body, coloured band, dark base.
W2, H2 = 2048, 256
b = np.ones((H2, W2, 3)) * 0.80
vv = np.linspace(0, 1, H2)[:, None]; uu = np.linspace(0, 1, W2)[None, :]
band = ((vv > 0.38) & (vv < 0.68))
panel = (np.floor(uu * 4) % 2).astype(bool) * np.ones((H2, 1), dtype=bool)  # alternating panels
col = np.where(panel[..., None], np.array([0.05, 0.12, 0.60]), np.array([0.75, 0.03, 0.05]))
b = np.where(band[..., None], col, b)
b = np.where((vv < 0.12)[..., None], np.array([0.06, 0.06, 0.06]), b)
b *= (0.92 + 0.1 * fbm(W2, H2, 16, 4, rng))[..., None]
save("T_Barrier", b)

# Chequered start line (not tiled).
ch = np.zeros((256, 256, 3)); yy, xx = np.mgrid[0:256, 0:256]
ch[:] = np.where(((xx // 32 + yy // 32) % 2 == 0)[..., None], 0.92, 0.04)
save("T_Chequer", ch)


def facade(name, frame_col, glass_a, glass_b, floors=5, wins=4, lit=0.0):
    """One tile = floors x wins windows. Frame + glass with a sky-reflection gradient."""
    W3, H3 = 512, 1024
    img = np.ones((H3, W3, 3)) * np.array(frame_col)
    img *= (0.9 + 0.15 * fbm(W3, H3, 8, 4, rng))[..., None]
    fh, fw = H3 // floors, W3 // wins
    for fy in range(floors):
        for fx in range(wins):
            y0, y1 = fy * fh + int(fh * 0.16), (fy + 1) * fh - int(fh * 0.10)
            x0, x1 = fx * fw + int(fw * 0.10), (fx + 1) * fw - int(fw * 0.10)
            t = np.linspace(0, 1, y1 - y0)[:, None, None]
            glass = (1 - t) * np.array(glass_a) + t * np.array(glass_b)
            glass = glass * (0.85 + 0.3 * rng.random())                    # per-window variation
            if rng.random() < lit:
                glass = np.array([0.95, 0.85, 0.55])
            img[y0:y1, x0:x1] = glass
            img[y0:y0 + 3, x0:x1] *= 0.5                                    # frame shadow line
    save(name, img)


facade("T_FacadeGlass", (0.30, 0.31, 0.33), (0.30, 0.50, 0.68), (0.08, 0.14, 0.22))
facade("T_FacadeConcrete", (0.62, 0.60, 0.56), (0.20, 0.27, 0.34), (0.06, 0.09, 0.12), floors=5, wins=3)
facade("T_FacadeBrick", (0.45, 0.20, 0.15), (0.16, 0.22, 0.28), (0.05, 0.07, 0.09), floors=5, wins=4)

# Grandstand seats: rows of coloured seats.
se = np.ones((512, 512, 3)) * 0.12
for row in range(32):
    for col_i in range(64):
        c = rng.choice([0, 1, 2, 3], p=[0.4, 0.25, 0.2, 0.15])
        colour = [(0.75, 0.04, 0.05), (0.90, 0.90, 0.90), (0.05, 0.12, 0.55), (0.9, 0.7, 0.1)][c]
        y0, x0 = row * 16 + 3, col_i * 8 + 1
        se[y0:y0 + 10, x0:x0 + 6] = np.array(colour) * (0.8 + 0.3 * rng.random())
save("T_Seats", se)


# ---------------------------------------------------------------------------------------------------
# Car detail textures
# ---------------------------------------------------------------------------------------------------
def save_rgba(name, rgba):
    h, w, _ = rgba.shape
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.colorspace_settings.name = "sRGB"
    img.alpha_mode = "STRAIGHT"
    img.pixels.foreach_set(np.clip(rgba, 0, 1).astype(np.float32)[::-1].ravel())
    img.filepath_raw = os.path.join(OUT, name + ".png")
    img.file_format = "PNG"
    img.save()
    print("TEXTURE", name, w, h)


# 2x2 twill carbon weave. UV scale in the car script is 1 tile per 0.30 m.
T = 512; s = 16
yy, xx = np.mgrid[0:T, 0:T]
cell = (xx // s + yy // s) % 4 < 2
gx = (xx % s) / s; gy = (yy % s) / s
grad = np.where(cell, gx, gy)
sheen = 0.025 + 0.075 * (0.5 + 0.5 * np.sin(grad * np.pi))
sheen = sheen * np.where(cell, 1.0, 0.72) + 0.012 * rng.random((T, T))
save("T_Carbon", np.dstack([sheen * 0.95, sheen * 0.98, sheen * 1.08]))

# Tyre sidewall (planar disc map; outer radius = full tyre radius). Compound colour ring + tick marks.
D = 512
yy, xx = np.mgrid[0:D, 0:D]
cx = (xx + 0.5) / D - 0.5; cy = (yy + 0.5) / D - 0.5
r = np.sqrt(cx * cx + cy * cy) / 0.5
ang = np.arctan2(cy, cx)
tw = 0.03 + 0.02 * rng.random((D, D))
tw += 0.015 * np.sin(r * 90)                                                # moulded concentric ridges
ring = (r > 0.70) & (r < 0.80)                                              # soft-compound red band
tick = (r > 0.82) & (r < 0.92) & (np.abs(((ang / (2 * np.pi)) * 72) % 1.0 - 0.5) < 0.12)
tyre = np.dstack([tw, tw, tw * 1.05])
tyre[ring] = np.array([0.72, 0.03, 0.04]) * (0.9 + 0.1 * rng.random())
tyre[tick] = np.array([0.55, 0.55, 0.55])
tyre[r > 1.0] = 0.012                                                       # tread corner region
tyre[0:4, 0:4] = 0.012
save("T_TyreSidewall", tyre)

# Rim face: 5 spokes, silver outer ring and hub, dark between the spokes.
rim = np.ones((D, D, 3)) * 0.02
spoke = (np.abs(((ang / (2 * np.pi)) * 5 + 0.0) % 1.0 - 0.5) < 0.17) & (r > 0.14) & (r < 0.98)
silver = np.array([0.62, 0.64, 0.68])
rim[spoke] = silver * (0.85 + 0.15 * rng.random())
rim[(r > 0.86) & (r <= 1.0)] = silver * 0.8
rim[r < 0.15] = silver
rim[(r > 0.16) & (r < 0.2)] = 0.01
save("T_RimSpokes", rim)


GLYPH = {
    "0": [[(0.12, 0), (0, 0.15), (0, 0.85), (0.12, 1), (0.88, 1), (1, 0.85), (1, 0.15), (0.88, 0), (0.12, 0)]],
    "1": [[(0.15, 0.78), (0.55, 1), (0.55, 0)]],
    "3": [[(0, 0.88), (0.15, 1), (0.85, 1), (1, 0.86), (1, 0.66), (0.85, 0.52), (0.3, 0.52)], [(0.85, 0.52), (1, 0.38), (1, 0.14), (0.85, 0), (0.15, 0), (0, 0.12)]],
    "4": [[(0.75, 0), (0.75, 1), (0, 0.3), (1, 0.3)]],
    "5": [[(1, 1), (0.1, 1), (0.04, 0.55), (0.8, 0.6), (1, 0.45), (1, 0.15), (0.85, 0), (0.15, 0), (0, 0.12)]],
    "6": [[(0.85, 1), (0.25, 1), (0, 0.7), (0, 0.15), (0.15, 0), (0.85, 0), (1, 0.15), (1, 0.4), (0.85, 0.55), (0.15, 0.55), (0, 0.4)]],
    "8": [[(0.15, 0.5), (0, 0.7), (0, 0.88), (0.12, 1), (0.88, 1), (1, 0.88), (1, 0.7), (0.85, 0.5), (0.15, 0.5), (0, 0.3), (0, 0.12), (0.12, 0), (0.88, 0), (1, 0.12), (1, 0.3), (0.85, 0.5)]],
    "9": [[(0.15, 0.45), (0, 0.6), (0, 0.88), (0.12, 1), (0.88, 1), (1, 0.88), (1, 0.6), (0.85, 0.45), (0.15, 0.45)], [(1, 0.6), (1, 0.14), (0.85, 0), (0.15, 0), (0, 0.12)]],
    "2": [[(0, 0.80), (0.18, 1), (0.82, 1), (1, 0.80), (1, 0.62), (0, 0), (1, 0)]],
    "7": [[(0, 1), (1, 1), (0.34, 0)]],
    "A": [[(0, 0), (0.5, 1), (1, 0)], [(0.2, 0.38), (0.8, 0.38)]],
    "P": [[(0, 0), (0, 1), (0.72, 1), (0.98, 0.82), (0.98, 0.62), (0.72, 0.46), (0, 0.46)]],
    "E": [[(1, 1), (0, 1), (0, 0), (1, 0)], [(0, 0.5), (0.72, 0.5)]],
    "X": [[(0, 0), (1, 1)], [(0, 1), (1, 0)]],
}


def render_text(w, h, text, text_w, text_h, thick, gap):
    """Aspect-correct bold stroke text. All sizes are in units of the image HEIGHT; text is centred.
    Returns coverage 0..1 (anti-aliased by a ~1.5 px soft edge)."""
    py, px = np.mgrid[0:h, 0:w]
    X = (px + 0.5) / h
    Y = 1.0 - (py + 0.5) / h
    n = len(text)
    gw = (text_w - gap * (n - 1)) / n
    ox = (w / h - text_w) / 2
    oy = (1.0 - text_h) / 2
    best = np.full((h, w), 1e9)
    for i, ch in enumerate(text):
        x0 = ox + i * (gw + gap)
        for line in GLYPH[ch]:
            pts = [(x0 + a * gw, oy + b * text_h) for a, b in line]
            for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
                dx, dy = bx - ax, by - ay
                t = np.clip(((X - ax) * dx + (Y - ay) * dy) / max(dx * dx + dy * dy, 1e-9), 0, 1)
                best = np.minimum(best, np.hypot(X - (ax + t * dx), Y - (ay + t * dy)))
    return np.clip((thick - best) * h / 1.5 + 0.5, 0, 1)


# Race number roundel: white disc, black bold "27", centred.
N_ = 512
cov_num = render_text(N_, N_, "27", 0.58, 0.52, 0.052, 0.10)
py, px = np.mgrid[0:N_, 0:N_]
disc = np.clip((0.495 - np.hypot((px + 0.5) / N_ - 0.5, (py + 0.5) / N_ - 0.5)) * N_ / 1.5 + 0.5, 0, 1)
rgba = np.zeros((N_, N_, 4))
rgba[..., :3] = (0.96 * (1 - cov_num) + 0.02 * cov_num)[..., None]
rgba[..., 3] = disc
save_rgba("T_DecalNumber", rgba)

# APEX wordmark: bold white letters on transparent (1024x256 -> aspect 4:1).
W4, H4 = 1024, 256
cov = render_text(W4, H4, "APEX", 2.9, 0.58, 0.055, 0.17)
rgba = np.zeros((H4, W4, 4)); rgba[..., :3] = 0.97; rgba[..., 3] = cov
save_rgba("T_DecalApex", rgba)


# ---------------------------------------------------------------------------------------------------
# Environment detail pass: crowd, foliage, bark, pit building, billboards, banner, rubber marks
# ---------------------------------------------------------------------------------------------------
# Crowd (overwrites the plain-seat T_Seats): 64 x 32 cells of seated people. cell = 8 x 16 px.
crowd = np.ones((512, 512, 3)) * np.array([0.08, 0.08, 0.10])
SHIRTS = [(0.75, 0.04, 0.05), (0.92, 0.92, 0.92), (0.05, 0.12, 0.60), (0.95, 0.75, 0.10), (0.10, 0.45, 0.20), (0.15, 0.15, 0.17), (0.9, 0.4, 0.05)]
SKIN = [(0.85, 0.65, 0.5), (0.65, 0.45, 0.32), (0.45, 0.30, 0.2), (0.92, 0.75, 0.6), (0.3, 0.2, 0.14)]
HAIR = [(0.08, 0.06, 0.05), (0.35, 0.2, 0.08), (0.8, 0.65, 0.3), (0.45, 0.45, 0.45)]
yy8, xx8 = np.mgrid[0:16, 0:8]
head = ((xx8 - 3.5) ** 2 + (yy8 - 4.5) ** 2) < 7.5
hair = ((xx8 - 3.5) ** 2 + (yy8 - 3.7) ** 2) < 7.5
hair &= yy8 < 4
torso = (yy8 >= 7) & (xx8 >= 1) & (xx8 <= 6)
for row in range(32):
    for col in range(64):
        if rng.random() < 0.06:        # an empty seat
            continue
        y0, x0 = row * 16, col * 8
        cell = crowd[y0:y0 + 16, x0:x0 + 8]
        shirt = np.array(SHIRTS[rng.integers(len(SHIRTS))]) * (0.8 + 0.3 * rng.random())
        cell[torso] = shirt
        cell[head] = np.array(SKIN[rng.integers(len(SKIN))])
        cell[hair & head] = np.array(HAIR[rng.integers(len(HAIR))])
        if rng.random() < 0.18:        # arms up, cheering
            cell[3:8, 0] = cell[head][0] if head.any() else shirt
            cell[3:8, 7] = cell[head][0] if head.any() else shirt
save("T_Seats", crowd)

# Foliage: tileable clumpy greens (pine is darker/bluer, oak lighter/yellower).
def leaf_tex(name, base, clump_dark, seed):
    r = np.random.default_rng(seed)
    big = fbm(S, S, 5, 4, r)
    clump = vnoise(S, S, 28, 28, r)
    grain = r.random((S, S))
    v = 0.55 + 0.6 * big
    v = v * (0.72 + 0.5 * clump) * (0.92 + 0.16 * grain)
    img = np.dstack([base[0] * v, base[1] * v, base[2] * v])
    img = np.where((clump < 0.28)[..., None], img * clump_dark, img)   # dark gaps between leaf clumps
    save(name, img)


leaf_tex("T_LeafPine", (0.04, 0.16, 0.08), 0.55, 21)
leaf_tex("T_LeafOak", (0.10, 0.30, 0.05), 0.6, 22)

# Bark: vertical streaks.
bk = 0.5 * vnoise(S, S, 36, 3, rng) + 0.35 * vnoise(S, S, 90, 8, rng) + 0.15 * rng.random((S, S))
save("T_Bark", np.dstack([0.20 * (0.4 + bk), 0.12 * (0.4 + bk), 0.07 * (0.4 + bk)]))

# Pit building front: one tile = 15 m wide x 6 m tall, two roller garage doors, team colour band above.
PW, PH = 1024, 410
pit = np.ones((PH, PW, 3)) * 0.58
pit *= (0.92 + 0.12 * fbm(PW, PH, 10, 4, rng))[..., None]
pit[: int(PH * 0.20)] = np.array([0.05, 0.10, 0.35])                    # blue band
pit[int(PH * 0.20): int(PH * 0.23)] = 0.92                              # white stripe
for dx in (0.07, 0.55):
    x0, x1 = int(PW * dx), int(PW * (dx + 0.38))
    y0, y1 = int(PH * 0.30), int(PH * 0.98)
    pit[y0 - 6:y1, x0 - 6:x1 + 6] = 0.20                                # door frame
    for yy_ in range(y0, y1):
        pit[yy_, x0:x1] = 0.34 if ((yy_ - y0) // 9) % 2 == 0 else 0.27  # roller slats
    pit[y0:y0 + 14, x0:x1] = 0.12
    pit[int(PH * 0.26): int(PH * 0.29), x0:x1] = np.array([0.75, 0.04, 0.05])  # bay marker light
save("T_PitFront", pit)

# Billboards: 2048 x 256 boards (8 m x 1 m aspect).
BW, BH = 2048, 256
def board_base(bg):
    return np.ones((BH, BW, 3)) * np.array(bg)

yb, xb = np.mgrid[0:BH, 0:BW]
a = board_base((0.70, 0.03, 0.05))                                       # A: red, white APEX, chevrons
cov = render_text(BW, BH, "APEX", 2.7, 0.55, 0.05, 0.17)
a = a * (1 - cov[..., None]) + 0.96 * cov[..., None]
for k in range(8):
    ch = ((xb + yb * 0.9) // 60 + k) % 12 < 2
    a[(xb > 1500 + k * 0) & ch] = np.minimum(a[(xb > 1500) & ch], 1.0) * 0.6 + 0.35
save("T_BoardA", a)

b2 = board_base((0.04, 0.10, 0.45))                                      # B: blue, diagonal white stripes + rings
stripe = ((xb + yb) // 40) % 5 == 0
b2[stripe & (xb < 900)] = 0.9
for cxr in (1300, 1650):
    ring = (np.abs(np.hypot(xb - cxr, yb - 128) - 85) < 16) | (np.abs(np.hypot(xb - cxr, yb - 128) - 40) < 14)
    b2[ring] = 0.95
save("T_BoardB", b2)

c2 = board_base((0.95, 0.75, 0.08))                                      # C: yellow, black chequer band and big 27
c2[(yb < 50) | (yb > 206)] = np.where((((xb // 50) + (yb // 50)) % 2 == 0)[(yb < 50) | (yb > 206)][..., None], 0.05, 0.95)
covc = render_text(BW, BH, "27", 1.0, 0.5, 0.055, 0.14)
c2 = c2 * (1 - covc[..., None]) + 0.05 * covc[..., None]
save("T_BoardC", c2)

# Start gantry banner: 16:1.
GW_, GH_ = 2048, 128
gb = np.ones((GH_, GW_, 3)) * np.array([0.08, 0.08, 0.10])
covg = render_text(GW_, GH_, "APEX", 3.6, 0.58, 0.06, 0.28)
gb = gb * (1 - covg[..., None]) + 0.95 * covg[..., None]
gb[:10] = np.array([0.75, 0.04, 0.05]); gb[-10:] = np.array([0.75, 0.04, 0.05])
save("T_Banner", gb)

# Rubber marks: two dark tyre tracks (U across), broken up along V. Alpha only; colour is near-black.
RS = 512
ur = np.linspace(0, 1, RS)[None, :] * np.ones((RS, 1))
vr = vnoise(RS, RS, 4, 24, rng)
track = np.exp(-((ur - 0.30) / 0.045) ** 2) + np.exp(-((ur - 0.70) / 0.045) ** 2)
alpha = np.clip(track * (0.35 + 0.65 * vr) * (0.7 + 0.5 * rng.random((RS, RS))), 0, 0.85)
rub = np.zeros((RS, RS, 4)); rub[..., :3] = 0.015; rub[..., 3] = alpha
save_rgba("T_Rubber", rub)


# ---------------------------------------------------------------------------------------------------
# Car paint inputs: flake normal map, dirt mask, scratch mask (all tileable, stored as raw data)
# ---------------------------------------------------------------------------------------------------
def save_data(name, rgb):
    """Like save() but non-colour data (normal maps, masks): values are written as-is."""
    h, w, _ = rgb.shape
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(rgb, 0, 1)
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = "Non-Color"
    img.pixels.foreach_set(rgba[::-1].ravel())
    img.filepath_raw = os.path.join(OUT, name + ".png")
    img.file_format = "PNG"
    img.save()
    print("TEXTURE", name, w, h, "(data)")


# Metallic flakes: each 2x2 px cell gets a randomly tilted micro-facet normal.
FS = 512
cells = FS // 2
tilt = rng.normal(0, 0.55, (cells, cells, 2))
tilt = np.clip(tilt, -1.2, 1.2)
tilt = np.repeat(np.repeat(tilt, 2, axis=0), 2, axis=1)
nz = np.ones((FS, FS))
nrm = np.dstack([tilt[..., 0], tilt[..., 1], nz])
nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
save_data("T_FlakeNormal", nrm * 0.5 + 0.5)

# Dirt: low-frequency blotches, stretched streaks, and a grime rim, as a greyscale mask.
dirt = 0.55 * fbm(S, S, 4, 5, rng) + 0.30 * vnoise(S, S, 10, 70, rng) + 0.15 * rng.random((S, S))
dirt = np.clip((dirt - 0.35) * 2.2, 0, 1)
save_data("T_CarDirt", np.dstack([dirt, dirt, dirt]))

# Scratches: many thin, random, slightly curved hairlines, plus a few deeper gouges.
sc = np.zeros((S, S))
for k in range(420):
    x, y = rng.random() * S, rng.random() * S
    ang = rng.random() * np.pi
    length = rng.integers(25, 180)
    bend = (rng.random() - 0.5) * 0.01
    strength = 0.35 + 0.65 * rng.random() * (1.0 if k < 380 else 1.5)
    for t in range(length):
        ang += bend
        x += np.cos(ang); y += np.sin(ang)
        sc[int(y) % S, int(x) % S] = max(sc[int(y) % S, int(x) % S], min(strength, 1.0))
save_data("T_CarScratch", np.dstack([sc, sc, sc]))


# ---------------------------------------------------------------------------------------------------
# Road marks mask (data): R = painted edge lines, G = rubbered racing line. Lets real scanned asphalt show through.
# U across the road, V along it, tiles every ~14 m like the old combined asphalt texture.
# ---------------------------------------------------------------------------------------------------
um = np.linspace(0, 1, S)[None, :] * np.ones((S, 1))
wear = 0.78 + 0.22 * fbm(S, S, 14, 4, rng)
line_mask = (((um > 0.025) & (um < 0.040)) | ((um > 0.960) & (um < 0.975))).astype(float) * wear
rubber_mask = np.exp(-((um - 0.5) / 0.16) ** 2) * (0.35 + 0.65 * vnoise(S, S, 3, 18, rng))
marks = np.dstack([line_mask, np.clip(rubber_mask, 0, 1), np.zeros((S, S))])
save_data("T_RoadMarks", marks)


# One roundel texture per driver number (the car's DecalNum slot gets T_DecalNum_<n> at spawn).
NUMBERS = [27, 1, 3, 4, 5, 7, 8, 10, 11, 14, 16, 18, 20, 22, 23, 31, 44, 55, 63, 77]
for n in NUMBERS:
    txt = str(n)
    tw = 0.58 if len(txt) == 2 else 0.30
    cov_n = render_text(N_, N_, txt, tw, 0.52, 0.052, 0.10)
    rgba_n = np.zeros((N_, N_, 4))
    rgba_n[..., :3] = (0.96 * (1 - cov_n) + 0.02 * cov_n)[..., None]
    rgba_n[..., 3] = disc
    save_rgba(f"T_DecalNum_{n}", rgba_n)
