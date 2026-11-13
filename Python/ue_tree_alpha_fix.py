"""Stop distant foliage going bare: preserve alpha coverage in the mips of every leaf/twig alpha mask, and keep Nanite on the tree meshes.

Masked leaf cards lose their leaves with distance because the alpha mips average below the material's clip value (0.3333).
Alpha-coverage preservation rescales each mip so the fraction of texels above the clip value matches the top mip.

  UnrealEditorCmd APEXFormula.uproject -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_tree_alpha_fix.py -unattended -nullrhi -nosplash -stdout
"""
import unreal

eal = unreal.EditorAssetLibrary
CLIP = 0.3333
n = 0
for path in eal.list_assets("/Game", recursive=True, include_folder=False):
    name = path.split("/")[-1].split(".")[0]
    if not (name.startswith("T_") and name.endswith("_alpha")):
        continue
    t = eal.load_asset(path)
    if not isinstance(t, unreal.Texture2D):
        continue
    t.set_editor_property("alpha_coverage_thresholds", unreal.Vector4(CLIP, 0.0, 0.0, 0.0))
    eal.save_loaded_asset(t)
    n += 1
    print("TREEALPHA_OK", name, "coverage threshold", CLIP)
for name in ("SM_Tree_Jacaranda", "SM_Tree_PineSapling", "SM_Tree_FirSapling"):
    p = f"/Game/Environments/Trees/{name}"
    m = eal.load_asset(p)
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset(p)
print("TREEALPHA_DONE", n, "textures")
