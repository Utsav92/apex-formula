"""Trackside scenery for APEX Formula: trees, buildings, grandstand. All original, procedurally generated.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_scenery.py
Out:  Blender/export/{Tree_Pine,Tree_Oak,Bld_Glass,Bld_Block,Bld_Brick,Bld_Slim,Grandstand}.fbx
Units: metres; origin at ground level, centred in plan. Material slot names are mapped in Python/ue_import.py.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)


class Builder:
    def __init__(self, name, slots):
        self.name, self.slots = name, slots
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")

    def idx(self, slot):
        return self.slots.index(slot)

    def quad(self, pts, uvs, slot):
        f = self.bm.faces.new([self.bm.verts.new(p) for p in pts])
        f.material_index = self.idx(slot)
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv
        return f

    def prim(self, slot, verts, loc=(0, 0, 0)):
        bmesh.ops.translate(self.bm, vec=loc, verts=verts)
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = self.idx(slot)

    def cone(self, slot, loc, r1, r2, depth, seg=8):
        res = bmesh.ops.create_cone(self.bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=depth)
        self.prim(slot, res["verts"], loc)

    def blob(self, slot, loc, radius, rng, subdiv=2, squash=0.8):
        res = bmesh.ops.create_icosphere(self.bm, subdivisions=subdiv, radius=radius)
        for v in res["verts"]:
            v.co *= 1.0 + 0.22 * (rng.random() - 0.5)
            v.co.z *= squash
        self.prim(slot, res["verts"], loc)

    def box(self, slot, c, size, uv_scale=None):
        """Axis-aligned box centred at c. Side faces use facade UVs when uv_scale=(tile_w, tile_h) is given."""
        sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
        x0, x1, y0, y1, z0, z1 = c[0] - sx, c[0] + sx, c[1] - sy, c[1] + sy, c[2] - sz, c[2] + sz
        tw, th = uv_scale if uv_scale else (1.0, 1.0)
        sides = [  # outward-facing quads, counter-clockwise seen from outside
            ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], size[0]),   # -Y
            ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], size[1]),   # +X
            ([(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)], size[0]),   # +Y
            ([(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)], size[1]),   # -X
        ]
        for pts, width in sides:
            self.quad(pts, [(0, 0), (width / tw, 0), (width / tw, size[2] / th), (0, size[2] / th)], "Facade")
        self.quad([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], [(0, 0), (1, 0), (1, 1), (0, 1)], "Roof")

    def finish(self, smooth):
        mesh = bpy.data.meshes.new(self.name)
        for s in self.slots:
            m = bpy.data.materials.get(s) or bpy.data.materials.new(s)
            mesh.materials.append(m)
        self.bm.normal_update()
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for p in mesh.polygons:
            p.use_smooth = smooth
        # Diagnostic: share of faces whose normal points away from the mesh's vertical axis/centre.
        if self.name.startswith("Tree"):
            cz = sum(v.co.z for v in mesh.vertices) / len(mesh.vertices)
            out = sum(1 for p in mesh.polygons if p.normal.dot(p.center - Vector((0, 0, cz if p.center.z < 3 else p.center.z))) > 0)
            print("NORMALS %s outward=%d/%d" % (self.name, out, len(mesh.polygons)))
        obj = bpy.data.objects.new(self.name, mesh)
        bpy.context.collection.objects.link(obj)
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = os.path.join(OUT, self.name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=path, use_selection=True, axis_forward="X", axis_up="Z",
                                 apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
        print("EXPORTED %s polys=%d size=%s" % (self.name, len(mesh.polygons), [round(d, 1) for d in obj.dimensions]))
        bpy.data.objects.remove(obj)


bpy.ops.wm.read_factory_settings(use_empty=True)

# --- trees ---------------------------------------------------------------------------------------
rng = random.Random(11)
b = Builder("Tree_Pine", ["Bark", "Leaf"])
b.cone("Bark", (0, 0, 1.4), 0.32, 0.20, 2.8, 7)
for k, (z, r, h) in enumerate([(2.6, 3.0, 4.0), (4.8, 2.5, 3.8), (6.8, 2.0, 3.4), (8.6, 1.5, 3.0), (10.2, 0.95, 2.6)]):
    b.cone("Leaf", (0, 0, z + h / 2), r, 0.05, h, 9)
b.finish(smooth=True)

b = Builder("Tree_Oak", ["Bark", "Leaf"])
b.cone("Bark", (0, 0, 2.2), 0.45, 0.28, 4.4, 8)
for loc, r in [((0, 0, 7.0), 3.6), ((1.9, 0.6, 6.0), 2.7), ((-1.8, -0.5, 6.2), 2.8), ((0.4, 1.8, 7.6), 2.5), ((-0.5, -1.7, 7.8), 2.4)]:
    b.blob("Leaf", loc, r, rng, subdiv=2, squash=0.85)
b.finish(smooth=True)

# --- buildings -----------------------------------------------------------------------------------
FACADE_TILE = (12.8, 18.0)   # one facade texture tile in metres (4 windows wide x 5 floors tall)


def building(name, w, d, h, tiers=()):
    b = Builder(name, ["Facade", "Roof"])
    b.box("Facade", (0, 0, h / 2), (w, d, h), FACADE_TILE)
    z = h
    for (sw, sd, sh) in tiers:                       # setbacks stacked on top
        b.box("Facade", (0, 0, z + sh / 2), (sw, sd, sh), FACADE_TILE)
        z += sh
    r = random.Random(name)
    for _ in range(3):                               # rooftop plant boxes
        b.box("Roof", ((r.random() - 0.5) * w * 0.5, (r.random() - 0.5) * d * 0.5, z + 1.2),
              (3 + r.random() * 3, 3 + r.random() * 3, 2.4))
    b.finish(smooth=False)


building("Bld_Glass", 40, 40, 90, tiers=[(30, 30, 18), (18, 18, 12)])
building("Bld_Block", 60, 34, 38)
building("Bld_Brick", 34, 34, 54, tiers=[(26, 26, 8)])
building("Bld_Slim", 26, 26, 130, tiers=[(18, 18, 20)])

# --- grandstand ----------------------------------------------------------------------------------
LEN, STEPS, TREAD, RISE = 120.0, 14, 1.0, 0.75
b = Builder("Grandstand", ["Seats", "Concrete", "Steel", "Canopy"])
SEAT_TILE_U, SEAT_TILE_V = 32.0, 25.6
# local layout: +X along the stand, rows climb toward +Y (away from the track at -Y), origin at the front-bottom centre
for j in range(STEPS):
    y0, y1 = j * TREAD, (j + 1) * TREAD
    zt = (j + 1) * RISE
    z0 = j * RISE
    # tread (seats) facing up, riser facing the track
    b.quad([(-LEN / 2, y0, zt), (LEN / 2, y0, zt), (LEN / 2, y1, zt), (-LEN / 2, y1, zt)],
           [(0, j * TREAD / SEAT_TILE_V), (LEN / SEAT_TILE_U, j * TREAD / SEAT_TILE_V),
            (LEN / SEAT_TILE_U, (j + 1) * TREAD / SEAT_TILE_V), (0, (j + 1) * TREAD / SEAT_TILE_V)], "Seats")
    # From track level the risers are what you see, so they carry the coloured seat-back texture.
    v0 = (j * 0.037) % 1.0
    b.quad([(-LEN / 2, y0, z0), (LEN / 2, y0, z0), (LEN / 2, y0, zt), (-LEN / 2, y0, zt)],
           [(0, v0), (LEN / SEAT_TILE_U, v0), (LEN / SEAT_TILE_U, v0 + 0.03), (0, v0 + 0.03)], "Seats")
# end walls and back wall
H = STEPS * RISE
D = STEPS * TREAD
for sx in (-1, 1):
    pts = [(sx * LEN / 2, 0, 0), (sx * LEN / 2, D, 0), (sx * LEN / 2, D, H), (sx * LEN / 2, 0, RISE)]
    if sx == -1:
        pts = list(reversed(pts))
    b.quad(pts, [(0, 0), (1, 0), (1, 1), (0, 1)], "Concrete")
b.quad([(LEN / 2, D, 0), (-LEN / 2, D, 0), (-LEN / 2, D, H), (LEN / 2, D, H)], [(0, 0), (1, 0), (1, 1), (0, 1)], "Concrete")
# canopy roof and support columns
ROOF_Z = H + 5.5
b.quad([(-LEN / 2 - 2, -3, ROOF_Z), (LEN / 2 + 2, -3, ROOF_Z), (LEN / 2 + 2, D + 2, ROOF_Z + 3.5), (-LEN / 2 - 2, D + 2, ROOF_Z + 3.5)],
       [(0, 0), (1, 0), (1, 1), (0, 1)], "Canopy")
b.quad([(-LEN / 2 - 2, D + 2, ROOF_Z + 3.5), (LEN / 2 + 2, D + 2, ROOF_Z + 3.5), (LEN / 2 + 2, -3, ROOF_Z), (-LEN / 2 - 2, -3, ROOF_Z)],
       [(0, 0), (1, 0), (1, 1), (0, 1)], "Canopy")
for i in range(9):
    x = -LEN / 2 + i * LEN / 8
    b.cone("Steel", (x, -2.0, ROOF_Z / 2), 0.3, 0.3, ROOF_Z, 6)
b.finish(smooth=False)
