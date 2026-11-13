"""Modular, detailed buildings for APEX Formula (original geometry; textures are scanned CC0, see ASSET_MANIFEST).

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_buildings.py
Out:  Blender/export/Bld_<Name>.fbx for Hotel_Med, Apt_BrickA, Apt_BrickB, Office_Concrete, Tower_GlassA, Tower_GlassB, Lodge_Alpine

Every building is real geometry: bays with recessed windows (reveals, frames, glazing bars, sills), balconies with railings,
shutters, cornices and string courses, tiled hip/gable roofs or parapets with rooftop plant, curtain-wall towers with mullions
and setbacks. Material slots: Wall, Trim, Glass, GlassB, GlassC, Roof, Metal, Wood. Units: metres, origin at ground centre.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)

SLOTS = ["Wall", "Trim", "Glass", "GlassB", "GlassC", "Roof", "Metal", "Wood"]
UV_SCALE = {"Wall": 3.0, "Trim": 2.0, "Roof": 2.0, "Metal": 2.0, "Wood": 2.0, "Glass": 1.0, "GlassB": 1.0, "GlassC": 1.0}
Z = Vector((0, 0, 1))


class Builder:
    def __init__(self, name, seed):
        self.name = name
        self.rng = random.Random(seed)
        self.bm = bmesh.new()
        self.faces = 0

    # -- primitives -------------------------------------------------------------------------------------------
    def quad(self, pts, slot, normal=None):
        v = [Vector(p) for p in pts]
        if normal is not None:
            n = (v[1] - v[0]).cross(v[3] - v[0])
            if n.dot(normal) < 0:
                v = v[::-1]
        try:
            f = self.bm.faces.new([self.bm.verts.new(p) for p in v])
        except ValueError:
            return
        f.material_index = SLOTS.index(slot)
        self.faces += 1

    def tri(self, pts, slot, normal=None):
        v = [Vector(p) for p in pts]
        if normal is not None and (v[1] - v[0]).cross(v[2] - v[0]).dot(normal) < 0:
            v = v[::-1]
        try:
            f = self.bm.faces.new([self.bm.verts.new(p) for p in v])
        except ValueError:
            return
        f.material_index = SLOTS.index(slot)
        self.faces += 1

    def box(self, c, size, slot, yaw=0.0):
        """Closed box centred at c (x,y,z), size (sx,sy,sz), optionally rotated about Z."""
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        cy, sy = math.cos(yaw), math.sin(yaw)

        def P(x, y, z):
            return Vector((c[0] + x * cy - y * sy, c[1] + x * sy + y * cy, c[2] + z))
        a = [P(-hx, -hy, -hz), P(hx, -hy, -hz), P(hx, hy, -hz), P(-hx, hy, -hz),
             P(-hx, -hy, hz), P(hx, -hy, hz), P(hx, hy, hz), P(-hx, hy, hz)]
        ctr = Vector(c)
        for idx in ((0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            pts = [a[i] for i in idx]
            mid = sum(pts, Vector()) / 4
            self.quad(pts, slot, normal=mid - ctr)

    def cylinder(self, c, radius, height, slot, seg=12):
        ring_b = [Vector((c[0] + radius * math.cos(2 * math.pi * i / seg), c[1] + radius * math.sin(2 * math.pi * i / seg), c[2])) for i in range(seg)]
        ring_t = [p + Vector((0, 0, height)) for p in ring_b]
        for i in range(seg):
            j = (i + 1) % seg
            mid = (ring_b[i] + ring_b[j]) / 2 - Vector((c[0], c[1], c[2]))
            mid.z = 0
            self.quad([ring_b[i], ring_b[j], ring_t[j], ring_t[i]], slot, normal=mid)
        self.bm.faces.new([self.bm.verts.new(p) for p in ring_t]).material_index = SLOTS.index(slot)

    # -- facade machinery ---------------------------------------------------------------------------------------
    def glass_slot(self, dark=0.15):
        r = self.rng.random()
        return "GlassC" if r < dark else ("GlassB" if r < dark + 0.30 else "Glass")

    def window_cell(self, o, u, n, bw, fh, floor, style, balcony):
        """One bay of one floor: wall around a recessed window, frame, sill, optional shutters/balcony."""
        ww = bw * style.get("win_w", 0.50)
        wh = fh * style.get("win_h", 0.52)
        sill = fh * 0.28
        if balcony:
            wh, sill = fh * 0.70, 0.12          # French door
        mx = (bw - ww) / 2
        d = style.get("reveal", 0.28)

        def at(x, z, dz=0.0):
            return o + u * x + Z * z - n * dz

        # wall around the opening (4 quads)
        self.quad([at(0, 0), at(bw, 0), at(bw, sill), at(0, sill)], "Wall", n)                        # below
        self.quad([at(0, sill + wh), at(bw, sill + wh), at(bw, fh), at(0, fh)], "Wall", n)            # above
        self.quad([at(0, sill), at(mx, sill), at(mx, sill + wh), at(0, sill + wh)], "Wall", n)        # left
        self.quad([at(mx + ww, sill), at(bw, sill), at(bw, sill + wh), at(mx + ww, sill + wh)], "Wall", n)  # right
        # reveals (face into the opening)
        self.quad([at(mx, sill), at(mx, sill, d), at(mx, sill + wh, d), at(mx, sill + wh)], "Wall", u)                # left reveal faces +u
        self.quad([at(mx + ww, sill), at(mx + ww, sill, d), at(mx + ww, sill + wh, d), at(mx + ww, sill + wh)], "Wall", -u)
        self.quad([at(mx, sill + wh), at(mx + ww, sill + wh), at(mx + ww, sill + wh, d), at(mx, sill + wh, d)], "Wall", -Z)  # head
        self.quad([at(mx, sill), at(mx + ww, sill), at(mx + ww, sill, d), at(mx, sill, d)], "Wall", Z)                       # sill board
        # glazing, set back
        self.quad([at(mx, sill, d), at(mx + ww, sill, d), at(mx + ww, sill + wh, d), at(mx, sill + wh, d)], self.glass_slot(style.get("dark", 0.12)), n)
        # frame + glazing bars (thin Metal boxes just in front of the glass)
        t = 0.05
        for (x, z, sx, sz) in ((mx + ww / 2, sill + wh / 2, t, wh), (mx + ww / 2, sill + wh * 0.62, ww, t),
                               (mx + t / 2, sill + wh / 2, t, wh), (mx + ww - t / 2, sill + wh / 2, t, wh),
                               (mx + ww / 2, sill + t / 2, ww, t), (mx + ww / 2, sill + wh - t / 2, ww, t)):
            c = o + u * x + Z * z - n * (d - 0.04)
            size = (abs(u.x) * sx + abs(n.x) * 0.06, abs(u.y) * sx + abs(n.y) * 0.06, sz)
            self.box(c, size, "Metal")
        # sill stone
        c = o + u * (mx + ww / 2) + Z * (sill - 0.04) + n * 0.07
        self.box(c, (abs(u.x) * (ww + 0.3) + abs(n.x) * 0.2, abs(u.y) * (ww + 0.3) + abs(n.y) * 0.2, 0.08), "Trim")
        # shutters (Mediterranean)
        if style.get("shutters") and not balcony and self.rng.random() < 0.8:
            sw = ww * 0.46
            for side in (-1, 1):
                x = mx + ww / 2 + side * (ww / 2 + sw / 2 + 0.03)
                c = o + u * x + Z * (sill + wh / 2) + n * 0.04
                self.box(c, (abs(u.x) * sw + abs(n.x) * 0.05, abs(u.y) * sw + abs(n.y) * 0.05, wh), "Wood")
        # balcony slab + railing
        if balcony:
            bd, bwid = 1.3, bw * 0.84
            cx = o + u * (bw / 2) + n * (bd / 2)
            self.box(cx + Z * 0.075, (abs(u.x) * bwid + abs(n.x) * bd, abs(u.y) * bwid + abs(n.y) * bd, 0.15), "Trim")
            rail_z = 0.15 + 0.52
            top = o + u * (bw / 2) + n * (bd - 0.03) + Z * (0.15 + 1.0)
            self.box(top, (abs(u.x) * bwid + abs(n.x) * 0.05, abs(u.y) * bwid + abs(n.y) * 0.05, 0.05), "Metal")
            posts = max(int(bwid / 0.32), 3)
            for k in range(posts + 1):
                x = bw / 2 - bwid / 2 + bwid * k / posts
                self.box(o + u * x + n * (bd - 0.03) + Z * (0.15 + 0.5), (0.03, 0.03, 1.0), "Metal")
            for side in (-1, 1):                                                   # side rails
                self.box(o + u * (bw / 2 + side * bwid / 2) + n * (bd / 2) + Z * (0.15 + 1.0),
                         (abs(n.x) * bd + abs(u.x) * 0.05, abs(n.y) * bd + abs(u.y) * 0.05, 0.05), "Metal")

    def storefront_cell(self, o, u, n, bw, fh, style):
        """Ground floor: tall glazed shop/lobby bay with frame and a canopy."""
        d = 0.3
        gh, gz = fh * 0.82, 0.12
        mx = 0.35

        def at(x, z, dz=0.0):
            return o + u * x + Z * z - n * dz
        self.quad([at(0, 0), at(bw, 0), at(bw, gz), at(0, gz)], "Wall", n)
        self.quad([at(0, gz + gh), at(bw, gz + gh), at(bw, fh), at(0, fh)], "Wall", n)
        self.quad([at(0, gz), at(mx, gz), at(mx, gz + gh), at(0, gz + gh)], "Wall", n)
        self.quad([at(bw - mx, gz), at(bw, gz), at(bw, gz + gh), at(bw - mx, gz + gh)], "Wall", n)
        self.quad([at(mx, gz), at(mx, gz, d), at(mx, gz + gh, d), at(mx, gz + gh)], "Wall", u)
        self.quad([at(bw - mx, gz), at(bw - mx, gz, d), at(bw - mx, gz + gh, d), at(bw - mx, gz + gh)], "Wall", -u)
        self.quad([at(mx, gz + gh), at(bw - mx, gz + gh), at(bw - mx, gz + gh, d), at(mx, gz + gh, d)], "Wall", -Z)
        self.quad([at(mx, gz, d), at(bw - mx, gz, d), at(bw - mx, gz + gh, d), at(mx, gz + gh, d)], self.glass_slot(0.05), n)
        for x in (bw / 2, mx + 0.03, bw - mx - 0.03):
            c = o + u * x + Z * (gz + gh / 2) - n * (d - 0.04)
            self.box(c, (abs(u.x) * 0.07 + abs(n.x) * 0.08, abs(u.y) * 0.07 + abs(n.y) * 0.08, gh), "Metal")
        c = o + u * (bw / 2) + Z * (gz + gh + 0.15) + n * 0.45                       # canopy
        self.box(c, (abs(u.x) * (bw - 0.5) + abs(n.x) * 0.95, abs(u.y) * (bw - 0.5) + abs(n.y) * 0.95, 0.1), "Metal")

    def facade(self, base, u, n, width, floors, floor_h, bay, style):
        nb = max(1, round(width / bay))
        bw = width / nb
        for f in range(floors):
            for b in range(nb):
                o = base + u * (b * bw) + Z * (f * floor_h)
                if f == 0 and style.get("storefront"):
                    self.storefront_cell(o, u, n, bw, floor_h, style)
                else:
                    balcony = bool(style.get("balconies")) and f > 0 and b % 2 == 1 and f % 2 == 1
                    self.window_cell(o, u, n, bw, floor_h, f, style, balcony)
        # cornice/string course bands (Trim) across the whole wall
        for z, depth, h in style.get("bands", []):
            zz = z if z >= 0 else floors * floor_h + z
            c = base + u * (width / 2) + Z * zz + n * (depth / 2)
            self.box(c, (abs(u.x) * (width + depth) + abs(n.x) * depth, abs(u.y) * (width + depth) + abs(n.y) * depth, h), "Trim")

    def curtain_wall(self, base, u, n, width, floors, floor_h, pane=2.0):
        """Glass tower wall: dark spandrel per floor, glass panes between slim mullions, set 0.12 m back."""
        npn = max(1, round(width / pane))
        pw = width / npn
        sp = 0.85
        d = 0.12
        for f in range(floors):
            z0 = f * floor_h
            o = base + Z * z0
            self.quad([o, o + u * width, o + u * width + Z * sp, o + Z * sp], "Metal", n)                      # spandrel band
            for k in range(npn):
                x0 = k * pw
                p0, p1 = o + u * x0 + Z * sp - n * d, o + u * (x0 + pw) + Z * sp - n * d
                self.quad([p0, p1, p1 + Z * (floor_h - sp), p0 + Z * (floor_h - sp)], self.glass_slot(0.18), n)
                c = o + u * x0 + Z * (sp + (floor_h - sp) / 2) - n * (d - 0.07)
                self.box(c, (abs(u.x) * 0.08 + abs(n.x) * 0.16, abs(u.y) * 0.08 + abs(n.y) * 0.16, floor_h - sp), "Metal")   # mullion
            # side returns of the recessed glass are hidden behind the corner columns
        c = base + u * (width / 2) + Z * (floors * floor_h)
        self.box(c + n * 0.1, (abs(u.x) * (width + 0.2) + abs(n.x) * 0.35, abs(u.y) * (width + 0.2) + abs(n.y) * 0.35, 0.5), "Trim")   # cap

    # -- masses, roofs --------------------------------------------------------------------------------------------
    def walls(self, cx, cy, W, D, floors, fh, bay, style, curtain=False, z0=0.0):
        """Four facades of a W x D block. Outward normals: -Y, +X, +Y, -X; u runs to the viewer's right."""
        hw, hd = W / 2, D / 2
        specs = [(Vector((cx - hw, cy - hd, z0)), Vector((1, 0, 0)), Vector((0, -1, 0)), W),
                 (Vector((cx + hw, cy - hd, z0)), Vector((0, 1, 0)), Vector((1, 0, 0)), D),
                 (Vector((cx + hw, cy + hd, z0)), Vector((-1, 0, 0)), Vector((0, 1, 0)), W),
                 (Vector((cx - hw, cy + hd, z0)), Vector((0, -1, 0)), Vector((-1, 0, 0)), D)]
        for base, u, n, width in specs:
            if curtain:
                self.curtain_wall(base, u, n, width, floors, fh)
            else:
                self.facade(base, u, n, width, floors, fh, bay, style)
        # corner pilasters (Trim) to hide the seams where facades meet
        for sx in (-1, 1):
            for sy in (-1, 1):
                self.box((cx + sx * hw, cy + sy * hd, z0 + floors * fh / 2), (0.34, 0.34, floors * fh), "Trim" if not curtain else "Metal")

    def flat_roof(self, cx, cy, W, D, z, plant=True, parapet=0.9):
        hw, hd = W / 2, D / 2
        self.quad([(cx - hw, cy - hd, z), (cx + hw, cy - hd, z), (cx + hw, cy + hd, z), (cx - hw, cy + hd, z)], "Trim", Z)
        t = 0.3
        for (px, py, sx, sy) in ((cx, cy - hd + t / 2, W, t), (cx, cy + hd - t / 2, W, t), (cx - hw + t / 2, cy, t, D), (cx + hw - t / 2, cy, t, D)):
            self.box((px, py, z + parapet / 2), (sx, sy, parapet), "Trim")
        if plant:
            r = self.rng
            for _ in range(r.randint(5, 11)):                          # air-conditioning units
                x, y = cx + r.uniform(-hw + 2, hw - 2), cy + r.uniform(-hd + 2, hd - 2)
                self.box((x, y, z + 0.6), (r.uniform(1.2, 2.0), r.uniform(0.8, 1.2), 1.2), "Metal", yaw=r.choice((0, math.pi / 2)))
                self.cylinder((x, y, z + 1.2), 0.4, 0.12, "Metal", 10)   # fan
            self.box((cx + r.uniform(-hw / 3, hw / 3), cy + r.uniform(-hd / 3, hd / 3), z + 1.5), (3.2, 3.2, 3.0), "Wall")  # stair core
            self.cylinder((cx + hw / 3, cy - hd / 3, z), 1.3, 2.6, "Metal", 14)                                           # water tank

    def hip_roof(self, cx, cy, W, D, z, pitch=26, over=0.7, chimneys=True):
        hw, hd = W / 2 + over, D / 2 + over
        rise = (min(hw, hd)) * math.tan(math.radians(pitch))
        rl = max(hw - hd, 0.0)                                       # ridge half length
        e = [Vector((cx - hw, cy - hd, z)), Vector((cx + hw, cy - hd, z)), Vector((cx + hw, cy + hd, z)), Vector((cx - hw, cy + hd, z))]
        r0, r1 = Vector((cx - rl, cy, z + rise)), Vector((cx + rl, cy, z + rise))
        self.quad([e[0], e[1], r1, r0], "Roof", Vector((0, -1, 1)))
        self.quad([e[2], e[3], r0, r1], "Roof", Vector((0, 1, 1)))
        self.tri([e[1], e[2], r1], "Roof", Vector((1, 0, 1)))
        self.tri([e[3], e[0], r0], "Roof", Vector((-1, 0, 1)))
        self.box((cx, cy, z - 0.15), (W + 2 * over - 0.1, D + 2 * over - 0.1, 0.3), "Trim")        # eaves board
        if chimneys:
            for sx in (-0.3, 0.3):
                self.box((cx + sx * W, cy - 0.15 * D, z + rise * 0.6), (0.9, 0.9, rise + 1.6), "Wall")
                self.box((cx + sx * W, cy - 0.15 * D, z + rise + 1.0), (1.1, 1.1, 0.15), "Trim")

    def gable_roof(self, cx, cy, W, D, z, pitch=36, over=1.1):
        hw, hd = W / 2 + over, D / 2 + over
        rise = (D / 2 + over) * math.tan(math.radians(pitch))
        a0, a1 = Vector((cx - hw, cy - hd, z)), Vector((cx + hw, cy - hd, z))
        b0, b1 = Vector((cx - hw, cy + hd, z)), Vector((cx + hw, cy + hd, z))
        r0, r1 = Vector((cx - hw, cy, z + rise)), Vector((cx + hw, cy, z + rise))
        self.quad([a0, a1, r1, r0], "Roof", Vector((0, -1, 1)))
        self.quad([b1, b0, r0, r1], "Roof", Vector((0, 1, 1)))
        gz = z + 0.01
        for sx in (-1, 1):                                            # timber gable ends
            x = cx + sx * (W / 2)
            self.tri([Vector((x, cy - D / 2, gz)), Vector((x, cy + D / 2, gz)), Vector((x, cy, z + (D / 2) * math.tan(math.radians(pitch))))], "Wood", Vector((sx, 0, 0)))


