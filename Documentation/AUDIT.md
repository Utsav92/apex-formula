# APEX Formula: project audit (2026-11-12)

Checkpoint: git tag `pre-aaa-environment` (commit a696984). Hardware: Apple M5 Pro, 24 GB, Metal 4, UE 5.8.4.
Direction now: **desktop only**. iOS/mobile work is parked (mobile sky dome and `_Mobile` paint permutations still exist
but are unused by default).

## What works (verified)
- Procedural 3,508 m circuit (designed layout, hairpin, chicane), collision on invisible cube proxies, 10 checkpoints.
- Car: 3.4k-poly original model, livery, decals, 4 spinning/steering wheels. Grip-limited physics, three-lap auto-drive 59.6/57.0/57.0 s.
- Touch controls, canvas HUD, lighting director (5 presets, calibrated exposure, validated), car-paint masters, 18 automation tests.
- NEW this pass: scanned CC0 PBR (asphalt, concrete, grass, gravel) on road/barrier/ground; desktop Lumen GI + Lumen
  reflections + Virtual Shadow Maps + TSR enabled and rendering on this Mac.

## Visibly primitive / missing (ranked)
1. **Trees** - pine = 64 polys of stacked cones, oak = 410 polys of lumpy spheres, flat green leaf material, no alpha foliage, no wind. FAILS the quality gate.
2. **Buildings** - 4 box variants (20-30 polys) with a repeating generated facade; no balconies, rooftops, interiors, trim. FAILS.
3. **Crowd** - texture-painted cells on the grandstand risers; no characters, no animation. FAILS.
4. **Opponents** - none. One car, driven by an auto-driver. No 20-car grid, teams, AI, pit stops.
5. **Track geometry** - flat (Z=0), no banking/camber, no pit lane, gravel traps, tyre walls, fences; barriers are one extruded profile.
6. **Trackside** - 3 billboard/pit/gantry textures generated but not placed; no advertising system, marshals, photographers.
7. **Audio / commentary / broadcast cameras / replay** - none.
8. **Weather VFX** - wet asphalt works; no rain/spray/smoke Niagara.
9. **Background** - 94 box buildings and trees on a flat plane; no terrain, mountains, sea, HLOD or World Partition.

## Measured
- Frame time on this Mac (Lumen + VSM + TSR, 1280x720, light scene): avg ~20 ms (50 fps) after a ~0.4 s first-frame shader hitch. Not a 4K/60 claim.
- Triangle counts: car 3.4k, track 3.3k, pine 64, oak 410, buildings 20-30, grandstand 105 (+2,356 trees, 94 buildings, 4 stands instanced).
- Textures: road scan 2K, barrier/ground 2K, generated facade 512x1024 (low), crowd 512x512 (low).

## Update: trees and Nanite (2026-11-12)
- Trees: the cone/sphere trees are replaced by three real CC0 Poly Haven models (Jacaranda, Pine Sapling, Fir Sapling), imported as
  Nanite meshes with alpha-masked, two-sided, wind-animated foliage materials (M_APEX_TreeBark, M_APEX_Leaves; seasonal tint param).
  Remaining weaknesses: the pine/fir are ~1.3 m SAPLINGS (used as understory, ~4 m), distant trees read bare (Nanite thins masked
  foliage at range), only 3 species (palms, cypress, olive, oak, birch, spruce still missing), density near the track is low.
- Nanite enabled on: trees, track mesh, 4 building meshes, grandstand. Not on: car (clear-coat/decal materials untested with Nanite).
- Bug found and fixed on the way: a Normal-sampler texture parameter with no default texture fails shader compilation
  ("Sampler type is Normal, should be Color") and the engine silently shows its default material.
- Measured: avg 20.0 ms (50 fps) at 1280x720, ~2,400 Nanite trees, Lumen + VSM + TSR, M5 Pro.

## Update: 20-car grid (2026-11-12)
Built (all verified by running it, see logs/screens in `Documentation/screenshots/grid/`):
- 10 fictional teams, 20 fictional drivers (`Racing/APEXRaceTypes.*`): unique numbers/codes/names/liveries, per-driver skill/aggression/consistency, per-team pace. Player = AMR #27.
- `AAPEXRaceManager`: spawns 19 AI cars on a 2-column staggered grid (player on slot 12, override `-APEXGridSlot=N`), 6 s countdown with 5 start lights, lights-out release, 10 Hz awareness/standings, finish classification, weather pace scale, AI level-of-detail.
- AI (`AFormulaVehiclePawn`): same physics as the player; pace-scaled racing line, speed plan on the curvature of the line actually driven, safe-gap following, hard no-pass-through, overtaking (only on straights), defending, slipstream (up to -25 % drag), side-by-side separation, lane-holding on the grid, driver mistakes, stuck recovery.
- Per-car livery (team colours via dynamic materials) and number roundel (`T_DecalNum_<n>`), live timing tower, start-light gantry on the HUD, desktop HUD hides touch buttons (`-APEXTouchUI` shows them).
- Distant AI ticks at 1/30 or 1/15 s with 60 Hz sub-stepping inside the car. Throttled vs full-rate: player laps 68.05/59.48/58.42 vs 68.05/59.45/58.45 s, same finishing order apart from two back-markers.

Measured (headless, 3 laps, 20 cars, player P13): 0 recoveries; 16 car-pair overlaps in 1,895 samples (peak 1 pair); single-car regression unchanged 59.57/57.00/57.00 s; 20/20 automation tests pass; ~20 ms/frame (50 fps) rendered at 1280x720 with all 20 cars.

Bugs found by the race (and fixed): controller offset vs measured lateral shared one variable (broke even the solo lap); following ignored faster leaders; the nearest car ahead hid the same-lane leader in the next lane.

Not done yet: physical car-to-car contact (cars pass through each other, avoidance is behavioural), qualifying, championship points, pit stops/tyre strategy/weather tyre choice, formation lap, per-team car geometry (one mesh, recoloured), driver helmets/names in 3D, engine audio, Chaos-based physics for AI, AI-specific LOD meshes.
