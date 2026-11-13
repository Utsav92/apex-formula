"""Builds Content/Cinematics/APEX_120s_Demo/ from Cinematics/shotlist.json:
 - LS_APEX_NYC_120s_Master (60 fps, 7,200 frames), a Camera Cuts track and a Shots (subsequence) track
 - one shot sequence per entry with a spawnable CineCameraActor (focal length from the shot's FOV, 24 mm-class filmback)
The camera *motion* of the rendered preview is produced by AAPEXCinematicDirector at runtime from the same shot list; the sequence holds the editable
timing, cuts and lens data (see Documentation/CINEMATIC.md)."""
import json, os, math
import unreal

ROOT = "/Game/Cinematics/APEX_120s_Demo"
proj = unreal.SystemLibrary.get_project_directory()
shots = json.load(open(os.path.join(proj, "Cinematics", "shotlist.json")))["shots"]
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FPS = 60
for d in ("MasterSequence", "ShotSequences", "Cameras", "CameraRigs", "LightingPresets", "WeatherPresets", "VehicleAnimation", "RaceSimulation", "PitStop", "Audio", "VFX", "Overlays", "RenderPresets"):
    eal.make_directory(f"{ROOT}/{d}")

def make_seq(name, folder, frames):
    full = f"{ROOT}/{folder}/{name}"
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    s = tools.create_asset(name, f"{ROOT}/{folder}", unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    s.set_display_rate(unreal.FrameRate(FPS, 1))
    s.set_tick_resolution(unreal.FrameRate(FPS * 100, 1)) if hasattr(s, "set_tick_resolution") else None
    s.set_playback_start(0)
    s.set_playback_end(frames)
    return s

def fov_to_focal(fov, sensor_w=36.0):
    return sensor_w / (2.0 * math.tan(math.radians(fov) / 2.0))

master = make_seq("LS_APEX_NYC_120s_Master", "MasterSequence", 7200)
sub_track = master.add_track(unreal.MovieSceneSubTrack)
sub_track.set_display_name("Shots")
cut_track = master.add_track(unreal.MovieSceneCameraCutTrack)
cut_track.set_display_name("Camera Cuts")
count = 0
for sh in shots:
    f0 = int(round(sh["start"] * FPS)); f1 = int(round((sh["start"] + sh["duration"]) * FPS))
    seq = make_seq("LS_" + sh["name"], "ShotSequences", f1 - f0)
    cam = seq.add_spawnable_from_class(unreal.CineCameraActor)
    cam.set_name("CAM_" + sh["name"])
    comp = None
    try:
        tmpl = cam.get_object_template()
        comp = tmpl.get_cine_camera_component() or tmpl.get_component_by_class(unreal.CineCameraComponent)
        comp.set_editor_property("current_focal_length", fov_to_focal(sh["fov"]))
        comp.set_editor_property("current_aperture", 2.8 if sh["fov"] < 40 else 5.6)
        fb = comp.get_editor_property("filmback"); fb.sensor_width = 36.0; fb.sensor_height = 20.25; comp.set_editor_property("filmback", fb)
    except Exception as e:
        unreal.log_warning(f"CINE_LENS_SKIP {sh['name']}: {e}")
    # camera cut inside the shot sequence
    ct = seq.add_track(unreal.MovieSceneCameraCutTrack)
    sec = ct.add_section(); sec.set_range(0, f1 - f0)
    try:
        sec.set_camera_binding_id(seq.make_binding_id(cam, unreal.MovieSceneObjectBindingSpace.LOCAL)) if hasattr(seq, "make_binding_id") else None
    except Exception as e:
        unreal.log_warning(f"CINE_BIND_SKIP {sh['name']}: {e}")
    ss = sub_track.add_section()
    ss.set_sequence(seq); ss.set_range(f0, f1)
    ss.set_editor_property("parameters", ss.get_editor_property("parameters"))
    eal.save_asset(seq.get_path_name().split('.')[0])
    count += 1
eal.save_asset(f"{ROOT}/MasterSequence/LS_APEX_NYC_120s_Master")
unreal.log(f"CINE_OK shots={count} frames=7200 fps={FPS}")
