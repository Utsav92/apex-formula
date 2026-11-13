"""Sponsor system assets: textures, the two sponsor materials (static board and LED advertising), a material instance per sponsor and a
UAPEXSponsorDataAsset per entry of Sponsors/registry.json.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_sponsors.py        (after building the C++ module)
"""
import json
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, mel, prop, tools  # noqa: E402

ROOT = os.path.expanduser("~/Development/APEXFormula")
reg = json.load(open(os.path.join(ROOT, "Sponsors", "registry.json")))
MATS = "/Game/Sponsors/Materials"
TEXD = "/Game/Sponsors/Textures"
DATA = "/Game/Sponsors/Data"
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")

# ---- textures -----------------------------------------------------------------------------------------------------------------------------------
for sp in reg["sponsors"]:
    name = f"T_Sponsor_{sp['id']}"
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ROOT, "Sponsors", "textures", name + ".png")
    t.destination_path = TEXD; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex = eal.load_asset(f"{TEXD}/{name}")
    if not tex:
        log("SPN_WARN", f"{name} failed"); continue
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    eal.save_asset(f"{TEXD}/{name}")
log("SPN_OK", f"{len(reg['sponsors'])} textures")

first_tex = eal.load_asset(f"{TEXD}/T_Sponsor_{reg['sponsors'][0]['id']}")


def texparam(g, pname, default):
    n = g.n(ME.MaterialExpressionTextureSampleParameter2D)
    n.set_editor_property("parameter_name", pname)
    n.set_editor_property("texture", default)
    return n


