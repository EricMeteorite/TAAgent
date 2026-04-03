# Niagara Grid DataInterfaces

Grid-based simulation data structures for fluid dynamics and spatial queries.

---

## Grid3DCollection DataInterface

### Overview

`UNiagaraDataInterfaceGrid3DCollection` provides 3D volumetric grid storage for smoke, fire, and other volumetric effects. Uses Texture3D for GPU efficiency.

### Shader Parameters

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FNDIGrid3DShaderParameters, )
    SHADER_PARAMETER(int, NumAttributes)
    SHADER_PARAMETER(int, NumNamedAttributes)
    SHADER_PARAMETER(FVector3f, UnitToUV)
    SHADER_PARAMETER(FIntVector, NumCells)
    SHADER_PARAMETER(FVector3f, CellSize)
    SHADER_PARAMETER(FIntVector, NumTiles)
    SHADER_PARAMETER(FVector3f, OneOverNumTiles)
    SHADER_PARAMETER(FVector3f, UnitClampMin)
    SHADER_PARAMETER(FVector3f, UnitClampMax)
    SHADER_PARAMETER(FVector3f, WorldBBoxSize)
    
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D<float>, Grid)
    SHADER_PARAMETER_SAMPLER(SamplerState, GridSampler)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, OutputGrid)
    SHADER_PARAMETER_SRV(Buffer<float4>, PerAttributeData)
END_SHADER_PARAMETER_STRUCT()
```

### GPU Shader Implementation

**Parameter Declaration:**

```hlsl
// NiagaraDataInterfaceGrid3DCollection.ush
Texture3D<float>     {ParameterName}_Grid;
SamplerState         {ParameterName}_GridSampler;
RWTexture3D<float>   {ParameterName}_OutputGrid;
Buffer<float4>       {ParameterName}_PerAttributeData;

int    {ParameterName}_NumAttributes;
int    {ParameterName}_NumNamedAttributes;
int3   {ParameterName}_NumCells;
float3 {ParameterName}_CellSize;
float3 {ParameterName}_UnitToUV;
int3   {ParameterName}_NumTiles;
float3 {ParameterName}_OneOverNumTiles;
float3 {ParameterName}_UnitClampMin;
float3 {ParameterName}_UnitClampMax;
```

**Attribute Index Mapping:**

```hlsl
// Named attributes stored in PerAttributeData buffer
// Format: float4(NameHash, SliceIndex, ComponentIndex, Padding)

int GetVector4AttributeIndex_{ParameterName}(FName AttributeName)
{
    uint NameHash = GetNameHash(AttributeName);
    
    for (int i = 0; i < {ParameterName}_NumNamedAttributes; ++i)
    {
        float4 AttrData = {ParameterName}_PerAttributeData[i];
        if (asuint(AttrData.x) == NameHash)
        {
            return int(AttrData.y);  // Slice index
        }
    }
    return -1;
}
```

**Grid Value Access:**

```hlsl
// Direct cell access
void SetGridValue_{ParameterName}(int3 CellIndex, int AttributeIndex, float4 Value)
{
    if (all(CellIndex >= 0) && all(CellIndex < {ParameterName}_NumCells))
    {
        int3 WriteCoord = int3(
            (CellIndex.x % {ParameterName}_NumTiles.x) * ({ParameterName}_NumCells.x / {ParameterName}_NumTiles.x) + CellIndex.x / {ParameterName}_NumTiles.x,
            CellIndex.y,
            CellIndex.z * {ParameterName}_NumTiles.z + AttributeIndex
        );
        
        {ParameterName}_OutputGrid[WriteCoord] = Value;
    }
}

