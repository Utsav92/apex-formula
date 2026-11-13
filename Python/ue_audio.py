"""Import the synthesised audio (Audio/gen_audio.py) as SoundWaves under /Game/Audio; loops are flagged looping.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_audio.py
"""
import os
import unreal

PROJECT = os.path.expanduser("~/Development/APEXFormula")
LOOPS = ["engine_low", "engine_high", "tyre_screech", "wind", "crowd", "city"]
ONESHOTS = ["gear_click", "thunder"]
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name in LOOPS + ONESHOTS:
    t = unreal.AssetImportTask()
    t.filename = os.path.join(PROJECT, "Audio", "export", name + ".wav")
    t.destination_path = "/Game/Audio"; t.destination_name = "SW_" + name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    w = unreal.EditorAssetLibrary.load_asset("/Game/Audio/SW_" + name)
    if not w:
        print("AUDIO_WARN", name, "failed"); continue
    w.set_editor_property("looping", name in LOOPS)
    unreal.EditorAssetLibrary.save_asset("/Game/Audio/SW_" + name)
    print("AUDIO_OK", name, "loop" if name in LOOPS else "oneshot")
