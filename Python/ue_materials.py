"""APEX master materials: car paint (cinematic + mobile), carbon, tyre, brake disc, metal, glass, decals,
wet-capable asphalt, and the procedural mobile sky dome. Run after ue_import.py (needs the textures).

  ~/unreal/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd ~/Development/APEXFormula/APEXFormula.uproject \
     -ExecutePythonScript=~/Development/APEXFormula/Python/ue_materials.py -unattended -nullrhi -nosplash -stdout

Idempotent. Prints MAT_OK / MAT_WARN lines. A MAT_WARN means a graph connection failed.
"""
import os

import unreal

PROJECT = os.path.expanduser("~/Development/APEXFormula")
TEXDIR = os.path.join(PROJECT, "Blender", "export", "textures")
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
ME = unreal  # shorthand for expression classes

CAR = "/Game/Materials/Car"
SKY = "/Game/Materials/Sky"
warnings = []


def log(tag, msg):
    unreal.log(f"{tag} {msg}")


def link(a, a_pin, b, b_pin):
    ok = mel.connect_material_expressions(a, a_pin, b, b_pin)
    if not ok:
        warnings.append(f"{type(a).__name__}.{a_pin or '*'} -> {type(b).__name__}.{b_pin or '*'}")
        log("MAT_WARN", f"connect failed: {type(a).__name__}.{a_pin or '*'} -> {type(b).__name__}.{b_pin or '*'}")
    return ok


def prop(node, pin, mat_prop):
    ok = mel.connect_material_property(node, pin, mat_prop)
    if not ok:
        warnings.append(f"property {mat_prop}")
        log("MAT_WARN", f"property connect failed: {mat_prop}")


class G:
    """Tiny graph builder: keeps a material and a running layout position."""

    def __init__(self, path, name, instanced=False):
        full = f"{path}/{name}"
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        self.mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
        self.full, self.name = full, name
        self.x, self.y = -900, 0
        if instanced:
            self.mat.set_editor_property("used_with_instanced_static_meshes", True)

    def n(self, cls, **props):
        e = mel.create_material_expression(self.mat, cls, self.x, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def s(self, name, default):
        return self.n(ME.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def v(self, name, rgb):
        return self.n(ME.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0))

    def c(self, value):
        return self.n(ME.MaterialExpressionConstant, r=value)

    def tex(self, tex_path, uv=None, normal=False):
        e = self.n(ME.MaterialExpressionTextureSample)
        e.set_editor_property("texture", eal.load_asset(tex_path))
        if normal:
            e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if uv is not None:
            link(uv, "", e, "UVs")
        return e

    def mul(self, a, b, a_pin="", b_pin=""):
        m = self.n(ME.MaterialExpressionMultiply)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B")
        return m

    def lerp(self, a, b, alpha, a_pin="", b_pin="", alpha_pin=""):
        m = self.n(ME.MaterialExpressionLinearInterpolate)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B"); link(alpha, alpha_pin, m, "Alpha")
        return m

    def one_minus(self, a, pin=""):
        m = self.n(ME.MaterialExpressionOneMinus)
        link(a, pin, m, "")
        return m

    def sat(self, a, pin=""):
        m = self.n(ME.MaterialExpressionSaturate)
        link(a, pin, m, "")
        return m

    def sub(self, a, b, a_pin="", b_pin=""):
        m = self.n(ME.MaterialExpressionSubtract)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B")
        return m

    def mx(self, a, b):
        m = self.n(ME.MaterialExpressionMax)
        link(a, "", m, "A"); link(b, "", m, "B")
        return m

    def uvs(self, tiling=None):
        tc = self.n(ME.MaterialExpressionTextureCoordinate)
        if tiling is None:
            return tc
        return self.mul(tc, tiling)

    def done(self):
        mel.recompile_material(self.mat)
        eal.save_asset(self.full)
        return self.mat


def make_instance(path, name, parent, scalars=None, vectors=None, textures=None):
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    for k, val in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, val)
    for k, val in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*val, 1.0))
    for k, val in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, eal.load_asset(val))
    mel.update_material_instance(mi)
    eal.save_asset(full)
    return mi


# ---- data textures (normal map + masks) ---------------------------------------------------------

