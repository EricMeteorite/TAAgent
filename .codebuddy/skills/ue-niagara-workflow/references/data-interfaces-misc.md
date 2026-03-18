# Niagara Misc DataInterfaces

Renderer info, texture arrays, and utility interfaces.

---

## MeshRendererInfo DataInterface

### Overview

`UNiagaraDataInterfaceMeshRendererInfo` provides access to mesh renderer properties - number of meshes, bounds, and SubUV configuration.

### Shader Parameters

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
    SHADER_PARAMETER(uint32, bSubImageBlend)
    SHADER_PARAMETER(FVector2f, SubImageSize)
    SHADER_PARAMETER(uint32, NumMeshes)
    SHADER_PARAMETER_SRV(Buffer<float>, MeshDataBuffer)
END_SHADER_PARAMETER_STRUCT()
```

### Functions

| Function | Output | Description |
|----------|--------|-------------|
| `GetNumMeshes` | int | Number of meshes in renderer |
| `GetMeshLocalBounds` | Min, Max, Size | Local bounds of mesh by index |
| `GetSubUVDetails` | BlendEnabled, SubImageSize | SubUV atlas configuration |

### GPU Shader

```hlsl
// NiagaraDataInterfaceMeshRendererInfoTemplate.ush
uint    {ParameterName}_NumMeshes;
uint    {ParameterName}_bSubImageBlend;
float2  {ParameterName}_SubImageSize;
Buffer<float> {ParameterName}_MeshDataBuffer;

void GetNumMeshes_{ParameterName}(out int OutNumMeshes)
{
    OutNumMeshes = {ParameterName}_NumMeshes;
}

void GetMeshLocalBounds_{ParameterName}(int MeshIndex, out float3 OutMinBounds, out float3 OutMaxBounds, out float3 OutSize)
{
    if (MeshIndex >= 0 && MeshIndex < int({ParameterName}_NumMeshes))
    {
        // Mesh data buffer layout: [MinX, MinY, MinZ, MaxX, MaxY, MaxZ, SizeX, SizeY, SizeZ] per mesh
        uint BaseIndex = MeshIndex * 9;
        OutMinBounds = float3(
            {ParameterName}_MeshDataBuffer[BaseIndex + 0],
            {ParameterName}_MeshDataBuffer[BaseIndex + 1],
            {ParameterName}_MeshDataBuffer[BaseIndex + 2]
        );
        OutMaxBounds = float3(
            {ParameterName}_MeshDataBuffer[BaseIndex + 3],
            {ParameterName}_MeshDataBuffer[BaseIndex + 4],
            {ParameterName}_MeshDataBuffer[BaseIndex + 5]
        );
        OutSize = float3(
            {ParameterName}_MeshDataBuffer[BaseIndex + 6],
            {ParameterName}_MeshDataBuffer[BaseIndex + 7],
            {ParameterName}_MeshDataBuffer[BaseIndex + 8]
        );
    }
    else
    {
        OutMinBounds = 0;
        OutMaxBounds = 0;
        OutSize = 0;
    }
}

void GetSubUVDetails_{ParameterName}(out bool BlendEnabled, out float2 SubImageSize)
{
    BlendEnabled = {ParameterName}_bSubImageBlend != 0;
    SubImageSize = {ParameterName}_SubImageSize;
}
```

### Use Cases

```hlsl
// 1. Scale particle based on mesh size
float3 MeshSize;
GetMeshLocalBounds_MeshInfo(MeshIndex, Min, Max, MeshSize);
float MaxDimension = max(max(MeshSize.x, MeshSize.y), MeshSize.z);
SpriteSize = MaxDimension * ScaleFactor;

// 2. Center particles on mesh origin
float3 MeshCenter = (Min + Max) * 0.5;
Position += MeshCenter;

