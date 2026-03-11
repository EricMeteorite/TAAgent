#!/usr/bin/env python3
"""Unreal Engine CLI Full - Complete command-line interface for UE5

This CLI provides comprehensive UE5 scene management capabilities.

Usage:
    ue-cli-full actor list
    ue-cli-full actor spawn -t PointLight -n MyLight -l 100,200,300
    ue-cli-full material create -n RedMat --color 1,0,0,1
    ue-cli-full build lighting --quality Production

Commands:
    actor       Actor management
    level       Level operations
    material    Material editing
    blueprint   Blueprint operations
    asset       Asset management
    select      Selection tools
    build       Build operations
    screenshot  Screenshot capture
    ui          Editor UI
    python      Python execution
    system      System commands
"""

import sys
import os
import json
import click
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cli_anything.unreal.core.session import Session
from cli_anything.unreal.utils import ue_backend

_session: Optional[Session] = None
_json_output = False
_repl_mode = False


def get_session() -> Session:
    global _session
    if _session is None:
        _session = Session()
    return _session


def output(data, message: str = ""):
    if _json_output:
        click.echo(json.dumps(data, indent=2, default=str))
    else:
        if message:
            click.echo(message)
        if isinstance(data, dict):
            _print_dict(data)
        elif isinstance(data, list):
            _print_list(data)
        else:
            click.echo(str(data))


def _print_dict(d: dict, indent: int = 0):
    prefix = "  " * indent
    for k, v in d.items():
        if isinstance(v, dict):
            click.echo(f"{prefix}{k}:")
            _print_dict(v, indent + 1)
        elif isinstance(v, list):
            click.echo(f"{prefix}{k}:")
            _print_list(v, indent + 1)
        else:
            click.echo(f"{prefix}{k}: {v}")


def _print_list(items: list, indent: int = 0):
    prefix = "  " * indent
    for i, item in enumerate(items):
        if isinstance(item, dict):
            click.echo(f"{prefix}[{i}]")
            _print_dict(item, indent + 1)
        else:
            click.echo(f"{prefix}- {item}")


def handle_error(func):
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            if _json_output:
                click.echo(json.dumps({"error": str(e), "type": type(e).__name__}))
            else:
                click.echo(f"Error: {e}", err=True)
            if not _repl_mode:
                sys.exit(1)
    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


# ============================================================================
# MAIN CLI
# ============================================================================

@click.group(invoke_without_command=True)
@click.option("--json", "use_json", is_flag=True, help="Output as JSON")
@click.option("--project", "project_path", type=str, default=None)
@click.pass_context
def cli(ctx, use_json, project_path):
    """Unreal Engine CLI - Full featured UE5 control from command line."""
    global _json_output
    _json_output = use_json

    if ctx.invoked_subcommand is None:
        ctx.invoke(repl)


# ============================================================================
# ACTOR COMMANDS
# ============================================================================

@cli.group()
def actor():
    """Actor management commands."""
    pass


@actor.command("list")
@click.option("--class", "filter_class", help="Filter by class")
@click.option("--tag", "filter_tag", help="Filter by tag")
@click.option("--name", "filter_name", help="Filter by name")
@handle_error
def actor_list(filter_class, filter_tag, filter_name):
    """List all actors with optional filters."""
    params = {}
    if filter_class:
        params["filter_class"] = filter_class
    if filter_tag:
        params["filter_tag"] = filter_tag
    if filter_name:
        params["filter_name"] = filter_name
    
    result = ue_backend.execute_command("get_actors", params)
    actors = result.get("actors", [])
    output(result, f"Found {len(actors)} actors:")


@actor.command("spawn")
@click.option("--type", "-t", default="StaticMeshActor", help="Actor class type")
@click.option("--name", "-n", help="Actor name/label")
@click.option("--location", "-l", default="0,0,0", help="Location as x,y,z")
@click.option("--rotation", "-r", default="0,0,0", help="Rotation as pitch,yaw,roll")
@click.option("--scale", "-s", default="1,1,1", help="Scale as x,y,z")
@click.option("--tag", multiple=True, help="Tags to add")
@handle_error
def actor_spawn(type, name, location, rotation, scale, tag):
    """Spawn a new actor."""
    loc = [float(x) for x in location.split(",")]
    rot = [float(x) for x in rotation.split(",")]
    scl = [float(x) for x in scale.split(",")]
    
    params = {
        "actor_class": type,
        "location": loc,
        "rotation": rot,
        "scale": scl,
        "tags": list(tag)
    }
    if name:
        params["name"] = name
    
    result = ue_backend.execute_command("spawn_actor", params)
    output(result, "Spawning actor...")


