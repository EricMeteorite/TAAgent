#!/usr/bin/env python3
"""Simple manual checks for Unreal Editor and Python commandlet support."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_PROJECT = REPO_ROOT / "plugins" / "unreal" / "UnrealMCP" / "RenderingMCP" / "RenderingMCP.uproject"


def _resolve_ue_path() -> str:
    configured_path = os.environ.get("UE_EDITOR_PATH")
    if configured_path:
        return configured_path

    for candidate in (
        r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
    ):
        if Path(candidate).exists():
            return candidate

    return "UnrealEditor.exe"


UE_PATH = _resolve_ue_path()
PROJECT_PATH = os.environ.get("TAAGENT_UE_UPROJECT", str(DEFAULT_PROJECT))


def test_ue_commandlet() -> None:
    """Test UE commandlet mode."""
    print("=" * 60)
    print("Testing UE commandlet mode")
    print("=" * 60)

    script = """
import sys
import unreal

print("UE Python Test")
print(f"Version: {unreal.SystemLibrary.get_engine_version()}")
sys.exit(0)
"""

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False, encoding="utf-8") as handle:
        handle.write(script)
        script_path = handle.name

    cmd = [
        UE_PATH,
        PROJECT_PATH,
        "-run=pythonscript",
        f"-script={script_path}",
        "-unattended",
        "-nopause",
        "-NullRHI",
        "-log",
    ]

    print(f"Command: {' '.join(cmd)}")
    print("-" * 60)

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )

        try:
            stdout, _ = proc.communicate(timeout=60)
            print(stdout)
            print(f"Exit code: {proc.returncode}")
        except subprocess.TimeoutExpired:
            proc.kill()
            print("Timed out; process was terminated")
    except Exception as exc:
        print(f"Error: {exc}")
    finally:
        os.unlink(script_path)


def check_ue_exists() -> bool:
    """Check whether the editor and test project exist."""
    print("Checking UE path...")
    if os.path.exists(UE_PATH):
        print(f"  OK: UE exists: {UE_PATH}")
    else:
        print(f"  ERROR: UE not found: {UE_PATH}")
        return False

    print("Checking project path...")
    if os.path.exists(PROJECT_PATH):
        print(f"  OK: project exists: {PROJECT_PATH}")
    else:
        print(f"  ERROR: project not found: {PROJECT_PATH}")
        return False

    return True


def test_ue_version() -> None:
    """Query UE version information."""
    print("\n" + "=" * 60)
    print("Getting UE version information")
    print("=" * 60)

    cmd = [UE_PATH, "-version"]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=10,
            encoding="utf-8",
            errors="replace",
        )
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
    except subprocess.TimeoutExpired:
        print("Timed out")
    except Exception as exc:
        print(f"Error: {exc}")


if __name__ == "__main__":
    print("UE test script")
    print("=" * 60)

    if not check_ue_exists():
        sys.exit(1)

    test_ue_version()
    # test_ue_commandlet()  # Enable manually when needed.
    print("\nDone")
