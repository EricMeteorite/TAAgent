#!/usr/bin/env python3
"""Auto-reload system for UE CLI - Monitors output and auto-reloads on errors

This script runs inside Unreal Engine and:
1. Monitors command execution
2. Captures errors
3. Auto-reloads the listener module when needed
4. Reports status back to CLI

Usage in UE Python Console:
    exec(open(r"D:/CodeBuddy/rendering-mcp/unreal/agent-harness/auto_reload_system.py").read())
    start_auto_reload()
"""

import unreal
import json
import os
import sys
import time
import traceback
from pathlib import Path

# Configuration
TEMP_DIR = Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"
LOG_FILE = TEMP_DIR / "ue_cli_log.json"

TEMP_DIR.mkdir(parents=True, exist_ok=True)

# Global state
_auto_reload_running = False
_last_error = None
_command_count = 0

# Store original module
_original_module = None

def log_message(level, message, data=None):
    """Log message to file for external monitoring."""
    log_entry = {
        "timestamp": time.time(),
        "level": level,
        "message": message,
        "data": data
    }
    try:
        logs = []
        if LOG_FILE.exists():
            with open(LOG_FILE, 'r', encoding='utf-8') as f:
                try:
                    logs = json.load(f)
                except:
                    logs = []
        
        logs.append(log_entry)
        
        # Keep only last 100 logs
        if len(logs) > 100:
            logs = logs[-100:]
        
        with open(LOG_FILE, 'w', encoding='utf-8') as f:
            json.dump(logs, f, indent=2, default=str)
    except Exception as e:
        print(f"[AutoReload] Failed to write log: {e}")

def get_module_source():
    """Get the source code of the listener module."""
    listener_path = Path(r"D:/CodeBuddy/rendering-mcp/unreal/agent-harness/ue_cli_listener_full.py")
    if listener_path.exists():
        with open(listener_path, 'r', encoding='utf-8') as f:
            return f.read()
    return None

def check_for_errors(result):
    """Check if command result indicates an error that needs reload."""
    if not isinstance(result, dict):
        return False
    
    error = result.get("error", "")
    if not error:
        return False
    
    # Check for common error patterns that indicate code issues
    error_patterns = [
        "has no attribute",
        "object has no attribute",
        "Failed to find property",
        "SyntaxError",
        "ImportError",
        "ModuleNotFoundError",
        "NameError",
    ]
    
    for pattern in error_patterns:
        if pattern in str(error):
            return True
    
    return False

def reload_listener():
    """Reload the listener module."""
    global _original_module
    
    try:
        log_message("INFO", "Reloading listener module...")
        
        # Read current source
        source = get_module_source()
        if not source:
            log_message("ERROR", "Could not read listener source")
            return False
        
        # Execute in a fresh namespace with all required globals
        namespace = {
            '__name__': '__main__',
            '__file__': r"D:/CodeBuddy/rendering-mcp/unreal/agent-harness/ue_cli_listener_full.py",
            'unreal': unreal,
            'json': json,
            'os': os,
            'time': time,
            'Path': Path,
            'traceback': traceback,
        }
        
        exec(source, namespace)
        
        # Update global commands
        # Support both COMMANDS and COMMAND_MAP
        command_dict = namespace.get('COMMANDS') or namespace.get('COMMAND_MAP')
        if command_dict:
            # Store reference to new commands
            _original_module = namespace
            
            log_message("SUCCESS", "Listener module reloaded successfully", {
                "commands": len(command_dict)
            })
            return True
        
        return False
        
    except Exception as e:
        log_message("ERROR", f"Failed to reload: {str(e)}", {
            "traceback": traceback.format_exc()
        })
        return False

