"""Visual track mesh for APEX Formula, built from the SAME centreline as the game code.

The centreline maths mirrors AFormulaTrack::ComputeCenterline (centripetal Catmull-Rom through the same
control points, resampled to 420 evenly spaced points). Collision stays in C++ (instanced cubes); this mesh is
visual only. Material slots: Asphalt, Kerb, Barrier, Chequer, Grass, Paint (pit lane markings).
The pit lane is built from Blender/export/pit_path.json (written by the game with -APEXExportPit) when present.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_track.py
Env:  APEX_MIRROR_Y=1 (default) flips Y so the geometry lands correctly after Unreal's FBX handedness flip.
Out:  Blender/export/Track.fbx
"""
import json
import math
import os

import bmesh
import bpy
from mathutils import Vector

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "export")
os.makedirs(OUT, exist_ok=True)
MIRROR_Y = os.environ.get("APEX_MIRROR_Y", "1") == "1"

# --- centreline (matches AFormulaTrack) ----------------------------------------------------------
CP = [(0, 0), (300, 0), (560, 0), (760, 50), (830, 190), (780, 310), (830, 430), (710, 525),
      (530, 560), (430, 480), (480, 380), (360, 300), (200, 335), (60, 425), (-120, 440),
      (-300, 385), (-420, 260), (-470, 120), (-430, 20), (-300, 0)]
N = 420
ROAD_HALF = 7.0
BARRIER_OFF = 8.0     # matches C++: RoadWidth/2 + 1 m


def cr_segment(p0, p1, p2, p3, n, alpha=0.5):
    def tj(ti, a, b):
        return ti + math.dist(a, b) ** alpha
    t0 = 0.0; t1 = tj(t0, p0, p1); t2 = tj(t1, p1, p2); t3 = tj(t2, p2, p3)
    out = []
    for k in range(n):
        t = t1 + (t2 - t1) * k / n

        def lerp(a, b, ta, tb):
            w = (t - ta) / (tb - ta) if tb != ta else 0
            return (a[0] * (1 - w) + b[0] * w, a[1] * (1 - w) + b[1] * w)
        a1 = lerp(p0, p1, t0, t1); a2 = lerp(p1, p2, t1, t2); a3 = lerp(p2, p3, t2, t3)
        b1 = lerp(a1, a2, t0, t2); b2 = lerp(a2, a3, t1, t3)
        out.append(lerp(b1, b2, t1, t2))
    return out


def centerline():
    dense = []
    c = len(CP)
    for i in range(c):
        dense += cr_segment(CP[(i - 1) % c], CP[i], CP[(i + 1) % c], CP[(i + 2) % c], 60)
    cum = [0.0]
    for i in range(len(dense)):
        cum.append(cum[-1] + math.dist(dense[i], dense[(i + 1) % len(dense)]))
    total = cum[-1]
    pts = []
    j = 0
    for k in range(N):
        s = total * k / N
        while cum[j + 1] < s:
            j += 1
        a = dense[j]; b = dense[(j + 1) % len(dense)]
        w = (s - cum[j]) / (cum[j + 1] - cum[j])
        pts.append((a[0] + (b[0] - a[0]) * w, a[1] + (b[1] - a[1]) * w))
    return pts, total


P, TOTAL = centerline()
STEP = TOTAL / N
S = [i * STEP for i in range(N + 1)]            # distance along the lap, ring N repeats ring 0
TAN = []
for i in range(N):
    a = Vector(P[(i - 1) % N]); b = Vector(P[(i + 1) % N])
    TAN.append((b - a).normalized())
NRM = [Vector((-t.y, t.x)) for t in TAN]         # left normal


def circum_r(a, b, c):
    A = math.dist(b, c); B = math.dist(a, c); C = math.dist(a, b)
    area2 = abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
    return float("inf") if area2 < 1e-9 else A * B * C / (2 * area2)


RAD = [circum_r(P[(i - 1) % N], P[i], P[(i + 1) % N]) for i in range(N)]
# Smooth the radius and flag corners (kerbs only where the road actually turns).
CORNER = []
for i in range(N):
    local = min(RAD[(i + k) % N] for k in range(-3, 4))
    CORNER.append(local < 220.0)

