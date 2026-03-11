#!/usr/bin/env python3
"""Test import functionality in UE"""

import unreal
import os

def import_texture_test():
    """Test texture import"""
    texture_path = r"C:\Users\LK867\Desktop\3D\mesh_Model_1_u1_v1_diffuse.png"
    destination = "/Game/Imported/Textures"
    
    # Ensure folder exists
    if not unreal.EditorAssetLibrary.does_directory_exist(destination):
        unreal.EditorAssetLibrary.make_directory(destination)
    
    # Create import task
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', texture_path)
    task.set_editor_property('destination_path', destination)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    
    # Execute
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])
    
    # Check result
    texture_name = os.path.splitext(os.path.basename(texture_path))[0]
    full_path = f"{destination}/{texture_name}.{texture_name}"
    
    imported = unreal.EditorAssetLibrary.load_asset(full_path)
    if imported:
        print(f"✓ Texture imported: {full_path}")
        return True
    else:
        print(f"✗ Texture failed: {full_path}")
        return False

def import_mesh_test():
    """Test mesh import"""
    mesh_path = r"C:\Users\LK867\Desktop\3D\mesh_Model_1.obj"
    destination = "/Game/Imported/Meshes"
    
    # Ensure folder exists
    if not unreal.EditorAssetLibrary.does_directory_exist(destination):
        unreal.EditorAssetLibrary.make_directory(destination)
    
    # Create import task
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', mesh_path)
    task.set_editor_property('destination_path', destination)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    
    # Execute
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])
    
    # Check result
    mesh_name = os.path.splitext(os.path.basename(mesh_path))[0]
    full_path = f"{destination}/{mesh_name}.{mesh_name}"
    
    imported = unreal.EditorAssetLibrary.load_asset(full_path)
    if imported:
        print(f"✓ Mesh imported: {full_path}")
        return True
    else:
        print(f"✗ Mesh failed: {full_path}")
        return False

# Run tests
print("=" * 50)
print("Testing Import Functionality")
print("=" * 50)
import_texture_test()
import_mesh_test()
print("=" * 50)
