"""Re-import Blender/export/Track.fbx (now with the pit lane + 'Paint' slot) and create M_PitPaint.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_track_pit.py -unattended -nullrhi -nosplash -stdout
Prints PIT_OK / PIT_WARN lines. Run AFTER ue_pbr_materials.py (the slot map uses the PBR M_Asphalt/M_Barrier/M_Grass).
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, eal, log, prop, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")

# ---- M_PitPaint: worn white road paint ---------------------------------------------------------------------------------
g = G("/Game/Materials", "M_PitPaint")
g.mat.set_editor_property("used_with_nanite", True)
prop(g.v("PaintColour", (0.62, 0.62, 0.60)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.55), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("PIT_OK", "M_PitPaint")

SLOT_MAP = {"Asphalt": "M_Asphalt", "Kerb": "M_Kerb", "Barrier": "M_Barrier", "Chequer": "M_Chequer", "Grass": "M_Asphalt", "Paint": "M_PitPaint", "Pavement": "Buildings/MI_Bld_Concrete_Grey"}
src = os.path.join(PROJECT, "Blender", "export", "Track.fbx")
t = unreal.AssetImportTask()
t.filename = src; t.destination_path = "/Game/Tracks"; t.destination_name = "SM_Track"
t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
ui = unreal.FbxImportUI()
ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
d = ui.static_mesh_import_data
d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
d.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
d.set_editor_property("build_nanite", True)
t.options = ui
tools.import_asset_tasks([t])
m = eal.load_asset("/Game/Tracks/SM_Track")
if not m:
    log("PIT_WARN", "SM_Track failed to import")
else:
    names = []
    for i, s in enumerate(m.get_editor_property("static_materials")):
        n = str(s.get_editor_property("material_slot_name")); names.append(n)
        tgt = SLOT_MAP.get(n)
        if tgt:
            m.set_material(i, eal.load_asset(f"/Game/Materials/{tgt}"))
        else:
            log("PIT_WARN", f"slot {n} unmapped")
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset("/Game/Tracks/SM_Track")
    e = m.get_bounds().box_extent
    log("PIT_OK", f"SM_Track slots={names} size_m=({e.x*2/100:.0f}, {e.y*2/100:.0f}, {e.z*2/100:.1f})")
