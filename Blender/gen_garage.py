"""Pit garage for APEX Formula: ten team bays on the north side of the start straight (original geometry).

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_garage.py
Out:  Blender/export/PitGarage.fbx

Layout (design metres; y = distance from the circuit on the pit side, which is UE -Y. The FBX import flips Y, so Blender +Y lands on UE -Y):
  x = -182.5 .. +40.5, front wall y = 24, back wall y = 36. Ten 22 m bays, centres x = -170 + 22 k, matching AFormulaTrack's pit boxes.
  Ground floor: open bays (19 m wide, 4.2 m high) with a team-coloured header, partitions, tool cabinets, tyre stacks, light strips.
  First floor: hospitality behind a glazed facade, a cantilevered canopy/balcony over the working lane, flat roof with plant,
  and a race-control tower over bays 4-5.
Material slots: Wall, Concrete, Glass, Metal, Roof, Floor, Rubber, Light, Team0..Team9.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)
MIRROR_Y = False   # design Y is the distance from the circuit on the pit side; Blender +Y becomes UE -Y on FBX import

SLOTS = ["Wall", "Concrete", "Glass", "Metal", "Roof", "Floor", "Rubber", "Light"] + [f"Team{i}" for i in range(10)]
UV_SCALE = {"Wall": 3.0, "Concrete": 2.0, "Roof": 2.0, "Metal": 2.0, "Floor": 4.0}

bpy.ops.wm.read_factory_settings(use_empty=True)
bm = bmesh.new()
rng = random.Random(20261113)


def V(x, y, z):
    return bm.verts.new(Vector((x, -y if MIRROR_Y else y, z)))


def quad(pts, slot, outward):
    """pts: four UE-space points; outward: UE-space direction the face must look toward."""
    try:
        f = bm.faces.new([V(*p) for p in pts])
    except ValueError:
        return
    f.material_index = SLOTS.index(slot)
    f.normal_update()
    o = Vector((outward[0], -outward[1] if MIRROR_Y else outward[1], outward[2]))
    if f.normal.dot(o) < 0:
        f.normal_flip()


def box(x0, x1, y0, y1, z0, z1, slot):
    if x1 <= x0 or y1 <= y0 or z1 <= z0:
        return
    quad([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], slot, (0, 0, -1))
    quad([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], slot, (0, 0, 1))
    quad([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], slot, (0, -1, 0))
    quad([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], slot, (0, 1, 0))
    quad([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], slot, (-1, 0, 0))
    quad([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], slot, (1, 0, 0))


def cylinder(cx, cy, z0, r, h, slot, seg=10):
    ring = [(cx + r * math.cos(2 * math.pi * i / seg), cy + r * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
    for i in range(seg):
        a, b = ring[i], ring[(i + 1) % seg]
        mx, my = (a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy
        quad([(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z0 + h), (a[0], a[1], z0 + h)], slot, (mx, my, 0))
    for z, nz in ((z0, -1), (z0 + h, 1)):
        try:
            f = bm.faces.new([V(x, y, z) for x, y in ring])
        except ValueError:
            continue
        f.material_index = SLOTS.index(slot)
        f.normal_update()
        if f.normal.dot(Vector((0, 0, nz))) < 0:
            f.normal_flip()


# ---- dimensions (m) -----------------------------------------------------------------------------------------------------
PIER_W = 3.0
X0, X1 = -182.5, 40.5
FY, BY = 24.0, 36.0
H1, H2 = 5.2, 9.2
PIERS = [-181.0 + 22.0 * k for k in range(11)]
BAYS = [-170.0 + 22.0 * k for k in range(10)]

# shell
box(X0, X1, FY, BY, -0.05, 0.10, "Floor")
box(X0, X1, BY - 0.4, BY, 0.10, H2, "Floor")           # "Floor" = the light, self-lit interior surface (back/end walls, partitions)
box(X0, X0 + 0.4, FY, BY, 0.10, H2, "Floor")
box(X1 - 0.4, X1, FY, BY, 0.10, H2, "Floor")
for xp in PIERS:
    box(xp - PIER_W / 2, xp + PIER_W / 2, FY, BY, 0.10, H1 + 0.5, "Floor")
    box(xp - PIER_W / 2 - 0.05, xp + PIER_W / 2 + 0.05, FY - 0.15, FY, 0.10, H1 + 0.5, "Concrete")   # pier front plate

# canopy / first-floor slab, projecting 2.4 m over the working lane
box(X0, X1, FY - 2.4, BY, H1, H1 + 0.5, "Concrete")
# balcony balustrade: glass with a metal handrail and posts
box(X0, X1, FY - 2.36, FY - 2.30, H1 + 0.5, H1 + 1.55, "Glass")
box(X0, X1, FY - 2.42, FY - 2.24, H1 + 1.55, H1 + 1.65, "Metal")
x = X0
while x < X1:
    box(x - 0.04, x + 0.04, FY - 2.40, FY - 2.26, H1 + 0.5, H1 + 1.6, "Metal")
    x += 4.0
# first-floor curtain wall (glass between mullions) and spandrel band
box(X0, X1, FY + 0.05, FY + 0.15, H1 + 0.6, H2 - 0.55, "Glass")
x = X0
while x <= X1 + 0.01:
    box(x - 0.06, x + 0.06, FY - 0.05, FY + 0.30, H1 + 0.5, H2, "Metal")
    x += 22.0 / 6.0
box(X0, X1, FY - 0.05, FY + 0.40, H2 - 0.55, H2, "Wall")
# roof with parapet and plant
box(X0, X1, FY - 0.6, BY + 0.2, H2, H2 + 0.35, "Roof")
box(X0, X1, FY - 0.6, FY - 0.3, H2 + 0.35, H2 + 1.10, "Concrete")
box(X0, X1, BY, BY + 0.2, H2 + 0.35, H2 + 1.10, "Concrete")
for _ in range(26):
    w, d, h = rng.uniform(1.6, 3.6), rng.uniform(1.0, 2.2), rng.uniform(0.9, 1.9)
    cx, cy = rng.uniform(X0 + 4, X1 - 4), rng.uniform(FY + 5, BY - 3)
    box(cx - w / 2, cx + w / 2, cy - d / 2, cy + d / 2, H2 + 0.35, H2 + 0.35 + h, "Metal")

# race-control tower over the middle of the building (bays 4 and 5), three storeys with a wrap-around glass band
TX0, TX1 = -86.0, -56.0
box(TX0, TX1, FY + 4.0, BY - 1.0, H2 + 0.35, H2 + 8.6, "Wall")
for zlo, zhi in ((H2 + 1.1, H2 + 3.2), (H2 + 4.0, H2 + 6.1)):
    box(TX0 - 0.15, TX1 + 0.15, FY + 3.85, FY + 4.0, zlo, zhi, "Glass")
    x = TX0
    while x <= TX1 + 0.01:
        box(x - 0.05, x + 0.05, FY + 3.75, FY + 4.05, zlo - 0.1, zhi + 0.1, "Metal")
        x += 3.0
box(TX0 - 1.2, TX1 + 1.2, FY + 2.6, BY - 0.5, H2 + 8.6, H2 + 9.0, "Roof")
box(TX0 - 1.2, TX1 + 1.2, FY + 2.6, FY + 2.75, H2 + 7.2, H2 + 8.6, "Concrete")      # fascia
box(TX0 + 8, TX0 + 8.15, FY + 4.0, FY + 4.15, H2 + 9.0, H2 + 14.0, "Metal")           # antenna mast

# ---- the ten bays ---------------------------------------------------------------------------------------------------------
for k, xc in enumerate(BAYS):
    team = f"Team{k}"
    box(xc - 9.5, xc + 9.5, FY - 0.20, FY + 0.30, 4.2, H1 + 0.5, team)                  # header in the team colour
    box(xc - 9.5, xc + 9.5, FY - 0.30, FY - 0.20, 4.1, 4.2, "Metal")                    # shutter rail
    box(xc - 10.5, xc + 10.5, BY - 0.5, BY - 0.4, 0.6, 3.6, team)                       # interior accent stripe on the back wall
    for j in range(4):                                                                  # tool cabinets
        bx = xc - 8.0 + j * 2.3
        box(bx, bx + 1.9, BY - 1.7, BY - 0.5, 0.10, 1.45, "Metal")
        box(bx - 0.03, bx + 1.93, BY - 1.75, BY - 0.45, 1.45, 1.52, "Concrete")
    for j in range(3):                                                                  # tyre stacks
        for t in range(4):
            cylinder(xc + 5.2 + j * 1.0, BY - 4.0, 0.10 + t * 0.30, 0.34, 0.28, "Rubber")
    box(xc - 3.2, xc + 3.2, BY - 5.4, BY - 4.0, 0.10, 0.95, "Metal")                    # mobile tooling carts
    for yl in (27.0, 31.0, 34.5):                                                       # ceiling light strips
        box(xc - 8.0, xc + 8.0, yl, yl + 0.30, H1 - 0.06, H1, "Light")

# ---- finish: UVs (world-scale box mapping), export --------------------------------------------------------------------------
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0008)
uv = bm.loops.layers.uv.new("UVMap")
for f in bm.faces:
    slot = SLOTS[f.material_index]
    sc = UV_SCALE.get(slot, 1.0)
    n = f.normal if f.normal.length > 0 else Vector((0, 0, 1))
    ax = max(range(3), key=lambda i: abs(n[i]))
    for loop in f.loops:
        co = loop.vert.co
        u, v = (co.y, co.z) if ax == 0 else (co.x, co.z) if ax == 1 else (co.x, co.y)
        loop[uv].uv = (u / sc, v / sc)
mesh = bpy.data.meshes.new("PitGarage")
for s in SLOTS:
    mesh.materials.append(bpy.data.materials.new(s))
bm.to_mesh(mesh)
bm.free()
for p in mesh.polygons:
    p.use_smooth = False
obj = bpy.data.objects.new("PitGarage", mesh)
bpy.context.collection.objects.link(obj)
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "PitGarage.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                         apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
print("EXPORTED PitGarage.fbx tris=%d slots=%d" % (tris, len(SLOTS)))
