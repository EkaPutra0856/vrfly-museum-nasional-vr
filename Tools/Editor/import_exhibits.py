# Imports the 6 exhibits staged by Tools/Assets/stage_exhibits.py (Saved/ImportStaging/exhibits.json) and builds their data.
# Editor must be CLOSED. Run with the commandlet (see run_py.ps1). Safe to re-run: existing assets are updated, the GLBs are
# only imported again with -MuseumReimport on the command line.
#
# Per exhibit, in /Game/Museum/Exhibits/<nn>_<Short>/:
#   Mesh/                    the GLB (Interchange): static mesh, its textured material, textures. Nanite on; slot 0 gets
#                            the hologram material (translucent, so the Nanite fallback mesh is what renders).
#   MI_Holo_<nn>_<Short>     M_ZooHologram with the GLB's base colour (BaseColorTex)
#   Audio/SW_<nn>_<ID|EN>_*  voice-overs: Judul/Title, Panel1..3
#   T_<nn>_<Short>           thumbnail (UI texture)
#   DA_Exhibit_<nn>_<Short>  UMuseumExhibitData
# Shared: /Game/Museum/Hologram/DA_MuseumHologramProfile, /Game/Museum/PathGuide/DA_MuseumPlaceCards,
#         /Game/Museum/Media/FMS_Exhibit<nn>_<Short> (demo plates), /Game/Museum/Blueprints/BP_MuseumHUD.
# Fixes on the copied Zoo assets: BP_ZooPathGuideWC gets the place cards and the bilingual button texts; the game modes use
# BP_MuseumHUD (BP_Zoo_HUD does not compile here: its AR media widgets were not copied).
import json
import os

import unreal

LOG = "[MuseumImport]"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

ROOT = "/Game/Museum"
HOLOGRAM_MATERIAL = "/Game/Zoo/Assets/Hologram/Materials/M_ZooHologram"
HOLOGRAM_SCALE = 0.6           # provisional: the twin beside the real object
NANITE_FALLBACK_PERCENT = 0.1  # ~200k of ~2M triangles; translucent materials render the fallback
REIMPORT = "-museumreimport" in unreal.SystemLibrary.get_command_line().lower()
SPOKEN = {"ID": ["Judul", "Panel1", "Panel2", "Panel3"], "EN": ["Title", "Panel1", "Panel2", "Panel3"]}


def log(message):
    unreal.log(f"{LOG} {message}")


