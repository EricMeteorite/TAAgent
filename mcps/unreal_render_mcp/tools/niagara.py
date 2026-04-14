"""
Niagara Tools for Unreal Render MCP

Consolidated Niagara system analysis and modification tools.

Design Philosophy:
- 4 tools following "Minimal Tool Set" principle
- Graph form: get/update Niagara script graphs
- Emitter form: get/update Emitter hierarchy structure
"""

from typing import Dict, Any, List, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import send_command, save_json_to_file, with_unreal_connection

logger = logging.getLogger("UnrealRenderMCP")


def _resolve_niagara_asset_path(
    asset_path: Optional[str] = None,
    asset_name: Optional[str] = None
) -> Optional[str]:
    if asset_path:
        return asset_path

    from editor import get_editor_context

    context = get_editor_context()
    if context.get("status") != "success":
        return None

    result = context.get("result", {})
    candidates: List[Dict[str, Any]] = []

    active_editor = result.get("active_asset_editor") or {}
    if active_editor.get("asset_type_name") == "NiagaraSystem":
        candidates.extend(active_editor.get("assets") or [])

    for editor in result.get("open_asset_editors") or []:
        if editor.get("asset_type_name") == "NiagaraSystem":
            candidates.extend(editor.get("assets") or [])

    for asset in result.get("open_assets") or []:
        if asset.get("class_name") == "NiagaraSystem":
            candidates.append(asset)

    def matches_name(candidate: Dict[str, Any]) -> bool:
        if not asset_name:
            return True
        candidate_names = {
            candidate.get("name"),
            candidate.get("asset_name"),
            candidate.get("package_name"),
            candidate.get("asset_path"),
        }
        return asset_name in {name for name in candidate_names if name}

    for candidate in candidates:
        if matches_name(candidate):
            resolved_path = candidate.get("asset_path")
            if resolved_path:
                return resolved_path

    return None


# ============================================================================
# Graph Form Tools - Read/Update Niagara Script Graphs
# ============================================================================

@with_unreal_connection
def get_niagara_graph(
    # Method 1: Locate script within System's Emitter
    asset_path: Optional[str] = None,
    emitter: Optional[str] = None,
    script: str = "spawn",
    
    # Method 2: Direct script path for standalone assets
    script_path: Optional[str] = None,
    
    # Optional filter
    module: str = "",
    max_depth: int = 1,
    include_all_properties: bool = False,
    save_to: Optional[str] = None
) -> Dict[str, Any]:
    """
    Get Niagara Graph nodes and connections.
    
    Unified interface for reading graphs from:
    1. Scripts within a Niagara System (specify asset_path + emitter + script)
    2. Standalone Niagara Script assets (specify script_path only)
    
    Args:
        asset_path: Path to Niagara System asset (for embedded scripts)
        emitter: Emitter name within the system
        script: Script type - "spawn" or "update" (default: "spawn")
        script_path: Direct path to standalone Niagara Script asset
        module: Optional filter - only return nodes matching this module name
        max_depth: Recursive reflection depth for nested Niagara UObject/UStruct values
        include_all_properties: Include non-editable reflected properties as well
        save_to: Optional path to save JSON
        
    Examples:
        # Get graph from System's Emitter spawn script
        get_niagara_graph(asset_path="/Game/Effects/NS_Fire", emitter="Flame", script="spawn")
        
        # Get graph from standalone Niagara Module
        get_niagara_graph(script_path="/Niagara/Modules/Update/Forces/CurlNoiseForce")
    """
    params = {
        "script": script,
        "module": module,
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
    }
    
    # Determine location method
    if script_path:
        params["script_path"] = script_path
    elif asset_path and emitter:
        params["asset_path"] = asset_path
        params["emitter"] = emitter
    else:
        return {
            "success": False,
            "error": "Must provide either (asset_path + emitter) or script_path"
        }
    
    result = send_command("get_niagara_graph", params)
    
    if save_to and result.get("success"):
        graph_name = script_path.split("/")[-1] if script_path else f"{emitter}_{script}"
        save_info = save_json_to_file(result, save_to, "niagara_graph", graph_name)
        result.update(save_info)
    
    return result


