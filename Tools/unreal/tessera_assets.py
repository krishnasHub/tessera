"""
Builds a game's Unreal assets from code, inside the editor's Python (UnrealEditor-Cmd -run=pythonscript). Tessera
ships no assets: each game runs its own script that calls these, and the assets land in the game's Content.

  materials   fresh(), scalar(), vector(), rgba(), const(), const3(), custom(), mul(), finish(): build a material
              graph node by node (regenerated from scratch every run, so the script is the source of truth)
  textures    import_textures(): a folder of PNGs as crisp pixel-art textures (nearest filter, no mips), with
              name prefixes that import smooth instead (e.g. high-res portraits)
  Tessera's   the materials Tessera's C++ drives, with the parameters it sets:
  materials     sprite()       <world>.assets.sprite      Tex, Cols, Rows, Col, Row, Flip, Tint, Flash, Emissive
                pixel_world()  <world>.assets.pixelWorld  Tex, Size, Tint
                night_shade()  <world>.assets.nightShade  Night, Hero, Light0..7
                glow()         <world>.assets.glow        Color, Intensity
                telegraph()    <world>.assets.telegraph   Color, Intensity, Opacity
                fresnel()      a rim-lit bubble           Color, Intensity, Opacity
                flash()        <world>.assets.flash       Color, Opacity

Use from a game's script:

    import os, sys
    sys.path.insert(0, os.path.join(<project>, "Plugins", "Tessera", "Tools", "unreal"))
    import tessera_assets as ta
    ta.import_textures(src_dir, "/Game/MyGame/Pixel", smooth_prefixes=("POR_",))
    ta.sprite("/Game/MyGame/Materials", "M_Sprite", default_texture="/Game/MyGame/Pixel/SPR_hero")
"""
import glob
import os

import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

# Usage flags a material may need (instanced foliage, characters, effects). Unknown ones are skipped.
DEFAULT_USAGE = ("used_with_instanced_static_meshes", "used_with_skeletal_mesh")
EFFECT_USAGE = ("used_with_skeletal_mesh", "used_with_instanced_static_meshes", "used_with_niagara_sprites",
                "used_with_niagara_mesh_particles", "used_with_particle_sprites")


def log(msg):
    unreal.log("[Tessera] " + msg)


# --- material graph helpers -------------------------------------------------------------------------

def fresh(folder, name, usage=DEFAULT_USAGE):
    """A new, empty material folder/name (deleting the old one)."""
    full = folder + "/" + name
    if eal.does_asset_exist(full):
        eal.delete_asset(full)
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    for flag in usage:
        try:
            mat.set_editor_property(flag, True)
        except Exception:
            unreal.log_warning("[Tessera] usage flag not available in this engine version: " + flag)
    return mat


def scalar(m, name, v, x=-600, y=200):
    e = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", v)
    return e


def vector(m, name, v, x=-600, y=0):
    e = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", unreal.LinearColor(*v))
    return e


def rgba(m, name, value, x, y):
    """A vector parameter as a full float4 (a vector parameter's default output is RGB only)."""
    v = vector(m, name, value, x, y)
    app = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, x + 250, y)
    mel.connect_material_expressions(v, "", app, "A")
    mel.connect_material_expressions(v, "A", app, "B")
    return app


def const3(m, v, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", unreal.LinearColor(v[0], v[1], v[2], 1))
    return e


def const(m, v, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    e.set_editor_property("r", v)
    return e


def custom(m, code, inputs, out_type, x, y):
    """An HLSL Custom node with named inputs."""
    c = mel.create_material_expression(m, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    ins = []
    for n in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    c.set_editor_property("inputs", ins)
    return c


def mul(m, a, b, x=-300, y=0):
    e = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, x, y)
    mel.connect_material_expressions(a, "", e, "A")
    mel.connect_material_expressions(b, "", e, "B")
    return e


def finish(m):
    mel.recompile_material(m)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    log("created " + m.get_path_name())


# --- textures ------------------------------------------------------------------------------------------

def import_textures(src_dir, dest, smooth_prefixes=()):
    """Every PNG in src_dir -> dest/<file name>: nearest filtering, no mips, uncompressed (crisp pixels). Names
    starting with one of smooth_prefixes import as smooth UI textures instead. Returns the number imported."""
    tasks = []
    for path in sorted(glob.glob(os.path.join(src_dir, "*.png"))):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.abspath(path))
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    for t in tasks:
        for obj_path in t.get_editor_property("imported_object_paths"):
            tex = unreal.load_asset(obj_path)
            if not isinstance(tex, unreal.Texture2D):
                continue
            if tex.get_name().startswith(tuple(smooth_prefixes)):
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
                tex.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
            else:
                tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_PIXELS2D)
                tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("never_stream", True)
            eal.save_loaded_asset(tex, only_if_is_dirty=False)
    log("imported %d textures into %s" % (len(tasks), dest))
    return len(tasks)


