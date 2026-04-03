# Niagara RenderTarget DataInterfaces

GPU texture read/write operations for particle effects.

---

## RenderTarget2D DataInterface

### Overview

`UNiagaraDataInterfaceRenderTarget2D` provides GPU-only render target read/write access. Enables particles to sample and modify textures in real-time.

### GPU-Only Design

```cpp
// All texture operations are GPU-only
Sig.bSupportsCPU = false;
```

### Shader Parameters

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
    SHADER_PARAMETER(FIntPoint, TextureSize)
    SHADER_PARAMETER(int, MipLevels)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWTexture)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, Texture)
    SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
END_SHADER_PARAMETER_STRUCT()
```

### GPU Shader Implementation

**Parameter Declaration:**

```hlsl
// NiagaraDataInterfaceRenderTarget2DTemplate.ush
int2            {ParameterName}_TextureSize;
int             {ParameterName}_MipLevels;
Texture2D       {ParameterName}_Texture;
SamplerState    {ParameterName}_TextureSampler;
RWTexture2D<float4> {ParameterName}_RWTexture;
```

**Core Functions:**

```hlsl
// Get texture dimensions
void GetRenderTargetSize_{ParameterName}(out int Width, out int Height)
{
    Width = {ParameterName}_TextureSize.x;
    Height = {ParameterName}_TextureSize.y;
}

// Direct texel load (no filtering)
void LoadRenderTargetValue_{ParameterName}(int IndexX, int IndexY, int MipLevel, out float4 Value)
{
    Value = {ParameterName}_Texture.Load(int3(IndexX, IndexY, MipLevel));
}

// Hardware filtered sampling
void SampleRenderTargetValue_{ParameterName}(float2 UV, float MipLevel, out float4 Value)
{
    Value = {ParameterName}_Texture.SampleLevel({ParameterName}_TextureSampler, UV, MipLevel);
}

// Write to texture (GPU only)
void SetRenderTargetValue_{ParameterName}(bool Enabled, int IndexX, int IndexY, float4 Value)
{
    if (Enabled)
    {
        {ParameterName}_RWTexture[int2(IndexX, IndexY)] = Value;
    }
}

// Linear index to 2D coordinate
void LinearToIndex_{ParameterName}(int Linear, out int IndexX, out int IndexY)
{
    IndexX = Linear % {ParameterName}_TextureSize.x;
    IndexY = Linear / {ParameterName}_TextureSize.x;
}

// Execution index to texture coordinate (for parallel writes)
void ExecToIndex_{ParameterName}(out int IndexX, out int IndexY)
{
    // Maps dispatch thread ID to texture coordinates
    uint3 DispatchThreadId = GetDispatchThreadId();
    LinearToIndex_{ParameterName}(DispatchThreadId.x, IndexX, IndexY);
}
```

### Configuration Options

| Property | Description |
|----------|-------------|
| `Size` | Default resolution (Width × Height) |
| `MipMapGeneration` | Enable mipmap generation |
| `MipMapGenerationType` | "Blur5" or "ComputeShader" |
| `OverrideRenderTargetFormat` | Force specific pixel format |
| `OverrideRenderTargetFilter` | Sampler filter mode |
| `RenderTargetUserParameter` | Bind external render target |

### Use Cases

```
┌─────────────────────────────────────────────────────────────────┐
│              RenderTarget2D Use Cases                            │
├─────────────────────────────────────────────────────────────────┤
│  1. Rain Drop Ripples                                            │
│     - CPU: SetSize() at spawn                                    │
│     - GPU: Write ripple patterns per particle                    │
│     - GPU: Sample for wave propagation                           │
│                                                                  │
│  2. Dynamic Decals                                               │
│     - Write: Particles paint on texture                          │
│     - Read: Sample for renderer                                  │
│                                                                  │
│  3. Feedback Effects                                             │
│     - Read previous frame                                        │
│     - Process (blur, displace)                                   │
│     - Write modified result                                      │
│                                                                  │
│  4. Simulation Data                                              │
│     - Store per-pixel density/velocity                           │
│     - Iterate with compute shaders                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## RenderTarget2DArray DataInterface

### Overview

`UNiagaraDataInterfaceRenderTarget2DArray` extends RenderTarget2D with texture array support for multi-layer effects.

### GPU Shader

```hlsl
Texture2DArray   {ParameterName}_Texture;
RWTexture2DArray<float4> {ParameterName}_RWTexture;
int              {ParameterName}_NumSlices;

void SetRenderTargetValue_{ParameterName}(bool Enabled, int IndexX, int IndexY, int Slice, float4 Value)
{
    if (Enabled)
    {
        {ParameterName}_RWTexture[int3(IndexX, IndexY, Slice)] = Value;
    }
}

void LoadRenderTargetValue_{ParameterName}(int IndexX, int IndexY, int Slice, int MipLevel, out float4 Value)
{
    Value = {ParameterName}_Texture.Load(int4(IndexX, IndexY, Slice, MipLevel));
}
```

### Use Cases

| Slice Index | Use Case |
|-------------|----------|
| 0-3 | Cascaded shadow maps |
| 0-5 | Cubemap faces |
| 0-N | Depth peeling layers |
| 0-N | Temporal history |

