# 🎨 UE Material Analysis Workflow

Complete workflow for analyzing existing Unreal Engine materials using MCP tools.

## Overview

This workflow enables deep analysis of UE materials including:
- Material properties (BlendMode, ShadingModel, etc.)
- Node graph structure and connections
- Expression parameters and values
- Material function references

## Tools Overview

| Tool | Purpose |
|------|---------|
| `get_available_materials` | List materials in project |
| `get_material_properties` | Get material attributes |
| `get_material_expressions` | List all nodes in material |
| `get_material_connections` | Get node connection relationships |
| `get_material_function_content` | Analyze material functions |

## Workflow

### Step 1: Find Target Material

```python
# List all materials in a path
get_available_materials(search_path="/Game/Materials/")
get_available_materials(search_path="/Engine/BasicShapes/")
```

### Step 2: Get Material Properties

```python
# Get basic material attributes
get_material_properties(material_name="/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
```

**Returns:**
- `blend_mode`: Opaque, Masked, Translucent, Additive, etc.
- `shading_models`: List of enabled shading models
- `two_sided`: Whether material is two-sided
- `material_domain`: Surface, UI, PostProcess, etc.
- `is_masked`: Whether material uses masking
- `opacity_mask_clip_value`: Clip threshold for masked materials

### Step 3: List Material Expressions

```python
# Get all nodes in the material graph
get_material_expressions(material_name="/Game/Materials/M_Water")
```

**Returns:**
- List of expression nodes with:
  - `node_id`: Unique identifier
  - `type`: Expression class name
  - `position`: [x, y] in graph
  - `parameters`: Values for constants, textures, etc.
  - `function_path`: For MaterialFunctionCall nodes

### Step 4: Get Node Connections

```python
# Get how nodes connect to each other
get_material_connections(material_name="/Game/Materials/M_Water")
```

**Returns:**
- `node_connections`: List of nodes with their input connections
  - `node_id`: Target node ID
  - `inputs`: Dict mapping input names to source nodes
    - `connected_node`: Source node ID
    - `output_index`: Which output pin
    - `output_name`: Name of the output pin
- `property_connections`: Connections to material properties
  - BaseColor, Metallic, Roughness, Normal, EmissiveColor, etc.

### Step 5: Analyze Material Functions (Optional)

```python
# Deep dive into material function internals
get_material_function_content(
    function_path="/Engine/Functions/Engine_MaterialFunctions01/Texturing/BitMask.BitMask"
)
```

**Returns:**
- `inputs`: Function input parameters
- `outputs`: Function output pins
- `expressions`: Internal expression nodes

## Example: Complete Material Analysis

```python
# 1. Find material
materials = get_available_materials(search_path="/Game/Materials/")
# Select: /Game/Materials/M_Water

# 2. Get properties
props = get_material_properties(material_name="/Game/Materials/M_Water")
# Result: blend_mode="Opaque", shading_models=["DefaultLit"], two_sided=False

# 3. Get all nodes
nodes = get_material_expressions(material_name="/Game/Materials/M_Water")
# Returns list of TextureSample, Multiply, Add, etc.

# 4. Get connections
connections = get_material_connections(material_name="/Game/Materials/M_Water")
# Returns node graph structure

# 5. Analyze referenced functions (if any)
for node in nodes:
    if node["type"] == "MaterialFunctionCall":
        func_content = get_material_function_content(node["function_path"])
        # Analyze function internals
```

## Output Format

### Material Properties
```json
{
  "blend_mode": "Opaque",
  "shading_models": ["DefaultLit"],
  "two_sided": false,
  "material_domain": "Surface",
  "is_masked": false,
  "opacity_mask_clip_value": 0.3333
}
```

### Node Connections
```json
{
  "node_connections": [
    {
      "node_id": "node_123",
      "inputs": {
        "A": {
          "connected_node": "node_100",
          "output_index": 0,
          "output_name": "Output"
        }
      }
    }
  ],
  "property_connections": {
    "BaseColor": {
      "connected_node": "node_123",
      "output_index": 0
    }
  }
}
```

## Common Expression Types

| Type | Description | Key Parameters |
|------|-------------|----------------|
| `Constant` | Single float value | `value` |
| `Constant3Vector` | RGB color | `value: [r, g, b]` |
| `Constant4Vector` | RGBA color | `value: [r, g, b, a]` |
| `TextureSample` | Texture lookup | `texture_path` |
| `Multiply` | Multiply inputs | - |
| `Add` | Add inputs | - |
| `Lerp` | Linear interpolation | - |
| `ScalarParameter` | Exposed float | `parameter_name`, `value` |
| `VectorParameter` | Exposed color | `parameter_name`, `value` |
| `TextureSampleParameter2D` | Exposed texture | `parameter_name`, `texture_path` |
| `MaterialFunctionCall` | Function instance | `function_path`, `function_name` |

## Use Cases

### 1. Material Documentation Generation
Analyze material structure and generate technical documentation for artists.

### 2. Material Replication
Extract material setup and recreate similar materials programmatically.

### 3. Debug Material Issues
Identify disconnected nodes, missing textures, or incorrect parameters.

### 4. Learning Reference
Study how complex UE materials (like water, foliage, skin) are constructed.

### 5. Material Optimization
Find redundant nodes or opportunities for material function extraction.

## Limitations

1. **Material Function Expansion**: `get_material_expressions` does not automatically expand MaterialFunctionCall nodes. Use `get_material_function_content` separately.

2. **Dynamic Parameters**: Some runtime parameters may show default values only.

3. **Custom Expressions**: Custom HLSL code is returned as-is without parsing.

4. **Subgraph Connections**: Connections inside material functions are not included in `get_material_connections`.

## Tips

- Use `search_path` to narrow down material search
- Combine with `renderdoc-material-reconstruction` skill for capture-based analysis
- Use node_id to trace data flow through the graph
- Material property connections reveal the final output structure
