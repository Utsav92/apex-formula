"""Import the sponsor board meshes (Blender/gen_sponsor_boards.py) and the announcement textures; map the steel / back / light slots.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_sponsor_meshes.py        (after ue_sponsors.py)
"""
import json
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import eal, log, tools  # noqa: E402

ROOT = os.path.expanduser("~/Development/APEXFormula")
reg = json.load(open(os.path.join(ROOT, "Sponsors", "registry.json")))
for a in reg["announcements"]:
    name = f"T_Ann_{a['id']}"
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ROOT, "Sponsors", "textures", name + ".png")
    t.destination_path = "/Game/Sponsors/Textures"; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(f"/Game/Sponsors/Textures/{name}")
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP); tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    eal.save_asset(f"/Game/Sponsors/Textures/{name}")
log("ADM_OK", f"{len(reg['announcements'])} announcement textures")

B = "/Game/Materials/Buildings"
MAP = {"Panel": f"/Game/Sponsors/Materials/MI_SponsorStatic_voltage", "PanelB": f"/Game/Sponsors/Materials/MI_SponsorStatic_cirrus", "Steel": f"{B}/MI_Bld_Metal",
       "Back": f"{B}/MI_Pit_Rubber", "Light": f"{B}/M_APEX_PitLight"}
for name in ("Barrier", "Large", "Gantry", "Billboard"):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ROOT, "Blender", "export", f"Ad_{name}.fbx")
    t.destination_path = "/Game/Sponsors/Meshes"; t.destination_name = f"SM_Ad_{name}"
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
    d.set_editor_property("build_nanite", True)
    t.options = ui
    tools.import_asset_tasks([t])
    m = eal.load_asset(f"/Game/Sponsors/Meshes/SM_Ad_{name}")
    if not m:
        log("ADM_WARN", f"{name} failed"); continue
    slots = []
    for i, s in enumerate(m.get_editor_property("static_materials")):
        sn = str(s.get_editor_property("material_slot_name")).split("_")[0]
        slots.append(sn)
        if sn in MAP:
            m.set_material(i, eal.load_asset(MAP[sn]))
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset(f"/Game/Sponsors/Meshes/SM_Ad_{name}")
    log("ADM_OK", f"SM_Ad_{name} slots={slots}")
