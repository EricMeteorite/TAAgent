#!/usr/bin/env python3
"""Auto-reload system for the UE CLI listener.

Run this inside Unreal Editor's Python console. It monitors command execution,
captures errors, and reloads the listener module when needed.
"""

from __future__ import annotations

import json
import os
import time
import traceback
from pathlib import Path

import unreal


def _resolve_listener_file() -> Path:
    configured = os.environ.get("TAAGENT_UE_LISTENER_FILE")
    if configured:
        return Path(configured).resolve()

    current_file = globals().get("__file__")
    if current_file:
        return Path(current_file).resolve().with_name("ue_cli_listener_full.py")

    return Path.cwd() / "ue_cli_listener_full.py"


TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"))
)
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"
LOG_FILE = TEMP_DIR / "ue_cli_log.json"
LISTENER_FILE = _resolve_listener_file()

TEMP_DIR.mkdir(parents=True, exist_ok=True)

_auto_reload_running = False
_last_error = None
_command_count = 0
_original_module = None


def log_message(level, message, data=None):
    """Log a message to the JSON log file for external monitoring."""
    log_entry = {
        "timestamp": time.time(),
        "level": level,
        "message": message,
        "data": data,
    }
    try:
        logs = []
        if LOG_FILE.exists():
            with open(LOG_FILE, "r", encoding="utf-8") as handle:
                try:
                    logs = json.load(handle)
                except Exception:
                    logs = []

        logs.append(log_entry)
        if len(logs) > 100:
            logs = logs[-100:]

        with open(LOG_FILE, "w", encoding="utf-8") as handle:
            json.dump(logs, handle, indent=2, default=str)
    except Exception as exc:  # pragma: no cover - runs inside Unreal
        print(f"[AutoReload] Failed to write log: {exc}")


def get_module_source():
    """Get the source code of the listener module."""
    if LISTENER_FILE.exists():
        with open(LISTENER_FILE, "r", encoding="utf-8") as handle:
            return handle.read()
    return None


def check_for_errors(result):
    """Check whether a command result indicates a code-level error."""
    if not isinstance(result, dict):
        return False

    error = result.get("error", "")
    if not error:
        return False

    error_patterns = [
        "has no attribute",
        "object has no attribute",
        "Failed to find property",
        "SyntaxError",
        "ImportError",
        "ModuleNotFoundError",
        "NameError",
    ]

    return any(pattern in str(error) for pattern in error_patterns)


def _build_listener_namespace():
    return {
        "__name__": "__main__",
        "__file__": str(LISTENER_FILE),
        "unreal": unreal,
        "json": json,
        "os": os,
        "time": time,
        "Path": Path,
        "traceback": traceback,
    }


def reload_listener():
    """Reload the listener module."""
    global _original_module

    try:
        log_message("INFO", "Reloading listener module...")

        source = get_module_source()
        if not source:
            log_message("ERROR", f"Could not read listener source: {LISTENER_FILE}")
            return False

        namespace = _build_listener_namespace()
        exec(source, namespace)

        command_dict = namespace.get("COMMANDS") or namespace.get("COMMAND_MAP")
        if command_dict:
            _original_module = namespace
            log_message("SUCCESS", "Listener module reloaded successfully", {"commands": len(command_dict)})
            return True

        log_message("ERROR", "Listener module loaded but no commands were registered")
        return False
    except Exception as exc:  # pragma: no cover - runs inside Unreal
        log_message("ERROR", f"Failed to reload: {exc}", {"traceback": traceback.format_exc()})
        return False


