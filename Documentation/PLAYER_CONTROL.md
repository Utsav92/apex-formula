# APEX FORMULA: player control

Backup before this work: git tag `pre-player-control` (commit 9fdeb9c).

## 1. Diagnosis (what was actually wrong)

Found by reading `FormulaVehiclePawn`, `APEXFormulaGameMode`, `APEXRaceManager`, `FormulaHUD` and the config before changing anything.

| Question from the brief | Finding |
|---|---|
| Who possesses the player car? | The engine's default `PlayerController` via `DefaultPawnClass` + `AutoPossessPlayer`. Possession worked. |
| Is an AI controller on it? | No. AI cars are `bIsAI` pawns driven by their own `ComputeAutoDrive`; the player car is not. |
| Why does it drive itself? | `bAutoDrive` is only set by the `-APEXAutoDrive` flag, which every automated test and screenshot run used. Launched without it the car was not auto-driving, but nothing else gave a usable player experience either. |
| How was manual input read? | A "Mac keyboard fallback" polled `IsInputKeyDown(W/A/S/D)` in the pawn's `Tick` and wrote 0/1 values directly: no Enhanced Input, no mapping context, no gamepad, no pause, no cameras, no driving-mode concept, no ramp (instant full lock). |
| Race manager overriding? | It only holds cars on the grid until the lights go out (`bHoldOnGrid`); it never writes the player's throttle or steering. |
| Cinematic / Sequencer / UI focus? | None of these steal input (there is no Sequencer intro); input mode was the default. |
| Chaos Vehicles? | Not used. The car is a deterministic grip-limited bicycle model by design; it was kept (the brief says not to replace a working vehicle system). |
| Steering law | Real defect: the model maps steering to a wheel angle and the grip clamp saturates yaw at tiny angles, so ~0.04 of input already reaches the limit at 70 m/s. Fine for the autopilot, twitchy for a person. Fixed for the player only (see below). |

So the "no real control" was an architecture gap, not a single blocked call: no controller, no input pipeline, and the only way anyone ran the game was the auto-drive flag.

## 2. What was built

* `Vehicles/APEXDrivingTypes.*`: `EAPEXDrivingMode {Manual, Assisted, AutonomousDemo}`, `EAPEXSteerPreset {Arcade, Sport, Simulation}`, `EAPEXCamView`, `EAPEXRaceFlow`, and `FAPEXDriveMath` (ramp, dead zone, speed-sensitive limit, gearbox, ownership rule).
* `Input/APEXInputConfig.*`: builds `IA_Throttle ... IA_Cancel` and `IMC_APEX_Driving` in code (keyboard + gamepad). Created at runtime, not as .uasset files.
* `Input/APEXPlayerController.*` (`AAPEXPlayerController`): possession, mapping install, Enhanced Input bindings, keyboard steering ramp, gamepad dead zones, driving mode / preset, pause + menu, camera switching, reset, disconnect handling, diagnostics, test harnesses.
* `Camera/APEXCameraManager.*`: C cycles Chase, Cockpit, Front wing, Rear, Nose, Cinematic; R shows Rear while held and restores the previous view.
* `AFormulaVehiclePawn`: driving mode (default **Manual**), single write point `SetDriverInputs()` (rejected for AI cars, demo mode, or a car held on the grid), gearbox (auto / manual with limiter and lugging), energy boost, overtaking aid, handbrake (arcade), assists (ABS / traction / stability), car-to-car contact with the AI, safe reset, five camera views, player steering law.
* `AFormulaHUD`: live telemetry from the real vehicle inputs, pause menu.
* The old key polling in the pawn was removed (the controller is the only writer). The broadcast director no longer reads C / R; its replay key is now **T**.

## 3. Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Throttle / Brake | W, Up / S, Down | Right trigger / Left trigger |
| Steer | A, D or Left, Right | Left stick X |
| Shift up / down | E / Q (manual gearbox: Simulation preset) | Right / Left bumper |
| Handbrake (arcade only) | Space | Right stick click |
| Energy boost | Left Shift | A |
| Overtaking aid | F | B |
| Camera cycle | C | Y |
| Look behind (hold) | R | X |
| Reset car | Backspace | View |
| Pause / menu | Escape (Up / Down / Enter in the menu) | Menu |
| Instant replay | T | |

Driving mode and preset: pause menu, Driving Assists. `-APEXAutoDrive` starts in AutonomousDemo (demo/test runs only).

## 4. Tests (all executed; logs are real output)

Run commands (headless, no GPU needed):

```
UnrealEditor-Cmd APEXFormula.uproject -ExecCmds="Automation RunTests APEX.; Quit" -unattended -nullrhi -nosplash -NoSound      # 36 unit tests
UnrealEditor-Cmd APEXFormula.uproject "/Engine/Maps/Entry?game=/Script/APEXFormula.APEXFormulaGameMode" -game -nullrhi -nosound -unattended -benchmark -fps=60 -APEXInputSelfTest
UnrealEditor-Cmd ... -APEXInputBot -APEXLaps=3      # scripted driver through injected key events
```

