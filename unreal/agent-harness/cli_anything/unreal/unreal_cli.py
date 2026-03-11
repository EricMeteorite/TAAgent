#!/usr/bin/env python3
"""Unreal Engine CLI — A stateful command-line interface for UE5 scene editing.

This CLI provides full UE5 scene management capabilities using a file-based
communication mechanism with the running UE editor.

Usage:
    # One-shot commands
    python -m cli_anything.unreal scene info
    python -m cli_anything.unreal actor spawn -t PointLight -n "MyLight"
    python -m cli_anything.unreal material create -n "RedMat" --color 1,0,0,1

    # Interactive REPL
    python -m cli_anything.unreal repl

Prerequisites:
    1. Unreal Engine 5.x with Python Script Plugin enabled
    2. Run ue_cli_listener.py in UE Python Console
    3. Set UE_EDITOR_PATH environment variable (optional)
"""

import sys
import os
import json
import click
from typing import Optional

# Add parent to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from cli_anything.unreal.core.session import Session
from cli_anything.unreal.utils import ue_backend

# Global session state
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
        except FileNotFoundError as e:
            if _json_output:
                click.echo(json.dumps({"error": str(e), "type": "file_not_found"}))
            else:
                click.echo(f"Error: {e}", err=True)
            if not _repl_mode:
                sys.exit(1)
        except (ValueError, IndexError, RuntimeError) as e:
            if _json_output:
                click.echo(json.dumps({"error": str(e), "type": type(e).__name__}))
            else:
                click.echo(f"Error: {e}", err=True)
            if not _repl_mode:
                sys.exit(1)
        except FileExistsError as e:
            if _json_output:
                click.echo(json.dumps({"error": str(e), "type": "file_exists"}))
            else:
                click.echo(f"Error: {e}", err=True)
            if not _repl_mode:
                sys.exit(1)
    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


# ── Main CLI Group ──────────────────────────────────────────────
@click.group(invoke_without_command=True)
@click.option("--json", "use_json", is_flag=True, help="Output as JSON")
@click.option("--project", "project_path", type=str, default=None,
              help="Path to .ue-cli.json project file")
@click.pass_context
def cli(ctx, use_json, project_path):
    """Unreal Engine CLI — Stateful UE5 scene editing from the command line.

    Run without a subcommand to enter interactive REPL mode.
    """
    global _json_output
    _json_output = use_json

    if project_path:
        sess = get_session()
        if not sess.has_project():
            # Load project
            with open(project_path, "r") as f:
                proj = json.load(f)
            sess.set_project(proj, project_path)

    if ctx.invoked_subcommand is None:
        ctx.invoke(repl, project_path=None)


# ── Info Commands ──────────────────────────────────────────────
@cli.group()
def info():
    """Information and status commands."""
    pass


@info.command("status")
@handle_error
def info_status():
    """Show UE connection status."""
    status = {
        "ue_running": ue_backend.is_ue_running(),
        "ue_version": ue_backend.get_ue_version(),
        "temp_dir": str(ue_backend.DEFAULT_TEMP_DIR),
    }
    output(status, "UE Status:")


@info.command("ping")
@handle_error
def info_ping():
    """Ping UE to check connection."""
    result = ue_backend.get_level_info()
    if result.get("success"):
        output({"status": "connected", "level": result.get("level")}, "UE is connected!")
    else:
        output({"status": "disconnected", "error": result.get("error")}, 
               "Failed to connect to UE. Make sure UE is running and the listener script is active.")


# ── Scene/Level Commands ──────────────────────────────────────────────
@cli.group()
def level():
    """Level management commands."""
    pass


@level.command("info")
@handle_error
def level_info():
    """Show current level information."""
    result = ue_backend.get_level_info()
    if result.get("success"):
        output(result.get("level", {}), "Level Info:")
    else:
        output(result, "Error:")


@level.command("open")
@click.argument("level_path")
@handle_error
def level_open(level_path):
    """Open a level by path."""
    result = ue_backend.open_level(level_path)
    output(result, f"Opening level: {level_path}")


@level.command("save")
@handle_error
def level_save():
    """Save the current level."""
    result = ue_backend.save_level()
    output(result, "Saving level...")


# ── Actor Commands ──────────────────────────────────────────────
@cli.group()
def actor():
    """Actor management commands."""
    pass


@actor.command("list")
@handle_error
def actor_list():
    """List all actors in the current level."""
    actors = ue_backend.get_actors()
    output(actors, f"Found {len(actors)} actors:")


@actor.command("spawn")
@click.option("--type", "-t", default="StaticMeshActor", 
              help="Actor class type (StaticMeshActor, PointLight, etc.)")
@click.option("--name", "-n", default=None, help="Actor name/label")
@click.option("--location", "-l", default="0,0,0", 
              help="Location as x,y,z")
@click.option("--rotation", "-r", default="0,0,0",
              help="Rotation as pitch,yaw,roll")
