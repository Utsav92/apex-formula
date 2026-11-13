"""Original open-wheel racing car for APEX Formula (not based on any real team's car or branding).

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_car.py
Out:  Blender/export/F1_Body.fbx  (body, origin at car centre on the ground, +X forward)
      Blender/export/F1_Wheel.fbx (one wheel, axis along Y, origin at hub centre)

Units: metres. Material slot names are mapped to real materials by Python/ue_import.py:
  body : Paint, Accent, Carbon, Black, Metal, Glass, Light, DecalNum, DecalApex
  wheel: Tyre, Rim, Metal
Livery is geometry-driven (faces are re-assigned to Accent/Carbon slots by position), decals are
shrink-wrapped quads with their own UVs, Carbon faces get a box-projected UV for the weave texture.
"""
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)

# ---- scene + materials -----------------------------------------------------------------------

bpy.ops.wm.read_factory_settings(use_empty=True)

COLOURS = {
    "Paint": (0.75, 0.02, 0.03, 1), "Accent": (0.95, 0.95, 0.95, 1), "Carbon": (0.015, 0.015, 0.017, 1),
    "Black": (0.03, 0.03, 0.035, 1), "Metal": (0.55, 0.56, 0.58, 1), "Glass": (0.02, 0.03, 0.05, 1),
    "Light": (1.0, 0.05, 0.02, 1), "Tyre": (0.02, 0.02, 0.022, 1), "Rim": (0.35, 0.36, 0.38, 1),
    "DecalNum": (1, 1, 1, 1), "DecalApex": (1, 1, 1, 1),
}
MATS = {}
for _name, _col in COLOURS.items():
    m = bpy.data.materials.new(_name)
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = _col
    MATS[_name] = m


def slot(obj, name):
    obj.data.materials.clear()
    obj.data.materials.append(MATS[name])


def finish(obj, mat, smooth=True, subsurf=0):
    slot(obj, mat)
    if subsurf:
        mod = obj.modifiers.new("sub", "SUBSURF")
        mod.levels = subsurf
        mod.render_levels = subsurf
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True
    return obj


def new_obj(name, bm):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def select_only(objs, active=None):
    for o in bpy.data.objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = active or objs[0]


# ---- primitives ------------------------------------------------------------------------------

