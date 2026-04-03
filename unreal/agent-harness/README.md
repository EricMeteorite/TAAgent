# Unreal Engine CLI

Command-line interface for Unreal Engine 5 - control UE from the terminal!

## Quick Start

### 1. Install CLI

```bash
cd unreal/agent-harness
pip install -e .
```

### 2. Start UE Listener (Auto-Reload Version)

In Unreal Editor:
1. Open **Python Console** (Window -> Developer Tools -> Python Console)
2. Run:
```python
# Replace the path below with your local TAAgent path
exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener_auto.py").read())
start_ue_cli()
```

This version **auto-reloads** when you modify `ue_cli_listener.py` - no need to restart!

Or use the manual version (no auto-reload):
```python
exec(open(r"D:/ABSOLUTE/PATH/TO/TAAgent/unreal/agent-harness/ue_cli_listener.py").read())
start_ue_cli()
```

### 3. Use CLI

```bash
# Check UE status
ue-cli info status

# List actors in current level
ue-cli actor list

# Spawn a point light
ue-cli actor spawn -t PointLight -n "MyLight" -l "100,200,300"

# Create a material
ue-cli material create -n "RedMat" --color "1,0,0,1"

# Take a screenshot
ue-cli screenshot capture -o "C:/screenshot.png"

# Get level info
ue-cli level info
```

## Available Commands

### Info Commands
- `ue-cli info status` - Check UE connection status
- `ue-cli info ping` - Ping UE to verify connection

### Level Commands
- `ue-cli level info` - Show current level information
- `ue-cli level open <path>` - Open a level
- `ue-cli level save` - Save current level

### Actor Commands
- `ue-cli actor list` - List all actors
- `ue-cli actor spawn -t <type> -n <name> -l <x,y,z>` - Spawn actor
- `ue-cli actor delete <name>` - Delete actor
- `ue-cli actor move <name> -l <x,y,z>` - Move actor

### Material Commands
- `ue-cli material create -n <name> -p <path>` - Create material
- `ue-cli material apply <actor> <material_path>` - Apply material

### Asset Commands
- `ue-cli asset list -p <path>` - List assets
- `ue-cli asset import <source> <dest>` - Import asset

### Screenshot Commands
- `ue-cli screenshot capture -o <path>` - Take screenshot

### Python Commands
- `ue-cli python exec <code>` - Execute Python code
- `ue-cli python file <path>` - Execute Python file

### Interactive REPL
```bash
ue-cli repl
```

## How It Works

The CLI uses a **file-based communication** mechanism:

1. CLI writes commands to `<temp>/ue_cli/command.json`
2. UE Python listener polls and executes commands
3. UE writes results to `result.json`
4. CLI reads results

This approach works without requiring network ports or complex setup.

## Requirements

- Unreal Engine 5.x
- Python Script Plugin enabled in UE
- Python 3.10+

## Troubleshooting

**"UE not running" error:**
- Make sure UE Editor is open
- Check that `UE_EDITOR_PATH` environment variable is set (optional)

**"Timeout waiting for result" error:**
- Make sure the listener script is running in UE Python Console
- Restart the listener if needed

**Commands not working:**
- Verify Python Script Plugin is enabled in UE
- Check UE Output Log for Python errors
