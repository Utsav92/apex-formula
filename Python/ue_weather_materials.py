"""Rain-streak and spray-droplet materials (translucent, unlit; rain is animated in the vertex shader).

  UnrealEditor-Cmd APEXFormula.uproject -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_weather_materials.py -unattended -nullrhi -nosplash -stdout
C++ (AAPEXWeatherFX) sets Luminance so the effects land at a fixed display brightness whatever the preset's exposure is (same calibration as the sky dome).
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop  # noqa: E402

mpc = eal.load_asset("/Game/Materials/MPC_APEX_World")


def base(name):
    g = G("/Game/Materials", name)
    g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    g.mat.set_editor_property("used_with_instanced_static_meshes", True)
    g.mat.set_editor_property("two_sided", True)
    return g


# ---- M_APEX_Rain: every streak sits on one plane at the component origin; its height is wrapped in the vertex shader ----------------------
# z = BoxHeight * (0.5 - frac(Time * FallSpeed / BoxHeight + PerInstanceRandom))   -> falls forever inside +-BoxHeight/2, phase random per streak
g = base("M_APEX_Rain")
t = g.n(ME.MaterialExpressionTime)
rnd = g.n(ME.MaterialExpressionPerInstanceRandom)
phase = g.add(g.mul(t, g.mul(g.s("FallSpeed", 1800.0), g.c(1.0 / 3000.0))), rnd)
fr = g.n(ME.MaterialExpressionFrac); link(phase, "", fr, "")
dz = g.mul(g.sub(g.c(0.5), fr), g.c(3000.0))                                      # BoxHeight is fixed at 30 m
off = g.n(ME.MaterialExpressionAppendVector)
link(g.n(ME.MaterialExpressionConstant2Vector, r=0.0, g=0.0), "", off, "A"); link(dz, "", off, "B")
prop(off, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
wet = g.n(ME.MaterialExpressionCollectionParameter, collection=mpc, parameter_name="Wetness")
prop(g.mul(g.v("Tint", (0.80, 0.88, 1.0)), g.s("Luminance", 150.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(g.mul(g.sat(wet), g.s("Opacity", 0.20)), "", unreal.MaterialProperty.MP_OPACITY)
g.done()
log("WX_OK", "M_APEX_Rain")

# ---- M_APEX_Spray: soft pale droplet; size/age fade is done by scaling the instance in C++ -----------------------------------------------
g = base("M_APEX_Spray")
prop(g.mul(g.v("Tint", (0.86, 0.91, 1.0)), g.s("Luminance", 150.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(g.s("Opacity", 0.28), "", unreal.MaterialProperty.MP_OPACITY)
g.done()
log("WX_OK", "M_APEX_Spray")
