"""
Editor Tools for Unreal Render MCP

Level management and editor operations.
"""

from typing import Dict, Any, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import send_command, with_unreal_connection

logger = logging.getLogger("UnrealRenderMCP")


@with_unreal_connection
def create_level(level_path: str) -> Dict[str, Any]:
    """
    Create a new level and save it to the specified path.
    
    Args:
        level_path: Path to save the level (e.g., "/Game/Levels/LookDev")
    
    Returns:
        Success status and level path
    """
    return send_command("create_level", {"level_path": level_path})


@with_unreal_connection
def load_level(level_path: str) -> Dict[str, Any]:
    """
    Load an existing level.
    
    Args:
        level_path: Path to the level (e.g., "/Game/Levels/LookDev")
    
    Returns:
        Success status and level path
    """
    return send_command("load_level", {"level_path": level_path})


@with_unreal_connection
def save_current_level() -> Dict[str, Any]:
    """
    Save the current level.
    
    Returns:
        Success status
    """
    return send_command("save_current_level", {})


@with_unreal_connection
def get_current_level() -> Dict[str, Any]:
    """
    Get information about the current level.
    
    Returns:
        Level name and path
    """
    return send_command("get_current_level", {})


@with_unreal_connection
def get_editor_context() -> Dict[str, Any]:
    """
    Get the current Unreal Editor context.

    Returns:
        Active tab, open asset editors, selected assets, selected actors, and current level.
    """
    return send_command("get_editor_context", {})


@with_unreal_connection
def get_open_asset_editors() -> Dict[str, Any]:
    """
    List all currently open asset editors.

    Returns:
        Open asset editor summaries and associated assets.
    """
    return send_command("get_open_asset_editors", {})


@with_unreal_connection
def get_selected_assets() -> Dict[str, Any]:
    """
    Get assets currently selected in the Content Browser.

    Returns:
        Selected asset summaries.
    """
    return send_command("get_selected_assets", {})


@with_unreal_connection
def get_selected_actors() -> Dict[str, Any]:
    """
    Get actors currently selected in the editor world.

    Returns:
        Selected actor summaries.
    """
    return send_command("get_selected_actors", {})


@with_unreal_connection
def open_asset(asset_path: str) -> Dict[str, Any]:
    """
    Open an asset in its default Unreal asset editor.

    Args:
        asset_path: Asset object path, for example "/Game/Foo/Bar.Bar"

    Returns:
        Open result and asset summary.
    """
    return send_command("open_asset", {"asset_path": asset_path})


@with_unreal_connection
def focus_asset_editor(asset_path: str) -> Dict[str, Any]:
    """
    Focus the asset editor for an already-open asset.

    Args:
        asset_path: Asset object path, for example "/Game/Foo/Bar.Bar"

    Returns:
        Focus result and editor summary.
    """
    return send_command("focus_asset_editor", {"asset_path": asset_path})


@with_unreal_connection
def close_asset_editors(asset_path: Optional[str] = None, close_all: bool = False) -> Dict[str, Any]:
    """
    Close asset editor windows.

    Args:
        asset_path: Optional asset object path to close.
        close_all: When true, closes all open asset editors.

    Returns:
        Close result.
    """
    params: Dict[str, Any] = {"close_all": close_all}
    if asset_path:
        params["asset_path"] = asset_path
    return send_command("close_asset_editors", params)


@with_unreal_connection
def save_asset(asset_path: str) -> Dict[str, Any]:
    """
    Save an asset by object path.

    Args:
        asset_path: Asset object path, for example "/Game/Foo/Bar.Bar"

    Returns:
        Save result and asset summary.
    """
    return send_command("save_asset", {"asset_path": asset_path})
