"""UE Backend with auto-reload support"""

import json
import os
import time
from pathlib import Path

TEMP_DIR = Path(os.environ.get("TEMP", "/tmp")) / "ue_cli"
COMMAND_FILE = TEMP_DIR / "command.json"
RESULT_FILE = TEMP_DIR / "result.json"
LOG_FILE = TEMP_DIR / "ue_cli_log.json"


def execute_command(command_type: str, params: dict) -> dict:
    """Execute a command via file-based communication with auto-reload support."""
    # Ensure temp directory exists
    TEMP_DIR.mkdir(parents=True, exist_ok=True)
    
    # Generate command ID
    command_id = f"cmd_{int(time.time() * 1000)}"
    
    # Write command
    command_data = {
        "id": command_id,
        "type": command_type,
        "params": params
    }
    
    with open(COMMAND_FILE, 'w', encoding='utf-8') as f:
        json.dump(command_data, f, indent=2)
    
    # Wait for result
    timeout = 30  # seconds
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if RESULT_FILE.exists():
            try:
                with open(RESULT_FILE, 'r', encoding='utf-8') as f:
                    result_data = json.load(f)
                
                if result_data.get("id") == command_id:
                    # Delete result file
                    try:
                        RESULT_FILE.unlink()
                    except:
                        pass
                    
                    return result_data.get("result", {"success": False, "error": "No result"})
            except:
                pass
        
        time.sleep(0.05)
    
    return {"success": False, "error": "Timeout waiting for result"}


def get_logs() -> list:
    """Get logs from UE for debugging."""
    if LOG_FILE.exists():
        try:
            with open(LOG_FILE, 'r', encoding='utf-8') as f:
                return json.load(f)
        except:
            return []
    return []


def clear_logs():
    """Clear log file."""
    if LOG_FILE.exists():
        try:
            LOG_FILE.unlink()
        except:
            pass
