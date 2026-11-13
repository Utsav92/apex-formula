"""Blinds and curtains over the interior-mapped windows: wraps the existing emissive (the interior room) with slatted blinds on ~35 % of rooms and side
drapes on ~25 %, lit by the room (dark when the room is dark). Patches M_APEX_BldGlass in place; run after ue_interior_windows.py.
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import ME, eal, log, mel  # noqa: E402

MATS = "/Game/Materials/Buildings"
mat = eal.load_asset(f"{MATS}/M_APEX_BldGlass")
CODE = r"""
float3 room = float3(360.0, 360.0, 340.0);
float3 cellPos = floor(LP / room);
float3 uvw = (LP - cellPos * room) / room;
float h = frac(sin(dot(cellPos, float3(12.9898, 78.233, 37.719))) * 43758.5453);
float hb = frac(h * 17.3 + 0.29);
float3 rd = normalize(LP - LC);
float hx = abs(rd.x) > abs(rd.y) ? uvw.y : uvw.x;        // horizontal position along the facade
float z = uvw.z;
float roomLum = max(dot(In, float3(0.3333, 0.3333, 0.3333)), 0.04);
float day = 1.0 - Night;
float3 outc = In;
float hasBlind = step(0.62, hb);
float drop = lerp(0.35, 0.9, frac(h * 5.7));
float blind = hasBlind * step(1.0 - drop, z);
float slats = 0.72 + 0.28 * step(0.5, frac(z * 34.0));
float3 blindCol = lerp(float3(0.78, 0.72, 0.62), float3(0.55, 0.58, 0.62), frac(h * 3.3)) * slats;
outc = lerp(outc, blindCol * (roomLum * 0.9 + day * 16.0), blind);
float hasCurtain = step(0.80, frac(h * 11.9)) * (1.0 - hasBlind);
float curtain = hasCurtain * saturate(step(hx, 0.18) + step(0.82, hx));
float3 curtCol = lerp(float3(0.55, 0.15, 0.12), float3(0.18, 0.25, 0.45), frac(h * 8.7)) * (0.8 + 0.2 * sin(hx * 60.0));
outc = lerp(outc, curtCol * (roomLum * 0.8 + day * 12.0), curtain);
return outc;
"""
mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")
x, y = -1000, 1500
def node(cls, **kw):
    global y
    e = mel.create_material_expression(mat, cls, x, y)
    y += 130
    for k, v in kw.items():
        e.set_editor_property(k, v)
    return e

em = mel.get_material_property_input_node(mat, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
if em is None:
    log("BLIND_WARN", "no emissive node")
else:
    # reuse the world->local transforms and NightGlow already in the graph by creating fresh ones (cheap)
    wp = node(ME.MaterialExpressionWorldPosition); cp = node(ME.MaterialExpressionCameraPositionWS)
    src = unreal.MaterialPositionTransformSource
    lp = node(ME.MaterialExpressionTransformPosition, transform_source_type=src.TRANSFORMPOSSOURCE_WORLD, transform_type=src.TRANSFORMPOSSOURCE_LOCAL)
    lc = node(ME.MaterialExpressionTransformPosition, transform_source_type=src.TRANSFORMPOSSOURCE_WORLD, transform_type=src.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(wp, "", lp, ""); mel.connect_material_expressions(cp, "", lc, "")
    ng = node(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="NightGlow")
    c = node(ME.MaterialExpressionCustom)
    c.set_editor_property("code", CODE); c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    c.set_editor_property("description", "BlindsCurtains")
    ins = []
    for n in ("In", "LP", "LC", "Night"):
        i = unreal.CustomInput(); i.set_editor_property("input_name", n); ins.append(i)
    c.set_editor_property("inputs", ins)
    ok = [mel.connect_material_expressions(em, "", c, "In"), mel.connect_material_expressions(lp, "", c, "LP"),
          mel.connect_material_expressions(lc, "", c, "LC"), mel.connect_material_expressions(ng, "", c, "Night"),
          mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)]
    mel.recompile_material(mat)
    eal.save_asset(f"{MATS}/M_APEX_BldGlass")
    log("BLIND_OK" if all(ok) else "BLIND_WARN", f"blinds/curtains connected={ok}")