@handle_error
def actor_spawn(type, name, location, rotation):
    """Spawn a new actor in the level."""
    loc = [float(x) for x in location.split(",")]
    rot = [float(x) for x in rotation.split(",")]
    
    result = ue_backend.spawn_actor(type, name, loc, rot)
    output(result, "Spawning actor...")


@actor.command("delete")
@click.argument("name")
@handle_error
def actor_delete(name):
    """Delete an actor by name."""
    result = ue_backend.delete_actor(name)
    output(result, f"Deleting actor: {name}")


@actor.command("move")
@click.argument("name")
@click.option("--location", "-l", default=None, help="New location as x,y,z")
@click.option("--rotation", "-r", default=None, help="New rotation as pitch,yaw,roll")
@click.option("--scale", "-s", default=None, help="New scale as x,y,z")
@handle_error
def actor_move(name, location, rotation, scale):
    """Move/rotate/scale an actor."""
    loc = [float(x) for x in location.split(",")] if location else None
    rot = [float(x) for x in rotation.split(",")] if rotation else None
    scl = [float(x) for x in scale.split(",")] if scale else None
    
    result = ue_backend.set_actor_transform(name, loc, rot, scl)
    output(result, f"Transforming actor: {name}")


# ── Material Commands ──────────────────────────────────────────────
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
    result = ue_backend.create_material(name, path, base_color)
    output(result, f"Creating material: {name}")


@material.command("apply")
@click.argument("actor_name")
@click.argument("material_path")
@handle_error
def material_apply(actor_name, material_path):
    """Apply a material to an actor."""
    result = ue_backend.apply_material(actor_name, material_path)
    output(result, f"Applying material to {actor_name}")


# ── Asset Commands ──────────────────────────────────────────────
@cli.group()
def asset():
    """Asset management commands."""
    pass


@asset.command("list")
@click.option("--path", "-p", default="/Game", help="Asset path")
@handle_error
def asset_list(path):
    """List assets in a path."""
    assets = ue_backend.get_assets(path)
    output(assets, f"Found {len(assets)} assets in {path}:")


@asset.command("import")
@click.argument("source_path")
@click.argument("destination_path")
@handle_error
def asset_import(source_path, destination_path):
    """Import an asset from file system."""
    result = ue_backend.import_asset(source_path, destination_path)
    output(result, f"Importing asset...")


# ── Screenshot Commands ──────────────────────────────────────────────
@cli.group()
def screenshot():
    """Screenshot and rendering commands."""
    pass


@screenshot.command("capture")
@click.option("--output", "-o", "output_path", default=None, help="Output file path")
@click.option("--resolution", "-r", default="1920,1080", help="Resolution as width,height")
@handle_error
def screenshot_capture(output_path, resolution):
    """Take a screenshot of the viewport."""
    res = [int(x) for x in resolution.split(",")]
    result = ue_backend.take_screenshot(output_path, res)
    output(result, "Taking screenshot...")


# ── Python Commands ──────────────────────────────────────────────
@cli.group()
def python():
    """Python script execution commands."""
    pass


@python.command("exec")
@click.argument("code")
@handle_error
def python_exec(code):
    """Execute Python code in UE."""
    result = ue_backend.execute_python(code)
    output(result, "Executing Python code...")


@python.command("file")
@click.argument("file_path")
@handle_error
def python_file(file_path):
    """Execute a Python file in UE."""
    with open(file_path, "r") as f:
        code = f.read()
    result = ue_backend.execute_python(code)
    output(result, f"Executing Python file: {file_path}")


# ── Session Commands ──────────────────────────────────────────────
@cli.group()
def session():
    """Session management commands."""
    pass


@session.command("status")
@handle_error
def session_status():
    """Show session status."""
    sess = get_session()
    output(sess.status(), "Session Status:")


@session.command("undo")
@handle_error
def session_undo():
    """Undo last operation."""
    sess = get_session()
    description = sess.undo()
    output({"undone": description}, f"Undone: {description}")


@session.command("redo")
@handle_error
def session_redo():
    """Redo last undone operation."""
    sess = get_session()
    description = sess.redo()
    output({"redone": description}, f"Redone: {description}")


# ── REPL Mode ──────────────────────────────────────────────
@cli.command()
@click.option("--project", "project_path", type=str, default=None)
def repl(project_path):
    """Start interactive REPL mode."""
    global _repl_mode
    _repl_mode = True

    if project_path:
        sess = get_session()
        if not sess.has_project():
            with open(project_path, "r") as f:
                proj = json.load(f)
            sess.set_project(proj, project_path)

    click.echo("╔════════════════════════════════════════════════════════════╗")
    click.echo("║          Unreal Engine CLI - Interactive Mode              ║")
    click.echo("╚════════════════════════════════════════════════════════════╝")
    click.echo("")
    click.echo("Type 'help' for available commands, 'exit' to quit.")
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

            # Parse and execute command
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
