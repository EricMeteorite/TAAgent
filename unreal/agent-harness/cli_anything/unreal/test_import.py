#!/usr/bin/env python3
"""Test import workflow with output log capture."""

import json
import os
import time
from pathlib import Path



def _resolve_temp_dir() -> Path:
    configured_dir = os.environ.get("TAAGENT_UE_CLI_DIR")
    if configured_dir:
        return Path(configured_dir)

    return Path(os.environ.get("TEMP", ".")) / "ue_cli"


TEMP_DIR = _resolve_temp_dir()
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"

def send_command(cmd_type: str, params: dict, wait_seconds: float = 2.0) -> dict:
    """Send command and wait for result."""
    cmd = {
        "id": f"test-{int(time.time()*1000)}",
        "type": cmd_type,
        "params": params
    }
    
    # Clear old result
    if RESULT_FILE.exists():
        RESULT_FILE.unlink()
    
    # Write command
    with open(COMMAND_FILE, 'w') as f:
        json.dump(cmd, f)
    
    print(f"Sent command: {cmd_type}")
    
    # Wait for result
    for i in range(int(wait_seconds * 10)):
        time.sleep(0.1)
        if RESULT_FILE.exists():
            time.sleep(0.1)  # Small delay to ensure write is complete
            with open(RESULT_FILE, 'r') as f:
                result = json.load(f)
            print(f"Got result: {json.dumps(result, indent=2)[:500]}...")
            return result
    
    print("Timeout waiting for result")
    return {"error": "timeout"}

def get_log(lines: int = 50, filter_str: str = ""):
    """Get recent UE output log."""
    return send_command("get_output_log", {"lines": lines, "filter": filter_str}, wait_seconds=1.0)

def import_texture(texture_path: str, dest_path: str = "/Game/Imported"):
    """Import a texture."""
    return send_command("import_asset", {
        "source_path": texture_path,
        "destination_path": dest_path
    })

def import_mesh(mesh_path: str, dest_path: str = "/Game/Imported"):
    """Import a mesh."""
    return send_command("import_asset", {
        "source_path": mesh_path,
        "destination_path": dest_path
    })

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python test_import.py <command> [args]")
        print("Commands:")
        print("  log [lines] [filter]  - Get output log")
        print("  texture <path>        - Import texture")
        print("  mesh <path>           - Import mesh")
        sys.exit(1)
    
    cmd = sys.argv[1]
    
    if cmd == "log":
        lines = int(sys.argv[2]) if len(sys.argv) > 2 else 50
        filter_str = sys.argv[3] if len(sys.argv) > 3 else ""
        result = get_log(lines, filter_str)
        if result.get("success"):
            print("\n=== UE Output Log ===")
            for line in result.get("log", []):
                print(line)
        else:
            print(f"Error: {result}")
    
    elif cmd == "texture":
        if len(sys.argv) < 3:
            print("Error: texture path required")
            sys.exit(1)
        texture_path = sys.argv[2]
        result = import_texture(texture_path)
        print(json.dumps(result, indent=2))
        
        # Get log after import
        time.sleep(0.5)
        log = get_log(30, "import")
        if log.get("success"):
            print("\n=== Import Log ===")
            for line in log.get("log", []):
                print(line)
    
    elif cmd == "mesh":
        if len(sys.argv) < 3:
            print("Error: mesh path required")
            sys.exit(1)
        mesh_path = sys.argv[2]
        result = import_mesh(mesh_path)
        print(json.dumps(result, indent=2))
        
        # Get log after import
        time.sleep(0.5)
        log = get_log(30, "import")
        if log.get("success"):
            print("\n=== Import Log ===")
            for line in log.get("log", []):
                print(line)
    
    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)
