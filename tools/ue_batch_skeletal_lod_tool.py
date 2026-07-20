import traceback

import unreal


MENU_SECTION = "ProjectRBatchTools"
MENU_ENTRY = "ProjectRBatchSkeletalLOD"
MENU_LABEL = "Batch Skeletal Mesh LOD..."


@unreal.uclass()
class ProjectRBatchSkeletalLODSettings(unreal.Object):
    target_lod_count = unreal.uproperty(
        int,
        meta={
            "DisplayName": "Target LOD Count",
            "ClampMin": "1",
            "UIMin": "1",
            "ClampMax": "8",
            "UIMax": "8",
            "ToolTip": "New LOD count to apply to every selected Skeletal Mesh.",
        },
    )
    save_assets = unreal.uproperty(
        bool,
        meta={
            "DisplayName": "Save Assets",
            "ToolTip": "Save changed Skeletal Mesh assets after regenerating LODs.",
        },
    )
    checkout_assets = unreal.uproperty(
        bool,
        meta={
            "DisplayName": "Checkout Assets",
            "ToolTip": "Try to checkout assets from source control before modifying them.",
        },
    )
    regenerate_imported_lods = unreal.uproperty(
        bool,
        meta={
            "DisplayName": "Regenerate Imported LODs",
            "ToolTip": "Overwrite imported/custom LODs as well. Leave off to preserve manually imported LODs.",
        },
    )
    generate_base_lod = unreal.uproperty(
        bool,
        meta={
            "DisplayName": "Generate Base LOD",
            "ToolTip": "Allow the base LOD to be reduced if the mesh has base LOD reduction data.",
        },
    )


def _make_settings():
    settings = ProjectRBatchSkeletalLODSettings()
    settings.set_editor_property("target_lod_count", 4)
    settings.set_editor_property("save_assets", True)
    settings.set_editor_property("checkout_assets", True)
    settings.set_editor_property("regenerate_imported_lods", False)
    settings.set_editor_property("generate_base_lod", False)
    return settings


def _selected_skeletal_meshes():
    meshes = []
    skipped = []
    seen_paths = set()

    for asset in unreal.EditorUtilityLibrary.get_selected_assets():
        if isinstance(asset, unreal.SkeletalMesh):
            path = asset.get_path_name()
            if path not in seen_paths:
                seen_paths.add(path)
                meshes.append(asset)
        else:
            skipped.append(asset.get_path_name())

    return meshes, skipped


def _show_message(title, message, message_type=unreal.AppMsgType.OK):
    return unreal.EditorDialog.show_message(title, message, message_type)


def _show_settings_dialog(settings):
    options = unreal.EditorDialogLibraryObjectDetailsViewOptions()
    options.set_editor_property("show_object_name", False)
    options.set_editor_property("allow_search", False)
    options.set_editor_property("allow_resizing", True)
    options.set_editor_property("min_width", 460)
    options.set_editor_property("min_height", 240)
    options.set_editor_property("value_column_width_ratio", 0.55)
    return unreal.EditorDialog.show_object_details_view(
        "Batch Skeletal Mesh LOD",
        settings,
        options,
    )


def _get_lod_count(mesh):
    try:
        return unreal.SkeletalMeshEditorSubsystem.get_lod_count(mesh)
    except Exception:
        return -1


def _apply_to_mesh(mesh, settings):
    target_lod_count = settings.get_editor_property("target_lod_count")
    checkout_assets = settings.get_editor_property("checkout_assets")
    save_assets = settings.get_editor_property("save_assets")
    regenerate_imported_lods = settings.get_editor_property("regenerate_imported_lods")
    generate_base_lod = settings.get_editor_property("generate_base_lod")

    before_count = _get_lod_count(mesh)

    if checkout_assets:
        unreal.EditorAssetLibrary.checkout_loaded_asset(mesh)

    mesh.modify()
    ok = unreal.SkeletalMeshEditorSubsystem.regenerate_lod(
        mesh,
        target_lod_count,
        regenerate_imported_lods,
        generate_base_lod,
    )
    after_count = _get_lod_count(mesh)

    if ok:
        if save_assets:
            if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, False):
                raise RuntimeError("regenerate_lod succeeded, but save_loaded_asset returned False")

    return ok, before_count, after_count