# --- Tessera's materials -------------------------------------------------------------------------------

def sprite(folder, name, default_texture=None, cols=4.0, rows=13.0):
    """Masked, lit, two-sided card playing one frame of a sprite sheet. Its normal is world-up, so a card facing
    the camera is lit like the ground under it. Params: Tex, Cols, Rows, Col, Row, Flip (mirror), Tint, Flash
    (hit flash), Emissive."""
    m = fresh(folder, name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("tangent_space_normal", False)
    m.set_editor_property("opacity_mask_clip_value", 0.5)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    calc = custom(m, "float u = lerp(UV.x, 1.0 - UV.x, Flip);\nreturn float2((u + Col) / Cols, (UV.y + Row) / Rows);",
                  ["UV", "Cols", "Rows", "Col", "Row", "Flip"], unreal.CustomMaterialOutputType.CMOT_FLOAT2, -1000, 0)
    mel.connect_material_expressions(uv, "", calc, "UV")
    for i, (n, v) in enumerate((("Cols", cols), ("Rows", rows), ("Col", 0.0), ("Row", 0.0), ("Flip", 0.0))):
        mel.connect_material_expressions(scalar(m, n, v, -1400, 120 + i * 90), "", calc, n)
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "Tex")
    sheet = unreal.load_asset(default_texture) if default_texture else None
    if sheet:
        tex.set_editor_property("texture", sheet)
    mel.connect_material_expressions(calc, "", tex, "UVs")
    tinted = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(tex, "RGB", tinted, "A")
    mel.connect_material_expressions(vector(m, "Tint", (1, 1, 1, 1), -700, 300), "", tinted, "B")
    flash = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -200, 0)
    mel.connect_material_expressions(tinted, "", flash, "A")
    mel.connect_material_expressions(const3(m, (1, 1, 1), -400, 200), "", flash, "B")
    mel.connect_material_expressions(scalar(m, "Flash", 0.0, -400, 320), "", flash, "Alpha")
    mel.connect_material_property(flash, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emis = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -100, 300)
    mel.connect_material_expressions(flash, "", emis, "A")
    mel.connect_material_expressions(scalar(m, "Emissive", 0.0, -400, 440), "", emis, "B")
    mel.connect_material_property(emis, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(const3(m, (0, 0, 1), -200, 500), "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(const(m, 1.0, -200, 600), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(m, 0.1, -200, 680), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)
    return m


def pixel_world(folder, name, default_texture=None):
    """Opaque, lit: a pixel-art texture projected by world position along whichever axis a face points (no UVs
    needed). Params: Tex, Size (world units per texture repeat), Tint."""
    m = fresh(folder, name)
    texobj = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -1000, 0)
    texobj.set_editor_property("parameter_name", "Tex")
    t = unreal.load_asset(default_texture) if default_texture else None
    if t:
        texobj.set_editor_property("texture", t)
    wp = mel.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -1000, 200)
    nrm = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -1000, 300)
    code = """float3 n = abs(N);
float3 p = WP / Size;
float2 uv = (n.z >= n.x && n.z >= n.y) ? p.xy : (n.x >= n.y ? float2(p.y, -p.z) : float2(p.x, -p.z));
return Texture2DSample(Tex, TexSampler, uv).rgb;"""
    tri = custom(m, code, ["Tex", "WP", "N", "Size"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, 0)
    mel.connect_material_expressions(texobj, "", tri, "Tex")
    mel.connect_material_expressions(wp, "", tri, "WP")
    mel.connect_material_expressions(nrm, "", tri, "N")
    mel.connect_material_expressions(scalar(m, "Size", 200.0, -1000, 420), "", tri, "Size")
    tmul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 0)
    mel.connect_material_expressions(tri, "", tmul, "A")
    mel.connect_material_expressions(vector(m, "Tint", (1, 1, 1, 1), -600, 300), "", tmul, "B")
    mel.connect_material_property(tmul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(m, 0.92, -300, 300), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(m, 0.15, -300, 380), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)
    return m


