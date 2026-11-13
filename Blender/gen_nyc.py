"""New York building types for APEX Formula (original geometry, generic NYC typologies; no real buildings, brands or logos).

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_nyc.py
Out:  Blender/export/Bld_NYC_<Name>.fbx  for Brownstone, Walkup, Mixed, Deco, Warehouse
Builds on gen_buildings.py's Builder (windows with reveals/frames, cornices, flat roofs). The FRONT facade is -Y in Blender (it lands facing +Y in UE after the
FBX flip), which is where shopfronts, stoops, awnings, signs and fire escapes go; the other three faces are plain bays.
Extra slots: Sign (emissive LED text band), Awn (shop awning).
"""
import math
import os

_src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_buildings.py")).read()
exec(compile(_src.split("bpy.ops.wm.read_factory_settings(use_empty=True)")[0], "gen_buildings_head", "exec"))   # Builder, finish, SLOTS ...
SLOTS += ["Sign", "Awn"]
UV_SCALE.update({"Sign": 1.0, "Awn": 1.0})
bpy.ops.wm.read_factory_settings(use_empty=True)
FRONT = Vector((0, -1, 0))


def shopfronts(b, cx, cy, W, D, shop_w=6.5, h=4.3):
    """Glazed shops with pilasters, awnings and a sign band across the front (-Y) facade."""
    y = cy - D / 2
    n = max(1, round(W / shop_w))
    sw = W / n
    for k in range(n):
        x = cx - W / 2 + sw * (k + 0.5)
        b.box((x, y - 0.14, 1.9), (sw - 0.9, 0.12, 3.1), "Glass")                              # shop window
        b.box((x, y - 0.2, 0.28), (sw - 0.9, 0.2, 0.55), "Metal")                             # stall riser
        b.box((x, y - 0.2, 3.5), (sw - 0.9, 0.2, 0.12), "Metal")                              # transom bar
        b.box((x - sw * 0.2, y - 0.2, 1.4), (0.07, 0.12, 2.6), "Metal")
        b.box((x, y - 0.75, 3.75), (sw - 0.7, 1.3, 0.1), "Awn")                               # awning
        b.box((x, y - 1.4, 3.55), (sw - 0.7, 0.08, 0.28), "Awn")                              # awning valance
        b.box((x, y - 0.17, 4.6), (sw - 1.0, 0.1, 0.8), "Sign")                               # sign band (emissive)
        if b.rng.random() < 0.35:                                                              # roll-down security shutter: ribbed slats and a housing box
            b.box((x, y - 0.30, 1.75), (sw - 0.9, 0.06, 3.0), "Metal")
            for j in range(15):
                b.box((x, y - 0.345, 0.3 + j * 0.2), (sw - 0.9, 0.03, 0.05), "Metal")
            b.box((x, y - 0.36, 3.42), (sw - 0.8, 0.28, 0.30), "Metal")
        else:                                                                                  # entrance door: frame, glass leaf, push bar and handle, number plate
            dx = x + sw * 0.30
            b.box((dx, y - 0.24, 1.2), (1.15, 0.10, 2.35), "Metal")                            # frame
            b.box((dx, y - 0.30, 1.15), (0.95, 0.04, 2.15), "Glass")                           # leaf
            b.box((dx - 0.32, y - 0.34, 1.05), (0.04, 0.06, 0.55), "Metal")                    # vertical pull handle
            b.box((dx, y - 0.33, 0.95), (0.80, 0.04, 0.04), "Metal")                           # push bar
            b.box((dx + 0.48, y - 0.30, 1.5), (0.14, 0.03, 0.10), "Trim")                      # street-number plate
            b.box((dx, y - 0.45, 2.55), (1.4, 0.55, 0.08), "Awn")                              # little door canopy

    for k in range(n + 1):
        x = cx - W / 2 + sw * k
        b.box((x, y - 0.15, h / 2), (0.55, 0.3, h), "Trim")                                   # pilasters
    b.box((cx, y - 0.18, h + 0.1), (W + 0.4, 0.45, 0.28), "Trim")                             # structural beam over the storefronts


