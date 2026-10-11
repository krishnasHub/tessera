"""
Builds a game's Unreal assets from code, inside the editor's Python (UnrealEditor-Cmd -run=pythonscript). Tessera
ships no assets: each game runs its own script that calls these, and the assets land in the game's Content.

  materials   fresh(), scalar(), vector(), rgba(), const(), const3(), custom(), mul(), finish(): build a material
              graph node by node (regenerated from scratch every run, so the script is the source of truth)
  textures    import_textures(): a folder of PNGs as crisp pixel-art textures (nearest filter, no mips), with
              name prefixes that import smooth instead (e.g. high-res portraits)
  Tessera's   the materials Tessera's C++ drives, with the parameters it sets:
  materials     sprite()       <world>.assets.sprite      Tex, Cols, Rows, Col, Row, Flip, Tint, Flash, Emissive
                                                          (see_through=True: translucent, + Opacity)
                pixel_world()  <world>.assets.pixelWorld  Tex, Size, Tint
                night_shade()  <world>.assets.nightShade  Night, Hero, Light0..7
                glow()         <world>.assets.glow        Color, Intensity
                telegraph()    <world>.assets.telegraph   Color, Intensity, Opacity
                fresnel()      a rim-lit bubble           Color, Intensity, Opacity
                flash()        <world>.assets.flash       Color, Opacity
                flame()        looping pixel-art fire     Cols, Rows, Intensity, Speed, Seed, Tint (on a card)

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

def sprite(folder, name, default_texture=None, cols=4.0, rows=13.0, see_through=False):
    """Masked, lit, two-sided card playing one frame of a sprite sheet. Its normal is world-up, so a card facing
    the camera is lit like the ground under it. Params: Tex, Cols, Rows, Col, Row, Flip (mirror), Tint, Flash
    (hit flash), Emissive. see_through: translucent instead of masked, with an Opacity param (ghosts, spirits)."""
    m = fresh(folder, name)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT if see_through else unreal.BlendMode.BLEND_MASKED)
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
    if see_through:
        alpha = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 760)
        mel.connect_material_expressions(tex, "A", alpha, "A")
        mel.connect_material_expressions(scalar(m, "Opacity", 0.6, -400, 760), "", alpha, "B")
        mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    else:
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


FLAME_HLSL = """// Pixel-art fire: the card is a Cols x Rows grid of squares; the flame steps at 10 frames a second like a sprite.
float cx = floor(UV.x * Cols), cy = floor(UV.y * Rows);
float y = 1.0 - (cy + 0.5) / Rows;              // 0 at the base, 1 at the top
float x = ((cx + 0.5) / Cols - 0.5) * 2.0;
float tq = floor((Time * Speed + Seed * 17.0) * 10.0) / 10.0;
float yb = y / 0.62;                            // the body fills the lower part; embers rise above it
// Value-noise fbm rising through the flame (cheap: no textures).
float n = 0.0, amp = 0.5;
float2 p = float2(x * 2.6 + Seed, yb * 2.2 - tq * 2.2);
for (int i = 0; i < 3; i++)
{
    float2 ip = floor(p), fp = frac(p);
    fp = fp * fp * (3.0 - 2.0 * fp);
    float a = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);
    float b = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
    float c = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
    float d = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
    n += amp * lerp(lerp(a, b, fp.x), lerp(c, d, fp.x), fp.y);
    p = p * 2.03 + float2(3.1, -tq * 0.6);
    amp *= 0.5;
}
// A round-based teardrop torn into licking tongues by the noise higher up.
float xs = x - (n - 0.5) * 0.8 * yb;
float w = 0.85 * saturate(1.0 - yb) * sqrt(saturate(yb / 0.15));
float dd = 1.0 - abs(xs) / max(w, 0.001) - (n - 0.45) * (0.4 + yb * 1.5);
float f = yb < 1.05 ? saturate(dd * 1.25) : 0.0;
int idx = f < 0.1 ? -1 : (f < 0.28 ? 0 : (f < 0.55 ? 1 : (f < 0.8 ? 2 : (f < 0.94 ? 3 : 4))));
// Embers: a few squares near the middle rise from the flame, wobble a square sideways, and cool as they go.
for (int k = 0; k < 2 && idx < 0; k++)
    for (int dx = -1; dx <= 1; dx++)
    {
        float col = cx + dx;
        float hc = frac(sin((col + Seed * 3.3) * 127.1 + k * 7.1 * 311.7) * 43758.5453);
        if (hc >= 0.3 || abs((col + 0.5) / Cols - 0.5) >= 0.28) continue;
        float e = frac(tq * (0.35 + 0.3 * hc) + hc * 5.0);
        float row = floor((1.0 - (0.35 + e * 0.65)) * Rows);
        float off = round(sin(tq * 3.0 + hc * 20.0) * 0.8);
        if (row == cy && col + off == cx) idx = e < 0.35 ? 2 : (e < 0.7 ? 1 : 0);
    }
