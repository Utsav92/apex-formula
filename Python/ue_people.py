"""People materials (palette picked per instance, bob/jump animated in the vertex shader) and the people meshes.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_people.py
Run after ue_garage.py (crew shirts reuse MI_Pit_Team0-9). Prints PPL_OK / PPL_WARN.
Assets: SM_Person_Stand / SM_Person_Cheer (spectators), SM_Person_Crew_T0..T9 (team shirts), SM_Person_Marshal (orange vest).
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"


def col(g, r, gg, b):
    return g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, gg, b, 1.0))


def person_material(name, palette_fn, animated=True):
    g = G(MATS, name)
    g.mat.set_editor_property("used_with_instanced_static_meshes", True)
    rnd = g.n(ME.MaterialExpressionPerInstanceRandom)

    def hash01(k):
        s_ = g.n(ME.MaterialExpressionSine); link(g.mul(rnd, g.c(k)), "", s_, "")
        f_ = g.n(ME.MaterialExpressionFrac); link(g.mul(s_, g.c(43758.5453)), "", f_, "")
        return f_

    def step(h_, e):
        return g.sat(g.mul(g.sub(h_, g.c(e)), g.c(1000.0)))

    prop(palette_fn(g, hash01, step), "", unreal.MaterialProperty.MP_BASE_COLOR)
    prop(g.c(0.75), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Far LOD only: inside 35 m the skeletal NPCs (AAPEXNPCCrowd) take over, so the block figure collapses to its pivot (zero-area triangles).
    wp = g.n(ME.MaterialExpressionWorldPosition)
    op = g.n(ME.MaterialExpressionObjectPositionWS)
    cp = g.n(ME.MaterialExpressionCameraPositionWS)
    dist = g.n(ME.MaterialExpressionDistance); link(cp, "", dist, "A"); link(op, "", dist, "B")
    near = g.sat(g.mul(g.sub(g.c(3500.0), dist), g.c(1000.0)))
    collapse = g.sub(op, wp)
    if animated:
        # jump/bob: z = amp * |sin(cycle)|, per-person phase, rate and amplitude (2-13 cm)
        t = g.n(ME.MaterialExpressionTime)
        rate = g.add(g.c(0.7), g.mul(hash01(7.31), g.c(1.6)))
        arg = g.add(g.mul(t, rate), g.mul(rnd, g.c(5.0)))
        sn = g.n(ME.MaterialExpressionSine); link(arg, "", sn, "")
        ab = g.n(ME.MaterialExpressionAbs); link(sn, "", ab, "")
        amp = g.add(g.c(2.0), g.mul(hash01(13.7), g.c(11.0)))
        dz = g.mul(ab, amp)
        bob = g.n(ME.MaterialExpressionAppendVector)
        link(g.n(ME.MaterialExpressionConstant2Vector, r=0.0, g=0.0), "", bob, "A"); link(dz, "", bob, "B")
    else:
        bob = g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
    prop(g.lerp(bob, collapse, near), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    g.done()
    log("PPL_OK", name)


def shirt(g, h, st):
    a = g.lerp(g.lerp(col(g, 0.55, 0.05, 0.05), col(g, 0.05, 0.16, 0.50), st(h(12.9898), 0.5)), g.lerp(col(g, 0.70, 0.55, 0.05), col(g, 0.60, 0.60, 0.60), st(h(78.233), 0.5)), st(h(39.346), 0.5))
    b = g.lerp(g.lerp(col(g, 0.04, 0.30, 0.12), col(g, 0.70, 0.22, 0.02), st(h(51.11), 0.5)), g.lerp(col(g, 0.25, 0.05, 0.30), col(g, 0.03, 0.03, 0.04), st(h(93.7), 0.5)), st(h(27.7), 0.5))
    return g.lerp(a, b, st(h(63.3), 0.5))


def skin(g, h, st):
    return g.lerp(g.lerp(col(g, 0.55, 0.38, 0.28), col(g, 0.34, 0.21, 0.14), st(h(17.3), 0.5)), col(g, 0.12, 0.07, 0.05), st(h(41.9), 0.78))


def dark(g, h, st):
    return g.lerp(col(g, 0.02, 0.02, 0.03), col(g, 0.07, 0.065, 0.06), st(h(23.9), 0.5))


person_material("M_APEX_PersonShirt", shirt)
person_material("M_APEX_PersonSkin", skin)
person_material("M_APEX_PersonDark", dark)
person_material("M_APEX_PersonMarshal", lambda g, h, st: col(g, 0.95, 0.30, 0.02), animated=False)

M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731


def import_person(name, mapping):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(PROJECT, "Blender", "export", f"Person_{name}.fbx")
    t.destination_path = "/Game/Environments/People"; t.destination_name = f"SM_Person_{name}"
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
    t.options = ui
    tools.import_asset_tasks([t])
    return f"/Game/Environments/People/SM_Person_{name}"


def apply(path, mapping):
    m = eal.load_asset(path)
    for i, s in enumerate(m.get_editor_property("static_materials")):
        slot = str(s.get_editor_property("material_slot_name")).split("_")[0]
        mat = mapping.get(slot)
        if mat:
            m.set_material(i, mat)
    eal.save_asset(path)


base = {"Skin": M("M_APEX_PersonSkin"), "Shirt": M("M_APEX_PersonShirt"), "Pants": M("M_APEX_PersonDark"), "Hair": M("M_APEX_PersonDark")}
for n in ("Stand", "Cheer"):
    p = import_person(n, base); apply(p, base); log("PPL_OK", p)
for k in range(10):
    dst = f"/Game/Environments/People/SM_Person_Crew_T{k}"
    if eal.does_asset_exist(dst):
        eal.delete_asset(dst)
    eal.duplicate_asset("/Game/Environments/People/SM_Person_Stand", dst)
    apply(dst, dict(base, Shirt=M(f"MI_Pit_Team{k}"), Pants=M("M_APEX_PersonDark")))
dst = "/Game/Environments/People/SM_Person_Marshal"
if eal.does_asset_exist(dst):
    eal.delete_asset(dst)
eal.duplicate_asset("/Game/Environments/People/SM_Person_Stand", dst)
apply(dst, dict(base, Shirt=M("M_APEX_PersonMarshal")))
log("PPL_OK", "crew x10 + marshal")


# ---- M_APEX_NPCCloth: plain skin-mesh material for the skeletal NPCs; C++ creates one instance per NPC and sets Colour ----------------------------------
g = G(MATS, "M_APEX_NPCCloth")
g.mat.set_editor_property("used_with_skeletal_mesh", True)
prop(g.v("Colour", (0.5, 0.5, 0.5)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.72), "", unreal.MaterialProperty.MP_ROUGHNESS)
g.done()
log("PPL_OK", "M_APEX_NPCCloth")
