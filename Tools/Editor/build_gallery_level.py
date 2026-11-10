# Builds the 3D gallery level /Game/Museum/Maps/L_MuseumGallery (rebuilt from scratch on every run): the six objects as real
# 3D objects (the Tripo meshes with their own textures, at real size) in a lit exhibition hall, with the same MR layer as the
# demo level (detection, hologram twin, information cards, HUD bar, menu, map, floor chevrons). No passthrough recordings:
# the "real world" is the hall itself.
#   - Materials in /Game/Museum/Gallery: M_Gallery_Surface (procedural concrete / plaster / paint / wood, no textures),
#     M_Gallery_Glass (vitrines), M_Gallery_Emissive (light panels), M_MR_Scan (the detection scan drawn on the real object,
#     AMuseumExhibit::ScanMaterial).
#   - Hall: polished concrete floor, charcoal walls with fins between the bays, dark ceiling with light tracks, benches,
#     the gallery title on the far wall. Objects 01-03 on the left wall, 04-06 on the right, tour order loops down and back.
#   - Per object: plinth (vitrine for 05 and 06), the object, a wall plaque, a framed documentation photo (Gambar_Pilihan_*.jpg,
#     imported to /Game/Museum/Gallery/Prints; the originals are only read), two track spots; StationNN / ZoneNN / ExhibitNN /
#     GazeNN like the demo level, so Tools/Demo/build_demo_script.py's tour runs here too.
#   - Cards: the exhibit data get supporting photos on cards 2 and 3 (the owner's Camera/*.jpg, imported to
#     /Game/Museum/Gallery/Photos) and the video card (FMS_ExhibitNN, Content/Movies/Exhibits). Each card gets a place
#     (AMuseumExhibit::PanelSlots): card 2 above the object; the maket's video under the hologram (the statues have a rope
#     there), the low objects' card 3 below the object; the rest stand in the column beside the object. "Above" clears the hologram too.
# Placement of the hologram twin and the card zone: angles seen from the viewing spot, as in build_demo_level.py, but the
# object's outline comes from its 3D bounds. Editor must be CLOSED.
import glob
import math
import os

import unreal

LOG = "[MuseumGallery]"
LEVEL = "/Game/Museum/Maps/L_MuseumGallery"
ROOT = "/Game/Museum"
GALLERY = f"{ROOT}/Gallery"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"

EYE = 165.0
VIEW_DISTANCE = 340.0  # viewing spot to the object's centre (floor plan)
HOLOGRAM_DEPTH = 220.0
VIEW_YAW = 52.0
VIEW_PITCH = 25.0
GAZE_YAW = 20.0
GAZE_PITCH = 8.0
MARGIN = 2.5
ZONE_WIDTHS = (38.0, 36.0, 34.0, 32.0, 30.0)
ZONE_PITCH = 17.0
SLOT_TOP = 23.0      # highest pitch a card above the object reaches (just under the view's top at the gaze)
SLOT_BOTTOM = -34.0  # lowest pitch a card below reaches (a reading desk in front of the plinth)
SLOT_HALF = 22.0     # most half width of a card above / below the object
SLOT_GAP = 2.0
ARRIVAL_RADIUS = 150.0
ZONE_EXTENT = unreal.Vector(60.0, 60.0, 150.0)

# The hall (cm). The world origin stays outside it (the NavMesh had a hole there in the demo level).
X0, X1 = 500.0, 4800.0
HALF_Y = 950.0
HEIGHT = 600.0
BAY_X = (1800.0, 2800.0, 3800.0)  # object centres along the hall
OBJECT_Y = 640.0                  # object centre distance from the hall's axis
FIN_X = (1300.0, 2300.0, 3300.0, 4300.0)

# Per object: real height (cm), plinth height, vitrine, rope barrier, turn of the mesh so its face looks at the visitor
OBJECTS = {
    1: {"height": 150.0, "plinth": 35.0, "vitrine": False, "rope": True, "turn": 0.0},
    2: {"height": 140.0, "plinth": 35.0, "vitrine": False, "rope": True, "turn": 0.0},
    3: {"height": 130.0, "plinth": 35.0, "vitrine": False, "rope": True, "turn": 0.0},
    4: {"height": 75.0, "plinth": 85.0, "vitrine": False, "rope": False, "turn": 0.0},
    5: {"height": 88.0, "plinth": 80.0, "vitrine": True, "rope": False, "turn": 0.0},
    6: {"width": 150.0, "plinth": 70.0, "vitrine": True, "rope": False, "turn": 0.0},
}


def log(message):
    unreal.log(f"{LOG} {message}")


def save(*objects):
    for obj in objects:
        assets.save_loaded_asset(obj, only_if_is_dirty=False)


# ====== Materials ====== //

NOISE = r"""
float3 q = P / max(NoiseScale, 1.0);
float n = 0.0;
float amp = 0.5;
for (int o = 0; o < 4; o++)
{
    float3 i = floor(q);
    float3 f = frac(q);
    f = f * f * (3.0 - 2.0 * f);
    float h000 = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h100 = frac(sin(dot(i + float3(1, 0, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h010 = frac(sin(dot(i + float3(0, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h110 = frac(sin(dot(i + float3(1, 1, 0), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h001 = frac(sin(dot(i + float3(0, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h101 = frac(sin(dot(i + float3(1, 0, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h011 = frac(sin(dot(i + float3(0, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float h111 = frac(sin(dot(i + float3(1, 1, 1), float3(127.1, 311.7, 74.7))) * 43758.5453);
    float v = lerp(lerp(lerp(h000, h100, f.x), lerp(h010, h110, f.x), f.y), lerp(lerp(h001, h101, f.x), lerp(h011, h111, f.x), f.y), f.z);
    n += amp * v;
    q *= 2.07;
    amp *= 0.5;
}
// Tile joints in the floor plane (Tile 0 = none) and wood-like streaks along X (Streaks 0 = none)
float joint = 0.0;
if (Tile > 0.0)
{
    float2 t = abs(frac(P.xy / Tile) - 0.5);
    joint = 1.0 - smoothstep(0.497, 0.4995, max(t.x, t.y));
}
float streak = Streaks > 0.0 ? 0.5 + 0.5 * sin(P.y * Streaks + n * 6.0) : 0.5;
"""

