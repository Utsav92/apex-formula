import unreal
m = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_Grass")
m.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.recompile_material(m)
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_Grass")
print("GRASS_FLAG_OK")