def import_data_texture(name, normal):
    src = os.path.join(TEXDIR, name + ".png")
    if not os.path.exists(src):
        log("MAT_WARN", f"{name}.png missing (run Blender/gen_textures.py)")
        return
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = "/Game/Textures"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    t = eal.load_asset(f"/Game/Textures/{name}")
    t.set_editor_property("srgb", False)
    t.set_editor_property("compression_settings",
                          unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    t.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    eal.save_asset(f"/Game/Textures/{name}")


import_data_texture("T_FlakeNormal", True)
import_data_texture("T_CarDirt", False)
import_data_texture("T_CarScratch", False)

T_FLAKE, T_DIRT, T_SCRATCH = "/Game/Textures/T_FlakeNormal", "/Game/Textures/T_CarDirt", "/Game/Textures/T_CarScratch"

# ---- M_APEX_CarPaint (cinematic) ----------------------------------------------------------------
g = G(CAR, "M_APEX_CarPaint")
g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
base, metal, rough = g.v("BaseColor", (0.75, 0.02, 0.03)), g.s("Metallic", 0.45), g.s("Roughness", 0.16)
flake_amt, flake_tile, normal_detail = g.s("FlakeAmount", 0.5), g.s("FlakeTiling", 18.0), g.s("NormalDetail", 0.6)
dirt_amt, scratch_amt, wear_amt = g.s("DirtAmount", 0.0), g.s("ScratchAmount", 0.0), g.s("WearAmount", 0.0)
cc, cc_rough = g.s("ClearCoat", 1.0), g.s("ClearCoatRoughness", 0.03)
dirt_col, primer_col = g.v("DirtColor", (0.16, 0.13, 0.10)), g.v("PrimerColor", (0.20, 0.20, 0.21))
scratch_col = g.c(0.55)

uv = g.uvs()
flake = g.tex(T_FLAKE, g.mul(uv, flake_tile), normal=True)
# FlattenNormal is not exposed to Python, so blend between a flat normal and the flake normal instead.
flat_n = g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
nrm = g.lerp(flat_n, flake, g.mul(normal_detail, flake_amt), "", "RGB", "")

dirt_tex = g.tex(T_DIRT, g.mul(uv, g.c(0.8)))
dirt_a = g.mul(g.mul(dirt_tex, dirt_amt, "R", ""), g.c(0.85))
scratch_tex = g.tex(T_SCRATCH, g.mul(uv, g.c(1.5)))
scratch_a = g.mul(g.mul(scratch_tex, scratch_amt, "R", ""), g.c(0.9))
wear_tex = g.tex(T_DIRT, g.mul(uv, g.c(2.7)))
wear_a = g.sat(g.mul(g.sub(wear_tex, g.one_minus(wear_amt), "R", ""), g.c(6.0)))

col = g.lerp(base, dirt_col, dirt_a)
col = g.lerp(col, scratch_col, scratch_a)
col = g.lerp(col, primer_col, wear_a)
r = g.lerp(rough, g.c(0.85), dirt_a)
r = g.lerp(r, g.c(0.45), scratch_a)
r = g.lerp(r, g.c(0.60), wear_a)
m = g.lerp(g.lerp(metal, g.c(0.0), dirt_a), g.c(0.0), wear_a)
cc_final = g.mul(cc, g.one_minus(g.mx(scratch_a, wear_a)))
# Clear coat pins are not in Python's MaterialProperty enum, so go through Material Attributes.
g.mat.set_editor_property("use_material_attributes", True)
ma = g.n(ME.MaterialExpressionMakeMaterialAttributes)
for pin, node in (("BaseColor", col), ("Metallic", m), ("Roughness", r), ("Normal", nrm),
                  ("ClearCoat", cc_final), ("ClearCoatRoughness", cc_rough)):
    link(node, "", ma, pin)
prop(ma, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
paint_master = g.done()

# ---- M_APEX_CarPaint_Mobile (default-lit: no clear coat, no dirt/scratch/wear layers) -----------
g = G(CAR, "M_APEX_CarPaint_Mobile")
base, metal, rough = g.v("BaseColor", (0.75, 0.02, 0.03)), g.s("Metallic", 0.45), g.s("Roughness", 0.16)
flake_amt, flake_tile = g.s("FlakeAmount", 0.5), g.s("FlakeTiling", 18.0)
flake = g.tex(T_FLAKE, g.mul(g.uvs(), flake_tile), normal=True)
flat_n = g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
prop(g.lerp(flat_n, flake, g.mul(g.c(0.6), flake_amt), "", "RGB", ""), "", unreal.MaterialProperty.MP_NORMAL)
prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(metal, "", unreal.MaterialProperty.MP_METALLIC)
prop(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
paint_mobile = g.done()

PAINTS = {
    "GlossyRed":     dict(BaseColor=(0.75, 0.02, 0.03), Metallic=0.45, Roughness=0.16, FlakeAmount=0.5),
    "MetallicBlue":  dict(BaseColor=(0.02, 0.08, 0.45), Metallic=0.85, Roughness=0.22, FlakeAmount=0.9),
    "MatteBlack":    dict(BaseColor=(0.012, 0.012, 0.014), Metallic=0.10, Roughness=0.55, FlakeAmount=0.0),
    "WhitePearl":    dict(BaseColor=(0.88, 0.88, 0.86), Metallic=0.25, Roughness=0.15, FlakeAmount=0.35),
    "OrangeRacing":  dict(BaseColor=(1.00, 0.27, 0.02), Metallic=0.40, Roughness=0.17, FlakeAmount=0.55),
}
for pname, p in PAINTS.items():
    scal = {k: v for k, v in p.items() if k != "BaseColor"}
    cin = dict(scal)
    if pname == "MatteBlack":
        cin["ClearCoat"] = 0.0           # matte: no lacquer layer
    make_instance(CAR, f"MI_APEX_Paint_{pname}", paint_master, cin, {"BaseColor": p["BaseColor"]})
    make_instance(CAR, f"MI_APEX_Paint_{pname}_Mobile", paint_mobile, scal, {"BaseColor": p["BaseColor"]})
# Showcase of the wear layers: dirt, scratches and paint wear on the red.
make_instance(CAR, "MI_APEX_Paint_GlossyRed_Worn", paint_master,
              {"Metallic": 0.45, "Roughness": 0.2, "FlakeAmount": 0.5, "DirtAmount": 0.6, "ScratchAmount": 0.7, "WearAmount": 0.35},
              {"BaseColor": PAINTS["GlossyRed"]["BaseColor"]})
log("MAT_OK", "car paint master + mobile + 5 colours (+worn showcase)")

# ---- M_APEX_CarbonFiber ------------------------------------------------------------------------
g = G(CAR, "M_APEX_CarbonFiber")
g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLEAR_COAT)
g.mat.set_editor_property("use_material_attributes", True)
weave = g.tex("/Game/Textures/T_Carbon", g.mul(g.uvs(), g.s("WeaveTiling", 1.0)))
ma = g.n(ME.MaterialExpressionMakeMaterialAttributes)
link(g.mul(weave, g.v("Tint", (1.0, 1.0, 1.0)), "RGB", ""), "", ma, "BaseColor")
link(g.s("Metallic", 0.35), "", ma, "Metallic")
link(g.s("Roughness", 0.30), "", ma, "Roughness")
link(g.c(1.0), "", ma, "ClearCoat")
link(g.s("ClearCoatRoughness", 0.04), "", ma, "ClearCoatRoughness")
prop(ma, "", unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
carbon = g.done()
make_instance(CAR, "MI_APEX_CarbonFiber", carbon)

# ---- M_APEX_TireRubber -------------------------------------------------------------------------
g = G(CAR, "M_APEX_TireRubber")
side = g.tex("/Game/Textures/T_TyreSidewall", g.uvs())
prop(side, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Roughness", 0.86), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
tyre = g.done()
make_instance(CAR, "MI_APEX_TireRubber", tyre)

# ---- M_APEX_BrakeDisc (BrakeTemp 0..1 glows towards orange) ----------------------------------
g = G(CAR, "M_APEX_BrakeDisc")
prop(g.v("BaseColor", (0.40, 0.40, 0.42)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(1.0), "", unreal.MaterialProperty.MP_METALLIC)
prop(g.s("Roughness", 0.35), "", unreal.MaterialProperty.MP_ROUGHNESS)
heat = g.mul(g.mul(g.v("HeatColor", (1.0, 0.25, 0.03)), g.s("BrakeTemp", 0.0)), g.s("HeatLuminance", 400.0))
prop(heat, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
disc = g.done()
make_instance(CAR, "MI_APEX_BrakeDisc", disc)

# ---- M_APEX_Metal ------------------------------------------------------------------------------
g = G(CAR, "M_APEX_Metal")
prop(g.v("BaseColor", (0.55, 0.56, 0.58)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Metallic", 1.0), "", unreal.MaterialProperty.MP_METALLIC)
prop(g.s("Roughness", 0.35), "", unreal.MaterialProperty.MP_ROUGHNESS)
metal_m = g.done()
make_instance(CAR, "MI_APEX_Metal", metal_m)

# ---- M_APEX_Glass (translucent visor/canopy) --------------------------------------------------
g = G(CAR, "M_APEX_Glass")
g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
prop(g.v("BaseColor", (0.02, 0.03, 0.05)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Roughness", 0.05), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.s("Specular", 0.8), "", unreal.MaterialProperty.MP_SPECULAR)
prop(g.s("Opacity", 0.35), "", unreal.MaterialProperty.MP_OPACITY)
glass = g.done()
make_instance(CAR, "MI_APEX_Glass", glass)

# ---- M_APEX_RacingDecals (masked; texture chosen per instance) --------------------------------
g = G(CAR, "M_APEX_RacingDecals")
g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
dtex = g.n(ME.MaterialExpressionTextureSampleParameter2D, parameter_name="DecalTex")
dtex.set_editor_property("texture", eal.load_asset("/Game/Textures/T_DecalApex"))
prop(g.mul(dtex, g.v("Tint", (1.0, 1.0, 1.0)), "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(dtex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
prop(g.s("Roughness", 0.40), "", unreal.MaterialProperty.MP_ROUGHNESS)
decals = g.done()
make_instance(CAR, "MI_APEX_Decal_Apex", decals, textures={"DecalTex": "/Game/Textures/T_DecalApex"})
make_instance(CAR, "MI_APEX_Decal_Number", decals, textures={"DecalTex": "/Game/Textures/T_DecalNumber"})
log("MAT_OK", "carbon, tyre, brake disc, metal, glass, decals + instances")

# ---- M_Asphalt with wetness (replaces the dry-only one from ue_import.py) ---------------------
g = G("/Game/Materials", "M_Asphalt")
wet = g.s("Wetness", 0.0)
asph = g.tex("/Game/Textures/T_Asphalt", g.uvs())
puddle_tc = g.n(ME.MaterialExpressionTextureCoordinate, u_tiling=0.04, v_tiling=0.04)
puddle_n = g.tex(T_DIRT, puddle_tc)
patch = g.sat(g.mul(g.sub(puddle_n, g.c(0.35), "R", ""), g.c(3.0)))     # where water pools
# Darker when wet (more so in puddles), and much glossier: dry 0.82 -> 0.28 damp -> 0.04 puddle (spec: 0.04-0.30).
dark = g.lerp(g.c(1.0), g.c(0.45), g.mul(wet, g.lerp(g.c(0.6), g.c(1.0), patch)))
prop(g.mul(asph, dark, "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
wet_rough = g.lerp(g.c(0.28), g.c(0.04), patch)
prop(g.lerp(g.c(0.82), wet_rough, wet), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
g.done()
log("MAT_OK", "M_Asphalt with Wetness parameter")

# ---- M_APEX_SkyDome (unlit gradient + sun glow; the mobile replacement for Sky Atmosphere) ----
g = G(SKY, "M_APEX_SkyDome")
g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
g.mat.set_editor_property("two_sided", True)
dirn = g.n(ME.MaterialExpressionNormalize)
link(g.sub(g.n(ME.MaterialExpressionWorldPosition), g.n(ME.MaterialExpressionCameraPositionWS)), "", dirn, "")
zmask = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
link(dirn, "", zmask, "")
height = g.n(ME.MaterialExpressionPower)
link(g.sat(zmask), "", height, "Base"); link(g.c(0.55), "", height, "Exp")
sky = g.lerp(g.v("Horizon", (0.55, 0.7, 0.9)), g.v("Zenith", (0.08, 0.22, 0.6)), height)
sun_dir = g.v("SunDir", (0.0, 0.0, 1.0))
dot = g.n(ME.MaterialExpressionDotProduct)
link(dirn, "", dot, "A"); link(sun_dir, "", dot, "B")
glow = g.n(ME.MaterialExpressionPower)
link(g.sat(dot), "", glow, "Base"); link(g.c(96.0), "", glow, "Exp")
sun_col = g.mul(g.mul(glow, g.s("SunGlow", 1.0)), g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.9, 0.7, 1.0)))
total = g.n(ME.MaterialExpressionAdd)
link(sky, "", total, "A"); link(g.mul(sun_col, g.c(4.0)), "", total, "B")
# Emissive units are cd/m2. With manual exposure the scene needs ~0.125 * 2^EV, which the director sets per preset.
prop(g.mul(total, g.s("Luminance", 4096.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.done()
log("MAT_OK", "M_APEX_SkyDome")

log("MAT_DONE", f"warnings={len(warnings)}")