SURFACE_COLOR = NOISE + r"""
float3 c = lerp(Base, Base2, saturate(n * 1.4 - 0.2));
c *= lerp(1.0, 0.75 + 0.5 * streak, saturate(Streaks));
c = lerp(c, c * 0.45, joint);
return c;
"""

SURFACE_ROUGHNESS = NOISE + r"""
return saturate(Roughness + RoughnessVar * (n - 0.5) + 0.35 * joint);
"""

SURFACE_SCALARS = {"NoiseScale": 60.0, "Tile": 0.0, "Streaks": 0.0, "Roughness": 0.8, "RoughnessVar": 0.1, "Metallic": 0.0}
SURFACE_VECTORS = {"Base": (0.1, 0.1, 0.1), "Base2": (0.14, 0.14, 0.14)}

SCAN_HLSL = r"""
// The MR detection on the real object: a light band climbing from its base to its top, painting a fine triangle grid
// on the surface (kept faintly where it has passed) and lighting the silhouette inside the band
float h = saturate((P.z - ScanBottom) / max(ScanTop - ScanBottom, 1.0));
float climb = frac(DetectTime * 0.42) * 1.25 - 0.1;
float band = exp(-pow((h - climb) * 14.0, 2.0));
float3 an = abs(N);
float2 uv = an.x > an.y ? P.yz : P.xz;
uv = an.z > max(an.x, an.y) ? P.xy : uv;
float2 g = uv / 5.0;
float f1 = abs(frac(g.y) - 0.5);
float f2 = abs(frac(g.x * 0.866 + g.y * 0.5) - 0.5);
float f3 = abs(frac(-g.x * 0.866 + g.y * 0.5) - 0.5);
float grid = smoothstep(0.42, 0.49, max(f1, max(f2, f3)));
float rim = pow(1.0 - saturate(abs(dot(N, V))), 2.5);
float3 cyan = float3(0.06, 0.62, 1.0);
float3 c = cyan * (band * (0.45 + 2.2 * rim) + grid * (0.9 * band + 0.08 * step(h, climb)));
return c * saturate(DetectAmount);
"""


def new_material(name, folder=GALLERY):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def custom_node(material, code, inputs, output_type, x, y, description):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("description", description)
    node.set_editor_property("output_type", output_type)
    entries = []
    for n in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", n)
        entries.append(entry)
    node.set_editor_property("inputs", entries)
    return node


def surface_material():
    material = new_material("M_Gallery_Surface")
    position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1200, 0)
    inputs = ["P"] + list(SURFACE_VECTORS) + list(SURFACE_SCALARS)
    color = custom_node(material, SURFACE_COLOR, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -500, -100, "SurfaceColor")
    rough = custom_node(material, SURFACE_ROUGHNESS, inputs, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -500, 200, "SurfaceRoughness")
    for node in (color, rough):
        mel.connect_material_expressions(position, "", node, "P")
    for i, (name, value) in enumerate(SURFACE_VECTORS.items()):
        param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, -300 + i * 120)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", unreal.LinearColor(value[0], value[1], value[2], 1.0))
        for node in (color, rough):
            mel.connect_material_expressions(param, "", node, name)
    metallic = None
    for i, (name, value) in enumerate(SURFACE_SCALARS.items()):
        param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 0 + i * 70)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        if name == "Metallic":
            metallic = param
        for node in (color, rough):
            mel.connect_material_expressions(param, "", node, name)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    save(material)
    return material


def glass_material():
    material = new_material("M_Gallery_Glass")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    material.set_editor_property("two_sided", True)
    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, -100)
    # Dark: the glass shows only its reflections, not a milky lit surface
    color.set_editor_property("constant", unreal.LinearColor(0.02, 0.025, 0.025, 1.0))
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for i, (prop, value) in enumerate(((unreal.MaterialProperty.MP_ROUGHNESS, 0.03), (unreal.MaterialProperty.MP_SPECULAR, 1.0),
                                       (unreal.MaterialProperty.MP_OPACITY, 0.12), (unreal.MaterialProperty.MP_METALLIC, 0.0))):
        node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 50 + i * 80)
        node.set_editor_property("r", value)
        mel.connect_material_property(node, "", prop)
    mel.recompile_material(material)
    save(material)
    return material


def emissive_material():
    material = new_material("M_Gallery_Emissive")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.9, 0.75, 1.0))
    strength = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    strength.set_editor_property("parameter_name", "Intensity")
    strength.set_editor_property("default_value", 4.0)
    multiply = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 50)
    mel.connect_material_expressions(color, "", multiply, "A")
    mel.connect_material_expressions(strength, "", multiply, "B")
    mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    save(material)
    return material


def scan_material():
    material = new_material("M_MR_Scan", f"{ROOT}/MR")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("two_sided", False)
    scalars = {"DetectAmount": 0.0, "DetectTime": 0.0, "ScanBottom": 0.0, "ScanTop": 100.0}
    node = custom_node(material, SCAN_HLSL, ["P", "N", "V"] + list(scalars), unreal.CustomMaterialOutputType.CMOT_FLOAT3, -400, 0, "DetectionScan")
    mel.connect_material_expressions(mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, -200), "", node, "P")
    mel.connect_material_expressions(mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -900, -100), "", node, "N")
    mel.connect_material_expressions(mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -900, 0), "", node, "V")
    for i, (name, value) in enumerate(scalars.items()):
        param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 100 + i * 70)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        mel.connect_material_expressions(param, "", node, name)
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    save(material)
    return material


PRINT_HLSL = r"""
// The photo spans the print's front face: UV from the world position (no dependence on the cube's own UVs)
float2 uv = float2(dot(P - Origin, Right) / Width + 0.5, (Origin.z + Height * 0.5 - P.z) / Height);
return Texture2DSample(Tex, TexSampler, saturate(uv)).rgb;
"""


