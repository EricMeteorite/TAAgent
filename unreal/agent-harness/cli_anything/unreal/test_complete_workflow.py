#!/usr/bin/env python3
"""Complete workflow test: import texture, mesh, create material, apply to mesh."""

import json
import time
from pathlib import Path

TEMP_DIR = Path("C:/Users/LK867/AppData/Local/Temp/ue_cli")
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"

def send_command(cmd_type: str, params: dict, wait_seconds: float = 3.0) -> dict:
    """Send command and wait for result."""
    cmd = {
        "id": f"test-{int(time.time()*1000)}",
        "type": cmd_type,
        "params": params
    }
    
    if RESULT_FILE.exists():
        RESULT_FILE.unlink()
    
    with open(COMMAND_FILE, 'w') as f:
        json.dump(cmd, f)
    
    print(f"[CMD] {cmd_type}")
    
    for i in range(int(wait_seconds * 10)):
        time.sleep(0.1)
        if RESULT_FILE.exists():
            time.sleep(0.1)
            with open(RESULT_FILE, 'r') as f:
                result = json.load(f)
            return result.get("result", result)
    
    return {"error": "timeout"}

def main():
    print("=" * 60)
    print("COMPLETE WORKFLOW TEST")
    print("=" * 60)
    
    # Step 1: Import texture
    print("\n[1] Importing texture...")
    result = send_command("import_asset", {
        "source_path": "d:/CodeBuddy/rendering-mcp/output/lookdev_rgb.png",
        "destination_path": "/Game/Test/Textures"
    })
    print(f"    Result: {result.get('success', False)}")
    if result.get("success"):
        texture_path = result["assets"][0]["path"]
        print(f"    Texture: {texture_path}")
    else:
        print(f"    Error: {result.get('error')}")
        return
    
    # Step 2: Import mesh
    print("\n[2] Importing mesh...")
    result = send_command("import_asset", {
        "source_path": "d:/CodeBuddy/rendering-mcp/output/test_cube.obj",
        "destination_path": "/Game/Test/Meshes"
    })
    print(f"    Result: {result.get('success', False)}")
    if result.get("success"):
        mesh_path = result["assets"][0]["path"]
        print(f"    Mesh: {mesh_path}")
    else:
        print(f"    Error: {result.get('error')}")
        return
    
    # Step 3: Create material
    print("\n[3] Creating material...")
    result = send_command("create_material", {
        "name": "M_TestComplete",
        "path": "/Game/Test/Materials"
    })
    print(f"    Result: {result.get('success', False)}")
    if result.get("success"):
        material_path = result["material"]["path"]
        print(f"    Material: {material_path}")
    else:
        print(f"    Error: {result.get('error')}")
        return
    
    # Step 4: Spawn actor
    print("\n[4] Spawning StaticMeshActor...")
    result = send_command("spawn_actor", {
        "actor_class": "StaticMeshActor",
        "name": "TestMeshActor",
        "location": [0, 0, 100]
    })
    print(f"    Result: {result.get('success', False)}")
    if result.get("success"):
        actor_name = result["actor"]["name"]
        print(f"    Actor: {actor_name}")
    else:
        print(f"    Error: {result.get('error')}")
        return
    
    # Step 5: Set mesh on actor
    print("\n[5] Setting mesh on actor...")
    result = send_command("set_actor_mesh", {
        "actor": actor_name,
        "mesh": mesh_path
    })
    print(f"    Result: {result.get('success', False)}")
    if not result.get("success"):
        print(f"    Error: {result.get('error')}")
    
    # Step 6: Apply material to mesh
    print("\n[6] Applying material to mesh...")
    result = send_command("apply_material", {
        "actor": actor_name,
        "material": material_path
    })
    print(f"    Result: {result.get('success', False)}")
    if not result.get("success"):
        print(f"    Error: {result.get('error')}")
    
    print("\n" + "=" * 60)
    print("WORKFLOW COMPLETE")
    print("=" * 60)
    print(f"Texture: {texture_path}")
    print(f"Mesh: {mesh_path}")
    print(f"Material: {material_path}")
    print(f"Actor: {actor_name}")

if __name__ == "__main__":
    main()
