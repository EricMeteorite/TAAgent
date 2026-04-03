#!/usr/bin/env python3
"""Send Python snippets to the legacy UE file-command listener."""

from __future__ import annotations

import json
import os
import time
from pathlib import Path


def _default_command_dir() -> Path:
    """Resolve the command directory for the current TAAgent checkout."""
    env_dir = os.environ.get("UE_CLI_COMMAND_DIR")
    if env_dir:
        return Path(env_dir)

    repo_root = Path(__file__).resolve().parents[4]
    return repo_root / ".taagent-local" / "tmp" / "ue_cli_commands"


COMMAND_DIR = _default_command_dir()
COMMAND_FILE = COMMAND_DIR / "command.py"
RESULT_FILE = COMMAND_DIR / "result.json"
LOCK_FILE = COMMAND_DIR / "lock"


def ensure_dir() -> None:
    """Ensure the command directory exists."""
    COMMAND_DIR.mkdir(parents=True, exist_ok=True)


def execute_command(script: str, timeout: float = 30.0) -> dict:
    """Execute a Python command through the file-based UE listener."""
    ensure_dir()

    start = time.time()
    while LOCK_FILE.exists() and (time.time() - start) < timeout:
        time.sleep(0.1)

    LOCK_FILE.touch()

    try:
        with open(COMMAND_FILE, "w", encoding="utf-8") as handle:
            handle.write(script)

        start = time.time()
        while not RESULT_FILE.exists() and (time.time() - start) < timeout:
            time.sleep(0.1)

        if RESULT_FILE.exists():
            with open(RESULT_FILE, "r", encoding="utf-8") as handle:
                result = json.load(handle)
            RESULT_FILE.unlink()
            return result

        return {"status": "timeout", "message": "Timed out waiting for result"}
    finally:
        if LOCK_FILE.exists():
            LOCK_FILE.unlink()


def test():
    """Send a small test command to Unreal."""
    script = """
import unreal
import json

result = {
    "status": "success",
    "engine_version": unreal.SystemLibrary.get_engine_version(),
    "project_name": unreal.SystemLibrary.get_project_name(),
}
"""

    result = execute_command(script)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return result


if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1:
        script = " ".join(sys.argv[1:])
        result = execute_command(script)
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        test()