@actor.command("delete")
@click.argument("name")
@handle_error
def actor_delete(name):
    """Delete an actor by name."""
    result = ue_backend.execute_command("delete_actor", {"name": name})
    output(result, f"Deleting actor: {name}")


@actor.command("move")
@click.argument("name")
@click.option("--location", "-l", help="New location as x,y,z")
@click.option("--rotation", "-r", help="New rotation as pitch,yaw,roll")
@click.option("--scale", "-s", help="New scale as x,y,z")
@handle_error
def actor_move(name, location, rotation, scale):
    """Move/rotate/scale an actor."""
    params = {"name": name}
    if location:
        params["location"] = [float(x) for x in location.split(",")]
    if rotation:
        params["rotation"] = [float(x) for x in rotation.split(",")]
    if scale:
        params["scale"] = [float(x) for x in scale.split(",")]
    
    result = ue_backend.execute_command("set_actor_transform", params)
    output(result, f"Transforming actor: {name}")


@actor.command("info")
@click.argument("name")
@handle_error
def actor_info(name):
    """Get detailed actor information."""
    result = ue_backend.execute_command("get_actor_properties", {"name": name})
    output(result, f"Actor info: {name}")


@actor.command("set")
@click.argument("name")
@click.option("--property", "-p", required=True, help="Property name")
@click.option("--value", "-v", required=True, help="Property value")
@handle_error
def actor_set(name, property, value):
    """Set actor property."""
    result = ue_backend.execute_command("set_actor_property", {
        "name": name,
        "property": property,
        "value": value
    })
    output(result, f"Setting {property} on {name}")


@actor.group()
def tag():
    """Actor tag management."""
    pass


@tag.command("add")
@click.argument("name")
@click.argument("tag_name")
@handle_error
def actor_tag_add(name, tag_name):
    """Add tag to actor."""
    result = ue_backend.execute_command("add_actor_tag", {
        "name": name,
        "tag": tag_name
    })
    output(result, f"Adding tag '{tag_name}' to {name}")


@tag.command("remove")
@click.argument("name")
@click.argument("tag_name")
@handle_error
def actor_tag_remove(name, tag_name):
    """Remove tag from actor."""
    result = ue_backend.execute_command("remove_actor_tag", {
        "name": name,
        "tag": tag_name
    })
    output(result, f"Removing tag '{tag_name}' from {name}")


@actor.command("components")
@click.argument("name")
@handle_error
def actor_components(name):
    """Get actor components."""
    result = ue_backend.execute_command("get_actor_components", {"name": name})
    output(result, f"Components of {name}:")


@actor.command("set-mesh")
@click.argument("name")
@click.option("--mesh", "-m", required=True, help="Static mesh path")
@click.option("--material", "-mat", help="Material path")
@handle_error
def actor_set_mesh(name, mesh, material):
    """Set static mesh and material for an actor."""
    params = {"name": name, "mesh_path": mesh}
    if material:
        params["material_path"] = material
    result = ue_backend.execute_command("set_actor_mesh", params)
    output(result, f"Setting mesh for {name}")


# ============================================================================
# LEVEL COMMANDS
# ============================================================================

@cli.group()
def level():
    """Level management commands."""
    pass


@level.command("info")
@handle_error
def level_info():
    """Show current level information."""
    result = ue_backend.execute_command("get_level_info")
    output(result, "Level Info:")


@level.command("open")
@click.argument("path")
@handle_error
def level_open(path):
    """Open a level by path."""
    result = ue_backend.execute_command("open_level", {"level_path": path})
    output(result, f"Opening level: {path}")


@level.command("save")
@handle_error
def level_save():
    """Save the current level."""
    result = ue_backend.execute_command("save_level")
    output(result, "Saving level...")


@level.command("create")
@click.argument("path")
@handle_error
def level_create(path):
    """Create a new level."""
    result = ue_backend.execute_command("create_level", {"level_path": path})
    output(result, f"Creating level: {path}")


# ============================================================================
# MATERIAL COMMANDS
# ============================================================================

@cli.group()
def material():
    """Material management commands."""
    pass


@material.command("create")
@click.option("--name", "-n", required=True, help="Material name")
@click.option("--path", "-p", default="/Game/Materials", help="Package path")
@click.option("--color", "-c", default="1,1,1,1", help="Base color as r,g,b,a")
@handle_error
def material_create(name, path, color):
    """Create a new material."""
    base_color = [float(x) for x in color.split(",")]
    result = ue_backend.execute_command("create_material", {
        "name": name,
        "path": path,
        "base_color": base_color
    })
    output(result, f"Creating material: {name}")


