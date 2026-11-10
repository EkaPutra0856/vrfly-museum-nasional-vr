# Builds the demo level /Game/Museum/Maps/L_MuseumDemo (rebuilt from scratch on every run) and what it needs:
#   - M_PassthroughPlate: the recordings' passthrough look (grain, desaturation, vignette, blurred fill around a scaled recording)
#     and the MR detection scan over the real object (AMuseumExhibit::GetDetectAmount, set by AMuseumPassthrough),
#   - FMS_Plate* / FMS_Walk*: media sources of Tools/Video/stabilize_plates.py's output, DA_PG_Places_Museum: the 6 Path Guide places.
# Level: an invisible floor (collision, NavMesh built at runtime), lights for the hands, a post process volume, the player start,
# AMuseumPassthrough, and 6 stations on a path turning 45 degrees at each one, so the visitor arrives facing the object. Per station:
#   StationNN  AWzPathGuideDestination at the viewing spot      ZoneNN   ATriggerBox there (the visit starts inside it)
#   ExhibitNN  AMuseumExhibit, X pointing away from the viewer   GazeNN   ATargetPoint the demo looks at (keeps the view on the plate)
# Placement (angles seen from the viewing spot): the plate covers PLATE_FOV degrees; the object's box (plates.json) gives the
# real-object box; the information cards get a zone beside the object (PanelZone, on its freer side) and the hologram twin goes
# on the other side, none covering the object or each other, shrinking the recording on its plate (PlateContentScale) when a
# large object leaves no room. Editor must be CLOSED.
import json
import math
import os

import unreal

LOG = "[MuseumLevel]"
LEVEL = "/Game/Museum/Maps/L_MuseumDemo"
ROOT = "/Game/Museum"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

EYE = 165.0            # eye height of the demo visitor (MuseumVRDemoScript EyeHeight)
PLATE_DISTANCE = 1500.0
PLATE_FOV = 150.0      # horizontal angle the station plate covers from the viewing spot (the view is 90: room for head turns)
MAX_CONTENT_FOV = 110.0  # the sharp recording spans at most this (the blurred fill covers the rest of the plate)
ASPECT = 16.0 / 9.0
REAL_DEPTH = 400.0     # depth of the (invisible) real-object box; only its outline seen from the spot matters
HOLOGRAM_DEPTH = 220.0
VIEW_YAW = 52.0        # items stay inside this (degrees from the plate centre; the head turns to the cards to read) ...
VIEW_PITCH = 25.0
GAZE_YAW = 20.0        # ... and the head never turns further than this (the plate edge stays out of view)
GAZE_PITCH = 8.0
MARGIN = 2.5           # angular gap between object, hologram and cards
ZONE_WIDTHS = (38.0, 36.0, 34.0, 32.0, 30.0)   # card zone width (degrees), widest that fits; the cards scale to it (readable from ~32)
ZONE_PITCH = 17.0      # card zone from -ZONE_PITCH to +ZONE_PITCH
ARRIVAL_RADIUS = 150.0
ZONE_EXTENT = unreal.Vector(60.0, 60.0, 150.0)


def log(message):
    unreal.log(f"{LOG} {message}")


def get_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def save(*objects):
    for obj in objects:
        assets.save_loaded_asset(obj, only_if_is_dirty=False)


def tag(name):
    value = unreal.GameplayTag()
    value.import_text(f'(TagName="{name}")')
    return value


# ====== Assets ====== //

