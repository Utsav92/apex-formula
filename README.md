# APEX FORMULA

Single-developer Unreal Engine 5.8 Formula-style racing game set on a fictional New York street circuit (Mac, Metal; desktop rendering).

- Source: `Source/APEXFormula` (C++), generators: `Blender/`, `Python/`, `Audio/`, `Sponsors/`. Docs: `Documentation/`.
- Cinematic showcase: `Cinematics/shotlist.json`, `Cinematics/render_4k.sh` (see `Documentation/CINEMATIC.md`).
- **Private prototype.** The sponsor registry contains plain-text mock-ups of real brand names as unlicensed placeholders; they are shown only with `-APEXAllowPlaceholders` and are blocked in distribution builds (fictional fallbacks are used).
- **Missing from this repo:** `Content/Environments/Trees/SM_Tree_Jacaranda.uasset` (119 MB, over GitHub's 100 MB file limit). Regenerate it with the Blender/Python tree scripts, or the tree actors will fall back to the other tree meshes.
- The 4K film is attached to the repository's Releases page, not committed.