// 3. SubUV atlas selection
bool BlendEnabled;
float2 SubImageSize;
GetSubUVDetails_MeshInfo(BlendEnabled, SubImageSize);
int SubImageIndex = floor(Random * SubImageSize.x * SubImageSize.y);
```

---

## SpriteRendererInfo DataInterface

### Overview

`UNiagaraDataInterfaceSpriteRendererInfo` provides sprite renderer configuration for particle alignment and sorting.

### Key Properties

- Alignment mode (Facing, Velocity, Custom)
- Sorting mode
- Facing camera mode

---

## CubeTexture DataInterface

### Overview

`UNiagaraDataInterfaceCubeTexture` provides cubemap sampling for environment reflections and omnidirectional effects.

### GPU-Only

```cpp
Sig.bSupportsCPU = false;
Sig.bSupportsGPU = true;
```

### Instance Data

```cpp
struct FNDICubeTextureInstanceData_GameThread
{
    TWeakObjectPtr<UTexture> CurrentTexture;
    FIntPoint CurrentTextureSize;
    FNiagaraParameterDirectBinding<UObject*> UserParamBinding;
};

struct FNDICubeTextureInstanceData_RenderThread
{
    FSamplerStateRHIRef SamplerStateRHI;
    FTextureReferenceRHIRef TextureReferenceRHI;
    FIntPoint TextureSize;
    FRDGTextureRef TransientRDGTexture;
};
```

### GPU Shader

```hlsl
// NiagaraDataInterfaceCubeTextureTemplate.ush
TextureCube     {ParameterName}_CubeTexture;
SamplerState    {ParameterName}_CubeTextureSampler;
int2            {ParameterName}_TextureSize;

void SampleCubeTexture_{ParameterName}(float3 UVW, float MipLevel, out float4 Value)
{
    Value = {ParameterName}_CubeTexture.SampleLevel(
        {ParameterName}_CubeTextureSampler, UVW, MipLevel);
}

void TextureDimensions_{ParameterName}(out int Width, out int Height)
{
    Width = {ParameterName}_TextureSize.x;
    Height = {ParameterName}_TextureSize.y;
}
```

### Use Cases

```hlsl
// 1. Environment reflection
float3 ReflectionDir = reflect(-ViewDir, Normal);
float3 EnvColor;
SampleCubeTexture_EnvMap(ReflectionDir, 0, EnvColor);

// 2. Ambient lighting
float3 Ambient = SampleCubeTexture_AmbientCube(Normal, 0);

// 3. Omni shadow lookup
float ShadowDepth = SampleCubeTexture_ShadowMap(LightDir, 0);
```

### Supported Assets

| Asset Type | Description |
|------------|-------------|
| UTextureCube | Static cubemap |
| UTextureRenderTargetCube | Dynamic cubemap |

---

## 2DArrayTexture DataInterface

### Overview

`UNiagaraDataInterface2DArrayTexture` provides access to Texture2DArray assets - stacks of 2D textures with identical dimensions.

### GPU Shader

```hlsl
// NiagaraDataInterfaceTexture2DArrayTemplate.ush
Texture2DArray  {ParameterName}_Texture2DArray;
SamplerState    {ParameterName}_Texture2DArraySampler;
float3          {ParameterName}_TextureSize;  // x,y = dimensions, z = num slices

// Direct texel load
void LoadTexture_{ParameterName}(int TexelX, int TexelY, int TexelZ, int MipLevel, out float4 Value)
{
    Value = {ParameterName}_Texture2DArray.Load(int4(TexelX, TexelY, TexelZ, MipLevel));
}

// Bilinear sampling
void SampleTexture_{ParameterName}(float3 UVW, float MipLevel, out float4 Value)
{
    // UVW.xy = texture coordinates
    // UVW.z = slice index
    Value = {ParameterName}_Texture2DArray.SampleLevel(
        {ParameterName}_Texture2DArraySampler, UVW, MipLevel);
}

// Gather for gradient computation
void GatherRedTexture_{ParameterName}(float3 UVW, out float4 Value)
{
    // Returns 4 red samples for gradient computation
    Value = {ParameterName}_Texture2DArray.GatherRed(
        {ParameterName}_Texture2DArraySampler, UVW);
}

