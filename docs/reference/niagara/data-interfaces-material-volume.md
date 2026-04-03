# Niagara Material & Volume DataInterfaces

Material parameter control and volumetric texture access.

---

## MaterialParameterCollection DataInterface

### Overview

`UNiagaraDataInterfaceMaterialParameterCollection` allows Niagara to write to Material Parameter Collections (MPC), enabling particles to affect materials globally.

### CPU-Only Design

```cpp
// MPC operations require game thread access
Sig.bSupportsCPU = true;
Sig.bSupportsGPU = false;
```

### C++ Implementation

**Instance Data:**

```cpp
struct FInstanceData_GT
{
    FNiagaraParameterDirectBinding<UObject*> ObjectBinding;
    TWeakObjectPtr<UMaterialParameterCollectionInstance> MPC;
    
    void ResolveParameterCollection(FNiagaraSystemInstance* SystemInstance, UMaterialParameterCollection* DefaultCollection)
    {
        UMaterialParameterCollection* BoundCollection = DefaultCollection;
        
        // Support both collection and instance binding
        if (UObject* BoundObject = ObjectBinding.GetValue())
        {
            if (UMaterialParameterCollectionInstance* AsInstance = Cast<UMaterialParameterCollectionInstance>(BoundObject))
            {
                MPC = AsInstance;
                return;
            }
            if (UMaterialParameterCollection* AsCollection = Cast<UMaterialParameterCollection>(BoundObject))
            {
                BoundCollection = AsCollection;
            }
        }
        
        // Get world instance
        if (BoundCollection)
        {
            UWorld* World = SystemInstance->GetWorld();
            MPC = World->GetParameterCollectionInstance(BoundCollection);
        }
    }
};
```

**Async Parameter Setting:**

```cpp
static void VMSetScalarParameter(FVectorVMExternalFunctionContext& Context, FName ParameterName)
{
    VectorVM::FUserPtrHandler<FInstanceData_GT> InstanceData(Context);
    FNDIInputParam<float> InValue(Context);
    
    TWeakObjectPtr<UMaterialParameterCollectionInstance> MPC = InstanceData->MPC;
    if (MPC.IsValid())
    {
        // Must use async task for game thread access
        AsyncTask(
            ENamedThreads::GameThread,
            [WeakMPC=MPC, ParameterName, Value=InValue.Get()]()
            {
                if (UMaterialParameterCollectionInstance* MPC = WeakMPC.Get())
                {
                    MPC->SetScalarParameterValue(ParameterName, Value);
                }
            }
        );
    }
}
```

### Function Signature

| Function | Inputs | Description |
|----------|--------|-------------|
| `SetScalarParameter` | Value (float) | Set scalar MPC parameter |
| `SetVector4Parameter` | Value (float4) | Set vector MPC parameter |

**Parameter Name:** Specified via `FunctionSpecifiers[0]` in the function binding.

### Use Cases

```
┌─────────────────────────────────────────────────────────────────┐
│              MPC Integration Patterns                            │
├─────────────────────────────────────────────────────────────────┤
│  1. Global Effect Control                                        │
│     - Particle count → material intensity                        │
│     - Average position → material offset                         │
│                                                                  │
│  2. Player Interaction                                           │
│     - Player footstep → ripple origin                            │
│     - Damage position → impact effect                            │
│                                                                  │
│  3. Environmental Effects                                        │
│     - Rain intensity → wetness mask                              │
│     - Wind direction → foliage bend                              │
│                                                                  │
│  4. Performance Optimization                                     │
│     - Distance from camera → LOD blend                           │
│     - Particle bounds → culling parameters                       │
└─────────────────────────────────────────────────────────────────┘
```

### Example Workflow

```cpp
// In Niagara System
// 1. Count active particles
float ActiveParticleCount = GetNumParticles();

// 2. Set MPC parameter
MPC_DI.SetScalarParameter("ParticleIntensity", ActiveParticleCount / MaxParticles);

// In Material
// 3. Use MPC parameter
// MaterialParameterCollection > ParticleIntensity
// → Blend between high/low quality effects
```

---

## MaterialInstanceDynamic DataInterface

### Overview

`UNiagaraDataInterfaceMaterialInstanceDynamic` provides direct access to Dynamic Material Instances for per-particle material control.

