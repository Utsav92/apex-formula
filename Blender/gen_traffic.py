"""Generic traffic car for APEX Formula (original geometry, no real model or marque): a 4.6 m sedan, long axis = X, front = +X, origin at ground centre.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_traffic.py
Out:  Blender/export/Traffic_Car.fbx     Slots: Body, Glass, Tyre, Light, Trim
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
SLOTS = ["Body", "Glass", "Tyre", "Light", "Trim"]
bpy.ops.wm.read_factory_settings(use_empty=True)
bm = bmesh.new()


def quad(pts, slot, outward):
    try:
        f = bm.faces.new([bm.verts.new(Vector(p)) for p in pts])
    except ValueError:
        return
    f.material_index = SLOTS.index(slot)
    f.normal_update()
    if f.normal.dot(Vector(outward)) < 0:
        f.normal_flip()


def hull(x0, x1, y0, y1, z0, z1, slot, top_in_x=(0.0, 0.0), top_in_y=0.0):
    """Box whose top face is inset: top_in_x = (back inset, front inset) along X, top_in_y along Y (windscreen / hood slopes, tumblehome)."""
    bx0, bx1 = x0 + top_in_x[0], x1 - top_in_x[1]
    by0, by1 = y0 + top_in_y, y1 - top_in_y
    B = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]
    T = [(bx0, by0, z1), (bx1, by0, z1), (bx1, by1, z1), (bx0, by1, z1)]
    c = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    quad(B, slot, (0, 0, -1))
    quad(T, slot, (0, 0, 1))
    for i in range(4):
        j = (i + 1) % 4
        mid = [(B[i][k] + B[j][k] + T[i][k] + T[j][k]) / 4 - c[k] for k in range(3)]
        quad([B[i], B[j], T[j], T[i]], slot, mid)


def wheel(cx, cy, r, w, slot="Tyre", seg=14):
    ring = [(cx + r * math.cos(2 * math.pi * i / seg), r + r * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
    for side, nx in ((-1, -1), (1, 1)):
        face = [bm.verts.new(Vector((x, cy + side * w / 2, z))) for x, z in ring]
        try:
            f = bm.faces.new(face)
        except ValueError:
            continue
        f.material_index = SLOTS.index(slot)
        f.normal_update()
        if f.normal.dot(Vector((0, nx, 0))) < 0:
            f.normal_flip()
    for i in range(seg):
        a, b = ring[i], ring[(i + 1) % seg]
        mid = ((a[0] + b[0]) / 2 - cx, 0, (a[1] + b[1]) / 2 - r)
        quad([(a[0], cy - w / 2, a[1]), (b[0], cy - w / 2, b[1]), (b[0], cy + w / 2, b[1]), (a[0], cy + w / 2, a[1])], slot, mid)


# body shell: lower body, hood and tail slopes, cabin with glass
hull(-2.3, 2.3, -0.92, 0.92, 0.32, 0.88, "Body", top_in_x=(0.15, 0.35), top_in_y=0.06)
hull(-1.15, 1.25, -0.80, 0.80, 0.88, 1.42, "Glass", top_in_x=(0.55, 0.85), top_in_y=0.10)                 # greenhouse (glass)
hull(-1.05, 1.1, -0.74, 0.74, 1.40, 1.46, "Body", top_in_x=(0.45, 0.65), top_in_y=0.10)                   # roof panel
for sy in (-1, 1):                                                                                          # B/C pillars as body strips
    hull(-0.05, 0.12, sy * 0.80 - 0.02, sy * 0.80 + 0.02, 0.88, 1.40, "Body")
    hull(-1.15, -0.95, sy * 0.80 - 0.02, sy * 0.80 + 0.02, 0.88, 1.35, "Body")
for sy in (-0.66, 0.66):                                                                                    # headlights / tail lights
    hull(2.27, 2.34, sy - 0.20, sy + 0.20, 0.62, 0.74, "Light")
    hull(-2.34, -2.27, sy - 0.20, sy + 0.20, 0.64, 0.74, "Light")
hull(-2.36, 2.36, -0.9, 0.9, 0.28, 0.40, "Trim")                                                           # bumpers / sills
for x in (-1.40, 1.45):
    for y in (-0.80, 0.80):
        wheel(x, y, 0.33, 0.24)

bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
uv = bm.loops.layers.uv.new("UVMap")
for f in bm.faces:
    for l in f.loops:
        l[uv].uv = (l.vert.co.x, l.vert.co.z)
mesh = bpy.data.meshes.new("Car")
for s in SLOTS:
    mesh.materials.append(bpy.data.materials.new(s))
bm.to_mesh(mesh)
bm.free()
for p in mesh.polygons:
    p.use_smooth = False
o = bpy.data.objects.new("Car", mesh)
bpy.context.collection.objects.link(o)
bpy.ops.object.select_all(action="DESELECT"); o.select_set(True)
bpy.context.view_layer.objects.active = o
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Traffic_Car.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                         apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
print("EXPORTED Traffic_Car tris=%d" % sum(len(p.vertices) - 2 for p in mesh.polygons))