def print_material():
    material = new_material("M_Gallery_Print")
    tex = mel.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -900, -200)
    tex.set_editor_property("parameter_name", "Photo")
    tex.set_editor_property("texture", assets.load_asset("/Engine/EngineResources/DefaultTexture"))
    node = custom_node(material, PRINT_HLSL, ["Tex", "P", "Origin", "Right", "Width", "Height"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -400, 0, "Print")
    mel.connect_material_expressions(tex, "", node, "Tex")
    mel.connect_material_expressions(mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, 0), "", node, "P")
    for i, name in enumerate(("Origin", "Right")):
        param = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -900, 100 + i * 120)
        param.set_editor_property("parameter_name", name)
        mel.connect_material_expressions(param, "", node, name)
    for i, (name, value) in enumerate((("Width", 200.0), ("Height", 120.0))):
        param = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 350 + i * 70)
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        mel.connect_material_expressions(param, "", node, name)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", 0.55)
    mel.connect_material_property(node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    save(material)
    return material


def import_photo(number):
    """The object's chosen photo as a texture (imported once; the source file is only read)."""
    path = f"{GALLERY}/Prints/T_Print_{number:02d}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    found = glob.glob(os.path.join(project, "..", "Hasil_Model_3D", f"{number:02d}_*", "Gambar_Pilihan_*.jpg"))
    if not found:
        unreal.log_warning(f"{LOG} no photo for {number:02d}")
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.normpath(found[0]))
    task.set_editor_property("destination_path", f"{GALLERY}/Prints")
    task.set_editor_property("destination_name", f"T_Print_{number:02d}")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = assets.load_asset(path)
    if texture:
        texture.set_editor_property("max_texture_size", 2048)
        save(texture)
    return texture


# Owner photos per object (Camera/<prefix>_NN_*.jpg): card 2 and card 3
CAMERA_PHOTOS = {
    1: "Arca_Dwarapala_Taman", 2: "Arca_Nandiswara", 3: "Prasasti_Gajah_Mada_1351M",
    4: "Relief_Pancuran_Arjunawiwaha", 5: "Wayang_Golek_Wali_Songo", 6: "Maket_Masjid_At_Thohir",
}


def import_camera_photo(number, index, filename):
    """One of the owner's photos as a UI texture (imported once; the source file is only read)."""
    name = f"T_Photo_{number:02d}_{index}"
    path = f"{GALLERY}/Photos/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", os.path.normpath(filename))
    task.set_editor_property("destination_path", f"{GALLERY}/Photos")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = assets.load_asset(path)
    if texture:
        texture.set_editor_property("max_texture_size", 2048)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("never_stream", True)
        save(texture)
    return texture


def dress_cards(data):
    """Supporting photos on cards 2 and 3 (card 1 stays all text) and the video card."""
    number = data.get_editor_property("number")
    project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    found = sorted(glob.glob(os.path.join(project, "..", "Camera", f"{CAMERA_PHOTOS[number]}_0*.jpg")))
    picks = [found[0], found[-1]] if len(found) >= 2 else found
    panels = list(data.get_editor_property("panels"))
    for slot, filename in zip((1, 2), picks):
        if slot < len(panels):
            panels[slot].set_editor_property("image", import_camera_photo(number, slot + 1, filename))
    data.set_editor_property("panels", panels)
    video = [p for p in assets.list_assets(f"{ROOT}/Media", recursive=False) if f"/FMS_Exhibit{number:02d}_" in p]
    if video:
        data.set_editor_property("media_video", assets.load_asset(video[0].split(".")[0]))
    save(data)
    log(f"{number:02d} card photos={[os.path.basename(f) for f in picks]} video={video[0] if video else None}")


def card_slots(number, outline, hologram, zone):
    """Places of the four cards (1-3, video) seen from the eye; None = the column beside the object."""
    centre = (outline[0] + outline[2]) / 2

    def span(top, bottom):
        """Yaw range around the object between the pitches, clear of the hologram and the side zone."""
        y0, y1 = centre - SLOT_HALF, centre + SLOT_HALF
        if hologram[3] > bottom and hologram[1] < top:
            y0 = max(y0, hologram[2] + SLOT_GAP) if hologram[0] < centre else y0
            y1 = min(y1, hologram[0] - SLOT_GAP) if hologram[0] > centre else y1
        if zone[3] > bottom and zone[1] < top:
            y1 = min(y1, zone[0] - SLOT_GAP) if zone[0] > centre else y1
            y0 = max(y0, zone[2] + SLOT_GAP) if zone[0] < centre else y0
        return y0, y1

    def slot(area, anchor_y):
        y0, y1 = area[0], area[2]
        anchor_x = min(1.0, max(0.0, (centre - y0) / (y1 - y0)))
        return unreal.MuseumPanelSlot(area=unreal.Vector4(*area), anchor=unreal.Vector2D(anchor_x, anchor_y))

    # Above the object and the hologram both (a wide band: the photo cards are landscape)
    bottom = max(outline[3], hologram[3]) + SLOT_GAP
    y0, y1 = span(SLOT_TOP, bottom)
    above = slot((y0, bottom, y1, SLOT_TOP), 0.0)
    tall = outline[1] < -18.0
    if tall and OBJECTS[number]["rope"]:
        # The rope runs under the hologram: the video joins the column
        return [None, above, None, None]
    if tall:
        # The video at a reading desk under the hologram (the floor is under the object), clear of the hologram's tag
        x0, x1 = hologram[0], min(hologram[2], outline[0] - SLOT_GAP)
        if x1 - x0 < 2 * SLOT_HALF:
            x0 = x1 - 2 * SLOT_HALF
        top = hologram[1] - 2.5 * SLOT_GAP
        anchor_x = min(1.0, max(0.0, ((hologram[0] + hologram[2]) / 2 - x0) / (x1 - x0)))
        below = unreal.MuseumPanelSlot(area=unreal.Vector4(x0, SLOT_BOTTOM, x1, top), anchor=unreal.Vector2D(anchor_x, 1.0))
        return [None, above, None, below]
    # Below the object, under the hologram's tag too (it hangs about 3 degrees under the hologram)
    top = min(outline[1], hologram[1] - 3.0) - SLOT_GAP
    y0, y1 = span(top, SLOT_BOTTOM)
    return [None, above, slot((y0, SLOT_BOTTOM, y1, top), 1.0), None]


def instance(parent, name, vectors=None, scalars=None):
    path = f"{GALLERY}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    mi = tools.create_asset(name, GALLERY, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for key, value in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for key, value in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
    save(mi)
    return mi


