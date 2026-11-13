# 120 s showcase film — status (honest)

**What exists and was verified by running**
- `Cinematics/shotlist.json`: 42 contiguous shots tiling 0–120 s (7,200 frames at 60 fps), 11 scenes. Single source for the runtime director and the Sequencer generator.
- `AAPEXCinematicDirector` (`-APEXCinematic`, `-APEXCinematicCapture=<dir>`, `-APEXCinematicFrom/To`): cuts cameras per shot on the live race, switches lighting presets, holds the grid until 30 s, asks the leader to pit at 80 s, writes one PNG per frame at a fixed 60 Hz step (`-benchmark -fps=60`).
- Full 120 s capture at 960x540 completed: 7,200 frames (one, frame 6407, was not written and was duplicated from its neighbour) -> `Cinematics/render/APEX_FORMULA_NYC_DEMO_540P_SILENT_PREVIEW.mp4`, 120.00 s, 60 fps, **no audio**.
- `Python/ue_cinematic_sequence.py` creates `/Game/Cinematics/APEX_120s_Demo/` with `LS_APEX_NYC_120s_Master` (60 fps, 7,200 frames) and 42 shot sequences, each with a spawnable CineCameraActor.

**Not done / known defects**
- The Sequencer assets are a timing skeleton: lens values and camera-cut bindings were not applied (script logs `CINE_LENS_SKIP`), and camera motion lives in the C++ director, not in Sequencer keys. Not rendered through Movie Render Queue.
- No 1080p or 4K render, no audio/music/commentary mix, no broadcast overlays or title card, no Niagara rain/tyre-spray set, no recorded-trajectory replay (the race is simulated live; repeatability across runs is not proven).
- Pit-stop scene does not show a stop in the preview frames I checked (timing of the forced pit vs the shots is wrong); crews are still mannequin stand-ins, not MetaHumans.
- Rain scene shows no visible rain; lighting presets for scenes 9–11 look like plain daylight.
- Rank-1 car dominates most shots after the start. A black-rectangle artefact appears in the wheel macro shot. A renderer ensure fired once (NaN bounds on the city-steam FX).
- Livery upgrade brief (per-team liveries for 20 cars): not started.
