"""Far-field city: facade material (procedural window grid, per-building tint, lit rooms at night) and the 12 block-mass meshes from Blender/gen_farcity.py.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_farcity.py
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, log, mel, tools  # noqa: E402

PROJECT = os.path.expanduser("~/Development/APEXFormula")
MATS = "/Game/Materials/Buildings"
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")

COMMON = r"""
float2 cellSize = float2(3.4, 3.7);                   // window bay and floor height (m)
float2 q = UV / cellSize;
float2 f = frac(q);
float2 id = floor(q);
float vertical = 1.0 - step(0.5, abs(NZ));
float win = step(abs(f.x - 0.5), 0.30) * step(abs(f.y - 0.55), 0.28) * vertical;
float2 bld = floor(WP.xy / 2600.0);                    // one hash per ~26 m cell ~ per building
float hb = frac(sin(dot(bld, float2(39.3, 11.1))) * 43758.5453);
float h  = frac(sin(dot(id + bld * 3.7, float2(12.9898, 78.233))) * 43758.5453);
"""
ALBEDO = COMMON + r"""
float3 wall = hb < 0.28 ? float3(0.28, 0.17, 0.11) : (hb < 0.52 ? float3(0.36, 0.33, 0.28) : (hb < 0.78 ? float3(0.30, 0.31, 0.33) : float3(0.12, 0.16, 0.21)));
wall *= 0.85 + 0.3 * frac(h * 5.3);                     // slight panel-to-panel variation
float3 roof = float3(0.22, 0.22, 0.23);
float3 base = lerp(wall, roof, step(0.5, abs(NZ)));
return lerp(base, float3(0.02, 0.03, 0.045), win);
"""
EMISSIVE = COMMON + r"""
float lit = step(0.55, h) * win;
float3 warm = lerp(float3(1.0, 0.72, 0.42), float3(0.75, 0.85, 1.0), step(0.85, frac(h * 9.1)));
float3 night = warm * lit * 20.0;
float3 day = float3(0.55, 0.65, 0.8) * win * 14.0;       // daytime: bright sky reflected in the glass
return lerp(day, night, Night);
"""

g = G(MATS, "M_APEX_FarFacade")
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.mat.set_editor_property("used_with_nanite", True)
uv = g.n(ME.MaterialExpressionTextureCoordinate)
wp = g.n(ME.MaterialExpressionWorldPosition)
nz = g.n(ME.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
vn = g.n(ME.MaterialExpressionVertexNormalWS)
mel.connect_material_expressions(vn, "", nz, "")
ng = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")


def custom(code, desc):
    c = g.n(ME.MaterialExpressionCustom)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", desc)
    ins = []
    for n in ("UV", "WP", "NZ", "Night"):
        i = unreal.CustomInput(); i.set_editor_property("input_name", n); ins.append(i)
    c.set_editor_property("inputs", ins)
    ok = [mel.connect_material_expressions(uv, "", c, "UV"), mel.connect_material_expressions(wp, "", c, "WP"),
          mel.connect_material_expressions(nz, "", c, "NZ"), mel.connect_material_expressions(ng, "", c, "Night")]
    return c, all(ok)


a, ok1 = custom(ALBEDO, "FarAlbedo")
e, ok2 = custom(EMISSIVE, "FarEmissive")
ok3 = mel.connect_material_property(a, "", unreal.MaterialProperty.MP_BASE_COLOR)
ok4 = mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(g.c(0.65), "", unreal.MaterialProperty.MP_ROUGHNESS)
g.done()
log("FAR_OK" if all([ok1, ok2, ok3, ok4]) else "FAR_WARN", f"M_APEX_FarFacade connected={[ok1, ok2, ok3, ok4]}")

mat = eal.load_asset(f"{MATS}/M_APEX_FarFacade")
for cat in ("Low", "Mid", "High", "Fin"):
    for n in range(3):
        name = f"{cat}{n}"
        t = unreal.AssetImportTask()
        t.filename = os.path.join(PROJECT, "Blender", "export", f"Far_{name}.fbx")
        t.destination_path = "/Game/Environments/Far"; t.destination_name = f"SM_Far_{name}"
        t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
        ui = unreal.FbxImportUI()
        ui.import_mesh = True; ui.import_as_skeletal = False; ui.import_materials = False; ui.import_textures = False; ui.import_animations = False
        ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        d = ui.static_mesh_import_data
        d.combine_meshes = True; d.auto_generate_collision = False; d.generate_lightmap_u_vs = False
        d.set_editor_property("build_nanite", True)
        t.options = ui
        tools.import_asset_tasks([t])
        m = eal.load_asset(f"/Game/Environments/Far/SM_Far_{name}")
        if not m:
            log("FAR_WARN", f"{name} failed"); continue
        for i, s in enumerate(m.get_editor_property("static_materials")):
            m.set_material(i, mat)
        ns = m.get_editor_property("nanite_settings"); ns.enabled = True; m.set_editor_property("nanite_settings", ns)
        eal.save_asset(f"/Game/Environments/Far/SM_Far_{name}")
log("FAR_OK", "12 far-field block masses imported")
