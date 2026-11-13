"""Fix the 'bare distant trees' problem: Nanite simplifies thin masked leaf cards away with distance. Voxelized shape preservation (Nanite foliage)
keeps canopy volume. Sets it on the three tree meshes and saves them.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_tree_nanite_foliage.py
"""
import unreal

for name in ("SM_Tree_Jacaranda", "SM_Tree_PineSapling", "SM_Tree_FirSapling"):
    path = f"/Game/Environments/Trees/{name}"
    m = unreal.EditorAssetLibrary.load_asset(path)
    ns = m.get_editor_property("nanite_settings")
    ns.set_editor_property("enabled", True)
    ns.set_editor_property("shape_preservation", unreal.NaniteShapePreservation.VOXELIZE)
    ns.set_editor_property("voxel_opacity", True)
    m.set_editor_property("nanite_settings", ns)
    unreal.EditorAssetLibrary.save_asset(path)
    print("TREEFIX_OK", name)
