import os
import unreal
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
SRC = os.path.expanduser("~/Development/APEXFormula/Blender/sources/polyhaven")
for aid, name in (("jacaranda_tree", "SM_Tree_Jacaranda"), ("pine_sapling_small", "SM_Tree_PineSapling"), ("fir_sapling", "SM_Tree_FirSapling")):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, aid, f"{aid}_2k.fbx")
    task.destination_path = "/Game/Environments/Trees"
    task.destination_name = name
    task.automated = True; task.replace_existing = True; task.replace_existing_settings = True; task.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False
    ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True
    d.set_editor_property("build_nanite", True)
    d.auto_generate_collision = False
    d.generate_lightmap_u_vs = False
    task.options = ui
    tools.import_asset_tasks([task])
    m = eal.load_asset(f"/Game/Environments/Trees/{name}")
    if not m:
        unreal.log(f"TREE_FAIL {name} did not import"); continue
    b = m.get_bounds().box_extent
    slots = [str(s.get_editor_property("material_slot_name")) for s in m.get_editor_property("static_materials")]
    ns = m.get_editor_property("nanite_settings")
    tris = unreal.StaticMeshEditorSubsystem().get_number_verts(m, 0)
    unreal.log(f"TREE_PROBE {name}: size_cm=({b.x*2:.0f},{b.y*2:.0f},{b.z*2:.0f}) verts={tris} slots={slots} nanite={ns.enabled}")
