# Checks what import_exhibits.py built: each DA_Exhibit_* (validation, tags, mesh, Nanite, hologram material, sounds),
# the place cards and the game modes' HUD. Removes the copied BP_Zoo_HUD once nothing references it (it does not compile
# without the Zoo's AR media widgets; the original stays in ZooEka2VR). Editor must be CLOSED.
import unreal

LOG = "[MuseumVerify]"
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log(f"{LOG} {message}")


for path in sorted(p.split(".")[0] for p in assets.list_assets("/Game/Museum/Exhibits", recursive=True) if "/DA_Exhibit_" in p):
    data = assets.load_asset(path)
    mesh = data.get_editor_property("mesh").load_synchronous() if hasattr(data.get_editor_property("mesh"), "load_synchronous") else data.get_editor_property("mesh")
    nanite = mesh.get_editor_property("nanite_settings") if mesh else None
    material = mesh.get_material(0) if mesh else None
    texture = mel.get_material_instance_texture_parameter_value(material, "BaseColorTex") if isinstance(material, unreal.MaterialInstance) else None
    panels = data.get_editor_property("panels")
    sounds = [data.get_editor_property("title_voice_over")] + [p.get_editor_property("voice_over") for p in panels]
    missing = sum(1 for s in sounds for lang in ("indonesian", "english") if not s.get_editor_property(lang))
    log(f"{data.get_name()} place={data.get_editor_property('place_tag').export_text()} visited={data.get_editor_property('visited_event').export_text()} "
        f"name={data.get_editor_property('name').get_editor_property('indonesian')} / {data.get_editor_property('name').get_editor_property('english')} "
        f"panels={len(panels)} missing_sounds={missing} thumb={bool(data.get_editor_property('thumbnail'))}")
    log(f"  mesh={mesh.get_name() if mesh else None} nanite={nanite.get_editor_property('enabled') if nanite else None} "
        f"fallback%={nanite.get_editor_property('fallback_percent_triangles') if nanite else None} "
        f"material={material.get_name() if material else None} BaseColorTex={texture.get_name() if texture else None}")
    log(f"  panel1 ID: {panels[0].get_editor_property('heading').get_editor_property('indonesian')} | {panels[0].get_editor_property('title').get_editor_property('indonesian')}")
    log(f"  panel3 EN: {panels[2].get_editor_property('heading').get_editor_property('english')} | {panels[2].get_editor_property('title').get_editor_property('english')}")

cards = assets.load_asset("/Game/Museum/PathGuide/DA_MuseumPlaceCards").get_editor_property("cards")
log("cards: " + ", ".join(f"{c.get_editor_property('number')}={c.get_editor_property('place_tag').export_text()}" for c in cards))

for path in ("/Game/Zoo/Blueprints/BP_ZooGameMode", "/Game/ZooVR/Blueprints/BP_ZooVRGameMode"):
    cdo = unreal.get_default_object(assets.load_asset(path).generated_class())
    log(f"{path} hud={cdo.get_editor_property('hud_class').get_name()}")

hud = "/Game/Zoo/Blueprints/BP_Zoo_HUD"
if assets.does_asset_exist(hud):
    referencers = [r for r in assets.find_package_referencers_for_asset(hud, load_assets_to_confirm=True) if r != hud]
    log(f"BP_Zoo_HUD referencers={referencers}")
    if not referencers:
        log(f"BP_Zoo_HUD deleted={assets.delete_asset(hud)}")
