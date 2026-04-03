#!/usr/bin/env python3
r"""UE command listener for running Python snippets inside Unreal Editor.

Usage from the Unreal Python console:
1. ``exec(open(r'D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/init_unreal.py').read())``
2. ``start_listener()``
"""

from __future__ import annotations

import json
import os
import threading
import time
from pathlib import Path

import unreal


def _default_command_dir() -> Path:
    """Resolve the command directory for the current TAAgent checkout."""
    env_dir = os.environ.get("UE_CLI_COMMAND_DIR")
    if env_dir:
        return Path(env_dir)

    repo_root = Path(__file__).resolve().parents[2]
    return repo_root / ".taagent-local" / "tmp" / "ue_cli_commands"


COMMAND_DIR = _default_command_dir()
COMMAND_FILE = COMMAND_DIR / "command.py"
RESULT_FILE = COMMAND_DIR / "result.json"
LOCK_FILE = COMMAND_DIR / "lock"

_running = False


def ensure_dir() -> None:
    """Ensure the command directory exists."""
    COMMAND_DIR.mkdir(parents=True, exist_ok=True)


def execute_command_file():
    """Execute the command script if present and persist the result JSON."""
    ensure_dir()

    if not COMMAND_FILE.exists():
        return None

    try:
        with open(COMMAND_FILE, "r", encoding="utf-8") as handle:
            script = handle.read()

        COMMAND_FILE.unlink()

        local_vars = {"unreal": unreal}
        exec_result: dict[str, object] = {}

        try:
            exec(script, local_vars, exec_result)
            if "result" in exec_result:
                result = exec_result["result"]
            else:
                result = {"status": "success", "message": "Command executed successfully"}
        except Exception as exc:  # pragma: no cover - runs inside Unreal
            result = {"status": "error", "message": str(exc)}

        with open(RESULT_FILE, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=2, ensure_ascii=False)

        return result
    except Exception as exc:  # pragma: no cover - runs inside Unreal
        return {"status": "error", "message": f"Execution failed: {exc}"}


def listener_loop(interval: float = 0.5) -> None:
    """Poll the command directory and execute incoming scripts."""
    global _running
    _running = True

    print(f"[UE Listener] Listening for command files in: {COMMAND_FILE}")

    while _running:
        try:
            if COMMAND_FILE.exists() and not LOCK_FILE.exists():
                print("[UE Listener] Command file detected, executing...")
                result = execute_command_file()
                print(f"[UE Listener] Result: {result}")
        except Exception as exc:  # pragma: no cover - runs inside Unreal
            print(f"[UE Listener] Error: {exc}")

        time.sleep(interval)


def start_listener(interval: float = 0.5) -> None:
    """Start the background listener thread."""
    global _running

    if _running:
        print("[UE Listener] Listener is already running")
        return

    thread = threading.Thread(target=listener_loop, args=(interval,), daemon=True)
    thread.start()

    print(f"[UE Listener] Listener started (thread id: {thread.ident})")


def stop_listener() -> None:
    """Stop the background listener thread."""
    global _running
    _running = False
    print("[UE Listener] Listener stopped")


def test() -> None:
    """Print a small Unreal environment diagnostic."""
    print("=" * 60)
    print("UE Python Environment Test")
    print("=" * 60)

    print(f"Engine version: {unreal.SystemLibrary.get_engine_version()}")
    print(f"Project name: {unreal.SystemLibrary.get_project_name()}")
    print(f"Project directory: {unreal.SystemLibrary.get_project_directory()}")

    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        print(f"Current world: {world.get_name()}")
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        print(f"Actor count: {len(actors)}")
    else:
        print("No world loaded")

    print("=" * 60)


if __name__ == "__main__":
    test()
