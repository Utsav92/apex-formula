"""Interior-mapped windows: patches M_APEX_BldGlass in place so every window cell shows a parallax room (side walls, back wall, floor, ceiling light strip)
instead of a flat glow. Object-space (works on rotated buildings), procedural (no textures), per-room hash for paint colour and lit/unlit.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_interior_windows.py
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import ME, eal, log, mel  # noqa: E402

MATS = "/Game/Materials/Buildings"
mat = eal.load_asset(f"{MATS}/M_APEX_BldGlass")
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")

CODE = r"""
float3 rd = normalize(LP - LC);                       // view ray, going into the room
float3 room = float3(360.0, 360.0, 340.0);            // room footprint (building-local x,y) and floor height
float3 p = LP + rd * 3.0;                              // start just behind the glass
float3 cell = floor(p / room);
float3 lo = cell * room;
float3 hi = lo + room;
float3 inv = 1.0 / (abs(rd) + 1e-4) * sign(rd + 1e-6);
float3 tf = max((lo - p) * inv, (hi - p) * inv);       // distance to leave the room through each axis
float t = min(tf.x, min(tf.y, tf.z));
float3 hit = p + rd * t;
float h  = frac(sin(dot(cell, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float h2 = frac(h * 7.31 + 0.17);
float h3 = frac(h * 13.7 + 0.53);
float3 paint = lerp(float3(0.78, 0.72, 0.62), float3(0.50, 0.60, 0.72), h2);
paint = lerp(paint, float3(0.70, 0.45, 0.40), step(0.8, h3));
float3 uvw = (hit - lo) / room;                         // 0..1 inside the room
float shade; float3 col = paint;
if (t == tf.z)                                          // floor or ceiling
{
    bool ceiling = rd.z > 0.0;
    shade = ceiling ? 1.15 : 0.55;
    float strip = step(abs(uvw.x - 0.5), 0.10) * step(abs(uvw.y - 0.5), 0.34);
    if (ceiling) { col = lerp(col, float3(1.0, 0.95, 0.85), strip); shade += strip * 2.2; }
    else { col = lerp(col, float3(0.35, 0.25, 0.18), step(0.5, frac(h * 5.0))); }   // carpet or wood
}
else if (t == tf.y) { shade = 0.95; col = lerp(col, float3(0.15, 0.17, 0.2), step(abs(uvw.x - 0.5), 0.22) * step(uvw.z, 0.55) * step(0.35, h3)); }  // back wall, sometimes with a desk/sofa block
else { shade = 0.75; }                                  // side walls
bool lit = h > 0.45;
float day = (1.0 - Night);
float lum = lerp(lit ? 80.0 : 45.0, lit ? 30.0 : 0.25, Night);   // calibrated like the LED boards: day exposure is ~EV14, night ~EV3
return col * shade * lum * lerp(float3(1,1,1), float3(1.0, 0.82, 0.55), Night * (lit ? 1.0 : 0.0));
"""

x, y = -1500, 900
def node(cls, **kw):
    global y
    e = mel.create_material_expression(mat, cls, x, y)
    y += 130
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e

wp = node(ME.MaterialExpressionWorldPosition)
cp = node(ME.MaterialExpressionCameraPositionWS)
lp = node(ME.MaterialExpressionTransformPosition, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
lc = node(ME.MaterialExpressionTransformPosition, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
mel.connect_material_expressions(wp, "", lp, "")
mel.connect_material_expressions(cp, "", lc, "")
ng = node(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
c = node(ME.MaterialExpressionCustom)
c.set_editor_property("code", CODE)
c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
c.set_editor_property("description", "InteriorMapping")
ins = []
for n in ("LP", "LC", "Night"):
    i = unreal.CustomInput(); i.set_editor_property("input_name", n); ins.append(i)
c.set_editor_property("inputs", ins)
ok = [mel.connect_material_expressions(lp, "", c, "LP"), mel.connect_material_expressions(lc, "", c, "LC"), mel.connect_material_expressions(ng, "", c, "Night")]
ok.append(mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR))
mel.recompile_material(mat)
eal.save_asset(f"{MATS}/M_APEX_BldGlass")
log("INTERIOR_OK" if all(ok) else "INTERIOR_WARN", f"M_APEX_BldGlass interior mapping connected={ok}")