def stoops(b, cx, cy, W, D, house_w):
    y = cy - D / 2
    n = max(1, round(W / house_w))
    hw = W / n
    for k in range(n):
        x = cx - W / 2 + hw * (k + 0.5)
        for s in range(4):
            b.box((x - hw * 0.22, y - 0.35 - (3 - s) * 0.32, 0.2 + s * 0.22), (1.5, 0.34 + 0.0, 0.4 + s * 0.44), "Trim")     # steps
        b.box((x - hw * 0.22, y - 0.05, 1.9 + 0.9), (1.2, 0.1, 2.2), "Wood")                                                # door
        b.box((x - hw * 0.22 + 0.8, y - 1.0, 2.0), (0.05, 1.5, 0.9), "Metal")                                               # railings
        b.box((x - hw * 0.22 - 0.8, y - 1.0, 2.0), (0.05, 1.5, 0.9), "Metal")
        b.box((x + hw * 0.22, y - 0.17, 0.9), (1.6, 0.14, 1.4), "Glass")                                                    # garden-level window


def fire_escape(b, cx, y, z0, floors, fh, w=2.4):
    """Zig-zag fire escape: a platform and rail per floor with a drop ladder (front facade at y)."""
    for f in range(1, floors):
        z = z0 + f * fh
        b.box((cx, y - 0.6, z), (w, 1.2, 0.07), "Metal")                                       # platform
        b.box((cx, y - 1.2, z + 0.5), (w, 0.04, 1.0), "Metal")                                 # rail
        for sx in (-1, 1):
            b.box((cx + sx * w / 2, y - 0.6, z + 0.5), (0.04, 1.2, 1.0), "Metal")
        if f < floors - 1:
            b.box((cx + w * 0.35, y - 0.9, z + fh / 2), (0.5, 0.05, fh), "Metal")              # ladder
    b.box((cx, y - 1.2, z0 + floors * fh * 0.5), (0.05, 0.05, floors * fh), "Metal")


def water_tower(b, x, y, z):
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((x + sx * 1.2, y + sy * 1.2, z + 1.2), (0.18, 0.18, 2.4), "Wood")
    b.box((x, y, z + 2.5), (3.4, 3.4, 0.2), "Wood")
    b.cylinder((x, y, z + 2.6), 1.8, 3.4, "Wood", 14)
    b.cylinder((x, y, z + 6.0), 1.9, 0.25, "Roof", 14)
    b.cylinder((x, y, z + 6.25), 0.9, 0.5, "Roof", 12)


def cornice(b, cx, cy, W, D, z):
    b.box((cx, cy - D / 2 - 0.35, z + 0.3), (W + 0.7, 0.8, 0.6), "Trim")
    b.box((cx, cy - D / 2 - 0.25, z - 0.25), (W + 0.4, 0.5, 0.5), "Trim")


# 1. Brownstone row: four 5.5 m houses, tall sash windows, stoops, deep cornice, brown brick
b = Builder("NYC_Brownstone", 31)
st = dict(win_w=0.46, win_h=0.62, reveal=0.22, dark=0.2, bands=[(3.2, 0.12, 0.2), (-0.3, 0.3, 0.4)])
b.walls(0, 0, 22, 16, 4, 3.6, 2.75, st)
b.flat_roof(0, 0, 22, 16, 4 * 3.6, plant=False, parapet=0.7)
cornice(b, 0, 0, 22, 16, 4 * 3.6)
stoops(b, 0, 0, 22, 16, 5.5)
water_tower(b, 5, 3, 4 * 3.6)
finish(b)