def night_shade(folder, name):
    """UI overlay for deep night (STSNightShade): near-black everywhere except ellipses around the hero and the
    nearest lights. Hero / Light0..7 = (u, v, radius u, radius v) in screen UV; Night = 0..1 strength."""
    m = fresh(folder, name)
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uvn = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    names = ["UV", "Hero", "Night"] + ["L%d" % i for i in range(8)]
    code = """float vis = 0.0;
float4 H[9] = { Hero, L0, L1, L2, L3, L4, L5, L6, L7 };
for (int i = 0; i < 9; i++)
{
    if (H[i].z <= 0.0) continue;
    float d = length((UV - H[i].xy) / H[i].zw);
    vis = max(vis, 1.0 - smoothstep(i == 0 ? 0.5 : 0.4, 1.0, d));
}
return Night * (1.0 - vis);"""
    shade = custom(m, code, names, unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700, 0)
    mel.connect_material_expressions(uvn, "", shade, "UV")
    mel.connect_material_expressions(rgba(m, "Hero", (0.5, 0.5, 0.3, 0.3), -1700, 150), "", shade, "Hero")
    mel.connect_material_expressions(scalar(m, "Night", 0.0, -1400, 300), "", shade, "Night")
    for i in range(8):
        mel.connect_material_expressions(rgba(m, "Light%d" % i, (0, 0, 0, 0), -1700, 400 + i * 100), "", shade, "L%d" % i)
    mel.connect_material_property(const3(m, (0.0, 0.0, 0.0), -400, 200), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)   # true black: a lifted tone reads as grey
    mel.connect_material_property(shade, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def glow(folder, name, color=(1.0, 0.5, 0.2, 1), intensity=8.0):
    """Unlit, Color * Intensity: projectiles, magic orbs, embers."""
    m = fresh(folder, name, EFFECT_USAGE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(mul(m, vector(m, "Color", color), scalar(m, "Intensity", intensity)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def telegraph(folder, name, color=(1.0, 0.15, 0.1, 1), intensity=3.0, opacity=0.35):
    """Translucent unlit, two-sided: attack wind-up markers on the ground."""
    m = fresh(folder, name, EFFECT_USAGE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    mel.connect_material_property(mul(m, vector(m, "Color", color), scalar(m, "Intensity", intensity)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar(m, "Opacity", opacity, y=400), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def fresnel(folder, name, color=(0.5, 0.6, 1.0, 1), intensity=4.0, opacity=0.6, exponent=2.5):
    """Translucent unlit rim glow: shields, auras."""
    m = fresh(folder, name, EFFECT_USAGE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -900, 300)
    fres.set_editor_property("exponent", exponent)
    col = mul(m, vector(m, "Color", color), scalar(m, "Intensity", intensity))
    mel.connect_material_property(mul(m, col, fres, -150, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(mul(m, fres, scalar(m, "Opacity", opacity, y=500), -150, 300), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def flash(folder, name, color=(3.0, 3.0, 3.0, 1), opacity=0.55):
    """Translucent unlit white overlay: hit flashes (used as an overlay material)."""
    m = fresh(folder, name, EFFECT_USAGE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel.connect_material_property(vector(m, "Color", color), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar(m, "Opacity", opacity, y=300), "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)
    return m


def flag_for_instancing(roots):
    """Set "Used with Instanced Static Meshes" on every material under the given content paths (a running game
    can't add the flag, so such meshes would fall back to the grey default material). Returns how many changed."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    changed = 0
    for root in roots:
        for data in registry.get_assets_by_path(root, recursive=True):
            if str(data.asset_class_path.asset_name) != "Material":
                continue
            mat = unreal.load_asset(str(data.package_name))
            if mat.get_editor_property("used_with_instanced_static_meshes"):
                continue
            mat.set_editor_property("used_with_instanced_static_meshes", True)
            mel.recompile_material(mat)
            eal.save_loaded_asset(mat, only_if_is_dirty=False)
            changed += 1
            log("flagged for instancing: " + str(data.package_name))
    return changed