def gallery_materials():
    surface = surface_material()
    emissive = emissive_material()
    m = {
        "floor": instance(surface, "MI_Gallery_Floor", {"Base": (0.16, 0.155, 0.15), "Base2": (0.23, 0.22, 0.205)},
                          {"NoiseScale": 45.0, "Tile": 120.0, "Roughness": 0.2, "RoughnessVar": 0.25}),
        "wall": instance(surface, "MI_Gallery_Wall", {"Base": (0.052, 0.056, 0.06), "Base2": (0.066, 0.07, 0.075)},
                         {"NoiseScale": 80.0, "Roughness": 0.88, "RoughnessVar": 0.08}),
        "accent": instance(surface, "MI_Gallery_WallAccent", {"Base": (0.16, 0.07, 0.035), "Base2": (0.19, 0.085, 0.045)},
                           {"NoiseScale": 80.0, "Roughness": 0.85, "RoughnessVar": 0.06}),
        "ceiling": instance(surface, "MI_Gallery_Ceiling", {"Base": (0.02, 0.02, 0.022), "Base2": (0.03, 0.03, 0.032)},
                            {"NoiseScale": 100.0, "Roughness": 0.95}),
        "plinth": instance(surface, "MI_Gallery_Plinth", {"Base": (0.62, 0.6, 0.56), "Base2": (0.68, 0.66, 0.62)},
                           {"NoiseScale": 30.0, "Roughness": 0.72, "RoughnessVar": 0.12}),
        "wood": instance(surface, "MI_Gallery_Wood", {"Base": (0.2, 0.1, 0.045), "Base2": (0.3, 0.16, 0.075)},
                         {"NoiseScale": 25.0, "Streaks": 1.6, "Roughness": 0.45, "RoughnessVar": 0.2}),
        "metal": instance(surface, "MI_Gallery_Brass", {"Base": (0.55, 0.42, 0.22), "Base2": (0.6, 0.47, 0.26)},
                          {"NoiseScale": 15.0, "Roughness": 0.3, "Metallic": 1.0}),
        "black": instance(surface, "MI_Gallery_Black", {"Base": (0.015, 0.015, 0.015), "Base2": (0.02, 0.02, 0.02)},
                          {"Roughness": 0.5}),
        "panel": instance(surface, "MI_Gallery_Panel", {"Base": (0.018, 0.02, 0.022), "Base2": (0.02, 0.022, 0.024)},
                          {"Roughness": 0.6}),
        "rope": instance(surface, "MI_Gallery_Rope", {"Base": (0.25, 0.02, 0.02), "Base2": (0.3, 0.03, 0.03)},
                         {"NoiseScale": 5.0, "Roughness": 0.9}),
        "glass": glass_material(),
        "print": print_material(),
        "light": instance(emissive, "MI_Gallery_LightPanel", {"Color": (1.0, 0.93, 0.82)}, {"Intensity": 6.0}),
        "lens": instance(emissive, "MI_Gallery_Lens", {"Color": (1.0, 0.85, 0.65)}, {"Intensity": 30.0}),
    }
    log(f"materials {sorted(m)}")
    return m


# ====== Level pieces ====== //

def spawn(cls, location, yaw=0.0, label=None, pitch=0.0):
    actor = actors.spawn_actor_from_class(cls, location, unreal.Rotator(0.0, pitch, yaw))
    if label:
        actor.set_actor_label(label)
    return actor


def box(label, centre, size, material, yaw=0.0, collide=True, shadow=True, folder="Hall"):
    """A cube scaled to size (cm) centred at centre."""
    actor = spawn(unreal.StaticMeshActor, centre, yaw, label)
    component = actor.static_mesh_component
    component.set_static_mesh(assets.load_asset(CUBE))
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    component.set_material(0, material)
    component.set_cast_shadow(shadow)
    if not collide:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_folder_path(folder)
    return actor


def cylinder(label, centre, diameter, height, material, collide=True, folder="Hall"):
    actor = spawn(unreal.StaticMeshActor, centre, 0.0, label)
    component = actor.static_mesh_component
    component.set_static_mesh(assets.load_asset(CYLINDER))
    actor.set_actor_scale3d(unreal.Vector(diameter / 100.0, diameter / 100.0, height / 100.0))
    component.set_material(0, material)
    if not collide:
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_folder_path(folder)
    return actor


def text(label, location, yaw, value, size, color=(0.92, 0.9, 0.86), align="left", folder="Hall"):
    actor = spawn(unreal.TextRenderActor, location, yaw, label)
    component = actor.get_editor_property("text_render")
    component.set_editor_property("text", value)
    component.set_editor_property("world_size", size)
    component.set_editor_property("text_render_color", unreal.Color(int(color[0] * 255), int(color[1] * 255), int(color[2] * 255), 255))
    component.set_editor_property("horizontal_alignment",
                                  {"left": unreal.HorizTextAligment.EHTA_LEFT, "center": unreal.HorizTextAligment.EHTA_CENTER}[align])
    component.set_editor_property("vertical_alignment", unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_folder_path(folder)
    return actor


def spot(label, location, target, intensity, cone, temperature=3200.0, shadows=True, folder="Lights"):
    direction = unreal.Vector(target.x - location.x, target.y - location.y, target.z - location.z)
    rotation = unreal.MathLibrary.make_rot_from_x(direction)
    actor = actors.spawn_actor_from_class(unreal.SpotLight, location, rotation)
    actor.set_actor_label(label)
    light = actor.spot_light_component
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_intensity(intensity)
    light.set_outer_cone_angle(cone)
    light.set_inner_cone_angle(cone * 0.55)
    light.set_editor_property("use_temperature", True)
    light.set_editor_property("temperature", temperature)
    light.set_attenuation_radius(2200.0)
    light.set_editor_property("source_radius", 3.0)
    light.set_cast_shadows(shadows)
    actor.set_folder_path(folder)
    return actor


def rect_light(label, location, size, intensity, temperature=4300.0, folder="Lights"):
    actor = actors.spawn_actor_from_class(unreal.RectLight, location, unreal.Rotator(0.0, -90.0, 0.0))
    actor.set_actor_label(label)
    light = actor.get_component_by_class(unreal.RectLightComponent)
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_intensity(intensity)
    light.set_editor_property("source_width", size[0])
    light.set_editor_property("source_height", size[1])
    light.set_editor_property("use_temperature", True)
    light.set_editor_property("temperature", temperature)
    light.set_attenuation_radius(2500.0)
    light.set_cast_shadows(False)
    actor.set_folder_path(folder)
    return actor


def fixture(label, location, target, materials):
    """A track spot's body: a black can on the rail, its warm lens facing the target."""
    direction = unreal.Vector(target.x - location.x, target.y - location.y, target.z - location.z)
    rotation = unreal.MathLibrary.make_rot_from_z(direction)
    can = spawn(unreal.StaticMeshActor, location, 0.0, label)
    can.set_actor_rotation(rotation, False)
    can.static_mesh_component.set_static_mesh(assets.load_asset(CYLINDER))
    can.set_actor_scale3d(unreal.Vector(0.11, 0.11, 0.2))
    can.static_mesh_component.set_material(0, materials["black"])
    can.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    can.static_mesh_component.set_cast_shadow(False)
    can.set_folder_path("Lights")
    n = direction.normal()
    lens = spawn(unreal.StaticMeshActor, unreal.Vector(location.x + n.x * 10.5, location.y + n.y * 10.5, location.z + n.z * 10.5), 0.0, label + "_Lens")
    lens.set_actor_rotation(rotation, False)
    lens.static_mesh_component.set_static_mesh(assets.load_asset(CYLINDER))
    lens.set_actor_scale3d(unreal.Vector(0.08, 0.08, 0.005))
    lens.static_mesh_component.set_material(0, materials["lens"])
    lens.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    lens.static_mesh_component.set_cast_shadow(False)
    lens.set_folder_path("Lights")


def post_process():
    volume = spawn(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), label="PostProcess_Gallery")
    volume.set_editor_property("unbound", True)
    settings = volume.get_editor_property("settings")
    values = {
        "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
        "auto_exposure_bias": 0.0,
        "auto_exposure_apply_physical_camera_exposure": False,
        "vignette_intensity": 0.3,
        "bloom_intensity": 0.35,
        "motion_blur_amount": 0.0,
        "lumen_final_gather_quality": 2.0,
        "lumen_reflection_quality": 2.0,
        "lumen_scene_lighting_quality": 2.0,
        "lumen_max_trace_distance": 6000.0,
        "white_temp": 6100.0,
        "scene_fringe_intensity": 0.15,
    }
    for name, value in values.items():
        try:
            settings.set_editor_property("override_" + name, True)
            settings.set_editor_property(name, value)
        except Exception as error:
            unreal.log_warning(f"{LOG} post process {name}: {error}")
    volume.set_editor_property("settings", settings)


