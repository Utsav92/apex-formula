"""Import the New York building set (Blender/gen_nyc.py) and build the awning material.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_nyc.py
Run after ue_buildings.py and ue_streetprops.py (reuses their materials). Prints NYC_OK / NYC_WARN.
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"
M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731

# ---- M_APEX_Awning: muted shop-awning colours chosen per awning from the hash of world X (one colour per 6 m) -----------------------------
g = G(MATS, "M_APEX_Awning")
g.mat.set_editor_property("used_with_nanite", True)
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
wp = g.n(ME.MaterialExpressionWorldPosition)
wx = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(wp, "", wx, "")
idx = g.n(ME.MaterialExpressionFloor); link(g.mul(wx, g.c(1.0 / 600.0)), "", idx, "")


def hash01(k):
    s_ = g.n(ME.MaterialExpressionSine); link(g.mul(idx, g.c(k)), "", s_, "")
    f_ = g.n(ME.MaterialExpressionFrac); link(g.mul(s_, g.c(43758.5453)), "", f_, "")
    return f_


def step(h_, e):
    return g.sat(g.mul(g.sub(h_, g.c(e)), g.c(1000.0)))


def col(r, gg, b):
    return g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, gg, b, 1.0))


pal = g.lerp(g.lerp(col(0.02, 0.18, 0.08), col(0.03, 0.06, 0.20), step(hash01(12.9898), 0.5)),
             g.lerp(col(0.28, 0.03, 0.04), col(0.35, 0.22, 0.04), step(hash01(78.233), 0.5)), step(hash01(39.346), 0.5))
prop(pal, "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.85), "", unreal.MaterialProperty.MP_ROUGHNESS)
g.done()
log("NYC_OK", "M_APEX_Awning")

COMMON = {"Glass": "MI_Bld_Glass", "GlassB": "MI_Bld_GlassB", "GlassC": "MI_Bld_GlassC", "Metal": "MI_Bld_Metal", "Wood": "MI_Bld_Wood",
          "Sign": "M_APEX_Led", "Awn": "M_APEX_Awning"}
BUILDINGS = {
    "NYC_Brownstone": dict(Wall="MI_Bld_Brick_Brown", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Steel_Roof"),
    "NYC_Walkup": dict(Wall="MI_Bld_Brick_Red", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Steel_Roof"),
    "NYC_Mixed": dict(Wall="MI_Bld_Brick_Brown", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Steel_Roof"),
    "NYC_Deco": dict(Wall="MI_Bld_Plaster_Ochre", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Steel_Roof"),
    "NYC_Warehouse": dict(Wall="MI_Bld_Brick_Red", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Steel_Roof"),
}
for name, slots in BUILDINGS.items():
    src = os.path.join(PROJECT, "Blender", "export", f"Bld_{name}.fbx")
    t = unreal.AssetImportTask()
    t.filename = src; t.destination_path = "/Game/Environments/Buildings"; t.destination_name = f"SM_Bld_{name}"
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
    d.set_editor_property("build_nanite", True)
    t.options = ui
    tools.import_asset_tasks([t])
    m = eal.load_asset(f"/Game/Environments/Buildings/SM_Bld_{name}")
    if not m:
        log("NYC_WARN", f"{name} failed"); continue
    mapping = dict(COMMON, **slots)
    for i, s in enumerate(m.get_editor_property("static_materials")):
        slot = str(s.get_editor_property("material_slot_name")).split("_")[0]
        mat = M(mapping.get(slot, "")) if slot in mapping else None
        if mat:
            m.set_material(i, mat)
        else:
            log("NYC_WARN", f"{name}: slot {slot} unmapped")
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset(f"/Game/Environments/Buildings/SM_Bld_{name}")
    b = m.get_bounds().box_extent
    log("NYC_OK", f"SM_Bld_{name}: size_m=({b.x*2/100:.1f}, {b.y*2/100:.1f}, {b.z*2/100:.1f})")
