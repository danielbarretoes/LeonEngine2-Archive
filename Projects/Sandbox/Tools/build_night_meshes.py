"""Build NightLevel static meshes in Blender and export FBX for LeonAssetTool.

Coordinate contract after FBX (axis_forward='-Z', axis_up='Y'):
engine is right-handed Y-up. Author in Blender Z-up, origin at ground center.
"""

from __future__ import annotations

import math
import os
from typing import Iterable, Optional, Sequence

import bpy
from mathutils import Vector

TEX_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Raw", "Textures")
)
MESH_DIR = os.path.normpath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Raw", "Meshes")
)


def _clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.lights, bpy.data.cameras):
        for item in list(block):
            block.remove(item)


def _load_image(filename: str):
    path = os.path.join(TEX_DIR, filename)
    if not os.path.isfile(path):
        return None
    img = bpy.data.images.load(path, check_existing=True)
    img.colorspace_settings.name = "sRGB" if "_Color" in filename else "Non-Color"
    return img


def _tex_node(nt, image, loc):
    node = nt.nodes.new("ShaderNodeTexImage")
    node.image = image
    node.location = loc
    return node


def make_pbr(
    name: str,
    color_file: Optional[str] = None,
    normal_file: Optional[str] = None,
    roughness_file: Optional[str] = None,
    ao_file: Optional[str] = None,
    metallic_file: Optional[str] = None,
    base_color: Sequence[float] = (1.0, 1.0, 1.0, 1.0),
    metallic: float = 0.0,
    roughness: float = 0.5,
    emission: Optional[Sequence[float]] = None,
    emission_strength: float = 0.0,
    alpha: float = 1.0,
    double_sided: bool = False,
):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.use_backface_culling = not double_sided
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    out = nt.nodes["Material Output"]
    bsdf.inputs["Base Color"].default_value = tuple(base_color)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if alpha < 1.0:
        bsdf.inputs["Alpha"].default_value = alpha
        mat.blend_method = "BLEND"

    x = -720
    color_img = _load_image(color_file) if color_file else None
    if color_img:
        tex = _tex_node(nt, color_img, (x, 280))
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])

    nrm_img = _load_image(normal_file) if normal_file else None
    if nrm_img:
        tex = _tex_node(nt, nrm_img, (x, 0))
        nrm = nt.nodes.new("ShaderNodeNormalMap")
        nrm.location = (x + 280, 0)
        nt.links.new(tex.outputs["Color"], nrm.inputs["Color"])
        nt.links.new(nrm.outputs["Normal"], bsdf.inputs["Normal"])

    rough_img = _load_image(roughness_file) if roughness_file else None
    if rough_img:
        tex = _tex_node(nt, rough_img, (x, -260))
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Roughness"])

    metal_img = _load_image(metallic_file) if metallic_file else None
    if metal_img:
        tex = _tex_node(nt, metal_img, (x, -480))
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Metallic"])

    ao_img = _load_image(ao_file) if ao_file else None
    if ao_img:
        tex = _tex_node(nt, ao_img, (x, 520))
        mix = nt.nodes.new("ShaderNodeMixRGB")
        mix.blend_type = "MULTIPLY"
        mix.inputs["Fac"].default_value = 1.0
        mix.location = (x + 280, 400)
        src = color_img
        if src:
            # Recreate a dedicated color sample for AO multiply if linked already.
            color_tex = None
            for node in nt.nodes:
                if node.type == "TEX_IMAGE" and node.image == color_img:
                    color_tex = node
                    break
            if color_tex:
                nt.links.new(color_tex.outputs["Color"], mix.inputs["Color1"])
                nt.links.new(tex.outputs["Color"], mix.inputs["Color2"])
                nt.links.new(mix.outputs["Color"], bsdf.inputs["Base Color"])

    if emission is not None:
        emit_key = "Emission Color" if "Emission Color" in bsdf.inputs else "Emission"
        bsdf.inputs[emit_key].default_value = (emission[0], emission[1], emission[2], 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength

    out.location = (300, 0)
    return mat


def assign_mat(obj, mat) -> None:
    if obj.data.materials:
        obj.data.materials[0] = mat
    else:
        obj.data.materials.append(mat)


def apply_and_uv(obj, island_margin: float = 0.02) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.quads_convert_to_tris(quad_method="BEAUTY", ngon_method="BEAUTY")
    bpy.ops.uv.smart_project(angle_limit=66.0, island_margin=island_margin)
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.select_set(False)


def add_cube(name, loc, scale, mat) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    assign_mat(obj, mat)
    return obj


def add_cylinder(name, loc, radius, depth, mat, vertices=16, rotation=(0.0, 0.0, 0.0)) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.rotation_euler = rotation
    assign_mat(obj, mat)
    return obj


def add_sphere(name, loc, radius, mat, segments=16) -> bpy.types.Object:
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=max(8, segments // 2), radius=radius, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    assign_mat(obj, mat)
    return obj


def add_cone(name, loc, radius1, depth, mat, vertices=12) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius1, radius2=0.0, depth=depth, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    assign_mat(obj, mat)
    return obj


def add_plane(name, loc, size, mat, subdiv=8) -> bpy.types.Object:
    bpy.ops.mesh.primitive_plane_add(size=1.0, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = (size[0], size[1], 1.0)
    assign_mat(obj, mat)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=subdiv)
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.unwrap(method="ANGLE_BASED", margin=0.001)
    bpy.ops.object.mode_set(mode="OBJECT")
    # World-XY tiling: 4 meters per texture repeat
    mesh = obj.data
    uv = mesh.uv_layers.active
    for loop in mesh.loops:
        co = mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (co.x / 4.0 + 0.5, co.y / 4.0 + 0.5)
    obj.select_set(False)
    return obj


def origin_to_ground(objects: Iterable[bpy.types.Object]) -> None:
    """Keep geometry, snap the shared origin to world (0,0,0) which is ground center."""
    for obj in objects:
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = list(objects)[0]
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR", center="MEDIAN")
    for obj in objects:
        obj.select_set(False)


def export_fbx(objects: Sequence[bpy.types.Object], filename: str) -> str:
    os.makedirs(MESH_DIR, exist_ok=True)
    path = os.path.join(MESH_DIR, filename)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        object_types={"MESH"},
        axis_forward="-Z",
        axis_up="Y",
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        bake_space_transform=True,
        mesh_smooth_type="FACE",
        use_tspace=True,
        path_mode="STRIP",
        embed_textures=False,
        add_leaf_bones=False,
    )
    bpy.ops.object.select_all(action="DESELECT")
    return path


def bounds_of(objects: Sequence[bpy.types.Object]) -> dict:
    mins = Vector((1e9, 1e9, 1e9))
    maxs = Vector((-1e9, -1e9, -1e9))
    for obj in objects:
        for corner in obj.bound_box:
            w = obj.matrix_world @ Vector(corner)
            mins.x, mins.y, mins.z = min(mins.x, w.x), min(mins.y, w.y), min(mins.z, w.z)
            maxs.x, maxs.y, maxs.z = max(maxs.x, w.x), max(maxs.y, w.y), max(maxs.z, w.z)
    return {
        "min": [round(mins.x, 3), round(mins.y, 3), round(mins.z, 3)],
        "max": [round(maxs.x, 3), round(maxs.y, 3), round(maxs.z, 3)],
    }


def hide_all(objects: Sequence[bpy.types.Object]) -> None:
    for obj in objects:
        obj.hide_set(True)
        obj.hide_render = True


def build_materials() -> dict:
    return {
        "street": make_pbr(
            "StreetAsphalt",
            "T_Street_Color.jpg",
            "T_Street_Normal.jpg",
            "T_Street_Roughness.jpg",
            metallic=0.02,
            roughness=0.72,
        ),
        "brick": make_pbr(
            "House_Walls",
            "T_Brick_Color.jpg",
            "T_Brick_Normal.jpg",
            "T_Brick_Roughness.jpg",
            "T_Brick_AO.jpg",
            roughness=0.78,
        ),
        "roof": make_pbr(
            "House_Roof",
            "T_Roof_Color.jpg",
            "T_Roof_Normal.jpg",
            "T_Roof_Roughness.jpg",
            roughness=0.55,
        ),
        "wood": make_pbr(
            "House_Wood",
            "T_WoodFloor_Color.jpg",
            "T_WoodFloor_Normal.jpg",
            "T_WoodFloor_Roughness.jpg",
            "T_WoodFloor_AO.jpg",
            "T_WoodFloor_Metallic.jpg",
            roughness=0.62,
        ),
        "window": make_pbr(
            "House_Windows",
            base_color=(0.35, 0.55, 0.72, 1.0),
            metallic=0.15,
            roughness=0.08,
            emission=(1.0, 0.82, 0.45),
            emission_strength=4.5,
        ),
        "stone": make_pbr(
            "House_Stone",
            "T_Street_Color.jpg",
            "T_Street_Normal.jpg",
            "T_Street_Roughness.jpg",
            roughness=0.85,
        ),
        "car_body": make_pbr(
            "Car_Body",
            base_color=(0.62, 0.05, 0.04, 1.0),
            metallic=0.45,
            roughness=0.28,
        ),
        "tire": make_pbr("Car_Tires", base_color=(0.04, 0.04, 0.04, 1.0), roughness=0.86),
        "car_glass": make_pbr(
            "Car_Windows",
            base_color=(0.08, 0.1, 0.12, 0.55),
            metallic=0.2,
            roughness=0.06,
            alpha=0.45,
        ),
        "headlight": make_pbr(
            "Car_Headlights",
            base_color=(1.0, 0.97, 0.9, 1.0),
            roughness=0.12,
            emission=(1.0, 0.97, 0.9),
            emission_strength=8.0,
        ),
        "taillight": make_pbr(
            "Car_Taillights",
            base_color=(0.7, 0.02, 0.02, 1.0),
            roughness=0.2,
            emission=(1.0, 0.04, 0.03),
            emission_strength=5.0,
        ),
        "chrome": make_pbr(
            "Car_Chrome",
            "T_MetalPlate_Color.jpg",
            "T_MetalPlate_Normal.jpg",
            "T_MetalPlate_Roughness.jpg",
            metallic=0.96,
            roughness=0.08,
        ),
        "bark": make_pbr(
            "Palm_Trunk",
            "T_Bark_Color.jpg",
            "T_Bark_Normal.jpg",
            "T_Bark_Roughness.jpg",
            "T_Bark_AO.jpg",
            roughness=0.82,
        ),
        "leaf": make_pbr(
            "Palm_Leaves",
            base_color=(0.12, 0.38, 0.14, 1.0),
            roughness=0.55,
            double_sided=True,
        ),
        "coconut": make_pbr("Palm_Coconuts", base_color=(0.28, 0.16, 0.07, 1.0), roughness=0.7),
        "lamp_metal": make_pbr(
            "StreetLamp_Metal",
            "T_MetalPlate_Color.jpg",
            "T_MetalPlate_Normal.jpg",
            "T_MetalPlate_Roughness.jpg",
            metallic_file="T_MetalPlate_Metallic.jpg",
            metallic=0.9,
            roughness=0.28,
        ),
        "lamp_glass": make_pbr(
            "StreetLamp_Glass",
            base_color=(1.0, 0.86, 0.55, 1.0),
            roughness=0.12,
            emission=(1.0, 0.78, 0.4),
            emission_strength=6.5,
        ),
    }


def build_ground(mats) -> list:
    ground = add_plane("Ground", (0.0, 0.0, 0.0), (42.0, 38.0), mats["street"], subdiv=10)
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    origin_to_ground([ground])
    return [ground]


def build_house(mats) -> list:
    objs = []
    # Body 6.4 x 4.8 x 3.4, front faces +Y (street)
    objs.append(add_cube("House_Body", (0.0, 0.0, 1.7), (3.2, 2.4, 1.7), mats["brick"]))
    roof_l = add_cube("House_RoofL", (0.0, -1.15, 3.85), (3.5, 1.55, 0.12), mats["roof"])
    roof_l.rotation_euler = (math.radians(38.0), 0.0, 0.0)
    roof_r = add_cube("House_RoofR", (0.0, 1.15, 3.85), (3.5, 1.55, 0.12), mats["roof"])
    roof_r.rotation_euler = (math.radians(-38.0), 0.0, 0.0)
    objs.extend([roof_l, roof_r])
    objs.append(add_cube("House_RoofRidge", (0.0, 0.0, 4.55), (3.55, 0.18, 0.1), mats["roof"]))
    objs.append(add_cube("House_Porch", (0.0, 2.85, 0.1), (1.7, 0.85, 0.1), mats["stone"]))
    objs.append(add_cylinder("House_PostL", (-1.35, 3.35, 1.05), 0.07, 1.9, mats["wood"], 10))
    objs.append(add_cylinder("House_PostR", (1.35, 3.35, 1.05), 0.07, 1.9, mats["wood"], 10))
    objs.append(add_cube("House_Lintel", (0.0, 3.35, 2.05), (1.55, 0.12, 0.08), mats["wood"]))
    objs.append(add_cube("House_Door", (0.0, 2.42, 1.15), (0.45, 0.06, 1.05), mats["wood"]))
    objs.append(add_cube("House_WindowL", (-1.55, 2.42, 2.15), (0.7, 0.05, 0.7), mats["window"]))
    objs.append(add_cube("House_WindowR", (1.55, 2.42, 2.15), (0.7, 0.05, 0.7), mats["window"]))
    objs.append(add_cube("House_WindowSide", (3.22, 0.4, 2.1), (0.05, 0.7, 0.7), mats["window"]))
    for obj in objs:
        apply_and_uv(obj)
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    origin_to_ground(objs)
    return objs


def build_car(mats) -> list:
    objs = []
    # Forward +Y. Wheels sit on Z=0.
    objs.append(add_cube("Car_Body", (0.0, 0.15, 0.58), (0.9, 1.85, 0.32), mats["car_body"]))
    objs.append(add_cube("Car_Cabin", (0.0, -0.15, 1.05), (0.78, 0.95, 0.28), mats["car_glass"]))
    objs.append(add_cube("Car_Hood", (0.0, 1.15, 0.72), (0.82, 0.55, 0.08), mats["car_body"]))
    objs.append(add_cube("Car_BumperF", (0.0, 1.95, 0.38), (0.88, 0.08, 0.12), mats["chrome"]))
    objs.append(add_cube("Car_BumperR", (0.0, -1.72, 0.38), (0.88, 0.08, 0.12), mats["chrome"]))
    wheel_pos = [(-0.78, 1.15, 0.28), (0.78, 1.15, 0.28), (-0.78, -1.15, 0.28), (0.78, -1.15, 0.28)]
    for i, p in enumerate(wheel_pos):
        objs.append(
            add_cylinder(
                f"Car_Wheel{i}",
                p,
                0.28,
                0.18,
                mats["tire"],
                vertices=18,
                rotation=(0.0, math.radians(90.0), 0.0),
            )
        )
    objs.append(add_cube("Car_HeadL", (-0.55, 1.92, 0.55), (0.14, 0.05, 0.1), mats["headlight"]))
    objs.append(add_cube("Car_HeadR", (0.55, 1.92, 0.55), (0.14, 0.05, 0.1), mats["headlight"]))
    objs.append(add_cube("Car_TailL", (-0.55, -1.78, 0.55), (0.12, 0.04, 0.08), mats["taillight"]))
    objs.append(add_cube("Car_TailR", (0.55, -1.78, 0.55), (0.12, 0.04, 0.08), mats["taillight"]))
    for obj in objs:
        apply_and_uv(obj)
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    origin_to_ground(objs)
    return objs


def build_palm(mats) -> list:
    objs = []
    objs.append(add_cylinder("Palm_Trunk", (0.0, 0.0, 2.6), 0.18, 5.2, mats["bark"], vertices=12))
    objs.append(add_sphere("Palm_Crown", (0.0, 0.0, 5.35), 0.32, mats["bark"], segments=12))
    # Fronds
    for i in range(8):
        ang = i * (math.pi * 2.0 / 8.0)
        loc = (math.cos(ang) * 0.85, math.sin(ang) * 0.85, 5.55)
        frond = add_cube(f"Palm_Frond{i}", loc, (0.12, 1.35, 0.03), mats["leaf"])
        frond.rotation_euler = (math.radians(18.0), 0.0, ang)
        objs.append(frond)
    objs.append(add_sphere("Palm_CoconutA", (0.18, 0.12, 5.15), 0.12, mats["coconut"], 10))
    objs.append(add_sphere("Palm_CoconutB", (-0.16, 0.1, 5.12), 0.11, mats["coconut"], 10))
    for obj in objs:
        apply_and_uv(obj)
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    origin_to_ground(objs)
    return objs


def build_streetlamp(mats) -> list:
    objs = []
    objs.append(add_cylinder("Lamp_Pole", (0.0, 0.0, 1.85), 0.07, 3.7, mats["lamp_metal"], 12))
    objs.append(add_cube("Lamp_Arm", (0.0, 0.45, 3.72), (0.06, 0.55, 0.06), mats["lamp_metal"]))
    objs.append(add_cube("Lamp_Head", (0.0, 0.85, 3.55), (0.22, 0.22, 0.12), mats["lamp_metal"]))
    glass = add_sphere("Lamp_Glass", (0.0, 0.85, 3.38), 0.16, mats["lamp_glass"], 12)
    objs.append(glass)
    for obj in objs:
        apply_and_uv(obj)
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    origin_to_ground(objs)
    return objs


def main() -> dict:
    _clear_scene()
    bpy.context.scene.cursor.location = (0.0, 0.0, 0.0)
    mats = build_materials()

    exports = {}
    ground = build_ground(mats)
    exports["Ground.fbx"] = {"path": export_fbx(ground, "Ground.fbx"), "bounds": bounds_of(ground)}
    hide_all(ground)

    house = build_house(mats)
    exports["House.fbx"] = {"path": export_fbx(house, "House.fbx"), "bounds": bounds_of(house)}
    hide_all(house)

    car = build_car(mats)
    exports["Car.fbx"] = {"path": export_fbx(car, "Car.fbx"), "bounds": bounds_of(car)}
    hide_all(car)

    palm = build_palm(mats)
    exports["PalmTree.fbx"] = {"path": export_fbx(palm, "PalmTree.fbx"), "bounds": bounds_of(palm)}
    hide_all(palm)

    lamp = build_streetlamp(mats)
    exports["StreetLamp.fbx"] = {"path": export_fbx(lamp, "StreetLamp.fbx"), "bounds": bounds_of(lamp)}

    glass = bpy.data.objects.get("Lamp_Glass")
    glass_loc = list(glass.matrix_world.translation) if glass else None
    return {"exports": exports, "lamp_glass": glass_loc, "tex_dir": TEX_DIR, "mesh_dir": MESH_DIR}


RESULT = main()
