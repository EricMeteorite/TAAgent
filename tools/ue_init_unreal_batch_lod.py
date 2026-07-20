try:
    import batch_skeletal_lod_tool  # noqa: F401
except Exception as exc:
    import unreal
    unreal.log_error("[Batch Skeletal Mesh LOD] Failed to register menu: {0}".format(exc))
