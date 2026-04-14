"""
Asset Tools for Unreal Render MCP

Generic asset management tools using UE reflection.
"""

from typing import Dict, Any, List, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import send_command, with_unreal_connection

logger = logging.getLogger("UnrealRenderMCP")


@with_unreal_connection
def create_asset(
    asset_type: str,
    name: str,
    path: str = "/Game/",
    properties: dict = None
) -> Dict[str, Any]:
    """
    Create any asset by type. Uses UE reflection for universal asset creation.
    
    Args:
        asset_type: Asset type name (e.g., "Material", "MaterialInstance", "MaterialFunction")
        name: Name for the new asset
        path: Destination path
        properties: Type-specific properties
    """
    params = {"asset_type": asset_type, "name": name, "path": path}
    if properties:
        params["properties"] = properties
    return send_command("create_asset", params)


@with_unreal_connection
def delete_asset(asset_path: str) -> Dict[str, Any]:
    """Delete an asset by path."""
    return send_command("delete_asset", {"asset_path": asset_path})


@with_unreal_connection
def set_asset_properties(asset_path: str, properties: dict) -> Dict[str, Any]:
    """
    Set properties on any asset using UE reflection.
    
    Args:
        asset_path: Full asset path
        properties: Dictionary of property names and values
    """
    return send_command("set_asset_properties", {
        "asset_path": asset_path,
        "properties": properties
    })


@with_unreal_connection
def get_asset_properties(
    asset_path: str,
    properties: list = None,
    max_depth: int = 1,
    include_all_properties: bool = False,
) -> Dict[str, Any]:
    """
    Get properties of any asset using UE reflection.
    
    Args:
        asset_path: Full asset path
        properties: Optional list of specific property names to get
        max_depth: Recursive reflection depth for nested UObject/UStruct values
        include_all_properties: Include non-editable reflected properties as well
    """
    params = {
        "asset_path": asset_path,
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
    }
    if properties:
        params["properties"] = properties
    return send_command("get_asset_properties", params)


@with_unreal_connection
def get_assets(
    path: str = "/Game/",
    asset_class: str = None,
    name_filter: str = None
) -> Dict[str, Any]:
    """
    Get list of assets in the project.
    
    Args:
        path: Search path
        asset_class: Asset class filter (e.g., "Material", "Texture", "StaticMesh")
        name_filter: Optional filter string for asset names
    """
    params = {"path": path}
    if asset_class:
        params["asset_class"] = asset_class
    if name_filter:
        params["name_filter"] = name_filter
    return send_command("get_assets", params)


@with_unreal_connection
def batch_create_assets(items: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Batch create multiple assets."""
    return send_command("batch_create_assets", {"items": items})


@with_unreal_connection
def batch_set_assets_properties(items: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Batch set properties on multiple assets."""
    return send_command("batch_set_assets_properties", {"items": items})
