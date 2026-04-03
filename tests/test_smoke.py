from __future__ import annotations

import importlib
import py_compile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def test_manual_scripts_compile() -> None:
    for relative_path in (
        "unreal/agent-harness/init_unreal.py",
        "test_screenshot_rgb.py",
        "unreal/agent-harness/auto_reload_system.py",
        "unreal/agent-harness/test_direct_udp.py",
        "unreal/agent-harness/test_import.py",
        "unreal/agent-harness/test_ue_direct.py",
        "unreal/agent-harness/test_ue_simple.py",
    ):
        py_compile.compile(str(REPO_ROOT / relative_path), doraise=True)


def test_pyproject_declares_fastmcp_dependency() -> None:
    pyproject = (REPO_ROOT / "pyproject.toml").read_text(encoding="utf-8")
    assert '"fastmcp' in pyproject


def test_socket_server_import_is_safe_without_pyside2() -> None:
    module = importlib.import_module("src.extension.socket_server")
    assert hasattr(module, "MCPBridgeServer")


def test_editor_tools_export_context_controls() -> None:
    module = importlib.import_module("mcps.unreal_render_mcp.tools.editor")
    for attr in (
        "get_editor_context",
        "get_open_asset_editors",
        "get_selected_assets",
        "get_selected_actors",
        "open_asset",
        "focus_asset_editor",
        "close_asset_editors",
        "save_asset",
    ):
        assert hasattr(module, attr)


def test_niagara_tools_export_bake_command() -> None:
    module = importlib.import_module("mcps.unreal_render_mcp.tools.niagara")
    assert hasattr(module, "bake_niagara_system")


def test_asset_validation_plugin_exists() -> None:
    asset_validation_uplugin = (
        REPO_ROOT
        / "plugins"
        / "unreal"
        / "UnrealMCP"
        / "RenderingMCP"
        / "Plugins"
        / "AssetValidation"
        / "AssetValidation.uplugin"
    )
    assert asset_validation_uplugin.exists()


def test_no_legacy_codebuddy_paths_in_runtime_entrypoints() -> None:
    checked_files = (
        REPO_ROOT / "unreal" / "agent-harness" / "init_unreal.py",
        REPO_ROOT / "unreal" / "agent-harness" / "cli_anything" / "unreal" / "file_commander.py",
        REPO_ROOT / "plugins" / "unreal" / "UnrealMCP" / "RenderingMCP" / "Plugins" / "AssetValidation" / "AssetValidation.uplugin",
    )

    for checked_file in checked_files:
        content = checked_file.read_text(encoding="utf-8")
        assert "CodeBuddy" not in content
        assert "rendering-mcp" not in content
