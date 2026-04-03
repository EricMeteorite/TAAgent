#!/usr/bin/env python3
"""Manual Unreal asset import checks."""

from __future__ import annotations

import os


def _require_unreal():
    import unreal

    if not hasattr(unreal, "EditorAssetLibrary"):
        raise RuntimeError("This script must run inside Unreal's embedded Python environment.")
    return unreal


def _get_required_test_asset_path(env_var_name: str, asset_kind: str) -> str:
    asset_path = os.environ.get(env_var_name)
    if asset_path:
        return asset_path

    raise RuntimeError(
        f"Set {env_var_name} to a local {asset_kind} file path before running this manual import test."
    )


def import_texture_test() -> bool:
    """Test texture import."""
    unreal = _require_unreal()
    texture_path = _get_required_test_asset_path("TAAGENT_TEST_TEXTURE_PATH", "texture")
    destination = "/Game/Imported/Textures"

    if not unreal.EditorAssetLibrary.does_directory_exist(destination):
        unreal.EditorAssetLibrary.make_directory(destination)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", texture_path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    texture_name = os.path.splitext(os.path.basename(texture_path))[0]
    full_path = f"{destination}/{texture_name}.{texture_name}"

    imported = unreal.EditorAssetLibrary.load_asset(full_path)
    if imported:
        print(f"Texture imported: {full_path}")
        return True

    print(f"Texture import failed: {full_path}")
    return False


def import_mesh_test() -> bool:
    """Test mesh import."""
    unreal = _require_unreal()
    mesh_path = _get_required_test_asset_path("TAAGENT_TEST_MESH_PATH", "mesh")
    destination = "/Game/Imported/Meshes"

    if not unreal.EditorAssetLibrary.does_directory_exist(destination):
        unreal.EditorAssetLibrary.make_directory(destination)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", mesh_path)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    mesh_name = os.path.splitext(os.path.basename(mesh_path))[0]
    full_path = f"{destination}/{mesh_name}.{mesh_name}"

    imported = unreal.EditorAssetLibrary.load_asset(full_path)
    if imported:
        print(f"Mesh imported: {full_path}")
        return True

    print(f"Mesh import failed: {full_path}")
    return False


def main() -> None:
    print("=" * 50)
    print("Testing import functionality")
    print("=" * 50)
    import_texture_test()
    import_mesh_test()
    print("=" * 50)


if __name__ == "__main__":
    main()
