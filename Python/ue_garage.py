"""Import the pit garage (Blender/gen_garage.py) with team-coloured bays, lit interiors and the scanned building materials.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_garage.py -unattended -nullrhi -nosplash -stdout
Run AFTER ue_buildings.py (reuses MI_Bld_*). Prints GAR_OK / GAR_WARN lines.
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, log, mel, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"

# ---- masters -------------------------------------------------------------------------------------------------------------
g = G(MATS, "M_APEX_PitPlain", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
tint = g.v("Tint", (0.5, 0.5, 0.5))
prop(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
# Self-lit lift for shaded interiors (stands in for the bounce light that a sunlit exterior would give a real bay). The exposure is
# set for a sunlit track, so shade needs thousands of nits to read; it fades out as NightGlow -> 1 (night exposure is ~11 stops higher).
mpc0 = eal.load_asset("/Game/Materials/MPC_APEX_World")
ng0 = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc0, parameter_name="NightGlow")
fade = g.lerp(g.c(1.0), g.c(0.03), ng0)
prop(g.mul(g.mul(tint, g.s("EmissiveScale", 0.0)), fade), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(g.s("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
prop(g.s("Roughness", 0.45), "", unreal.MaterialProperty.MP_ROUGHNESS)
plain = g.done()

g = G(MATS, "M_APEX_PitLight", instanced=False)
g.mat.set_editor_property("used_with_nanite", True)
prop(g.v("Tint", (0.9, 0.9, 0.9)), "", unreal.MaterialProperty.MP_BASE_COLOR)
warm = g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.93, 0.80, 1.0))
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")
# Exposure is set for a sunlit track (~EV 14), so a lamp must be very bright to read in daylight; at night the exposure rises ~11 stops
# and the same value would blind, so it fades to a normal lamp luminance as NightGlow goes to 1.
ng = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
glow = g.lerp(g.s("DayLuminance", 6000.0), g.s("NightLuminance", 60.0), ng)
prop(g.mul(warm, glow), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.done()
log("GAR_OK", "masters")


def mi(name, parent, tint, metallic=0.0, rough=0.45, emissive=0.0):
    full = f"{MATS}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    m.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(m, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.set_material_instance_scalar_parameter_value(m, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(m, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(m, "EmissiveScale", emissive)
    mel.update_material_instance(m)
    eal.save_asset(full)
    return m


# Team colours: read the game's own data when the C++ library is reachable from Python, else a fixed palette.
PALETTE = [(0.80, 0.04, 0.04), (0.05, 0.15, 0.75), (0.95, 0.55, 0.05), (0.05, 0.55, 0.30), (0.45, 0.10, 0.65),
           (0.05, 0.60, 0.70), (0.90, 0.80, 0.05), (0.85, 0.20, 0.55), (0.10, 0.10, 0.12), (0.55, 0.60, 0.65)]
cols = list(PALETTE)
try:
    teams = unreal.APEXGridLibrary.get_teams()
    cols = [(t.primary.r, t.primary.g, t.primary.b) for t in teams]
    log("GAR_OK", f"team colours from game data ({len(cols)})")
except Exception as e:  # noqa: BLE001
    log("GAR_WARN", f"APEXGridLibrary not reachable from Python ({e}); using the fixed palette")
for i, c in enumerate(cols[:10]):
    mi(f"MI_Pit_Team{i}", plain, c, 0.0, 0.32, 3500.0)
mi("MI_Pit_Rubber", plain, (0.015, 0.015, 0.017), 0.0, 0.85)
mi("MI_Pit_Floor", plain, (0.42, 0.42, 0.43), 0.0, 0.30, 2200.0)      # polished interior floor, light enough to read in shade
light = eal.load_asset(f"{MATS}/M_APEX_PitLight")
M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731

SLOT_MAP = {"Wall": M("MI_Bld_Plaster_White"), "Concrete": M("MI_Bld_Concrete_Grey"), "Glass": M("MI_Bld_Glass"), "Metal": M("MI_Bld_Metal"),
            "Roof": M("MI_Bld_Trim_Grey"), "Floor": M("MI_Pit_Floor"), "Rubber": M("MI_Pit_Rubber"), "Light": light}
for i in range(10):
    SLOT_MAP[f"Team{i}"] = M(f"MI_Pit_Team{i}")

src = os.path.join(PROJECT, "Blender", "export", "PitGarage.fbx")
t = unreal.AssetImportTask()
t.filename = src; t.destination_path = "/Game/Environments"; t.destination_name = "SM_PitGarage"
t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
ui = unreal.FbxImportUI()
ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
d = ui.static_mesh_import_data
d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
d.set_editor_property("build_nanite", True)
t.options = ui
tools.import_asset_tasks([t])
m = eal.load_asset("/Game/Environments/SM_PitGarage")
if not m:
    log("GAR_WARN", "SM_PitGarage failed to import")
else:
    for i, s in enumerate(m.get_editor_property("static_materials")):
        n = str(s.get_editor_property("material_slot_name"))
        if SLOT_MAP.get(n):
            m.set_material(i, SLOT_MAP[n])
        else:
            log("GAR_WARN", f"slot {n} unmapped")
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset("/Game/Environments/SM_PitGarage")
    e = m.get_bounds(); o = e.origin; x = e.box_extent
    log("GAR_OK", f"SM_PitGarage size_m=({x.x*2/100:.0f}, {x.y*2/100:.0f}, {x.z*2/100:.1f}) centre_m=({o.x/100:.0f}, {o.y/100:.0f}, {o.z/100:.1f})")
