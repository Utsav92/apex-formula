"""Import street props (Blender/gen_streetprops.py) and build the LED sponsor-board material.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_streetprops.py -unattended -nullrhi -nosplash -stdout
Run AFTER ue_garage.py (reuses MI_Bld_Metal, M_APEX_PitLight, MI_Pit_Team0). Prints SP_OK / SP_WARN.
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")

# ---- M_APEX_Led: each board picks a sponsor colour scheme from the hash of its world X (one scheme per 8 m board) -----------------------
g = G(MATS, "M_APEX_Led")
g.mat.set_editor_property("used_with_nanite", True)
prop(g.c(0.01), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.2), "", unreal.MaterialProperty.MP_ROUGHNESS)
wp = g.n(ME.MaterialExpressionWorldPosition)
wx = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(wp, "", wx, "")
idx = g.n(ME.MaterialExpressionFloor); link(g.mul(wx, g.c(1.0 / 900.0)), "", idx, "")                    # board index along X (8 m boards)


def hash01(k):
    sn_ = g.n(ME.MaterialExpressionSine); link(g.mul(idx, g.c(k)), "", sn_, "")
    f_ = g.n(ME.MaterialExpressionFrac); link(g.mul(sn_, g.c(43758.5453)), "", f_, "")
    return f_


def step(h_, edge):
    return g.sat(g.mul(g.sub(h_, g.c(edge)), g.c(1000.0)))


def col(r, gg, bb):
    return g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, gg, bb, 1.0))


# four vivid fictional-sponsor colourways, chosen per board by three independent hashes
cA, cB, cC, cD = col(1.0, 0.08, 0.35), col(0.05, 0.75, 1.0), col(1.0, 0.55, 0.05), col(0.35, 1.0, 0.25)
palette = g.lerp(g.lerp(cA, cB, step(hash01(12.9898), 0.5)), g.lerp(cC, cD, step(hash01(78.233), 0.5)), step(hash01(39.346), 0.5))
# horizontal "text rows": bright bands across the screen, darker gaps, so it reads as a display and not a flat colour
uv = g.tc(1.0, 1.0)
v = g.n(ME.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); link(uv, "", v, "")
tm = g.n(ME.MaterialExpressionTime)
scroll = g.mul(tm, g.add(g.c(0.10), g.mul(hash01(5.5), g.c(0.45))))          # each board scrolls its text rows at its own speed
fr = g.n(ME.MaterialExpressionFrac); link(g.mul(g.add(v, scroll), g.c(5.0)), "", fr, "")
band = g.add(g.c(0.30), g.mul(g.sat(g.mul(g.sub(g.c(0.62), fr), g.c(40.0))), g.c(0.70)))
ng = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
lum = g.lerp(g.s("DayLuminance", 5000.0), g.s("NightLuminance", 90.0), ng)
sn2 = g.n(ME.MaterialExpressionSine); link(g.add(g.mul(tm, g.c(0.9)), idx), "", sn2, "")
flicker = g.add(g.c(0.90), g.mul(sn2, g.c(0.10)))                              # slow brightness breathing, out of phase between boards
prop(g.mul(g.mul(g.mul(palette, band), lum), flicker), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.done()
log("SP_OK", "M_APEX_Led")

M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731
# Non-emissive street paint (the team materials glow, which is right for the garage and wrong for a hydrant).
_plain = M("M_APEX_PitPlain")
if eal.does_asset_exist(f"{MATS}/MI_Street_Red"):
    eal.delete_asset(f"{MATS}/MI_Street_Red")
_mi = tools.create_asset("MI_Street_Red", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
_mi.set_editor_property("parent", _plain)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(_mi, "Tint", unreal.LinearColor(0.55, 0.03, 0.02, 1.0))
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(_mi, "Metallic", 0.0)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(_mi, "Roughness", 0.45)
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(_mi, "EmissiveScale", 0.0)
unreal.MaterialEditingLibrary.update_material_instance(_mi)
eal.save_asset(f"{MATS}/MI_Street_Red")
slot_maps = {
    "Lamp": {"Glass": M("MI_Bld_Glass"), "Stripe": M("MI_Bld_Concrete_Grey"), "Metal": M("MI_Bld_Metal"), "Light": M("M_APEX_PitLight"), "Base": M("MI_Bld_Concrete_Grey"), "Paint": M("MI_Pit_Team0"), "Led": M("M_APEX_Led")},
    "Led": {"Glass": M("MI_Bld_Glass"), "Stripe": M("MI_Bld_Concrete_Grey"), "Metal": M("MI_Bld_Metal"), "Led": M("M_APEX_Led"), "Base": M("MI_Bld_Concrete_Grey"), "Light": M("M_APEX_PitLight"), "Paint": M("MI_Pit_Team0")},
    "Hydrant": {"Glass": M("MI_Bld_Glass"), "Stripe": M("MI_Bld_Concrete_Grey"), "Metal": M("MI_Bld_Metal"), "Paint": M("MI_Street_Red"), "Base": M("MI_Bld_Concrete_Grey"), "Light": M("M_APEX_PitLight"), "Led": M("M_APEX_Led")},
}
P = lambda n: eal.load_asset(f"/Game/Materials/{n}")  # noqa: E731
common = {"Net": M("MI_Bld_Shutter_Green"), "Black": M("MI_Pit_Rubber"), "Metal": M("MI_Bld_Metal"), "Base": M("MI_Bld_Concrete_Grey"), "Light": M("M_APEX_PitLight"), "Led": M("M_APEX_Led"),
          "Glass": M("MI_Bld_Glass"), "Stripe": P("M_PitPaint"), "Paint": P("M_PitPaint")}
for n in ("Signal", "Shelter", "Crosswalk", "Kerb", "NeonScreen", "NeonBlade", "SteamStack", "Scaffold", "Bin", "Bags", "Bollard"):
    slot_maps[n] = dict(common)
for name, smap in slot_maps.items():
    t = unreal.AssetImportTask()
    t.filename = os.path.join(PROJECT, "Blender", "export", f"Prop_{name}.fbx")
    t.destination_path = "/Game/Environments/Props"; t.destination_name = f"SM_Prop_{name}"
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
    d.set_editor_property("build_nanite", True)
    t.options = ui
    tools.import_asset_tasks([t])
    m = eal.load_asset(f"/Game/Environments/Props/SM_Prop_{name}")
    if not m:
        log("SP_WARN", f"{name} failed"); continue
    for i, s in enumerate(m.get_editor_property("static_materials")):
        mat = smap.get(str(s.get_editor_property("material_slot_name")).split("_")[0])   # Blender suffixes reused names (Metal_001)
        if mat:
            m.set_material(i, mat)
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset(f"/Game/Environments/Props/SM_Prop_{name}")
    log("SP_OK", f"SM_Prop_{name}")