def hall(materials):
    length, width = X1 - X0, 2 * HALF_Y
    cx = (X0 + X1) / 2
    box("Floor", unreal.Vector(cx, 0, -10), (length + 100, width + 100, 20), materials["floor"])
    box("Ceiling", unreal.Vector(cx, 0, HEIGHT + 10), (length + 100, width + 100, 20), materials["ceiling"])
    box("Wall_North", unreal.Vector(cx, HALF_Y + 15, HEIGHT / 2), (length + 100, 30, HEIGHT), materials["wall"])
    box("Wall_South", unreal.Vector(cx, -HALF_Y - 15, HEIGHT / 2), (length + 100, 30, HEIGHT), materials["wall"])
    box("Wall_West", unreal.Vector(X0 - 15, 0, HEIGHT / 2), (30, width, HEIGHT), materials["wall"])
    box("Wall_East", unreal.Vector(X1 + 15, 0, HEIGHT / 2), (30, width, HEIGHT), materials["accent"])
    # A darker skirting and a cornice line along the long walls
    for side in (1, -1):
        name = "North" if side > 0 else "South"
        box(f"Skirting_{name}", unreal.Vector(cx, side * (HALF_Y - 1), 6), (length, 4, 12), materials["black"], collide=False)
        box(f"Cornice_{name}", unreal.Vector(cx, side * (HALF_Y - 6), 455), (length, 12, 6), materials["black"], collide=False)
        for k, fx in enumerate(FIN_X):
            box(f"Fin_{name}_{k}", unreal.Vector(fx, side * (HALF_Y - 110), 210), (24, 220, 420), materials["wall"])
        # Light tracks on the ceiling over each row of objects
        box(f"Track_{name}", unreal.Vector(cx, side * 330, HEIGHT - 4), (length - 200, 6, 5), materials["black"], collide=False, shadow=False)
    # Diffuse ceiling panels over the aisle
    for k in range(5):
        x = X0 + 700 + k * (length - 1400) / 4
        box(f"LightPanel_{k}", unreal.Vector(x, 0, HEIGHT - 1), (360, 70, 2), materials["light"], collide=False, shadow=False)
        rect_light(f"Ambient_{k}", unreal.Vector(x, 0, HEIGHT - 4), (360, 70), 40.0)
    # Benches on the aisle, between the bays
    for k, bx in enumerate((2300.0, 3300.0)):
        box(f"Bench_{k}_Seat", unreal.Vector(bx, 0, 44), (220, 55, 8), materials["wood"])
        for leg in (-1, 1):
            box(f"Bench_{k}_Leg{leg}", unreal.Vector(bx + leg * 90, 0, 20), (8, 50, 40), materials["black"])

    # The gallery title on the far wall, lit by two washers
    wall_x = X1 - 2
    text("Title", unreal.Vector(wall_x, 0, 330), 180.0, "JEJAK PERADABAN NUSANTARA", 62.0, (0.95, 0.9, 0.8), "center")
    text("Subtitle", unreal.Vector(wall_x, 0, 268), 180.0, "Museum Nasional Indonesia  ·  Ruang Arkeologi & Seni", 26.0, (0.85, 0.8, 0.7), "center")
    text("Entrance", unreal.Vector(X0 + 2, 300, 300), 0.0, "Pintu Masuk  ·  Entrance", 22.0, (0.8, 0.78, 0.74), "center")
    for side in (1, -1):
        spot(f"Washer_Title_{side}", unreal.Vector(X1 - 380, side * 380, HEIGHT - 20), unreal.Vector(X1, side * 150, 290), 90.0, 50.0, 3600.0, False)


# ====== Placement (angles from the eye at the viewing spot, degrees; yaw + right, pitch + up) ====== //

def rect_at(yaw, pitch, w, h):
    return (yaw - w / 2, pitch - h / 2, yaw + w / 2, pitch + h / 2)


def overlaps(a, b, margin=MARGIN):
    return not (a[2] + margin <= b[0] or b[2] + margin <= a[0] or a[3] + margin <= b[1] or b[3] + margin <= a[1])


def gap(a, b):
    dx = max(b[0] - a[2], a[0] - b[2], 0.0)
    dy = max(b[1] - a[3], a[1] - b[3], 0.0)
    return math.hypot(dx, dy)


def inside_view(r):
    return r[0] >= -VIEW_YAW and r[2] <= VIEW_YAW and r[1] >= -VIEW_PITCH and r[3] <= VIEW_PITCH