@with_unreal_connection
def update_niagara_graph(
    # Method 1: Locate script within System's Emitter
    asset_path: Optional[str] = None,
    emitter: Optional[str] = None,
    script: str = "spawn",
    
    # Method 2: Direct script path for standalone assets
    script_path: Optional[str] = None,
    
    operations: List[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """
    Update Niagara Graph with operations.
    
    Unified interface for updating graphs in:
    1. Scripts within a Niagara System (specify asset_path + emitter + script)
    2. Standalone Niagara Script assets (specify script_path only)
    
    Operations:
    - add_module: Add a module to the script
    - remove_module: Remove a module from the script
    - set_parameter: Set a parameter value
    - connect: Connect two nodes
    - disconnect: Disconnect nodes
    
    Args:
        asset_path: Path to Niagara System asset (for embedded scripts)
        emitter: Emitter name within the system
        script: Script type - "spawn" or "update"
        script_path: Direct path to standalone Niagara Script asset
        operations: List of operations to perform
        
    Examples:
        # Add module to System's Emitter script
        update_niagara_graph(
            asset_path="/Game/Effects/NS_Fire",
            emitter="Flame",
            script="update",
            operations=[{"action": "add_module", "module_path": "/Niagara/Modules/Update/Forces/CurlNoiseForce"}]
        )
        
        # Add module to standalone script
        update_niagara_graph(
            script_path="/Game/Niagara/MyCustomModule",
            operations=[{"action": "add_module", "module_path": "/Niagara/Modules/Common/Math"}]
        )
    """
    params = {
        "script": script,
        "operations": operations or []
    }
    
    # Determine location method
    if script_path:
        params["script_path"] = script_path
    elif asset_path and emitter:
        params["asset_path"] = asset_path
        params["emitter"] = emitter
    else:
        return {
            "success": False,
            "error": "Must provide either (asset_path + emitter) or script_path"
        }
    
    return send_command("update_niagara_graph", params)


# ============================================================================
# Emitter Form Tools - Read/Update Emitter Hierarchy
# ============================================================================

@with_unreal_connection
def get_niagara_emitter(
    asset_path: str,
    detail_level: str = "overview",
    emitters: List[str] = None,
    include: List[str] = None,
    max_depth: int = 1,
    include_all_properties: bool = False,
    save_to: Optional[str] = None
) -> Dict[str, Any]:
    """
    Get Niagara Emitter structure and properties.
    
    Args:
        asset_path: Path to the Niagara system asset
        detail_level: Level of detail - "overview" or "full"
        emitters: Optional list of emitter names to filter
        include: Optional list of sections to include:
            - "scripts": Spawn/Update script details
            - "renderers": Renderer configurations
            - "parameters": Parameter definitions
            - "stateless_analysis": Analyze Stateless conversion compatibility
        max_depth: Recursive reflection depth for nested Niagara UObject/UStruct values
        include_all_properties: Include non-editable reflected properties as well
        save_to: Optional path to save JSON
        
    Examples:
        # Quick overview of all emitters
        get_niagara_emitter("/Game/Effects/NS_Fire")
        
        # Full detail for specific emitter with stateless analysis
        get_niagara_emitter(
            "/Game/Effects/NS_Fire",
            detail_level="full",
            emitters=["Flame"],
            include=["scripts", "renderers", "parameters", "stateless_analysis"]
        )
    """
    params = {
        "asset_path": asset_path,
        "detail_level": detail_level,
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
    }
    if emitters is not None:
        params["emitters"] = emitters
    if include is not None:
        params["include"] = include
    
    result = send_command("get_niagara_emitter", params)
    
    if save_to and result.get("success"):
        asset_name = result.get("asset_name", asset_path.split("/")[-1])
        save_info = save_json_to_file(result, save_to, "niagara_emitter", asset_name)
        result.update(save_info)
    
    return result


@with_unreal_connection
def update_niagara_emitter(
    asset_path: str,
    operations: List[Dict[str, Any]]
) -> Dict[str, Any]:
    """
    Batch update Niagara Emitter with multiple operations.
    
    Operations format:
    [
        # Emitter operations
        {"target": "emitter", "name": "Smoke", "action": "set_enabled", "value": false},
        {"target": "emitter", "name": "Flame", "action": "rename", "value": "BigFlame"},
        {"target": "emitter", "name": "Smoke", "action": "add", "template": "/Niagara/Templates/SimpleSmoke"},
        {"target": "emitter", "name": "OldEmitter", "action": "remove"},
        
        # Stateless conversion (UE 5.7+)
        {"target": "emitter", "name": "Flame", "action": "convert_to_stateless", "force": false},
        
        # Renderer operations
        {"target": "renderer", "emitter": "Flame", "index": 0, "action": "set_enabled", "value": true},
        
        # Parameter operations
        {"target": "parameter", "emitter": "Flame", "script": "spawn", "name": "SpawnRate", "value": 100.0},
        
        # Simulation stage operations
        {"target": "sim_stage", "emitter": "Flame", "name": "Collision", "action": "set_enabled", "value": true},
        
        # Stateless module operations (UE 5.7+)
        {"target": "stateless_module", "emitter": "Flame", "name": "Lifetime", 
         "action": "set_property", "property": "LifetimeMin", "value": 1.5}
    ]
    
    Args:
        asset_path: Path to the Niagara system asset
        operations: List of operations to perform
        
    Examples:
        # Enable emitter and adjust spawn rate
        update_niagara_emitter("/Game/Effects/NS_Fire", [
            {"target": "emitter", "name": "Flame", "action": "set_enabled", "value": True},
            {"target": "parameter", "emitter": "Flame", "script": "spawn", "name": "SpawnRate", "value": 100.0}
        ])
        
        # Convert emitter to Stateless mode
        update_niagara_emitter("/Game/Effects/NS_Fire", [
            {"target": "emitter", "name": "Flame", "action": "convert_to_stateless", "force": False}
        ])
    """
    return send_command("update_niagara_emitter", {
        "asset_path": asset_path,
        "operations": operations
    })


# ============================================================================
# Debug Tools - Compiled Code Inspection
# ============================================================================

@with_unreal_connection
def get_niagara_compiled_code(
    asset_path: str,
    emitter: Optional[str] = None,
    script: str = "spawn"
) -> Dict[str, Any]:
    """
    Get compiled HLSL code from Niagara scripts.
    
    Returns generated HLSL code for CPU (VM) and GPU (Compute Shader) scripts.
    Useful for debugging and understanding Niagara execution.
    
    Args:
        asset_path: Path to Niagara System asset
        emitter: Emitter name (required for particle scripts)
        script: Script type - "spawn", "update", "gpu_compute", 
                "system_spawn", or "system_update"
    
    Returns:
        {
            "success": true,
            "hlsl_cpu": "...",      // CPU/VM HLSL code
            "hlsl_gpu": "...",      // GPU Compute Shader HLSL
            "hlsl_cpu_length": 1234,
            "hlsl_gpu_length": 5678,
            "compile_errors": [],
            "error_count": 0
        }
    
    Examples:
        # Get spawn script HLSL for an emitter
        get_niagara_compiled_code("/Game/Effects/NS_Fire", emitter="Flame", script="spawn")
        
        # Get update script HLSL
        get_niagara_compiled_code("/Game/Effects/NS_Fire", emitter="Flame", script="update")
        
        # Get GPU compute shader
        get_niagara_compiled_code("/Game/Effects/NS_Fire", emitter="Flame", script="gpu_compute")
        
        # Get system-level script
        get_niagara_compiled_code("/Game/Effects/NS_Fire", script="system_spawn")
    """
    params = {
        "asset_path": asset_path,
        "script": script
    }
    if emitter:
        params["emitter"] = emitter
    
    return send_command("get_niagara_compiled_code", params)


# ============================================================================
# Debug Tools - Runtime Particle Inspection
# ============================================================================

@with_unreal_connection
def get_niagara_particle_attributes(
    component_name: str,
    emitter: Optional[str] = None,
    frame: int = 0
) -> Dict[str, Any]:
    """
    Get particle attribute data from a running Niagara component.
    
    Captures current particle state from a Niagara system in the level.
    Returns particle data similar to the Attribute Spreadsheet in UE editor.
    
    Args:
        component_name: Name of the Niagara component in the level
        emitter: Optional emitter name to filter (returns all if not specified)
        frame: Frame index to read (default 0 = current frame)
    
    Returns:
        {
            "success": true,
            "component_name": "NiagaraComponent",
            "num_frames": 1,
            "current_frame": 0,
            "emitters": [
                {
                    "name": "EmitterName",
                    "particle_count": 100,
                    "attributes": [
                        {"name": "Position", "float_count": 3},
                        {"name": "Color", "float_count": 4},
                        ...
                    ],
                    "particles": [
                        {
                            "index": 0,
                            "position": [x, y, z],
                            "color": [r, g, b, a],
                            "lifetime": 2.5,
                            "age": 0.5,
                            "velocity": [vx, vy, vz]
                        },
                        ...
                    ]
                }
            ]
        }
    
    Examples:
        # Get all particle data from a component
        result = get_niagara_particle_attributes("MyNiagaraComponent")
        
        # Get data for a specific emitter
        result = get_niagara_particle_attributes(
            component_name="MyNiagaraComponent",
            emitter="Flame"
        )
    """
    params = {
        "component_name": component_name,
        "frame": frame
    }
    if emitter:
        params["emitter"] = emitter
    
    return send_command("get_niagara_particle_attributes", params)


@with_unreal_connection
def bake_niagara_system(
    asset_path: Optional[str] = None,
    asset_name: Optional[str] = None,
    output_dir: Optional[str] = None,
    output_prefix: str = "",
    frame_width: int = 0,
    frame_height: int = 0,
    start_seconds: Optional[float] = None,
    duration_seconds: Optional[float] = None,
    frames_per_second: int = 0,
    frames_x: int = 0,
    frames_y: int = 0,
    render_component_only: Optional[bool] = None,
    file_extension: str = ".png"
) -> Dict[str, Any]:
    """
    Bake a Niagara System to a frame sequence with final-color RGB and merged alpha.

    If `asset_path` is omitted, the tool tries to use the currently active/open Niagara
    asset from the editor context.

    Args:
        asset_path: Full Niagara System asset path.
        asset_name: Optional name to match against currently open Niagara assets.
        output_dir: Optional absolute output directory. Defaults to the asset's on-disk folder.
        output_prefix: Optional output file prefix. Defaults to the asset name.
        frame_width: Optional output frame width. Defaults to current baker frame size or 512.
        frame_height: Optional output frame height. Defaults to current baker frame size or 512.
        start_seconds: Optional override for baker start time.
        duration_seconds: Optional override for baker duration.
        frames_per_second: Optional override for baker simulation FPS.
        frames_x: Optional override for bake frame grid X count.
        frames_y: Optional override for bake frame grid Y count.
        render_component_only: Optional override for component-only rendering.
        file_extension: Image extension, usually ".png" or ".exr".
    """
    resolved_asset_path = _resolve_niagara_asset_path(asset_path=asset_path, asset_name=asset_name)
    if not resolved_asset_path:
        return {
            "success": False,
            "error": "Unable to resolve a Niagara System asset path from the provided arguments or current editor context"
        }

    params: Dict[str, Any] = {
        "asset_path": resolved_asset_path,
    }

    if output_dir:
        params["output_dir"] = output_dir
    if output_prefix:
        params["output_prefix"] = output_prefix
    if frame_width > 0:
        params["frame_width"] = frame_width
    if frame_height > 0:
        params["frame_height"] = frame_height
    if start_seconds is not None:
        params["start_seconds"] = start_seconds
    if duration_seconds is not None:
        params["duration_seconds"] = duration_seconds
    if frames_per_second > 0:
        params["frames_per_second"] = frames_per_second
    if frames_x > 0:
        params["frames_x"] = frames_x
    if frames_y > 0:
        params["frames_y"] = frames_y
    if render_component_only is not None:
        params["render_component_only"] = render_component_only
    if file_extension:
        params["file_extension"] = file_extension

    return send_command("bake_niagara_system", params)
