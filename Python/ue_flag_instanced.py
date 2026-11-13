"""Flag the garage/street-prop masters for instanced static meshes (HISM). Without the flag Unreal silently falls back to the default grid material."""
import unreal
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
for n in ("M_APEX_Led", "M_APEX_PitLight", "M_APEX_PitPlain", "M_APEX_BldMetal", "M_APEX_BldGlass", "M_APEX_BldSurface"):
    m = eal.load_asset(f"/Game/Materials/Buildings/{n}")
    if m:
        m.set_editor_property("used_with_instanced_static_meshes", True)
        m.set_editor_property("used_with_nanite", True)
        mel.recompile_material(m); eal.save_loaded_asset(m)
        print("FLAG_OK", n)
