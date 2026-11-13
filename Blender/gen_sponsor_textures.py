"""Sponsor wordmark textures (1024 x 256 PNG) for every entry of Sponsors/registry.json. Plain text in Blender's built-in font on a flat colour:
for the real-brand entries these are PRIVATE-PROTOTYPE PLACEHOLDERS, not the brands' logos; for the fictional entries they are the project's own marks.

Run:  /Applications/Blender.app/Contents/MacOS/Blender -b -P Blender/gen_sponsor_textures.py
Out:  Sponsors/textures/T_Sponsor_<id>.png
"""
import json
import os

import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Sponsors", "textures")
os.makedirs(OUT, exist_ok=True)
reg = json.load(open(os.path.join(ROOT, "Sponsors", "registry.json")))

W, H = 1024, 256
items = [(sp, f"T_Sponsor_{sp['id']}") for sp in reg["sponsors"]] + [(a, f"T_Ann_{a['id']}") for a in reg.get("announcements", [])]
for sp, fname in items:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x, sc.render.resolution_y = W, H
    sc.render.image_settings.file_format = "PNG"
    sc.display.shading.light = "FLAT"
    sc.display.shading.color_type = "OBJECT"
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("w"); sc.world = world
    world.use_nodes = False; world.color = sp["bg"]
    sc.render.film_transparent = False
    cam_d = bpy.data.cameras.new("cam"); cam_d.type = "ORTHO"
    cam = bpy.data.objects.new("cam", cam_d); sc.collection.objects.link(cam); sc.camera = cam
    cam.location = (0, 0, 5)
    txt_d = bpy.data.curves.new("t", "FONT"); txt_d.body = sp["text"]; txt_d.align_x = "CENTER"; txt_d.align_y = "CENTER"
    txt_d.space_character = 1.15
    t = bpy.data.objects.new("t", txt_d); sc.collection.objects.link(t)
    t.color = (*sp["fg"], 1.0)
    bpy.context.view_layer.update()
    w, h = t.dimensions.x, t.dimensions.y
    s = min(0.84 * (W / H) / max(w, 1e-4), 0.62 / max(h, 1e-4))        # fit inside 84 % of the width and 62 % of the height (units: image height = 1)
    t.scale = (s, s, s)
    cam_d.ortho_scale = W / H
    sc.render.filepath = os.path.join(OUT, fname + ".png")
    bpy.ops.render.render(write_still=True)
    print("RENDERED", sp["id"], sp["text"])
