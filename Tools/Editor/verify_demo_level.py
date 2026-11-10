# Checks L_MuseumDemo after Tools/Editor/build_demo_level.py: the actors the demo script names, each exhibit's wiring
# (zone, recording, card zone), the passthrough actor, the plate material (with the MR detection parameters), the MR
# materials (Tools/Editor/build_mr_assets.py) and the Path Guide places. Prints "[MuseumCheck] OK" or each failure.
import unreal

LOG = "[MuseumCheck]"
assets = unreal.EditorAssetLibrary
failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        unreal.log_error(f"{LOG} FAIL {message}")


unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Museum/Maps/L_MuseumDemo")
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
unreal.log(f"{LOG} actors: {sorted(actors)}")

for name in ("Floor", "NavBounds", "PlayerStart", "Passthrough", "PostProcess_Passthrough"):
    check(name in actors, f"actor {name} missing")
passthrough = actors.get("Passthrough")
if passthrough:
    check(passthrough.get_editor_property("plate_material") is not None, "passthrough has no plate material")
    check(len(passthrough.get_editor_property("walk_clips")) == 3, "passthrough walk clips != 3")

    material = passthrough.get_editor_property("plate_material")
    if material:
        names = [str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(material)]
        for name in ("DetectAmount", "DetectTime", "RectU0", "RectV1", "ContentScale"):
            check(name in names, f"plate material has no {name} (has {names})")
for name in ("M_MR_Glass", "M_MR_Glow", "M_MR_Ring"):
    check(assets.does_asset_exist(f"/Game/Museum/MR/{name}"), f"MR material {name} missing (run build_mr_assets.py)")

places = assets.load_asset("/Game/Museum/PathGuide/DA_PG_Places_Museum")
place_tags = {str(p.get_editor_property("place_tag").get_editor_property("tag_name")) for p in places.get_editor_property("places")}
for number in range(1, 7):
    exhibit = actors.get(f"Exhibit{number:02d}")
    station = actors.get(f"Station{number:02d}")
    check(exhibit is not None and station is not None and f"Gaze{number:02d}" in actors and f"Zone{number:02d}" in actors,
          f"station {number:02d} actors missing")
    if not exhibit or not station:
        continue
    data = exhibit.get_editor_property("data")
    check(data is not None and data.get_editor_property("number") == number, f"Exhibit{number:02d} data")
    check(exhibit.get_editor_property("viewing_zone") == actors.get(f"Zone{number:02d}"), f"Exhibit{number:02d} zone")
    video = exhibit.get_editor_property("plate_video")
    # File media sources keep paths under Content relative to it ("./Movies/...")
    video_file = unreal.Paths.combine([unreal.Paths.project_content_dir(), video.get_editor_property("file_path")]) if video else ""
    check(video is not None and unreal.Paths.file_exists(video_file), f"Exhibit{number:02d} plate video file {video_file}")
    tag = str(station.get_editor_property("place_tag").get_editor_property("tag_name"))
    check(tag == str(data.get_editor_property("place_tag").get_editor_property("tag_name")) and tag in place_tags,
          f"Station{number:02d} place tag {tag}")
    zone = exhibit.get_editor_property("panel_zone")
    width = abs(zone.z - zone.x)
    check(width >= 29.0 and abs(zone.x) <= 53.0 and abs(zone.z) <= 53.0, f"Exhibit{number:02d} card zone {zone}")
    unreal.log(f"{LOG} {number:02d} tag={tag} content={exhibit.get_editor_property('plate_content_scale')} zone=({zone.x:.1f},{zone.y:.1f},{zone.z:.1f},{zone.w:.1f}) "
               f"hologram_scale={data.get_editor_property('hologram_scale'):.3f} video={video.get_editor_property('file_path')}")

unreal.log(f"{LOG} OK" if not failures else f"{LOG} {len(failures)} failures")
