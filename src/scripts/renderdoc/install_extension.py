"""
RenderDoc Extension Installer
Copies the extension to RenderDoc's extension directory.
"""

import os
import shutil
import sys
from pathlib import Path


def get_extension_dir():
    """Get RenderDoc extension directory."""
    configured_dir = os.environ.get("RENDERDOC_EXTENSION_DIR")
    if configured_dir:
        return Path(configured_dir)

    if sys.platform == "win32":
        appdata = os.environ.get("APPDATA")
        if appdata:
            return Path(appdata) / "qrenderdoc" / "extensions"
    else:
        home = Path.home()
        return home / ".local" / "share" / "qrenderdoc" / "extensions"

    raise RuntimeError("Cannot determine RenderDoc extension directory")


def get_extension_source():
    """Locate the extension source directory inside the repository."""
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parents[2]
    candidates = [
        project_root / "src" / "extension",
        project_root / "renderdoc_extension",
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    raise RuntimeError(
        "Extension source not found. Expected one of: %s"
        % ", ".join(str(candidate) for candidate in candidates)
    )


def install():
    """Install the extension."""
    extension_src = get_extension_source()

    # Destination directory
    ext_dir = get_extension_dir()
    ext_dir.mkdir(parents=True, exist_ok=True)

    dest = ext_dir / "renderdoc_mcp_bridge"

    # Remove existing installation
    if dest.exists():
        print("Removing existing installation at %s" % dest)
        shutil.rmtree(dest)

    # Copy extension (excluding __pycache__)
    shutil.copytree(
        extension_src,
        dest,
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "*.pyo"),
    )
    print("Extension installed to %s" % dest)
    print("  (__pycache__ directories excluded)")
    print("")
    print("Please restart RenderDoc and enable the extension in:")
    print("  Tools > Manage Extensions > RenderDoc MCP Bridge")


def uninstall():
    """Uninstall the extension"""
    ext_dir = get_extension_dir()
    dest = ext_dir / "renderdoc_mcp_bridge"

    if dest.exists():
        shutil.rmtree(dest)
        print("Extension uninstalled from %s" % dest)
    else:
        print("Extension not found at %s" % dest)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "uninstall":
        uninstall()
    else:
        install()