// Dark red -> red -> orange -> amber -> gold, all saturated: a pale shade bleaches to grey on screen.
float3 pal[5] = { float3(0.4, 0.04, 0.01), float3(0.75, 0.12, 0.02), float3(1.0, 0.3, 0.03), float3(1.0, 0.5, 0.06), float3(1.0, 0.68, 0.12) };
return idx < 0 ? float3(0, 0, 0) : pal[idx];"""


def flame(folder, name, intensity=1.0, speed=1.0):
    """Translucent unlit, two-sided: a looping pixel-art fire on a card (UV v=0 at the top): a flame of chunky squares
    in five shades plus embers rising off it, all computed in the shader (no textures, no particles); its brightness
    ignores the camera's exposure. Params: Cols, Rows (the square grid: match the sprites' pixel size), Intensity,
    Speed, Seed (give each fire its own so they don't burn in step), Tint."""
    m = fresh(folder, name, EFFECT_USAGE)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tm = mel.create_material_expression(m, unreal.MaterialExpressionTime, -1400, 100)
    fl = custom(m, FLAME_HLSL, ["UV", "Time", "Speed", "Seed", "Cols", "Rows"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 0)
    mel.connect_material_expressions(uv, "", fl, "UV")
    mel.connect_material_expressions(tm, "", fl, "Time")
    mel.connect_material_expressions(scalar(m, "Speed", speed, -1400, 200), "", fl, "Speed")
    mel.connect_material_expressions(scalar(m, "Seed", 0.0, -1400, 300), "", fl, "Seed")
    mel.connect_material_expressions(scalar(m, "Cols", 24.0, -1400, 400), "", fl, "Cols")
    mel.connect_material_expressions(scalar(m, "Rows", 36.0, -1400, 500), "", fl, "Rows")
    lit = mul(m, fl, mul(m, vector(m, "Tint", (1, 1, 1, 1), -900, 300), scalar(m, "Intensity", intensity, -900, 450), -600, 300), -300, 0)
    # Cancel the camera's exposure, so a flame looks the same in a dark night, a bright day or a dim cave (an
    # emissive otherwise blows out to white where the eye adapts up, and fades where it adapts down).
    eye = mel.create_material_expression(m, unreal.MaterialExpressionEyeAdaptationInverse, -150, 0)
    mel.connect_material_expressions(lit, "", eye, "LightValueInput")
    mel.connect_material_property(eye, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Solid squares: every lit square is opaque (each palette shade has red > 0), the rest see-through.
    red = mel.create_material_expression(m, unreal.MaterialExpressionComponentMask, -600, 600)
    red.set_editor_property("r", True)
    mel.connect_material_expressions(fl, "", red, "")
    solid = mel.create_material_expression(m, unreal.MaterialExpressionCeil, -400, 600)
    mel.connect_material_expressions(red, "", solid, "")
    mel.connect_material_property(solid, "", unreal.MaterialProperty.MP_OPACITY)
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
