#!/usr/bin/env python3
"""Unreal Engine CLI Listener - Run this in UE Python Console

This script runs inside Unreal Engine and listens for commands from the CLI
via file-based communication.

To use:
1. Open Unreal Editor
2. Open Python Console (Window -> Developer Tools -> Python Console)
3. Run this file from your local TAAgent checkout, for example:
   exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener.py").read())
4. Or copy-paste this script into the console

The listener will poll for command files and execute them.
"""

import unreal
import json
import os
from pathlib import Path

# Configuration
TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"))
)
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"

# Ensure temp directory exists
TEMP_DIR.mkdir(parents=True, exist_ok=True)

print(f"[UE CLI] Listener initialized. Watching: {COMMAND_FILE}")
print(f"[UE CLI] Run 'ue_cli_tick()' to process one command, or 'start_ue_cli()' to start auto-poll")


def write_result(result: dict):
    """Write result to result file."""
    try:
        with open(RESULT_FILE, "w") as f:
            json.dump(result, f, indent=2)
    except Exception as e:
        print(f"[UE CLI] Error writing result: {e}")


def get_actors(params: dict) -> dict:
    """Get all actors in current level."""
    try:
        actors = []
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            actors.append({
                "name": actor.get_name(),
                "class": actor.get_class().get_name(),
                "location": {
                    "x": actor.get_actor_location().x,
                    "y": actor.get_actor_location().y,
                    "z": actor.get_actor_location().z
                }
            })
        return {"success": True, "actors": actors}
    except Exception as e:
        return {"success": False, "error": str(e)}


def spawn_actor(params: dict) -> dict:
    """Spawn an actor in the level."""
    try:
        actor_class = params.get("actor_class", "StaticMeshActor")
        name = params.get("name")
        location = params.get("location", [0, 0, 0])
        rotation = params.get("rotation", [0, 0, 0])
        
        # Get actor class
        if actor_class == "StaticMeshActor":
            cls = unreal.StaticMeshActor
        elif actor_class == "PointLight":
            cls = unreal.PointLight
        elif actor_class == "DirectionalLight":
            cls = unreal.DirectionalLight
        elif actor_class == "CameraActor":
            cls = unreal.CameraActor
        elif actor_class == "SkyLight":
            cls = unreal.SkyLight
        elif actor_class == "PlayerStart":
            cls = unreal.PlayerStart
        else:
            # Try to find class by name
            cls = unreal.load_class(None, f"/Script/Engine.{actor_class}")
            if not cls:
                return {"success": False, "error": f"Unknown actor class: {actor_class}"}
        
        # Spawn actor
        loc = unreal.Vector(location[0], location[1], location[2])
        rot = unreal.Rotator(rotation[0], rotation[1], rotation[2])
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
        
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


