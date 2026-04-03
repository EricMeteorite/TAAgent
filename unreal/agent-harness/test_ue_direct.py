#!/usr/bin/env python3
"""Manual test for running a Python script inside Unreal Editor."""

from __future__ import annotations

import os
import subprocess
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


def test_ue() -> None:
    script = """
import os
import sys
import unreal

sys.stdout.reconfigure(line_buffering=True)

print("=" * 60)
print("UE Python Environment Test")
print("=" * 60, flush=True)

try:
    version = unreal.SystemLibrary.get_engine_version()
    print(f"UE Version: {version}", flush=True)
except Exception as exc:
    print(f"Version check failed: {exc}", flush=True)

try:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        print(f"Current World: {world.get_name()}", flush=True)
    else:
        print("No world loaded", flush=True)
except Exception as exc:
    print(f"World check failed: {exc}", flush=True)

print("=" * 60)
print("Test Complete!", flush=True)
print("=" * 60)
os._exit(0)
"""

    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False, encoding="utf-8") as handle:
        handle.write(script)
        script_path = handle.name

    print(f"Script path: {script_path}")
    print(f"UE path: {UE_PATH}")
    print(f"Project path: {PROJECT_PATH}")

    if not os.path.exists(UE_PATH):
        print(f"ERROR: UE not found at {UE_PATH}")
        return

    if not os.path.exists(PROJECT_PATH):
        print(f"ERROR: Project not found at {PROJECT_PATH}")
        return

    cmd = [
        UE_PATH,
        PROJECT_PATH,
        f"-ExecutePythonScript={script_path}",
        "-unattended",
        "-nopause",
        "-log",
        "-stdout",
        "-forcelogflush",
        "-RenderOffscreen",
    ]

    print(f"\nExecuting: {' '.join(cmd)}")
    print("-" * 60)

    try:
        env = os.environ.copy()
        env["PYTHONIOENCODING"] = "utf-8"
        env["PYTHONUTF8"] = "1"

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            encoding="utf-8",
            errors="replace",
            env=env,
        )

        print("=== STDOUT ===")
        print(result.stdout)
        print("\n=== STDERR ===")
        print(result.stderr)
        print(f"\nExit code: {result.returncode}")
    except subprocess.TimeoutExpired:
        print("ERROR: Timeout after 120 seconds")
    except Exception as exc:
        print(f"ERROR: {exc}")
    finally:
        try:
            os.unlink(script_path)
            print(f"\nCleaned up: {script_path}")
        except OSError:
            pass


if __name__ == "__main__":
    test_ue()