# 2. Walk-up tenement: six floors of red brick, fire escapes up the front, roof water tank
b = Builder("NYC_Walkup", 32)
st = dict(win_w=0.42, win_h=0.58, reveal=0.2, dark=0.25, bands=[(-0.3, 0.3, 0.45), (3.4, 0.1, 0.18)])
b.walls(0, 0, 18, 16, 6, 3.3, 3.0, st)
b.flat_roof(0, 0, 18, 16, 6 * 3.3, plant=True, parapet=0.8)
cornice(b, 0, 0, 18, 16, 6 * 3.3)
for x in (-4.5, 4.5):
    fire_escape(b, x, -8.2, 0.0, 6, 3.3)
shopfronts(b, 0, 0, 18, 16, shop_w=9.0, h=4.0)
water_tower(b, -3, 2, 6 * 3.3)
finish(b)

# 3. Mixed-use block: eight floors, three shops with awnings and signs, fire escape, roof tank
b = Builder("NYC_Mixed", 33)
st = dict(win_w=0.5, win_h=0.56, reveal=0.25, dark=0.2, bands=[(-0.3, 0.35, 0.5), (4.5, 0.1, 0.2)])
b.walls(0, 0, 26, 18, 8, 3.4, 3.25, st)
b.flat_roof(0, 0, 26, 18, 8 * 3.4, plant=True, parapet=0.9)
cornice(b, 0, 0, 26, 18, 8 * 3.4)
shopfronts(b, 0, 0, 26, 18, shop_w=8.6)
fire_escape(b, 9.5, -9.2, 0.0, 8, 3.4)
water_tower(b, 5, 3, 8 * 3.4)
finish(b)

# 4. Art-Deco setback tower: limestone base with shops, three setbacks, crown and spire, water tank on a terrace
b = Builder("NYC_Deco", 34)
fh = 3.8
st = dict(win_w=0.46, win_h=0.58, reveal=0.25, dark=0.22, bands=[(-0.35, 0.4, 0.5), (4.6, 0.1, 0.25)])
b.walls(0, 0, 30, 30, 12, fh, 3.0, st)
shopfronts(b, 0, 0, 30, 30, shop_w=7.5, h=4.4)
z = 12 * fh
b.flat_roof(0, 0, 30, 30, z, plant=False, parapet=0.7)
cornice(b, 0, 0, 30, 30, z)
for w, fl in ((22, 10), (15, 8), (9, 6)):
    b.walls(0, 0, w, w, fl, fh, 3.0, dict(st, bands=[(-0.3, 0.3, 0.4)]), z0=z)
    z += fl * fh
    b.flat_roof(0, 0, w, w, z, plant=False, parapet=0.6)
b.box((0, 0, z + 3), (5, 5, 6), "Trim")                                                        # crown
b.box((0, 0, z + 7), (3.2, 3.2, 2), "Trim")
b.cylinder((0, 0, z + 8), 0.35, 24, "Metal", 8)                                                # spire
finish(b)

# 5. Harbour warehouse: three storeys of brick, big roller doors on the front, loading dock canopy
b = Builder("NYC_Warehouse", 35)
st = dict(win_w=0.30, win_h=0.42, reveal=0.3, dark=0.35, bands=[(-0.3, 0.3, 0.5)])
b.walls(0, 0, 44, 26, 3, 5.0, 4.0, st)
b.flat_roof(0, 0, 44, 26, 15.0, plant=True, parapet=1.0)
for k in range(4):
    x = -16.5 + k * 11
    b.box((x, -13.1, 2.4), (6.0, 0.2, 4.4), "Metal")                                           # roller door
    for j in range(8):
        b.box((x, -13.22, 0.5 + j * 0.55), (6.0, 0.08, 0.06), "Metal")
b.box((0, -14.6, 5.2), (44, 3.0, 0.25), "Metal")                                               # dock canopy
for x in range(-20, 21, 10):
    b.box((x, -15.9, 2.6), (0.2, 0.2, 5.2), "Metal")
finish(b)
