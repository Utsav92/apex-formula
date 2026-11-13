"""Traffic car: paint material (colour picked per instance), import of Blender/gen_traffic.py.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_traffic.py     (after ue_streetprops.py)
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"
g = G(MATS, "M_APEX_TrafficPaint")
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.mat.set_editor_property("used_with_nanite", True)
rnd = g.n(ME.MaterialExpressionPerInstanceRandom)


def hash01(k):
    s_ = g.n(ME.MaterialExpressionSine); link(g.mul(rnd, g.c(k)), "", s_, "")
    f_ = g.n(ME.MaterialExpressionFrac); link(g.mul(s_, g.c(43758.5453)), "", f_, "")
    return f_


def step(h_, e):
    return g.sat(g.mul(g.sub(h_, g.c(e)), g.c(1000.0)))


def col(r, gg, b):
    return g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, gg, b, 1.0))


a = g.lerp(g.lerp(col(0.60, 0.02, 0.02), col(0.02, 0.06, 0.30), step(hash01(12.9898), 0.5)), g.lerp(col(0.55, 0.55, 0.57), col(0.02, 0.02, 0.025), step(hash01(78.233), 0.5)), step(hash01(39.346), 0.5))
b = g.lerp(g.lerp(col(0.82, 0.74, 0.05), col(0.03, 0.22, 0.10), step(hash01(51.11), 0.5)), g.lerp(col(0.75, 0.75, 0.72), col(0.30, 0.32, 0.36), step(hash01(93.7), 0.5)), step(hash01(27.7), 0.5))
prop(g.lerp(a, b, step(hash01(63.3), 0.5)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.45), "", unreal.MaterialProperty.MP_METALLIC)
prop(g.c(0.28), "", unreal.MaterialProperty.MP_ROUGHNESS)
g.done()
log("TRF_OK", "M_APEX_TrafficPaint")

M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731
t = unreal.AssetImportTask()
t.filename = os.path.join(PROJECT, "Blender", "export", "Traffic_Car.fbx")
t.destination_path = "/Game/Environments/Props"; t.destination_name = "SM_Traffic_Car"
t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
ui = unreal.FbxImportUI()
ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
d = ui.static_mesh_import_data
d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
d.set_editor_property("build_nanite", True)
t.options = ui
tools.import_asset_tasks([t])
m = eal.load_asset("/Game/Environments/Props/SM_Traffic_Car")
mapping = {"Body": M("M_APEX_TrafficPaint"), "Glass": M("MI_Bld_Glass"), "Tyre": M("MI_Pit_Rubber"), "Light": M("M_APEX_PitLight"), "Trim": M("MI_Bld_Metal")}
for i, s in enumerate(m.get_editor_property("static_materials")):
    mat = mapping.get(str(s.get_editor_property("material_slot_name")).split("_")[0])
    if mat:
        m.set_material(i, mat)
    else:
        log("TRF_WARN", f"slot {s.get_editor_property('material_slot_name')} unmapped")
ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
eal.save_asset("/Game/Environments/Props/SM_Traffic_Car")
log("TRF_OK", "SM_Traffic_Car")
