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


@with_unreal_connection
def execute_unreal_python(
    code: str,
    execution_mode: str = "ExecuteFile",
    file_execution_scope: str = "Private",
    unattended: bool = True,
) -> Dict[str, Any]:
    """
    Execute Python code inside the running Unreal Editor.

    Args:
        code: Python source code or statement to execute.
        execution_mode: One of ExecuteFile, ExecuteStatement, EvaluateStatement.
        file_execution_scope: Private or Public when using ExecuteFile.
        unattended: Whether to suppress interactive UI while executing.

    Returns:
        Success flag, command result string, and captured Unreal Python log output.
    """
    return send_command(
        "execute_unreal_python",
        {
            "code": code,
            "execution_mode": execution_mode,
            "file_execution_scope": file_execution_scope,
            "unattended": unattended,
        },
    )


@with_unreal_connection
def get_post_process_volume_details(
    actor_name: Optional[str] = None,
    actor_path: Optional[str] = None,
    max_depth: int = 1,
    include_all_properties: bool = False,
    include_settings: bool = True,
    include_blendables: bool = True,
) -> Dict[str, Any]:
    """
    Get detailed post process volume info including weighted blendables.

    Args:
        actor_name: PostProcessVolume actor name.
        actor_path: Optional full actor path.
        max_depth: Recursive reflection depth for nested settings/object values.
        include_all_properties: Include non-editable reflected properties as well.
        include_settings: Include full FPostProcessSettings struct serialization.
        include_blendables: Include weighted blendable entries.

    Returns:
        Volume settings and weighted blendable asset list.
    """
    params: Dict[str, Any] = {
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
        "include_settings": include_settings,
        "include_blendables": include_blendables,
    }
    if actor_name:
        params["actor_name"] = actor_name
    if actor_path:
        params["actor_path"] = actor_path
    return send_command("get_post_process_volume_details", params)


@with_unreal_connection
def get_current_level_post_process_overview(
    level_filter: Optional[str] = None,
    max_depth: int = 1,
    include_all_properties: bool = False,
    include_settings: bool = False,
    include_blendables: bool = True,
) -> Dict[str, Any]:
    """
    Get current level post process overview for optimization analysis.

    Args:
        level_filter: Optional substring filter for sublevel/actor path.
        max_depth: Recursive reflection depth for nested settings/object values.
        include_all_properties: Include non-editable reflected properties as well.
        include_settings: Include full FPostProcessSettings struct serialization.
        include_blendables: Include weighted blendable entries.

    Returns:
        All post process volumes in the current world with weighted blendables.
    """
    params: Dict[str, Any] = {
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
        "include_settings": include_settings,
        "include_blendables": include_blendables,
    }
    if level_filter:
        params["level_filter"] = level_filter
    return send_command("get_current_level_post_process_overview", params)
