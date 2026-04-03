#!/usr/bin/env python3
"""Unreal Engine CLI Listener Full - Auto-Reload Version

Usage:
1. Open Unreal Editor
2. Open Python Console
3. Run this file from your local TAAgent checkout, for example:
   exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener_full_auto.py").read())
4. Run: start_ue_cli()

Auto-reloads when ue_cli_listener_full.py changes.
"""

import unreal
import json
import os
import time
from pathlib import Path


def _resolve_listener_file() -> Path:
    configured = os.environ.get("TAAGENT_UE_LISTENER_FILE")
    if configured:
        return Path(configured)

    current_file = globals().get("__file__")
    if current_file:
        return Path(current_file).resolve().with_name("ue_cli_listener_full.py")

    return Path.cwd() / "ue_cli_listener_full.py"


TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"))
)
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"
LISTENER_FILE = _resolve_listener_file()

TEMP_DIR.mkdir(parents=True, exist_ok=True)

print(f"[UE CLI Full Auto] Initialized")
print(f"[UE CLI Full Auto] Source: {LISTENER_FILE}")

_ue_cli_module = None
_ue_cli_tick_handle = None
_last_modified = 0


def load_listener_module():
    """Load or reload the listener module."""
    global _ue_cli_module, _last_modified
    
    try:
        with open(LISTENER_FILE, 'r') as f:
            code = f.read()
        
        module_namespace = {
            'unreal': unreal,
            'json': json,
            'os': os,
            'Path': Path,
            'TEMP_DIR': TEMP_DIR,
            'COMMAND_FILE': COMMAND_FILE,
            'RESULT_FILE': RESULT_FILE,
        }
        
        exec(code, module_namespace)
        
        _ue_cli_module = {
            'process_command': module_namespace.get('process_command'),
            'COMMANDS': module_namespace.get('COMMANDS', {}),
        }
        
        _last_modified = LISTENER_FILE.stat().st_mtime
        cmd_count = len(_ue_cli_module.get('COMMANDS', {}))
        print(f"[UE CLI Full Auto] Module loaded - {cmd_count} commands available")
        return True
        
    except Exception as e:
        print(f"[UE CLI Full Auto] Error loading module: {e}")
        return False


def check_reload():
    """Check if module needs reload."""
    global _last_modified
    
    try:
        current_modified = LISTENER_FILE.stat().st_mtime
        if current_modified > _last_modified:
            print(f"[UE CLI Full Auto] File changed, reloading...")
            return load_listener_module()
    except Exception as e:
        pass
    
    return False


def ue_cli_tick(delta_time):
    """Tick function."""
    global _ue_cli_module
    
    if int(delta_time * 100) % 60 == 0:
        check_reload()
    
    if _ue_cli_module and _ue_cli_module.get('process_command'):
        try:
            _ue_cli_module['process_command']()
        except Exception as e:
            print(f"[UE CLI Full Auto] Error: {e}")
    
    return True


def start_ue_cli():
    """Start auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        print("[UE CLI Full Auto] Already running")
        return
    
    if not _ue_cli_module:
        load_listener_module()
    
    _ue_cli_tick_handle = unreal.register_slate_post_tick_callback(ue_cli_tick)
    print("[UE CLI Full Auto] Started!")
    print("[UE CLI Full Auto] Run stop_ue_cli() to stop")


def stop_ue_cli():
    """Stop auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        unreal.unregister_slate_post_tick_callback(_ue_cli_tick_handle)
        _ue_cli_tick_handle = None
        print("[UE CLI Full Auto] Stopped")
    else:
        print("[UE CLI Full Auto] Not running")


def reload_ue_cli():
    """Manually reload."""
    load_listener_module()


load_listener_module()

print("[UE CLI Full Auto] Ready!")
print("[UE CLI Full Auto] Run start_ue_cli() to begin")