---

## RenderTargetCube DataInterface

### Overview

`UNiagaraDataInterfaceRenderTargetCube` provides cubemap rendering and sampling for 360° effects.

### GPU Shader

```hlsl
TextureCube     {ParameterName}_Texture;
RWTexture2DArray<float4> {ParameterName}_RWTexture;  // Stored as 2D array

// Cubemap face indices
// 0 = +X, 1 = -X, 2 = +Y, 3 = -Y, 4 = +Z, 5 = -Z

float4 SampleCubemap_{ParameterName}(float3 Direction, float MipLevel)
{
    return {ParameterName}_Texture.SampleLevel({ParameterName}_TextureSampler, Direction, MipLevel);
}

// World position to cubemap direction
float3 WorldToCubemapDirection(float3 WorldPos, float3 CenterPos)
{
    return normalize(WorldPos - CenterPos);
}
```

### Use Cases

- Dynamic environment maps
- Reflection probes
- 360° particle effects
- Omnidirectional shadows

---

## RenderTargetVolume DataInterface

### Overview

`UNiagaraDataInterfaceRenderTargetVolume` provides 3D texture rendering for volumetric effects.

### GPU Shader

```hlsl
Texture3D       {ParameterName}_Texture;
RWTexture3D<float4> {ParameterName}_RWTexture;
int3            {ParameterName}_TextureSize;

void SetVolumeValue_{ParameterName}(int3 Index, float4 Value)
{
    if (all(Index >= 0) && all(Index < {ParameterName}_TextureSize))
    {
        {ParameterName}_RWTexture[Index] = Value;
    }
}

float4 SampleVolume_{ParameterName}(float3 UVW, float MipLevel)
{
    return {ParameterName}_Texture.SampleLevel({ParameterName}_TextureSampler, UVW, MipLevel);
}
```

### Use Cases

- 3D fluid simulation
- Volumetric fog
- 3D noise baking

---

## IntRenderTarget2D DataInterface

### Overview

`UNiagaraDataInterfaceIntRenderTarget2D` provides integer texture format support for indexing and counting operations.

### GPU Shader

```hlsl
RWTexture2D<int> {ParameterName}_RWTexture;
Texture2D<int>   {ParameterName}_Texture;

void SetIntValue_{ParameterName}(int2 Index, int Value)
{
    {ParameterName}_RWTexture[Index] = Value;
}

int GetIntValue_{ParameterName}(int2 Index)
{
    return {ParameterName}_Texture.Load(int3(Index, 0)).x;
}

// Atomic operations
int AtomicAddInt_{ParameterName}(int2 Index, int Value)
{
    int Previous;
    InterlockedAdd({ParameterName}_RWTexture[Index], Value, Previous);
    return Previous;
}
```

### Use Cases

- Particle ID storage
- Counter buffers
- Index maps
- Stencil-like operations

---

## Comparison Matrix

### RenderTarget Types

| Type | Dimensions | Format | Atomic Ops | Best For |
|------|------------|--------|------------|----------|
| RenderTarget2D | 2D | Float4 | No | Standard effects |
| RenderTarget2DArray | 2D + Slice | Float4 | No | Layered effects |
| RenderTargetCube | Cubemap | Float4 | No | Environment |
| RenderTargetVolume | 3D | Float4 | No | Volumetric |
| IntRenderTarget2D | 2D | R32_SINT | Yes | Indexing/counting |

### Memory Requirements

| Resolution | Format | Memory (no mip) |
|------------|--------|-----------------|
| 512² | Float4 | 4 MB |
| 1024² | Float4 | 16 MB |
| 2048² | Float4 | 64 MB |
| 256³ | Float4 | 64 MB |
| 128³ | Float4 | 8 MB |

### Console Variables

```ini
fx.Niagara.RenderTarget2D.SimCacheCompressed=true  ; Use Oodle compression for sim cache
```

---

## Best Practices

### 1. Resolution Management

```cpp
// Start small, scale up only when needed
if (RequiresHighResolution)
{
    SetRenderTargetSize(1024, 1024);
}
else
{
    SetRenderTargetSize(512, 512);
}
```

### 2. Mipmap Generation

```cpp
// For effects that need LOD sampling
MipMapGeneration = ERenderTargetMipMapGeneration::Enable;
MipMapGenerationType = ERenderTargetMipMapGenerationType::Blur5;  // Higher quality
// or
MipMapGenerationType = ERenderTargetMipMapGenerationType::ComputeShader;  // Faster
```

### 3. Format Selection

| Format | Channels | Precision | Use Case |
|--------|----------|-----------|----------|
| RTF_RGBA8 | 4 | 8-bit | Colors, masks |
| RTF_RGBA16f | 4 | 16-bit float | HDR effects |
| RTF_RGBA32f | 4 | 32-bit float | Precision critical |
| RTF_R32f | 1 | 32-bit float | Single channel data |
| RTF_R32_SINT | 1 | 32-bit int | Counting, IDs |

---

## References

- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceRenderTarget*.cpp`
- Shader Templates: `Engine/Plugins/FX/Niagara/Shaders/Private/NiagaraDataInterfaceRenderTarget*Template.ush`