def import_file(filename, folder, name=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", folder)
    if name:
        task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    return [assets.load_asset(p) for p in task.get_editor_property("imported_object_paths")]


def get_or_create(name, folder, cls, factory):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def data_asset(name, folder, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return get_or_create(name, folder, cls, factory)


def tag(name):
    value = unreal.GameplayTag()
    value.import_text(f'(TagName="{name}")')
    if name not in value.export_text():
        unreal.log_error(f"{LOG} gameplay tag {name} is not registered (Config/DefaultGameplayTags.ini)")
    return value


def text(pair):
    return unreal.MuseumText(indonesian=pair["ID"], english=pair["EN"])


def assets_in(folder, cls):
    return [a for a in (assets.load_asset(p) for p in assets.list_assets(folder, recursive=True, include_folder=False)) if isinstance(a, cls)]


def save(*objects):
    for obj in objects:
        assets.save_loaded_asset(obj, only_if_is_dirty=False)


# ====== Mesh + hologram material ====== //

def base_color_texture(mesh_folder):
    """The GLB's base colour, read from the imported material's texture parameters; by name when that fails."""
    for mi in assets_in(mesh_folder, unreal.MaterialInstanceConstant):
        parent = mi.get_base_material()
        for param in (mel.get_texture_parameter_names(parent) if parent else []):
            key = str(param).lower()
            if "base" in key or "albedo" in key or "diffuse" in key:
                texture = mel.get_material_instance_texture_parameter_value(mi, param)
                if texture:
                    return texture
    textures = assets_in(mesh_folder, unreal.Texture2D)
    for texture in textures:
        key = texture.get_name().lower()
        if any(k in key for k in ("basecolor", "base_color", "albedo", "diffuse", "color")):
            return texture
    srgb = [t for t in textures if t.get_editor_property("srgb")]
    return srgb[0] if srgb else None


def enable_nanite(mesh):
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    try:
        settings.set_editor_property("fallback_target", unreal.NaniteFallbackTarget.PERCENT_TRIANGLES)
    except Exception as error:
        unreal.log_warning(f"{LOG} fallback_target: {error}")
    settings.set_editor_property("fallback_percent_triangles", NANITE_FALLBACK_PERCENT)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if hasattr(subsystem, "set_nanite_settings"):
        subsystem.set_nanite_settings(mesh, settings, apply_changes=True)
    else:
        mesh.set_editor_property("nanite_settings", settings)


def import_mesh(exhibit, folder):
    mesh_folder = f"{folder}/Mesh"
    meshes = assets_in(mesh_folder, unreal.StaticMesh) if assets.does_directory_exist(mesh_folder) else []
    if not meshes or REIMPORT:
        import_file(exhibit["glb"], mesh_folder)
        meshes = assets_in(mesh_folder, unreal.StaticMesh)
    if len(meshes) != 1:
        unreal.log_error(f"{LOG} {exhibit['short']}: {len(meshes)} static meshes in {mesh_folder}, expected 1")
        if not meshes:
            return None, None
    mesh = meshes[0]

    tag_name = f"{exhibit['number']:02d}_{exhibit['short']}"
    hologram = get_or_create(f"MI_Holo_{tag_name}", folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(hologram, assets.load_asset(HOLOGRAM_MATERIAL))
    texture = base_color_texture(mesh_folder)
    if texture:
        mel.set_material_instance_texture_parameter_value(hologram, "BaseColorTex", texture)
    else:
        unreal.log_warning(f"{LOG} {tag_name}: no base colour texture found")
    mel.update_material_instance(hologram)

    enable_nanite(mesh)
    mesh.set_material(0, hologram)
    bounds = mesh.get_bounds()
    log(f"{tag_name} mesh={mesh.get_path_name()} height={bounds.box_extent.z * 2:.1f}cm size={bounds.box_extent * 2} "
        f"slots={len(mesh.get_editor_property('static_materials'))} basecolor={texture.get_name() if texture else None}")
    save(hologram, mesh)
    return mesh, hologram


# ====== Audio + thumbnail ====== //

def import_audio(exhibit, folder):
    sounds = {}
    for lang in ("ID", "EN"):
        sounds[lang] = []
        for wav, spoken in zip(exhibit["audio"][lang], SPOKEN[lang]):
            name = f"SW_{exhibit['number']:02d}_{lang}_{spoken}"
            path = f"{folder}/Audio/{name}"
            sound = assets.load_asset(path) if assets.does_asset_exist(path) and not REIMPORT else None
            if not sound:
                imported = import_file(wav, f"{folder}/Audio", name)
                sound = imported[0] if imported else None
            if not sound:
                unreal.log_error(f"{LOG} could not import {wav}")
            sounds[lang].append(sound)
    return sounds


def import_thumbnail(exhibit, folder):
    name = os.path.splitext(os.path.basename(exhibit["thumbnail"]))[0]
    path = f"{folder}/{name}"
    texture = assets.load_asset(path) if assets.does_asset_exist(path) and not REIMPORT else None
    if not texture:
        imported = import_file(exhibit["thumbnail"], folder, name)
        texture = imported[0] if imported else None
    if texture:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("never_stream", True)
        save(texture)
    return texture


def media_source(exhibit):
    name = f"FMS_Exhibit{exhibit['number']:02d}_{exhibit['short']}"
    source = get_or_create(name, f"{ROOT}/Media", unreal.FileMediaSource, unreal.FileMediaSourceFactoryNew())
    source.set_file_path(exhibit["video"])
    save(source)
    return source


# ====== Data ====== //

def hologram_profile():
    profile = data_asset("DA_MuseumHologramProfile", f"{ROOT}/Hologram", unreal.MuseumHologramProfile)
    # As the Zoo animals' profiles, minus NS_ZooHolo_Materialize: it samples skeletal meshes only.
    profile.set_editor_property("ambient_effect", assets.load_asset("/Game/Zoo/Assets/Hologram/VFX/NS_ZooHolo_Ambient"))
    profile.set_editor_property("appear_sound", assets.load_asset("/Game/Zoo/PathGuide/UI/Sounds/S_PG_Open"))
    profile.set_editor_property("disappear_sound", assets.load_asset("/Game/Zoo/PathGuide/UI/Sounds/S_PG_Close"))
    profile.set_editor_property("glitch_sound", assets.load_asset("/Game/Zoo/PathGuide/UI/Sounds/S_PG_Select"))
    save(profile)
    return profile


def exhibit_data(exhibit, folder, mesh, sounds, thumbnail, profile):
    number = exhibit["number"]
    data = data_asset(f"DA_Exhibit_{number:02d}_{exhibit['short']}", folder, unreal.MuseumExhibitData)
    data.set_editor_property("number", number)
    data.set_editor_property("place_tag", tag(f"PathGuide.Place.Museum.{number:02d}.{exhibit['short']}"))
    data.set_editor_property("visited_event", tag(f"Narrative.Event.Museum.Visited.{number:02d}"))
    data.set_editor_property("name", text(exhibit["name"]))
    data.set_editor_property("subtitle", text(exhibit["subtitle"]))
    if mesh:
        data.set_editor_property("mesh", mesh)
    data.set_editor_property("real_height", 0.0)
    data.set_editor_property("hologram_scale", HOLOGRAM_SCALE)
    data.set_editor_property("hologram_profile", profile)
    data.set_editor_property("title_voice_over", unreal.MuseumSound(indonesian=sounds["ID"][0], english=sounds["EN"][0]))
    panels = []
    for i in range(3):
        pid, pen = exhibit["panels"]["ID"][i], exhibit["panels"]["EN"][i]
        panels.append(unreal.MuseumExhibitPanel(
            heading=unreal.MuseumText(indonesian=pid["heading"], english=pen["heading"]),
            title=unreal.MuseumText(indonesian=pid["title"], english=pen["title"]),
            body=unreal.MuseumText(indonesian=pid["body"], english=pen["body"]),
            voice_over=unreal.MuseumSound(indonesian=sounds["ID"][i + 1], english=sounds["EN"][i + 1])))
    data.set_editor_property("panels", panels)
    if thumbnail:
        data.set_editor_property("thumbnail", thumbnail)
    save(data)
    log(f"{data.get_path_name()} panels={len(panels)} sounds={sum(1 for l in sounds.values() for s in l if s)}/8")
    return data


def place_cards(exhibits):
    cards_asset = data_asset("DA_MuseumPlaceCards", f"{ROOT}/PathGuide", unreal.MuseumPlaceCardsAsset)
    cards = []
    for exhibit, data in exhibits:
        cards.append(unreal.MuseumPlaceCard(
            place_tag=data.get_editor_property("place_tag"), number=exhibit["number"],
            name=text(exhibit["name"]), description=text(exhibit["subtitle"]),
            thumbnail=data.get_editor_property("thumbnail")))
    cards_asset.set_editor_property("cards", cards)
    save(cards_asset)
    log(f"{cards_asset.get_path_name()} cards={len(cards)}")
    return cards_asset


# ====== Copied Zoo Blueprints ====== //

def blueprint_cdo(path):
    blueprint = assets.load_asset(path)
    return blueprint, unreal.get_default_object(blueprint.generated_class())


def fix_path_guide(cards_asset):
    blueprint, cdo = blueprint_cdo("/Game/Zoo/Blueprints/Widgets/PathGuide/BP_ZooPathGuideWC")
    cdo.set_editor_property("place_cards", cards_asset)
    cdo.set_editor_property("navigate_button_text", unreal.MuseumText(indonesian="Navigasi", english="Navigate"))
    cdo.set_editor_property("guiding_button_format", unreal.MuseumText(indonesian="Menuju: {Place}", english="Heading to: {Place}"))
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)
    log(f"BP_ZooPathGuideWC place_cards={cdo.get_editor_property('place_cards').get_name()}")


def museum_hud():
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MuseumHUD)
    blueprint = get_or_create("BP_MuseumHUD", f"{ROOT}/Blueprints", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    _, zoo = blueprint_cdo("/Game/Zoo/Blueprints/BP_Zoo_HUD")
    for prop in ("path_guide_widget_class", "path_guide_widget_controller_class", "narrative_widget_class", "narrative_widget_controller_class"):
        value = zoo.get_editor_property(prop)
        cdo.set_editor_property(prop, value)
        log(f"BP_MuseumHUD {prop}={value.get_name() if value else None}")
    save(blueprint)

    for path in ("/Game/Zoo/Blueprints/BP_ZooGameMode", "/Game/ZooVR/Blueprints/BP_ZooVRGameMode"):
        game_mode, mode_cdo = blueprint_cdo(path)
        old = mode_cdo.get_editor_property("hud_class")
        mode_cdo.set_editor_property("hud_class", blueprint.generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(game_mode)
        save(game_mode)
        log(f"{path} hud_class {old.get_name() if old else None} -> BP_MuseumHUD_C")


def main():
    staging = os.path.join(unreal.Paths.project_saved_dir(), "ImportStaging", "exhibits.json")
    with open(staging, encoding="utf-8") as f:
        exhibits = json.load(f)
    log(f"{len(exhibits)} exhibits, reimport={REIMPORT}")

    profile = hologram_profile()
    built = []
    for exhibit in exhibits:
        folder = f"{ROOT}/Exhibits/{exhibit['number']:02d}_{exhibit['short']}"
        mesh, _ = import_mesh(exhibit, folder)
        sounds = import_audio(exhibit, folder)
        thumbnail = import_thumbnail(exhibit, folder)
        media_source(exhibit)
        built.append((exhibit, exhibit_data(exhibit, folder, mesh, sounds, thumbnail, profile)))

    cards_asset = place_cards(built)
    fix_path_guide(cards_asset)
    museum_hud()
    assets.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
    log("done")


main()