PLATE_HLSL = r"""
float s = max(ContentScale, 0.05);
float2 inner = (UV - 0.5) / s + 0.5;
float3 sharp = Texture2DSampleLevel(Tex, TexSampler, saturate(inner), 0).rgb;

// Around a shrunk recording: the recording stretched over the plate, blurred and dimmed
float3 fill = 0;
for (int i = 0; i < 12; i++)
{
    float a = 6.2831853 * (i + 0.5) / 12.0;
    float2 o = float2(cos(a), sin(a) * 1.7777) * FillBlur;
    fill += Texture2DSampleLevel(Tex, TexSampler, saturate(UV + o), 0).rgb;
    fill += Texture2DSampleLevel(Tex, TexSampler, saturate(UV + o * 0.45), 0).rgb;
}
fill = fill / 24.0 * FillDim;
float2 edge = abs(inner - 0.5) - 0.5;
float3 c = lerp(sharp, fill, smoothstep(-0.09, 0.0, max(edge.x, edge.y)));

// Passthrough look
float luma = dot(c, float3(0.2126, 0.7152, 0.0722));
c = lerp(c, luma.xxx, Desaturate);
c = (c - 0.5) * Contrast + 0.5;
c *= Brightness;
float2 d = (UV - 0.5) * float2(1.0, 0.5625) * 2.0;
c *= 1.0 - Vignette * smoothstep(0.55, 1.15, length(d));
float2 cell = floor(UV * float2(960.0, 540.0)) + floor(Time * 30.0) * float2(17.0, 59.0);
float n = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
c += (n - 0.5) * Grain;
c = max(c, 0.0);
if (DetectAmount <= 0.001) return c;

// MR detection, kept calm: only the 3D scan climbing the real object (a light band painting a fine triangle grid,
// the object's edges lit where the band passes). No darkening, no rings or brackets around it.
float t = DetectTime;
float2 asp = float2(1.7777, 1.0);
float2 rmin = float2(RectU0, RectV0);
float2 rmax = float2(RectU1, RectV1);
float2 rc = (rmin + rmax) * 0.5;
float2 rh = max((rmax - rmin) * 0.5, 0.002);
float2 q = abs(UV - rc) * asp - rh * asp;
float sd = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
float inside = 1.0 - smoothstep(-0.01, 0.01, sd);
float3 cyan = float3(0.06, 0.62, 1.0);

// Edges of the object (Sobel on the recording)
float2 px = float2(1.6 / 1920.0, 1.6 / 1080.0) / s;
float3 w = float3(0.2126, 0.7152, 0.0722);
float l00 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(-1, -1), 0).rgb, w);
float l10 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(0, -1), 0).rgb, w);
float l20 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(1, -1), 0).rgb, w);
float l01 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(-1, 0), 0).rgb, w);
float l21 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(1, 0), 0).rgb, w);
float l02 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(-1, 1), 0).rgb, w);
float l12 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(0, 1), 0).rgb, w);
float l22 = dot(Texture2DSampleLevel(Tex, TexSampler, inner + px * float2(1, 1), 0).rgb, w);
float gx = (l20 + 2.0 * l21 + l22) - (l00 + 2.0 * l01 + l02);
float gy = (l02 + 2.0 * l12 + l22) - (l00 + 2.0 * l10 + l20);
float edges = saturate(length(float2(gx, gy)) * 2.6 - 0.12);

// The band climbs from the base to the top; the grid stays faintly where it has passed
float h = saturate((rmax.y - UV.y) / max(rmax.y - rmin.y, 0.002));
float climb = frac(t * 0.42) * 1.25 - 0.1;
float band = exp(-pow((h - climb) * 16.0, 2.0));
float2 g = (UV - rc) * asp * 38.0;
float f1 = abs(frac(g.y) - 0.5);
float f2 = abs(frac(g.x * 0.866 + g.y * 0.5) - 0.5);
float f3 = abs(frac(-g.x * 0.866 + g.y * 0.5) - 0.5);
float grid = smoothstep(0.44, 0.49, max(f1, max(f2, f3)));
float3 res = c;
res += cyan * inside * band * (0.35 + 1.1 * edges);
res += cyan * inside * grid * (0.45 * band + 0.08 * step(h, climb));

return lerp(c, max(res, 0.0), saturate(DetectAmount));
"""

PLATE_SCALARS = {"ContentScale": 1.0, "FillBlur": 0.05, "FillDim": 0.55, "Desaturate": 0.18, "Contrast": 0.95,
                 "Brightness": 1.1, "Vignette": 0.3, "Grain": 0.035,
                 "DetectAmount": 0.0, "DetectTime": 0.0, "RectU0": 0.4, "RectV0": 0.3, "RectU1": 0.6, "RectV1": 0.8}


