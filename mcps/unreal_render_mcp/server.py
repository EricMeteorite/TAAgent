"""
Unreal Render MCP Server Entry Point

FastMCP server for Unreal Engine rendering operations.
"""

import sys
import os

# Ensure current directory is in path for imports when running as script
_current_dir = os.path.dirname(os.path.abspath(__file__))
if _current_dir not in sys.path:
    sys.path.insert(0, _current_dir)

import logging
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any

from fastmcp import FastMCP

# Use absolute imports for MCP runtime compatibility
from connection import reset_unreal_connection
from tools import (
    # Material
    build_material_graph,
    get_material_graph,
    # Texture
    import_texture,
    # Mesh
    import_fbx,
    # Actor
    spawn_actor,
    delete_actor,
    get_actors,
    set_actor_properties,
    get_actor_properties,
    batch_spawn_actors,
    batch_delete_actors,
    batch_set_actors_properties,
    # Asset
    create_asset,
    delete_asset,
    set_asset_properties,
    get_asset_properties,
    get_assets,
    batch_create_assets,
    batch_set_assets_properties,
    # Niagara
    get_niagara_graph,
    update_niagara_graph,
    get_niagara_emitter,
    update_niagara_emitter,
    get_niagara_compiled_code,
    get_niagara_particle_attributes,
    bake_niagara_system,
    # Viewport
    get_viewport_screenshot,
    set_viewport_camera,
    get_viewport_camera,
    # Editor
    get_editor_context,
    get_open_asset_editors,
    get_selected_assets,
    get_selected_actors,
    open_asset,
    focus_asset_editor,
    close_asset_editors,
    save_asset,
    create_level,
    load_level,
    save_current_level,
    get_current_level,
    # Blueprint
    create_blueprint_variable,
    delete_blueprint_variable,
    get_blueprint_info,
    set_blueprint_variable_properties,
    update_blueprint,
)


# Configure logging – write to .taagent-local/logs/ when available, else CWD
_log_dir = os.environ.get("TAAGENT_RUNTIME_ROOT")
_log_path = (
    os.path.join(_log_dir, "logs", "unreal_render_mcp.log")
    if _log_dir
    else "unreal_render_mcp.log"
)
os.makedirs(os.path.dirname(_log_path) if os.path.dirname(_log_path) else ".", exist_ok=True)

logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler(_log_path),
    ]
)
logger = logging.getLogger("UnrealRenderMCP")


@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    logger.info("Unreal Render MCP server starting up")
    logger.info("Connection will be established lazily on first tool call")

    try:
        yield {}
    finally:
        reset_unreal_connection()
        logger.info("Unreal Render MCP server shut down")


# Initialize server
mcp = FastMCP(
    "UnrealRenderMCP",
    lifespan=server_lifespan
)


# ============================================================================
# Register Material Tools
# ============================================================================

mcp.tool()(build_material_graph)
mcp.tool()(get_material_graph)


# ============================================================================
# Register Texture Tools
# ============================================================================

mcp.tool()(import_texture)


# ============================================================================
# Register Mesh Tools
# ============================================================================

mcp.tool()(import_fbx)


# ============================================================================
# Register Actor Tools
# ============================================================================

mcp.tool()(spawn_actor)
mcp.tool()(delete_actor)
mcp.tool()(get_actors)
mcp.tool()(set_actor_properties)
mcp.tool()(get_actor_properties)
mcp.tool()(batch_spawn_actors)
mcp.tool()(batch_delete_actors)
mcp.tool()(batch_set_actors_properties)


# ============================================================================
# Register Asset Tools
# ============================================================================

mcp.tool()(create_asset)
mcp.tool()(delete_asset)
mcp.tool()(set_asset_properties)
mcp.tool()(get_asset_properties)
mcp.tool()(get_assets)
mcp.tool()(batch_create_assets)
mcp.tool()(batch_set_assets_properties)


# ============================================================================
# Register Niagara Tools
# ============================================================================

mcp.tool()(get_niagara_graph)
mcp.tool()(update_niagara_graph)
mcp.tool()(get_niagara_emitter)
mcp.tool()(update_niagara_emitter)
mcp.tool()(get_niagara_compiled_code)
mcp.tool()(get_niagara_particle_attributes)
mcp.tool()(bake_niagara_system)


# ============================================================================
# Register Viewport Tools
# ============================================================================

mcp.tool()(get_viewport_screenshot)
mcp.tool()(set_viewport_camera)
mcp.tool()(get_viewport_camera)


# ============================================================================
# Register Editor Tools
# ============================================================================

mcp.tool()(get_editor_context)
mcp.tool()(get_open_asset_editors)
mcp.tool()(get_selected_assets)
mcp.tool()(get_selected_actors)
mcp.tool()(open_asset)
mcp.tool()(focus_asset_editor)
mcp.tool()(close_asset_editors)
mcp.tool()(save_asset)
mcp.tool()(create_level)
mcp.tool()(load_level)
mcp.tool()(save_current_level)
mcp.tool()(get_current_level)


# ============================================================================
# Register Blueprint Tools
# ============================================================================

mcp.tool()(create_blueprint_variable)
mcp.tool()(delete_blueprint_variable)
mcp.tool()(get_blueprint_info)
mcp.tool()(set_blueprint_variable_properties)
mcp.tool()(update_blueprint)


# ============================================================================
# Main Entry Point
# ============================================================================

if __name__ == "__main__":
    mcp.run()
