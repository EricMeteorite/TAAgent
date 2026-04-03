#!/usr/bin/env python3
"""Unreal Engine CLI Listener with Auto-Reload

This version automatically reloads when the script file changes.

To use:
1. Open Unreal Editor
2. Open Python Console
3. Run this file from your local TAAgent checkout, for example:
   exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener_auto.py").read())
4. Run: start_ue_cli()

The listener will auto-reload when you save changes to ue_cli_listener.py
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
        return Path(current_file).resolve().with_name("ue_cli_listener.py")

    return Path.cwd() / "ue_cli_listener.py"


# Configuration
TEMP_DIR = Path(
    os.environ.get("UE_CLI_TEMP_DIR", str(Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"))
)
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"
LISTENER_FILE = _resolve_listener_file()

# Ensure temp directory exists
TEMP_DIR.mkdir(parents=True, exist_ok=True)

print(f"[UE CLI Auto] Listener initialized with auto-reload")
print(f"[UE CLI Auto] Watching: {COMMAND_FILE}")
print(f"[UE CLI Auto] Source: {LISTENER_FILE}")

# Store module state
_ue_cli_module = None
_ue_cli_tick_handle = None
_last_modified = 0


def load_listener_module():
    """Load or reload the listener module."""
    global _ue_cli_module, _last_modified
    
    try:
        # Read the listener file
        with open(LISTENER_FILE, 'r') as f:
            code = f.read()
        
        # Create a new module namespace
        module_namespace = {
            'unreal': unreal,
            'json': json,
            'os': os,
            'Path': Path,
            'TEMP_DIR': TEMP_DIR,
            'COMMAND_FILE': COMMAND_FILE,
            'RESULT_FILE': RESULT_FILE,
        }
        
        # Execute the code
        exec(code, module_namespace)
        
        # Extract the functions we need
        _ue_cli_module = {
            'process_command': module_namespace.get('process_command'),
            'COMMANDS': module_namespace.get('COMMANDS', {}),
        }
        
        _last_modified = LISTENER_FILE.stat().st_mtime
        print(f"[UE CLI Auto] Module loaded (modified: {time.ctime(_last_modified)})")
        return True
        
    except Exception as e:
        print(f"[UE CLI Auto] Error loading module: {e}")
        return False


def check_reload():
    """Check if module needs reload."""
    global _last_modified
    
    try:
        current_modified = LISTENER_FILE.stat().st_mtime
        if current_modified > _last_modified:
            print(f"[UE CLI Auto] Detected file change, reloading...")
            return load_listener_module()
    except Exception as e:
        print(f"[UE CLI Auto] Error checking file: {e}")
    
    return False


def ue_cli_tick(delta_time):
    """Tick function - checks for reload and processes commands."""
    global _ue_cli_module
    
    # Check for file changes every 60 ticks (~1 second)
    if int(delta_time * 100) % 60 == 0:
        check_reload()
    
    # Process command if module is loaded
    if _ue_cli_module and _ue_cli_module.get('process_command'):
        try:
            _ue_cli_module['process_command']()
        except Exception as e:
            print(f"[UE CLI Auto] Command error: {e}")
    
    return True


def start_ue_cli():
    """Start auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        print("[UE CLI Auto] Already running")
        return
    
    # Load module first
    if not _ue_cli_module:
        load_listener_module()
    
    # Register tick callback
    _ue_cli_tick_handle = unreal.register_slate_post_tick_callback(ue_cli_tick)
    print("[UE CLI Auto] Started! Commands will be processed automatically.")
    print("[UE CLI Auto] File changes will be auto-detected and reloaded.")
    print("[UE CLI Auto] Run stop_ue_cli() to stop")


def stop_ue_cli():
    """Stop auto-polling."""
    global _ue_cli_tick_handle
    
    if _ue_cli_tick_handle:
        unreal.unregister_slate_post_tick_callback(_ue_cli_tick_handle)
        _ue_cli_tick_handle = None
        print("[UE CLI Auto] Stopped")
    else:
        print("[UE CLI Auto] Not running")


def reload_ue_cli():
    """Manually reload the module."""
    load_listener_module()


# Initial load
load_listener_module()

print("[UE CLI Auto] Ready!")
print("[UE CLI Auto] Run start_ue_cli() to begin")
