"""Facade weathering, patched into M_APEX_BldSurface (every brick/stone/plaster/concrete building uses it): world-space grime near the street, vertical
water-stain streaks below ledges, soot at the top, and lighter repair patches. Reads the material's existing base colour and multiplies it.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_facade_weathering.py
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import ME, eal, log, mel  # noqa: E402

MATS = "/Game/Materials/Buildings"
mat = eal.load_asset(f"{MATS}/M_APEX_BldSurface")
wetness = None

CODE = r"""
float z = WP.z;
float2 hxy = WP.xy;
// street grime: darker and rougher in the first ~5 m, fading up; stronger in the lowest metre (splash zone)
float grime = saturate(1.0 - z / 520.0);
grime = grime * grime;
// water-stain streaks: columns ~60 cm wide, each with its own length, running down from a random start height
float col = floor((hxy.x + hxy.y) / 60.0);
float rc = frac(sin(col * 12.9898 + floor(z / 1600.0) * 4.1) * 43758.5453);
float streakLen = lerp(80.0, 520.0, frac(rc * 7.3));
float zt = frac(z / 1600.0) * 1600.0;                                 // position inside a ~16 m repeat
float start = lerp(300.0, 1500.0, frac(rc * 3.1));
float streak = step(0.72, rc) * smoothstep(start - streakLen, start, zt) * (1.0 - smoothstep(start, start + 40.0, zt));
// repair patches: ~6 % of 2.4 m x 1.8 m cells get a paler, smoother patch with a hard edge
float2 pc = floor(float2((hxy.x + hxy.y) / 240.0, z / 180.0));
float rp = frac(sin(dot(pc, float2(31.7, 91.3))) * 43758.5453);
float patch = step(0.94, rp);
float3 c = Base;
c *= 1.0 - 0.42 * grime;
c *= 1.0 - 0.30 * streak;
c = lerp(c, c * float3(1.18, 1.12, 1.02) + 0.03, patch);
float topSoot = smoothstep(2200.0, 4200.0, z) * 0.0;                  // reserved: roofline soot
return c * (1.0 - topSoot);
"""

x, y = -1400, 700
def node(cls, **kw):
    global y
    e = mel.create_material_expression(mat, cls, x, y)
    y += 130
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e

base_node = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
base_pin = mel.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_BASE_COLOR)
if base_node is None:
    log("WEATHER_WARN", "M_APEX_BldSurface has no base colour node to wrap")
else:
    wp = node(ME.MaterialExpressionWorldPosition)
    c = node(ME.MaterialExpressionCustom)
    c.set_editor_property("code", CODE)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "FacadeWeathering")
    ins = []
    for n in ("Base", "WP"):
        i = unreal.CustomInput(); i.set_editor_property("input_name", n); ins.append(i)
    c.set_editor_property("inputs", ins)
    ok = [mel.connect_material_expressions(base_node, base_pin or "", c, "Base"), mel.connect_material_expressions(wp, "", c, "WP"),
          mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)]
    mel.recompile_material(mat)
    eal.save_asset(f"{MATS}/M_APEX_BldSurface")
    log("WEATHER_OK" if all(ok) else "WEATHER_WARN", f"M_APEX_BldSurface weathering connected={ok} (input pin '{base_pin}')")