def auto_reload_tick():
    """Tick function that checks for commands and handles auto-reload."""
    global _auto_reload_running, _last_error, _command_count

    if not _auto_reload_running:
        return

    try:
        if COMMAND_FILE.exists():
            with open(COMMAND_FILE, "r", encoding="utf-8") as handle:
                command_data = json.load(handle)

            command_type = command_data.get("type")
            command_id = command_data.get("id")
            params = command_data.get("params", {})

            log_message("INFO", f"Processing command: {command_type}", {"id": command_id})

            if _original_module:
                command_map = _original_module.get("COMMANDS") or _original_module.get("COMMAND_MAP", {})
            else:
                import __main__

                command_map = getattr(__main__, "COMMANDS", None) or getattr(__main__, "COMMAND_MAP", {})

            result = None

            if command_type in command_map:
                try:
                    result = command_map[command_type](params)
                    _command_count += 1

                    if check_for_errors(result):
                        _last_error = result.get("error")
                        log_message(
                            "WARN",
                            "Error detected, attempting auto-reload",
                            {"error": _last_error, "command": command_type},
                        )

                        if reload_listener() and _original_module:
                            command_map = _original_module.get("COMMANDS") or _original_module.get("COMMAND_MAP", {})
                            if command_type in command_map:
                                result = command_map[command_type](params)
                                result["retried_after_reload"] = True
                except Exception as exc:
                    _last_error = str(exc)
                    log_message(
                        "ERROR",
                        f"Command failed: {exc}",
                        {"command": command_type, "traceback": traceback.format_exc()},
                    )

                    if reload_listener() and _original_module:
                        try:
                            command_map = _original_module.get("COMMANDS") or _original_module.get("COMMAND_MAP", {})
                            if command_type in command_map:
                                result = command_map[command_type](params)
                                result["retried_after_reload"] = True
                        except Exception as retry_exc:
                            result = {
                                "success": False,
                                "error": str(retry_exc),
                                "original_error": _last_error,
                            }
                    else:
                        result = {
                            "success": False,
                            "error": _last_error,
                            "traceback": traceback.format_exc(),
                        }
            else:
                result = {"success": False, "error": f"Unknown command: {command_type}"}

            result_data = {
                "id": command_id,
                "type": command_type,
                "result": result,
            }

            with open(RESULT_FILE, "w", encoding="utf-8") as handle:
                json.dump(result_data, handle, indent=2, default=str)

            try:
                COMMAND_FILE.unlink()
            except Exception:
                pass

            log_message("INFO", f"Command completed: {command_type}", {"success": result.get("success", False)})
    except Exception as exc:  # pragma: no cover - runs inside Unreal
        _last_error = str(exc)
        log_message("ERROR", f"Tick error: {exc}", {"traceback": traceback.format_exc()})


def start_auto_reload():
    """Start the auto-reload system."""
    global _auto_reload_running, _original_module

    if _auto_reload_running:
        print("[AutoReload] Already running")
        return

    source = get_module_source()
    if source:
        namespace = _build_listener_namespace()
        try:
            exec(source, namespace)
            _original_module = namespace
            command_dict = namespace.get("COMMANDS") or namespace.get("COMMAND_MAP", {})
            cmd_count = len(command_dict)
            print(f"[AutoReload] Loaded {cmd_count} commands from {LISTENER_FILE}")
            if cmd_count == 0:
                print("[AutoReload] WARNING: no commands loaded")
        except Exception as exc:
            print(f"[AutoReload] Failed to load module: {exc}")
            print(traceback.format_exc())

    _auto_reload_running = True

    def tick_callback(delta_time):
        if _auto_reload_running:
            auto_reload_tick()

    unreal.register_slate_post_tick_callback(tick_callback)

    print("[AutoReload] Auto-reload system started")
    print("[AutoReload] Commands will auto-retry on matching code errors")
    log_message("INFO", "Auto-reload system started", {"listener_file": str(LISTENER_FILE)})


def stop_auto_reload():
    """Stop the auto-reload system."""
    global _auto_reload_running
    _auto_reload_running = False
    print("[AutoReload] Stopped")
    log_message("INFO", "Auto-reload system stopped")


def get_status():
    """Get the current status."""
    return {
        "running": _auto_reload_running,
        "commands_processed": _command_count,
        "last_error": _last_error,
        "module_loaded": _original_module is not None,
        "listener_file": str(LISTENER_FILE),
    }


print("=" * 50)
print("UE CLI Auto-Reload System")
print("=" * 50)
print(f"Listener file: {LISTENER_FILE}")
print("Usage:")
print("  start_auto_reload()  - Start auto-reload system")
print("  stop_auto_reload()   - Stop auto-reload system")
print("  get_status()         - Get current status")
print("=" * 50)