@material.command("instance")
@click.option("--name", "-n", required=True, help="Instance name")
@click.option("--parent", required=True, help="Parent material path")
@click.option("--path", "-p", default="/Game/Materials", help="Package path")
@handle_error
def material_instance(name, parent, path):
    """Create a material instance."""
    result = ue_backend.execute_command("create_material_instance", {
        "name": name,
        "parent": parent,
        "path": path
    })
    output(result, f"Creating material instance: {name}")


@material.command("param")
@click.argument("material_path")
@click.option("--name", "-n", required=True, help="Parameter name")
@click.option("--value", "-v", required=True, help="Parameter value")
@click.option("--type", "-t", default="scalar", help="Parameter type (scalar/vector/texture)")
@handle_error
def material_param(material_path, name, value, type):
    """Set material instance parameter."""
    params = {
        "material": material_path,
        "parameter": name,
        "type": type
    }
    
    if type == "vector":
        params["value"] = [float(x) for x in value.split(",")]
    else:
        params["value"] = value
    
    result = ue_backend.execute_command("set_material_parameter", params)
    output(result, f"Setting parameter {name}")


@material.command("apply")
@click.argument("actor_name")
@click.argument("material_path")
@click.option("--slot", "-s", default=0, help="Material slot index")
@handle_error
def material_apply(actor_name, material_path, slot):
    """Apply material to an actor."""
    result = ue_backend.execute_command("apply_material", {
        "actor_name": actor_name,
        "material_path": material_path,
        "slot": slot
    })
    output(result, f"Applying material to {actor_name}")


# ============================================================================
# WORKFLOW COMMANDS
# ============================================================================

@cli.group()
def workflow():
    """Complete workflow commands (import + material setup)."""
    pass


@workflow.command("import-and-setup")
@click.option("--texture", "-t", required=True, help="Path to texture file")
@click.option("--mesh", "-m", help="Path to mesh file (fbx/obj)")
@click.option("--name", "-n", default="M_AutoGenerated", help="Material name")
@click.option("--path", "-p", default="/Game/Imported", help="Import destination path")
@handle_error
def workflow_import_setup(texture, mesh, name, path):
    """Complete workflow: Import texture/mesh, create material with texture."""
    params = {
        "texture_path": texture,
        "material_name": name,
        "import_path": path
    }
    if mesh:
        params["mesh_path"] = mesh
    
    result = ue_backend.execute_command("import_and_setup_material", params)
    output(result, "Import and setup workflow:")


# BLUEPRINT COMMANDS
# ============================================================================

@cli.group()
def blueprint():
    """Blueprint management commands."""
    pass


@blueprint.command("create")
@click.option("--name", "-n", required=True, help="Blueprint name")
@click.option("--parent", "-p", default="Actor", help="Parent class")
@click.option("--path", default="/Game/Blueprints", help="Package path")
@handle_error
def blueprint_create(name, parent, path):
    """Create a new blueprint."""
    result = ue_backend.execute_command("create_blueprint", {
        "name": name,
        "parent_class": parent,
        "path": path
    })
    output(result, f"Creating blueprint: {name}")


@blueprint.command("compile")
@click.argument("path")
@handle_error
def blueprint_compile(path):
    """Compile a blueprint."""
    result = ue_backend.execute_command("compile_blueprint", {"blueprint": path})
    output(result, f"Compiling blueprint: {path}")


@blueprint.command("spawn")
@click.argument("path")
@click.option("--name", "-n", help="Actor name")
@click.option("--location", "-l", default="0,0,0", help="Location as x,y,z")
@click.option("--rotation", "-r", default="0,0,0", help="Rotation as pitch,yaw,roll")
@handle_error
def blueprint_spawn(path, name, location, rotation):
    """Spawn actor from blueprint."""
    loc = [float(x) for x in location.split(",")]
    rot = [float(x) for x in rotation.split(",")]
    
    params = {
        "blueprint": path,
        "location": loc,
        "rotation": rot
    }
    if name:
        params["name"] = name
    
    result = ue_backend.execute_command("spawn_from_blueprint", params)
    output(result, f"Spawning from blueprint: {path}")


# ============================================================================
# ASSET COMMANDS
# ============================================================================

@cli.group()
def asset():
    """Asset management commands."""
    pass