def get_level_info(params: dict) -> dict:
    """Get current level information."""
    try:
        # Get all actors
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        
        # Get level name from world
        world = unreal.EditorLevelLibrary.get_editor_world()
        level_name = world.get_name() if world else "Unknown"
        
        return {
            "success": True,
            "level": {
                "name": level_name,
                "actor_count": len(actors)
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
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def save_level(params: dict) -> dict:
    """Save the current level."""
    try:
        result = unreal.EditorLevelLibrary.save_current_level()
        return {"success": result}
    except Exception as e:
        return {"success": False, "error": str(e)}


def create_material(params: dict) -> dict:
    """Create a new material."""
    try:
        name = params.get("name", "NewMaterial")
        path = params.get("path", "/Game/Materials")
        base_color = params.get("base_color", [1.0, 1.0, 1.0, 1.0])
        
        # Create material factory
        factory = unreal.MaterialFactoryNew()
        
        # Create the material
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        material = asset_tools.create_asset(
            asset_name=name,
            package_path=path,
            asset_class=unreal.Material,
            factory=factory
        )
        
        if material:
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


def apply_material(params: dict) -> dict:
    """Apply material to an actor."""
    try:
        actor_name = params.get("actor_name")
        material_path = params.get("material_path")
        
        if not actor_name or not material_path:
            return {"success": False, "error": "Actor name and material path required"}
        
        # Load material
        material = unreal.EditorAssetLibrary.load_asset(material_path)
        if not material:
            return {"success": False, "error": f"Material not found: {material_path}"}
        
        # Find actor and apply material
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_name() == actor_name or actor.get_actor_label() == actor_name:
                # Get static mesh component
                mesh_comp = actor.get_component_by_class(unreal.StaticMeshComponent)
                if mesh_comp:
                    mesh_comp.set_material(0, material)
                    return {"success": True, "actor": actor_name, "material": material_path}
                else:
                    return {"success": False, "error": "Actor has no StaticMeshComponent"}
        
        return {"success": False, "error": f"Actor not found: {actor_name}"}
    except Exception as e:
        return {"success": False, "error": str(e)}


def take_screenshot(params: dict) -> dict:
    """Take a screenshot of the viewport."""
    try:
        output_path = params.get("output_path")
        resolution = params.get("resolution", [1920, 1080])
        
        if not output_path:
            output_path = str(TEMP_DIR / "screenshot.png")
        
        # Ensure directory exists
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        # Take screenshot using HighResScreenshot system
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


def execute_python(params: dict) -> dict:
    """Execute arbitrary Python code."""
    try:
        code = params.get("code", "")
        if not code:
            return {"success": False, "error": "No code provided"}
        
        # Execute the code
        exec(code, {"unreal": unreal})
        
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


def get_assets(params: dict) -> dict:
    """Get assets in a path."""
    try:
        path = params.get("path", "/Game")
        
        assets = []
        asset_data_list = unreal.EditorAssetLibrary.list_assets(path, recursive=True)
        
        for asset_data in asset_data_list:
            assets.append({
                "name": asset_data.asset_name,
                "path": asset_data.object_path,
                "class": str(asset_data.asset_class)
            })
        
        return {"success": True, "assets": assets}
    except Exception as e:
        return {"success": False, "error": str(e)}


def import_asset(params: dict) -> dict:
    """Import an asset into the project."""
    try:
        source_path = params.get("source_path")
        destination_path = params.get("destination_path")
        
        if not source_path or not destination_path:
            return {"success": False, "error": "Source and destination paths required"}
        
        # Import using AssetTools
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        
        # Create import task
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source_path)
        task.set_editor_property('destination_path', destination_path)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('automated', True)
        
        asset_tools.import_asset_tasks([task])
        
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


# Command dispatcher
COMMANDS = {
    "get_actors": get_actors,
    "spawn_actor": spawn_actor,
    "delete_actor": delete_actor,
    "set_actor_transform": set_actor_transform,
    "get_level_info": get_level_info,
    "open_level": open_level,
    "save_level": save_level,
    "create_material": create_material,
    "apply_material": apply_material,
    "take_screenshot": take_screenshot,
    "execute_python": execute_python,
    "get_assets": get_assets,
    "import_asset": import_asset,
}


def process_command():
    """Process a single command from file. Returns True if command was processed."""
    try:
        if not COMMAND_FILE.exists():
            return False
        
        # Read command
        with open(COMMAND_FILE, "r") as f:
            command = json.load(f)
        
        cmd_type = command.get("type")
        cmd_id = command.get("id", "unknown")
        params = command.get("params", {})
        
        print(f"[UE CLI] Processing: {cmd_type} (id: {cmd_id})")
        
        # Execute command
        if cmd_type in COMMANDS:
            result = COMMANDS[cmd_type](params)
        else:
            result = {"success": False, "error": f"Unknown command: {cmd_type}"}
        
        # Add command ID to result
        result["command_id"] = cmd_id
        result["command_type"] = cmd_type
        
        # Write result
        write_result(result)
        
        print(f"[UE CLI] Completed: {cmd_type}")
        
        # Delete command file
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


# Global tick handle for auto-polling
_ue_cli_tick_handle = None

def ue_cli_tick(delta_time):
    """Tick function for auto-polling. Call this manually or use start_ue_cli()."""
    process_command()
    return True  # Continue ticking


def start_ue_cli():
    """Start auto-polling using UE's tick system."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        print("[UE CLI] Already running")
        return
    
    # Register tick callback
    _ue_cli_tick_handle = unreal.register_slate_post_tick_callback(ue_cli_tick)
    print("[UE CLI] Auto-polling started. Commands will be processed automatically.")
    print("[UE CLI] Run stop_ue_cli() to stop")


def stop_ue_cli():
    """Stop auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        unreal.unregister_slate_post_tick_callback(_ue_cli_tick_handle)
        _ue_cli_tick_handle = None
        print("[UE CLI] Auto-polling stopped")
    else:
        print("[UE CLI] Not running")


# Manual mode - just process one command
print("[UE CLI] Ready!")
print("[UE CLI] Options:")
print("  1. Manual: Run ue_cli_tick() to process one command")
print("  2. Auto: Run start_ue_cli() to start auto-polling")
print("  3. Stop: Run stop_ue_cli() to stop auto-polling")