### Key Functions

| Function | Description |
|----------|-------------|
| `SetScalarParameterValue` | Set scalar parameter on MID |
| `SetVectorParameterValue` | Set vector parameter on MID |
| `SetTextureParameterValue` | Set texture parameter on MID |

---

## VolumeTexture DataInterface

### Overview

`UNiagaraDataInterfaceVolumeTexture` provides sampling of 3D volume textures (UVolumeTexture, UTextureRenderTargetVolume).

### GPU-Only Operations

```cpp
// Texture sampling is GPU-only
Sig.bSupportsCPU = false;
Sig.bSupportsGPU = true;
```

### Instance Data

```cpp
struct FNDIVolumeTextureInstanceData_GameThread
{
    TWeakObjectPtr<UTexture> CurrentTexture;
    FIntVector CurrentTextureSize;
    FNiagaraParameterDirectBinding<UObject*> UserParamBinding;
};

struct FNDIVolumeTextureInstanceData_RenderThread
{
    FSamplerStateRHIRef SamplerStateRHI;
    FTextureReferenceRHIRef TextureReferenceRHI;
    FVector3f TextureSize;
    FRDGTextureRef TransientRDGTexture;
};
```

### GPU Shader

```hlsl
// NiagaraDataInterfaceVolumeTextureTemplate.ush
Texture3D       {ParameterName}_VolumeTexture;
SamplerState    {ParameterName}_VolumeTextureSampler;
float3          {ParameterName}_TextureSize;

// Direct voxel load
void LoadVolumeTexture_{ParameterName}(
    int TexelX, int TexelY, int TexelZ, int MipLevel, 
    out float4 Value)
{
    Value = {ParameterName}_VolumeTexture.Load(int4(TexelX, TexelY, TexelZ, MipLevel));
}

// Trilinear sampling
void SampleVolumeTexture_{ParameterName}(
    float3 UVW, float MipLevel, 
    out float4 Value)
{
    Value = {ParameterName}_VolumeTexture.SampleLevel(
        {ParameterName}_VolumeTextureSampler, UVW, MipLevel);
}

// Get dimensions
void TextureDimensions3D_{ParameterName}(out float3 Dimensions)
{
    Dimensions = {ParameterName}_TextureSize;
}
```

### Supported Texture Types

| Type | Read | Write | Streaming |
|------|------|-------|-----------|
| UVolumeTexture | ✓ | ✗ | ✓ |
| UTextureRenderTargetVolume | ✓ | ✓ | ✗ |

### Use Cases

| Use Case | Resolution | Format |
|----------|------------|--------|
| 3D noise | 64³-256³ | R8/R16f |
| Density field | 128³ | R16f |
| Color LUT | 32³-64³ | RGBA8 |
| Velocity field | 64³-128³ | RGBA16f |

---

## SparseVolumeTexture DataInterface

### Overview

`UNiagaraDataInterfaceSparseVolumeTexture` provides access to sparse (tiled) volume textures with streaming support and animation playback.

### Sparse Volume Texture Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│              Sparse Volume Texture Structure                     │
├─────────────────────────────────────────────────────────────────┤
│  Page Table (Texture3D)                                         │
│  └─ Maps virtual coords to physical tile location               │
│                                                                  │
│  Physical Texture Pool                                          │
│  └─ Array of 64³ tiles with only resident pages                 │
│                                                                  │
│  Attribute Textures                                             │
│  ├─ AttributeA (Density, Albedo...)                             │
│  └─ AttributeB (Normal, Emission...)                            │
│                                                                  │
│  Benefits:                                                       │
│  - Only loaded tiles consume memory                             │
│  - Supports huge volumes (e.g., 4096³)                          │
│  - Mip-mapped for LOD                                           │
└─────────────────────────────────────────────────────────────────┘
```

### Instance Data

```cpp
struct FNDISparseVolumeTextureInstanceData_GameThread
{
    TWeakObjectPtr<USparseVolumeTexture> CurrentTexture;
    const UE::SVT::FTextureRenderResources* CurrentRenderResources;
    FIntVector3 CurrentTextureSize;
    int32 CurrentTextureMipLevels;
    FNiagaraParameterDirectBinding<UObject*> UserParamBinding;
    float CurrentFrame;
    float FrameRate;
    int32 NumFrames;
};
```

### GPU Shader

```hlsl
// Two attribute outputs for density + color/normal
void LoadSparseVolumeTexture_{ParameterName}(
    int TexelX, int TexelY, int TexelZ, int MipLevel,
    out float4 AttributesA,
    out float4 AttributesB)
{
    // Uses sparse volume texture sampling
    // Hardware handles page table lookup
    AttributesA = SparseVolumeTextureAttributesA.Load(int4(TexelX, TexelY, TexelZ, MipLevel));
    AttributesB = SparseVolumeTextureAttributesB.Load(int4(TexelX, TexelY, TexelZ, MipLevel));
}