@asset.command("list")
@click.option("--path", "-p", default="/Game", help="Asset path")
@click.option("--class", "asset_class", help="Filter by class")
@click.option("--recursive/--no-recursive", default=True, help="Recursive search")
@handle_error
def asset_list(path, asset_class, recursive):
    """List assets in a path."""
    params = {"path": path, "recursive": recursive}
    if asset_class:
        params["class"] = asset_class
    
    result = ue_backend.execute_command("get_assets", params)
    assets = result.get("assets", [])
    output(result, f"Found {len(assets)} assets in {path}:")


@asset.command("import")
@click.argument("source")
@click.argument("destination")
@handle_error
def asset_import(source, destination):
    """Import an asset from file system."""
    result = ue_backend.execute_command("import_asset", {
        "source_path": source,
        "destination_path": destination
    })
    output(result, f"Importing asset...")


@asset.command("export")
@click.argument("asset_path")
@click.argument("export_path")
@handle_error
def asset_export(asset_path, export_path):
    """Export an asset to file."""
    result = ue_backend.execute_command("export_asset", {
        "asset": asset_path,
        "export_path": export_path
    })
    output(result, f"Exporting asset...")


@asset.command("rename")
@click.argument("source")
@click.argument("destination")
@handle_error
def asset_rename(source, destination):
    """Rename an asset."""
    result = ue_backend.execute_command("rename_asset", {
        "source": source,
        "destination": destination
    })
    output(result, f"Renaming asset...")


@asset.command("duplicate")
@click.argument("source")
@click.argument("destination")
@handle_error
def asset_duplicate(source, destination):
    """Duplicate an asset."""
    result = ue_backend.execute_command("duplicate_asset", {
        "source": source,
        "destination": destination
    })
    output(result, f"Duplicating asset...")


@asset.command("delete")
@click.argument("path")
@handle_error
def asset_delete(path):
    """Delete an asset."""
    result = ue_backend.execute_command("delete_asset", {"asset": path})
    output(result, f"Deleting asset: {path}")


@asset.command("refs")
@click.argument("path")
@handle_error
def asset_refs(path):
    """Find references to an asset."""
    result = ue_backend.execute_command("find_asset_references", {"asset": path})
    output(result, f"References to {path}:")


# ============================================================================
# SELECT COMMANDS
# ============================================================================

@cli.group()
def select():
    """Selection management commands."""
    pass


@select.command("actors")
@handle_error
def select_actors():
    """Get selected actors."""
    result = ue_backend.execute_command("get_selected_actors")
    output(result, "Selected actors:")


@select.command("assets")
@handle_error
def select_assets():
    """Get selected assets."""
    result = ue_backend.execute_command("get_selected_assets")
    output(result, "Selected assets:")


@select.command("set")
@click.argument("names", nargs=-1)
@handle_error
def select_set(names):
    """Set selected actors by name."""
    result = ue_backend.execute_command("set_selected_actors", {"actors": list(names)})
    output(result, f"Selecting actors: {', '.join(names)}")


@select.command("by-class")
@click.argument("class_name")
@handle_error
def select_by_class(class_name):
    """Select actors by class."""
    result = ue_backend.execute_command("select_by_class", {"class": class_name})
    output(result, f"Selecting actors of class: {class_name}")


@select.command("by-tag")
@click.argument("tag")
@handle_error
def select_by_tag(tag):
    """Select actors by tag."""
    result = ue_backend.execute_command("select_by_tag", {"tag": tag})
    output(result, f"Selecting actors with tag: {tag}")


# ============================================================================
# BUILD COMMANDS
# ============================================================================

@cli.group()
def build():
    """Build commands."""
    pass


@build.command("lighting")
@click.option("--quality", "-q", default="Production", 
              type=click.Choice(["Preview", "Medium", "High", "Production"]))
@handle_error
def build_lighting(quality):
    """Build lighting."""
    result = ue_backend.execute_command("build_lighting", {"quality": quality})
    output(result, f"Building lighting ({quality})...")


@build.command("navigation")
@handle_error
def build_navigation():
    """Build navigation."""
    result = ue_backend.execute_command("build_navigation")
    output(result, "Building navigation...")


@build.command("reflections")
@handle_error
def build_reflections():
    """Build reflection captures."""
    result = ue_backend.execute_command("build_reflection_captures")
    output(result, "Building reflection captures...")


@build.command("all")
@handle_error
def build_all():
    """Build all (lighting, navigation, reflections)."""
    result = ue_backend.execute_command("build_all")
    output(result, "Building all...")


