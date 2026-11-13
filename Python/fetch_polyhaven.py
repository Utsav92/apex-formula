"""Download CC0 Poly Haven models (FBX + textures) and merge them into Documentation/ASSET_MANIFEST.{json,md}.

  python3 Python/fetch_polyhaven.py
Checks the CC0 licence page first. Downloads to Blender/sources/polyhaven/<id>/ (git-ignored).
"""
import json
import os
import re
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(ROOT, "Blender", "sources", "polyhaven")
RES = "2k"
# id -> (use, species note)
WANTED = {
    "jacaranda_tree": ("broadleaf hero tree (flowering/ornamental)", "Jacaranda"),
    "pine_sapling_small": ("pine", "Pine"),
    "fir_sapling": ("alpine fir", "Fir"),
}


def get(url, timeout=120):
    req = urllib.request.Request(url, headers={"User-Agent": "apex-formula-asset-fetch/1.0"})
    return urllib.request.urlopen(req, timeout=timeout).read()


text = get("https://polyhaven.com/license").decode("utf-8", "ignore")
if not re.search(r"CC0|Creative Commons Zero", text, re.I):
    sys.exit("Poly Haven licence page did not state CC0; not downloading.")
print("LICENCE Poly Haven = CC0 (verified from polyhaven.com/license)")

entries = []
for aid, (use, species) in WANTED.items():
    e = {"id": aid, "use": use, "source": f"https://polyhaven.com/a/{aid}", "licence": "CC0 (verified from polyhaven.com/license)",
         "resolution": RES, "status": "failed"}
    try:
        info = json.loads(get(f"https://api.polyhaven.com/info/{aid}"))
        files = json.loads(get(f"https://api.polyhaven.com/files/{aid}"))
        e["authors"] = list(info.get("authors", {}).keys())
        e["polycount"] = info.get("polycount")
        fb = files["fbx"][RES]["fbx"]
        out = os.path.join(DEST, aid)
        os.makedirs(out, exist_ok=True)
        fbx_path = os.path.join(out, f"{aid}_{RES}.fbx")
        if not os.path.exists(fbx_path):
            open(fbx_path, "wb").write(get(fb["url"], timeout=600))
        for rel, meta in fb.get("include", {}).items():
            p = os.path.join(out, rel)
            if not os.path.exists(p):
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, "wb").write(get(meta["url"], timeout=300))
        e["files"] = sorted(os.path.relpath(os.path.join(dp, f), out) for dp, _, fs in os.walk(out) for f in fs)
        e["status"] = "downloaded"
        print("FETCH_OK", aid, f"{os.path.getsize(fbx_path)/1e6:.1f} MB fbx,", len(e["files"]) - 1, "textures, authors:", e["authors"])
    except Exception as ex:  # noqa: BLE001
        e["error"] = str(ex)
        print("FETCH_FAIL", aid, ex)
    entries.append(e)

mj = os.path.join(ROOT, "Documentation", "ASSET_MANIFEST.json")
m = json.load(open(mj))
m["assets"] = [a for a in m["assets"] if a["id"] not in WANTED] + entries
m.setdefault("licence_checks", {})["PolyHaven"] = True
json.dump(m, open(mj, "w"), indent=2)
with open(os.path.join(ROOT, "Documentation", "ASSET_MANIFEST.md"), "w") as f:
    f.write("# Asset manifest\n\nThird-party assets only. Everything produced by `Blender/gen_*.py` is original and generated here.\n\n")
    f.write("| Asset | Use | Source | Licence | Resolution | Authors | Status |\n|---|---|---|---|---|---|---|\n")
    for a in m["assets"]:
        f.write(f"| {a['id']} | {a['use']} | {a['source']} | {a['licence']} | {a['resolution']} | {', '.join(a.get('authors', [])) or '-'} | {a['status']} |\n")
    f.write("\nUnreal import status is printed by the `Python/ue_*` scripts (`PBR_OK`, `TREE_OK`).\n")
print("MANIFEST updated")
