# Builds the materials of the MR layer (Source/Museum/MR, Docs/STORYBOARD.md), rebuilt from scratch on every run:
#   M_MR_Glass  translucent glass with glowing edges (UV border) and a fresnel rim: card slabs, pedestals, the map table
#   M_MR_Glow   additive light: lines, chevrons, markers (Color x Intensity x vertex colour, alpha = vertex alpha)
#   M_MR_Ring   additive ring on a plane (UV 0..1): scan rings, pedestal rings, map pulses, optional dashes
# Also logs the typefaces of the UI fonts the code-built widgets use. Editor must be CLOSED (Tools/Editor/run_py.ps1).
import unreal

LOG = "[MuseumMR]"
FOLDER = "/Game/Museum/MR"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log(f"{LOG} {message}")


def new_material(name, blend, two_sided):
    path = f"{FOLDER}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", blend)
    material.set_editor_property("two_sided", two_sided)
    material.set_editor_property("used_with_static_lighting", False)
    for flag in ("used_with_nanite", "used_with_instanced_static_meshes"):
        try:
            material.set_editor_property(flag, False)
        except Exception:
            pass
    return material


def custom(material, code, inputs, output=unreal.CustomMaterialOutputType.CMOT_FLOAT3, x=-400, y=0, description="MR"):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("description", description)
    node.set_editor_property("output_type", output)
    entries = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        entries.append(entry)
    node.set_editor_property("inputs", entries)
    return node


def scalar(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector(material, name, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(*value))
    return node


def wire(material, node, inputs, scalars, vectors):
    """Feeds the custom node: built-ins by name, then scalar and vector parameters (vectors as their RGB)."""
    y = 0
    for name, source in inputs.items():
        mel.connect_material_expressions(source, "", node, name)
    for name, value in scalars.items():
        mel.connect_material_expressions(scalar(material, name, value, -900, y), "", node, name)
        y += 70
    for name, value in vectors.items():
        mel.connect_material_expressions(vector(material, name, value, -900, y), "RGB", node, name)
        y += 140


def finish(material, emissive, opacity=None):
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if opacity is not None:
        mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    assets.save_loaded_asset(material, only_if_is_dirty=False)
    log(f"material {material.get_path_name()}")


# ====== Glass ====== //

GLASS_EMISSIVE = r"""
float2 e = min(UV, 1.0 - UV);
float border = 1.0 - smoothstep(EdgeWidth * 0.6, EdgeWidth, min(e.x, e.y));
float rim = pow(saturate(1.0 - Fres), 3.0);
return Tint + EdgeColor * (EdgeGlow * border + FresnelGlow * rim);
"""

GLASS_OPACITY = r"""
float2 e = min(UV, 1.0 - UV);
float border = 1.0 - smoothstep(EdgeWidth * 0.6, EdgeWidth, min(e.x, e.y));
float rim = pow(saturate(1.0 - Fres), 3.0);
return saturate(Opacity + border * 0.85 + rim * 0.25) * Fade;
"""


def glass():
    material = new_material("M_MR_Glass", unreal.BlendMode.BLEND_TRANSLUCENT, False)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1200, -300)
    normal = mel.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -1400, -200)
    camera = mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1400, -100)
    dot = mel.create_material_expression(material, unreal.MaterialExpressionDotProduct, -1200, -150)
    mel.connect_material_expressions(normal, "", dot, "A")
    mel.connect_material_expressions(camera, "", dot, "B")
    absolute = mel.create_material_expression(material, unreal.MaterialExpressionAbs, -1100, -150)
    mel.connect_material_expressions(dot, "", absolute, "")

    emissive = custom(material, GLASS_EMISSIVE, ["UV", "Fres", "EdgeWidth", "EdgeGlow", "FresnelGlow", "Tint", "EdgeColor"], y=-100, description="GlassEmissive")
    opacity = custom(material, GLASS_OPACITY, ["UV", "Fres", "EdgeWidth", "Opacity", "Fade"], output=unreal.CustomMaterialOutputType.CMOT_FLOAT1, y=200, description="GlassOpacity")
    for node in (emissive, opacity):
        mel.connect_material_expressions(uv, "", node, "UV")
        mel.connect_material_expressions(absolute, "", node, "Fres")
    edge_width = scalar(material, "EdgeWidth", 0.02, -900, -400)
    for node in (emissive, opacity):
        mel.connect_material_expressions(edge_width, "", node, "EdgeWidth")
    wire(material, emissive, {}, {"EdgeGlow": 4.0, "FresnelGlow": 0.6}, {"Tint": (0.012, 0.03, 0.045, 1.0), "EdgeColor": (0.31, 0.85, 1.0, 1.0)})
    wire(material, opacity, {}, {"Opacity": 0.72, "Fade": 1.0}, {})
    finish(material, emissive, opacity)


