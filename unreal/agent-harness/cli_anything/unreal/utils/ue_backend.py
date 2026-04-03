"""Unreal Engine Backend - File-based communication with running UE editor.

This backend uses a file-based communication mechanism:
1. CLI writes command JSON to a command file
2. UE Python script (running in editor) polls and executes commands
3. UE writes results to a result file
4. CLI reads results from the file

Requires: Unreal Engine 5.x with Python Script Plugin enabled
"""

import os
import json
import time
import tempfile
import subprocess
import shutil
from typing import Optional, Dict, Any, List
from pathlib import Path

# Default paths for file-based communication
DEFAULT_TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(tempfile.gettempdir()) / "ue_cli"))
)
COMMAND_FILE = DEFAULT_TEMP_DIR / "command.json"
RESULT_FILE = DEFAULT_TEMP_DIR / "result.json"
LOCK_FILE = DEFAULT_TEMP_DIR / "lock"

# UE Editor executable paths (common locations)
UE_EDITOR_PATHS = [
    r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
    r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe",
    r"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe",
    r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
]


def find_ue_editor() -> str:
    """Find the Unreal Editor executable."""
    # Check environment variable first
    ue_path = os.environ.get("UE_EDITOR_PATH")
    if ue_path and os.path.exists(ue_path):
        return ue_path
    
    # Check common paths
    for path in UE_EDITOR_PATHS:
        if os.path.exists(path):
            return path
    
    # Try to find in PATH
    for name in ("UnrealEditor.exe", "UE4Editor.exe"):
        path = shutil.which(name)
        if path:
            return path
    
    raise RuntimeError(
        "Unreal Editor not found. Please set UE_EDITOR_PATH environment variable "
        "or ensure UE is installed in a standard location."
    )


def get_ue_version() -> str:
    """Get the installed Unreal Engine version."""
    try:
        ue_editor = find_ue_editor()
        # Extract version from path
        parts = ue_editor.split(os.sep)
        for part in parts:
            if "UE_" in part or "UE" in part:
                return part
        return "Unknown"
    except RuntimeError:
        return "Not Found"


def ensure_temp_dir():
    """Ensure the temp directory exists."""
    DEFAULT_TEMP_DIR.mkdir(parents=True, exist_ok=True)


def write_command(command: Dict[str, Any]) -> str:
    """Write a command to the command file.
    
    Returns:
        Command ID for tracking
    """
    ensure_temp_dir()
    
    # Generate unique command ID
    cmd_id = f"cmd_{int(time.time() * 1000)}"
    command["id"] = cmd_id
    command["timestamp"] = time.time()
    
    # Write command file
    with open(COMMAND_FILE, "w") as f:
        json.dump(command, f, indent=2)
    
    # Clear any old result
    if RESULT_FILE.exists():
        RESULT_FILE.unlink()
    
    return cmd_id


def wait_for_result(cmd_id: str, timeout: float = 30.0) -> Dict[str, Any]:
    """Wait for and read the result file.
    
    Args:
        cmd_id: The command ID to wait for
        timeout: Maximum seconds to wait
        
    Returns:
        The result dictionary
    """
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if RESULT_FILE.exists():
            try:
                with open(RESULT_FILE, "r") as f:
                    result = json.load(f)
                
                # Check if this is our result
                if result.get("command_id") == cmd_id:
                    # Clean up files
                    RESULT_FILE.unlink()
                    if COMMAND_FILE.exists():
                        COMMAND_FILE.unlink()
                    return result
            except (json.JSONDecodeError, IOError):
                pass
        
        time.sleep(0.1)
    
    # Timeout
    return {
        "success": False,
        "error": f"Timeout waiting for result (cmd_id: {cmd_id})",
        "command_id": cmd_id
    }


def execute_command(command_type: str, params: Dict[str, Any] = None, 
                   timeout: float = 30.0) -> Dict[str, Any]:
    """Execute a command in Unreal Engine.
    
    Args:
        command_type: The type of command (e.g., 'get_actors', 'spawn_actor')
        params: Command parameters
        timeout: Maximum seconds to wait for result
        
    Returns:
        Dict with command result
    """
    command = {
        "type": command_type,
        "params": params or {}
    }
    
    cmd_id = write_command(command)
    result = wait_for_result(cmd_id, timeout)
    
    return result