def auto_reload_tick():
    """Tick function that checks for commands and handles auto-reload."""
    global _auto_reload_running, _last_error, _command_count
    
    if not _auto_reload_running:
        return
    
    try:
        # Check for command file
        if COMMAND_FILE.exists():
            with open(COMMAND_FILE, 'r', encoding='utf-8') as f:
                command_data = json.load(f)
            
            command_type = command_data.get("type")
            command_id = command_data.get("id")
            params = command_data.get("params", {})
            
            log_message("INFO", f"Processing command: {command_type}", {"id": command_id})
            
            # Get current command map
            # Support both COMMANDS and COMMAND_MAP
            if _original_module:
                command_map = _original_module.get('COMMANDS') or _original_module.get('COMMAND_MAP', {})
            else:
                # Fallback: try to get from global
                import __main__
                command_map = getattr(__main__, 'COMMANDS', None) or getattr(__main__, 'COMMAND_MAP', {})
            
            result = None
            
            if command_type in command_map:
                try:
                    result = command_map[command_type](params)
                    _command_count += 1
                    
                    # Check if error indicates need for reload
                    if check_for_errors(result):
                        log_message("WARN", f"Error detected, auto-reloading...", {
                            "error": result.get("error"),
                            "command": command_type
                        })
                        
                        if reload_listener():
                            # Retry command after reload
                            if _original_module:
                                command_map = _original_module.get('COMMANDS') or _original_module.get('COMMAND_MAP', {})
                                if command_type in command_map:
                                    result = command_map[command_type](params)
                                    result["retried_after_reload"] = True
                        
                except Exception as e:
                    error_msg = str(e)
                    log_message("ERROR", f"Command failed: {error_msg}", {
                        "command": command_type,
                        "traceback": traceback.format_exc()
                    })
                    
                    # Try reload on error
                    if reload_listener():
                        # Retry
                        try:
                            if _original_module:
                                command_map = _original_module.get('COMMANDS') or _original_module.get('COMMAND_MAP', {})
                                if command_type in command_map:
                                    result = command_map[command_type](params)
                                    result["retried_after_reload"] = True
                        except Exception as retry_e:
                            result = {
                                "success": False,
                                "error": str(retry_e),
                                "original_error": error_msg
                            }
                    else:
                        result = {
                            "success": False,
                            "error": error_msg,
                            "traceback": traceback.format_exc()
                        }
            else:
                result = {"success": False, "error": f"Unknown command: {command_type}"}
            
            # Write result
            result_data = {
                "id": command_id,
                "type": command_type,
                "result": result
            }
            
            with open(RESULT_FILE, 'w', encoding='utf-8') as f:
                json.dump(result_data, f, indent=2, default=str)
            
            # Delete command file
            try:
                COMMAND_FILE.unlink()
            except:
                pass
            
            log_message("INFO", f"Command completed: {command_type}", {"success": result.get("success", False)})
    
    except Exception as e:
        log_message("ERROR", f"Tick error: {str(e)}", {
            "traceback": traceback.format_exc()
        })
    
    # No need to schedule next tick - slate_post_tick_callback runs every frame

def start_auto_reload():
    """Start the auto-reload system."""
    global _auto_reload_running, _original_module
    
    if _auto_reload_running:
        print("[AutoReload] Already running")
        return
    
    # Load initial module
    source = get_module_source()
    if source:
        namespace = {
            '__name__': '__main__',
            '__file__': r"D:/CodeBuddy/rendering-mcp/unreal/agent-harness/ue_cli_listener_full.py",
            'unreal': unreal,
            'json': json,
            'os': os,
            'time': time,
            'Path': Path,
            'traceback': traceback,
        }
        try:
            exec(source, namespace)
            _original_module = namespace
            # Support both COMMANDS and COMMAND_MAP
            command_dict = namespace.get('COMMANDS') or namespace.get('COMMAND_MAP', {})
            cmd_count = len(command_dict)
            print(f"[AutoReload] Loaded {cmd_count} commands")
            if cmd_count == 0:
                print("[AutoReload] WARNING: No commands loaded!")
        except Exception as e:
            print(f"[AutoReload] Failed to load module: {e}")
            print(traceback.format_exc())
    
    _auto_reload_running = True
    
    # Use timer callback instead of tick
    def tick_callback(delta_time):
        if _auto_reload_running:
            auto_reload_tick()
    
    # Register with slate tick
    unreal.register_slate_post_tick_callback(tick_callback)
    
    print("[AutoReload] Auto-reload system started!")
    print("[AutoReload] Commands will auto-retry on errors")
    log_message("INFO", "Auto-reload system started")

def stop_auto_reload():
    """Stop the auto-reload system."""
    global _auto_reload_running
    _auto_reload_running = False
    print("[AutoReload] Stopped")
    log_message("INFO", "Auto-reload system stopped")

def get_status():
    """Get current status."""
    return {
        "running": _auto_reload_running,
        "commands_processed": _command_count,
        "last_error": _last_error,
        "module_loaded": _original_module is not None
    }

# Print initialization message
print("=" * 50)
print("UE CLI Auto-Reload System")
print("=" * 50)
print("Usage:")
print("  start_auto_reload()  - Start auto-reload system")
print("  stop_auto_reload()   - Stop auto-reload system")
print("  get_status()         - Get current status")
print("=" * 50)