# ====== Glow ====== //

GLOW = r"""
return Color * Intensity * VC.rgb * VA * Fade;
"""


def glow():
    material = new_material("M_MR_Glow", unreal.BlendMode.BLEND_ADDITIVE, True)
    vertex = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -1200, 0)
    node = custom(material, GLOW, ["VC", "VA", "Color", "Intensity", "Fade"], description="Glow")
    # Colour and alpha apart (some passes, e.g. Lumen cards, see the vertex colour as float3)
    mel.connect_material_expressions(vertex, "", node, "VC")
    mel.connect_material_expressions(vertex, "A", node, "VA")
    wire(material, node, {}, {"Intensity": 4.0, "Fade": 1.0}, {"Color": (0.31, 0.85, 1.0, 1.0)})
    finish(material, node)


# ====== Ring ====== //

RING = r"""
float2 p = UV - 0.5;
float d = length(p) * 2.0;
float soft = max(Softness, 0.001);
float ring = smoothstep(Radius - Width - soft, Radius - Width, d) * (1.0 - smoothstep(Radius, Radius + soft, d));
float dash = 1.0;
if (Segments > 0.5)
{
    float a = atan2(p.y, p.x) / 6.2831853 + 0.5;
    dash = step(0.35, frac(a * Segments + Time * Spin));
}
float fill = Fill * pow(saturate(1.0 - d / max(Radius, 0.001)), 1.5) * step(d, Radius);
return Color * Intensity * (ring * dash + fill) * Fade;
"""


def ring():
    material = new_material("M_MR_Ring", unreal.BlendMode.BLEND_ADDITIVE, True)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1200, -200)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1200, -100)
    node = custom(material, RING, ["UV", "Time", "Radius", "Width", "Softness", "Segments", "Spin", "Fill", "Intensity", "Fade", "Color"], description="Ring")
    mel.connect_material_expressions(uv, "", node, "UV")
    mel.connect_material_expressions(time, "", node, "Time")
    wire(material, node, {}, {"Radius": 0.9, "Width": 0.06, "Softness": 0.03, "Segments": 0.0, "Spin": 0.0, "Fill": 0.0,
                              "Intensity": 4.0, "Fade": 1.0}, {"Color": (0.31, 0.85, 1.0, 1.0)})
    finish(material, node)


# ====== Fonts ====== //

def fonts():
    for path in ("/Game/Zoo/Fonts/F_UI_Inter", "/Game/Zoo/Fonts/F_UI_Rajdhani"):
        font = assets.load_asset(path)
        if not font:
            log(f"font {path} missing")
            continue
        names = []
        try:
            composite = font.get_editor_property("composite_font")
            typeface = composite.get_editor_property("default_typeface")
            names = [str(entry.get_editor_property("name")) for entry in typeface.get_editor_property("fonts")]
        except Exception as error:
            names = [f"? {error}"]
        log(f"font {path} typefaces={names}")


glass()
glow()
ring()
fonts()
log("done")
