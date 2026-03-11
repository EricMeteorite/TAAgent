# CLI-Anything Unreal Engine

CLI harness for Unreal Engine 5 - Material, Niagara, and Scene operations via MCP server.

## Requirements

- **Unreal Engine 5.3+** (tested with UE 5.7)
- **UnrealMCP Plugin** - Running in UE Editor
- **Python 3.10+**

## Installation

```bash
cd unreal/agent-harness
pip install -e .
```

## Usage

### Check Installation

```bash
cli-anything-unreal --help
```

### REPL Mode (Interactive)

```bash
# Enter interactive REPL
cli-anything-unreal

# Or with project file
cli-anything-unreal -p my_project.json
```

### Subcommand Mode (Scripting)

```bash
# Create new project session
cli-anything-unreal new -n MyProject -o project.json

# Check connection status
cli-anything-unreal status project.json

# Create material
cli-anything-unreal material create -p /Game/Materials -n M_Test

# Build material graph
cli-anything-unreal material build-graph \
  -m /Game/Materials/M_Test \
  -n '[{"type":"Constant3Vector","location":[-200,0]}]' \
  -c '[{"from":"RGB","to":"BaseColor"}]'

# Spawn actor
cli-anything-unreal actor spawn \
  --type StaticMeshActor \
  --location 0,0,100 \
  --name MyActor

# Capture screenshot
cli-anything-unreal screenshot -o preview.png
```

### JSON Output Mode

All commands support `--json` flag for structured output:

```bash
cli-anything-unreal --json status project.json
```

## MCP Server Connection

The CLI connects to UnrealMCP WebSocket server running in UE Editor.

Default: `ws://localhost:8080`

Custom connection:

```bash
cli-anything-unreal --host 192.168.1.100 --port 9999
```

## Available Commands

### Project Management
- `new` - Create new project session
- `status` - Show connection and project status

### Material Operations
- `material create` - Create new material
- `material build-graph` - Build material graph

### Actor Operations
- `actor spawn` - Spawn actor in scene

### Utility
- `screenshot` - Capture viewport screenshot

## Architecture

This CLI wraps the existing **UnrealMCP** WebSocket server:

```
CLI (Click)
    ↓ JSON-RPC
MCP Server (Python)
    ↓ WebSocket
UnrealMCP Plugin (C++)
    ↓ UE C++ API
Unreal Engine
```

## Why CLI Wrapper?

This CLI provides:
- **Script-friendly interface** - Batch operations via command line
- **Agent-friendly interface** - JSON output for AI consumption
- **Interactive REPL** - For manual testing and exploration

## Testing

```bash
cd unreal/agent-harness
pytest cli_anything/unreal/tests/ -v
```

## License

MIT
