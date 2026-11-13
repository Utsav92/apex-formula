"""Shared material-graph helpers for the Unreal Python scripts (same builder as ue_materials.py)."""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
ME = unreal
warnings = []


def log(tag, msg):
    unreal.log(f"{tag} {msg}")


def link(a, a_pin, b, b_pin):
    ok = mel.connect_material_expressions(a, a_pin, b, b_pin)
    if not ok:
        warnings.append(f"{type(a).__name__}.{a_pin or '*'} -> {type(b).__name__}.{b_pin or '*'}")
        log("MAT_WARN", f"connect failed: {type(a).__name__}.{a_pin or '*'} -> {type(b).__name__}.{b_pin or '*'}")
    return ok


def prop(node, pin, mat_prop):
    if not mel.connect_material_property(node, pin, mat_prop):
        warnings.append(f"property {mat_prop}")
        log("MAT_WARN", f"property connect failed: {mat_prop}")


class G:
    def __init__(self, path, name, instanced=False):
        full = f"{path}/{name}"
        if eal.does_asset_exist(full):
            eal.delete_asset(full)
        self.mat = tools.create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
        self.full, self.name, self.x, self.y = full, name, -900, 0
        if instanced:
            self.mat.set_editor_property("used_with_instanced_static_meshes", True)

    def n(self, cls, **props):
        e = mel.create_material_expression(self.mat, cls, self.x, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def s(self, name, default):
        return self.n(ME.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)

    def v(self, name, rgb):
        return self.n(ME.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0))

    def c(self, value):
        return self.n(ME.MaterialExpressionConstant, r=value)

    def tc(self, u=1.0, v=1.0):
        return self.n(ME.MaterialExpressionTextureCoordinate, u_tiling=u, v_tiling=v)

    def tex(self, path, uv=None, normal=False):
        e = self.n(ME.MaterialExpressionTextureSample)
        e.set_editor_property("texture", eal.load_asset(path))
        if normal:
            e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        if uv is not None:
            link(uv, "", e, "UVs")
        return e

    def mul(self, a, b, a_pin="", b_pin=""):
        m = self.n(ME.MaterialExpressionMultiply)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B")
        return m

    def add(self, a, b, a_pin="", b_pin=""):
        m = self.n(ME.MaterialExpressionAdd)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B")
        return m

    def sub(self, a, b, a_pin="", b_pin=""):
        m = self.n(ME.MaterialExpressionSubtract)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B")
        return m

    def lerp(self, a, b, alpha, a_pin="", b_pin="", alpha_pin=""):
        m = self.n(ME.MaterialExpressionLinearInterpolate)
        link(a, a_pin, m, "A"); link(b, b_pin, m, "B"); link(alpha, alpha_pin, m, "Alpha")
        return m

    def sat(self, a, pin=""):
        m = self.n(ME.MaterialExpressionSaturate)
        link(a, pin, m, "")
        return m

    def done(self):
        mel.recompile_material(self.mat)
        eal.save_asset(self.full)
        return self.mat