def plate_material():
    path = f"{ROOT}/Passthrough/M_PassthroughPlate"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = tools.create_asset("M_PassthroughPlate", f"{ROOT}/Passthrough", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    video = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1000, -200)
    video.set_editor_property("parameter_name", "Video")
    default = assets.load_asset("/Engine/EngineResources/Black") or assets.load_asset("/Engine/EngineResources/DefaultTexture")
    video.set_editor_property("texture", default)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1000, 100)

    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -450, 0)
    custom.set_editor_property("code", PLATE_HLSL)
    custom.set_editor_property("description", "PassthroughLook")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["Tex", "UV", "Time"] + list(PLATE_SCALARS)
    inputs = []
    for n in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", n)
        inputs.append(entry)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(video, "", custom, "Tex")
    mel.connect_material_expressions(uv, "", custom, "UV")
    mel.connect_material_expressions(time, "", custom, "Time")
    for i, (name, value) in enumerate(PLATE_SCALARS.items()):
        param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 200 + i * 70)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        mel.connect_material_expressions(param, "", custom, name)
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    save(material)
    log(f"material {material.get_path_name()} inputs={names}")
    return material


def media_source(name, relative_file):
    source = get_or_create(name, f"{ROOT}/Media", unreal.FileMediaSource, unreal.FileMediaSourceFactoryNew())
    source.set_file_path(os.path.join(unreal.Paths.project_content_dir(), relative_file))
    save(source)
    return source


def places_asset(exhibits):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.WzPathGuidePlacesAsset)
    places = get_or_create("DA_PG_Places_Museum", f"{ROOT}/PathGuide", unreal.WzPathGuidePlacesAsset, factory)
    entries = []
    for data in exhibits:
        place = unreal.PathGuidePlace()
        place.set_editor_property("place_tag", data.get_editor_property("place_tag"))
        place.set_editor_property("display_name", data.get_editor_property("name").get_editor_property("indonesian"))
        place.set_editor_property("arrival_radius", ARRIVAL_RADIUS)
        entries.append(place)
    places.set_editor_property("places", entries)
    save(places)
    log(f"places {places.get_path_name()} = {len(entries)}")
    return places


# ====== Placement (angles from the viewing spot, degrees; yaw + right, pitch + up) ====== //

def plate_extent():
    width = 2.0 * PLATE_DISTANCE * math.tan(math.radians(PLATE_FOV / 2.0))
    return width, width / ASPECT


def frame_to_angles(u, v, scale):
    """A point of the recording (normalized) on the plate, shrunk about the centre by scale."""
    width, height = plate_extent()
    y = (u - 0.5) * scale * width
    z = (0.5 - v) * scale * height
    return math.degrees(math.atan2(y, PLATE_DISTANCE)), math.degrees(math.atan2(z, math.hypot(PLATE_DISTANCE, y)))


def object_rect(box, scale):
    left, top = frame_to_angles(box[0], box[1], scale)
    right, bottom = frame_to_angles(box[2], box[3], scale)
    return (left, bottom, right, top)


def rect_at(yaw, pitch, width_deg, height_deg):
    return (yaw - width_deg / 2, pitch - height_deg / 2, yaw + width_deg / 2, pitch + height_deg / 2)


def overlaps(a, b, margin=MARGIN):
    return not (a[2] + margin <= b[0] or b[2] + margin <= a[0] or a[3] + margin <= b[1] or b[3] + margin <= a[1])


def gap(a, b):
    dx = max(b[0] - a[2], a[0] - b[2], 0.0)
    dy = max(b[1] - a[3], a[1] - b[3], 0.0)
    return math.hypot(dx, dy)


def inside_view(r):
    return r[0] >= -VIEW_YAW and r[2] <= VIEW_YAW and r[1] >= -VIEW_PITCH and r[3] <= VIEW_PITCH


def best_spot(width_deg, height_deg, blockers, cost):
    best, best_cost = None, None
    for yaw in range(-int(VIEW_YAW), int(VIEW_YAW) + 1):
        for pitch in range(-int(VIEW_PITCH), int(VIEW_PITCH) + 1):
            r = rect_at(yaw, pitch, width_deg, height_deg)
            if not inside_view(r) or any(overlaps(r, b) for b in blockers):
                continue
            c = cost(yaw, pitch, r)
            if best_cost is None or c < best_cost:
                best, best_cost = (yaw, pitch, r), c
    return best


def angular(size, depth):
    return math.degrees(2.0 * math.atan(size / 2.0 / depth))


def card_zone(real, side, width):
    """The cards' zone beside the object on one side (+1 right, -1 left), or None when it leaves the view."""
    if side > 0:
        r = (real[2] + MARGIN, -ZONE_PITCH, real[2] + MARGIN + width, ZONE_PITCH)
    else:
        r = (real[0] - MARGIN - width, -ZONE_PITCH, real[0] - MARGIN, ZONE_PITCH)
    return r if inside_view(r) else None


