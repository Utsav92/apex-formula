"""Harbour water: animated wave normals (sum of travelling sines, analytic slope), Fresnel-friendly glossy surface, depth-tinted colour.
  UnrealEditor-Cmd ... -ExecutePythonScript=$HOME/Development/APEXFormula/Python/ue_water.py
"""
import os
import sys

sys.path.insert(0, os.path.expanduser("~/Development/APEXFormula/Python"))
import unreal  # noqa: E402
from ue_matlib import G, ME, eal, link, log, prop  # noqa: E402

g = G("/Game/Materials", "M_APEX_Water")
g.mat.set_editor_property("used_with_instanced_static_meshes", True)
wp = g.n(ME.MaterialExpressionWorldPosition)
px = g.n(ME.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(wp, "", px, "")
py = g.n(ME.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False); link(wp, "", py, "")
tm = g.n(ME.MaterialExpressionTime)


def wave(kx, ky, speed, amp):
    """Returns (dh/dx, dh/dy) of  amp*sin(k.p + speed*t)  -> amp*k*cos(...)  (sine/cosine nodes have period 1, so k is in cycles per cm)."""
    arg = g.add(g.add(g.mul(px, g.c(kx)), g.mul(py, g.c(ky))), g.mul(tm, g.c(speed)))
    c = g.n(ME.MaterialExpressionCosine); link(arg, "", c, "")
    return g.mul(c, g.c(amp * kx)), g.mul(c, g.c(amp * ky))


waves = [wave(0.0042, 0.0011, 0.11, 1.0), wave(-0.0023, 0.0051, 0.14, 0.8), wave(0.0090, -0.0070, 0.21, 0.45), wave(-0.0150, -0.0110, 0.30, 0.25)]
sx = waves[0][0]; sy = waves[0][1]
for wx, wy in waves[1:]:
    sx = g.add(sx, wx); sy = g.add(sy, wy)
slope = g.n(ME.MaterialExpressionAppendVector)
link(g.mul(sx, g.s("WaveStrength", 14.0)), "", slope, "A"); link(g.mul(sy, g.s("WaveStrength", 14.0)), "", slope, "B")
nrm = g.n(ME.MaterialExpressionAppendVector)
link(slope, "", nrm, "A"); link(g.c(1.0), "", nrm, "B")
nn = g.n(ME.MaterialExpressionNormalize); link(nrm, "", nn, "")
prop(nn, "", unreal.MaterialProperty.MP_NORMAL)
# colour: deeper teal looking straight down, paler towards the horizon (cheap Fresnel tint)
cam = g.n(ME.MaterialExpressionCameraVectorWS)
vn = g.n(ME.MaterialExpressionDotProduct); link(cam, "", vn, "A"); link(g.n(ME.MaterialExpressionVertexNormalWS), "", vn, "B")
f1 = g.sub(g.c(1.0), g.sat(vn))
fres = g.mul(g.mul(f1, f1), f1)
deep = g.v("DeepColour", (0.006, 0.030, 0.042))
pale = g.v("PaleColour", (0.040, 0.090, 0.110))
prop(g.lerp(deep, pale, g.sat(fres)), "", unreal.MaterialProperty.MP_BASE_COLOR)
prop(g.s("Roughness", 0.05), "", unreal.MaterialProperty.MP_ROUGHNESS)
prop(g.s("Specular", 0.9), "", unreal.MaterialProperty.MP_SPECULAR)
g.done()
log("WATER_OK", "M_APEX_Water (animated wave normals)")
