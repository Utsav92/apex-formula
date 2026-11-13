"""Import Blender-generated assets into the APEXFormula Unreal project and build their materials.

Run (from anywhere):
  ~/unreal/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd ~/Development/APEXFormula/APEXFormula.uproject \
     -ExecutePythonScript=~/Development/APEXFormula/Python/ue_import.py -unattended -nullrhi -nosplash -stdout

Idempotent: re-running replaces existing assets. Prints IMPORT_OK / IMPORT_FAIL lines to grep.
"""
import os

import unreal

PROJECT = os.path.expanduser("~/Development/APEXFormula")
EXPORT = os.path.join(PROJECT, "Blender", "export")
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

# name: (base colour RGB, metallic, roughness, emissive strength or 0)
MATERIALS = {
    "M_Paint": ((0.75, 0.02, 0.03), 0.55, 0.16, 0),   # glossy metallic paint
    "M_Accent": ((0.92, 0.92, 0.92), 0.0, 0.35, 0),
    "M_Carbon": ((0.015, 0.015, 0.017), 0.25, 0.32, 0),
    "M_Black": ((0.03, 0.03, 0.035), 0.0, 0.5, 0),
    "M_Metal": ((0.55, 0.56, 0.58), 1.0, 0.35, 0),
    "M_Glass": ((0.02, 0.03, 0.05), 0.0, 0.06, 0),
    "M_Light": ((1.0, 0.05, 0.02), 0.0, 0.4, 6.0),
    "M_Tyre": ((0.02, 0.02, 0.022), 0.0, 0.85, 0),
    "M_Rim": ((0.35, 0.36, 0.38), 1.0, 0.3, 0),
}

TEXTURES = ["T_Asphalt", "T_Grass", "T_GrassMacro", "T_Kerb", "T_Barrier", "T_Chequer",
            "T_FacadeGlass", "T_FacadeConcrete", "T_FacadeBrick", "T_Seats",
            "T_Carbon", "T_TyreSidewall", "T_RimSpokes", "T_DecalNumber", "T_DecalApex"]

# Decals: colour from the texture, shape from its alpha (masked). name: (texture, roughness)
MASKED_MATERIALS = {
    "M_DecalNumber": ("T_DecalNumber", 0.45),
    "M_DecalApex": ("T_DecalApex", 0.45),
}

# name: (texture, roughness, metallic, macro texture or None, macro tiling)
TEX_MATERIALS = {
    "M_Asphalt": ("T_Asphalt", 0.80, 0.0, None, 0),
    "M_Kerb": ("T_Kerb", 0.65, 0.0, None, 0),
    "M_Barrier": ("T_Barrier", 0.55, 0.0, None, 0),
    "M_Chequer": ("T_Chequer", 0.6, 0.0, None, 0),
    "M_Grass": ("T_Grass", 0.95, 0.0, "T_GrassMacro", 0.035),
    "M_FacadeGlass": ("T_FacadeGlass", 0.35, 0.0, None, 0),
    "M_FacadeConcrete": ("T_FacadeConcrete", 0.7, 0.0, None, 0),
    "M_FacadeBrick": ("T_FacadeBrick", 0.8, 0.0, None, 0),
    "M_Seats": ("T_Seats", 0.7, 0.0, None, 0),
    # Car: carbon weave, moulded tyre sidewall with a compound ring, spoked rim.
    "M_Carbon": ("T_Carbon", 0.28, 0.35, None, 0),
    "M_Tyre": ("T_TyreSidewall", 0.85, 0.0, None, 0),
    "M_Rim": ("T_RimSpokes", 0.30, 0.85, None, 0),
}
# Plain colour materials for scenery (same helper as the car's).
MATERIALS.update({
    "M_Bark": ((0.22, 0.13, 0.07), 0.0, 0.95, 0),
    "M_Leaf": ((0.10, 0.30, 0.06), 0.0, 0.85, 0),
    "M_Roof": ((0.38, 0.38, 0.40), 0.0, 0.8, 0),
    "M_Concrete": ((0.50, 0.50, 0.48), 0.0, 0.9, 0),
    "M_Canopy": ((0.80, 0.80, 0.82), 0.0, 0.6, 0),
})

