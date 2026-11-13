"""Spark material for the city FX actor (additive, unlit; C++ sets Luminance for the preset's exposure).
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_cityfx_materials.py
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop  # noqa: E402

g = G("/Game/Materials", "M_APEX_Spark")
g.mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
g.mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
g.mat.set_editor_property("two_sided", True)
prop(g.mul(g.v("Tint", (1.0, 0.42, 0.08)), g.s("Luminance", 600.0)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
g.done()
log("FX_OK", "M_APEX_Spark")
