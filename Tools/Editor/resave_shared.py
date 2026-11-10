# Loads the Zoo assets copied into this project (Tools/Assets/zoo_asset_closure.py), compiles their Blueprints against
# the Museum classes (CoreRedirects) and saves them, so they no longer depend on the redirects. Also lists the
# hologram material's parameters (the contract the artifact material must keep). Editor must be CLOSED.
import unreal

LOG = "[MuseumShared]"
assets = unreal.EditorAssetLibrary

paths = []
for root in ("/Game/Zoo", "/Game/ZooVR"):
    paths += [p.split(".")[0] for p in assets.list_assets(root, recursive=True, include_folder=False)]
unreal.log(f"{LOG} {len(paths)} assets")

# Blueprints first (their compile errors are what matters), then everything is saved
failed = []
for path in paths:
    asset = assets.load_asset(path)
    if not asset:
        failed.append(path)
        unreal.log_warning(f"{LOG} could not load {path}")
        continue
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        generated = asset.generated_class()
        cdo = unreal.get_default_object(generated) if generated else None
        # The CDO's class chain shows whether the Zoo parent was redirected to the Museum class
        chain = []
        cls = generated
        while cls and len(chain) < 4:
            chain.append(cls.get_name())
            try:
                cls = cls.get_super_class()
            except Exception:
                break
        unreal.log(f"{LOG} BP {path} class={chain} cdo={'ok' if cdo else None}")

saved = assets.save_directory("/Game/Zoo", only_if_is_dirty=False, recursive=True) and assets.save_directory("/Game/ZooVR", only_if_is_dirty=False, recursive=True)
unreal.log(f"{LOG} saved={saved} failed_loads={failed}")

# The hologram material's parameters
mel = unreal.MaterialEditingLibrary
for path in ("/Game/Zoo/Assets/Hologram/Materials/M_ZooHologram",):
    material = assets.load_asset(path)
    for kind, getter in (("scalar", mel.get_scalar_parameter_names), ("vector", mel.get_vector_parameter_names), ("texture", mel.get_texture_parameter_names), ("switch", mel.get_static_switch_parameter_names)):
        unreal.log(f"{LOG} {path} {kind}: {list(getter(material))}")
    unreal.log(f"{LOG} {path} blend={material.get_editor_property('blend_mode')} shading={material.get_editor_property('shading_model')} two_sided={material.get_editor_property('two_sided')}")
