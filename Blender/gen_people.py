"""Low-poly people for APEX Formula (original geometry): spectators (standing, cheering) used for crowds, pit crews and marshals.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_people.py
Out:  Blender/export/Person_Stand.fbx, Person_Cheer.fbx   (1.75 m, origin between the feet, front = +Y in Blender = -Y in UE after import)
Slots: Skin, Shirt, Pants, Hair. The game animates them in the vertex shader (bob / jump), so the meshes are static.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
SLOTS = ["Skin", "Shirt", "Pants", "Hair"]


class P:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()

    def quad(self, pts, slot, outward):
        try:
            f = self.bm.faces.new([self.bm.verts.new(Vector(p)) for p in pts])
        except ValueError:
            return
        f.material_index = SLOTS.index(slot)
        f.normal_update()
        if f.normal.dot(Vector(outward)) < 0:
            f.normal_flip()

    def box(self, cx, cy, z0, sx, sy, h, slot, lean=0.0):
        """Box centred at (cx, cy), base at z0; lean shears the top in x (for angled arms)."""
        x0, x1, y0, y1, z1 = cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2, z0 + h
        l = lean
        q = self.quad
        q([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], slot, (0, 0, -1))
        q([(x0 + l, y0, z1), (x1 + l, y0, z1), (x1 + l, y1, z1), (x0 + l, y1, z1)], slot, (0, 0, 1))
        q([(x0, y0, z0), (x1, y0, z0), (x1 + l, y0, z1), (x0 + l, y0, z1)], slot, (0, -1, 0))
        q([(x0, y1, z0), (x1, y1, z0), (x1 + l, y1, z1), (x0 + l, y1, z1)], slot, (0, 1, 0))
        q([(x0, y0, z0), (x0, y1, z0), (x0 + l, y1, z1), (x0 + l, y0, z1)], slot, (-1, 0, 0))
        q([(x1, y0, z0), (x1, y1, z0), (x1 + l, y1, z1), (x1 + l, y0, z1)], slot, (1, 0, 0))

    def cyl(self, cx, cy, z0, r, h, slot, seg=8):
        ring = [(cx + r * math.cos(2 * math.pi * i / seg), cy + r * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
        for i in range(seg):
            a, b = ring[i], ring[(i + 1) % seg]
            self.quad([(a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z0 + h), (a[0], a[1], z0 + h)], slot, ((a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy, 0))
        for z, nz in ((z0, -1), (z0 + h, 1)):
            try:
                f = self.bm.faces.new([self.bm.verts.new(Vector((x, y, z))) for x, y in ring])
            except ValueError:
                continue
            f.material_index = SLOTS.index(slot)
            f.normal_update()
            if f.normal.dot(Vector((0, 0, nz))) < 0:
                f.normal_flip()

    def export(self):
        bm = self.bm
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
        uv = bm.loops.layers.uv.new("UVMap")
        for f in bm.faces:
            for l in f.loops:
                l[uv].uv = (l.vert.co.x, l.vert.co.z)
        mesh = bpy.data.meshes.new(self.name)
        for s in SLOTS:
            mesh.materials.append(bpy.data.materials.new(s))
        bm.to_mesh(mesh); bm.free()
        for p in mesh.polygons:
            p.use_smooth = False
        o = bpy.data.objects.new(self.name, mesh)
        bpy.context.collection.objects.link(o)
        bpy.ops.object.select_all(action="DESELECT"); o.select_set(True)
        bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Person_{self.name}.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                                 apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
        print("EXPORTED Person_%s tris=%d" % (self.name, sum(len(p.vertices) - 2 for p in mesh.polygons)))
        for ob in list(bpy.data.objects):
            bpy.data.objects.remove(ob)


def person(name, arms_up):
    p = P(name)
    for x in (-0.10, 0.10):                                   # legs and shoes
        p.box(x, 0.0, 0.07, 0.15, 0.17, 0.84, "Pants")
        p.box(x, 0.04, 0.0, 0.16, 0.30, 0.08, "Hair")
    p.box(0, 0, 0.88, 0.40, 0.23, 0.62, "Shirt")              # torso
    p.cyl(0, 0, 1.50, 0.055, 0.07, "Skin")                    # neck
    p.cyl(0, 0.01, 1.57, 0.105, 0.19, "Skin", 10)             # head
    p.cyl(0, -0.005, 1.70, 0.11, 0.07, "Hair", 10)            # hair
    if arms_up:
        for s in (-1, 1):
            p.box(s * 0.30, 0.0, 1.20, 0.09, 0.10, 0.42, "Shirt", lean=s * 0.10)       # upper arms angled out and up
            p.box(s * 0.40, 0.0, 1.62, 0.08, 0.09, 0.30, "Skin", lean=s * 0.06)        # forearms/hands raised
    else:
        for s in (-1, 1):
            p.box(s * 0.255, 0.0, 0.92, 0.09, 0.11, 0.46, "Shirt")
            p.box(s * 0.255, 0.0, 0.62, 0.08, 0.10, 0.32, "Skin")
    p.export()


bpy.ops.wm.read_factory_settings(use_empty=True)
person("Stand", False)
person("Cheer", True)
