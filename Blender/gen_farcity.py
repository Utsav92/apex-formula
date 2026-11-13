"""Far-field city masses for APEX Formula (original geometry): one mesh per city block (260 m x 80 m) holding two rows of buildings with varied heights, setbacks
and tower tiers. They fill everything beyond the detailed near field so no direction ever shows empty ground; the facade (window grid, lit rooms) is a shader.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_farcity.py
Out:  Blender/export/Far_<Cat><n>.fbx   Cat = Low (15-35 m), Mid (30-75 m), High (55-170 m), Fin (70-230 m); n = 0..2
UVs are in metres (vertical faces: along-wall x height; roofs: x,y) so the material can lay out windows at real size. One slot: Wall.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
BU, BV = 130.0, 40.0                     # block half-extents (m): 260 x 80
CATS = {"Low": (15, 35, 0.0), "Mid": (30, 75, 0.10), "High": (55, 170, 0.45), "Fin": (70, 230, 0.65)}

bpy.ops.wm.read_factory_settings(use_empty=True)


def build(name, seed, hmin, hmax, tower_p):
    rng = random.Random(seed)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")

    def box(x0, x1, y0, y1, z0, z1):
        v = [bm.verts.new(Vector(p)) for p in ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))]
        faces = ((0, 1, 5, 4, (0, -1, 0)), (1, 2, 6, 5, (1, 0, 0)), (2, 3, 7, 6, (0, 1, 0)), (3, 0, 4, 7, (-1, 0, 0)), (4, 5, 6, 7, (0, 0, 1)))
        for a, b, c, d, n in faces:
            try:
                f = bm.faces.new([v[a], v[b], v[c], v[d]])
            except ValueError:
                continue
            f.normal_update()
            if f.normal.dot(Vector(n)) < 0:
                f.normal_flip()
            for l in f.loops:
                co = l.vert.co
                if n[2] == 1:
                    l[uv].uv = (co.x, co.y)
                elif n[0] != 0:
                    l[uv].uv = (co.y, co.z)
                else:
                    l[uv].uv = (co.x, co.z)

    for row in (-1, 1):
        x = -BU + 1.0
        while x < BU - 14:
            w = rng.uniform(16, 42)
            w = min(w, BU - 1 - x)
            depth = rng.uniform(24, 34)
            y0, y1 = (row * BV - (0 if row > 0 else depth) + (-depth if row > 0 else 0), 0)  # placeholder, replaced below
            # row +1 sits against the +y street edge, row -1 against the -y edge; depth reaches inward
            if row > 0:
                y0, y1 = BV - depth, BV - 0.5
            else:
                y0, y1 = -BV + 0.5, -BV + depth
            h = rng.uniform(hmin, hmax)
            if rng.random() < tower_p:
                h *= rng.uniform(1.2, 1.7)
            # base mass, then up to two setback tiers for tall buildings
            tiers = [(0.0, h * 0.55, 0.0)]
            if h > 55:
                tiers.append((h * 0.55, h * 0.82, 0.14))
            if h > 110:
                tiers.append((h * 0.82, h, 0.28))
            if len(tiers) == 1:
                tiers = [(0.0, h, 0.0)]
            for z0, z1, inset in tiers:
                ix, iy = w * inset * 0.5, (y1 - y0) * inset * 0.5
                box(x + ix, x + w - ix, y0 + iy, y1 - iy, z0, z1)
            if rng.random() < 0.6:                              # rooftop plant box
                top = tiers[-1][1]
                ix = w * tiers[-1][2] * 0.5
                box(x + w * 0.3, x + w * 0.55, (y0 + y1) / 2 - 3, (y0 + y1) / 2 + 3, top, top + rng.uniform(2.5, 5))
            x += w + rng.uniform(0.0, 2.5)

    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0008)
    mesh = bpy.data.meshes.new(name)
    mesh.materials.append(bpy.data.materials.new("Wall"))
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = False
    o = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT"); o.select_set(True)
    bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Far_{name}.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                             apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
    print("EXPORTED Far_%s tris=%d" % (name, sum(len(p.vertices) - 2 for p in mesh.polygons)))
    bpy.data.objects.remove(o)


for cat, (hmin, hmax, tp) in CATS.items():
    for n in range(3):
        build(f"{cat}{n}", 1000 + 31 * n + len(cat), hmin, hmax, tp)
