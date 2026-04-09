"""
Unreal Render MCP Tools

Modular tool definitions for Unreal Engine operations.
"""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from material import (
    build_material_graph,
    get_material_graph,
    get_material_analysis,
    get_material_instance_details,
)

from texture import (
    import_texture,
)

from mesh import (
    import_fbx,
)

from actor import (
    spawn_actor,
    delete_actor,
    get_actors,
    set_actor_properties,
    get_actor_properties,
    batch_spawn_actors,
    batch_delete_actors,
    batch_set_actors_properties,
)

from asset import (
    create_asset,
    delete_asset,
    set_asset_properties,
    get_asset_properties,
    get_assets,
    batch_create_assets,
    batch_set_assets_properties,
)

from niagara import (
    get_niagara_graph,
    update_niagara_graph,
    get_niagara_emitter,
    update_niagara_emitter,
    get_niagara_compiled_code,
    get_niagara_particle_attributes,
    bake_niagara_system,
)

from viewport import (
    get_viewport_screenshot,
    set_viewport_camera,
    get_viewport_camera,
)

from editor import (
    get_editor_context,
    get_open_asset_editors,
    get_selected_assets,
    get_selected_actors,
    execute_unreal_python,
    get_post_process_volume_details,
    get_current_level_post_process_overview,
    open_asset,
    focus_asset_editor,
    close_asset_editors,
    save_asset,
    create_level,
    load_level,
    save_current_level,
    get_current_level,
)

from optimization import (
    analyze_current_level_post_process,
)

from blueprint import (
    get_blueprint_info,
    update_blueprint,
)

__all__ = [
    # Material
    "build_material_graph",
    "get_material_graph",
    "get_material_analysis",
    "get_material_instance_details",
    # Texture
    "import_texture",
    # Mesh
    "import_fbx",
    # Actor
    "spawn_actor",
    "delete_actor",
    "get_actors",
    "set_actor_properties",
    "get_actor_properties",
    "batch_spawn_actors",
    "batch_delete_actors",
    "batch_set_actors_properties",
    # Asset
    "create_asset",
    "delete_asset",
    "set_asset_properties",
    "get_asset_properties",
    "get_assets",
    "batch_create_assets",
    "batch_set_assets_properties",
    # Niagara
    "get_niagara_graph",
    "update_niagara_graph",
    "get_niagara_emitter",
    "update_niagara_emitter",
    "get_niagara_compiled_code",
    "get_niagara_particle_attributes",
    "bake_niagara_system",
    # Viewport
    "get_viewport_screenshot",
    "set_viewport_camera",
    "get_viewport_camera",
    # Editor
    "get_editor_context",
    "get_open_asset_editors",
    "get_selected_assets",
    "get_selected_actors",
    "execute_unreal_python",
    "get_post_process_volume_details",
    "get_current_level_post_process_overview",
    "open_asset",
    "focus_asset_editor",
    "close_asset_editors",
    "save_asset",
    "create_level",
    "load_level",
    "save_current_level",
    "get_current_level",
    # Optimization
    "analyze_current_level_post_process",
    # Blueprint
    "get_blueprint_info",
    "update_blueprint",
]