# ---- M_APEX_SponsorBoard: printed vinyl on a rigid panel, readable at night through a faint lift (floodlight / ambient spill) -----------------------
g = G(MATS, "M_APEX_SponsorBoard")
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.mat.set_editor_property("used_with_nanite", True)
uv = g.tc(1.0, 1.0)
logo = texparam(g, "Logo", first_tex); link(uv, "", logo, "UVs")
prop(g.mul(logo, g.c(0.92), "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Roughness", 0.46), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.c(0.04), "", unreal.MaterialProperty.MP_METALLIC)
ng = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
prop(g.mul(g.mul(logo, g.mul(ng, g.s("NightLift", 5.0)), "RGB", ""), g.c(1.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.done()
log("SPN_OK", "M_APEX_SponsorBoard")

# ---- M_APEX_LED_Advertising ---------------------------------------------------------------------------------------------------------------------------
PIXEL = r"""
float2 p = UV * Grid;
float2 cell = floor(p);
float2 f = frac(p) - 0.5;
float d = length(f);
float mask = saturate((0.47 - d) * 6.0);                       // round LED dot inside each pixel cell
float2 q = (cell + 0.5) / Grid;                                 // every dot shows the colour at its own cell centre
float flick = 1.0 + 0.045 * sin(Time * Refresh * 6.2831 + cell.y * 0.31);   // slow refresh shimmer (scan)
return float4(q, mask, flick);
"""
g = G(MATS, "M_APEX_LED_Advertising")
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.mat.set_editor_property("used_with_nanite", True)
uv = g.tc(1.0, 1.0)
tm = g.n(ME.MaterialExpressionTime)
uvx = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(uv, "", uvx, "")
uvy = g.n(ME.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); link(uv, "", uvy, "")
fr = g.n(ME.MaterialExpressionFrac); link(g.add(uvx, g.mul(tm, g.s("ScrollSpeed", 0.0))), "", fr, "")      # optional sideways scroll, wraps
uv2 = g.n(ME.MaterialExpressionAppendVector); link(fr, "", uv2, "A"); link(uvy, "", uv2, "B")
gx = g.s("GridX", 160.0)
grid = g.n(ME.MaterialExpressionAppendVector); link(gx, "", grid, "A"); link(g.mul(gx, g.c(0.25)), "", grid, "B")   # 4:1 panel -> square pixels
c = g.n(ME.MaterialExpressionCustom)
c.set_editor_property("code", PIXEL)
c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
c.set_editor_property("description", "LEDPixels")
ins = []
for n in ("UV", "Grid", "Time", "Refresh"):
    i_ = unreal.CustomInput(); i_.set_editor_property("input_name", n); ins.append(i_)
c.set_editor_property("inputs", ins)
link(uv2, "", c, "UV"); link(grid, "", c, "Grid"); link(tm, "", c, "Time"); link(g.s("RefreshRate", 0.6), "", c, "Refresh")
q = g.n(ME.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False); link(c, "", q, "")
mk = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(c, "", mk, "")
fl = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); link(c, "", fl, "")
ta = texparam(g, "TexA", first_tex); link(q, "", ta, "UVs")
tb = texparam(g, "TexB", first_tex); link(q, "", tb, "UVs")
# The dot UVs jump from cell to cell, so screen-space derivatives are huge and automatic mip selection picks the blurriest level (smeared, wavy text).
# Pick the mip explicitly: level 2 averages ~4x4 texels, which is about one LED cell of a 1024-wide wordmark on a 160-dot panel.
for t_ in (ta, tb):
    t_.set_editor_property("mip_value_mode", unreal.TextureMipValueMode.TMVM_MIP_LEVEL)
    t_.set_editor_property("const_mip_value", 2)
colour = g.lerp(ta, tb, g.sat(g.s("Blend", 0.0)))
ng = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
# Exposure-aware brightness, calibrated like the other lit props: a sunlit track is exposed ~11 stops darker than a night street, so a panel that reads
# in daylight would blind at night. Luminance falls from DayLuminance to NightLuminance as NightGlow goes to 1 and Brightness trims it per board.
lum = g.lerp(g.s("DayLuminance", 2600.0), g.s("NightLuminance", 55.0), ng)
emissive = g.mul(g.mul(g.mul(colour, mk), g.mul(lum, g.s("Brightness", 1.0))), fl)
prop(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.35), "", unreal.MaterialProperty.MP_ROUGHNESS)
g.done()
log("SPN_OK", "M_APEX_LED_Advertising")

# ---- material instances + data assets -----------------------------------------------------------------------------------------------------------------
def mi(name, parent_name, tex_params):
    full = f"{MATS}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    m = tools.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    m.set_editor_property("parent", eal.load_asset(f"{MATS}/{parent_name}"))
    for k, v in tex_params.items():
        mel.set_material_instance_texture_parameter_value(m, k, v)
    mel.update_material_instance(m)
    eal.save_asset(full)
    return m


FORMAT = {"Static": unreal.APEXSponsorFormat.STATIC, "LED": unreal.APEXSponsorFormat.LED, "Banner": unreal.APEXSponsorFormat.BANNER}
LICENSE = {"OriginalFictional": unreal.APEXSponsorLicense.ORIGINAL_FICTIONAL, "Licensed": unreal.APEXSponsorLicense.LICENSED,
           "UnlicensedPlaceholder": unreal.APEXSponsorLicense.UNLICENSED_PLACEHOLDER}
for sp in reg["sponsors"]:
    sid = sp["id"]
    tex = eal.load_asset(f"{TEXD}/T_Sponsor_{sid}")
    mi(f"MI_SponsorStatic_{sid}", "M_APEX_SponsorBoard", {"Logo": tex})
    mi(f"MI_SponsorLED_{sid}", "M_APEX_LED_Advertising", {"TexA": tex, "TexB": tex})
    name = f"DA_Sponsor_{sid}"
    full = f"{DATA}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    da = tools.create_asset(name, DATA, unreal.APEXSponsorDataAsset, unreal.DataAssetFactory())
    da.set_editor_property("sponsor_id", sid)
    da.set_editor_property("sponsor_name", sp["name"])
    da.set_editor_property("logo", tex)
    da.set_editor_property("static_material", eal.load_asset(f"{MATS}/MI_SponsorStatic_{sid}"))
    da.set_editor_property("led_material", eal.load_asset(f"{MATS}/MI_SponsorLED_{sid}"))
    da.set_editor_property("preferred_format", FORMAT[sp["format"]])
    da.set_editor_property("background_color", unreal.LinearColor(*sp["bg"], 1.0))
    da.set_editor_property("text_color", unreal.LinearColor(*sp["fg"], 1.0))
    da.set_editor_property("fade_seconds", sp["fade"])
    da.set_editor_property("scroll_speed", sp["scroll"])
    da.set_editor_property("emissive_scale", sp["emissive"])
    da.set_editor_property("placement_categories", [unreal.Name(c) for c in sp["category"]])
    da.set_editor_property("asset_source", sp["source"])
    da.set_editor_property("license", LICENSE[sp["license"]])
    da.set_editor_property("fallback_sponsor_id", sp["fallback"])
    eal.save_asset(full)
    log("SPN_OK", f"{name} ({sp['license']}, fallback '{sp['fallback']}')")
