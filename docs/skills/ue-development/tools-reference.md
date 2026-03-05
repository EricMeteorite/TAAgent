# 🔧 Advanced Tools Reference

Complete documentation for all 21 tools in the Unreal MCP Advanced Server.

## 🏗️ World Building Tools

### create_town
Create complete urban environments with buildings, roads, and infrastructure.

**Parameters:**
- `town_size` (string): "small", "medium", "large", or "metropolis"
- `architectural_style` (string): "modern", "medieval", "suburban", "downtown", "mixed", or "futuristic"  
- `building_density` (float, 0.0-1.0): How packed the buildings are
- `location` (array): [X, Y, Z] world position for town center
- `include_infrastructure` (bool): Add roads, utilities, etc.
- `name_prefix` (string): Prefix for spawned building actors

**Example:**
```bash
create_town(town_size="medium", architectural_style="modern", building_density=0.8, location=[0, 0, 0])
```

### construct_house  
Build realistic multi-room houses with architectural details.

**Parameters:**
- `width` (int): House width in centimeters (default: 1200)
- `depth` (int): House depth in centimeters (default: 1000)
- `height` (int): Wall height in centimeters (default: 600)
- `location` (array): House center position
- `house_style` (string): "modern", "cottage", or "mansion"
- `mesh` (string): Static mesh asset path
- `name_prefix` (string): Prefix for house components

**Features:**
- **Foundation & Floor**: Proper structural base
- **Room Division**: Interior walls creating realistic spaces
- **Windows & Doors**: Authentic openings with proper sizing
- **Pitched Roof**: Angled rooftop instead of flat surface
- **Style Variations**: Different proportions and decorative elements

**Examples:**
```bash
# Modern family home
construct_house(house_style="modern", location=[0, 0, 0])

# Large mansion
construct_house(width=1500, depth=1200, house_style="mansion", location=[2000, 0, 0])

# Cozy cottage  
construct_house(house_style="cottage", location=[-1000, 1000, 0])
```

### create_tower
Build architectural towers with various styles and decorative elements.

**Parameters:**
- `height` (int): Number of vertical levels (default: 10)
- `base_size` (int): Base diameter/width (default: 4)
- `tower_style` (string): "cylindrical", "square", or "tapered"
- `block_size` (float): Size of building blocks in cm
- `location` (array): Tower base center position
- `mesh` (string): Static mesh for blocks
- `name_prefix` (string): Actor naming prefix

**Styles:**
- **Cylindrical**: Round tower with blocks in circular pattern
- **Square**: Hollow square tower with corner reinforcements  
- **Tapered**: Tower that narrows toward the top

**Example:**
```bash
create_tower(height=15, base_size=6, tower_style="cylindrical", location=[1000, 0, 0])
```

### create_arch
Create decorative arch structures using blocks arranged in semicircles.

**Parameters:**
- `radius` (float): Arch radius in centimeters (default: 300)
- `segments` (int): Number of blocks forming the arch (default: 6)
- `location` (array): Arch center base position
- `mesh` (string): Static mesh asset path
- `name_prefix` (string): Actor naming prefix

## 🧩 Level Design Tools

### create_maze
Generate solvable mazes using recursive backtracking algorithm.

**Parameters:**
- `rows` (int): Maze height in cells (default: 8)
- `cols` (int): Maze width in cells (default: 8)  
- `cell_size` (float): Size of each maze cell in cm (default: 300)
- `wall_height` (int): Height of walls in block layers (default: 3)
- `location` (array): Maze center position

**Features:**
- **Guaranteed Solvable**: Uses recursive backtracking for valid paths
- **Clear Entrance/Exit**: Marked with distinctive objects
- **Open Top Design**: Walls are limited height for aerial viewing
- **No Dead Ends**: Every area is accessible

**Example:**
```bash
create_maze(rows=12, cols=12, wall_height=4, cell_size=250, location=[0, 0, 0])
```

### create_pyramid
Build stepped pyramids from stacked blocks.

**Parameters:**
- `base_size` (int): Number of blocks on base edge (default: 3)
- `block_size` (float): Edge length of each block in cm (default: 100)
- `location` (array): Pyramid base center
- `mesh` (string): Static mesh asset path
- `name_prefix` (string): Actor naming prefix

### create_wall
Generate straight walls from repeated block elements.