def best_spot(w, h, blockers, cost):
    best, best_cost = None, None
    for yaw in range(-int(VIEW_YAW), int(VIEW_YAW) + 1):
        for pitch in range(-int(VIEW_PITCH), int(VIEW_PITCH) + 1):
            r = rect_at(yaw, pitch, w, h)
            if not inside_view(r) or any(overlaps(r, b) for b in blockers):
                continue
            c = cost(yaw, pitch, r)
            if best_cost is None or c < best_cost:
                best, best_cost = (yaw, pitch, r), c
    return best


def angular(size, depth):
    return math.degrees(2.0 * math.atan(size / 2.0 / depth))


def card_zone(real, side, width):
    if side > 0:
        r = (real[2] + MARGIN, -ZONE_PITCH, real[2] + MARGIN + width, ZONE_PITCH)
    else:
        r = (real[0] - MARGIN - width, -ZONE_PITCH, real[0] - MARGIN, ZONE_PITCH)
    return r if inside_view(r) else None


def solve(real, mesh_width, mesh_height, prefer_side):
    """Hologram (centre, height cm) and card zone for the object's outline real (degrees)."""
    centre = (real[0] + real[2]) / 2
    for zone_width in ZONE_WIDTHS:
        for side in (prefer_side, -prefer_side):
            zone = card_zone(real, side, zone_width)
            if not zone:
                continue
            for size_deg in (24, 21, 18, 16):
                height = 2.0 * HOLOGRAM_DEPTH * math.tan(math.radians(size_deg / 2.0)) / max(1.0, mesh_width / mesh_height)
                h_deg = angular(height, HOLOGRAM_DEPTH)
                w_deg = angular(mesh_width * height / mesh_height, HOLOGRAM_DEPTH)
                hologram = best_spot(w_deg, h_deg, [real, zone],
                                     lambda y, p, r: gap(r, real) + 0.15 * abs(y) + 0.3 * abs(p + 3) + (25.0 if (y - centre) * side > 0 else 0.0))
                if hologram:
                    return hologram, height, zone
    return None


def spot_point(yaw, pitch, depth):
    """Viewing-spot space (X ahead, Y right, Z up from the floor) of a direction at a depth along X."""
    y = depth * math.tan(math.radians(yaw))
    z = EYE + math.hypot(depth, y) * math.tan(math.radians(pitch))
    return unreal.Vector(depth, y, z)


def heading(deg):
    return unreal.Vector(math.cos(math.radians(deg)), math.sin(math.radians(deg)), 0.0)


def to_world(origin, yaw, local):
    f, r = heading(yaw), heading(yaw + 90.0)
    return unreal.Vector(origin.x + f.x * local.x + r.x * local.y, origin.y + f.y * local.x + r.y * local.y, local.z)


def to_local(origin, yaw, world):
    f, r = heading(yaw), heading(yaw + 90.0)
    d = unreal.Vector(world.x - origin.x, world.y - origin.y, 0.0)
    return unreal.Vector(d.x * f.x + d.y * f.y, d.x * r.x + d.y * r.y, world.z)


# ====== Objects ====== //

def textured_material(mesh):
    """The Tripo mesh's own textured material (the import put the hologram material on slot 0)."""
    folder = mesh.get_path_name().rsplit("/StaticMeshes/", 1)[0]
    for path in assets.list_assets(folder, recursive=True):
        asset = assets.find_asset_data(path)
        if asset.asset_class_path.asset_name in ("Material", "MaterialInstanceConstant") and "Holo" not in str(asset.asset_name):
            return assets.load_asset(path.split(".")[0])
    return None


