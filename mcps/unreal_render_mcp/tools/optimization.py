"""
Optimization Analysis Tools for Unreal Render MCP

High-level helpers that combine post process and material data for profiling workflows.
"""

from typing import Any, Dict, Optional
import logging

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from common import with_unreal_connection
from editor import get_current_level, get_current_level_post_process_overview
from material import get_material_analysis, get_material_instance_details

logger = logging.getLogger("UnrealRenderMCP")


def _unwrap_result(response: Dict[str, Any]) -> Dict[str, Any]:
    if isinstance(response, dict) and isinstance(response.get("result"), dict):
        return response["result"]
    return response if isinstance(response, dict) else {}


@with_unreal_connection
def analyze_current_level_post_process(
    level_filter: Optional[str] = None,
    include_material_analysis: bool = True,
    include_instance_details: bool = True,
) -> Dict[str, Any]:
    """
    Build a consolidated post process optimization report for the current level.

    Args:
        level_filter: Optional level/sublevel substring, for example "Channel_Light4".
        include_material_analysis: Whether to attach graph summaries for blendable materials.
        include_instance_details: Whether to attach resolved material instance parameters.

    Returns:
        Consolidated optimization report for all post process volumes in the current level.
    """
    current_level = _unwrap_result(get_current_level())
    overview = _unwrap_result(get_current_level_post_process_overview(level_filter=level_filter))

    if not overview.get("success"):
        return overview

    seen_analysis: Dict[str, Dict[str, Any]] = {}
    seen_instance_details: Dict[str, Dict[str, Any]] = {}

    for volume in overview.get("volumes", []):
        for blendable in volume.get("weighted_blendables", []):
            asset_path = blendable.get("asset_path")
            object_class = blendable.get("object_class", "")
            analysis_target = blendable.get("analysis_target_path") or blendable.get("base_material_path") or asset_path

            if include_instance_details and asset_path and "MaterialInstance" in object_class:
                if asset_path not in seen_instance_details:
                    seen_instance_details[asset_path] = _unwrap_result(get_material_instance_details(asset_path))
                blendable["material_instance_details"] = seen_instance_details[asset_path]
                analysis_target = (
                    seen_instance_details[asset_path].get("base_material_path")
                    or seen_instance_details[asset_path].get("parent_path")
                    or analysis_target
                )

            if include_material_analysis and analysis_target:
                if analysis_target not in seen_analysis:
                    seen_analysis[analysis_target] = _unwrap_result(get_material_analysis(analysis_target))
                blendable["material_analysis"] = seen_analysis[analysis_target]

    return {
        "success": True,
        "current_level": current_level,
        "post_process_overview": overview,
        "level_filter": level_filter,
        "material_analysis_count": len(seen_analysis),
        "material_instance_detail_count": len(seen_instance_details),
    }