**Parameters:**
- `length` (int): Number of blocks along wall (default: 5)
- `height` (int): Number of block layers vertically (default: 2)
- `block_size` (float): Block dimensions in cm (default: 100)
- `location` (array): Wall starting position
- `orientation` (string): Direction to extend - "x" or "y"
- `mesh` (string): Static mesh asset path
- `name_prefix` (string): Actor naming prefix

### create_staircase
Build stepped staircases with configurable dimensions.

**Parameters:**
- `steps` (int): Number of steps (default: 5)
- `step_size` (array): [width, depth, height] of each step (default: [100, 100, 50])
- `location` (array): Staircase starting position
- `mesh` (string): Static mesh asset path
- `name_prefix` (string): Actor naming prefix

## ⚛️ Physics & Materials

### spawn_physics_blueprint_actor 
Create actors with custom physics properties and materials.

**Parameters:**
- `name` (string): Actor name (must be unique)
- `mesh_path` (string): Path to static mesh asset
- `location` (array): Spawn position (default: [0, 0, 0])
- `mass` (float): Physics mass in kg (default: 1.0)
- `simulate_physics` (bool): Enable physics simulation (default: true)
- `gravity_enabled` (bool): Enable gravity effects (default: true)

**Process:**
1. Creates temporary Blueprint class
2. Adds StaticMeshComponent with specified mesh
3. Configures physics properties
4. Compiles Blueprint and spawns actor


## 🎨 Blueprint System

### create_blueprint
Create new Blueprint classes for custom actors.

**Parameters:**
- `name` (string): Blueprint name (must be unique)
- `parent_class` (string): Base class - typically "Actor"

### add_component_to_blueprint
Add components to existing Blueprint classes.

**Parameters:**
- `blueprint_name` (string): Target Blueprint name
- `component_type` (string): Component class name
- `component_name` (string): Name for the new component
- `location` (array): Relative position within Blueprint
- `rotation` (array): Relative rotation in degrees
- `scale` (array): Relative scale factors
- `component_properties` (object): Additional component settings

**Common Component Types:**
- `StaticMeshComponent`: 3D geometry rendering
- `CameraComponent`: Viewport and rendering cameras  
- `LightComponent`: Lighting sources
- `AudioComponent`: Sound playback

### set_static_mesh_properties
Configure mesh assets on StaticMeshComponents.

**Parameters:**
- `blueprint_name` (string): Blueprint containing the component
- `component_name` (string): StaticMeshComponent to modify
- `static_mesh` (string): Asset path to mesh (default: "/Engine/BasicShapes/Cube.Cube")

**Available Basic Meshes:**
- `/Engine/BasicShapes/Cube.Cube`
- `/Engine/BasicShapes/Sphere.Sphere`  
- `/Engine/BasicShapes/Cylinder.Cylinder`
- `/Engine/BasicShapes/Plane.Plane`

### set_physics_properties
Configure physics simulation parameters on components.

**Parameters:**
- `blueprint_name` (string): Blueprint containing component
- `component_name` (string): Component to configure
- `mass` (float): Object mass in kilograms (default: 1.0)
- `linear_damping` (float): Resistance to linear motion (default: 0.01)
- `angular_damping` (float): Resistance to rotation (default: 0.0)
- `simulate_physics` (bool): Enable physics simulation (default: true)
- `gravity_enabled` (bool): Enable gravity effects (default: true)

### set_mesh_material_color
Apply colored materials to mesh components.

**Parameters:**
- `blueprint_name` (string): Blueprint containing component
- `component_name` (string): StaticMeshComponent to color
- `color` (array): [R, G, B, A] values (0.0-1.0 range)
- `material_path` (string): Material asset path 
- `parameter_name` (string): Material parameter to modify

**Common Colors:**
- Red: `[1.0, 0.0, 0.0, 1.0]`
- Green: `[0.0, 1.0, 0.0, 1.0]` 
- Blue: `[0.0, 0.0, 1.0, 1.0]`
- Yellow: `[1.0, 1.0, 0.0, 1.0]`
- Purple: `[1.0, 0.0, 1.0, 1.0]`
- White: `[1.0, 1.0, 1.0, 1.0]`

### compile_blueprint
Compile Blueprint classes to apply changes.

**Parameters:**
- `blueprint_name` (string): Blueprint to compile

**Note:** Always compile Blueprints before spawning actors from them.

