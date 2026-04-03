# Unreal MCP Tools Reference

UE asset and actor management via MCP using generic reflection-based tools.

## Design Philosophy

### 1. Generic First
Use reflection-based generic tools instead of type-specific commands.

```python
# Good: Generic tools work for any type
create_asset("Material", "M_Red")
create_asset("MaterialInstance", "MI_Red")
create_asset("NiagaraSystem", "NS_Fire")  # Same tool!

get_assets(asset_class="NiagaraSystem")  # List any asset type
```

### 2. Minimal Tool Set

| Category | Count | Tools |
|----------|-------|-------|
| Generic Asset | 7 | create/delete/set/get/batch_create/batch_set/get_assets |
| Generic Actor | 8 | spawn/delete/set/get/batch_spawn/batch_delete/batch_set/get_actors |
| Material | 3 | build_material_graph, compile_material, get_material_graph |
| Import | 2 | import_texture, import_fbx |
| Niagara | 4 | get_niagara_graph, update_niagara_graph, get_niagara_emitter, update_niagara_emitter |

---

## Generic Asset Tools (7 tools)

| Tool | Purpose |
|------|---------|
| `create_asset(type, name, path, properties)` | Create any asset type |
| `delete_asset(path)` | Delete asset |
| `set_asset_properties(path, properties)` | Set any asset properties |
| `get_asset_properties(path, properties?)` | Get asset properties |
| `get_assets(path?, asset_class?, name_filter?)` | List assets by type |
| `batch_create_assets(items)` | Batch create |
| `batch_set_assets_properties(items)` | Batch modify |

### Examples

```python
# Get all materials
get_assets("/Game/", "Material")

# Get material functions
get_assets("/Engine/Functions/", "MaterialFunction")

# Create materials
create_asset("Material", "M_Red", "/Game/Materials/")
create_asset("MaterialInstance", "MI_Red", properties={"parent": "/Game/Materials/M_Base"})

# Set properties
set_asset_properties("/Game/Materials/M_Red.M_Red", {
    "shading_model": "DefaultLit",
    "blend_mode": "Opaque"
})
```

---

## Generic Actor Tools (8 tools)

| Tool | Purpose |
|------|---------|
| `spawn_actor(class, name?, location?, rotation?, scale?, properties?)` | Spawn any actor |
| `delete_actor(name)` | Delete actor |
| `get_actors(class?, detailed?)` | List actors |
| `set_actor_properties(name, properties)` | Set actor properties |
| `get_actor_properties(name, properties?)` | Get actor properties |
| `batch_spawn_actors(items)` | Batch spawn |
| `batch_delete_actors(names)` | Batch delete |
| `batch_set_actors_properties(items)` | Batch modify |

### Examples

```python
# Spawn any actor type
spawn_actor("DirectionalLight", "KeyLight", 
    rotation={"pitch": -45, "yaw": 30},
    properties={"intensity": 10.0, "cast_shadows": True})

spawn_actor("StaticMeshActor", "MaterialBall",
    location={"x": 0, "y": 0, "z": 100},
    properties={"static_mesh": "/Engine/BasicShapes/Sphere"})

# Batch operations
batch_spawn_actors([
    {"actor_class": "Sphere", "name": "Ball1", "location": {"x": 0, "y": 0, "z": 100}},
    {"actor_class": "Sphere", "name": "Ball2", "location": {"x": 200, "y": 0, "z": 100}}
])
```

---

## Material Graph Tools (3 tools)

### `build_material_graph` (Recommended)

Build an entire material graph in a single call.