# --- mesh building ------------------------------------------------------------------------------
MATS = ["Asphalt", "Kerb", "Barrier", "Chequer", "Grass", "Paint", "Pavement"]
bpy.ops.wm.read_factory_settings(use_empty=True)
mesh = bpy.data.meshes.new("Track")
for name in MATS:
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    mesh.materials.append(m)
bm = bmesh.new()
uv_layer = bm.loops.layers.uv.new("UVMap")


def world(off_i, ring, off, z):
    p = Vector(P[ring % N]) + NRM[ring % N] * off
    return Vector((p.x, -p.y if MIRROR_Y else p.y, z))


def add_quad(verts, uvs, mat, desired_up=None):
    try:
        f = bm.faces.new(verts)
    except ValueError:
        return None  # degenerate duplicate face
    f.material_index = MATS.index(mat)
    # Assign UVs BEFORE any flip: flipping reverses the loop order, which would scramble them.
    for loop, uv in zip(f.loops, uvs):
        loop[uv_layer].uv = uv
    f.normal_update()
    if desired_up is not None and f.normal.dot(desired_up) < 0:
        f.normal_flip()
    return f


def ring_verts(ring, section):
    return [bm.verts.new(world(None, ring, off, z)) for (off, z) in section]


def strip(section, mat, uvf, desired, mask=None):
    """Sweep a cross-section ((offset, z) points) along the lap.
    uvf(s, k, section_len) -> (u, v). desired(ring) -> intended outward normal (in output space)."""
    verts = [ring_verts(i, section) for i in range(N)]
    verts.append(ring_verts(0, section))   # closing ring gets its own verts so UVs keep increasing
    for i in range(N):
        if mask is not None and not mask[i]:
            continue
        for k in range(len(section) - 1):
            v00, v10 = verts[i][k], verts[i + 1][k]
            v11, v01 = verts[i + 1][k + 1], verts[i][k + 1]
            uvs = [uvf(S[i], k, len(section)), uvf(S[i + 1], k, len(section)),
                   uvf(S[i + 1], k + 1, len(section)), uvf(S[i], k + 1, len(section))]
            add_quad([v00, v10, v11, v01], uvs, mat, desired(i, k))


def flip_y(v):
    return Vector((v.x, -v.y if MIRROR_Y else v.y, v.z))


UP = Vector((0, 0, 1))
ROAD_TILE = TOTAL / round(TOTAL / 14.0)          # whole number of tiles per lap: no seam at the line
KERB_TILE = 1.5
BARRIER_TILE = 8.0

# Road surface
strip([(-ROAD_HALF, 0.02), (ROAD_HALF, 0.02)], "Asphalt",
      lambda s, k, n: (float(k), s / ROAD_TILE), lambda i, k: UP)

# Kerbs on both sides, corners only
for side in (1, -1):
    sec = [(side * ROAD_HALF, 0.03), (side * (ROAD_HALF + 0.75), 0.03)]
    strip(sec, "Kerb", lambda s, k, n: (float(k), s / KERB_TILE), lambda i, k: UP, mask=CORNER)

# City pavement ribbon along the whole lap (barrier to 58 m from the centreline), replacing the grass verge. One UV unit = 4 m.
for side in (1, -1):
    sec = [(side * (BARRIER_OFF + 0.3), 0.006), (side * 58.0, 0.006)]
    strip(sec, "Pavement", lambda s, k, n: (float(k) * 8.0, s / 4.0), lambda i, k: UP)