### spawn_blueprint_actor
Create actor instances from compiled Blueprint classes.

**Parameters:**
- `blueprint_name` (string): Source Blueprint class
- `actor_name` (string): Name for spawned actor
- `location` (array): World spawn position
- `rotation` (array): World rotation in degrees

## 🎯 Actor Management

### get_actors_in_level
List all actors currently in the level.

**Returns:** Array of actor information including names, types, and transforms.

### find_actors_by_name
Search for actors using name patterns.

**Parameters:**
- `pattern` (string): Search pattern (supports wildcards)

### spawn_actor  
Create basic actor types directly.

**Parameters:**
- `name` (string): Actor name
- `type` (string): Actor class name
- `location` (array): Spawn position (default: [0, 0, 0])
- `rotation` (array): Spawn rotation (default: [0, 0, 0])

**Common Types:**
- `StaticMeshActor`: Basic 3D objects
- `CameraActor`: Viewport cameras
- `LightActor`: Scene lighting

### delete_actor
Remove actors from the level.

**Parameters:**
- `name` (string): Name of actor to delete

### set_actor_transform
Modify actor position, rotation, and scale.

**Parameters:**
- `name` (string): Actor to transform
- `location` (array): New world position (optional)
- `rotation` (array): New rotation in degrees (optional)  
- `scale` (array): New scale factors (optional)

---

## 🎨 Material Analysis Tools

Tools for analyzing existing UE materials.

### get_available_materials
List all materials available in the project.

**Parameters:**
- `search_path` (string): Path to search (default: "/Game/")

**Returns:** List of material paths.

**Example:**
```bash
get_available_materials(search_path="/Engine/BasicShapes/")
```

### get_material_properties
Get material attributes including blend mode, shading model, etc.

**Parameters:**
- `material_name` (string): Material path or name

**Returns:**
- `blend_mode`: Opaque, Masked, Translucent, Additive, etc.
- `shading_models`: List of enabled shading models
- `two_sided`: Whether material is two-sided
- `material_domain`: Surface, UI, PostProcess, etc.
- `is_masked`: Whether material uses masking
- `opacity_mask_clip_value`: Clip threshold for masked materials

**Example:**
```bash
get_material_properties(material_name="/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
```

### get_material_expressions
List all expression nodes in a material graph.

**Parameters:**
- `material_name` (string): Material path or name

**Returns:** List of nodes with:
- `node_id`: Unique identifier
- `type`: Expression class name
- `position`: [x, y] in graph
- Parameters specific to each expression type
- `function_path`: For MaterialFunctionCall nodes

**Example:**
```bash
get_material_expressions(material_name="/Game/Materials/M_Water")
```

### get_material_connections
Get node connection relationships in a material.

**Parameters:**
- `material_name` (string): Material path or name

**Returns:**
- `node_connections`: List of nodes with input connections
- `property_connections`: Connections to material properties (BaseColor, Normal, etc.)

**Example:**
```bash
get_material_connections(material_name="/Game/Materials/M_Water")
```

### get_material_function_content
Get detailed content of a Material Function including internal node connections.

**Parameters:**
- `function_path` (string): Full path to the Material Function

**Returns:**
- `inputs`: Function input parameters
- `outputs`: Function output pins
- `expressions`: Internal expression nodes (with `node_id` for each)
- `connections`: Node connection relationships (new!)
  - `from`: Source node ID
  - `to`: Target node ID
  - `from_output`: Output pin name on source node
  - `to_input`: Input pin name on target node
- `connection_count`: Total number of connections

**Example:**
```bash
get_material_function_content(
    function_path="/Engine/Functions/Engine_MaterialFunctions01/Texturing/BitMask.BitMask"
)
```

**Connection Example:**
```json
{
  "connections": [
    {
      "from": "Expr_Constant_1234",
      "to": "Expr_Multiply_5678",
      "from_output": "Output_0",
      "to_input": "A"
    }
  ]
}
```

**Note:** This allows full reconstruction of Material Function node graphs, including nested MaterialFunctionCalls.

**See Also:** `material-analysis.md` for complete workflow documentation.

---

## 📷 Viewport Tools

Tools for capturing and analyzing viewport content.

### get_viewport_screenshot
Capture a screenshot of the current viewport and save as image file.