# FBX file -> (asset name, folder, {fbx material slot name: material asset name}, complex-as-simple collision)
MESHES = {
    "Track.fbx": ("SM_Track", "/Game/Tracks", {"Asphalt": "M_Asphalt", "Kerb": "M_Kerb", "Barrier": "M_Barrier",
                  "Chequer": "M_Chequer", "Grass": "M_Grass"}, False),
    "Tree_Pine.fbx": ("SM_Tree_Pine", "/Game/Environments", {"Bark": "M_Bark", "Leaf": "M_Leaf"}, False),
    "Tree_Oak.fbx": ("SM_Tree_Oak", "/Game/Environments", {"Bark": "M_Bark", "Leaf": "M_Leaf"}, False),
    "Bld_Glass.fbx": ("SM_Bld_Glass", "/Game/Environments", {"Facade": "M_FacadeGlass", "Roof": "M_Roof"}, False),
    "Bld_Block.fbx": ("SM_Bld_Block", "/Game/Environments", {"Facade": "M_FacadeConcrete", "Roof": "M_Roof"}, False),
    "Bld_Brick.fbx": ("SM_Bld_Brick", "/Game/Environments", {"Facade": "M_FacadeBrick", "Roof": "M_Roof"}, False),
    "Bld_Slim.fbx": ("SM_Bld_Slim", "/Game/Environments", {"Facade": "M_FacadeGlass", "Roof": "M_Roof"}, False),
    "Grandstand.fbx": ("SM_Grandstand", "/Game/Environments", {"Seats": "M_Seats", "Concrete": "M_Concrete",
                       "Steel": "M_Metal", "Canopy": "M_Canopy"}, False),
    "F1_Body.fbx": ("SM_F1_Body", "/Game/Vehicles", {"Paint": "M_Paint", "Accent": "M_Accent", "Carbon": "M_Carbon",
                    "Black": "M_Black", "Metal": "M_Metal", "Glass": "M_Glass", "Light": "M_Light",
                    "DecalNum": "M_DecalNumber", "DecalApex": "M_DecalApex"}, False),
    "F1_Wheel.fbx": ("SM_F1_Wheel", "/Game/Vehicles", {"Tyre": "M_Tyre", "Rim": "M_Rim", "Metal": "M_Metal"}, False),
}


def log(tag, msg):
    unreal.log(f"{tag} {msg}")


def make_material(name, colour, metallic, roughness, emissive):
    path = "/Game/Materials"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    # Trees, buildings and grandstands are drawn as instanced meshes; without this flag the engine silently
    # swaps in the default material at runtime.
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "BaseColor")
    col.set_editor_property("default_value", unreal.LinearColor(*colour, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    met = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 200)
    met.set_editor_property("parameter_name", "Metallic")
    met.set_editor_property("default_value", metallic)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 400)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", roughness)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if emissive > 0:
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 600)
        k = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 650)
        k.set_editor_property("r", emissive)
        mel.connect_material_expressions(col, "", mul, "A")
        mel.connect_material_expressions(k, "", mul, "B")
        mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_asset(full)
    return mat


def import_texture(name):
    src = os.path.join(EXPORT, "textures", name + ".png")
    if not os.path.exists(src):
        log("IMPORT_FAIL", f"{name}.png not found (run gen_textures.py)")
        return
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = "/Game/Textures"
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    tex = eal.load_asset(f"/Game/Textures/{name}")
    if tex:
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
        eal.save_asset(f"/Game/Textures/{name}")
    else:
        log("IMPORT_FAIL", f"texture {name} did not import")