# Barriers: inner face, top, outer face. Same position/height as the C++ collision cubes.
for side in (1, -1):
    inner, outer = BARRIER_OFF - 0.25, BARRIER_OFF + 0.25
    sec = [(side * inner, 0.0), (side * inner, 1.2), (side * outer, 1.2), (side * outer, 0.0)]

    def barrier_uv(s, k, n):
        # k: 0 inner face, 1 top, 2 outer face
        v = {0: 0.0, 1: 1.0, 2: 1.0}[k]
        return (s / BARRIER_TILE, v)

    def barrier_desired(i, k, side=side):
        n = Vector((NRM[i].x, -NRM[i].y if MIRROR_Y else NRM[i].y, 0))   # left normal in output space
        if k == 0:
            return n * (-side)      # inner face looks back at the road
        if k == 1:
            return UP
        return n * side             # outer face looks away
    # Build with explicit per-face UVs so each of the three faces maps the full wall height.
    verts = [ring_verts(i, sec) for i in range(N)] + [ring_verts(0, sec)]
    for i in range(N):
        # Pit entry / exit: the pit-side (-Y) barrier is open where the lane crosses it (same X ranges as AFormulaTrack::IsBarrierGap).
        if side == -1 and abs(P[i][1]) < 6.0 and (-300.0 < P[i][0] < -245.0 or 265.0 < P[i][0] < 320.0):
            continue
        for k in range(3):
            v00, v10, v11, v01 = verts[i][k], verts[i + 1][k], verts[i + 1][k + 1], verts[i][k + 1]
            vtop, vbot = (1.0, 0.0)
            if k == 0:
                uvs = [(S[i] / BARRIER_TILE, 0.0), (S[i + 1] / BARRIER_TILE, 0.0), (S[i + 1] / BARRIER_TILE, 1.0), (S[i] / BARRIER_TILE, 1.0)]
            elif k == 1:
                uvs = [(S[i] / BARRIER_TILE, 0.95), (S[i + 1] / BARRIER_TILE, 0.95), (S[i + 1] / BARRIER_TILE, 0.95), (S[i] / BARRIER_TILE, 0.95)]
            else:
                uvs = [(S[i] / BARRIER_TILE, 1.0), (S[i + 1] / BARRIER_TILE, 1.0), (S[i + 1] / BARRIER_TILE, 0.0), (S[i] / BARRIER_TILE, 0.0)]
            add_quad([v00, v10, v11, v01], uvs, "Barrier", barrier_desired(i, k))


# ---- Pit lane (surface, markings) -------------------------------------------------------------------------------------
PIT_JSON = os.path.join(OUT, "pit_path.json")
PIT_BOXES = [(-170.0 + 22.0 * t, 20.5) for t in range(10)]      # centre of each team box (m), matches AFormulaTrack
if os.path.exists(PIT_JSON):
    pit = [(x / 100.0, y / 100.0) for x, y in json.load(open(PIT_JSON))]

    def smooth(a, b, x):
        t = max(0.0, min(1.0, (x - a) / (b - a)))
        return t * t * (3 - 2 * t)

    def out_v(x, y, z):
        return bm.verts.new(Vector((x, -y if MIRROR_Y else y, z)))

    # Road surface: 8 m wide where the lane joins the circuit, 13 m on the straight (fast lane + working lane), same as the collision strip.
    PIT_Z = 0.025
    cum = [0.0]
    for i in range(1, len(pit)):
        cum.append(cum[-1] + math.dist(pit[i], pit[i - 1]))
    frames = []
    for i in range(len(pit)):
        a = Vector(pit[max(i - 1, 0)]); b = Vector(pit[min(i + 1, len(pit) - 1)])
        t = (b - a).normalized(); n = Vector((-t.y, t.x))
        k = smooth(9.0, 15.0, -pit[i][1])
        c = Vector(pit[i]) + Vector((0.0, -2.25 * k))
        frames.append((c, n, 4.0 + 2.5 * k))
    for i in range(len(pit) - 1):
        (c0, n0, h0), (c1, n1, h1) = frames[i], frames[i + 1]
        l0, r0 = c0 + n0 * h0, c0 - n0 * h0
        l1, r1 = c1 + n1 * h1, c1 - n1 * h1
        uv = lambda off, h, s_: (off / (2 * h) * (2 * h / ROAD_TILE) + 0.5, s_ / ROAD_TILE)  # noqa: E731
        add_quad([out_v(r0.x, r0.y, PIT_Z), out_v(l0.x, l0.y, PIT_Z), out_v(l1.x, l1.y, PIT_Z), out_v(r1.x, r1.y, PIT_Z)],
                 [uv(-h0, h0, cum[i]), uv(h0, h0, cum[i]), uv(h1, h1, cum[i + 1]), uv(-h1, h1, cum[i + 1])], "Asphalt", UP)

    def paint(x0, x1, y0, y1, z=0.03):
        # y0/y1 are distances from the circuit on the pit side (positive); the lane lies on -Y.
        add_quad([out_v(x0, -y0, z), out_v(x1, -y0, z), out_v(x1, -y1, z), out_v(x0, -y1, z)], [(0, 0), (1, 0), (1, 1), (0, 1)], "Paint", UP)

    LW = 0.14
    ZONE0, ZONE1 = -225.0, 245.0
    # Lane edge lines (track side and garage side) and the line between the fast lane and the working lane.
    paint(ZONE0, ZONE1, 12.0, 12.0 + LW)
    paint(ZONE0, ZONE1, 24.3, 24.3 + LW)
    paint(ZONE0, ZONE1, 18.35, 18.35 + LW)
    # Speed-limit start / end lines across the whole lane.
    for xz in (ZONE0, ZONE1):
        paint(xz, xz + 0.45, 12.0, 24.3 + LW)
    # Box outlines (8.4 m x 3.6 m) with bay dividers behind them, and a stop tick at the front axle position.
    for (bx, by) in PIT_BOXES:
        paint(bx - 4.2, bx + 4.2, by - 1.8, by - 1.8 + LW)
        paint(bx - 4.2, bx + 4.2, by + 1.8 - LW, by + 1.8)
        paint(bx - 4.2, bx - 4.2 + LW, by - 1.8, by + 1.8)
        paint(bx + 4.2 - LW, bx + 4.2, by - 1.8, by + 1.8)
        paint(bx + 1.0, bx + 1.0 + 0.3, by - 0.9, by + 0.9)          # stop mark
        paint(bx - 11.0, bx - 11.0 + LW, 18.5, 24.3)                 # divider between bays
    paint(PIT_BOXES[-1][0] + 11.0, PIT_BOXES[-1][0] + 11.0 + LW, 18.5, 24.3)
    print("PIT lane: %d path points, %d boxes, length %.0f m" % (len(pit), len(PIT_BOXES), cum[-1]))
