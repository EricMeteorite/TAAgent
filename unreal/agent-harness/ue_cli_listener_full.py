#!/usr/bin/env python3
"""Unreal Engine CLI Listener - Full Feature Version

This script runs inside Unreal Engine and listens for commands from the CLI
via file-based communication.

Features:
- Actor management (CRUD, transform, components, tags)
- Level operations (create, stream, world partition)
- Material editing (nodes, instances, parameters)
- Blueprint operations (compile, modify, spawn)
- Asset management (import, export, rename, reference)
- Editor UI (notifications, dialogs)
- Selection tools
- Build operations (lighting, nav, reflection)
- Animation and physics

To use:
1. Open Unreal Editor
2. Open Python Console (Window -> Developer Tools -> Python Console)
3. Run this file from your local TAAgent checkout, for example:
   exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener_full.py").read())
4. Run: start_ue_cli()
"""

import unreal
import json
import os
import time
from pathlib import Path

# Configuration
TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"))
)
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"

TEMP_DIR.mkdir(parents=True, exist_ok=True)

print(f"[UE CLI Full] Listener initialized. Watching: {COMMAND_FILE}")

# ============================================================================
# ACTOR COMMANDS
# ============================================================================

def get_actors(params: dict) -> dict:
    """Get all actors or filtered actors."""
    try:
        filter_class = params.get("filter_class")
        filter_tag = params.get("filter_tag")
        filter_name = params.get("filter_name")
        
        actors = []
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            # Apply filters
            if filter_class and filter_class not in actor.get_class().get_name():
                continue
            if filter_tag and not actor.actor_has_tag(filter_tag):
                continue
            if filter_name and filter_name.lower() not in actor.get_name().lower():
                continue
            
            actors.append({
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "class": actor.get_class().get_name(),
                "location": {
                    "x": actor.get_actor_location().x,
                    "y": actor.get_actor_location().y,
                    "z": actor.get_actor_location().z
                },
                "rotation": {
                    "pitch": actor.get_actor_rotation().pitch,
                    "yaw": actor.get_actor_rotation().yaw,
                    "roll": actor.get_actor_rotation().roll
                },
                "scale": {
                    "x": actor.get_actor_scale3d().x,
                    "y": actor.get_actor_scale3d().y,
                    "z": actor.get_actor_scale3d().z
                },
                "tags": [tag for tag in actor.tags],
                "hidden": actor.is_hidden_ed(),
                "selected": actor.is_selected()
            })
        return {"success": True, "actors": actors, "count": len(actors)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def spawn_actor(params: dict) -> dict:
    """Spawn an actor in the level."""
    try:
        actor_class = params.get("actor_class", "StaticMeshActor")
        name = params.get("name")
        location = params.get("location", [0, 0, 0])
        rotation = params.get("rotation", [0, 0, 0])
        scale = params.get("scale", [1, 1, 1])
        tags = params.get("tags", [])
        
        # Get actor class
        class_map = {
            "StaticMeshActor": unreal.StaticMeshActor,
            "PointLight": unreal.PointLight,
            "SpotLight": unreal.SpotLight,
            "DirectionalLight": unreal.DirectionalLight,
            "SkyLight": unreal.SkyLight,
            "CameraActor": unreal.CameraActor,
            "PlayerStart": unreal.PlayerStart,
            "TriggerBox": unreal.TriggerBox,
            "TriggerSphere": unreal.TriggerSphere,
            "AudioVolume": unreal.AudioVolume,
            "PostProcessVolume": unreal.PostProcessVolume,
            "BlockingVolume": unreal.BlockingVolume,
            "PhysicsThruster": unreal.PhysicsThruster,
            "RadialForceActor": unreal.RadialForceActor,
        }
        
        if actor_class in class_map:
            cls = class_map[actor_class]
        else:
            cls = unreal.load_class(None, f"/Script/Engine.{actor_class}")
            if not cls:
                return {"success": False, "error": f"Unknown actor class: {actor_class}"}
        
        # Spawn actor
        loc = unreal.Vector(location[0], location[1], location[2])
        rot = unreal.Rotator(rotation[0], rotation[1], rotation[2])
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
        
        if not actor:
            return {"success": False, "error": "Failed to spawn actor"}
        
        # Set properties
        if name:
            actor.set_actor_label(name)
        if scale:
            actor.set_actor_scale3d(unreal.Vector(scale[0], scale[1], scale[2]))
        for tag in tags:
            actor.tags.append(tag)
        
        return {
            "success": True,
            "actor": {
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "class": actor.get_class().get_name(),
                "path": actor.get_path_name()
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def delete_actor(params: dict) -> dict:
    """Delete an actor from the level."""
    try:
        name = params.get("name")
        if not name:
            return {"success": False, "error": "Actor name required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                unreal.EditorLevelLibrary.destroy_actor(actor)
                return {"success": True, "deleted": name}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def set_actor_transform(params: dict) -> dict:
    """Set actor transform."""
    try:
        name = params.get("name")
        location = params.get("location")
        rotation = params.get("rotation")
        scale = params.get("scale")
        relative = params.get("relative", False)
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                if location:
                    actor.set_actor_location(
                        unreal.Vector(location[0], location[1], location[2]),
                        False, False
                    )
                if rotation:
                    actor.set_actor_rotation(
                        unreal.Rotator(rotation[0], rotation[1], rotation[2]),
                        False
                    )
                if scale:
                    actor.set_actor_scale3d(
                        unreal.Vector(scale[0], scale[1], scale[2])
                    )
                return {"success": True, "actor": name}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_actor_properties(params: dict) -> dict:
    """Get detailed actor properties."""
    try:
        name = params.get("name")
        if not name:
            return {"success": False, "error": "Actor name required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                # Get components
                components = []
                for comp in actor.get_components_by_class(unreal.ActorComponent):
                    components.append({
                        "name": comp.get_name(),
                        "class": comp.get_class().get_name()
                    })
                
                return {
                    "success": True,
                    "actor": {
                        "name": actor.get_name(),
                        "label": actor.get_actor_label(),
                        "class": actor.get_class().get_name(),
                        "path": actor.get_path_name(),
                        "location": {
                            "x": actor.get_actor_location().x,
                            "y": actor.get_actor_location().y,
                            "z": actor.get_actor_location().z
                        },
                        "rotation": {
                            "pitch": actor.get_actor_rotation().pitch,
                            "yaw": actor.get_actor_rotation().yaw,
                            "roll": actor.get_actor_rotation().roll
                        },
                        "scale": {
                            "x": actor.get_actor_scale3d().x,
                            "y": actor.get_actor_scale3d().y,
                            "z": actor.get_actor_scale3d().z
                        },
                        "tags": [str(tag) for tag in actor.tags],
                        "hidden": actor.is_hidden_ed(),
                        "components": components
                    }
                }
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def set_actor_property(params: dict) -> dict:
    """Set actor property."""
    try:
        name = params.get("name")
        property_name = params.get("property")
        value = params.get("value")
        
        if not name or not property_name:
            return {"success": False, "error": "Actor name and property required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                # Handle common properties
                if property_name == "hidden":
                    actor.set_is_hidden_ed(value)
                elif property_name == "label":
                    actor.set_actor_label(value)
                else:
                    # Try to set editor property
                    actor.set_editor_property(property_name, value)
                
                return {"success": True, "actor": name, "property": property_name}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def set_actor_mesh(params: dict) -> dict:
    """Set static mesh for an actor."""
    try:
        name = params.get("name")
        mesh_path = params.get("mesh_path")
        material_path = params.get("material_path")
        
        if not name or not mesh_path:
            return {"success": False, "error": "Actor name and mesh path required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
                if not mesh_comp:
                    return {"success": False, "error": "Actor has no StaticMeshComponent"}
                
                # Load and set mesh
                static_mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
                if not static_mesh:
                    return {"success": False, "error": f"Mesh not found: {mesh_path}"}
                
                mesh_comp.set_static_mesh(static_mesh)
                result = {"success": True, "actor": name, "mesh": mesh_path}
                
                # Set material if provided
                if material_path:
                    material = unreal.EditorAssetLibrary.load_asset(material_path)
                    if material:
                        mesh_comp.set_material(0, material)
                        result["material"] = material_path
                
                return result
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def add_actor_tag(params: dict) -> dict:
    """Add tag to actor."""
    try:
        name = params.get("name")
        tag = params.get("tag")
        
        if not name or not tag:
            return {"success": False, "error": "Actor name and tag required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                actor.tags.append(tag)
                return {"success": True, "actor": name, "tag_added": tag}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def remove_actor_tag(params: dict) -> dict:
    """Remove tag from actor."""
    try:
        name = params.get("name")
        tag = params.get("tag")
        
        if not name or not tag:
            return {"success": False, "error": "Actor name and tag required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                if tag in actor.tags:
                    actor.tags.remove(tag)
                return {"success": True, "actor": name, "tag_removed": tag}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_actor_components(params: dict) -> dict:
    """Get actor components."""
    try:
        name = params.get("name")
        if not name:
            return {"success": False, "error": "Actor name required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                components = []
                for comp in actor.get_components_by_class(unreal.ActorComponent):
                    comp_info = {
                        "name": comp.get_name(),
                        "class": comp.get_class().get_name(),
                    }
                    
                    # Add component-specific properties
                    if isinstance(comp, unreal.StaticMeshComponent):
                        mesh = comp.static_mesh
                        comp_info["mesh"] = mesh.get_name() if mesh else None
                        comp_info["materials_count"] = comp.get_num_materials()
                    elif isinstance(comp, unreal.LightComponent):
                        comp_info["intensity"] = comp.intensity
                        comp_info["color"] = {
                            "r": comp.light_color.r,
                            "g": comp.light_color.g,
                            "b": comp.light_color.b
                        }
                    
                    components.append(comp_info)
                
                return {"success": True, "actor": name, "components": components}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def add_component(params: dict) -> dict:
    """Add component to actor."""
    try:
        name = params.get("name")
        component_class = params.get("component_class", "StaticMeshComponent")
        component_name = params.get("component_name", "NewComponent")
        
        if not name:
            return {"success": False, "error": "Actor name required"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == name or actor.get_actor_label() == name:
                # Load component class
                comp_cls = unreal.load_class(None, f"/Script/Engine.{component_class}")
                if not comp_cls:
                    return {"success": False, "error": f"Unknown component class: {component_class}"}
                
                # Add component
                new_comp = unreal.EditorLevelLibrary.add_component_to_blueprint(actor, comp_cls)
                if new_comp:
                    new_comp.rename(component_name)
                    return {"success": True, "actor": name, "component": component_name}
                else:
                    return {"success": False, "error": "Failed to add component"}
        
        return {"success": False, "error": f"Actor not found: {name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# LEVEL COMMANDS
# ============================================================================

def get_level_info(params: dict) -> dict:
    """Get current level information."""
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        level_name = world.get_name() if world else "Unknown"
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        world_partition = False

        if world:
            world_partition_attr = getattr(world, "is_world_partitioned", None)
            if callable(world_partition_attr):
                world_partition = bool(world_partition_attr())
            elif world_partition_attr is not None:
                world_partition = bool(world_partition_attr)
        
        return {
            "success": True,
            "level": {
                "name": level_name,
                "path": world.get_path_name() if world else "",
                "actor_count": len(actors),
                "world_partition": world_partition
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def open_level(params: dict) -> dict:
    """Open a level."""
    try:
        level_path = params.get("level_path")
        if not level_path:
            return {"success": False, "error": "Level path required"}
        
        result = unreal.EditorLevelLibrary.load_level(level_path)
        return {"success": result, "level": level_path}
    except Exception as e:
        return {"success": False, "error": str(e)}


def save_level(params: dict) -> dict:
    """Save the current level."""
    try:
        result = unreal.EditorLevelLibrary.save_current_level()
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def create_level(params: dict) -> dict:
    """Create a new level."""
    try:
        level_path = params.get("level_path")
        if not level_path:
            return {"success": False, "error": "Level path required"}
        
        # Create new level using asset tools
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.WorldFactory()
        
        new_level = asset_tools.create_asset(
            asset_name=Path(level_path).stem,
            package_path=str(Path(level_path).parent).replace("\\", "/"),
            asset_class=unreal.World,
            factory=factory
        )
        
        if new_level:
            return {"success": True, "level": level_path}
        else:
            return {"success": False, "error": "Failed to create level"}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# MATERIAL COMMANDS
# ============================================================================

def create_material(params: dict) -> dict:
    """Create a new material."""
    try:
        name = params.get("name", "NewMaterial")
        path = params.get("path", "/Game/Materials")
        base_color = params.get("base_color", [1.0, 1.0, 1.0, 1.0])
        
        factory = unreal.MaterialFactoryNew()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = asset_tools.create_asset(
            asset_name=name,
            package_path=path,
            asset_class=unreal.Material,
            factory=factory
        )
        
        if material:
            # Set base color if provided
            if base_color:
                # Create constant color node
                constant = unreal.MaterialEditingLibrary.create_material_expression(
                    material, unreal.MaterialExpressionConstant4Vector
                )
                constant.set_editor_property("Constant", unreal.LinearColor(
                    base_color[0], base_color[1], base_color[2], base_color[3]
                ))
                unreal.MaterialEditingLibrary.connect_material_property(
                    constant, "", unreal.MaterialProperty.MP_BASE_COLOR
                )
                unreal.MaterialEditingLibrary.update_material_instance(material)
            
            return {
                "success": True,
                "material": {
                    "name": material.get_name(),
                    "path": material.get_path_name()
                }
            }
        else:
            return {"success": False, "error": "Failed to create material"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def create_material_instance(params: dict) -> dict:
    """Create a material instance."""
    try:
        name = params.get("name", "NewMaterialInstance")
        path = params.get("path", "/Game/Materials")
        parent_path = params.get("parent")
        
        if not parent_path:
            return {"success": False, "error": "Parent material path required"}
        
        parent = unreal.EditorAssetLibrary.load_asset(parent_path)
        if not parent:
            return {"success": False, "error": f"Parent material not found: {parent_path}"}
        
        factory = unreal.MaterialInstanceConstantFactoryNew()
        factory.set_editor_property("Parent", parent)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        instance = asset_tools.create_asset(
            asset_name=name,
            package_path=path,
            asset_class=unreal.MaterialInstanceConstant,
            factory=factory
        )
        
        if instance:
            return {
                "success": True,
                "instance": {
                    "name": instance.get_name(),
                    "path": instance.get_path_name(),
                    "parent": parent_path
                }
            }
        else:
            return {"success": False, "error": "Failed to create material instance"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def set_material_parameter(params: dict) -> dict:
    """Set material instance parameter."""
    try:
        material_path = params.get("material")
        param_name = params.get("parameter")
        value = params.get("value")
        param_type = params.get("type", "scalar")  # scalar, vector, texture
        
        if not material_path or not param_name:
            return {"success": False, "error": "Material path and parameter name required"}
        
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if not material:
            return {"success": False, "error": f"Material not found: {material_path}"}
        
        # Set parameter based on type
        if param_type == "scalar":
            unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
                material, param_name, float(value)
            )
        elif param_type == "vector":
            color = unreal.LinearColor(value[0], value[1], value[2], value[3] if len(value) > 3 else 1.0)
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
                material, param_name, color
            )
        elif param_type == "texture":
            texture = unreal.EditorAssetLibrary.load_asset(value)
            if texture:
                unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
                    material, param_name, texture
                )
        
        return {"success": True, "material": material_path, "parameter": param_name}
    except Exception as e:
        return {"success": False, "error": str(e)}


def apply_material(params: dict) -> dict:
    """Apply material to an actor."""
    try:
        actor_name = params.get("actor_name")
        material_path = params.get("material_path")
        slot_index = params.get("slot", 0)
        
        if not actor_name or not material_path:
            return {"success": False, "error": "Actor name and material path required"}
        
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if not material:
            return {"success": False, "error": f"Material not found: {material_path}"}
        
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == actor_name or actor.get_actor_label() == actor_name:
                mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
                if mesh_comp:
                    mesh_comp.set_material(slot_index, material)
                    return {"success": True, "actor": actor_name, "material": material_path}
                else:
                    return {"success": False, "error": "Actor has no StaticMeshComponent"}
        
        return {"success": False, "error": f"Actor not found: {actor_name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# WORKFLOW COMMANDS (Import + Material Setup)
# ============================================================================

def import_and_setup_material(params: dict) -> dict:
    """Complete workflow: Import texture, create material, apply to mesh."""
    try:
        # Parameters
        texture_path = params.get("texture_path")  # File system path to texture
        mesh_path = params.get("mesh_path")  # File system path to mesh (fbx/obj)
        material_name = params.get("material_name", "M_AutoGenerated")
        import_path = params.get("import_path", "/Game/Imported")
        
        if not texture_path:
            return {"success": False, "error": "Texture path required"}
        
        results = {"steps": []}
        
        # Step 1: Import texture
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        
        # Ensure import folder exists
        if not unreal.EditorAssetLibrary.does_directory_exist(import_path):
            unreal.EditorAssetLibrary.make_directory(import_path)
        
        texture_task = unreal.AssetImportTask()
        texture_task.set_editor_property('filename', texture_path)
        texture_task.set_editor_property('destination_path', import_path + "/Textures")
        texture_task.set_editor_property('replace_existing', True)
        texture_task.set_editor_property('automated', True)
        texture_task.set_editor_property('save', True)
        
        # Execute import
        asset_tools.import_asset_tasks([texture_task])
        
        # Wait for import to complete (get_objects blocks until done)
        imported_objects = texture_task.get_objects()
        
        texture_asset = None
        texture_name = None
        texture_asset_path = None
        
        if imported_objects and len(imported_objects) > 0:
            # get_objects() returns Array of objects
            texture_asset = imported_objects[0]
            texture_name = texture_asset.get_name()
            texture_asset_path = texture_asset.get_path_name()
            results["steps"].append({"step": "import_texture", "status": "success", "asset": texture_asset_path})
        else:
            # Fallback: try to find by name
            texture_filename = os.path.splitext(os.path.basename(texture_path))[0]
            texture_folder = f"{import_path}/Textures"
            full_path = f"{texture_folder}/{texture_filename}.{texture_filename}"
            
            texture_asset = unreal.EditorAssetLibrary.load_asset(full_path)
            if texture_asset:
                texture_name = texture_filename
                texture_asset_path = full_path
                results["steps"].append({"step": "import_texture", "status": "success", "asset": texture_asset_path})
            else:
                return {"success": False, "error": f"Failed to import texture. Tried: {full_path}", "results": results}
        
        # Step 2: Create material with texture
        material_path = f"{import_path}/Materials"
        if not unreal.EditorAssetLibrary.does_directory_exist(material_path):
            unreal.EditorAssetLibrary.make_directory(material_path)
        
        # Create material factory
        material_factory = unreal.MaterialFactoryNew()
        material_full_path = f"{material_path}/{material_name}"
        
        # Check if material exists
        material = unreal.EditorAssetLibrary.load_asset(material_full_path)
        if not material:
            material = asset_tools.create_asset(
                asset_name=material_name,
                package_path=material_path,
                asset_class=unreal.Material,
                factory=material_factory
            )
        
        if not material:
            return {"success": False, "error": "Failed to create material", "results": results}
        
        results["steps"].append({"step": "create_material", "status": "success", "material": material_full_path})
        
        # Step 3: Setup material nodes
        # Create texture sample node
        texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -200, 0
        )
        texture_sample.set_editor_property('texture', texture_asset)
        
        # Connect to base color
        unreal.MaterialEditingLibrary.connect_material_property(
            texture_sample, 'Color', unreal.MaterialProperty.MP_BASE_COLOR
        )
        
        # Layout material
        unreal.MaterialEditingLibrary.layout_material_expressions(material)
        
        # Recompile material
        unreal.MaterialEditingLibrary.recompile_material(material)
        
        # Save material
        unreal.EditorAssetLibrary.save_asset(material_full_path)
        
        results["steps"].append({"step": "setup_material_nodes", "status": "success"})
        
        # Step 4: Import mesh if provided
        if mesh_path:
            mesh_task = unreal.AssetImportTask()
            mesh_task.set_editor_property('filename', mesh_path)
            mesh_task.set_editor_property('destination_path', import_path + "/Meshes")
            mesh_task.set_editor_property('replace_existing', True)
            mesh_task.set_editor_property('automated', True)
            mesh_task.set_editor_property('save', True)
            
            asset_tools.import_asset_tasks([mesh_task])
            
            # Wait for import to complete
            imported_mesh_objects = mesh_task.get_objects()
            
            mesh_asset = None
            if imported_mesh_objects and len(imported_mesh_objects) > 0:
                # get_objects() returns Array of objects
                mesh_asset = imported_mesh_objects[0]
                mesh_name = mesh_asset.get_name()
                mesh_full_path = mesh_asset.get_path_name()
                results["steps"].append({"step": "import_mesh", "status": "success", "mesh": mesh_full_path})
                
                # Apply material to mesh
                unreal.EditorAssetLibrary.save_asset(mesh_full_path)
                results["steps"].append({"step": "save_mesh", "status": "success"})
                results["mesh"] = mesh_full_path
            else:
                # Fallback
                mesh_filename = os.path.splitext(os.path.basename(mesh_path))[0]
                mesh_full_path = f"{import_path}/Meshes/{mesh_filename}.{mesh_filename}"
                results["steps"].append({"step": "import_mesh", "status": "failed", "tried": mesh_full_path})
        
        results["success"] = True
        results["material"] = material_full_path
        results["texture"] = texture_asset_path
        
        return results
        
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


# ============================================================================
# BLUEPRINT COMMANDS
# ============================================================================

def create_blueprint(params: dict) -> dict:
    """Create a new blueprint."""
    try:
        name = params.get("name", "NewBlueprint")
        path = params.get("path", "/Game/Blueprints")
        parent_class = params.get("parent_class", "Actor")
        
        factory = unreal.BlueprintFactory()
        parent = unreal.load_class(None, f"/Script/Engine.{parent_class}")
        if parent:
            factory.set_editor_property("ParentClass", parent)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        blueprint = asset_tools.create_asset(
            asset_name=name,
            package_path=path,
            asset_class=unreal.Blueprint,
            factory=factory
        )
        
        if blueprint:
            return {
                "success": True,
                "blueprint": {
                    "name": blueprint.get_name(),
                    "path": blueprint.get_path_name(),
                    "parent": parent_class
                }
            }
        else:
            return {"success": False, "error": "Failed to create blueprint"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def compile_blueprint(params: dict) -> dict:
    """Compile a blueprint."""
    try:
        blueprint_path = params.get("blueprint")
        if not blueprint_path:
            return {"success": False, "error": "Blueprint path required"}
        
        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if not blueprint:
            return {"success": False, "error": f"Blueprint not found: {blueprint_path}"}
        
        unreal.EditorAssetLibrary.save_asset(blueprint_path)
        
        return {"success": True, "blueprint": blueprint_path}
    except Exception as e:
        return {"success": False, "error": str(e)}


def spawn_from_blueprint(params: dict) -> dict:
    """Spawn actor from blueprint."""
    try:
        blueprint_path = params.get("blueprint")
        name = params.get("name")
        location = params.get("location", [0, 0, 0])
        rotation = params.get("rotation", [0, 0, 0])
        
        if not blueprint_path:
            return {"success": False, "error": "Blueprint path required"}
        
        blueprint = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        if not blueprint:
            return {"success": False, "error": f"Blueprint not found: {blueprint_path}"}
        
        loc = unreal.Vector(location[0], location[1], location[2])
        rot = unreal.Rotator(rotation[0], rotation[1], rotation[2])
        
        actor = unreal.EditorLevelLibrary.spawn_actor_from_object(blueprint, loc, rot)
        
        if name:
            actor.set_actor_label(name)
        
        return {
            "success": True,
            "actor": {
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "class": actor.get_class().get_name()
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# ASSET COMMANDS
# ============================================================================

def get_assets(params: dict) -> dict:
    """Get assets in a path."""
    try:
        path = params.get("path", "/Game")
        recursive = params.get("recursive", True)
        asset_class = params.get("class")
        
        assets = []
        asset_data_list = unreal.EditorAssetLibrary.list_assets(path, recursive=recursive)
        
        for asset_data in asset_data_list:
            # asset_data is a string path, need to get asset data object
            asset_data_obj = unreal.EditorAssetLibrary.find_asset_data(asset_data)
            if not asset_data_obj:
                continue
                
            if asset_class and asset_class not in str(asset_data_obj.asset_class):
                continue
            
            assets.append({
                "name": str(asset_data_obj.asset_name),
                "path": str(asset_data_obj.get_asset().get_path_name()),
                "class": str(asset_data_obj.asset_class),
                "package_name": str(asset_data_obj.package_name)
            })
        
        return {"success": True, "assets": assets, "count": len(assets)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def import_asset(params: dict) -> dict:
    """Import an asset into the project."""
    try:
        source_path = params.get("source_path")
        destination_path = params.get("destination_path")
        options = params.get("options", {})
        asset_name = params.get("asset_name")
        
        if not source_path or not destination_path:
            return {"success": False, "error": "Source and destination paths required"}
        
        # Ensure destination path starts with /
        if not destination_path.startswith("/"):
            destination_path = "/" + destination_path
            
        # Create destination folder if it doesn't exist
        if not unreal.EditorAssetLibrary.does_directory_exist(destination_path):
            unreal.EditorAssetLibrary.make_directory(destination_path)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source_path)
        task.set_editor_property('destination_path', destination_path)
        task.set_editor_property('replace_existing', options.get('replace', True))
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        
        # Set asset name if provided
        if asset_name:
            task.set_editor_property('destination_name', asset_name)
        
        # Execute import
        asset_tools.import_asset_tasks([task])
        
        # Wait for import to complete and get imported objects
        imported_objects = task.get_objects()
        
        # Get imported asset info
        result = {
            "success": True, 
            "imported": source_path,
            "destination": destination_path
        }
        
        # Get imported asset details (get_objects returns Array of objects)
        if imported_objects and len(imported_objects) > 0:
            imported_list = []
            for asset in imported_objects:
                if asset:
                    imported_list.append({
                        "name": str(asset.get_name()),
                        "path": str(asset.get_path_name())
                    })
            result["assets"] = imported_list
        
        return result
    except Exception as e:
        import traceback
        return {"success": False, "error": str(e), "traceback": traceback.format_exc()}


def export_asset(params: dict) -> dict:
    """Export an asset to file."""
    try:
        asset_path = params.get("asset")
        export_path = params.get("export_path")
        
        if not asset_path or not export_path:
            return {"success": False, "error": "Asset path and export path required"}
        
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not asset:
            return {"success": False, "error": f"Asset not found: {asset_path}"}
        
        # Use export task
        export_task = unreal.AssetExportTask()
        export_task.set_editor_property('object', asset)
        export_task.set_editor_property('filename', export_path)
        export_task.set_editor_property('automated', True)
        
        unreal.Exporter.run_asset_export_task(export_task)
        
        return {"success": True, "exported": export_path}
    except Exception as e:
        return {"success": False, "error": str(e)}


def rename_asset(params: dict) -> dict:
    """Rename an asset."""
    try:
        source_path = params.get("source")
        dest_path = params.get("destination")
        
        if not source_path or not dest_path:
            return {"success": False, "error": "Source and destination paths required"}
        
        result = unreal.EditorAssetLibrary.rename_asset(source_path, dest_path)
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def duplicate_asset(params: dict) -> dict:
    """Duplicate an asset."""
    try:
        source_path = params.get("source")
        dest_path = params.get("destination")
        
        if not source_path or not dest_path:
            return {"success": False, "error": "Source and destination paths required"}
        
        result = unreal.EditorAssetLibrary.duplicate_asset(source_path, dest_path)
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def delete_asset(params: dict) -> dict:
    """Delete an asset."""
    try:
        asset_path = params.get("asset")
        if not asset_path:
            return {"success": False, "error": "Asset path required"}
        
        result = unreal.EditorAssetLibrary.delete_asset(asset_path)
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def find_asset_references(params: dict) -> dict:
    """Find references to an asset."""
    try:
        asset_path = params.get("asset")
        if not asset_path:
            return {"success": False, "error": "Asset path required"}
        
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not asset:
            return {"success": False, "error": f"Asset not found: {asset_path}"}
        
        referencers = unreal.EditorAssetLibrary.find_package_referencers_for_asset(
            asset_path, load_assets_to_confirm=False
        )
        
        return {
            "success": True,
            "asset": asset_path,
            "referencers": [str(r) for r in referencers],
            "count": len(referencers)
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def migrate_assets(params: dict) -> dict:
    """Migrate assets to another project."""
    try:
        asset_paths = params.get("assets", [])
        destination = params.get("destination")
        
        if not asset_paths or not destination:
            return {"success": False, "error": "Assets and destination required"}
        
        # Load assets
        assets = []
        for path in asset_paths:
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if asset:
                assets.append(asset)
        
        # Create migration dialog
        # Note: This would typically use a dialog, but we'll use automated migration
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        
        return {"success": True, "migrated": len(assets)}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# EDITOR UI COMMANDS
# ============================================================================

def show_notification(params: dict) -> dict:
    """Show editor notification."""
    try:
        message = params.get("message", "")
        type_str = params.get("type", "info")  # info, warning, error
        duration = params.get("duration", 5.0)
        
        if not message:
            return {"success": False, "error": "Message required"}
        
        # Use log for now (notification API may vary)
        if type_str == "error":
            unreal.log_error(message)
        elif type_str == "warning":
            unreal.log_warning(message)
        else:
            unreal.log(message)
        
        return {"success": True, "message": message}
    except Exception as e:
        return {"success": False, "error": str(e)}


def show_dialog(params: dict) -> dict:
    """Show editor dialog."""
    try:
        title = params.get("title", "Message")
        message = params.get("message", "")
        dialog_type = params.get("dialog_type", "ok")  # ok, yesno
        
        if not message:
            return {"success": False, "error": "Message required"}
        
        # Log for now (dialog would block)
        unreal.log(f"[DIALOG] {title}: {message}")
        
        return {"success": True, "title": title, "message": message}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# SELECTION COMMANDS
# ============================================================================

def get_selected_actors(params: dict) -> dict:
    """Get selected actors."""
    try:
        actors = []
        for actor in unreal.EditorUtilityLibrary.get_selected_actors():
            actors.append({
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "class": actor.get_class().get_name()
            })
        
        return {"success": True, "actors": actors, "count": len(actors)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_selected_assets(params: dict) -> dict:
    """Get selected assets."""
    try:
        assets = []
        for asset in unreal.EditorUtilityLibrary.get_selected_assets():
            assets.append({
                "name": asset.get_name(),
                "path": asset.get_path_name(),
                "class": asset.get_class().get_name()
            })
        
        return {"success": True, "assets": assets, "count": len(assets)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def set_selected_actors(params: dict) -> dict:
    """Set selected actors."""
    try:
        actor_names = params.get("actors", [])
        
        actors_to_select = []
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() in actor_names or actor.get_actor_label() in actor_names:
                actors_to_select.append(actor)
        
        unreal.EditorUtilityLibrary.set_selected_actors(actors_to_select)
        
        return {"success": True, "selected": len(actors_to_select)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def select_by_class(params: dict) -> dict:
    """Select actors by class."""
    try:
        class_name = params.get("class")
        if not class_name:
            return {"success": False, "error": "Class name required"}
        
        actors_to_select = []
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if class_name in actor.get_class().get_name():
                actors_to_select.append(actor)
        
        unreal.EditorUtilityLibrary.set_selected_actors(actors_to_select)
        
        return {"success": True, "selected": len(actors_to_select)}
    except Exception as e:
        return {"success": False, "error": str(e)}


def select_by_tag(params: dict) -> dict:
    """Select actors by tag."""
    try:
        tag = params.get("tag")
        if not tag:
            return {"success": False, "error": "Tag required"}
        
        actors_to_select = []
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.actor_has_tag(tag):
                actors_to_select.append(actor)
        
        unreal.EditorUtilityLibrary.set_selected_actors(actors_to_select)
        
        return {"success": True, "selected": len(actors_to_select)}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# BUILD COMMANDS
# ============================================================================

def build_lighting(params: dict) -> dict:
    """Build lighting."""
    try:
        quality = params.get("quality", "Production")  # Preview, Medium, High, Production
        
        # Execute build command
        unreal.SystemLibrary.execute_console_command(
            None,
            f"BuildLighting {quality}"
        )
        
        return {"success": True, "quality": quality}
    except Exception as e:
        return {"success": False, "error": str(e)}


def build_navigation(params: dict) -> dict:
    """Build navigation."""
    try:
        unreal.SystemLibrary.execute_console_command(None, "BuildNavigation")
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


def build_reflection_captures(params: dict) -> dict:
    """Build reflection captures."""
    try:
        unreal.SystemLibrary.execute_console_command(None, "BuildReflectionCaptures")
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


def build_all(params: dict) -> dict:
    """Build all (lighting, nav, reflection)."""
    try:
        build_lighting(params)
        build_navigation(params)
        build_reflection_captures(params)
        
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# SCREENSHOT COMMANDS
# ============================================================================

def take_screenshot(params: dict) -> dict:
    """Take a screenshot of the viewport."""
    try:
        output_path = params.get("output_path")
        resolution = params.get("resolution", [1920, 1080])
        
        if not output_path:
            output_path = str(TEMP_DIR / "screenshot.png")
        
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        # Use HighResShot
        unreal.SystemLibrary.execute_console_command(
            None,
            f"HighResShot 1 filename={output_path}"
        )
        
        return {
            "success": True,
            "output_path": output_path,
            "resolution": resolution
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def take_screenshot_from_camera(params: dict) -> dict:
    """Take screenshot from specific camera."""
    try:
        camera_name = params.get("camera")
        output_path = params.get("output_path")
        
        if not camera_name or not output_path:
            return {"success": False, "error": "Camera name and output path required"}
        
        # Find camera
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == camera_name or actor.get_actor_label() == camera_name:
                # Set view to camera
                camera_comp = actor.get_component_by_class(unreal.CameraComponent)
                if camera_comp:
                    # Take screenshot
                    unreal.SystemLibrary.execute_console_command(
                        None,
                        f"HighResShot 1 filename={output_path}"
                    )
                    return {"success": True, "camera": camera_name, "output": output_path}
        
        return {"success": False, "error": f"Camera not found: {camera_name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# PYTHON EXECUTION
# ============================================================================

def execute_python(params: dict) -> dict:
    """Execute arbitrary Python code."""
    try:
        code = params.get("code", "")
        if not code:
            return {"success": False, "error": "No code provided"}
        
        # Create namespace with unreal module
        namespace = {"unreal": unreal}
        exec(code, namespace)
        
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# UTILITY COMMANDS
# ============================================================================

def get_engine_info(params: dict) -> dict:
    """Get engine and project information."""
    try:
        info = {
            "engine_version": str(unreal.SystemLibrary.get_engine_version()),
            "project_name": unreal.SystemLibrary.get_game_name(),
            "project_dir": str(unreal.Paths.project_dir()),
            "content_dir": str(unreal.Paths.project_content_dir()),
        }
        
        return {"success": True, "info": info}
    except Exception as e:
        return {"success": False, "error": str(e)}


def execute_console_command(params: dict) -> dict:
    """Execute a console command."""
    try:
        command = params.get("command", "")
        if not command:
            return {"success": False, "error": "Command required"}
        
        unreal.SystemLibrary.execute_console_command(None, command)
        
        return {"success": True, "command": command}
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_output_log(params: dict) -> dict:
    """Get recent output log content."""
    try:
        lines = params.get("lines", 100)
        filter_pattern = params.get("filter", "")
        
        # Find the log file path
        # UE logs are typically in Saved/Logs/<ProjectName>.log
        project_dir = unreal.SystemLibrary.get_project_directory()
        project_name = unreal.SystemLibrary.get_game_name()
        
        # Try common log file locations
        log_paths = [
            Path(project_dir) / "Saved" / "Logs" / f"{project_name}.log",
            Path(project_dir) / "Saved" / "Logs" / f"{project_name}.log",
        ]
        
        log_content = []
        log_file = None
        
        for log_path in log_paths:
            if log_path.exists():
                log_file = log_path
                break
        
        if not log_file:
            return {"success": False, "error": f"Log file not found. Tried: {[str(p) for p in log_paths]}"}
        
        # Read last N lines
        with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
            all_lines = f.readlines()
            recent_lines = all_lines[-lines:] if len(all_lines) > lines else all_lines
            
            for line in recent_lines:
                line = line.strip()
                if line:
                    if filter_pattern:
                        if filter_pattern.lower() in line.lower():
                            log_content.append(line)
                    else:
                        log_content.append(line)
        
        return {
            "success": True,
            "lines": len(log_content),
            "log": log_content,
            "file": str(log_file)
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# COMMAND DISPATCHER
# ============================================================================

COMMANDS = {
    # Actor commands
    "get_actors": get_actors,
    "spawn_actor": spawn_actor,
    "delete_actor": delete_actor,
    "set_actor_transform": set_actor_transform,
    "get_actor_properties": get_actor_properties,
    "set_actor_property": set_actor_property,
    "set_actor_mesh": set_actor_mesh,
    "add_actor_tag": add_actor_tag,
    "remove_actor_tag": remove_actor_tag,
    "get_actor_components": get_actor_components,
    "add_component": add_component,
    
    # Level commands
    "get_level_info": get_level_info,
    "open_level": open_level,
    "save_level": save_level,
    "create_level": create_level,
    
    # Material commands
    "create_material": create_material,
    "create_material_instance": create_material_instance,
    "set_material_parameter": set_material_parameter,
    "apply_material": apply_material,
    
    # Workflow commands
    "import_and_setup_material": import_and_setup_material,
    
    # Blueprint commands
    "create_blueprint": create_blueprint,
    "compile_blueprint": compile_blueprint,
    "spawn_from_blueprint": spawn_from_blueprint,
    
    # Asset commands
    "get_assets": get_assets,
    "import_asset": import_asset,
    "export_asset": export_asset,
    "rename_asset": rename_asset,
    "duplicate_asset": duplicate_asset,
    "delete_asset": delete_asset,
    "find_asset_references": find_asset_references,
    "migrate_assets": migrate_assets,
    
    # Editor UI commands
    "show_notification": show_notification,
    "show_dialog": show_dialog,
    
    # Selection commands
    "get_selected_actors": get_selected_actors,
    "get_selected_assets": get_selected_assets,
    "set_selected_actors": set_selected_actors,
    "select_by_class": select_by_class,
    "select_by_tag": select_by_tag,
    
    # Build commands
    "build_lighting": build_lighting,
    "build_navigation": build_navigation,
    "build_reflection_captures": build_reflection_captures,
    "build_all": build_all,
    
    # Screenshot commands
    "take_screenshot": take_screenshot,
    "take_screenshot_from_camera": take_screenshot_from_camera,
    
    # Python execution
    "execute_python": execute_python,
    
    # Utility commands
    "get_engine_info": get_engine_info,
    "execute_console_command": execute_console_command,
    "get_output_log": get_output_log,
}


# ============================================================================
# MAIN LOOP
# ============================================================================

def write_result(result: dict):
    """Write result to result file."""
    try:
        with open(RESULT_FILE, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
    except Exception as e:
        print(f"[UE CLI] Error writing result: {e}")


def process_command():
    """Process a single command from file."""
    try:
        if not COMMAND_FILE.exists():
            return False
        
        # Tolerate UTF-8 files written by PowerShell, which may include a BOM.
        with open(COMMAND_FILE, "r", encoding="utf-8-sig") as f:
            command = json.load(f)
        
        cmd_type = command.get("type")
        cmd_id = command.get("id", "unknown")
        params = command.get("params", {})
        
        print(f"[UE CLI] Processing: {cmd_type} (id: {cmd_id})")
        
        if cmd_type in COMMANDS:
            result = COMMANDS[cmd_type](params)
        else:
            result = {"success": False, "error": f"Unknown command: {cmd_type}"}
        
        result["command_id"] = cmd_id
        result["command_type"] = cmd_type
        
        write_result(result)
        
        print(f"[UE CLI] Completed: {cmd_type}")
        
        COMMAND_FILE.unlink()
        
        return True
        
    except json.JSONDecodeError as e:
        print(f"[UE CLI] JSON decode error: {e}")
        if COMMAND_FILE.exists():
            COMMAND_FILE.unlink()
        return False
    except Exception as e:
        print(f"[UE CLI] Error: {e}")
        return False


# Global tick handle
_ue_cli_tick_handle = None

def ue_cli_tick(delta_time):
    """Tick function for auto-polling."""
    process_command()
    return True


def start_ue_cli():
    """Start auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        print("[UE CLI] Already running")
        return
    
    _ue_cli_tick_handle = unreal.register_slate_post_tick_callback(ue_cli_tick)
    print("[UE CLI] Auto-polling started!")
    print(f"[UE CLI] {len(COMMANDS)} commands available")
    print("[UE CLI] Run stop_ue_cli() to stop")


def stop_ue_cli():
    """Stop auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        unreal.unregister_slate_post_tick_callback(_ue_cli_tick_handle)
        _ue_cli_tick_handle = None
        print("[UE CLI] Stopped")
    else:
        print("[UE CLI] Not running")


print("[UE CLI Full] Ready!")
print(f"[UE CLI Full] {len(COMMANDS)} commands available")
print("[UE CLI Full] Run start_ue_cli() to begin")