float4 GetPreviousGridValue_{ParameterName}(int3 CellIndex, int AttributeIndex)
{
    if (any(CellIndex < 0) || any(CellIndex >= {ParameterName}_NumCells))
        return 0;
    
    float3 UVW = (float3(CellIndex) + 0.5) * {ParameterName}_UnitToUV;
    UVW.z = (UVW.z + float(AttributeIndex)) * {ParameterName}_OneOverNumTiles.z;
    
    return {ParameterName}_Grid.SampleLevel({ParameterName}_GridSampler, UVW, 0);
}
```

**Trilinear Sampling:**

```hlsl
float4 SamplePreviousGrid_{ParameterName}(float3 WorldPosition, int AttributeIndex)
{
    // Transform world to unit space
    float3 UnitPos = (WorldPosition - WorldBBoxMin) / {ParameterName}_WorldBBoxSize;
    
    // Clamp to valid range
    UnitPos = clamp(UnitPos, {ParameterName}_UnitClampMin, {ParameterName}_UnitClampMax);
    
    // Convert to UV space
    float3 UVW = UnitPos * {ParameterName}_UnitToUV;
    UVW.z = (UVW.z + float(AttributeIndex)) * {ParameterName}_OneOverNumTiles.z;
    
    return {ParameterName}_Grid.SampleLevel({ParameterName}_GridSampler, UVW, 0);
}
```

**Cubic Interpolation (Higher Quality):**

```hlsl
float4 CubicSamplePreviousGrid_{ParameterName}(float3 WorldPosition, int AttributeIndex)
{
    float3 UnitPos = (WorldPosition - WorldBBoxMin) / {ParameterName}_WorldBBoxSize;
    float3 GridPos = UnitPos * float3({ParameterName}_NumCells) - 0.5;
    int3 CellBase = int3(floor(GridPos));
    float3 t = frac(GridPos);
    
    float4 Result = 0;
    float WeightSum = 0;
    
    // 4x4x4 kernel for tricubic
    [unroll]
    for (int z = -1; z <= 2; ++z)
    {
        [unroll]
        for (int y = -1; y <= 2; ++y)
        {
            [unroll]
            for (int x = -1; x <= 2; ++x)
            {
                int3 SampleCell = CellBase + int3(x, y, z);
                float4 Value = GetPreviousGridValue_{ParameterName}(SampleCell, AttributeIndex);
                
                // Cubic weights
                float wx = CubicWeight(x, t.x);
                float wy = CubicWeight(y, t.y);
                float wz = CubicWeight(z, t.z);
                
                Result += Value * wx * wy * wz;
                WeightSum += wx * wy * wz;
            }
        }
    }
    
    return Result / WeightSum;
}

float CubicWeight(int Offset, float t)
{
    // Monotonic cubic weights (Catmull-Rom variant)
    float s = float(Offset) + t;
    s = abs(s);
    
    if (s >= 2.0) return 0.0;
    if (s >= 1.0) return (2.0 - s) * (2.0 - s) * (2.0 - s) / 6.0;
    return (4.0 - 6.0 * s * s + 3.0 * s * s * s) / 6.0;
}
```

### Volume Rendering Integration

```
┌─────────────────────────────────────────────────────────────────┐
│              Grid3D Smoke Simulation Pipeline                    │
├─────────────────────────────────────────────────────────────────┤
│ Stage 1: Advection (Semi-Lagrangian)                            │
│   - velocity_new = Sample(velocity, position - velocity * dt)   │
│   - density_new = Sample(density, position - velocity * dt)     │
│                                                                  │
│ Stage 2: Buoyancy                                                │
│   - buoyancy = density * buoyancy_factor * up_vector            │
│   - velocity += buoyancy * dt                                   │
│                                                                  │
│ Stage 3: Vorticity Confinement                                   │
│   - curl = ∇ × velocity                                         │
│   - confinement = normalize(∇|curl|) × curl                     │
│   - velocity += confinement * strength * dt                     │
│                                                                  │
│ Stage 4: Dissipation                                             │
│   - density *= exp(-dissipation * dt)                           │
│   - velocity *= exp(-velocity_dissipation * dt)                 │
│                                                                  │
│ Stage 5: Boundary Conditions                                     │
│   - Clamp values at boundaries                                  │
│   - Apply collision fields                                      │
└─────────────────────────────────────────────────────────────────┘
```

### Console Variables

```ini
fx.Niagara.Grid3D.ResolutionMultiplier=1.0    ; Global resolution scale
fx.Niagara.Grid3D.OverrideFormat=-1           ; Force pixel format
```

---

## RasterizationGrid3D DataInterface

### Overview

`UNiagaraDataInterfaceRasterizationGrid3D` provides a specialized grid for rasterizing particles into voxels - useful for foam, bubbles, and splashes in fluid simulation.

### Key Difference from Grid3DCollection

| Feature | Grid3DCollection | RasterizationGrid3D |
|---------|------------------|---------------------|
| Primary Use | Smoke, fire | Foam, splash particles |
| Data Type | Float grids | Particle ID + Count |
| Access Pattern | Sample everywhere | Write sparse |
| Memory | Full grid | Compressed sparse |

### GPU Implementation

**Buffer Structure:**

```hlsl
RWTexture3D<uint>   {ParameterName}_RasterizedGrid;  // Packed particle data
RWBuffer<uint>      {ParameterName}_ParticleList;     // List of particles per cell
RWBuffer<uint>      {ParameterName}_CellCount;        // Count per cell