else:
    print("PIT: no pit_path.json, skipping pit lane")

# Chequered start/finish line across the road at ring 0, 1.75 m long.
t0 = TAN[0]
base = Vector(P[0])
half_len = 0.875
corners = []
for da, db in ((-half_len, -ROAD_HALF), (half_len, -ROAD_HALF), (half_len, ROAD_HALF), (-half_len, ROAD_HALF)):
    p = base + t0 * da + NRM[0] * db
    corners.append(bm.verts.new(Vector((p.x, -p.y if MIRROR_Y else p.y, 0.035))))
add_quad(corners, [(0, 0), (0, 0.25), (2, 0.25), (2, 0)], "Chequer", UP)

# City pavement (Phase 1 urban canyon): the first 500 m of the start straight is paved wall to wall instead of grass. One UV unit = 4 m.
PX0, PX1, PY0, PY1 = -330.0, 340.0, -95.0, 95.0
pv = [bm.verts.new((x, -y if MIRROR_Y else y, 0.006)) for (x, y) in ((PX0, PY0), (PX1, PY0), (PX1, PY1), (PX0, PY1))]
add_quad(pv, [(PX0 / 4, PY0 / 4), (PX1 / 4, PY0 / 4), (PX1 / 4, PY1 / 4), (PX0 / 4, PY1 / 4)], "Pavement", UP)

# Ground plane (city asphalt), 12 km square, one UV unit = 8 m.
G = 6000.0
gv = [bm.verts.new((x, y, -0.02)) for (x, y) in ((-G, -G), (G, -G), (G, G), (-G, G))]
add_quad(gv, [(-G / 8, -G / 8), (G / 8, -G / 8), (G / 8, G / 8), (-G / 8, G / 8)], "Grass", UP)

bm.to_mesh(mesh)
bm.free()
obj = bpy.data.objects.new("Track", mesh)
bpy.context.collection.objects.link(obj)
for p in mesh.polygons:
    p.use_smooth = False
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "Track.fbx"), use_selection=True, axis_forward="X", axis_up="Z",
                         apply_unit_scale=True, global_scale=1.0, mesh_smooth_type="FACE", add_leaf_bones=False)
print("EXPORTED Track.fbx lap_m=%.0f verts=%d polys=%d corner_points=%d/%d mirror_y=%s" %
      (TOTAL, len(mesh.vertices), len(mesh.polygons), sum(CORNER), N, MIRROR_Y))