def loft(name, sections, y_off=0.0, ring=32):
    """Lofted body along X. section = (x, half_width, z_bottom, z_top, squareness)."""
    bm = bmesh.new()
    rings = []
    for (x, hw, zb, zt, p) in sections:
        zc, hh = (zb + zt) / 2, (zt - zb) / 2
        verts = []
        for i in range(ring):
            t = 2 * math.pi * i / ring
            c, s = math.cos(t), math.sin(t)
            y = y_off + hw * math.copysign(abs(c) ** (2 / p), c)
            z = zc + hh * math.copysign(abs(s) ** (2 / p), s)
            verts.append(bm.verts.new((x, y, z)))
        rings.append(verts)
    for a, b in zip(rings[:-1], rings[1:]):
        for i in range(ring):
            j = (i + 1) % ring
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(rings[0])
    bm.faces.new(list(reversed(rings[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm)


def box(name, center, size, rot=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= size[0]
        v.co.y *= size[1]
        v.co.z *= size[2]
    obj = new_obj(name, bm)
    obj.rotation_euler = rot
    obj.location = center
    return obj


def tube(name, a, b, radius, seg=8):
    a, b = Vector(a), Vector(b)
    d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=radius, radius2=radius, depth=d.length)
    obj = new_obj(name, bm)
    obj.location = (a + b) / 2
    obj.rotation_euler = Vector((0, 0, 1)).rotation_difference(d.normalized()).to_euler()
    return obj


def curve_tube(name, points, radius):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 3
    sp = cu.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for p, co in zip(sp.points, points):
        p.co = (*co, 1)
    obj = bpy.data.objects.new(name, cu)
    bpy.context.collection.objects.link(obj)
    select_only([obj])
    bpy.ops.object.convert(target="MESH")
    return bpy.context.view_layer.objects.active


body = []


def add(obj, mat, smooth=True, subsurf=0):
    body.append(finish(obj, mat, smooth, subsurf))
    return obj


R = math.radians

# ---- body ------------------------------------------------------------------------------------

add(loft("Monocoque", [
    (2.45, 0.05, 0.16, 0.24, 2.0), (2.25, 0.09, 0.15, 0.27, 2.1), (2.00, 0.13, 0.14, 0.30, 2.3),
    (1.75, 0.18, 0.13, 0.35, 2.35), (1.50, 0.23, 0.12, 0.40, 2.4), (1.20, 0.27, 0.11, 0.46, 2.45),
    (0.90, 0.29, 0.10, 0.52, 2.5), (0.55, 0.30, 0.095, 0.54, 2.55), (0.20, 0.31, 0.09, 0.55, 2.6),
    (-0.20, 0.30, 0.09, 0.58, 2.6), (-0.60, 0.29, 0.09, 0.60, 2.6), (-1.00, 0.26, 0.095, 0.62, 2.5),
    (-1.40, 0.23, 0.10, 0.62, 2.4), (-1.80, 0.19, 0.12, 0.57, 2.3), (-2.20, 0.14, 0.14, 0.50, 2.2),
    (-2.45, 0.10, 0.17, 0.44, 2.1), (-2.65, 0.07, 0.20, 0.38, 2.0)]), "Paint")

for side in (1, -1):
    add(loft(f"Sidepod{side}", [
        (0.85, 0.15, 0.12, 0.50, 2.4), (0.55, 0.18, 0.11, 0.49, 2.45), (0.20, 0.20, 0.10, 0.48, 2.5),
        (-0.25, 0.20, 0.10, 0.44, 2.5), (-0.60, 0.19, 0.10, 0.40, 2.5), (-1.00, 0.17, 0.10, 0.35, 2.45),
        (-1.40, 0.13, 0.10, 0.30, 2.4), (-1.85, 0.05, 0.11, 0.22, 2.0)], y_off=side * 0.50), "Paint")
    add(box(f"Inlet{side}", (0.86, side * 0.50, 0.31), (0.03, 0.26, 0.30)), "Black", smooth=False)
    # cooling louvres on the sidepod top, bargeboard vanes ahead of the sidepod
    for k in range(5):
        add(box(f"Louvre{side}{k}", (-0.95 - k * 0.11, side * 0.50, 0.345 - k * 0.012), (0.045, 0.20, 0.006)), "Black", smooth=False)
    for k, (bx, bh) in enumerate(((1.02, 0.26), (0.92, 0.22), (0.82, 0.18))):
        add(box(f"Barge{side}{k}", (bx, side * (0.42 + k * 0.03), 0.07 + bh / 2), (0.30, 0.006, bh), rot=(0, 0, side * R(8))), "Carbon", smooth=False)

# engine airbox + intake + shark fin, cockpit surround, exhaust
add(loft("Airbox", [(0.00, 0.09, 0.52, 0.66, 2.2), (-0.40, 0.14, 0.50, 0.82, 2.4), (-0.95, 0.12, 0.50, 0.74, 2.4),
                    (-1.50, 0.06, 0.52, 0.64, 2.2)]), "Paint")
add(box("Intake", (0.01, 0, 0.74), (0.03, 0.17, 0.22)), "Black", smooth=False)
add(loft("Fin", [(-0.90, 0.007, 0.60, 0.78, 2.0), (-1.50, 0.007, 0.58, 0.72, 2.0), (-2.10, 0.007, 0.52, 0.60, 2.0),
                 (-2.45, 0.006, 0.46, 0.50, 2.0)], ring=8), "Paint", smooth=False)
add(box("CockpitRim", (0.30, 0, 0.555), (0.85, 0.50, 0.025)), "Carbon", smooth=False)
add(tube("Exhaust", (-2.35, 0, 0.36), (-2.78, 0, 0.36), 0.04, 16), "Metal")
add(tube("ExhaustLip", (-2.76, 0, 0.36), (-2.80, 0, 0.36), 0.046, 16), "Black")

# floor, diffuser, splitter
add(box("Floor", (-0.55, 0, 0.06), (3.65, 1.62, 0.03)), "Carbon", smooth=False)
add(box("Splitter", (1.35, 0, 0.055), (0.60, 1.20, 0.025)), "Carbon", smooth=False)
add(box("Diffuser", (-2.40, 0, 0.17), (0.65, 1.0, 0.03), rot=(0, R(-14), 0)), "Carbon", smooth=False)
for i in range(-2, 3):
    add(box(f"DiffFin{i}", (-2.40, i * 0.17, 0.17), (0.65, 0.012, 0.16), rot=(0, R(-14), 0)), "Carbon", smooth=False)

# front wing: 4 elements, endplates, pylons, cascade winglets
add(box("FW1", (2.58, 0, 0.085), (0.34, 2.00, 0.016)), "Carbon", smooth=False)
add(box("FW2", (2.47, 0, 0.140), (0.26, 1.90, 0.014), rot=(0, R(-6), 0)), "Carbon", smooth=False)
add(box("FW3", (2.38, 0, 0.190), (0.20, 1.80, 0.012), rot=(0, R(-10), 0)), "Accent", smooth=False)
add(box("FW4", (2.30, 0, 0.235), (0.14, 1.64, 0.010), rot=(0, R(-16), 0)), "Accent", smooth=False)
for side in (1, -1):
    add(box(f"FWEnd{side}", (2.52, side * 1.00, 0.15), (0.62, 0.014, 0.26)), "Accent", smooth=False)
    add(box(f"FWPylon{side}", (2.30, side * 0.09, 0.14), (0.05, 0.02, 0.13), rot=(0, R(8), 0)), "Carbon", smooth=False)
    add(box(f"FWCascade{side}", (2.36, side * 0.80, 0.27), (0.18, 0.28, 0.008), rot=(side * R(10), R(-12), 0)), "Carbon", smooth=False)

# rear wing: main plane + flap (with DRS gap), endplates, pillar, beam wing
add(box("RW1", (-2.52, 0, 0.92), (0.38, 1.10, 0.016), rot=(0, R(8), 0)), "Carbon", smooth=False)
add(box("RW2", (-2.58, 0, 1.03), (0.26, 1.10, 0.014), rot=(0, R(18), 0)), "Accent", smooth=False)
add(box("RWGap", (-2.47, 0, 0.99), (0.02, 1.08, 0.012), rot=(0, R(18), 0)), "Black", smooth=False)
add(box("RWBeam", (-2.45, 0, 0.55), (0.30, 0.80, 0.012)), "Carbon", smooth=False)
for side in (1, -1):
    add(box(f"RWEnd{side}", (-2.55, side * 0.55, 0.78), (0.60, 0.014, 0.50)), "Accent", smooth=False)
add(box("RWPillar", (-2.48, 0, 0.72), (0.12, 0.05, 0.40)), "Carbon", smooth=False)
add(box("RainLight", (-2.69, 0, 0.30), (0.03, 0.10, 0.06)), "Light", smooth=False)

# halo (hoop + front pylon), mirrors, helmet, steering wheel
add(curve_tube("HaloHoop", [(0.10, 0.27, 0.58), (0.35, 0.26, 0.80), (0.82, 0.05, 0.80), (0.82, -0.05, 0.80),
                            (0.35, -0.26, 0.80), (0.10, -0.27, 0.58)], 0.013), "Carbon")
add(tube("HaloPylon", (0.84, 0, 0.80), (0.96, 0, 0.54), 0.014), "Carbon")
for side in (1, -1):
    add(tube(f"MirrorStalk{side}", (0.72, side * 0.30, 0.55), (0.74, side * 0.45, 0.60), 0.008), "Carbon")
    add(box(f"Mirror{side}", (0.74, side * 0.48, 0.61), (0.07, 0.14, 0.07)), "Black", smooth=True)
add(loft("Helmet", [(0.40, 0.07, 0.60, 0.70, 2.0), (0.28, 0.12, 0.58, 0.78, 2.0), (0.14, 0.12, 0.58, 0.78, 2.0),
                    (0.05, 0.08, 0.60, 0.72, 2.0)]), "Accent")
add(box("Visor", (0.36, 0, 0.705), (0.10, 0.20, 0.05)), "Glass")
add(box("SteerWheel", (0.74, 0, 0.47), (0.04, 0.26, 0.10), rot=(0, R(-30), 0)), "Black", smooth=False)

# suspension: double wishbones front and rear, push rods, brake duct fairings
for axle_x, hub_y in ((1.70, 0.74), (-1.70, 0.74)):
    for side in (1, -1):
        for dx, z0, z1 in ((0.16, 0.20, 0.30), (-0.16, 0.18, 0.18)):
            add(tube(f"Arm{axle_x}{side}{dx}", (axle_x + dx * 0.4, side * 0.26, z0), (axle_x, side * hub_y, z1), 0.012), "Metal")
            add(tube(f"ArmB{axle_x}{side}{dx}", (axle_x + dx * 0.4, side * 0.26, z0), (axle_x + dx * 0.1, side * hub_y, z1), 0.010), "Metal")
        add(tube(f"Push{axle_x}{side}", (axle_x, side * hub_y, 0.34), (axle_x + 0.05, side * 0.20, 0.46), 0.012), "Metal")
    for side in (1, -1):
        add(box(f"Duct{axle_x}{side}", (axle_x, side * (hub_y - 0.10), 0.24), (0.22, 0.08, 0.20)), "Carbon", smooth=False)

# ---- join, livery, UVs -----------------------------------------------------------------------

select_only(body, body[0])
bpy.ops.object.join()
car = bpy.context.view_layer.objects.active
car.name = "F1_Body"
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

me = car.data
slot_index = {m.name: i for i, m in enumerate(me.materials)}
# Make sure every slot we may reassign to exists.
for needed in ("Accent", "Carbon", "DecalNum", "DecalApex"):
    if needed not in slot_index:
        me.materials.append(MATS[needed])
        slot_index[needed] = len(me.materials) - 1

# Livery: white centre stripe along the nose/engine cover, dark carbon lower sidepod band.
stripe = band = 0
for p in me.polygons:
    if me.materials[p.material_index].name != "Paint":
        continue
    c, n = p.center, p.normal
    if c.z > 0.30 and abs(c.y) < 0.075 and n.z > 0.25 and c.x > -2.35:
        p.material_index = slot_index["Accent"]; stripe += 1
    elif abs(c.y) > 0.28 and c.z < 0.20:
        p.material_index = slot_index["Carbon"]; band += 1
print("LIVERY stripe_faces=%d dark_band_faces=%d" % (stripe, band))

# Box-projected UVs. Carbon: 1 weave tile per 0.30 m. Painted/other body slots: 1 UV unit = 1 m, so the paint
# shader can tile flakes, dirt and scratches by scaling. Decal slots keep their own UVs.
uv = me.uv_layers.new(name="UVMap")
UV_SCALE = {"Carbon": 0.30, "Paint": 1.0, "Accent": 1.0, "Black": 1.0, "Metal": 1.0}
by_index = {slot_index[n]: sc for n, sc in UV_SCALE.items() if n in slot_index}
for p in me.polygons:
    scale = by_index.get(p.material_index)
    if scale is None:
        continue
    n = p.normal
    ax = max(range(3), key=lambda i: abs(n[i]))
    for li in p.loop_indices:
        co = me.vertices[me.loops[li].vertex_index].co
        u, v = (co.y, co.z) if ax == 0 else (co.x, co.z) if ax == 1 else (co.x, co.y)
        uv.data[li].uv = (u / scale, v / scale)

# ---- decals (shrink-wrapped quads) -----------------------------------------------------------


def decal(name, mat_name, center, size, rot, project_axis):
    bpy.ops.mesh.primitive_grid_add(x_subdivisions=16, y_subdivisions=16, size=1.0, location=center, rotation=rot)
    o = bpy.context.view_layer.objects.active
    o.name = name
    for v in o.data.vertices:
        v.co.x *= size[0]
        v.co.y *= size[1]
    slot(o, mat_name)
    mod = o.modifiers.new("wrap", "SHRINKWRAP")
    mod.target = car
    mod.wrap_method = "PROJECT"
    mod.use_project_x = project_axis == "x"
    mod.use_project_y = project_axis == "y"
    mod.use_project_z = project_axis == "z"
    mod.use_positive_direction = True
    mod.use_negative_direction = True
    mod.offset = 0.004
    select_only([o])
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for poly in o.data.polygons:
        poly.use_smooth = True
    return o


decals = []
# Reading direction notes (X forward, Y left): top decals read toward -Y with 'up' = +X -> rotate -90 about Z;
# right side (y<0) reads toward +X -> rotate 90 about X; left side (y>0) reads toward -X -> rotate 90 about X, 180 about Z.
TOP = (0, 0, -math.pi / 2)
RIGHT = (math.pi / 2, 0, 0)
LEFT = (math.pi / 2, 0, math.pi)
decals.append(decal("NumNose", "DecalNum", (1.38, 0, 0.55), (0.27, 0.27), TOP, "z"))
decals.append(decal("NumAirboxR", "DecalNum", (-0.42, -0.45, 0.68), (0.30, 0.30), RIGHT, "y"))
decals.append(decal("NumAirboxL", "DecalNum", (-0.42, 0.45, 0.68), (0.30, 0.30), LEFT, "y"))
decals.append(decal("ApexPodR", "DecalApex", (-0.15, -0.95, 0.29), (0.95, 0.24), RIGHT, "y"))
decals.append(decal("ApexPodL", "DecalApex", (-0.15, 0.95, 0.29), (0.95, 0.24), LEFT, "y"))
decals.append(decal("ApexRearWing", "DecalApex", (-2.585, 0, 1.12), (0.30, 0.9), TOP, "z"))

select_only([car] + decals, car)
bpy.ops.object.join()
car = bpy.context.view_layer.objects.active
car.name = "F1_Body"

# ---- wheel -----------------------------------------------------------------------------------


def wheel_mesh(radius, width, rim_r):
    """Revolve a tyre profile around the Y axis. Slots: Tyre (sidewall disc UV), Rim (spoke UV), Metal (hub)."""
    bm = bmesh.new()
    hw = width / 2
    prof = [(rim_r, -hw * 0.92), (radius * 0.86, -hw), (radius * 0.97, -hw * 0.95), (radius, -hw * 0.55),
            (radius, hw * 0.55), (radius * 0.97, hw * 0.95), (radius * 0.86, hw), (rim_r, hw * 0.92)]
    seg = 48
    rings = []
    for (r, y) in prof:
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * k / seg), y, r * math.sin(2 * math.pi * k / seg))) for k in range(seg)])
    for a, b in zip(rings[:-1], rings[1:]):
        for k in range(seg):
            n = (k + 1) % seg
            bm.faces.new((a[k], a[n], b[n], b[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    tyre = new_obj("Tyre", bm)
    finish(tyre, "Tyre")

    bm2 = bmesh.new()
    bmesh.ops.create_cone(bm2, cap_ends=True, segments=48, radius1=rim_r, radius2=rim_r * 0.96, depth=width * 0.80)
    rim = new_obj("Rim", bm2)
    rim.rotation_euler = (R(90), 0, 0)
    finish(rim, "Rim")
    bm3 = bmesh.new()
    bmesh.ops.create_cone(bm3, cap_ends=True, segments=16, radius1=rim_r * 0.20, radius2=rim_r * 0.20, depth=width * 0.97)
    hub = new_obj("Hub", bm3)
    hub.rotation_euler = (R(90), 0, 0)
    finish(hub, "Metal")
    select_only([tyre, rim, hub], tyre)
    bpy.ops.object.join()
    w = bpy.context.view_layer.objects.active
    w.name = "F1_Wheel"
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    wm = w.data
    names = [m.name for m in wm.materials]
    wuv = wm.uv_layers.new(name="UVMap")
    for p in wm.polygons:
        slot_name = names[p.material_index]
        for li in p.loop_indices:
            co = wm.vertices[wm.loops[li].vertex_index].co
            if slot_name == "Tyre":
                if abs(p.normal.y) > 0.45:   # sidewall: planar disc map, outer radius == texture edge
                    wuv.data[li].uv = (co.x / (2 * radius) + 0.5, co.z / (2 * radius) + 0.5)
                else:                         # tread: the dark corner of the sidewall texture
                    wuv.data[li].uv = (0.003, 0.003)
            elif slot_name == "Rim":
                if abs(p.normal.y) > 0.5:     # rim face: spokes
                    wuv.data[li].uv = (co.x / (2 * rim_r) + 0.5, co.z / (2 * rim_r) + 0.5)
                else:                         # rim barrel: silver outer ring of the spoke texture
                    wuv.data[li].uv = (0.5, 0.97)
    return w


wheel = wheel_mesh(0.365, 0.405, 0.24)

# ---- export ----------------------------------------------------------------------------------


def export(obj, path):
    select_only([obj])
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, axis_forward="X", axis_up="Z",
                             apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
    print("EXPORTED", path, "verts", len(obj.data.vertices), "polys", len(obj.data.polygons),
          "bounds", [round(c, 2) for c in obj.dimensions], "slots", [m.name for m in obj.data.materials])


export(car, os.path.join(OUT, "F1_Body.fbx"))
export(wheel, os.path.join(OUT, "F1_Wheel.fbx"))
