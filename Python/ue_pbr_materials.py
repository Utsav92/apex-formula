"""Import the scanned CC0 PBR sets (see Documentation/ASSET_MANIFEST.md) and rebuild the track materials with them.
Run AFTER ue_import.py and ue_materials.py (it replaces M_Asphalt / M_Barrier / M_Grass and adds M_Gravel).

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_pbr_materials.py -unattended -nullrhi -nosplash -stdout
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, eal, link, log, prop, tools, warnings  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
SRC = os.path.join(PROJECT, "Blender", "sources", "ambientcg")
TEXDIR = "/Game/Textures/PBR"
SETS = ["Asphalt031", "Concrete034", "Grass004", "Gravel040"]


def import_tex(path, name, kind):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = TEXDIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    t = eal.load_asset(f"{TEXDIR}/{name}")
    if not t:
        log("PBR_FAIL", f"{name} did not import")
        return None
    if kind == "color":
        t.set_editor_property("srgb", True)
    elif kind == "normal":
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:  # roughness / AO / masks
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    t.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    t.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    eal.save_asset(f"{TEXDIR}/{name}")
    return t


def import_set(sid):
    ok = 0
    # DirectX-style normals: Unreal expects green pointing down, which is what the NormalDX files are.
    for suffix, kind in (("Color", "color"), ("NormalDX", "normal"), ("Roughness", "mask"), ("AmbientOcclusion", "mask")):
        p = os.path.join(SRC, sid, f"{sid}_2K-JPG_{suffix}.jpg")
        if os.path.exists(p):
            ok += 1 if import_tex(p, f"T_{sid}_{suffix}", kind) else 0
    log("PBR_OK", f"{sid}: {ok} maps imported")
    return ok


for sid in SETS:
    import_set(sid)

# the marks mask comes from Blender/gen_textures.py
rm = os.path.join(PROJECT, "Blender", "export", "textures", "T_RoadMarks.png")
import_tex(rm, "T_RoadMarks", "mask")


def tp(sid, suffix):
    return f"{TEXDIR}/T_{sid}_{suffix}"


def remap_rough(g, rough, scale, offset):
    """Scanned roughness maps here run low (asphalt 0.52, grass 0.26 measured). Remap into the real-world range
    while keeping the scan's variation: r' = offset + scale * r."""
    return g.add(g.mul(rough, g.c(scale), "R", ""), g.c(offset))


def pbr_inputs(g, sid, uv, with_ao=True):
    col = g.tex(tp(sid, "Color"), uv)
    nrm = g.tex(tp(sid, "NormalDX"), uv, normal=True)
    rough = g.tex(tp(sid, "Roughness"), uv)
    ao = g.tex(tp(sid, "AmbientOcclusion"), uv) if with_ao and eal.does_asset_exist(tp(sid, "AmbientOcclusion")) else None
    return col, nrm, rough, ao


# ---- M_Asphalt: scanned asphalt + two scales + rubber + painted lines + wetness/puddles --------------------------
g = G("/Game/Materials", "M_Asphalt")
wet = g.s("Wetness", 0.0)
uv = g.tc()                                          # mesh UV: 0..1 across 14 m, one tile per 14 m along
fine = g.tc(4.67, 4.67)                              # ~3 m scan tile
macro = g.tc(1.13, 1.13)                             # second, larger scale so the scan tile does not visibly repeat
col, nrm, rough, ao = pbr_inputs(g, "Asphalt031", fine)
macro_col = g.tex(tp("Asphalt031", "Color"), macro)
variation = g.lerp(g.c(0.80), g.c(1.20), macro_col, "", "", "R")
base = g.mul(col, variation, "RGB", "")
marks = g.tex("/Game/Textures/PBR/T_RoadMarks", uv)
base = g.lerp(base, g.mul(base, g.c(0.55)), marks, "", "", "G")           # rubbered racing line
line_col = g.n(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.70, 0.70, 0.68, 1.0))
base = g.lerp(base, line_col, marks, "", "", "R")                          # painted edge lines
puddle_n = g.tex("/Game/Textures/T_CarDirt", g.tc(0.04, 0.04))
patch = g.sat(g.mul(g.sub(puddle_n, g.c(0.35), "R", ""), g.c(3.0)))       # where water pools
dark = g.lerp(g.c(1.0), g.c(0.45), g.mul(wet, g.lerp(g.c(0.6), g.c(1.0), patch)))
prop(g.mul(base, dark), "", unreal.MaterialProperty.MP_BASE_COLOR)
dry_rough = remap_rough(g, rough, 0.50, 0.55)
wet_rough = g.lerp(g.c(0.28), g.c(0.04), patch)                            # damp 0.28 -> puddle 0.04
prop(g.lerp(dry_rough, wet_rough, wet), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(nrm, "", unreal.MaterialProperty.MP_NORMAL)
if ao:
    prop(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("PBR_OK", "M_Asphalt (scanned base + marks + wetness)")

# ---- M_Barrier: concrete scan under the painted panels ------------------------------------------------------------
g = G("/Game/Materials", "M_Barrier")
uv = g.tc()
cuv = g.tc(4.0, 0.6)                                 # barrier UV: u = 8 m per unit, v = 1.2 m per unit -> ~2 m concrete tiles
col, nrm, rough, ao = pbr_inputs(g, "Concrete034", cuv, with_ao=False)
paint = g.tex("/Game/Textures/T_Barrier", uv)
shade = g.add(g.mul(col, g.c(0.9), "R", ""), g.c(0.55))
prop(g.mul(paint, shade, "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(remap_rough(g, rough, 0.50, 0.55), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(nrm, "", unreal.MaterialProperty.MP_NORMAL)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("PBR_OK", "M_Barrier (concrete scan + painted panels)")

# ---- M_Grass --------------------------------------------------------------------------------------------------------
g = G("/Game/Materials", "M_Grass")
guv = g.tc(6.0, 6.0)                                 # ground UV = 8 m per unit -> ~1.3 m scan tile
col, nrm, rough, ao = pbr_inputs(g, "Grass004", guv)
macro = g.tex("/Game/Textures/T_GrassMacro", g.tc(0.035, 0.035))
tint = g.v("GrassTint", (0.72, 0.95, 0.72))        # scan is dry yellow-green; push towards a healthy green
prop(g.mul(g.mul(col, macro, "RGB", "RGB"), tint), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(remap_rough(g, rough, 0.40, 0.78), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(nrm, "", unreal.MaterialProperty.MP_NORMAL)
if ao:
    prop(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("PBR_OK", "M_Grass (scanned grass x macro variation)")

# ---- M_Gravel (run-off / pit edges; placed by later geometry work) --------------------------------------------------
g = G("/Game/Materials", "M_Gravel")
guv = g.tc(8.0, 8.0)
col, nrm, rough, ao = pbr_inputs(g, "Gravel040", guv)
prop(col, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
prop(remap_rough(g, rough, 0.55, 0.60), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(nrm, "", unreal.MaterialProperty.MP_NORMAL)
if ao:
    prop(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("PBR_OK", "M_Gravel")
log("PBR_DONE", f"warnings={len(warnings)}")
