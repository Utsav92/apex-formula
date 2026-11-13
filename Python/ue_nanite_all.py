"""Turn Nanite on for the static environment meshes and make their materials Nanite-compatible.
(The foliage masters already carry the flag; this covers track, buildings, grandstand and the PBR materials.)
A material without the Nanite usage flag silently falls back to the default material, so flag them all.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_nanite_all.py -unattended -nullrhi -nosplash -stdout
"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
MESHES = ["/Game/Tracks/SM_Track", "/Game/Environments/SM_Bld_Glass", "/Game/Environments/SM_Bld_Block",
          "/Game/Environments/SM_Bld_Brick", "/Game/Environments/SM_Bld_Slim", "/Game/Environments/SM_Grandstand"]


def flag(mat):
    base = mat
    while isinstance(base, unreal.MaterialInstanceConstant):       # flags live on the parent material
        base = base.get_editor_property("parent")
    if isinstance(base, unreal.Material) and not base.get_editor_property("used_with_nanite"):
        base.set_editor_property("used_with_nanite", True)
        mel.recompile_material(base)
        eal.save_loaded_asset(base)
        return 1
    return 0


done = flagged = 0
for path in MESHES:
    m = eal.load_asset(path)
    if not m:
        unreal.log(f"NANITE_WARN {path} missing")
        continue
    ns = m.get_editor_property("nanite_settings")
    ns.enabled = True
    m.set_editor_property("nanite_settings", ns)
    for s in m.get_editor_property("static_materials"):
        mat = s.get_editor_property("material_interface")
        if mat:
            flagged += flag(mat)
    eal.save_loaded_asset(m)
    done += 1
    unreal.log(f"NANITE_OK {path}")
unreal.log(f"NANITE_DONE meshes={done} materials_flagged={flagged}")