void TextureDimensions_{ParameterName}(out float3 Dimensions)
{
    Dimensions = {ParameterName}_TextureSize;
}
```

### Use Cases

| Slice Index | Use Case |
|-------------|----------|
| 0-15 | Animated flipbook frames |
| 0-5 | Cascaded shadow maps |
| 0-N | Per-particle texture variation |
| 0-N | Texture atlas pages |

### Example: Random Texture Variation

```hlsl
// Each particle uses a different texture
float RandomValue = Random(ParticleID);
int SliceIndex = int(RandomValue * NumSlices);

float3 UVW = float3(UV, SliceIndex);
float4 Color;
SampleTexture_TextureArray(UVW, 0, Color);
```

---

## RWBase DataInterface

### Overview

`UNiagaraDataInterfaceRWBase` is the base class for read-write data interfaces, providing common grid/array functionality.

### Common Functions

| Function | Description |
|----------|-------------|
| `SetNumCells` | Resize grid dimensions |
| `GetWorldBBoxSize` | Get simulation bounds |
| `GetNumCells` | Get current grid resolution |

### Resolution Methods

```cpp
enum class ESetResolutionMethod : uint8
{
    CellSize,    // Specify cell size, compute resolution
    CellCount    // Specify cell count directly
};
```

---

## PlatformSet DataInterface

### Overview

`UNiagaraDataInterfacePlatformSet` provides platform-specific parameter overrides for scalability.

### Usage

Define different values per platform:

```
Default: Value = 100
Mobile:  Value = 50
Console: Value = 75
```

---

## Comparison Matrix

### Texture DataInterfaces

| Interface | Dimensions | GPU Read | GPU Write | Special |
|-----------|------------|----------|-----------|---------|
| Texture | 2D | ✓ | ✗ | Pseudo-volume |
| CubeTexture | Cubemap | ✓ | ✗ | Directional |
| VolumeTexture | 3D | ✓ | ✗ | - |
| 2DArrayTexture | 2D + Slice | ✓ | ✗ | - |
| SparseVolumeTexture | Sparse 3D | ✓ | ✗ | Streaming |
| RenderTarget2D | 2D | ✓ | ✓ | Mip gen |
| RenderTarget2DArray | 2D + Slice | ✓ | ✓ | - |
| RenderTargetCube | Cubemap | ✓ | ✓ | - |
| RenderTargetVolume | 3D | ✓ | ✓ | - |

### Renderer Info DataInterfaces

| Interface | CPU | GPU | Purpose |
|-----------|-----|-----|---------|
| MeshRendererInfo | ✓ | ✓ | Mesh bounds, count |
| SpriteRendererInfo | ✓ | ✗ | Sprite alignment |

---

## Memory Estimation

### Texture2DArray

| Resolution | Slices | Float4 Memory |
|------------|--------|---------------|
| 512² | 4 | 4 MB |
| 1024² | 8 | 32 MB |
| 2048² | 16 | 256 MB |

### CubeTexture

| Resolution | Float4 Memory |
|------------|---------------|
| 256² | 1.5 MB |
| 512² | 6 MB |
| 1024² | 24 MB |

---

## Best Practices

### MeshRendererInfo

```hlsl
// Good: Use for initialization
if (SpawnIndex == 0)
{
    GetMeshLocalBounds_MeshInfo(0, Min, Max, Size);
    // Store for later use
}
```

### Texture Arrays

```hlsl
// Good: Pre-compute slice index
int SliceIndex = int(AttributeReader.GetFloat("TextureIndex"));

// Avoid: Dynamic slice selection per frame
int Slice = floor(Time * FrameRate);  // Causes cache thrashing
```

---

## References

- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceMeshRendererInfo.cpp`
- Cube Texture: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceCubeTexture.cpp`
- 2D Array: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterface2DArrayTexture.cpp`