# ============================================================================
# SCREENSHOT COMMANDS
# ============================================================================

@cli.group()
def screenshot():
    """Screenshot commands."""
    pass


@screenshot.command("capture")
@click.option("--output", "-o", help="Output file path")
@click.option("--resolution", "-r", default="1920,1080", help="Resolution as width,height")
@handle_error
def screenshot_capture(output, resolution):
    """Take a screenshot of the viewport."""
    res = [int(x) for x in resolution.split(",")]
    params = {"resolution": res}
    if output:
        params["output_path"] = output
    
    result = ue_backend.execute_command("take_screenshot", params)
    output(result, "Taking screenshot...")


@screenshot.command("camera")
@click.argument("camera_name")
@click.option("--output", "-o", required=True, help="Output file path")
@handle_error
def screenshot_camera(camera_name, output):
    """Take screenshot from specific camera."""
    result = ue_backend.execute_command("take_screenshot_from_camera", {
        "camera": camera_name,
        "output_path": output
    })
    output(result, f"Taking screenshot from {camera_name}...")


# ============================================================================
# UI COMMANDS
# ============================================================================

@cli.group()
def ui():
    """Editor UI commands."""
    pass


@ui.command("notify")
@click.argument("message")
@click.option("--type", "-t", default="info", type=click.Choice(["info", "warning", "error"]))
@click.option("--duration", "-d", default=5.0, help="Duration in seconds")
@handle_error
def ui_notify(message, type, duration):
    """Show editor notification."""
    result = ue_backend.execute_command("show_notification", {
        "message": message,
        "type": type,
        "duration": duration
    })
    output(result, "Showing notification...")


@ui.command("dialog")
@click.option("--title", "-t", default="Message", help="Dialog title")
@click.option("--message", "-m", required=True, help="Dialog message")
@handle_error
def ui_dialog(title, message):
    """Show editor dialog."""
    result = ue_backend.execute_command("show_dialog", {
        "title": title,
        "message": message
    })
    output(result, "Showing dialog...")


# ============================================================================
# PYTHON COMMANDS
# ============================================================================

@cli.group()
def python():
    """Python execution commands."""
    pass


@python.command("exec")
@click.argument("code")
@handle_error
def python_exec(code):
    """Execute Python code in UE."""
    result = ue_backend.execute_command("execute_python", {"code": code})
    output(result, "Executing Python code...")


@python.command("file")
@click.argument("file_path")
@handle_error
def python_file(file_path):
    """Execute a Python file in UE."""
    with open(file_path, "r") as f:
        code = f.read()
    result = ue_backend.execute_command("execute_python", {"code": code})
    output(result, f"Executing Python file: {file_path}")


# ============================================================================
# SYSTEM COMMANDS
# ============================================================================

@cli.group()
def system():
    """System commands."""
    pass


@system.command("info")
@handle_error
def system_info():
    """Get engine and project information."""
    result = ue_backend.execute_command("get_engine_info")
    output(result, "Engine Info:")


@system.command("cmd")
@click.argument("command")
@handle_error
def system_cmd(command):
    """Execute console command."""
    result = ue_backend.execute_command("execute_console_command", {"command": command})
    output(result, f"Executing: {command}")


# ============================================================================
# REPL MODE
# ============================================================================

@cli.command()
def repl():
    """Start interactive REPL mode."""
    global _repl_mode
    _repl_mode = True

    click.echo("╔════════════════════════════════════════════════════════════╗")
    click.echo("║     Unreal Engine CLI Full - Interactive Mode              ║")
    click.echo("╚════════════════════════════════════════════════════════════╝")
    click.echo("")
    click.echo("Commands: actor, level, material, blueprint, asset, select,")
    click.echo("          build, screenshot, ui, python, system")
    click.echo("Type 'help' for help, 'exit' to quit.")
    click.echo("")

    while True:
        try:
            user_input = click.prompt("ue-cli", type=str)
            user_input = user_input.strip()

            if not user_input:
                continue

            if user_input.lower() in ("exit", "quit"):
                click.echo("Goodbye!")
                break

            if user_input.lower() == "help":
                click.echo(cli.get_help(click.Context(cli)))
                continue

            args = user_input.split()
            cli(args, standalone_mode=False)

        except click.exceptions.Exit:
            break
        except KeyboardInterrupt:
            click.echo("\nUse 'exit' to quit.")
        except Exception as e:
            click.echo(f"Error: {e}", err=True)


if __name__ == "__main__":
    cli()