def place_object(k, data, materials, scan):
    number = data.get_editor_property("number")
    cfg = OBJECTS[number]
    north = k < 3
    x = BAY_X[k] if north else BAY_X[2 - (k - 3)]
    side = 1.0 if north else -1.0
    centre = unreal.Vector(x, side * OBJECT_Y, 0.0)
    view_yaw = 90.0 if north else -90.0           # the visitor looks at the wall the object stands by
    spot_location = unreal.Vector(x, side * (OBJECT_Y - VIEW_DISTANCE), 0.0)

    mesh = data.get_editor_property("mesh")
    if not isinstance(mesh, unreal.StaticMesh):
        mesh = assets.load_asset(str(mesh).split("'")[-2] if "'" in str(mesh) else str(mesh))
    bounds = mesh.get_bounds()
    mesh_height = 2.0 * bounds.box_extent.z
    scale = cfg["height"] / mesh_height if "height" in cfg else cfg["width"] / (2.0 * max(bounds.box_extent.x, bounds.box_extent.y))
    footprint = (2.0 * bounds.box_extent.x * scale, 2.0 * bounds.box_extent.y * scale)
    plinth_h = cfg["plinth"]
    tag = f"{number:02d}"

    # Plinth (or a table with a vitrine), the object on it, the mesh's bounds centre over the plinth's centre
    plinth_size = (max(footprint) + 40.0, max(footprint) + 40.0)
    box(f"Plinth{tag}", unreal.Vector(centre.x, centre.y, plinth_h / 2), (plinth_size[0], plinth_size[1], plinth_h), materials["plinth"], folder=f"Objects/{tag}")
    box(f"PlinthShadow{tag}", unreal.Vector(centre.x, centre.y, 1.5), (plinth_size[0] - 6, plinth_size[1] - 6, 3), materials["black"], collide=False, folder=f"Objects/{tag}")
    # The Tripo meshes face their local +Y
    yaw_mesh = view_yaw + 90.0 + cfg["turn"]
    real = spawn(unreal.StaticMeshActor, centre, yaw_mesh, f"Real{tag}")
    real.set_folder_path(f"Objects/{tag}")
    component = real.static_mesh_component
    component.set_static_mesh(mesh)
    real.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    own = textured_material(mesh)
    if own:
        for slot in range(component.get_num_materials()):
            component.set_material(slot, own)
    else:
        unreal.log_warning(f"{LOG} {tag}: no textured material next to {mesh.get_path_name()}")
    # Seat the mesh: its bounds' bottom on the plinth top, its bounds' centre over the plinth centre
    offset = unreal.Vector(bounds.origin.x * scale, bounds.origin.y * scale, 0.0)
    turned = unreal.MathLibrary.rotate_angle_axis(offset, yaw_mesh, unreal.Vector(0, 0, 1))
    real.set_actor_location(unreal.Vector(centre.x - turned.x, centre.y - turned.y, plinth_h - (bounds.origin.z - bounds.box_extent.z) * scale), False, False)
    top = plinth_h + mesh_height * scale

    if cfg["vitrine"]:
        glass_h = top - plinth_h + 25.0
        box(f"Vitrine{tag}", unreal.Vector(centre.x, centre.y, plinth_h + glass_h / 2 + 0.5), (plinth_size[0] - 4, plinth_size[1] - 4, glass_h),
            materials["glass"], collide=False, shadow=False, folder=f"Objects/{tag}")
        # Slim black frame on the glass edges
        hx, hy, bar = (plinth_size[0] - 4) / 2, (plinth_size[1] - 4) / 2, 1.6
        for sx in (-1, 1):
            for sy in (-1, 1):
                box(f"VitrinePost{tag}_{sx}{sy}", unreal.Vector(centre.x + sx * hx, centre.y + sy * hy, plinth_h + glass_h / 2), (bar, bar, glass_h),
                    materials["black"], collide=False, shadow=False, folder=f"Objects/{tag}")
        for sy in (-1, 1):
            box(f"VitrineTopX{tag}_{sy}", unreal.Vector(centre.x, centre.y + sy * hy, plinth_h + glass_h), (2 * hx, bar, bar), materials["black"], collide=False, shadow=False, folder=f"Objects/{tag}")
        for sx in (-1, 1):
            box(f"VitrineTopY{tag}_{sx}", unreal.Vector(centre.x + sx * hx, centre.y, plinth_h + glass_h), (bar, 2 * hy, bar), materials["black"], collide=False, shadow=False, folder=f"Objects/{tag}")
    if cfg["rope"]:
        reach = plinth_size[1] / 2 + 70.0
        posts = []
        for lateral in (-1, 1):
            p = to_world(centre, view_yaw + 180.0, unreal.Vector(reach, lateral * (plinth_size[0] / 2 + 30.0), 0.0))
            cylinder(f"Post{tag}_{lateral}", unreal.Vector(p.x, p.y, 47), 6, 94, materials["metal"], folder=f"Objects/{tag}")
            cylinder(f"PostBase{tag}_{lateral}", unreal.Vector(p.x, p.y, 1.5), 32, 3, materials["metal"], folder=f"Objects/{tag}")
            posts.append(p)
        mid = unreal.Vector((posts[0].x + posts[1].x) / 2, (posts[0].y + posts[1].y) / 2, 84)
        span = math.hypot(posts[1].x - posts[0].x, posts[1].y - posts[0].y)
        box(f"Rope{tag}", mid, (span, 3, 3), materials["rope"], yaw=view_yaw + 90.0, collide=False, folder=f"Objects/{tag}")

    # Wall plaque beside the object, away from the cards (they open on the visitor's right: -X on the north wall, +X on the south)
    name = data.get_editor_property("name").get_editor_property("indonesian")
    subtitle = data.get_editor_property("subtitle").get_editor_property("indonesian")
    lateral = 1.0 if north else -1.0  # world X side of the plaque from the object
    px = centre.x + lateral * 270.0
    box(f"Plaque{tag}", unreal.Vector(px, side * (HALF_Y - 2), 160), (190, 3, 100), materials["panel"], collide=False, folder=f"Objects/{tag}")
    face_yaw = view_yaw + 180.0
    inset = side * (HALF_Y - 5)
    text(f"PlaqueNo{tag}", unreal.Vector(px, inset, 188), face_yaw,
         f"{number:02d}", 20.0, (0.9, 0.75, 0.45), "center", folder=f"Objects/{tag}")
    text(f"PlaqueName{tag}", unreal.Vector(px, inset, 160), face_yaw, name, 10.0, (0.95, 0.93, 0.9), "center", folder=f"Objects/{tag}")
    text(f"PlaqueSub{tag}", unreal.Vector(px, inset, 140), face_yaw, subtitle, 7.5, (0.75, 0.72, 0.68), "center", folder=f"Objects/{tag}")

    # The documentation photo on the other side of the object, framed, with its own washer
    photo = import_photo(number)
    if photo:
        landscape = photo.blueprint_get_size_x() >= photo.blueprint_get_size_y()
        pw, ph = (230.0, 130.0) if landscape else (120.0, 213.0)
        qx = centre.x - lateral * 300.0
        wall = side * (HALF_Y - 4)
        box(f"PrintFrame{tag}", unreal.Vector(qx, side * (HALF_Y - 2), 215), (pw + 10, 4, ph + 10), materials["black"], collide=False, folder=f"Objects/{tag}")
        mi = instance(materials["print"], f"MI_Print_{number:02d}")
        mel.set_material_instance_texture_parameter_value(mi, "Photo", photo)
        mel.set_material_instance_vector_parameter_value(mi, "Origin", unreal.LinearColor(qx, wall, 215.0, 0.0))
        # Seen from the hall, the picture's right runs -X on the north wall and +X on the south wall
        mel.set_material_instance_vector_parameter_value(mi, "Right", unreal.LinearColor(-side, 0.0, 0.0, 0.0))
        mel.set_material_instance_scalar_parameter_value(mi, "Width", pw)
        mel.set_material_instance_scalar_parameter_value(mi, "Height", ph)
        save(mi)
        box(f"Print{tag}", unreal.Vector(qx, wall, 215), (pw, 2, ph), mi, collide=False, folder=f"Objects/{tag}")
        washer = unreal.Vector(qx, side * 380, HEIGHT - 30)
        spot(f"Spot{tag}_Print", washer, unreal.Vector(qx, side * HALF_Y, 215), 120.0, 20.0, 3600.0, False)
        fixture(f"Fixture{tag}_Print", washer, unreal.Vector(qx, side * HALF_Y, 215), materials)

    # Two track spots: a warm key from the visitor's side, a softer one from the other side; a washer on the plaque
    aim = unreal.Vector(centre.x, centre.y, plinth_h + (top - plinth_h) * 0.55)
    for name_, dx, power, cone in (("Key", -110.0, 900.0, 15.0), ("Fill", 130.0, 380.0, 19.0)):
        location = unreal.Vector(centre.x + dx * lateral, side * 330, HEIGHT - 30)
        spot(f"Spot{tag}_{name_}", location, aim, power, cone, 3100.0, True)
        fixture(f"Fixture{tag}_{name_}", location, aim, materials)
    plaque_spot = unreal.Vector(px, side * 330, HEIGHT - 30)
    spot(f"Spot{tag}_Plaque", plaque_spot, unreal.Vector(px, side * HALF_Y, 160), 140.0, 14.0, 3100.0, False)
    fixture(f"Fixture{tag}_Plaque", plaque_spot, unreal.Vector(px, side * HALF_Y, 160), materials)

    # Station, zone, exhibit (X away from the viewer, at the object's centre), gaze
    destination = spawn(unreal.WzPathGuideDestination, spot_location, view_yaw, f"Station{tag}")
    destination.set_editor_property("place_tag", data.get_editor_property("place_tag"))
    zone = spawn(unreal.TriggerBox, unreal.Vector(spot_location.x, spot_location.y, ZONE_EXTENT.z), view_yaw, f"Zone{tag}")
    zone.get_component_by_class(unreal.BoxComponent).set_box_extent(ZONE_EXTENT, True)
    exhibit = spawn(unreal.MuseumExhibit, centre, view_yaw, f"Exhibit{tag}")
    exhibit.set_editor_property("data", data)
    exhibit.set_editor_property("viewing_zone", zone)
    exhibit.set_editor_property("pointer_index", 10 + number)
    exhibit.set_editor_property("real_actor", real)
    exhibit.set_editor_property("scan_material", scan)

    # The real object's outline seen from the eye
    origin, extent = real.get_actor_bounds(False)
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                w = unreal.Vector(origin.x + sx * extent.x, origin.y + sy * extent.y, origin.z + sz * extent.z)
                corners.append(to_local(spot_location, view_yaw, w))
    yaws = [math.degrees(math.atan2(c.y, c.x)) for c in corners]
    pitches = [math.degrees(math.atan2(c.z - EYE, math.hypot(c.x, c.y))) for c in corners]
    outline = (min(yaws), min(pitches), max(yaws), max(pitches))
    local_centre = to_local(centre, view_yaw, origin)
    real_box = exhibit.get_editor_property("real_object")
    xs, ys = [c.x - VIEW_DISTANCE for c in corners], [c.y for c in corners]
    real_box.set_editor_property("relative_location", unreal.Vector(local_centre.x, local_centre.y, origin.z))
    real_box.set_box_extent(unreal.Vector((max(xs) - min(xs)) / 2, (max(ys) - min(ys)) / 2, extent.z), False)

    mesh_width = 2.0 * math.hypot(bounds.box_extent.x, bounds.box_extent.y)
    # Cards toward the hall's middle (more room than toward the entrance); the plaque is on the other side
    solved = solve(outline, mesh_width, mesh_height, 1)
    if not solved:
        unreal.log_error(f"{LOG} {tag}: no placement for hologram and cards")
        return
    hologram, twin_height, card_area = solved
    point = spot_point(hologram[0], hologram[1], HOLOGRAM_DEPTH)
    twin_scale = twin_height / mesh_height
    exhibit.get_editor_property("hologram_root").set_editor_property(
        "relative_location", unreal.Vector(point.x - VIEW_DISTANCE, point.y, point.z - bounds.origin.z * twin_scale))
    exhibit.set_editor_property("twin_height", twin_height)
    exhibit.set_editor_property("view_offset", unreal.Vector(-VIEW_DISTANCE, 0.0, EYE))
    exhibit.set_editor_property("panel_zone", unreal.Vector4(card_area[0], card_area[1], card_area[2], card_area[3]))
    slots = card_slots(number, outline, hologram[2], card_area)
    exhibit.set_editor_property("panel_slots", [s or unreal.MuseumPanelSlot() for s in slots])
    log(f"{tag} slots=" + " ".join("column" if not s else
        "(" + ",".join(f"{v:.1f}" for v in (s.area.x, s.area.y, s.area.z, s.area.w)) + f")@{s.anchor.x:.2f},{s.anchor.y:.0f}" for s in slots))

    items = [outline, hologram[2], card_area]
    gaze_yaw = max(-GAZE_YAW, min(GAZE_YAW, sum((r[0] + r[2]) / 2 for r in items) / 3))
    gaze_pitch = max(-GAZE_PITCH, min(GAZE_PITCH, sum((r[1] + r[3]) / 2 for r in items) / 3))
    spawn(unreal.TargetPoint, to_world(spot_location, view_yaw, spot_point(gaze_yaw, gaze_pitch, 300.0)), view_yaw, f"Gaze{tag}")
    for actor_ in (destination, zone, exhibit):
        actor_.set_folder_path(f"Objects/{tag}")

    log(f"{tag} scale={scale:.3f} size={footprint[0]:.0f}x{footprint[1]:.0f}x{mesh_height * scale:.0f} top={top:.0f} "
        f"outline={tuple(round(v, 1) for v in outline)} hologram=({hologram[0]},{hologram[1]}) h={twin_height:.0f} "
        f"zone={tuple(round(v, 1) for v in card_area)} gaze=({gaze_yaw:.1f},{gaze_pitch:.1f}) material={own.get_path_name() if own else None}")


