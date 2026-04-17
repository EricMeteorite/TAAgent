"""
Blueprint Tools for Unreal Render MCP

Editor Blueprint analysis and update tools.

Note: For listing EditorUtilityWidgetBlueprint assets, use get_assets with:
    get_assets(asset_class="EditorUtilityWidgetBlueprint")
"""

from typing import Dict, Any, List, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import send_command, with_unreal_connection

logger = logging.getLogger("UnrealRenderMCP")


@with_unreal_connection
def create_blueprint_variable(
    blueprint_name: str,
    variable_name: str,
    variable_type: str,
    default_value: Any = None,
    is_public: bool = False,
    tooltip: str = "",
    category: str = "Default"
) -> Dict[str, Any]:
    """
    Create a member variable in a Blueprint.

    Args:
        blueprint_name: Blueprint asset path or name understood by UnrealMCP
        variable_name: New variable name
        variable_type: Variable type string such as bool, int, float, string, vector, rotator
        default_value: Optional default value
        is_public: Whether the variable should be instance-editable
        tooltip: Optional variable tooltip
        category: Variable category shown in the Blueprint editor

    Returns:
        UnrealMCP response JSON
    """
    params = {
        "blueprint_name": blueprint_name,
        "variable_name": variable_name,
        "variable_type": variable_type,
        "is_public": is_public,
        "tooltip": tooltip,
        "category": category,
    }
    if default_value is not None:
        params["default_value"] = default_value
    return send_command("create_variable", params)


@with_unreal_connection
def delete_blueprint_variable(
    blueprint_name: str,
    variable_name: str,
) -> Dict[str, Any]:
    """
    Delete a member variable from a Blueprint.

    Args:
        blueprint_name: Blueprint asset path or name understood by UnrealMCP
        variable_name: Variable name to remove

    Returns:
        UnrealMCP response JSON
    """
    return send_command(
        "delete_variable",
        {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
        },
    )


@with_unreal_connection
def set_blueprint_variable_properties(
    blueprint_name: str,
    variable_name: str,
    properties: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """
    Update an existing Blueprint variable's properties.

    Args:
        blueprint_name: Blueprint asset path or name understood by UnrealMCP
        variable_name: Existing variable name
        properties: Variable property overrides accepted by UnrealMCP

    Returns:
        UnrealMCP response JSON
    """
    params = {
        "blueprint_name": blueprint_name,
        "variable_name": variable_name,
    }
    if properties:
        params.update(properties)
    return send_command("set_blueprint_variable_properties", params)


@with_unreal_connection
def get_blueprint_info(
    blueprint_path: str,
    include_variables: bool = True,
    include_functions: bool = True,
    include_widget_tree: bool = True
) -> Dict[str, Any]:
    """
    Get detailed information about a Blueprint.
    
    Works with both Game Blueprints and Editor Utility Widget Blueprints.
    
    Args:
        blueprint_path: Full path to the blueprint asset
            - Game Blueprint: "/Script/Engine.Blueprint'/Game/MyBP.MyBP'"
            - Editor Widget: "/Script/Blutility.EditorUtilityWidgetBlueprint'/Game/MyWidget.MyWidget'"
        include_variables: Include variable list
        include_functions: Include function list
        include_widget_tree: Include widget tree info (for Editor Utility Widgets)
    
    Returns:
        Blueprint info including variables, functions, graphs, and metadata
    """
    params = {
        "blueprint_path": blueprint_path,
        "include_variables": include_variables,
        "include_functions": include_functions,
        "include_widget_tree": include_widget_tree
    }
    return send_command("get_editor_widget_blueprint_info", params)


@with_unreal_connection
def update_blueprint(
    blueprint_path: str,
    properties: dict = None,
    compile_after: bool = False
) -> Dict[str, Any]:
    """
    Update blueprint properties.
    
    Args:
        blueprint_path: Full path to the blueprint asset
        properties: Blueprint properties to set (handled via UE reflection)
        compile_after: Whether to compile after updating
    
    Returns:
        Success status and any error messages
    """
    params = {"blueprint_path": blueprint_path, "compile_after": compile_after}
    if properties:
        params["properties"] = properties
    return send_command("update_editor_widget_blueprint", params)