def is_ue_running() -> bool:
    """Check if Unreal Editor is running."""
    import subprocess
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"],
        capture_output=True, text=True
    )
    return "UnrealEditor.exe" in result.stdout


def launch_ue_editor(project_path: Optional[str] = None, 
                     background: bool = True) -> subprocess.Popen:
    """Launch Unreal Editor.
    
    Args:
        project_path: Path to .uproject file (optional)
        background: Run in background
        
    Returns:
        The subprocess.Popen object
    """
    ue_editor = find_ue_editor()
    
    cmd = [ue_editor]
    if project_path:
        cmd.append(project_path)
    
    if background:
        return subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=subprocess.CREATE_NEW_CONSOLE
        )
    else:
        return subprocess.Popen(cmd)


# Command wrappers for common operations

def get_actors() -> List[Dict[str, Any]]:
    """Get all actors in the current level."""
    result = execute_command("get_actors", timeout=10.0)
    return result.get("actors", []) if result.get("success") else []


def spawn_actor(actor_class: str, name: Optional[str] = None,
                location: Optional[List[float]] = None,
                rotation: Optional[List[float]] = None) -> Dict[str, Any]:
    """Spawn an actor in the level."""
    params = {"actor_class": actor_class}
    if name:
        params["name"] = name
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    
    return execute_command("spawn_actor", params, timeout=10.0)


def delete_actor(actor_name: str) -> Dict[str, Any]:
    """Delete an actor from the level."""
    return execute_command("delete_actor", {"name": actor_name}, timeout=10.0)


def set_actor_transform(actor_name: str, 
                        location: Optional[List[float]] = None,
                        rotation: Optional[List[float]] = None,
                        scale: Optional[List[float]] = None) -> Dict[str, Any]:
    """Set actor transform."""
    params = {"name": actor_name}
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    
    return execute_command("set_actor_transform", params, timeout=10.0)


def get_level_info() -> Dict[str, Any]:
    """Get current level information."""
    return execute_command("get_level_info", timeout=10.0)


def open_level(level_path: str) -> Dict[str, Any]:
    """Open a level."""
    return execute_command("open_level", {"level_path": level_path}, timeout=30.0)


def save_level() -> Dict[str, Any]:
    """Save the current level."""
    return execute_command("save_level", timeout=10.0)


def create_material(name: str, path: str = "/Game/Materials",
                   base_color: Optional[List[float]] = None) -> Dict[str, Any]:
    """Create a new material."""
    params = {"name": name, "path": path}
    if base_color:
        params["base_color"] = base_color
    
    return execute_command("create_material", params, timeout=10.0)


def apply_material(actor_name: str, material_path: str) -> Dict[str, Any]:
    """Apply material to an actor."""
    return execute_command("apply_material", {
        "actor_name": actor_name,
        "material_path": material_path
    }, timeout=10.0)


def take_screenshot(output_path: Optional[str] = None,
                   resolution: Optional[List[int]] = None) -> Dict[str, Any]:
    """Take a screenshot of the viewport."""
    params = {}
    if output_path:
        params["output_path"] = output_path
    if resolution:
        params["resolution"] = resolution
    
    return execute_command("take_screenshot", params, timeout=30.0)


def execute_python(code: str) -> Dict[str, Any]:
    """Execute arbitrary Python code in UE."""
    return execute_command("execute_python", {"code": code}, timeout=30.0)


def get_assets(path: str = "/Game") -> List[Dict[str, Any]]:
    """Get assets in a path."""
    result = execute_command("get_assets", {"path": path}, timeout=10.0)
    return result.get("assets", []) if result.get("success") else []


def import_asset(source_path: str, destination_path: str,
                options: Optional[Dict] = None) -> Dict[str, Any]:
    """Import an asset into the project."""
    params = {
        "source_path": source_path,
        "destination_path": destination_path
    }
    if options:
        params["options"] = options
    
    return execute_command("import_asset", params, timeout=30.0)