Neither harness uses the auto-drive flag. Both inject **real key / axis events at the PlayerController**, so Enhanced Input, the steering ramp, the speed limit and `SetDriverInputs()` are all exercised.

| Brief test | How it was run | Result |
|---|---|---|
| 1 Possession | self-test T1 | PASS |
| 2 Acceleration (W) | T2: 0 to 49 km/h in 2.5 s | PASS |
| 3 Braking (S) | T3: 49 to 0 km/h | PASS |
| 4 Steering (A / D, opposing keys) | T4a/b/c: +27 deg right, -11 deg left, A+D cancels | PASS |
| 5 Gear shift (E / Q) | T5 (manual gearbox) | PASS |
| 6 Camera (C, R) | T6: six views in order, wraps to Chase, R held shows Rear and restores | PASS |
| 7 Pause (Escape) | T7a-d: pauses, world frozen (0.00 cm moved), resumes, W works afterwards | PASS |
| 8 AI separation | T9: 19 AI cars, none owned by the player controller; ownership rule unit-tested | PASS |
| 9 Race completion | bot: 3 laps (69.2 / 60.4 / 60.2 s), classified 14th of 20 after real contacts with AI cars, positions and timing consistent | PASS |
| 10 Cinematic handoff | T6/T10: after Cinematic the view target is the player's car again. **There is no race-introduction Sequencer in this project, so only the camera hand-back was tested.** | PASS (limited) |
| Reset, gamepad, boost | T8a/b/c | PASS |

First required test (launch, W, A/D, S, complete a lap, AI keep racing): done by the bot, in Manual mode, with the 19 AI cars racing independently.

Regression: `-APEXAutoDrive` (AutonomousDemo) still gives 68.37 s / 60.17 s laps, identical to before.

## 5. Added after the first report

* **HUD**: tyre compound and wear, pit stops, live and last sector splits (purple = personal best; three equal parts of the lap distance, recorded only for cleanly driven sectors), race flag banner (green for the first 10 s, yellow when the car sits stopped, chequered at the finish).
* **Start menu**: interactive launches open on it (Start Race / Controls / Camera / Assists / Graphics / Audio / Quit) with the world paused; the pause menu now has "Main Menu" (restarts the level into the start menu) and "Exit Game". All automation flags (`-APEXAutoDrive`, `-APEXInputBot`, `-APEXInputSelfTest`, `-APEXNoMenu`, capture flags) skip it; `-APEXStartMenu` forces it.
* **Remapping**: Controls page, Enter on a row then press a key. Keyboard keys only; Escape, Enter, Tab, gamepad / mouse keys and keys already in use are refused. Saved to `GameUserSettings.ini` (section `APEXKeyBindings`) and reloaded on launch; "Reset to defaults" restores them.
* **Pre-start**: while the car is held on the grid the throttle revs the engine (HUD and audio), it never moves the car.
* **Race intro**: interactive launches fly the broadcast cameras for 4.5 s during the countdown, then hand the view back to the player's car (`-APEXIntro` forces it in tests, `-APEXNoIntro` disables it). This is code-driven, not a Sequencer asset.

New executed checks (self-test run with `-APEXInputSelfTest -APEXStartMenu -APEXIntro`, 36 of 36 pass): T10a/b intro plays then the player has camera and car, T11 pre-start rev, T12a-h remapping through the real menu (open, navigate, capture a key, saved, old key stops driving, new key drives, reset), T13a/b start menu and Start Race. 37 of 37 unit tests pass, including a rebind test.

A bug the test found and fixed: key capture first read the engine's `AnyKey` pseudo-key, which reads as pressed for any key, so the rebind went to `AnyKey` and every key became throttle.

## 6. Still NOT done / not verified

* **Never driven by a human.** The bot and self-test prove the chain; they cannot judge how the keyboard or pad *feels*. Ramp rates, steering authority and the presets are first guesses.
* **No real gamepad was connected.** Gamepad input was tested with injected axis events only. Rumble and the disconnect-pause are written but untested.
* **Canvas HUD, not UMG.** `WBP_APEX_RaceHUD` / `WBP_APEX_PauseMenu` assets do not exist. Time gap is the timing tower's; blue flags and sector deltas are not shown.
* **Remapping is keyboard-only**; gamepad bindings are fixed. Input Actions and the mapping context are created at runtime in code, not saved as `.uasset` files.
* **Not Chaos Vehicles / no wheel-slip or suspension simulation**: the existing grip-limited model plus the player steering law, gearbox, contacts and aids. The handbrake "slide" is a yaw multiplier.
* **Contact** is footprint overlap with momentum sharing (player vs AI only); AI-vs-AI cars do not touch and the AI do not defend against the player.
* **The race intro is not a Sequencer asset.**
* The start menu / pause menu Audio and Graphics pages change master volume, resolution scale and motion blur only.