def solve(box, mesh_width, mesh_height):
    """Content scale, hologram (centre, height cm) and card zone (yaw0, pitch0, yaw1, pitch1) for one station."""
    # A wide card zone first (the recording is shrunk before the zone is), then the largest recording
    width, _ = plate_extent()
    largest = PLATE_DISTANCE * math.tan(math.radians(MAX_CONTENT_FOV / 2.0)) / (width / 2.0)
    for zone_width in ZONE_WIDTHS:
        for scale in [round(largest * (1.0 - 0.04 * i), 3) for i in range(16)]:
            real = object_rect(box, scale)
            centre = (real[0] + real[2]) / 2
            # The freer side first
            sides = (1, -1) if centre <= 0 else (-1, 1)
            for side in sides:
                zone = card_zone(real, side, zone_width)
                if not zone:
                    continue
                # The twin's larger side (its height, or the width it sweeps on the turntable) spans size_deg; the object's other side first
                for size_deg in (24, 21, 18, 16):
                    height = 2.0 * HOLOGRAM_DEPTH * math.tan(math.radians(size_deg / 2.0)) / max(1.0, mesh_width / mesh_height)
                    hologram_deg = angular(height, HOLOGRAM_DEPTH)
                    width_deg = angular(mesh_width * height / mesh_height, HOLOGRAM_DEPTH)
                    hologram = best_spot(width_deg, hologram_deg, [real, zone],
                                         lambda y, p, r: gap(r, real) + 0.15 * abs(y) + 0.3 * abs(p + 3) + (25.0 if (y - centre) * side > 0 else 0.0))
                    if hologram:
                        return scale, real, hologram, height, zone
    return None


def local_point(yaw, pitch, depth):
    """Viewing-spot space (X ahead, Y right, Z up from the floor) of a direction at a depth along X."""
    y = depth * math.tan(math.radians(yaw))
    z = EYE + math.hypot(depth, y) * math.tan(math.radians(pitch))
    return unreal.Vector(depth, y, z)


# ====== Level ====== //

def spawn(cls, location, yaw=0.0, label=None):
    actor = actors.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, 0.0, yaw))
    if label:
        actor.set_actor_label(label)
    return actor


def heading(deg):
    return unreal.Vector(math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0.0)


def to_world(spot, yaw, local):
    f, r = heading(yaw), heading(yaw + 90.0)
    return unreal.Vector(spot.x + f.x * local.x + r.x * local.y, spot.y + f.y * local.x + r.y * local.y, local.z)


def character_info():
    info = {"walk_speed": 300.0}
    try:
        mode = unreal.get_default_object(assets.load_asset("/Game/ZooVR/Blueprints/BP_ZooVRGameMode").generated_class())
        pawn = unreal.get_default_object(mode.get_editor_property("default_pawn_class"))
        info["pawn"] = pawn.get_class().get_name()
        info["walk_speed"] = pawn.get_editor_property("character_movement").get_editor_property("max_walk_speed")
        for name in ("camera", "first_person_camera", "camera_component"):
            try:
                info["fov"] = pawn.get_editor_property(name).get_editor_property("field_of_view")
                break
            except Exception:
                pass
    except Exception as error:
        unreal.log_warning(f"{LOG} pawn info: {error}")
    return info


def post_process():
    volume = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), label="PostProcess_Passthrough")
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    values = {
        "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
        "auto_exposure_bias": 0.0,
        "auto_exposure_apply_physical_camera_exposure": False,
        "vignette_intensity": 0.35,
        "bloom_intensity": 0.25,
        "motion_blur_amount": 0.0,
    }
    for name, value in values.items():
        try:
            settings.set_editor_property("override_" + name, True)
            settings.set_editor_property(name, value)
        except Exception as error:
            unreal.log_warning(f"{LOG} post process {name}: {error}")
    volume.set_editor_property("settings", settings)