# ---- building recipes -----------------------------------------------------------------------------------------

def finish(b):
    bm = b.bm
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0008)
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        slot = SLOTS[f.material_index]
        sc = UV_SCALE[slot]
        n = f.normal if f.normal.length > 0 else Vector((0, 0, 1))
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            co = loop.vert.co
            u, v = (co.y, co.z) if ax == 0 else (co.x, co.z) if ax == 1 else (co.x, co.y)
            loop[uv].uv = (u / sc, v / sc)
    mesh = bpy.data.meshes.new(b.name)
    for s in SLOTS:
        m = bpy.data.materials.get(s) or bpy.data.materials.new(s)
        mesh.materials.append(m)
    bm.to_mesh(mesh)
    bm.free()
    for p in mesh.polygons:
        p.use_smooth = False
    obj = bpy.data.objects.new(b.name, mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(OUT, f"Bld_{b.name}.fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, axis_forward="X", axis_up="Z", apply_unit_scale=True,
                             global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
    tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
    print("EXPORTED Bld_%s tris=%d size=%s" % (b.name, tris, [round(d, 1) for d in obj.dimensions]))
    bpy.data.objects.remove(obj)


bpy.ops.wm.read_factory_settings(use_empty=True)

# Mediterranean hotel: plaster, shutters, balconies, shops below, tiled hip roof
b = Builder("Hotel_Med", 1)
st = dict(shutters=True, balconies=True, storefront=True, win_w=0.46, win_h=0.55, reveal=0.3, dark=0.1, bands=[(3.2, 0.12, 0.22), (-0.35, 0.45, 0.45)])
b.walls(0, 0, 36, 16, 6, 3.3, 3.6, st)
b.hip_roof(0, 0, 36, 16, 6 * 3.3 + 0.0)
finish(b)

# Brick apartment block, 8 floors, balconies, flat roof with plant
b = Builder("Apt_BrickA", 2)
st = dict(balconies=True, win_w=0.52, win_h=0.5, reveal=0.25, dark=0.2, bands=[(-0.3, 0.3, 0.4), (3.0, 0.08, 0.16)])
b.walls(0, 0, 30, 14, 8, 3.0, 3.3, st)
b.flat_roof(0, 0, 30, 14, 8 * 3.0)
finish(b)

b = Builder("Apt_BrickB", 3)
st = dict(balconies=True, win_w=0.5, win_h=0.52, reveal=0.25, dark=0.15, bands=[(-0.3, 0.3, 0.4)])
b.walls(0, 0, 24, 14, 5, 3.0, 3.0, st)
b.flat_roof(0, 0, 24, 14, 5 * 3.0)
finish(b)

# Concrete office with wide ribbon windows
b = Builder("Office_Concrete", 4)
st = dict(win_w=0.80, win_h=0.46, reveal=0.4, dark=0.25, storefront=True, bands=[(-0.4, 0.5, 0.5)])
b.walls(0, 0, 40, 22, 10, 3.7, 5.0, st)
b.flat_roof(0, 0, 40, 22, 10 * 3.7, parapet=1.1)
finish(b)

# Glass towers with setbacks and crowns
for name, seed, W, floors, set_f in (("Tower_GlassA", 5, 34, 28, (18, 24)), ("Tower_GlassB", 6, 26, 18, (12,))):
    b = Builder(name, seed)
    fh = 3.9
    b.walls(0, 0, W, W, set_f[0], fh, 2, {}, curtain=True, z0=0.0)
    top_w, z, prev = W, set_f[0] * fh, set_f[0]
    for nxt in list(set_f[1:]) + [floors]:
        b.flat_roof(0, 0, top_w, top_w, z, plant=False, parapet=0.6)                       # terrace at the setback
        top_w -= 8
        cnt = nxt - prev
        b.walls(0, 0, top_w, top_w, cnt, fh, 2, {}, curtain=True, z0=z)                    # next block stands on the terrace
        prev, z = nxt, z + cnt * fh
    b.flat_roof(0, 0, top_w, top_w, z, plant=True, parapet=0.8)
    b.box((0, 0, z + 5), (top_w * 0.5, top_w * 0.5, 10), "Metal")                       # crown / mechanical penthouse
    b.cylinder((0, 0, z + 10), 0.25, 14, "Metal", 8)                                    # mast
    finish(b)

# Alpine lodge: timber lower storeys, plaster upper, deep-eaved gable roof, wooden balconies
b = Builder("Lodge_Alpine", 7)
st = dict(shutters=True, balconies=True, win_w=0.42, win_h=0.5, reveal=0.3, dark=0.1, bands=[(3.0, 0.1, 0.2)])
b.walls(0, 0, 22, 14, 3, 3.2, 3.6, st)
b.gable_roof(0, 0, 22, 14, 3 * 3.2)
finish(b)