int3   {ParameterName}_NumCells;
float3 {ParameterName}_CellSize;
float3 {ParameterName}_WorldBBoxMin;
uint   {ParameterName}_MaxParticlesPerCell;
```

**Particle Rasterization:**

```hlsl
void RasterizeParticle_{ParameterName}(float3 WorldPosition, uint ParticleID)
{
    // Compute cell index
    float3 LocalPos = WorldPosition - {ParameterName}_WorldBBoxMin;
    int3 CellIndex = int3(floor(LocalPos / {ParameterName}_CellSize));
    
    // Bounds check
    if (any(CellIndex < 0) || any(CellIndex >= {ParameterName}_NumCells))
        return;
    
    // Linear index
    uint LinearIndex = CellIndex.x + CellIndex.y * {ParameterName}_NumCells.x 
                     + CellIndex.z * {ParameterName}_NumCells.x * {ParameterName}_NumCells.y;
    
    // Atomic increment to get slot
    uint Slot;
    InterlockedAdd({ParameterName}_CellCount[LinearIndex], 1, Slot);
    
    if (Slot < {ParameterName}_MaxParticlesPerCell)
    {
        uint WriteIndex = LinearIndex * {ParameterName}_MaxParticlesPerCell + Slot;
        {ParameterName}_ParticleList[WriteIndex] = ParticleID;
    }
}
```

**Query Rasterized Particles:**

```hlsl
uint GetParticleCount_{ParameterName}(int3 CellIndex)
{
    if (any(CellIndex < 0) || any(CellIndex >= {ParameterName}_NumCells))
        return 0;
    
    uint LinearIndex = CellIndex.x + CellIndex.y * {ParameterName}_NumCells.x 
                     + CellIndex.z * {ParameterName}_NumCells.x * {ParameterName}_NumCells.y;
    
    return min({ParameterName}_CellCount[LinearIndex], {ParameterName}_MaxParticlesPerCell);
}

uint GetParticleID_{ParameterName}(int3 CellIndex, uint Slot)
{
    uint LinearIndex = CellIndex.x + CellIndex.y * {ParameterName}_NumCells.x 
                     + CellIndex.z * {ParameterName}_NumCells.x * {ParameterName}_NumCells.y;
    
    uint ReadIndex = LinearIndex * {ParameterName}_MaxParticlesPerCell + Slot;
    return {ParameterName}_ParticleList[ReadIndex];
}
```

### Use Cases

| Use Case | Grid Type | Typical Resolution |
|----------|-----------|-------------------|
| Smoke plume | Grid3DCollection | 128³ - 256³ |
| Fire effect | Grid3DCollection | 64³ - 128³ |
| Water foam | RasterizationGrid3D | 64³ - 128³ |
| Splash particles | RasterizationGrid3D | 32³ - 64³ |
| Clouds | Grid3DCollection | 256³+ |

---

## Grid Comparison Matrix

### Performance Characteristics

| Grid Type | Memory | Bandwidth | Parallelism |
|-----------|--------|-----------|-------------|
| Grid2DCollection | O(N²) | Medium | High (2D dispatch) |
| Grid3DCollection | O(N³) | High | High (3D dispatch) |
| RasterizationGrid3D | O(N³ × MaxPerCell) | Low-Medium | Medium (particle dispatch) |

### Recommended Resolutions

| Target Platform | Max Grid2D | Max Grid3D |
|-----------------|------------|------------|
| High-end PC | 2048² | 256³ |
| Console | 1024² | 128³ |
| Mobile | 512² | 64³ |

---

## References

- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceGrid3DCollection.cpp`
- Shader: `Engine/Plugins/FX/Niagara/Shaders/Private/NiagaraDataInterfaceGrid3DCollection.ush`