```python
build_material_graph(
    material_name="M_MyMaterial",
    nodes=[
        {"id": "color", "type": "Constant3Vector", "pos_x": -300, "pos_y": 0, 
         "value": [1.0, 0.0, 0.0]},
        {"id": "roughness", "type": "Constant", "pos_x": -300, "pos_y": 200, 
         "value": 0.5},
        {"id": "tex_base", "type": "TextureSample", "pos_x": -500, "pos_y": 0,
         "texture": "/Game/Textures/T_BaseColor"}
    ],
    connections=[
        {"source": "tex_base", "target": "Material", "target_input": "BaseColor"},
        {"source": "roughness", "target": "Material", "target_input": "Roughness"}
    ],
    properties={"shading_model": "DefaultLit"},
    compile=True
)
```

**Supported Node Types**: Constant, Constant2/3/4Vector, ScalarParameter, VectorParameter, TextureSample, Multiply, Add, Subtract, Divide, Lerp, Clamp, TextureCoordinate, Time, VertexNormal, WorldPosition, OneMinus, Saturate, Normalize, DotProduct, CrossProduct, ComponentMask, AppendVector, Fresnel, Power, Panner, Rotator, Sine, Cosine, Abs, Floor, Ceil, Frac, SquareRoot, Desaturation, ReflectionVector, CameraPosition, CameraVector

### `get_material_graph`

Get complete material or material function graph (nodes + connections). Supports both Material and MaterialFunction assets.

```python
# For Material
result = get_material_graph("/Game/Materials/M_Material")
# Returns: {
#   "asset_type": "Material",
#   "nodes": [...],
#   "connections": [...],
#   "property_connections": {"BaseColor": {...}, "Normal": {...}, ...}
# }

# For MaterialFunction
result = get_material_graph("/Engine/Functions/Engine_MaterialFunctions01/Texturing/BitMask")
# Returns: {
#   "asset_type": "MaterialFunction",
#   "nodes": [...],
#   "connections": [...],
#   "inputs": [...],
#   "outputs": [...],
#   "description": "..."
# }
```

### `compile_material`

Compile a material to update its shader.

```python
compile_material("/Game/Materials/M_MyMaterial")
```

---

## Niagara Tools (4 tools)

Following "Minimal Tool Set" principle:
- **Graph Form**: Read/update Niagara script graphs (embedded or standalone)
- **Emitter Form**: Read/update Emitter hierarchy structure

### `get_niagara_graph`

Get Niagara Graph nodes and connections. Unified interface for reading graphs from:
1. Scripts within a Niagara System (specify asset_path + emitter + script)
2. Standalone Niagara Script assets (specify script_path only)

```python
# Get graph from System's Emitter spawn script
get_niagara_graph(asset_path="/Game/Effects/NS_Fire", emitter="Flame", script="spawn")

# Get graph from standalone Niagara Module
get_niagara_graph(script_path="/Niagara/Modules/Update/Forces/CurlNoiseForce")
```

### `update_niagara_graph`

Update Niagara Graph with operations.

```python
# Add module to System's Emitter script
update_niagara_graph(
    asset_path="/Game/Effects/NS_Fire",
    emitter="Flame",
    script="update",
    operations=[{"action": "add_module", "module_path": "/Niagara/Modules/Update/Forces/CurlNoiseForce"}]
)

# Add module to standalone script
update_niagara_graph(
    script_path="/Game/Niagara/MyCustomModule",
    operations=[{"action": "add_module", "module_path": "/Niagara/Modules/Common/Math"}]
)
```

### `get_niagara_emitter`

Get Niagara Emitter structure and properties.

```python
# Quick overview of all emitters
get_niagara_emitter("/Game/Effects/NS_Fire")

# Full detail with stateless analysis
get_niagara_emitter(
    "/Game/Effects/NS_Fire",
    detail_level="full",
    emitters=["Flame"],
    include=["scripts", "renderers", "parameters", "stateless_analysis"]
)
```

### `update_niagara_emitter`

Batch update Niagara Emitter with multiple operations.

