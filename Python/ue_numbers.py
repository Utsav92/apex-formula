"""Import the per-driver number roundels as /Game/Textures/Numbers/T_DecalNum_<n>."""
import os
import unreal
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
SRC = os.path.expanduser("~/Development/APEXFormula/Blender/export/textures")
ok = 0
for n in [27, 1, 3, 4, 5, 7, 8, 10, 11, 14, 16, 18, 20, 22, 23, 31, 44, 55, 63, 77]:
    p = os.path.join(SRC, f"T_DecalNum_{n}.png")
    if not os.path.exists(p):
        unreal.log(f"NUM_FAIL missing {p}"); continue
    t = unreal.AssetImportTask()
    t.filename = p; t.destination_path = "/Game/Textures/Numbers"; t.destination_name = f"T_DecalNum_{n}"
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t]); ok += 1
unreal.log(f"NUM_OK {ok} number textures")