def apply_to_selected(settings):
    meshes, skipped = _selected_skeletal_meshes()
    if not meshes:
        _show_message(
            "Batch Skeletal Mesh LOD",
            "No Skeletal Mesh assets are selected in the Content Browser.",
        )
        return {"processed": 0, "success": [], "failed": [], "skipped": skipped}

    target_lod_count = settings.get_editor_property("target_lod_count")
    if target_lod_count < 1:
        _show_message("Batch Skeletal Mesh LOD", "Target LOD Count must be at least 1.")
        return {"processed": 0, "success": [], "failed": [], "skipped": skipped}

    success = []
    failed = []

    with unreal.ScopedSlowTask(len(meshes), "Batch regenerating Skeletal Mesh LODs") as slow_task:
        slow_task.make_dialog(True)

        for mesh in meshes:
            if slow_task.should_cancel():
                break

            path = mesh.get_path_name()
            slow_task.enter_progress_frame(1, "Regenerating LODs: {0}".format(mesh.get_name()))

            try:
                ok, before_count, after_count = _apply_to_mesh(mesh, settings)
                item = {
                    "path": path,
                    "before_lod_count": before_count,
                    "after_lod_count": after_count,
                    "target_lod_count": target_lod_count,
                }

                if ok:
                    success.append(item)
                    unreal.log(
                        "[Batch Skeletal Mesh LOD] OK: {0} ({1} -> {2})".format(
                            path,
                            before_count,
                            after_count,
                        )
                    )
                else:
                    item["error"] = "regenerate_lod returned False"
                    failed.append(item)
                    unreal.log_error(
                        "[Batch Skeletal Mesh LOD] Failed: {0} ({1} -> {2})".format(
                            path,
                            before_count,
                            after_count,
                        )
                    )
            except Exception as exc:
                failed.append({"path": path, "error": str(exc)})
                unreal.log_error("[Batch Skeletal Mesh LOD] Exception on {0}: {1}".format(path, exc))
                unreal.log_error(traceback.format_exc())

    summary = [
        "Target LOD Count: {0}".format(target_lod_count),
        "Succeeded: {0}".format(len(success)),
        "Failed: {0}".format(len(failed)),
    ]

    if skipped:
        summary.append("Skipped non-SkeletalMesh assets: {0}".format(len(skipped)))

    if failed:
        summary.append("")
        summary.append("Failed assets:")
        for item in failed[:10]:
            summary.append("- {0}: {1}".format(item.get("path"), item.get("error", "Unknown error")))
        if len(failed) > 10:
            summary.append("- ... {0} more".format(len(failed) - 10))

    _show_message("Batch Skeletal Mesh LOD", "\n".join(summary))

    return {
        "processed": len(success) + len(failed),
        "success": success,
        "failed": failed,
        "skipped": skipped,
    }


def open_dialog():
    settings = _make_settings()
    if not _show_settings_dialog(settings):
        return None
    return apply_to_selected(settings)


def _add_menu_entry(menu_name):
    menus = unreal.ToolMenus.get()
    menu = menus.extend_menu(menu_name)
    menu.add_section(MENU_SECTION, "ProjectR")

    entry = unreal.ToolMenuEntry(
        name=MENU_ENTRY,
        type=unreal.MultiBlockType.MENU_ENTRY,
    )
    entry.set_label(MENU_LABEL)
    entry.set_tool_tip("Set the LOD count for all selected Skeletal Mesh assets.")
    entry.set_string_command(
        unreal.ToolMenuStringCommandType.PYTHON,
        "",
        "import batch_skeletal_lod_tool; batch_skeletal_lod_tool.open_dialog()",
    )
    menu.add_menu_entry(MENU_SECTION, entry)


def register_menu():
    menus = unreal.ToolMenus.get()

    for menu_name in (
        "LevelEditor.MainMenu.Tools",
        "ContentBrowser.AssetContextMenu.SkeletalMesh",
    ):
        try:
            menus.remove_entry(menu_name, MENU_SECTION, MENU_ENTRY)
        except Exception:
            pass
        _add_menu_entry(menu_name)

    menus.refresh_all_widgets()
    unreal.log("[Batch Skeletal Mesh LOD] Menu registered.")


register_menu()
