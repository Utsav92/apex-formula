"""Create BP_APEX_LightingDirector and one DA_APEX_Lighting_<Name> data asset per preset.
The values come from the C++ table (AAPEXLightingDirector::GetBuiltinPreset), so there is one source of truth.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_lighting_assets.py -unattended -nullrhi -nosplash -stdout
"""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
ROOT = "/Game/Lighting"


def log(tag, msg):
    unreal.log(f"{tag} {msg}")


# Blueprint wrapper so artists can drop it in a level and edit the preset in the details panel.
bp_path = f"{ROOT}/BP_APEX_LightingDirector"
if eal.does_asset_exist(bp_path):
    eal.delete_asset(bp_path)
factory = unreal.BlueprintFactory()
factory.set_editor_property("parent_class", unreal.APEXLightingDirector)
bp = tools.create_asset("BP_APEX_LightingDirector", ROOT, unreal.Blueprint, factory)
if bp:
    eal.save_asset(bp_path)
    log("LIGHT_OK", f"created {bp_path}")
else:
    log("LIGHT_FAIL", "could not create the blueprint")

PRESETS = {
    "SunnyRace": unreal.APEXLightingPreset.SUNNY_RACE,
    "GoldenHour": unreal.APEXLightingPreset.GOLDEN_HOUR,
    "CloudyRace": unreal.APEXLightingPreset.CLOUDY_RACE,
    "RainyRace": unreal.APEXLightingPreset.RAINY_RACE,
    "NightRace": unreal.APEXLightingPreset.NIGHT_RACE,
}
for name, enum in PRESETS.items():
    path = f"{ROOT}/DA_APEX_Lighting_{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    f = unreal.DataAssetFactory()
    f.set_editor_property("data_asset_class", unreal.APEXLightingPresetAsset)
    da = tools.create_asset(f"DA_APEX_Lighting_{name}", ROOT, unreal.APEXLightingPresetAsset, f)
    data = unreal.APEXLightingDirector.get_builtin_preset(enum)
    da.set_editor_property("data", data)
    eal.save_asset(path)
    log("LIGHT_OK", f"{name}: lux={data.sun_lux:.0f} K={data.sun_temperature_k:.0f} elev={data.sun_elevation_deg:.1f} EV100={data.exposure_ev100:.1f}")