def build():
    data = {int(p.split("_")[-2]): assets.load_asset(p.split(".")[0])
            for p in assets.list_assets(f"{ROOT}/Exhibits", recursive=True) if "/DA_Exhibit_" in p}
    data = [data[k] for k in sorted(data)]
    materials = gallery_materials()
    scan = scan_material()

    if assets.does_asset_exist(LEVEL):
        assets.delete_asset(LEVEL)
    levels.new_level(LEVEL)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", assets.load_asset("/Game/ZooVR/Blueprints/BP_ZooVRGameMode").generated_class())

    hall(materials)
    post_process()
    nav = spawn(unreal.NavMeshBoundsVolume, unreal.Vector((X0 + X1) / 2, 0, 150.0), label="NavBounds")
    _, extent = nav.get_actor_bounds(False)
    if extent.x > 1:
        nav.set_actor_scale3d(unreal.Vector((X1 - X0) / 2 / extent.x, HALF_Y / extent.y, 300.0 / extent.z))
    spawn(unreal.PlayerStart, unreal.Vector(X0 + 250, 0, 100), yaw=0.0, label="PlayerStart")

    for k, exhibit_data in enumerate(data):
        dress_cards(exhibit_data)
        place_object(k, exhibit_data, materials, scan)

    log(f"saved={levels.save_current_level()}")


build()