**Parameters:**
- `output_path` (string, **required**): Full path where to save the screenshot (e.g., "C:/temp/screenshot.png")
- `format` (string): Image format - "png", "jpg", or "bmp" (default: "png")
- `quality` (int): JPEG quality 1-100 (only for jpg, default: 85)
- `include_ui` (bool): Whether to include editor UI (default: false)

**Returns:**
- `success`: Whether the capture succeeded
- `file_path`: Path to the saved image file
- `format`: Image format
- `width`: Image width in pixels
- `height`: Image height in pixels
- `size_bytes`: Size of the image data in bytes
- `viewport_type`: Type of viewport captured ("Editor", "PIE", or "Game")

**Example:**
```bash
# Capture PNG screenshot
get_viewport_screenshot(output_path="C:/temp/screenshot.png")

# Capture JPEG with lower quality
get_viewport_screenshot(output_path="C:/temp/screenshot.jpg", format="jpg", quality=50)
```

**Note:** Captures the active viewport - either the editor viewport or PIE/Game viewport. The image is saved directly to the specified file path.

---

## 💡 Light Tools

Tools for creating and managing lights in the scene.

### create_light
Create a light in the scene.

**Parameters:**
- `light_type` (string): Type of light - "point", "directional", "spot", "rect" (default: "point")
- `name` (string): Name for the light (auto-generated if not provided)
- `location` (array): World position [x, y, z] (default: [0, 0, 200])
- `rotation` (array): World rotation [pitch, yaw, roll] in degrees
- `intensity` (float): Light intensity/brightness
- `color` (array): Light color [r, g, b] or [r, g, b, a] (0.0-1.0)
- `mobility` (string): "movable", "stationary", or "static" (default: "movable")
- `cast_shadows` (bool): Whether light casts shadows (default: true)
- `attenuation_radius` (float): Attenuation radius for point/spot lights
- `inner_cone_angle` (float): Inner cone angle for spot lights (degrees)
- `outer_cone_angle` (float): Outer cone angle for spot lights (degrees)
- `source_radius` (float): Source radius for soft shadows

**Example:**
```bash
# Create a point light
create_light(light_type="point", intensity=1000.0, location=[0, 0, 300])

# Create a directional light (sun)
create_light(light_type="directional", intensity=10.0, rotation=[-45, 0, 0])

# Create a spot light
create_light(light_type="spot", intensity=2000.0, outer_cone_angle=45.0)
```

### set_light_properties
Set properties of an existing light.

**Parameters:**
- `name` (string): Name or path of the light (required)
- `intensity` (float): Light intensity/brightness
- `color` (array): Light color [r, g, b] or [r, g, b, a]
- `temperature` (float): Color temperature in Kelvin (1700-12000)
- `use_temperature` (bool): Whether to use temperature instead of color
- `cast_shadows` (bool): Whether light casts shadows
- `location` (array): World position [x, y, z]
- `rotation` (array): World rotation [pitch, yaw, roll]
- ... (more type-specific parameters)

**Example:**
```bash
set_light_properties(name="PointLight_0", intensity=5000.0, color=[1.0, 0.9, 0.8])
```

### get_lights
Get list of all lights in the current level.

**Parameters:**
- `light_type` (string): Optional filter by type - "point", "directional", "spot", "rect"

**Returns:** List of lights with name, type, location, intensity, color, etc.

**Example:**
```bash
get_lights()
get_lights(light_type="directional")
```

### delete_light
Delete a light from the scene.

**Parameters:**
- `name` (string): Name or path of the light to delete

**Example:**
```bash
delete_light(name="PointLight_0")
```

---

## 💡 Usage Tips

### Performance Optimization
- Use advanced composition tools instead of individual spawning
- Keep total actor counts reasonable (< 1000 actors)
- Use physics sparingly for better performance

### Naming Conventions
- Use descriptive, unique names for all actors
- Include prefixes for grouped objects (e.g., "House1_Wall", "House1_Roof")
- Avoid special characters in actor names

### Coordinate Guidelines  
- Place objects at Z > 0 to avoid ground clipping
- Use large separation distances for multiple structures
- Remember Unreal uses centimeters (100 = 1 meter)

### Blueprint Workflow
1. Create Blueprint class
2. Add required components  
3. Set component properties (mesh, physics, materials)
4. Compile Blueprint
5. Spawn actors from compiled Blueprint

---