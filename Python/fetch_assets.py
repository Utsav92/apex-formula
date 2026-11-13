"""Download CC0 PBR texture sets and write an asset manifest. Verifies the licence text before downloading.

  python3 Python/fetch_assets.py

Sources: ambientCG (https://ambientcg.com, CC0 1.0). Poly Haven (https://polyhaven.com, CC0) is checked too.
Downloads go to Blender/sources/ (git-ignored); the manifest is written to Documentation/ASSET_MANIFEST.{json,md}.
"""
import hashlib
import io
import json
import os
import re
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Blender", "sources", "ambientcg")
os.makedirs(SRC, exist_ok=True)

# id -> what it is used for in the game
WANTED = {
    "Asphalt031": "road surface (base layer)",
    "Concrete034": "track barriers / walls",
    "Grass004": "trackside ground",
    "Gravel040": "run-off areas and pit lane edges",
    "PaintedPlaster017": "Mediterranean building walls",
    "Bricks105": "apartment block facades",
    "RoofingTiles013A": "terracotta roofs",
    "WoodSiding008": "alpine lodge cladding",
    "Concrete036": "office block concrete",
    "CorrugatedSteel009": "garage and utility roofs",
}
RESOLUTION = "2K-JPG"


def get(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": "apex-formula-asset-fetch/1.0"})
    return urllib.request.urlopen(req, timeout=timeout).read()


def licence_ok():
    """Read the licence pages and require the CC0 text. Returns {site: bool}."""
    out = {}
    for site, url in (("ambientCG", "https://docs.ambientcg.com/license/"), ("PolyHaven", "https://polyhaven.com/license")):
        try:
            text = get(url).decode("utf-8", "ignore")
            out[site] = bool(re.search(r"CC0|Creative Commons Zero|public domain", text, re.I))
        except Exception as e:  # noqa: BLE001
            print(f"LICENCE_CHECK_FAILED {site}: {e}")
            out[site] = False
    return out


lic = licence_ok()
print("LICENCE", lic)
if not lic.get("ambientCG"):
    sys.exit("ambientCG licence could not be verified as CC0; not downloading.")

manifest = []
for aid, use in WANTED.items():
    entry = {"id": aid, "use": use, "source": f"https://ambientcg.com/view?id={aid}", "licence": "CC0 1.0 (verified from docs.ambientcg.com/license)",
             "resolution": RESOLUTION, "status": "failed"}
    try:
        meta = json.loads(get(f"https://ambientcg.com/api/v2/full_json?id={aid}&include=downloadData"))
        asset = next(a for a in meta["foundAssets"] if a["assetId"] == aid)
        dl = next(d for d in asset["downloadFolders"]["default"]["downloadFiletypeCategories"]["zip"]["downloads"] if d["attribute"] == RESOLUTION)
        entry["download_url"] = dl["fullDownloadPath"]
        dest = os.path.join(SRC, aid)
        if not os.path.isdir(dest) or not os.listdir(dest):
            data = get(dl["fullDownloadPath"], timeout=180)
            entry["sha256"] = hashlib.sha256(data).hexdigest()
            os.makedirs(dest, exist_ok=True)
            zipfile.ZipFile(io.BytesIO(data)).extractall(dest)
        files = sorted(os.listdir(dest))
        entry["files"] = [f for f in files if f.lower().endswith((".jpg", ".png"))]
        entry["tags"] = asset.get("tags", [])[:8]
        entry["status"] = "downloaded"
        print("FETCH_OK", aid, entry["files"])
    except Exception as e:  # noqa: BLE001
        entry["error"] = str(e)
        print("FETCH_FAIL", aid, e)
    manifest.append(entry)

doc = os.path.join(ROOT, "Documentation")
with open(os.path.join(doc, "ASSET_MANIFEST.json"), "w") as f:
    json.dump({"licence_checks": lic, "assets": manifest}, f, indent=2)
with open(os.path.join(doc, "ASSET_MANIFEST.md"), "w") as f:
    f.write("# Asset manifest\n\nThird-party assets only. Everything under `Blender/gen_*.py` is original and generated here.\n\n")
    f.write("| Asset | Use | Source | Licence | Resolution | Status |\n|---|---|---|---|---|---|\n")
    for e in manifest:
        f.write(f"| {e['id']} | {e['use']} | {e['source']} | {e['licence']} | {e['resolution']} | {e['status']} |\n")
    f.write("\nUnreal import status is recorded by `Python/ue_pbr_materials.py` (look for `PBR_OK` lines).\n")
print("MANIFEST written")
