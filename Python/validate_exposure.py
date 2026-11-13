"""Exposure validation for rendered frames. Run with Blender (it ships numpy):

  /Applications/Blender.app/Contents/MacOS/Blender -b -P Python/validate_exposure.py -- sunny.png golden.png ...

For each image (file stem selects the expectations) it prints mean luma, clipped-highlight % and crushed-shadow %,
and PASS/FAIL against the limits below. Values are in display (sRGB-encoded) space, 0..1.
"""
import os
import sys

import bpy
import numpy as np

# name -> (mean_min, mean_max, max_clipped_pct, max_crushed_pct)
LIMITS = {
    "sunny":  (0.22, 0.60, 4.0, 25.0),
    "golden": (0.18, 0.55, 4.0, 35.0),
    "cloudy": (0.22, 0.60, 3.0, 20.0),
    "rainy":  (0.15, 0.45, 2.0, 35.0),
    "night":  (0.03, 0.30, 3.0, 85.0),
}

paths = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
failed = 0
for path in paths:
    img = bpy.data.images.load(path)
    img.colorspace_settings.name = "Non-Color"      # read raw 0..1 values, no conversion
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[..., :3]
    luma = 0.2126 * px[..., 0] + 0.7152 * px[..., 1] + 0.0722 * px[..., 2]
    mean = float(luma.mean())
    clipped = float((px.max(axis=2) >= 0.985).mean() * 100)
    crushed = float((luma < 0.03).mean() * 100)
    p5, p95 = (float(np.percentile(luma, q)) for q in (5, 95))
    key = next((k for k in LIMITS if k in os.path.basename(path).lower()), None)
    verdict = ""
    if key:
        lo, hi, cmax, kmax = LIMITS[key]
        ok = lo <= mean <= hi and clipped <= cmax and crushed <= kmax
        verdict = "PASS" if ok else "FAIL"
        failed += 0 if ok else 1
        verdict += f"  (mean {lo}-{hi}, clipped<={cmax}%, crushed<={kmax}%)"
    print(f"EXPOSURE {os.path.basename(path):<14} mean={mean:.3f} p5={p5:.3f} p95={p95:.3f} clipped={clipped:.2f}% crushed={crushed:.2f}% {verdict}")
print(f"EXPOSURE_SUMMARY images={len(paths)} failed={failed}")
