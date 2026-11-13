"""Import the detailed buildings (Blender/gen_buildings.py), build their scanned-PBR materials and glass, and enable Nanite.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_buildings.py -unattended -nullrhi -nosplash -stdout
Prints BLD_OK / BLD_WARN lines.
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, mel, prop, tools, warnings  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
SRC = os.path.join(PROJECT, "Blender", "sources", "ambientcg")
EXPORT = os.path.join(PROJECT, "Blender", "export")
TEX = "/Game/Textures/Buildings"
MATS = "/Game/Materials/Buildings"
SETS = ["PaintedPlaster017", "Bricks105", "RoofingTiles013A", "WoodSiding008", "Concrete036", "Concrete034", "CorrugatedSteel009"]


def import_tex(path, name, kind):
    t = unreal.AssetImportTask()
    t.filename = path; t.destination_path = TEX; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    a = eal.load_asset(f"{TEX}/{name}")
    if not a:
        log("BLD_WARN", f"{name} did not import"); return None
    a.set_editor_property("srgb", kind == "color")
    if kind == "normal":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    a.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    eal.save_asset(f"{TEX}/{name}")
    return f"{TEX}/{name}"


TP = {}   # TP[(set, map)] -> asset path
for sid in SETS:
    for suffix, kind in (("Color", "color"), ("NormalDX", "normal"), ("Roughness", "mask"), ("AmbientOcclusion", "mask")):
        p = os.path.join(SRC, sid, f"{sid}_2K-JPG_{suffix}.jpg")
        if os.path.exists(p):
            TP[(sid, suffix)] = import_tex(p, f"T_{sid}_{suffix}", kind)
log("BLD_OK", f"{len(TP)} PBR maps imported")

# ---- shared parameters (night glow, wetness) ----------------------------------------------------------------------------
mpc_path = "/Game/Materials/MPC_APEX_World"
if eal.does_asset_exist(mpc_path):
    eal.delete_asset(mpc_path)
mpc = tools.create_asset("MPC_APEX_World", "/Game/Materials", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
params = []
for name in ("NightGlow", "Wetness"):
    sp = unreal.CollectionScalarParameter()
    sp.set_editor_property("parameter_name", name)
    sp.set_editor_property("default_value", 0.0)
    params.append(sp)
mpc.set_editor_property("scalar_parameters", params)
eal.save_asset(mpc_path)


def tex_param(g, name, normal=False, default=None):
    e = g.n(ME.MaterialExpressionTextureSampleParameter2D, parameter_name=name)
    if default:
        e.set_editor_property("texture", eal.load_asset(default))
    if normal:
        e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    return e


def mpc_param(g, name):
    return g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name=name)


# ---- M_APEX_BldSurface: PBR + street-level grime + rain darkening ---------------------------------------------------------
g = G(MATS, "M_APEX_BldSurface", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
d = tex_param(g, "Diffuse", default=TP[("PaintedPlaster017", "Color")])
n = tex_param(g, "Normal", True, TP[("PaintedPlaster017", "NormalDX")])
r = tex_param(g, "Roughness", default=TP[("PaintedPlaster017", "Roughness")])
ao = tex_param(g, "AO", default=TP[("Bricks105", "AmbientOcclusion")])
tint = g.v("Tint", (1.0, 1.0, 1.0))
pos, obj = g.n(ME.MaterialExpressionWorldPosition), g.n(ME.MaterialExpressionObjectPositionWS)
rel = g.sub(pos, obj)
zm = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(rel, "", zm, "")
height_m = g.mul(zm, g.c(0.01))
grime = g.mul(g.sat(g.sub(g.c(1.0), g.mul(height_m, g.c(0.4)))), g.c(0.30))              # up to 30 % darker at street level, fading by ~2.5 m
wet = mpc_param(g, "Wetness")
base = g.mul(g.mul(d, tint, "RGB", ""), g.sub(g.c(1.0), grime))
base = g.mul(base, g.lerp(g.c(1.0), g.c(0.62), wet))                                       # rain darkens masonry
prop(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
rough = g.add(g.mul(r, g.s("RoughScale", 0.5), "R", ""), g.s("RoughOffset", 0.5))
prop(g.lerp(rough, g.c(0.35), g.mul(wet, g.c(0.6))), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(n, "", unreal.MaterialProperty.MP_NORMAL)
prop(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
prop(g.s("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
surface = g.done()


def surface_mi(name, set_id, tint=(1, 1, 1), rough_scale=0.5, rough_offset=0.5, metallic=0.0, ao_set=None):
    full = f"{MATS}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", surface)
    for param, suffix in (("Diffuse", "Color"), ("Normal", "NormalDX"), ("Roughness", "Roughness")):
        if (set_id, suffix) in TP:
            mel.set_material_instance_texture_parameter_value(mi, param, eal.load_asset(TP[(set_id, suffix)]))
    ao_key = (ao_set or set_id, "AmbientOcclusion")
    if ao_key in TP:
        mel.set_material_instance_texture_parameter_value(mi, "AO", eal.load_asset(TP[ao_key]))
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "RoughScale", rough_scale)
    mel.set_material_instance_scalar_parameter_value(mi, "RoughOffset", rough_offset)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    mel.update_material_instance(mi)
    eal.save_asset(full)
    return mi


# plaster in three colours, brick in two, concrete in two, tile, wood, steel
surface_mi("MI_Bld_Plaster_Ochre", "PaintedPlaster017", (1.00, 0.80, 0.52))
surface_mi("MI_Bld_Plaster_White", "PaintedPlaster017", (1.05, 1.03, 0.98))
surface_mi("MI_Bld_Brick_Red", "Bricks105", (1.0, 0.85, 0.80))
surface_mi("MI_Bld_Brick_Brown", "Bricks105", (0.75, 0.62, 0.52))
surface_mi("MI_Bld_Concrete_Grey", "Concrete036", (0.95, 0.95, 0.95))
surface_mi("MI_Bld_Trim_Cream", "Concrete034", (1.15, 1.10, 0.95))
surface_mi("MI_Bld_Trim_Grey", "Concrete034", (0.85, 0.85, 0.87))
surface_mi("MI_Bld_Roof_Terracotta", "RoofingTiles013A", (1.0, 0.75, 0.62), rough_scale=0.4, rough_offset=0.55)
surface_mi("MI_Bld_Wood", "WoodSiding008", (1.0, 1.0, 1.0), rough_scale=0.4, rough_offset=0.45)
surface_mi("MI_Bld_Shutter_Green", "WoodSiding008", (0.25, 0.42, 0.30), rough_scale=0.4, rough_offset=0.45)
surface_mi("MI_Bld_Steel_Roof", "CorrugatedSteel009", (0.8, 0.82, 0.85), rough_scale=0.4, rough_offset=0.35, metallic=0.7)
log("BLD_OK", "surface master + 11 instances")

# ---- glass: reflective, random warm lit windows at night --------------------------------------------------------------
g = G(MATS, "M_APEX_BldGlass", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
tint = g.v("Tint", (0.03, 0.06, 0.09))
prop(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Roughness", 0.06), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.s("Specular", 0.9), "", unreal.MaterialProperty.MP_SPECULAR)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
pos = g.n(ME.MaterialExpressionWorldPosition)
cx_ = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(pos, "", cx_, "")
cy_ = g.n(ME.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); link(pos, "", cy_, "")
cz_ = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(pos, "", cz_, "")
def floor_of(node, size):
    f = g.n(ME.MaterialExpressionFloor); link(g.mul(node, g.c(1.0 / size)), "", f, ""); return f
cell = g.add(g.add(g.mul(floor_of(cz_, 330.0), g.c(7.13)), g.mul(floor_of(cx_, 340.0), g.c(3.77))), g.mul(floor_of(cy_, 340.0), g.c(5.31)))
sn = g.n(ME.MaterialExpressionSine); link(g.mul(cell, g.c(12.9898)), "", sn, "")
hash_ = g.n(ME.MaterialExpressionFrac); link(g.mul(sn, g.c(43758.5453)), "", hash_, "")
lit = g.sat(g.mul(g.sub(hash_, g.c(0.62)), g.c(50.0)))                                      # ~38 % of window cells lit
warm = g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.72, 0.38, 1.0))
glow = g.mul(g.mul(g.mul(warm, lit), mpc_param(g, "NightGlow")), g.s("EmissiveLuminance", 30.0))
prop(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
glass_master = g.done()
for name, col in (("MI_Bld_Glass", (0.03, 0.06, 0.09)), ("MI_Bld_GlassB", (0.025, 0.07, 0.06)), ("MI_Bld_GlassC", (0.015, 0.016, 0.02))):
    full = f"{MATS}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", glass_master)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*col, 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(full)

# ---- metal for frames, railings, plant ----------------------------------------------------------------------------------
g = G(MATS, "M_APEX_BldMetal", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
prop(g.v("Tint", (0.07, 0.075, 0.08)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.c(0.85), "", unreal.MaterialProperty.MP_METALLIC)
prop(g.s("Roughness", 0.40), "", unreal.MaterialProperty.MP_ROUGHNESS)
metal_master = g.done()
full = f"{MATS}/MI_Bld_Metal"
if eal.does_asset_exist(full):
    eal.delete_asset(full)
mi = tools.create_asset("MI_Bld_Metal", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mi.set_editor_property("parent", metal_master)
eal.save_asset(full)
log("BLD_OK", "glass x3 + metal")

# ---- meshes --------------------------------------------------------------------------------------------------------------
M = lambda n: eal.load_asset(f"{MATS}/{n}")  # noqa: E731
GLASS = {"Glass": "MI_Bld_Glass", "GlassB": "MI_Bld_GlassB", "GlassC": "MI_Bld_GlassC", "Metal": "MI_Bld_Metal"}
BUILDINGS = {
    "Hotel_Med": dict(Wall="MI_Bld_Plaster_Ochre", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Roof_Terracotta", Wood="MI_Bld_Shutter_Green"),
    "Apt_BrickA": dict(Wall="MI_Bld_Brick_Red", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Trim_Grey", Wood="MI_Bld_Wood"),
    "Apt_BrickB": dict(Wall="MI_Bld_Brick_Brown", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Trim_Grey", Wood="MI_Bld_Wood"),
    "Office_Concrete": dict(Wall="MI_Bld_Concrete_Grey", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Trim_Grey", Wood="MI_Bld_Wood"),
    "Tower_GlassA": dict(Wall="MI_Bld_Concrete_Grey", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Trim_Grey", Wood="MI_Bld_Wood"),
    "Tower_GlassB": dict(Wall="MI_Bld_Concrete_Grey", Trim="MI_Bld_Trim_Grey", Roof="MI_Bld_Trim_Grey", Wood="MI_Bld_Wood"),
    "Lodge_Alpine": dict(Wall="MI_Bld_Plaster_White", Trim="MI_Bld_Trim_Cream", Roof="MI_Bld_Steel_Roof", Wood="MI_Bld_Wood"),
}
for name, slots in BUILDINGS.items():
    src = os.path.join(EXPORT, f"Bld_{name}.fbx")
    if not os.path.exists(src):
        log("BLD_WARN", f"{src} missing"); continue
    t = unreal.AssetImportTask()
    t.filename = src; t.destination_path = "/Game/Environments/Buildings"; t.destination_name = f"SM_Bld_{name}"
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    d = ui.static_mesh_import_data
    d.combine_meshes = True
    d.set_editor_property("build_nanite", True)
    d.auto_generate_collision = False
    d.generate_lightmap_u_vs = False
    t.options = ui
    tools.import_asset_tasks([t])
    m = eal.load_asset(f"/Game/Environments/Buildings/SM_Bld_{name}")
    if not m:
        log("BLD_WARN", f"{name} failed to import"); continue
    mapping = dict(GLASS, **slots)
    set_names = []
    for i, s in enumerate(m.get_editor_property("static_materials")):
        slot = str(s.get_editor_property("material_slot_name"))
        mat = M(mapping.get(slot, "")) if slot in mapping else None
        if mat:
            m.set_material(i, mat)
        else:
            log("BLD_WARN", f"{name}: slot {slot} unmapped")
        set_names.append(slot)
    ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
    eal.save_asset(f"/Game/Environments/Buildings/SM_Bld_{name}")
    b = m.get_bounds().box_extent
    log("BLD_OK", f"SM_Bld_{name}: size_m=({b.x*2/100:.1f}, {b.y*2/100:.1f}, {b.z*2/100:.1f}) slots={set_names}")
log("BLD_DONE", f"warnings={len(warnings)}")