```python
# Emitter operations
update_niagara_emitter("/Game/Effects/NS_Fire", [
    {"target": "emitter", "name": "Smoke", "action": "set_enabled", "value": False},
    {"target": "emitter", "name": "Flame", "action": "rename", "value": "BigFlame"},
    
    # Convert to Stateless (UE 5.7+)
    {"target": "emitter", "name": "Flame", "action": "convert_to_stateless", "force": False},
    
    # Parameter operations
    {"target": "parameter", "emitter": "Flame", "script": "spawn", "name": "SpawnRate", "value": 100.0},
    
    # Stateless module operations (UE 5.7+)
    {"target": "stateless_module", "emitter": "Flame", "name": "Lifetime", 
     "action": "set_property", "property": "LifetimeMin", "value": 1.5}
])
```

---

## Import Tools (2 tools)

### `import_texture`

Import texture file (DDS, PNG, TGA, etc.) into Unreal.

```python
import_texture(
    source_path="C:/Textures/Normal.png",
    name="T_Normal",
    destination_path="/Game/Textures/",
    srgb=False,
    compression_settings="TC_NormalMap"
)
```

### `import_fbx`

Import FBX file into Unreal.

```python
import_fbx(
    fbx_path="C:/Models/Character.fbx",
    destination_path="/Game/Meshes/",
    spawn_in_level=True,
    location=[0, 0, 0]
)
```

---

## Other Tools

| Tool | Purpose |
|------|---------|
| `create_static_mesh_from_data` | Create mesh directly from vertex data |
| `get_viewport_screenshot` | Capture viewport screenshot |

---

## Common Property Names

### Material
- `shading_model`: "DefaultLit", "Unlit", "Subsurface"
- `blend_mode`: "Opaque", "Masked", "Translucent"
- `two_sided`: true/false

### Light
- `intensity`: float
- `light_color`: [R, G, B, A]
- `temperature`: float (K)
- `cast_shadows`: true/false

### Texture
- `srgb`: true/false
- `compression_settings`: "Default", "TC_NormalMap", "TC_Grayscale"

---

## Legacy Tools (Removed)

| Removed Tool | Replacement |
|--------------|-------------|
| `get_available_materials` | `get_assets(path, "Material")` |
| `get_material_functions` | `get_assets(path, "MaterialFunction")` |
| `get_material_expressions` | `get_material_graph` (nodes field) |
| `get_material_connections` | `get_material_graph` (connections field) |
| `get_material_function_content` | `get_material_graph` (now supports MaterialFunction) |
| `create_light` | `spawn_actor("DirectionalLight", ...)` |
| `set_light_properties` | `set_actor_properties(...)` |
| `get_lights` | `get_actors(actor_class="Light")` |
| `delete_light` | `delete_actor(name)` |
| `create_material` | `create_asset("Material", name, path)` |
| `create_material_function` | `create_asset("MaterialFunction", name, path)` |
| `create_material_instance` | `create_asset("MaterialInstance", name, path, {"parent": ...})` |
| `set_material_properties` | `set_asset_properties(path, props)` |
| `set_material_instance_parameter` | `set_asset_properties(path, props)` |
| `set_texture_properties` | `set_asset_properties(path, props)` |
| `apply_material_to_actor` | `set_actor_properties(name, {"materials": [...]})` |
| `apply_material_to_blueprint` | `set_asset_properties(path, {"materials": [...]})` |
| `get_actor_material_info` | `get_actor_properties(name, ["materials"])` |
| `get_blueprint_material_info` | `get_asset_properties(path, ["materials"])` |
| `get_niagara_asset_details` | `get_niagara_emitter` |
| `update_niagara_asset` | `update_niagara_emitter` |
| `analyze_stateless_compatibility` | `get_niagara_emitter(include=["stateless_analysis"])` |
| `convert_to_stateless` | `update_niagara_emitter(..., [{"target": "emitter", "action": "convert_to_stateless"}])` |
| `get_niagara_module_graph` | `get_niagara_graph(asset_path, emitter, script)` |
| `get_niagara_script_asset` | `get_niagara_graph(script_path)` |
| `update_niagara_script_asset` | `update_niagara_graph(script_path, operations)` |
