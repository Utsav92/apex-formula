"""Foliage masters (M_APEX_TreeBark, M_APEX_Leaves) with shared wind, instances for the three Poly Haven trees, and
Nanite meshes. Run after Python/fetch_polyhaven.py.

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=Python/ue_trees.py -unattended -nullrhi -nosplash -stdout
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, mel, prop, tools, warnings  # noqa: E402

SRC = os.path.expanduser("~/Development/APEXFormula/Blender/sources/polyhaven")
TREES = "/Game/Environments/Trees"
TEX = "/Game/Textures/Trees"

# (asset id, mesh name, {slot: (material instance, texture prefix, kind)})
SPECIES = [
    ("jacaranda_tree", "SM_Tree_Jacaranda", {
        "jacaranda_tree_branches": ("branches", "bark"), "jacaranda_tree_trunk": ("trunk", "bark"),
        "jacaranda_tree_leaves": ("leaves", "leaf")}),
    ("pine_sapling_small", "SM_Tree_PineSapling", {
        "pine_sapling_small_bark": ("bark", "bark"), "pine_sapling_small_twig": ("twig", "leaf")}),
    ("fir_sapling", "SM_Tree_FirSapling", {
        "fir_sapling_branches": ("branches", "bark"), "fir_sapling_twigs": ("twigs", "leaf")}),
]


def import_tex(aid, part, suffix, kind):
    src = os.path.join(SRC, aid, "textures", f"{aid}_{part}_{suffix}_2k.png")
    if not os.path.exists(src):
        return None
    name = f"T_{aid}_{part}_{suffix}"
    task = unreal.AssetImportTask()
    task.filename = src; task.destination_path = TEX; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    tools.import_asset_tasks([task])
    t = eal.load_asset(f"{TEX}/{name}")
    if kind == "color":
        t.set_editor_property("srgb", True)
    else:
        t.set_editor_property("srgb", False)
        t.set_editor_property("compression_settings",
                              unreal.TextureCompressionSettings.TC_NORMALMAP if kind == "normal" else unreal.TextureCompressionSettings.TC_MASKS)
    t.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    eal.save_asset(f"{TEX}/{name}")
    return f"{TEX}/{name}"


def tex_param(g, name, normal=False, default=None):
    """Texture parameter WITH a real default texture: a Normal-sampler parameter left on Unreal's grey default
    texture fails to compile ("Sampler type is Normal, should be Color")."""
    e = g.n(ME.MaterialExpressionTextureSampleParameter2D, parameter_name=name)
    if default:
        e.set_editor_property("texture", eal.load_asset(default))
    if normal:
        e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    return e


def wind_offset(g, flutter):
    """World-position offset: sway grows with height above the instance pivot (so trunks stay planted); phase is
    per-instance so a forest does not move in lockstep. `flutter` adds a fast, small leaf wobble."""
    strength, speed, hscale = g.s("WindStrength", 25.0), g.s("WindSpeed", 1.1), g.s("WindHeightScale", 1800.0)
    # Inside a WPO graph the position must EXCLUDE shader offsets (it would be circular otherwise).
    pos = g.n(ME.MaterialExpressionWorldPosition)
    pos.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    obj = g.n(ME.MaterialExpressionObjectPositionWS)
    rel = g.sub(pos, obj)
    rel_z = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(rel, "", rel_z, "")
    # normalise height by WindHeightScale: h = saturate(rel_z / scale)^2
    inv = g.n(ME.MaterialExpressionDivide); link(rel_z, "", inv, "A"); link(hscale, "", inv, "B")
    h2 = g.n(ME.MaterialExpressionPower); link(g.sat(inv), "", h2, "Base"); link(g.c(2.0), "", h2, "Exp")
    ox = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(obj, "", ox, "")
    oy = g.n(ME.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); link(obj, "", oy, "")
    phase = g.add(g.mul(ox, g.c(0.0113)), g.mul(oy, g.c(0.0171)))
    t = g.mul(g.n(ME.MaterialExpressionTime), speed)
    s1 = g.n(ME.MaterialExpressionSine); link(g.add(t, phase), "", s1, "")
    s2 = g.n(ME.MaterialExpressionSine); link(g.add(g.mul(t, g.c(2.3)), g.mul(phase, g.c(1.7))), "", s2, "")
    gust = g.add(g.mul(s1, g.c(0.7)), g.mul(s2, g.c(0.3)))
    sway = g.mul(g.mul(g.mul(gust, strength), h2), g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, 0.6, 0.0, 1.0)))
    if not flutter:
        return sway
    fl = g.n(ME.MaterialExpressionSine)
    px = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(pos, "", px, "")
    link(g.add(g.mul(g.n(ME.MaterialExpressionTime), g.c(9.0)), g.mul(px, g.c(0.03))), "", fl, "")
    flutter_v = g.mul(g.mul(fl, g.mul(g.s("LeafFlutter", 3.0), h2)), g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.5, 0.5, 1.0, 1.0)))
    return g.add(sway, flutter_v)


def flip_green(g, nrm):
    return g.mul(nrm, g.n(ME.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(1.0, -1.0, 1.0, 1.0)), "RGB", "")


# defaults for the master parameters come from the Jacaranda set (instances override every one of them)
DEF = {k: import_tex("jacaranda_tree", part, suf, kind) for k, (part, suf, kind) in {
    "b_diff": ("branches", "diff", "color"), "b_nor": ("branches", "nor_gl", "normal"), "b_rough": ("branches", "rough", "mask"),
    "l_diff": ("leaves", "diff", "color"), "l_nor": ("leaves", "nor_gl", "normal"), "l_rough": ("leaves", "rough", "mask"),
    "l_alpha": ("leaves", "alpha", "mask")}.items()}

# ---- M_APEX_TreeBark ----------------------------------------------------------------------------------
g = G("/Game/Materials/Trees", "M_APEX_TreeBark", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
diff, nrm, rough = tex_param(g, "Diffuse", default=DEF["b_diff"]), tex_param(g, "Normal", True, DEF["b_nor"]), tex_param(g, "Roughness", default=DEF["b_rough"])
prop(g.mul(diff, g.v("Tint", (1.0, 1.0, 1.0)), "RGB", ""), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(flip_green(g, nrm), "", unreal.MaterialProperty.MP_NORMAL)
prop(g.add(g.mul(rough, g.c(0.35), "R", ""), g.c(0.55)), "", unreal.MaterialProperty.MP_ROUGHNESS)   # bark is rough: 0.55-0.9
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
prop(wind_offset(g, flutter=False), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
bark = g.done()

# ---- M_APEX_Leaves (masked, two-sided foliage, translucency, season, wind + flutter) -------------------
g = G("/Game/Materials/Trees", "M_APEX_Leaves", instanced=True)
g.mat.set_editor_property("used_with_nanite", True)
g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
g.mat.set_editor_property("two_sided", True)
diff, nrm, rough, alpha = (tex_param(g, "Diffuse", default=DEF["l_diff"]), tex_param(g, "Normal", True, DEF["l_nor"]),
                           tex_param(g, "Roughness", default=DEF["l_rough"]), tex_param(g, "Alpha", default=DEF["l_alpha"]))
tinted = g.mul(diff, g.v("Tint", (1.0, 1.0, 1.0)), "RGB", "")
autumn = g.mul(g.v("AutumnColor", (0.85, 0.38, 0.06)), g.mul(diff, g.c(1.0), "G", ""))   # keep the leaf's own light/dark detail
colour = g.lerp(tinted, autumn, g.s("Season", 0.0))                                           # 0 = summer green, 1 = autumn
prop(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.mul(colour, g.c(0.55)), "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)              # light through the leaf
prop(flip_green(g, nrm), "", unreal.MaterialProperty.MP_NORMAL)
prop(g.add(g.mul(rough, g.c(0.35), "R", ""), g.c(0.45)), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.c(0.0), "", unreal.MaterialProperty.MP_METALLIC)
# Distance boost: the alpha mips average below the 0.33 clip value, which strips leaves from distant canopies (bare sticks). Scaling the mask by
# up to 5x between 40 m and 340 m keeps the canopy dense at distance (the leaf cards are subpixel there, so only the coverage matters).
alpha_r = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(alpha, "", alpha_r, "")
depth = g.n(ME.MaterialExpressionPixelDepth)
boost = g.add(g.c(1.0), g.mul(g.sat(g.mul(g.sub(depth, g.c(4000.0)), g.c(1.0 / 30000.0))), g.c(4.0)))
prop(g.sat(g.mul(alpha_r, boost)), "", unreal.MaterialProperty.MP_OPACITY_MASK)
prop(wind_offset(g, flutter=True), "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
leaves = g.done()


def make_mi(name, parent, tex, tint=None):
    full = f"/Game/Materials/Trees/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mi = tools.create_asset(name, "/Game/Materials/Trees", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    for k, v in tex.items():
        if v:
            mel.set_material_instance_texture_parameter_value(mi, k, eal.load_asset(v))
    if tint:
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(full)
    return mi


for aid, mesh_name, slots in SPECIES:
    mesh = eal.load_asset(f"{TREES}/{mesh_name}")
    if not mesh:
        log("TREE_FAIL", f"{mesh_name} missing (run ue_trees_probe.py first)")
        continue
    # make sure Nanite is on
    ns = mesh.get_editor_property("nanite_settings")
    ns.enabled = True
    mesh.set_editor_property("nanite_settings", ns)
    count = 0
    for i, s in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(s.get_editor_property("material_slot_name"))
        part_kind = slots.get(slot)
        if not part_kind:
            log("TREE_WARN", f"{mesh_name}: unmapped slot {slot}")
            continue
        part, kind = part_kind
        d = import_tex(aid, part, "diff", "color")
        n = import_tex(aid, part, "nor_gl", "normal")
        r = import_tex(aid, part, "rough", "mask")
        if kind == "leaf":
            a = import_tex(aid, part, "alpha", "mask") or import_tex(aid, part, "mask", "mask")
            mi = make_mi(f"MI_{aid}_{part}", leaves, {"Diffuse": d, "Normal": n, "Roughness": r, "Alpha": a})
        else:
            mi = make_mi(f"MI_{aid}_{part}", bark, {"Diffuse": d, "Normal": n, "Roughness": r})
        mesh.set_material(i, mi)
        count += 1
    eal.save_asset(f"{TREES}/{mesh_name}")
    log("TREE_OK", f"{mesh_name}: {count} slots materialised, nanite={ns.enabled}")
log("TREE_DONE", f"warnings={len(warnings)}")
