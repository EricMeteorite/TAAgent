"""
Actor Tools for Unreal Render MCP

Generic actor management tools using UE reflection.
"""

from typing import Dict, Any, List, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from common import send_command, with_unreal_connection

logger = logging.getLogger("UnrealRenderMCP")


@with_unreal_connection
def spawn_actor(
    actor_class: str,
    name: str = None,
    location: dict = None,
    rotation: dict = None,
    scale: dict = None,
    properties: dict = None
) -> Dict[str, Any]:
    """
    Spawn any actor by class name. Uses UE reflection for universal actor creation.
    
    Args:
        actor_class: Actor class name (e.g., "DirectionalLight", "PointLight", "Sphere")
        name: Optional actor name
        location: Optional {"x": float, "y": float, "z": float}
        rotation: Optional {"pitch": float, "yaw": float, "roll": float}
        scale: Optional {"x": float, "y": float, "z": float}
        properties: Optional dict of properties to set
    """
    params = {"actor_class": actor_class}
    if name:
        params["name"] = name
    if location:
        params["location"] = location
    if rotation:
        params["rotation"] = rotation
    if scale:
        params["scale"] = scale
    if properties:
        params["properties"] = properties
    return send_command("spawn_actor", params)


@with_unreal_connection
def delete_actor(name: str) -> Dict[str, Any]:
    """Delete an actor by name."""
    return send_command("delete_actor", {"name": name})


@with_unreal_connection
def get_actors(
    actor_class: str = None,
    detailed: bool = False
) -> Dict[str, Any]:
    """
    Get list of actors in the level with optional filtering.
    
    Args:
        actor_class: Optional class filter
        detailed: If True, includes all editable properties
    """
    params = {"detailed": detailed}
    if actor_class:
        params["actor_class"] = actor_class
    return send_command("get_actors", params)


@with_unreal_connection
def set_actor_properties(name: str, properties: dict) -> Dict[str, Any]:
    """
    Set properties on an actor using UE reflection.
    
    Args:
        name: Actor name
        properties: Dictionary of property names and values
    """
    return send_command("set_actor_properties", {
        "name": name,
        "properties": properties
    })


@with_unreal_connection
def get_actor_properties(
    name: str,
    properties: list = None,
    max_depth: int = 1,
    include_all_properties: bool = False,
) -> Dict[str, Any]:
    """
    Get properties of an actor using UE reflection.
    
    Args:
        name: Actor name
        properties: Optional list of specific property names to get
        max_depth: Recursive reflection depth for nested UObject/UStruct values
        include_all_properties: Include non-editable reflected properties as well
    """
    params = {
        "name": name,
        "max_depth": max_depth,
        "include_all_properties": include_all_properties,
    }
    if properties:
        params["properties"] = properties
    return send_command("get_actor_properties", params)


@with_unreal_connection
def batch_spawn_actors(items: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    Batch spawn multiple actors.
    
    Args:
        items: List of actor configurations
    """
    return send_command("batch_spawn_actors", {"items": items})


@with_unreal_connection
def batch_delete_actors(names: List[str]) -> Dict[str, Any]:
    """Batch delete multiple actors by name."""
    return send_command("batch_delete_actors", {"names": names})


@with_unreal_connection
def batch_set_actors_properties(actors: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    Batch set properties on multiple actors.
    
    Args:
        actors: List of actor configurations with name and properties
    """
    return send_command("batch_set_actors_properties", {"actors": actors})
