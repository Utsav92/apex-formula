"""Sponsor advertising meshes (original geometry). Every panel is a single quad with UVs 0..1 across the face, so a 4:1 texture on a 4:1 panel can never stretch.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_sponsor_boards.py
Out:  Blender/export/Ad_Barrier.fbx    4.8 x 1.2 m board on two posts, bottom at 1.35 m (clears a 1.2 m barrier)
      Blender/export/Ad_Large.fbx      9.6 x 2.4 m board on two braced posts
      Blender/export/Ad_Gantry.fbx     28 m steel overhead bridge, two 9.6 x 2.4 m panels facing each way, LED strips, bolted truss
      Blender/export/Ad_Billboard.fbx  12 x 3 m wall-mounted LED screen with brackets, rear plate and cable conduit (origin = wall face; front = +Y Blender)
Conventions as the other props: units metres, front = +Y in Blender = -Y in UE after import. Slots: Panel (front), PanelB (back, gantry), Steel, Back, Light.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
SLOTS = ["Panel", "PanelB", "Steel", "Back", "Light"]
bpy.ops.wm.read_factory_settings(use_empty=True)


class M:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.new("UVMap")

    def quad(self, pts, slot, outward, uvs=None):
        try:
            verts = [self.bm.verts.new(Vector(p)) for p in pts]
            f = self.bm.faces.new(verts)
        except ValueError:
            return
        f.material_index = SLOTS.index(slot)
        f.normal_update()
        flip = f.normal.dot(Vector(outward)) < 0
        if flip:
            f.normal_flip()
        if uvs:
            self.bm.verts.index_update()
            by_vert = {v.index: uv for v, uv in zip(verts, uvs)}      # keyed by vertex, because normal_flip reverses the loop order
            for l in f.loops:
                l[self.uv].uv = by_vert[l.vert.index]
        else:
            for l in f.loops:
                c = l.vert.co
                n = f.normal
                ax = max(range(3), key=lambda i: abs(n[i]))
                l[self.uv].uv = (c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y)

    def box(self, x0, x1, y0, y1, z0, z1, slot):
        q = self.quad
        q([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], slot, (0, 0, -1))
        q([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], slot, (0, 0, 1))
        q([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], slot, (0, -1, 0))
        q([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], slot, (0, 1, 0))
        q([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], slot, (-1, 0, 0))
        q([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], slot, (1, 0, 0))

    def beam(self, a, b, t, slot="Steel"):
        """Square-section strut from point a to b (thickness t), built from a rotated box."""
        a, b = Vector(a), Vector(b)
        d = b - a
        L = d.length
        if L < 1e-4:
            return
        z = d.normalized()
        up = Vector((0, 0, 1)) if abs(z.z) < 0.95 else Vector((1, 0, 0))
        x = z.cross(up).normalized(); y = z.cross(x).normalized()
        h = t / 2
        c = [a + x * sx * h + y * sy * h for sx in (-1, 1) for sy in (-1, 1)]
        e = [p + d for p in c]
        mid = (a + b) / 2
        for i, j in ((0, 1), (1, 3), (3, 2), (2, 0)):
            self.quad([c[i], c[j], e[j], e[i]], slot, (c[i] + c[j]) / 2 - a)
        self.quad([c[0], c[1], c[3], c[2]], slot, -z)
        self.quad([e[0], e[1], e[3], e[2]], slot, z)

    def panel(self, x0, x1, z0, z1, y, slot, facing=1):
        """Front (+Y, facing=1) or back (-Y, facing=-1) face. UV (0,0) bottom-left as seen from the viewer, (1,1) top-right."""
        if facing > 0:
            self.quad([(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], slot, (0, 1, 0), uvs=[(1, 0), (0, 0), (0, 1), (1, 1)])
        else:
            self.quad([(x1, y, z0), (x0, y, z0), (x0, y, z1), (x1, y, z1)], slot, (0, -1, 0), uvs=[(1, 0), (0, 0), (0, 1), (1, 1)])

    def export(self):
        bm = self.bm
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0004)
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
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Ad_{self.name}.fbx"), use_selection=True, axis_forward="X", axis_up="Z", apply_unit_scale=True,
                                 global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
        print("EXPORTED Ad_%s tris=%d dims=%s" % (self.name, sum(len(p.vertices) - 2 for p in mesh.polygons), [round(d, 2) for d in o.dimensions]))
        for ob in list(bpy.data.objects):
            bpy.data.objects.remove(ob)


def framed_board(m, w, h, z0, y, post_x, depth=0.12):
    """Panel with a thin steel frame around it and a dark back; returns nothing. Panel face at y, frame behind it."""
    fw = 0.08
    m.panel(-w / 2, w / 2, z0, z0 + h, y, "Panel", 1)
    m.box(-w / 2 - fw, w / 2 + fw, y - depth, y - 0.002, z0 - fw, z0, "Steel")                 # bottom rail
    m.box(-w / 2 - fw, w / 2 + fw, y - depth, y - 0.002, z0 + h, z0 + h + fw, "Steel")         # top rail
    m.box(-w / 2 - fw, -w / 2, y - depth, y - 0.002, z0, z0 + h, "Steel")
    m.box(w / 2, w / 2 + fw, y - depth, y - 0.002, z0, z0 + h, "Steel")
    m.box(-w / 2, w / 2, y - depth, y - depth + 0.002, z0, z0 + h, "Back")                      # rear skin
    m.quad([(w / 2, y - depth, z0), (-w / 2, y - depth, z0), (-w / 2, y - depth, z0 + h), (w / 2, y - depth, z0 + h)], "Back", (0, -1, 0))


# ---- barrier board: 4.8 x 1.2 m ---------------------------------------------------------------------------------------------------------------------
m = M("Barrier")
W, H, Z0 = 4.8, 1.2, 1.35
framed_board(m, W, H, Z0, 0.10, 0)
for px in (-W * 0.36, W * 0.36):
    m.box(px - 0.07, px + 0.07, -0.30, -0.10, 0.0, Z0, "Steel")                                 # posts (behind the board plane)
    m.box(px - 0.20, px + 0.20, -0.55, 0.0, 0.0, 0.06, "Steel")                                 # base plates
    m.box(px - 0.07, px + 0.07, -0.30, 0.0, Z0 - 0.12, Z0 - 0.02, "Steel")                      # cross bracket
    for bx in (-0.14, 0.14):                                                                    # bolts
        m.box(px + bx - 0.02, px + bx + 0.02, -0.52, -0.48, 0.06, 0.09, "Steel")
m.export()

# ---- large board: 9.6 x 2.4 m ----------------------------------------------------------------------------------------------------------------------
m = M("Large")
W, H, Z0 = 9.6, 2.4, 2.2
framed_board(m, W, H, Z0, 0.10, 0, 0.18)
for px in (-W * 0.38, W * 0.38):
    m.box(px - 0.12, px + 0.12, -0.55, -0.10, 0.0, Z0 + H * 0.6, "Steel")
    m.box(px - 0.35, px + 0.35, -1.0, 0.1, 0.0, 0.08, "Steel")
    m.beam((px, -0.95, 0.08), (px, -0.30, Z0 + H * 0.45), 0.10)                                 # diagonal brace
m.export()

# ---- gantry: 28 m overhead bridge ------------------------------------------------------------------------------------------------------------------
m = M("Gantry")
SPAN, TOP, PW, PH = 28.0, 10.2, 9.6, 2.4
tx = SPAN / 2
for sx in (-1, 1):                                                                              # two lattice towers, 10.2 m tall
    x = sx * tx
    for dx in (-0.45, 0.45):
        for dy in (-0.45, 0.45):
            m.beam((x + dx, dy, 0.0), (x + dx * 0.9, dy * 0.9, TOP), 0.22)                      # corner legs, slightly tapering
    for k in range(6):                                                                          # cross bracing between the legs
        z0, z1 = k * TOP / 6, (k + 1) * TOP / 6
        m.beam((x - 0.4, -0.4, z0), (x + 0.4, 0.4, z1), 0.10); m.beam((x + 0.4, -0.4, z0), (x - 0.4, 0.4, z1), 0.10)
        m.beam((x - 0.4, -0.4, z1), (x + 0.4, -0.4, z1), 0.10); m.beam((x - 0.4, 0.4, z1), (x + 0.4, 0.4, z1), 0.10)
    m.box(x - 0.9, x + 0.9, -0.9, 0.9, 0.0, 0.12, "Steel")                                      # base plate
    for bx in (-0.7, 0.7):
        for by in (-0.7, 0.7):
            m.box(x + bx - 0.05, x + bx + 0.05, by - 0.05, by + 0.05, 0.12, 0.2, "Steel")       # anchor bolts
for yy in (-0.5, 0.5):                                                                          # top truss: two chords with Warren diagonals
    m.beam((-tx, yy, TOP), (tx, yy, TOP), 0.28)
    m.beam((-tx, yy, TOP + 1.1), (tx, yy, TOP + 1.1), 0.22)
n = 28
for i in range(n):
    x0, x1 = -tx + SPAN * i / n, -tx + SPAN * (i + 1) / n
    m.beam((x0, 0, TOP), (x1, 0, TOP + 1.1) if i % 2 == 0 else (x1, 0, TOP + 1.1), 0.12)
    m.beam((x0, 0, TOP + 1.1), (x1, 0, TOP), 0.12)
    for yy in (-0.5, 0.5):
        m.box(x0 - 0.06, x0 + 0.06, yy - 0.06, yy + 0.06, TOP + 1.1, TOP + 1.18, "Steel")      # bolt heads on the upper chord
m.beam((-tx, -0.5, TOP), (-tx, 0.5, TOP), 0.2); m.beam((tx, -0.5, TOP), (tx, 0.5, TOP), 0.2)
# two hanging panels on each face (front +Y faces oncoming cars, back faces leaving cars), hung from the lower chord on brackets
for sgn, slot in ((1, "Panel"), (-1, "PanelB")):
    y = sgn * 0.72
    for cx in (-5.6, 5.6):
        m.panel(cx - PW / 2, cx + PW / 2, TOP - 0.2 - PH, TOP - 0.2, y, slot, sgn)
        m.box(cx - PW / 2 - 0.1, cx + PW / 2 + 0.1, min(y, 0.0) - 0.0 if sgn > 0 else y, max(y, 0.0) if sgn > 0 else 0.0, TOP - 0.3 - PH, TOP - 0.2 - PH, "Steel")
        for hx in (-PW * 0.4, 0, PW * 0.4):
            m.beam((cx + hx, 0.0, TOP - 0.05), (cx + hx, y * 0.98, TOP - 0.2), 0.10)             # hanger brackets
        m.box(cx - PW / 2, cx + PW / 2, y * 0.98 if sgn > 0 else y, y if sgn > 0 else y * 0.98, TOP - 0.2 - PH, TOP - 0.2, "Back")
        m.box(cx - PW / 2, cx + PW / 2, y + sgn * 0.03 - 0.04, y + sgn * 0.03 + 0.04, TOP - 0.3 - PH, TOP - 0.22 - PH, "Light")   # LED wash strip under each panel
m.export()

# ---- building billboard: 12 x 3 m LED screen on brackets (origin on the wall face) ----------------------------------------------------------------
m = M("Billboard")
W, H, Z0, D = 12.0, 3.0, 0.0, 0.9
m.panel(-W / 2, W / 2, Z0, Z0 + H, D, "Panel", 1)
fw = 0.14
m.box(-W / 2 - fw, W / 2 + fw, D - 0.35, D, Z0 - fw, Z0, "Steel"); m.box(-W / 2 - fw, W / 2 + fw, D - 0.35, D, Z0 + H, Z0 + H + fw, "Steel")
m.box(-W / 2 - fw, -W / 2, D - 0.35, D, Z0, Z0 + H, "Steel"); m.box(W / 2, W / 2 + fw, D - 0.35, D, Z0, Z0 + H, "Steel")
m.box(-W / 2, W / 2, D - 0.36, D - 0.34, Z0, Z0 + H, "Back")
for k in range(5):                                                                              # horizontal rails and brackets back to the wall
    x = -W / 2 + W * k / 4
    m.box(x - 0.15, x + 0.15, 0.0, D - 0.35, Z0 - 0.1, Z0 + 0.12, "Steel")                      # lower arm
    m.box(x - 0.15, x + 0.15, 0.0, D - 0.35, Z0 + H - 0.12, Z0 + H + 0.1, "Steel")              # upper arm
    m.beam((x, 0.05, Z0 - 0.05), (x, D - 0.4, Z0 + H * 0.5), 0.10)                              # diagonal strut
    for zz in (Z0 + 0.02, Z0 + H):
        m.box(x - 0.2, x + 0.2, -0.02, 0.06, zz - 0.18, zz + 0.18, "Steel")                      # wall plate
        for bx in (-0.12, 0.12):
            m.box(x + bx - 0.025, x + bx + 0.025, 0.06, 0.1, zz - 0.02, zz + 0.02, "Steel")      # anchor bolt heads
m.box(-W / 2 - 0.2, W / 2 + 0.2, 0.0, 0.12, Z0 + H + 0.35, Z0 + H + 0.65, "Steel")             # top cable tray along the wall
for k in range(14):                                                                              # conduit run: a pipe along the top with clips and a drop
    x0 = -W / 2 + W * k / 14
    m.beam((x0, 0.2, Z0 + H + 0.25), (x0 + W / 14, 0.2, Z0 + H + 0.25), 0.08)
    m.box(x0 - 0.03, x0 + 0.03, 0.12, 0.28, Z0 + H + 0.18, Z0 + H + 0.32, "Steel")
m.beam((W / 2 - 0.4, 0.2, Z0 + H + 0.25), (W / 2 - 0.4, 0.2, Z0 - 2.5), 0.08)                  # drop pipe down the side
m.box(W / 2 - 0.5, W / 2 - 0.1, 0.0, 0.34, Z0 + H * 0.35, Z0 + H * 0.35 + 0.6, "Steel")          # junction box
m.export()