void SampleSparseVolumeTexture_{ParameterName}(
    float3 UVW, int MipLevel,
    out float4 AttributesA,
    out float4 AttributesB)
{
    AttributesA = SparseVolumeTextureAttributesA.SampleLevel(Sampler, UVW, MipLevel);
    AttributesB = SparseVolumeTextureAttributesB.SampleLevel(Sampler, UVW, MipLevel);
}
```

### CPU Functions

| Function | Purpose |
|----------|---------|
| `RequestSparseVolumeTextureFrame` | Queue frame for streaming |
| `GetNumFrames` | Get animation frame count |
| `GetTextureDimensions` | Get resolution at mip level |
| `GetNumMipLevels` | Get mip chain length |

### Animation Support

```cpp
// Request frame loading
bool VMRequestSparseVolumeTextureFrame(FVectorVMExternalFunctionContext& Context)
{
    float FrameRate = InFrameRate.Get();
    float Frame = InFrame.Get();
    
    // Queue streaming request
    InstanceData->CurrentFrame = Frame;
    InstanceData->FrameRate = FrameRate;
    
    // Returns success if frame is available
    return true;
}
```

### Properties

| Property | Description |
|----------|-------------|
| `SparseVolumeTexture` | Asset reference |
| `BlockingStreamingRequests` | Block on first frame load |
| `SparseVolumeTextureUserParameter` | External binding |

---

## VirtualTextureSample DataInterface

### Overview

`UNiagaraDataInterfaceVirtualTextureSample` provides access to Virtual Textures, allowing sampling of extremely high-resolution textures.

### GPU Shader

```hlsl
// Virtual texture sampling with feedback
void SampleVirtualTexture_{ParameterName}(
    float2 UV,
    float MipLevel,
    out float4 Value)
{
    // VT page table lookup
    // Automatic tile streaming if needed
    Value = VirtualTextureSample(VTPageTable, VTPhysicalTexture, UV, MipLevel);
}
```

### Use Cases

- Terrain detail maps
- Ultra-high resolution decals
- Large world texturing

---

## Comparison Matrix

### Volume Texture Types

| Type | Max Resolution | Memory | Streaming | Animation |
|------|----------------|--------|-----------|-----------|
| VolumeTexture | 256³ typical | Full | No | No |
| SparseVolumeTexture | 4096³+ | Sparse | Yes | Yes |
| RenderTargetVolume | 256³ | Full | No | No |

### Material Integration

| Interface | CPU | GPU | Latency |
|-----------|-----|-----|---------|
| MaterialParameterCollection | ✓ | ✗ | 1 frame (async) |
| MaterialInstanceDynamic | ✓ | ✗ | 1 frame (async) |

---

## Best Practices

### MPC Usage

```cpp
// Good: Set once per frame, use for global values
MPC_DI.SetScalarParameter("GlobalIntensity", AverageValue);

// Avoid: Setting per-particle (performance)
for (each particle)
    MPC_DI.SetScalarParameter("PerParticle", value);  // Too many async tasks!
```

### Volume Texture Memory

| Resolution | Float4 Memory | R8 Memory |
|------------|---------------|-----------|
| 64³ | 1 MB | 256 KB |
| 128³ | 8 MB | 2 MB |
| 256³ | 64 MB | 16 MB |
| 512³ | 512 MB | 128 MB |

---

## References

- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceMaterialParameterCollection.cpp`
- Volume Texture: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceVolumeTexture.cpp`
- Sparse Volume Texture: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceSparseVolumeTexture.cpp`