def build():
    project = unreal.Paths.project_dir()
    with open(os.path.join(project, "Tools", "Video", "plates.json"), encoding="utf-8") as f:
        layout = json.load(f)
    data = {int(p.split("_")[-2]): assets.load_asset(p.split(".")[0])
            for p in assets.list_assets(f"{ROOT}/Exhibits", recursive=True) if "/DA_Exhibit_" in p}
    data = [data[k] for k in sorted(data)]

    material = plate_material()
    plates = {p["number"]: media_source(f"FMS_Plate{p['number']:02d}_{p['short']}", p["file"]) for p in layout["plates"]}
    walks = [media_source(f"FMS_Walk{w['number']:02d}_{w['short']}", w["file"]) for w in layout["walks"]]
    boxes = {p["number"]: p["box"] for p in layout["plates"]}
    places_asset(data)

    pawn = character_info()
    leg = max(1200.0, min(3000.0, pawn["walk_speed"] * 8.0))
    log(f"pawn {pawn}, leg {leg:.0f} cm")

    if assets.does_asset_exist(LEVEL):
        assets.delete_asset(LEVEL)
    levels.new_level(LEVEL)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", assets.load_asset("/Game/ZooVR/Blueprints/BP_ZooVRGameMode").generated_class())

    # The route: start, then one leg per station, turning 45 degrees left each time
    spots, yaws = [unreal.Vector(0, 0, 0)], []
    for k in range(len(data)):
        yaw = -45.0 * k
        f = heading(yaw)
        last = spots[-1]
        spots.append(unreal.Vector(last.x + f.x * leg, last.y + f.y * leg, 0.0))
        yaws.append(yaw)

    xs = [p.x for p in spots]
    ys = [p.y for p in spots]
    centre = unreal.Vector((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, 0)
    size = unreal.Vector(max(xs) - min(xs) + 4000, max(ys) - min(ys) + 4000, 0)

    floor = spawn(unreal.StaticMeshActor, unreal.Vector(centre.x, centre.y, -5.0), label="Floor")
    floor.static_mesh_component.set_static_mesh(assets.load_asset("/Engine/BasicShapes/Cube"))
    floor.set_actor_scale3d(unreal.Vector(size.x / 100.0, size.y / 100.0, 0.1))
    floor.set_actor_hidden_in_game(True)
    nav = spawn(unreal.NavMeshBoundsVolume, unreal.Vector(centre.x, centre.y, 100.0), label="NavBounds")
    origin, extent = nav.get_actor_bounds(False)
    if extent.x > 1:
        nav.set_actor_scale3d(unreal.Vector(size.x / 2 / extent.x, size.y / 2 / extent.y, 300.0 / extent.z))
    log(f"nav bounds before scale extent={extent}, after={nav.get_actor_bounds(False)[1]}")

    key = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), label="Light_Key")
    key.set_actor_rotation(unreal.Rotator(0.0, -50.0, 30.0), False)
    key.light_component.set_intensity(4.0)
    fill = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 500), label="Light_Fill")
    fill.set_actor_rotation(unreal.Rotator(0.0, 20.0, -150.0), False)
    fill.light_component.set_intensity(1.2)
    fill.light_component.set_cast_shadows(False)
    post_process()

    spawn(unreal.PlayerStart, unreal.Vector(-250, 0, 100), yaw=yaws[0], label="PlayerStart")
    passthrough = spawn(unreal.MuseumPassthrough, unreal.Vector(0, 0, 0), label="Passthrough")
    passthrough.set_editor_property("plate_material", material)
    passthrough.set_editor_property("walk_clips", walks)

    width, height = plate_extent()
    for k, exhibit_data in enumerate(data):
        number = exhibit_data.get_editor_property("number")
        spot, yaw = spots[k + 1], yaws[k]
        mesh = exhibit_data.get_editor_property("mesh")
        if not isinstance(mesh, unreal.StaticMesh):
            mesh = assets.load_asset(str(mesh).split("'")[-2] if "'" in str(mesh) else str(mesh))
        bounds = mesh.get_bounds()
        mesh_width = 2.0 * math.hypot(bounds.box_extent.x, bounds.box_extent.y)
        mesh_height = 2.0 * bounds.box_extent.z
        solved = solve(boxes[number], mesh_width, mesh_height)
        if not solved:
            unreal.log_error(f"{LOG} {number:02d}: no placement for hologram and cards (mesh {mesh_width:.0f}x{mesh_height:.0f})")
            continue
        scale, real, hologram, hologram_height, card_area = solved

        destination = spawn(unreal.WzPathGuideDestination, spot, yaw, f"Station{number:02d}")
        destination.set_editor_property("place_tag", exhibit_data.get_editor_property("place_tag"))
        zone = spawn(unreal.TriggerBox, unreal.Vector(spot.x, spot.y, ZONE_EXTENT.z), yaw, f"Zone{number:02d}")
        zone.get_component_by_class(unreal.BoxComponent).set_box_extent(ZONE_EXTENT, True)

        # The exhibit sits at the real-object depth, X away from the viewer: actor space = spot space shifted by REAL_DEPTH
        anchor = to_world(spot, yaw, unreal.Vector(REAL_DEPTH, 0, 0))
        exhibit = spawn(unreal.MuseumExhibit, unreal.Vector(anchor.x, anchor.y, 0.0), yaw, f"Exhibit{number:02d}")

        def relative(local):
            return unreal.Vector(local.x - REAL_DEPTH, local.y, local.z)

        exhibit.set_editor_property("data", exhibit_data)
        exhibit.set_editor_property("viewing_zone", zone)
        # Each exhibit's card pointer is its own slate pointer (shared ones cancel each other's clicks)
        exhibit.set_editor_property("pointer_index", 10 + number)
        exhibit.set_editor_property("plate_video", plates[number])
        exhibit.set_editor_property("plate_offset", unreal.Vector(PLATE_DISTANCE - REAL_DEPTH, 0.0, EYE))
        exhibit.set_editor_property("plate_size", unreal.Vector2D(width, height))
        exhibit.set_editor_property("plate_content_scale", scale)

        # Real object: the box whose outline, seen from the spot, is the object's outline in the recording
        a, b = local_point(real[0], real[1], REAL_DEPTH), local_point(real[2], real[3], REAL_DEPTH)
        real_object = exhibit.get_editor_property("real_object")
        real_object.set_editor_property("relative_location", relative(unreal.Vector(REAL_DEPTH, (a.y + b.y) / 2, (a.z + b.z) / 2)))
        real_object.set_box_extent(unreal.Vector(5.0, abs(b.y - a.y) / 2, abs(b.z - a.z) / 2), False)

        # Hologram twin: the mesh's bounds centre (not its pivot) on the chosen centre; the turntable spins about the root
        centre_point = local_point(hologram[0], hologram[1], HOLOGRAM_DEPTH)
        hologram_scale = hologram_height / mesh_height
        exhibit.get_editor_property("hologram_root").set_editor_property(
            "relative_location", relative(unreal.Vector(centre_point.x, centre_point.y, centre_point.z - bounds.origin.z * hologram_scale)))
        exhibit_data.set_editor_property("real_height", 0.0)
        exhibit_data.set_editor_property("hologram_scale", hologram_scale)
        save(exhibit_data)

        # The cards: laid out at runtime inside the zone, seen from the eye at the viewing spot (exhibit space)
        exhibit.set_editor_property("view_offset", unreal.Vector(-REAL_DEPTH, 0.0, EYE))
        exhibit.set_editor_property("panel_zone", unreal.Vector4(card_area[0], card_area[1], card_area[2], card_area[3]))

        # Where the head looks during the visit: the middle of the three, inside the gaze limits
        items = [real, hologram[2], card_area]
        gaze_yaw = max(-GAZE_YAW, min(GAZE_YAW, sum((r[0] + r[2]) / 2 for r in items) / 3))
        gaze_pitch = max(-GAZE_PITCH, min(GAZE_PITCH, sum((r[1] + r[3]) / 2 for r in items) / 3))
        spawn(unreal.TargetPoint, to_world(spot, yaw, local_point(gaze_yaw, gaze_pitch, 300.0)), yaw, f"Gaze{number:02d}")

        log(f"{number:02d} mesh={mesh_width:.0f}x{mesh_height:.0f} spot=({spot.x:.0f},{spot.y:.0f}) yaw={yaw:.0f} content={scale} real={tuple(round(x, 1) for x in real)} "
            f"hologram=({hologram[0]},{hologram[1]}) h={hologram_height:.0f}cm scale={hologram_height / mesh_height:.3f} "
            f"zone=({card_area[0]:.1f},{card_area[1]:.1f},{card_area[2]:.1f},{card_area[3]:.1f}) gaze=({gaze_yaw:.1f},{gaze_pitch:.1f})")

    log(f"saved={levels.save_current_level()}")


build()
