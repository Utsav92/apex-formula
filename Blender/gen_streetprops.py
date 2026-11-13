"""Street props for the New York canyon (original geometry): streetlight, LED sponsor board, fire hydrant.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_streetprops.py
Out:  Blender/export/Prop_Lamp.fbx, Prop_Led.fbx, Prop_Hydrant.fbx   (metres, origin at ground centre, front/track side = +Y in Blender = -Y in UE after import)
Slots: Metal, Led, Light, Paint, Base, Glass, Stripe (white road paint)
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
SLOTS = ["Metal", "Led", "Light", "Paint", "Base", "Glass", "Stripe", "Net", "Black"]


class B:
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

    def box(self, x0, x1, y0, y1, z0, z1, slot):
        q = self.quad
        q([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], slot, (0, 0, -1))
        q([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], slot, (0, 0, 1))
        q([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], slot, (0, -1, 0))
        q([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], slot, (0, 1, 0))
        q([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], slot, (-1, 0, 0))
        q([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], slot, (1, 0, 0))

    def cyl(self, cx, cy, z0, r0, r1, h, slot, seg=12):
        ring0 = [(cx + r0 * math.cos(2 * math.pi * i / seg), cy + r0 * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
        ring1 = [(cx + r1 * math.cos(2 * math.pi * i / seg), cy + r1 * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
        for i in range(seg):
            j = (i + 1) % seg
            mx, my = (ring0[i][0] + ring0[j][0]) / 2 - cx, (ring0[i][1] + ring0[j][1]) / 2 - cy
            self.quad([(*ring0[i], z0), (*ring0[j], z0), (*ring1[j], z0 + h), (*ring1[i], z0 + h)], slot, (mx, my, 0))
        for ring, z, nz in ((ring0, z0, -1), (ring1, z0 + h, 1)):
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
            n = f.normal if f.normal.length > 0 else Vector((0, 0, 1))
            ax = max(range(3), key=lambda i: abs(n[i]))
            for l in f.loops:
                c = l.vert.co
                u, v = (c.y, c.z) if ax == 0 else (c.x, c.z) if ax == 1 else (c.x, c.y)
                l[uv].uv = (u, v)
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
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f"Prop_{self.name}.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                                 apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
        print("EXPORTED Prop_%s.fbx tris=%d" % (self.name, sum(len(p.vertices) - 2 for p in mesh.polygons)))
        for ob in list(bpy.data.objects):
            bpy.data.objects.remove(ob)


bpy.ops.wm.read_factory_settings(use_empty=True)

# --- streetlight: 9 m tapered pole, base plinth, 3 m arm over the road (toward +Y), luminaire head ---------------------------------
b = B("Lamp")
b.cyl(0, 0, 0.0, 0.32, 0.26, 0.6, "Base")
b.cyl(0, 0, 0.6, 0.13, 0.08, 8.4, "Metal")
b.box(-0.05, 0.05, 0.0, 3.2, 8.75, 8.95, "Metal")                  # arm
b.box(-0.30, 0.30, 2.5, 3.9, 8.55, 8.75, "Metal")                  # head housing
b.box(-0.24, 0.24, 2.6, 3.8, 8.50, 8.55, "Light")                  # lens (emissive)
b.box(-0.08, 0.08, -0.12, 0.0, 2.2, 5.0, "Metal")                  # banner arm bracket on the pole
b.export()

# --- LED sponsor board: 8 m x 1.2 m screen on two posts, bezel, rear brace -----------------------------------------------------------
b = B("Led")
b.box(-4.1, 4.1, -0.12, 0.05, 1.35, 2.65, "Metal")               # frame (raised so the screen clears the 1.2 m barrier)
b.box(-4.0, 4.0, 0.05, 0.09, 1.45, 2.55, "Led")                   # screen face toward +Y
for x in (-3.4, 3.4):
    b.box(x - 0.08, x + 0.08, -0.3, -0.1, 0.0, 1.4, "Metal")      # posts
    b.box(x - 0.2, x + 0.2, -0.6, 0.0, 0.0, 0.06, "Base")
b.export()

# --- fire hydrant ---------------------------------------------------------------------------------------------------------------------
b = B("Hydrant")
b.cyl(0, 0, 0.0, 0.14, 0.12, 0.62, "Paint")
b.cyl(0, 0, 0.62, 0.15, 0.0, 0.12, "Paint")
b.box(-0.22, 0.22, -0.06, 0.06, 0.36, 0.46, "Paint")
b.box(-0.05, 0.05, -0.05, 0.10, 0.50, 0.60, "Metal")
b.export()

# --- traffic light: 6 m pole, 4.5 m mast arm over the pavement edge, three-lens signal head (emissive), pedestrian head on the pole ------
b = B("Signal")
b.cyl(0, 0, 0.0, 0.18, 0.15, 0.4, "Base")
b.cyl(0, 0, 0.4, 0.11, 0.08, 5.6, "Metal")
b.box(-0.05, 0.05, 0.0, 4.2, 5.5, 5.62, "Metal")                   # mast arm toward +Y (the track)
for k, y in enumerate((3.2, 4.0)):                                  # two signal heads on the arm
    b.box(-0.17, 0.17, y - 0.17, y + 0.17, 4.55, 5.5, "Metal")
    for j, z in enumerate((5.25, 5.02, 4.79)):
        b.box(-0.10, 0.10, y + 0.17, y + 0.20, z - 0.09, z + 0.09, "Light" if j == 0 else "Metal")
b.box(-0.2, 0.2, 0.11, 0.30, 2.5, 3.1, "Metal")                     # pedestrian head
b.box(-0.14, 0.14, 0.30, 0.33, 2.6, 3.0, "Light")
b.box(-0.2, 0.2, -0.3, -0.11, 1.3, 1.5, "Metal")                    # push-button box
b.export()

# --- bus shelter: 3.6 m x 1.6 m glazed enclosure with roof, rear panel, bench and an ad panel (lit) ---------------------------------
b = B("Shelter")
b.box(-1.9, 1.9, -0.9, 0.8, 2.55, 2.75, "Metal")                   # roof slab
for x in (-1.8, 1.8):
    for y in (-0.8, 0.7):
        b.box(x - 0.04, x + 0.04, y - 0.04, y + 0.04, 0.0, 2.55, "Metal")
b.box(-1.8, 1.8, -0.84, -0.78, 0.5, 2.5, "Glass")                  # rear glazing
b.box(-1.84, -1.76, -0.8, 0.7, 0.5, 2.5, "Glass")                  # side glazing
b.box(1.76, 1.84, -0.8, 0.7, 0.5, 2.5, "Glass")
b.box(-1.2, 1.2, -0.62, -0.18, 0.46, 0.52, "Metal")                # bench
b.box(-1.2, -1.14, -0.62, -0.18, 0.0, 0.46, "Metal"); b.box(1.14, 1.2, -0.62, -0.18, 0.0, 0.46, "Metal")
b.box(1.9, 2.5, -0.4, -0.3, 0.5, 2.3, "Metal")                     # ad-panel frame beside the shelter
b.box(1.95, 2.45, -0.3, -0.26, 0.6, 2.2, "Led")
b.box(-1.9, 1.9, -0.9, 0.8, 0.0, 0.04, "Base")
b.export()

# --- crosswalk: ten 0.5 m x 3 m zebra bars across an 11 m-wide strip, 2 cm high (laid on the paving at cross-street mouths) --------------
b = B("Crosswalk")
for i in range(10):
    x0 = -5.0 + i * 1.0
    b.box(x0, x0 + 0.5, -1.5, 1.5, 0.0, 0.02, "Stripe")
b.export()

# --- kerb: 4 m run of raised pavement edge with a chamfered top, 15 cm high ---------------------------------------------------------------------
b = B("Kerb")
b.box(-2.0, 2.0, -0.12, 0.0, 0.0, 0.16, "Base")
b.box(-2.0, 2.0, 0.0, 0.05, 0.0, 0.14, "Stripe")
b.export()

# --- neon screen tower: 7 m x 13 m video wall on a steel frame with two legs (Times Square-style), faces +Y -----------------------------
b = B("NeonScreen")
b.box(-3.7, 3.7, -0.35, 0.05, 3.0, 16.4, "Metal")
b.box(-3.5, 3.5, 0.05, 0.12, 3.3, 16.1, "Led")
for x in (-2.8, 2.8):
    b.box(x - 0.25, x + 0.25, -0.9, -0.1, 0.0, 3.0, "Metal")
b.box(-3.7, 3.7, -0.3, 0.0, 16.4, 16.7, "Light")
b.export()

# --- vertical blade sign: 1.8 m x 9 m double-sided neon blade on a pole -----------------------------------------------------------------
b = B("NeonBlade")
b.box(-0.12, 0.12, -0.12, 0.12, 0.0, 4.0, "Metal")
b.box(-0.9, 0.9, -0.14, 0.14, 4.0, 13.0, "Metal")
b.box(-0.8, 0.8, 0.14, 0.18, 4.2, 12.8, "Led")
b.box(-0.8, 0.8, -0.18, -0.14, 4.2, 12.8, "Led")
b.export()

# --- steam stack: 2.6 m tapering stack with two white bands and a collar (generic utility steam vent; no real-world marks) ----------------
b = B("SteamStack")
b.cyl(0, 0, 0.0, 0.34, 0.34, 0.12, "Base")
b.cyl(0, 0, 0.12, 0.26, 0.17, 1.9, "Paint")
b.cyl(0, 0, 0.7, 0.225, 0.2, 0.28, "Stripe")
b.cyl(0, 0, 1.25, 0.2, 0.18, 0.28, "Stripe")
b.cyl(0, 0, 2.02, 0.2, 0.2, 0.1, "Metal")
b.cyl(0, 0, 2.12, 0.12, 0.12, 0.5, "Paint")
b.export()

# --- scaffold: 6 m x 1.4 m bay, four lifts of 2.6 m, tube-and-board with green debris netting on the street face (front = +Y Blender) -----------------
b = B("Scaffold")
for x in (-3.0, 0.0, 3.0):
    for y in (0.0, 1.4):
        b.box(x - 0.025, x + 0.025, y - 0.025, y + 0.025, 0.0, 10.6, "Metal")                          # standards
for k in range(5):
    z = k * 2.6
    for y in (0.0, 1.4):
        b.box(-3.0, 3.0, y - 0.025, y + 0.025, z, z + 0.05, "Metal")                                   # ledgers
    b.box(-3.0, 3.0, 0.0, 1.4, z + 0.05, z + 0.09, "Base")                                              # boards
    for x in (-3.0, 0.0, 3.0):
        b.box(x - 0.02, x + 0.02, 0.0, 1.4, z, z + 0.04, "Metal")                                      # transoms
    if k:
        b.box(-3.0, 3.0, 1.38, 1.40, z + 0.1, z + 1.1, "Metal")                                        # guard rail
for k in range(4):
    b.box(-3.0, 3.0, 1.43, 1.45, k * 2.6 + 0.15, k * 2.6 + 2.5, "Net")                                  # debris netting
b.export()

# --- litter bin: 0.9 m steel drum with a domed lid -------------------------------------------------------------------------------------------------
b = B("Bin")
b.cyl(0, 0, 0.0, 0.26, 0.26, 0.85, "Metal")
b.cyl(0, 0, 0.85, 0.28, 0.2, 0.1, "Black")
b.export()

# --- trash bags: a pile of five black bags at the kerb ------------------------------------------------------------------------------------------------
b = B("Bags")
for (x, y, h, r) in ((0.0, 0.0, 0.7, 0.30), (0.5, 0.1, 0.6, 0.28), (-0.5, 0.05, 0.55, 0.27), (0.2, 0.55, 0.5, 0.26), (-0.25, -0.5, 0.55, 0.27)):
    b.cyl(x, y, 0.0, r, r * 0.55, h, "Black", 8)
b.export()

# --- bollard row piece: a 1 m steel bollard with a reflective band -----------------------------------------------------------------------------------
b = B("Bollard")
b.cyl(0, 0, 0.0, 0.1, 0.1, 1.0, "Metal", 10)
b.cyl(0, 0, 0.75, 0.105, 0.105, 0.12, "Stripe", 10)
b.export()