def make_tex_material(name, tex_name, roughness, metallic, macro, macro_tile, glow=0.0):
    path = "/Game/Materials"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    tex = eal.load_asset(f"/Game/Textures/{tex_name}")

    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0)
    ts.set_editor_property("texture", tex)
    out = ts
    if macro:
        # Second, low-frequency sample multiplied in so the large grass field does not visibly repeat.
        tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 300)
        tc.set_editor_property("u_tiling", macro_tile)
        tc.set_editor_property("v_tiling", macro_tile)
        ms = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 300)
        ms.set_editor_property("texture", eal.load_asset(f"/Game/Textures/{macro}"))
        mel.connect_material_expressions(tc, "", ms, "UVs")
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 100)
        mel.connect_material_expressions(ts, "RGB", mul, "A")
        mel.connect_material_expressions(ms, "RGB", mul, "B")
        out = mul
        mel.connect_material_property(out, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mel.connect_material_property(out, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    if glow > 0:
        # A little self-lit colour stands in for bounced light, so shaded building sides are not black.
        gm = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 700)
        gk = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 750)
        gk.set_editor_property("r", glow)
        mel.connect_material_expressions(ts, "RGB", gm, "A")
        mel.connect_material_expressions(gk, "", gm, "B")
        mel.connect_material_property(gm, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    m = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 500)
    m.set_editor_property("r", metallic)
    mel.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_asset(full)


def make_masked_material(name, tex_name, roughness):
    path = "/Game/Materials"
    full = f"{path}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 0)
    ts.set_editor_property("texture", eal.load_asset(f"/Game/Textures/{tex_name}"))
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_asset(full)


def import_fbx(filename, asset_name, folder, slot_map, complex_collision):
    src = os.path.join(EXPORT, filename)
    if not os.path.exists(src):
        log("IMPORT_FAIL", f"{filename} not found (run the Blender generator first)")
        return
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = folder
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.generate_lightmap_u_vs = False
    ui.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task.options = ui
    tools.import_asset_tasks([task])

    mesh = eal.load_asset(f"{folder}/{asset_name}")
    if not mesh:
        log("IMPORT_FAIL", f"{asset_name} did not import")
        return

    slots = list(mesh.get_editor_property("static_materials"))
    for i, s in enumerate(slots):
        slot_name = str(s.get_editor_property("material_slot_name"))
        target = slot_map.get(slot_name)
        if target:
            mesh.set_material(i, eal.load_asset(f"/Game/Materials/{target}"))
        else:
            log("IMPORT_WARN", f"{asset_name}: slot '{slot_name}' has no material mapping")

    if complex_collision:
        body_setup = mesh.get_editor_property("body_setup")
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)

    eal.save_asset(f"{folder}/{asset_name}")
    b = mesh.get_bounds()
    ext = b.box_extent
    o = b.origin
    log("IMPORT_OK", f"{asset_name}: slots={[str(s.get_editor_property('material_slot_name')) for s in slots]} "
                     f"size_cm=({ext.x * 2:.0f}, {ext.y * 2:.0f}, {ext.z * 2:.0f}) centre_cm=({o.x:.0f}, {o.y:.0f}, {o.z:.0f})")


for t in TEXTURES:
    import_texture(t)
log("IMPORT_OK", f"{len(TEXTURES)} textures")

for n, (colour, metallic, rough, emissive) in MATERIALS.items():
    make_material(n, colour, metallic, rough, emissive)
for n, (tex, rough, metallic, macro, macro_tile) in TEX_MATERIALS.items():
    make_tex_material(n, tex, rough, metallic, macro, macro_tile, glow=0.55 if n.startswith("M_Facade") else (0.35 if n == "M_Seats" else 0.0))
for n, (tex, rough) in MASKED_MATERIALS.items():
    make_masked_material(n, tex, rough)
log("IMPORT_OK", f"{len(MATERIALS)} colour + {len(TEX_MATERIALS)} textured + {len(MASKED_MATERIALS)} masked materials")

for fbx, (name, folder, slot_map, cc) in MESHES.items():
    import_fbx(fbx, name, folder, slot_map, cc)
