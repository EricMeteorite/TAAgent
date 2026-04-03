# Niagara Data Interface Implementation Deep Dive

This document provides an in-depth analysis of key Niagara DataInterface implementations, covering both C++ and GPU shader (HLSL) layers.

---

## Table of Contents

**This Document:**
1. [Core Architecture](#core-architecture)
2. [Texture DataInterface](#texture-datainterface)
3. [NeighborGrid3D DataInterface](#neighborgrid3d-datainterface)
4. [SkeletalMesh DataInterface](#skeletalmesh-datainterface)
5. [CollisionQuery DataInterface](#collisionquery-datainterface)
6. [Grid2DCollection DataInterface](#grid2dcollection-datainterface)
7. [Array DataInterface](#array-datainterface)
8. [Comparison Matrix](#comparison-matrix)
9. [Extension Guidelines](#extension-guidelines)
10. [Audio DataInterface](#audio-datainterface)
11. [Spline DataInterface](#spline-datainterface)
12. [Camera DataInterface](#camera-datainterface)
13. [CurveBase DataInterface](#curvebase-datainterface)
14. [ParticleRead DataInterface](#particleread-datainterface)
15. [VectorField DataInterface](#vectorfield-datainterface)
16. [RigidMeshCollisionQuery DataInterface](#rigidmeshcollisionquery-datainterface)
17. [Landscape DataInterface](#landscape-datainterface)
18. [Export DataInterface](#export-datainterface)

**Related Documents:**
- [Grid DataInterfaces](data-interfaces-grid.md) - Grid3DCollection, RasterizationGrid3D
- [RenderTarget DataInterfaces](data-interfaces-rendertarget.md) - RenderTarget2D, Cube, Volume, IntRenderTarget
- [Noise & Occlusion DataInterfaces](data-interfaces-noise-occlusion.md) - CurlNoise, Occlusion
- [Material & Volume DataInterfaces](data-interfaces-material-volume.md) - MPC, VolumeTexture, SparseVolumeTexture
- [Misc DataInterfaces](data-interfaces-misc.md) - MeshRendererInfo, CubeTexture, 2DArrayTexture

---

## Core Architecture

### CPU/GPU Dual-Path Execution Model

Every DataInterface implements a dual execution model:

```
┌─────────────────────────────────────────────────────────────────┐
│                    UNiagaraDataInterface                        │
├─────────────────────────────────────────────────────────────────┤
│  CPU Path (VectorVM)          │  GPU Path (Compute Shader)      │
├───────────────────────────────┼─────────────────────────────────┤
│  GetVMExternalFunction()      │  GetParameterDefinitionHLSL()   │
│  - Binds VM functions         │  - Generates shader parameters  │
│  - Executes on CPU threads    │  - Generates HLSL functions     │
│                               │                                 │
│  Instance Data:               │  Instance Data:                 │
│  - PerInstanceDataSize()      │  - BuildShaderParameters()      │
│  - InitPerInstanceData()      │  - SetShaderParameters()        │
│  - PerInstanceTick()          │  - Uses RenderGraph (RDG)       │
└───────────────────────────────┴─────────────────────────────────┘
```

### Instance Data Flow

```cpp
// Game Thread Instance Data
struct FNDITextureInstanceData_GameThread
{
    TWeakObjectPtr<UTexture> CurrentTexture;
    FIntPoint CurrentTextureSize;
    int32 CurrentTextureMipLevels;
    FNiagaraParameterDirectBinding<UObject*> UserParamBinding;
};

// Render Thread Instance Data
struct FNDITextureInstanceData_RenderThread
{
    FSamplerStateRHIRef SamplerStateRHI;
    FTextureReferenceRHIRef TextureReferenceRHI;
    FIntPoint TextureSize;
    int32 MipLevels;
    FRDGTextureRef TransientRDGTexture;  // RDG-managed texture
};

// Proxy Pattern for Thread Safety
struct FNiagaraDataInterfaceProxyTexture : public FNiagaraDataInterfaceProxy
{
    TMap<FNiagaraSystemInstanceID, FNDITextureInstanceData_RenderThread> InstanceData_RT;
};
```

---

## Texture DataInterface

### Overview

`UNiagaraDataInterfaceTexture` provides texture sampling capabilities to Niagara particles. It supports 2D textures, pseudo-volume textures, and MIP level access.

### C++ Implementation

**Key Methods:**

```cpp
// Function Registration (Editor Only)
void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
    // LoadTexture2D - Direct texel read without filtering
    Sig.Name = LoadTexture2DName;
    Sig.Inputs: TexelX, TexelY, MipLevel
    Sig.Outputs: float4 Value
    
    // SampleTexture2D - Hardware filtered sampling
    Sig.Name = SampleTexture2DName;
    Sig.Inputs: UV, MipLevel
    Sig.Outputs: float4 Value
    
    // SamplePseudoVolumeTexture - 2D texture as 3D volume
    Sig.Name = SamplePseudoVolumeTextureName;
    Sig.Inputs: UVW, XYNumFrames, TotalNumFrames, MipMode, MipLevel, DDX, DDY
    Sig.Outputs: float4 Value
}
```

**Instance Tick - Texture Update:**

```cpp
bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    // Check for texture changes
    UTexture* CurrentTexture = InstanceData->UserParamBinding.GetValueOrDefault<UTexture>(Texture);
    
    if (InstanceData->CurrentTexture != CurrentTexture || ...)
    {
        // Push update to render thread
        ENQUEUE_RENDER_COMMAND(NDITexture_UpdateInstance)(
            [RT_Proxy, RT_InstanceID, RT_Texture, RT_TextureSize, RT_MipLevels](FRHICommandListImmediate&)
            {
                FNDITextureInstanceData_RenderThread& InstanceData = RT_Proxy->InstanceData_RT.FindOrAdd(RT_InstanceID);
                InstanceData.TextureReferenceRHI = RT_Texture->TextureReference.TextureReferenceRHI;
                InstanceData.SamplerStateRHI = RT_Texture->GetResource()->SamplerStateRHI;
            }
        );
    }
}
```

### GPU Shader Implementation

**Parameter Declaration (Template):**

```hlsl
// NiagaraDataInterfaceTextureTemplate.ush
int2            {ParameterName}_TextureSize;
int             {ParameterName}_MipLevels;
Texture2D       {ParameterName}_Texture;
SamplerState    {ParameterName}_TextureSampler;
```

**Sample Functions:**

```hlsl
// Direct texel load - no filtering
void LoadTexture2D_{ParameterName}(in int TexelX, in int TexelY, in int MipLevel, out float4 OutValue)
{
    OutValue = {ParameterName}_Texture.Load(int3(TexelX, TexelY, MipLevel));
}

// Hardware bilinear sampling
void SampleTexture2D_{ParameterName}(in float2 UV, in float MipLevel, out float4 OutValue)
{
    OutValue = {ParameterName}_Texture.SampleLevel({ParameterName}_TextureSampler, UV, MipLevel);
}

// Pseudo volume texture - treats 2D flipbook as 3D texture
void SamplePseudoVolumeTexture_{ParameterName}(in float3 UVW, in float2 XYNumFrames, ...)
{
    OutValue = PseudoVolumeTexture({ParameterName}_Texture, {ParameterName}_TextureSampler, UVW, XYNumFrames, ...);
}
```

### Use Cases

| Function | Best For | Performance |
|----------|----------|-------------|
| `LoadTexture2D` | Direct data lookup, height maps | Fastest (no filtering) |
| `SampleTexture2D` | Visual effects, gradients | Medium (hardware filtering) |
| `SamplePseudoVolumeTexture` | 3D effects from 2D flipbooks | Slower (frame interpolation) |

---

## NeighborGrid3D DataInterface

### Overview

`UNiagaraDataInterfaceNeighborGrid3D` is the core data structure for SPH fluid simulation and neighbor search algorithms. It provides O(1) spatial hashing for particle neighbor queries.

### Algorithm: Spatial Hashing

```
World Space → Grid Cell Index → Linear Index → Particle List

┌─────────────────────────────────────────────────────────────────┐
│  World Position (x, y, z)                                      │
│         ↓                                                       │
│  Cell Index = floor(Position / CellSize)                        │
│         ↓                                                       │
│  Linear Index = X + Y*NumCellsX + Z*NumCellsX*NumCellsY         │
│         ↓                                                       │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ Cell Buffer: [Count | ParticleID_0 | ParticleID_1 | ...]    ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

### C++ Implementation

**Buffer Structure:**

```cpp
void FNDINeighborGrid3DInstanceData_RT::ResizeBuffers(FRDGBuilder& GraphBuilder)
{
    const uint32 NumTotalCells = NumCells.X * NumCells.Y * NumCells.Z;
    const uint32 NumIntsInGridBuffer = NumTotalCells * MaxNeighborsPerCell;

    // Two buffers:
    // 1. NeighborhoodCountBuffer - stores particle count per cell
    NeighborhoodCountBuffer.Initialize(GraphBuilder, TEXT("NiagaraNeighborGrid3D::NeighborCount"), 
        EPixelFormat::PF_R32_SINT, sizeof(int32), FMath::Max<uint32>(NumTotalCells, 1u), BUF_Static);
    
    // 2. NeighborhoodBuffer - stores particle indices
    NeighborhoodBuffer.Initialize(GraphBuilder, TEXT("NiagaraNeighborGrid3D::NeighborsGrid"), 
        EPixelFormat::PF_R32_SINT, sizeof(int32), FMath::Max(NumIntsInGridBuffer, 1u), BUF_Static);
}
```

**Reset Before Simulation:**

```cpp
void FNiagaraDataInterfaceProxyNeighborGrid3D::ResetData(const FNDIGpuComputeResetContext& Context)
{
    FRDGBuilder& GraphBuilder = Context.GetGraphBuilder();
    AddClearUAVPass(GraphBuilder, ProxyData->NeighborhoodBuffer.GetOrCreateUAV(GraphBuilder), -1);  // Invalid ID
    AddClearUAVPass(GraphBuilder, ProxyData->NeighborhoodCountBuffer.GetOrCreateUAV(GraphBuilder), 0);  // Zero count
}
```

### GPU Shader Implementation

**Buffer Declaration:**

```hlsl
int             {ParameterName}_MaxNeighborsPerCellValue;
Buffer<int>     {ParameterName}_ParticleNeighbors;        // Read-only (previous frame)
Buffer<int>     {ParameterName}_ParticleNeighborCount;
RWBuffer<int>   {ParameterName}_OutputParticleNeighbors;  // Write-only (current frame)
RWBuffer<int>   {ParameterName}_OutputParticleNeighborCount;
```

**Add Particle to Grid:**

```hlsl
void AddParticle_{ParameterName}(int IndexX, int IndexY, int IndexZ, int ParticleIndex, out bool Success)
{
    Success = false;
    
    // Bounds check
    if (IndexX >= 0 && IndexX < {NumCellsName}.x && 
        IndexY >= 0 && IndexY < {NumCellsName}.y && 
        IndexZ >= 0 && IndexZ < {NumCellsName}.z)
    {
        int LinearIndex = IndexX + IndexY * {NumCellsName}.x + IndexZ * {NumCellsName}.x * {NumCellsName}.y;

        // Atomic increment to get slot index
        int PreviousNeighborCount;
        InterlockedAdd({OutputParticleNeighborCount}[LinearIndex], 1, PreviousNeighborCount);

        if (PreviousNeighborCount < {MaxNeighborsPerCellName})
        {
            Success = true;
            
            // Calculate storage index
            int NeighborGridLinear = 
                PreviousNeighborCount + IndexX * {MaxNeighborsPerCellName} + 
                IndexY * {MaxNeighborsPerCellName} * {NumCellsName}.x + 
                IndexZ * {MaxNeighborsPerCellName} * {NumCellsName}.x * {NumCellsName}.y;
            
            {OutputParticleNeighbors}[NeighborGridLinear] = ParticleIndex;
        }
    }
}
```

**Query Neighbors (SPH Density Calculation):**

```hlsl
// Typical SPH neighbor iteration
int CellX = floor(Position.x / CellSize);
int CellY = floor(Position.y / CellSize);
int CellZ = floor(Position.z / CellSize);

float Density = 0;
for (int dz = -1; dz <= 1; dz++) {
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int LinearIdx = NeighborGridIndexToLinear(CellX+dx, CellY+dy, CellZ+dz);
            int Count = GetParticleNeighborCount(LinearIdx);
            
            for (int n = 0; n < Count; n++) {
                int NeighborIdx = GetParticleNeighbor(LinearIdx, n);
                // Compute SPH kernel contribution
            }
        }
    }
}
```

### SPH Fluid Simulation Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│ Frame N                                                          │
├─────────────────────────────────────────────────────────────────┤
│ Stage 1: Fill Grid (Output Stage)                               │
│   - For each particle: AddParticle(Position → Cell)             │
│   - Uses InterlockedAdd for thread-safe insertion               │
│                                                                  │
│ Stage 2: Compute Density (Main Stage)                           │
│   - For each particle: Query 27 neighbor cells                  │
│   - Accumulate density from neighbors                           │
│                                                                  │
│ Stage 3: Compute Pressure & Forces                              │
│   - Pressure = k * (Density - RestDensity)                      │
│   - Pressure gradient force                                     │
│   - Viscosity force                                              │
│                                                                  │
│ Stage 4: Integrate                                               │
│   - Velocity += Acceleration * dt                               │
│   - Position += Velocity * dt                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Console Variables

```ini
fx.MaxNiagaraNeighborGridCells=128*128*128*64  ; Maximum grid cells
```

---

## SkeletalMesh DataInterface

### Overview

`UNiagaraDataInterfaceSkeletalMesh` allows particles to interact with animated skeletal meshes - sampling vertices, bones, and performing GPU skinning.

### C++ Implementation

**Shader Parameters Structure:**

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
    SHADER_PARAMETER_SRV(Buffer<uint>,      MeshIndexBuffer)
    SHADER_PARAMETER_SRV(Buffer<float>,     MeshVertexBuffer)
    SHADER_PARAMETER_SRV(Buffer<uint>,      MeshSkinWeightBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>,    MeshCurrBonesBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>,    MeshPrevBonesBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>,    MeshTangentBuffer)
    SHADER_PARAMETER_SRV(Buffer<float2>,    MeshTexCoordBuffer)
    SHADER_PARAMETER_SRV(Buffer<float4>,    MeshColorBuffer)
    // ... sampling buffers
    SHADER_PARAMETER(uint32,                MeshTriangleCount)
    SHADER_PARAMETER(uint32,                MeshVertexCount)
    SHADER_PARAMETER(int,                   NumBones)
    SHADER_PARAMETER(FMatrix44f,            InstanceTransform)
END_SHADER_PARAMETER_STRUCT()
```

**Skinning Data Management:**

```cpp
class FSkeletalMeshSkinningData
{
    TArray<FMatrix44f> CurrBoneRefToLocals;  // Current frame bone matrices
    TArray<FMatrix44f> PrevBoneRefToLocals;  // Previous frame for motion vectors
    
    TArray<FVector3f> CurrSkinnedPositions[LODIndex];  // CPU-skinned positions
    TArray<FTangentBasis> CurrSkinnedTangentBasis[LODIndex];
    
    void UpdateBoneTransforms()
    {
        // Handle leader pose component
        if (USkinnedMeshComponent* LeaderComponent = SkelComp->LeaderPoseComponent.Get())
        {
            // Use leader's bone transforms
        }
        else
        {
            SkelComp->CacheRefToLocalMatrices(CurrBones);
        }
    }
};
```

### GPU Shader Implementation

**Bone Matrix Retrieval:**

```hlsl
// NiagaraDataInterfaceSkeletalMeshTemplate.ush
float3x4 GetCurrBoneSkinningMatrix_{ParameterName}(uint Bone)
{
    // 3 float4s per bone matrix (3x4 affine transform)
    return float3x4(
        {ParameterName}_MeshCurrBonesBuffer[Bone * 3], 
        {ParameterName}_MeshCurrBonesBuffer[Bone * 3 + 1], 
        {ParameterName}_MeshCurrBonesBuffer[Bone * 3 + 2]
    );
}
```

**GPU Skinning:**

```hlsl
FDISkelMeshSkinnedVertex GetSkinnedVertex_{ParameterName}(uint VertexIndex)
{
    // Read raw position
    float3 CurrPosition = float3(
        {ParameterName}_MeshVertexBuffer[VertexIndex * 3], 
        {ParameterName}_MeshVertexBuffer[VertexIndex * 3 + 1], 
        {ParameterName}_MeshVertexBuffer[VertexIndex * 3 + 2]
    );
    
    // Read skin weights
    uint SectionBoneOffset = {ParameterName}_MeshTriangleMatricesOffsetBuffer[VertexIndex];
    int4 BlendIndices;
    float4 BlendWeights;
    GetIndexWeight_{ParameterName}(BoneIndexOffset, BoneWeightOffset, 0, BlendIndices, BlendWeights);
    
    // Accumulate bone matrices
    float3x4 CurrBoneMatrix = GetCurrSkinningMatrix_{ParameterName}(SectionBoneOffset, BlendIndices, BlendWeights);
    
    // Apply skinning
    SkinnedVertex.Position = mul(CurrBoneMatrix, float4(CurrPosition, 1.0));
    
    return SkinnedVertex;
}
```

**Uniform Triangle Sampling:**

```hlsl
// Area-weighted triangle sampling using alias method
// Pre-computed in C++:
// - Probability buffer (area ratios)
// - Alias buffer (alternative triangles)

float3 DISKelMesh_RandomBarycentricCoord(uint Seed1, uint Seed2, uint Seed3)
{
    float r0 = DISKelMesh_Random(Seed1, Seed2, Seed3);
    float r1 = DISKelMesh_Random(Seed1, Seed2, Seed3);
    float sqrt0 = sqrt(r0);
    return float3(1.0f - sqrt0, sqrt0 * (1.0 - r1), r1 * sqrt0);
}
```

### Supported Operations

| Operation | CPU | GPU | Description |
|-----------|-----|-----|-------------|
| GetBoneTransform | ✓ | ✓ | World-space bone matrix |
| GetVertexData | ✓ | ✓ | Raw vertex position/tangent |
| GetSkinnedVertexData | ✓ | ✓ | GPU-skinned vertex |
| RandomVertexSampling | ✓ | ✓ | Uniform random vertex |
| RandomTriangleSampling | ✓ | ✓ | Area-weighted random triangle |
| UVMapping | ✓ | ✓ | Query triangles by UV |
| Connectivity | ✓ | ✓ | Vertex adjacency for spreading |

### Sampling Region Support

```cpp
// Multiple sampling regions for selective spawning
FSkeletalMeshAreaWeightedSampler::GetWeights(TArray<float>& OutWeights)
{
    // Each region can have different area weights
    // Example: Spawn on "Head" region only, ignoring "Body"
}
```

---

## CollisionQuery DataInterface

### Overview

`UNiagaraDataInterfaceCollisionQuery` provides multiple collision query methods for particle-world interaction.

### Query Types

| Query Type | GPU | CPU | Description |
|------------|-----|-----|-------------|
| SceneDepth | ✓ | ✗ | Sample depth buffer |
| CustomDepth | ✓ | ✗ | Sample custom depth buffer |
| PartialDepth | ✓ | ✗ | Depth excluding current emitter |
| DistanceField | ✓ | ✗ | Global distance field query |
| SyncTrace | ✗ | ✓ | Immediate raycast |
| AsyncTrace | ✗ | ✓ | One-frame latency raycast |

### GPU Shader Implementation

**Scene Depth Query:**

```hlsl
// NiagaraDataInterfaceCollisionQuery.ush
void NDICollisionQuery_QuerySceneDepthGPU(
    in float3 In_SamplePos, 
    in float3 In_LWCTile, 
    out float Out_SceneDepth, 
    out float3 Out_CameraPosWorld, 
    out bool Out_IsInsideView, 
    out float3 Out_WorldPos, 
    out float3 Out_WorldNormal)
{
    // Transform to clip space
    float4 SamplePosition = float4(LWCToFloat(LWCAdd(LwcSamplePos, PrimaryView.TileOffset.PreViewTranslation)), 1);
    float4 ClipPosition = mul(SamplePosition, View.TranslatedWorldToClip);
    float2 ScreenPosition = ClipPosition.xy / ClipPosition.w;
    
    // Check if inside view
    if (all(abs(ScreenPosition.xy) <= float2(1, 1)))
    {
        float2 ScreenUV = ScreenPosition * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;
        float SceneDepth = CalcSceneDepth(ScreenUV);
        
        // Get world normal from GBuffer
        #if NDICOLLISIONQUERY_USE_GBUFFER_NORMAL
            uint2 ScreenPos = uint2(ScreenUV * View.BufferSizeAndInvSize.xy);
            float3 WorldNormal = SubstratePublic_GetWorldNormal(ScreenPos);
        #else
            // Approximate from depth derivatives
            float SceneDepth0 = CalcSceneDepth(ScreenUV + float2(View.BufferSizeAndInvSize.z, 0.0));
            float SceneDepth1 = CalcSceneDepth(ScreenUV + float2(0.0, View.BufferSizeAndInvSize.w));
            // Cross product of depth derivatives gives normal
        #endif
    }
}
```

**Distance Field Query:**

```hlsl
void NDICollisionQuery_QueryMeshDistanceFieldGPU(
    in float3 In_SamplePos, 
    out float Out_DistanceToNearestSurface, 
    out float3 Out_FieldGradient, 
    out bool Out_IsDistanceFieldValid)
{
    #if PLATFORM_SUPPORTS_DISTANCE_FIELDS
        float3 SamplePosition = LWCToFloat(LWCAdd(LwcSamplePos, PrimaryView.TileOffset.PreViewTranslation));
        
        Out_DistanceToNearestSurface = GetDistanceToNearestSurfaceGlobal(SamplePosition);
        Out_FieldGradient = GetDistanceFieldGradientGlobal(SamplePosition);
        Out_IsDistanceFieldValid = (MaxGlobalDFAOConeDistance > 0);
    #endif
}
```

### CPU Async Collision

```cpp
void PerformQueryAsyncCPU(FVectorVMExternalFunctionContext& Context)
{
    // Submit query for next frame
    int QueryID = InstanceData->CollisionBatch.SubmitQuery(Start, End, TraceChannel, TraceComplex);
    OutQueryID.SetAndAdvance(QueryID + 1);
    
    // Retrieve previous frame's result
    FNiagaraDICollsionQueryResult Res;
    if (ID > 0 && InstanceData->CollisionBatch.GetQueryResult(ID - 1, Res))
    {
        OutCollisionPos.SetAndAdvance(Res.CollisionPos);
        OutCollisionNormal.SetAndAdvance(Res.CollisionNormal);
        OutFriction.SetAndAdvance(Res.Friction);
        OutRestitution.SetAndAdvance(Res.Restitution);
    }
}
```

### Use Case Recommendations

| Scenario | Recommended Method | Reason |
|----------|-------------------|--------|
| Particle collision with ground | SceneDepth | Fast, no CPU roundtrip |
| Character interaction | AsyncTrace CPU | Accurate per-polygon collision |
| Large-scale obstacle avoidance | DistanceField | Continuous, no discrete hits |
| Custom collision volumes | CustomDepth | Stencil-based filtering |

---

## Grid2DCollection DataInterface

### Overview

`UNiagaraDataInterfaceGrid2DCollection` provides 2D grid storage for fluid simulation, height fields, and procedural effects. Uses Texture2DArray for GPU efficiency.

### C++ Implementation

**Shader Parameters:**

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FNDIGrid2DShaderParameters, )
    SHADER_PARAMETER(int, NumAttributes)
    SHADER_PARAMETER(FVector2f, UnitToUV)
    SHADER_PARAMETER(FIntPoint, NumCells)
    SHADER_PARAMETER(FVector2f, CellSize)
    SHADER_PARAMETER(FVector2f, WorldBBoxSize)
    
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, Grid)
    SHADER_PARAMETER_SAMPLER(SamplerState, GridSampler)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, OutputGrid)
END_SHADER_PARAMETER_STRUCT()
```

### GPU Shader Implementation

**Cubic Interpolation:**

```hlsl
// NiagaraDataInterfaceGrid2DCollection.ush

// Monotonic cubic interpolation (prevents overshoot)
float Monotonic1DCubic_{ParameterName}(float fm1, float f0, float f1, float f2, float t)
{
    float d0 = .5 * (f1 - fm1);
    float d1 = .5 * (f2 - f0);
    float delta0 = f1 - f0;

    // Check for monotonicity
    if (sign(d0) != sign(d1) || sign(delta0) != sign(d0) || abs(delta0) < 1.1754943508e-38)
    {
        d0 = 0; d1 = 0;  // Linear interpolation at boundaries
    }

    float a0 = f0;
    float a1 = d0;
    float a2 = 3.0f * delta0 - 2.0f * d0 - d1;
    float a3 = d0 + d1 - 2 * delta0;

    return a3 * t*t*t + a2 * t*t + a1 * t + a0;
}

float Basic2DCubic_{ParameterName}(SamplerState Sampler, float3 UVW, int MipLevel)
{
    const float2 GridPos = UVW.xy * {NumCellsName}.xy - .5;
    const int2 GridCell = floor(GridPos);
    
    // 4x4 kernel for bicubic
    float4 InterpInX;
    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 4; ++i)
        {
            GridVals[i] = {GridName}.Load(int4(GridCell.x + i - 1, GridCell.y + j - 1, AttributeIndex, MipLevel));
        }
        InterpInX[j] = Basic1DCubic_{ParameterName}(GridVals[0], GridVals[1], GridVals[2], GridVals[3], t[0]);
    }
    return Basic1DCubic_{ParameterName}(InterpInX[0], InterpInX[1], InterpInX[2], InterpInX[3], t[1]);
}
```

### Fluid Simulation Workflow

```
┌─────────────────────────────────────────────────────────────────┐
│ Grid2D Fluid Simulation Pipeline                                 │
├─────────────────────────────────────────────────────────────────┤
│ Stage 1: Advection (Semi-Lagrangian)                            │
│   - For each cell: Sample velocity at previous position         │
│   - new_value = Sample(previous_position - velocity * dt)       │
│                                                                  │
│ Stage 2: Pressure Solve (Jacobi Iteration)                      │
│   - Divergence = ∇·velocity                                     │
│   - Pressure iterations: 20-80 passes                           │
│   - ∇²p = divergence                                            │
│                                                                  │
│ Stage 3: Projection                                              │
│   - velocity -= ∇pressure                                       │
│   - Makes velocity divergence-free                              │
│                                                                  │
│ Stage 4: Boundary Conditions                                     │
│   - Neumann (no-flux) or Dirichlet (fixed value)                │
│   - Copy to boundary cells                                      │
└─────────────────────────────────────────────────────────────────┘
```

### Console Variables

```ini
fx.Niagara.Grid2D.ResolutionMultiplier=1.0    ; Global resolution scale
fx.Niagara.Grid2D.OverrideFormat=-1           ; Force pixel format
fx.Niagara.Grid2D.CubicInterpMethod=0         ; 0=Bridson, 1=monotonic/Fedkiw
```

---

## Array DataInterface

### Overview

`UNiagaraDataInterfaceArray` provides dynamic arrays for particle-particle communication and data storage.

### GPU Shader Implementation

```hlsl
// NiagaraDataInterfaceArrayRWTemplate.ush
RWBuffer<{RWBufferType}>    {ParameterName}_ArrayRWBuffer;
int2                        {ParameterName}_ArrayBufferParams;  // x=CountOffset, y=Capacity

int GetLength_{ParameterName}() 
{ 
    return RWInstanceCounts[GetCountOffset_{ParameterName}()]; 
}

void Add_{ParameterName}_UEImpureCall(bool bSkip, {VariableType} Value)
{
    if (bSkip) return;
    
    uint Index;
    InterlockedAdd(RWInstanceCounts[GetCountOffset_{ParameterName}()], 1, Index);
    
    if (Index < GetCapacity_{ParameterName}())
    {
        {ParameterName}_ArrayRWBuffer[Index] = Value;
    }
    else
    {
        // Capacity exceeded, rollback count
        InterlockedAdd(RWInstanceCounts[GetCountOffset_{ParameterName}()], uint(-1));
    }
}

// Atomic operations for thread-safe access
void AtomicAdd_{ParameterName}_UEImpureCall(bool bSkip, int Index, int Value, out {VariableType} PrevValue)
{
    if (!bSkip && Index < GetLength_{ParameterName}())
    {
        InterlockedAdd({ParameterName}_ArrayRWBuffer[Index], Value, PrevValue);
    }
}
```

### Array Types

| Type | HLSL Type | Use Case |
|------|-----------|----------|
| Float | `RWBuffer<float>` | Scalar values, IDs |
| Vector2 | `RWBuffer<float2>` | UV coordinates, 2D positions |
| Vector3 | `RWBuffer<float3>` | Positions, velocities |
| Vector4 | `RWBuffer<float4>` | Colors with alpha |
| Matrix | `RWBuffer<float4>` x4 | Transforms |

---

## Comparison Matrix

### GPU vs CPU Support

| DataInterface | GPU Compute | CPU VM | Notes |
|---------------|-------------|--------|-------|
| Texture | ✓ | ✓ (limited) | CPU returns default values |
| NeighborGrid3D | ✓ | ✗ | GPU-only, requires atomics |
| SkeletalMesh | ✓ | ✓ | CPU does full skinning |
| CollisionQuery | Partial | ✓ | Depth/DF on GPU, Trace on CPU |
| Grid2DCollection | ✓ | ✗ | GPU-only for parallel grid ops |
| Array | ✓ | ✓ | Both with atomic support |
| Curve | ✓ | ✓ | Both supported |
| RenderTarget2D | ✓ | ✗ | GPU-only texture ops |

### Memory Layout Comparison

| DataInterface | Storage Type | Access Pattern |
|---------------|--------------|----------------|
| Texture | Texture2D + Sampler | Cache-friendly 2D locality |
| NeighborGrid3D | RWBuffer<int> x2 | Scatter-gather, atomic |
| SkeletalMesh | Multiple SRV Buffers | Structured read |
| Grid2DCollection | Texture2DArray | 2D spatial locality |
| Array | RWBuffer<T> | Linear, random access |

---

## Extension Guidelines

### Creating Custom DataInterface

**1. Header File:**

```cpp
// NiagaraDataInterfaceMyCustom.h
UCLASS()
class NIAGARA_API UNiagaraDataInterfaceMyCustom : public UNiagaraDataInterface
{
    GENERATED_BODY()
    
public:
    // Required overrides
    virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
    virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo&, void*, FVMExternalFunction&) override;
    virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance*) override;
    virtual int32 PerInstanceDataSize() const override;
    
#if WITH_EDITORONLY_DATA
    virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo&, const FNiagaraDataInterfaceGeneratedFunction&, int, FString&) override;
    virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo&, FString&) override;
#endif
    
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder&) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext&) const override;
};
```

**2. Shader Template:**

```hlsl
// NiagaraDataInterfaceMyCustomTemplate.ush

// Parameters
Buffer<float>  {ParameterName}_MyDataBuffer;
int            {ParameterName}_DataCount;

// Functions
void MyFunction_{ParameterName}(int Index, out float Value)
{
    Value = {ParameterName}_MyDataBuffer[Index];
}
```

**3. Key Implementation Points:**

1. **Thread Safety**: Use proxy pattern for render thread data
2. **RDG Integration**: Use `FRDGBuilder` for GPU resources
3. **LWC Support**: Handle large world coordinates properly
4. **Versioning**: Implement function version upgrades for backward compatibility

### Performance Considerations

1. **Minimize CPU-GPU sync**: Use async patterns where possible
2. **Batch operations**: Prefer single large operations over many small ones
3. **Memory coherence**: Design for GPU cache efficiency
4. **Atomic contention**: Limit atomic operations in hot paths

---

## Audio DataInterface

### Overview

`UNiagaraDataInterfaceAudio` provides real-time audio data access for Niagara particles. It uses a Submix Listener pattern to capture audio buffers.

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Audio Submix Pipeline                         │
├─────────────────────────────────────────────────────────────────┤
│  USoundSubmix                                                   │
│       │                                                         │
│       ▼                                                         │
│  FNiagaraSubmixListener (Audio Thread)                          │
│       │  - OnNewSubmixBuffer() callback                         │
│       │  - Pushes to MixerInput ring buffer                     │
│       ▼                                                         │
│  FNiagaraDataInterfaceProxySubmix (Render Thread)               │
│       │  - PopAudio() returns buffer data                       │
│       │  - Thread-safe access via atomic counters               │
│       ▼                                                         │
│  Niagara GPU Compute (Compute Shader)                           │
└─────────────────────────────────────────────────────────────────┘
```

### C++ Implementation

```cpp
// Submix listener registered with audio device
class FNiagaraSubmixListener : public FSharedFromThis<FNiagaraSubmixListener>, public ISubmixBufferListener
{
    // Ring buffer for audio data
    Audio::FPatchMixer MixerInput;
    std::atomic<int32> NumChannelsInSubmix;
    std::atomic<float> SubmixSampleRate;
    
    void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, 
                           int32 NumSamples, int32 NumChannels, const int32 SampleRate, 
                           double AudioClock) override
    {
        NumChannelsInSubmix = NumChannels;
        SubmixSampleRate = SampleRate;
        MixerInput.PushAudio(AudioData, NumSamples);  // Thread-safe push
    }
};

// Proxy manages multiple audio devices (split-screen)
struct FNiagaraDataInterfaceProxySubmix : FNiagaraDataInterfaceProxy
{
    Audio::FPatchMixer PatchMixer;
    TMap<Audio::FDeviceId, TSharedPtr<FNiagaraSubmixListener>> SubmixListeners;
    
    int32 PopAudio(float* OutBuffer, int32 NumSamples, bool bUseLatestAudio)
    {
        return PatchMixer.PopAudio(OutBuffer, NumSamples, bUseLatestAudio);
    }
};
```

### Key Features

1. **Multi-Device Support**: Handles split-screen with multiple audio devices
2. **Thread-Safe**: Uses `std::atomic` for cross-thread communication
3. **Configurable Buffer Size**: Default 16384 samples (~371ms at 44.1kHz)

### Derived Types

| Class | Purpose |
|-------|---------|
| `AudioSubmix` | Raw audio buffer access |
| `AudioSpectrum` | FFT frequency analysis |
| `AudioOscilloscope` | Waveform visualization |
| `AudioPlayer` | Sound asset playback control |

---

## Spline DataInterface

### Overview

`UNiagaraDataInterfaceSpline` provides spline curve sampling with optional LUT (Look-Up Table) acceleration.

### LUT Optimization Strategy

```cpp
// Configurable via cvar
static bool GbNiagaraDISplineDisableLUTs = false;
CONSOLE_VARIABLE("fx.Niagara.NDISpline.GDisableLUTs", GbNiagaraDISplineDisableLUTs, ECVF_Default);

// Template binder selects LUT vs direct computation
template<typename NextBinder>
struct TSplineUseLUTBinder
{
    static void Bind(UNiagaraDataInterface* Interface, ...)
    {
        UNiagaraDataInterfaceSpline* SplineInterface = CastChecked<UNiagaraDataInterfaceSpline>(Interface);
        if (SplineInterface->bUseLUT && !GbNiagaraDISplineDisableLUTs)
        {
            NextBinder::template Bind<..., TIntegralConstant<bool, true>>(...);  // LUT path
        }
        else
        {
            NextBinder::template Bind<..., TIntegralConstant<bool, false>>(...);  // Direct path
        }
    }
};
```

### GPU Shader (HLSL)

```hlsl
// LUT buffers for spline data
Buffer<float4> {ParameterName}_SplinePositionsLUT;
Buffer<float4> {ParameterName}_SplineScalesLUT;
Buffer<float4> {ParameterName}_SplineRotationsLUT;

int     {ParameterName}_MaxIndex;
float   {ParameterName}_SplineDistanceStep;
float   {ParameterName}_InvSplineDistanceStep;

// Key lookup with linear interpolation
void FindNeighborKeys_{ParameterName}(float InDistance, out int PrevKey, out int NextKey, out float Alpha)
{
    const float Key = InDistance * {ParameterName}_InvSplineDistanceStep;
    PrevKey = clamp(int(floor(Key)), 0, {ParameterName}_MaxIndex);
    NextKey = clamp(int(ceil(Key)), 0, {ParameterName}_MaxIndex);
    Alpha = frac(Key);
}

// Position sampling
float3 EvaluatePosition_{ParameterName}(float InDistance)
{
    int PrevKey, NextKey;
    float Alpha;
    FindNeighborKeys_{ParameterName}(InDistance, PrevKey, NextKey, Alpha);
    
    if (NextKey == PrevKey)
        return {ParameterName}_SplinePositionsLUT[PrevKey].xyz;
    
    return lerp({ParameterName}_SplinePositionsLUT[PrevKey].xyz, 
                 {ParameterName}_SplinePositionsLUT[NextKey].xyz, Alpha).xyz;
}

// Rotation uses spherical linear interpolation (SLERP)
float4 EvaluateRotation_{ParameterName}(float InDistance)
{
    // ... key lookup ...
    return NiagaraQuatSLerp({ParameterName}_SplineRotationsLUT[PrevKey], 
                             {ParameterName}_SplineRotationsLUT[NextKey], Alpha);
}

// Find closest point on spline (iterative search)
float EvaluateFindNearestPosition_{ParameterName}(float3 InPosition)
{
    float MinDistance = length2({ParameterName}_SplinePositionsLUT[0].xyz - InPosition);
    float KeyToNearest = 0.0f;
    
    for (int i = 1; i <= {ParameterName}_MaxIndex; i++)
    {
        const float Distance = length2({ParameterName}_SplinePositionsLUT[i].xyz - InPosition);
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            KeyToNearest = i;
        }
    }
    return KeyToNearest > 0 ? float(KeyToNearest) / float({ParameterName}_MaxIndex) : 0.0f;
}
```

### Performance Notes

| Operation | LUT Enabled | LUT Disabled |
|-----------|-------------|--------------|
| Sample Position | O(1) lookup + lerp | O(n) spline eval |
| Sample Rotation | O(1) lookup + SLERP | O(n) spline eval |
| Find Closest | O(n) iterative | O(n²) or worse |

---

## Camera DataInterface

### Overview

`UNiagaraDataInterfaceCamera` provides access to camera/view properties for GPU particles.

### GPU-Only Design

```cpp
// Function signature explicitly marks CPU support as false
Sig.bSupportsCPU = false;
Sig.Name = GetViewPropertiesName;
```

### Shader Parameters

```hlsl
// From View uniform buffer
void GetViewPropertiesGPU_{ParameterName}(
    out float3 Out_ViewPositionWorld,
    out float3 Out_ViewForwardVector,
    out float3 Out_ViewUpVector,
    out float3 Out_ViewRightVector,
    out float4 Out_ViewSizeAndInverseSize,
    out float4 Out_ScreenToViewSpace,
    out float2 Out_Current_TAAJitter,
    out float2 Out_Previous_TAAJitter,
    out float3 Out_PreViewTranslation,
    out float4 Out_BufferSizeAndInverseSize,
    out float2 Out_ViewportOffset,
    out float Out_NearPlane,
    out float2 Out_FOVCurrent,
    out float2 Out_FOVPrevious)
{
    // Access global View uniform buffer
    FLWCVector3 ViewPos = PrimaryView.TileOffset.WorldViewOrigin;
    Out_ViewPositionWorld = LWCToFloat(ViewPos);
    Out_ViewForwardVector = View.ViewForward;
    Out_ViewUpVector = View.ViewUp;
    Out_ViewRightVector = View.ViewRight;
    // ... etc
}

// Transform matrices
void GetClipSpaceTransformsGPU_{ParameterName}(
    out float4x4 Out_WorldToClipTransform,
    out float4x4 Out_TranslatedWorldToClipTransform,
    out float4x4 Out_ClipToWorldTransform,
    out float4x4 Out_ClipToPrevClipTransform,  // For velocity
    ...)
{
    Out_WorldToClipTransform = DFHackToFloat(PrimaryView.WorldToClip);
    Out_ClipToPrevClipTransform = View.ClipToPrevClip;  // Motion vectors
}
```

### LWC (Large World Coordinates) Support

```hlsl
// PreViewTranslation for precision
void ApplyPreViewTranslationToPositionGPU_{ParameterName}(in float3 In_WorldPosition, out float4 Out_TranslatedPosition)
{
    FLWCVector3 LwcPos = MakeLWCVector3(GetEngineOwnerLWCTile(), In_WorldPosition);
    float3 ResolvedLwcPos = LWCToFloat(LWCAdd(LwcPos, PrimaryView.TileOffset.PreViewTranslation));
    Out_TranslatedPosition = float4(ResolvedLwcPos, 1);
}
```

---

## CurveBase DataInterface

### Overview

`UNiagaraDataInterfaceCurveBase` provides curve sampling via LUT for both CPU and GPU.

### LUT Generation & Optimization

```cpp
// Error threshold for LUT optimization
float GNiagaraLUTOptimizeThreshold = UNiagaraDataInterfaceCurveBase::DefaultOptimizeThreshold;
CONSOLE_VARIABLE("fx.Niagara.LUT.OptimizeThreshold", GNiagaraLUTOptimizeThreshold, ECVF_Default);

// Error checking between resampled LUTs
bool PassesErrorThreshold(TConstArrayView<float> ShaderLUT, TConstArrayView<float> ResampledLUT, 
                          int32 NumElements, float ErrorThreshold)
{
    for (int iSample = 0; iSample < CurrNumSamples; ++iSample)
    {
        const float NormalizedSampleTime = float(iSample) / float(CurrNumSamples - 1);
        
        // Interpolate both LUTs at same time
        float LhsValue = FMath::Lerp(ShaderLUT[...], ShaderLUT[...], LhsInterp);
        float RhsValue = FMath::Lerp(ResampledLUT[...], ResampledLUT[...], RhsInterp);
        
        if (FMath::Abs(LhsValue - RhsValue) > ErrorThreshold)
            return false;  // Needs higher resolution LUT
    }
    return true;
}

// Vectorized version for SIMD
template<int32 ElementStride>
bool PassesErrorThresholdVectorized(...)
{
    VectorRegister4Float Threshold = VectorSetFloat1(ErrorThreshold);
    VectorRegister4Float Error = VectorAbs(VectorSubtract(LhsValue, RhsValue));
    return !VectorAnyGreaterThan(Error, Threshold);
}
```

### GPU Shader

```hlsl
Buffer<float> {ParameterName}_CurveLUT;
float  {ParameterName}_MinTime;
float  {ParameterName}_MaxTime;
float  {ParameterName}_InvTimeRange;
uint   {ParameterName}_CurveLUTNumMinusOne;

void GetCurveLUTIndices_{ParameterName}(float Time, out uint IndexA, out uint IndexB, out float Fraction)
{
    float RemappedTime = saturate((Time - {ParameterName}_MinTime) * {ParameterName}_InvTimeRange) 
                         * float({ParameterName}_CurveLUTNumMinusOne);
    IndexA = floor(RemappedTime);
    IndexB = min(IndexA + 1, {ParameterName}_CurveLUTNumMinusOne);
    Fraction = frac(RemappedTime);
}

// Template-generated for different element counts
void SampleCurve_{ParameterName}(float Time, out float Value)  // NumElements=1
{
    uint IndexA, IndexB;
    float Fraction;
    GetCurveLUTIndices_{ParameterName}(Time, IndexA, IndexB, Fraction);
    
    IndexA *= NumElements;
    IndexB *= NumElements;
    
    [unroll]
    for (int i = 0; i < NumElements; ++i)
    {
        Value = lerp({ParameterName}_CurveLUT[IndexA + i], 
                     {ParameterName}_CurveLUT[IndexB + i], Fraction);
    }
}
```

### Derived Types

| Class | Elements | Use Case |
|-------|----------|----------|
| `Curve` | 1 (float) | Single value curves |
| `ColorCurve` | 4 (float4) | Color over time |
| `VectorCurve` | 3 (float3) | Vector over time |
| `Vector2DCurve` | 2 (float2) | 2D curves |
| `Vector4Curve` | 4 (float4) | Quaternions, colors |

---

## ParticleRead DataInterface

### Overview

`UNiagaraDataInterfaceParticleRead` allows reading particle data from another emitter, enabling inter-emitter communication.

### Data Flow Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│              Particle Read Data Flow                             │
├─────────────────────────────────────────────────────────────────┤
│  Source Emitter                                                  │
│  └─ FNiagaraDataSet                                             │
│      └─ FNiagaraDataBuffer                                      │
│          ├─ FloatComponentBuffer (positions, velocities...)     │
│          ├─ IntComponentBuffer (IDs, flags)                     │
│          └─ HalfComponentBuffer (compressed data)               │
│                                                                  │
│  ParticleRead DI                                                 │
│  └─ AttributeIndices[] - maps attribute names to buffer offsets │
│  └─ IDToIndexTable - maps particle ID to buffer index           │
│                                                                  │
│  Target Emitter                                                  │
│  └─ GetByID / GetByIndex functions                              │
└─────────────────────────────────────────────────────────────────┘
```

### Implementation Details

```cpp
// Find attribute indices at runtime
void NDIParticleRead_FindAttributeIndices(FNDIParticleRead_RenderInstanceData* InstanceData, 
                                           const FNiagaraDataSet* SourceDataSet,
                                           const FNiagaraDataInterfaceParametersCS_ParticleRead& ShaderStorage)
{
    for (int AttrNameIdx = 0; AttrNameIdx < ShaderStorage.AttributeNames.Num(); ++AttrNameIdx)
    {
        const FMemoryImageName& AttrName = ShaderStorage.AttributeNames[AttrNameIdx];
        
        for (int VarIdx = 0; VarIdx < SourceEmitterVariables.Num(); ++VarIdx)
        {
            if (Var.GetName() == AttrName)
            {
                // Check type compatibility
                if (CheckVariableType(Var.GetType(), AttributeType))
                {
                    InstanceData->AttributeIndices[AttrNameIdx] = Layout.GetFloatComponentStart();
                    InstanceData->AttributeCompressed[AttrNameIdx] = 0;  // Not half
                }
                else if (CheckHalfVariableType(Var.GetType(), AttributeType))
                {
                    InstanceData->AttributeIndices[AttrNameIdx] = Layout.GetHalfComponentStart();
                    InstanceData->AttributeCompressed[AttrNameIdx] = 1;  // Half precision
                }
            }
        }
    }
}
```

### Access Patterns

| Function | Lookup Method | Use Case |
|----------|---------------|----------|
| `GetByID` | ID → Index Table → Buffer | Stable reference across frames |
| `GetByIndex` | Direct Buffer Access | Fastest, but index can change |
| `GetNumParticles` | Buffer Count | Loop iteration bounds |

---

## VectorField DataInterface

### Overview

`UNiagaraDataInterfaceVectorField` provides 3D vector field sampling for particle forces.

### ISPC Optimization

```cpp
#if INTEL_ISPC
static_assert(sizeof(ispc::FVector) == sizeof(FVector), "Size mismatch");

bool GNiagaraVectorFieldUseIspc = NIAGARA_VECTOR_FIELD_ISPC_ENABLED_DEFAULT;
CONSOLE_VARIABLE("fx.Niagara.VectorFieldUseIspc", GNiagaraVectorFieldUseIspc, ECVF_Default);
#endif
```

### Functions

```cpp
static const FName SampleVectorFieldName("SampleField");
static const FName LoadVectorFieldName("LoadField");
static const FName GetVectorFieldTilingAxesName("FieldTilingAxes");
static const FName GetVectorFieldDimensionsName("FieldDimensions");
static const FName GetVectorFieldBoundsName("FieldBounds");
```

### Supported Assets

| Asset Type | Features |
|------------|----------|
| `UVectorFieldStatic` | Static baked field |
| `UVectorFieldAnimated` | Animated sequence |

---

## RigidMeshCollisionQuery DataInterface

### Overview

`UNiagaraDataInterfaceRigidMeshCollisionQuery` provides collision detection against physics-enabled meshes (StaticMesh + SkeletalMesh).

### Collision Primitive Types

```hlsl
#define BOX_INDEX 0
#define SPHERE_INDEX 1
#define CAPSULE_INDEX 2
#define NUM_ELEMENTS_INDEX 3

uint4 {ParameterName}_ElementOffsets;  // Start indices for each type
```

### GPU Data Layout

```hlsl
// Transform buffers (3x4 matrix = 3 float4s)
Buffer<float4> {ParameterName}_WorldTransformBuffer;
Buffer<float4> {ParameterName}_InverseTransformBuffer;

// Per-element data
Buffer<float4> {ParameterName}_ElementExtentBuffer;  // Size/radius
Buffer<float4> {ParameterName}_MeshScaleBuffer;
Buffer<uint>   {ParameterName}_PhysicsTypeBuffer;
Buffer<uint>   {ParameterName}_DFIndexBuffer;        // Distance Field index
```

### Collision Detection

```hlsl
// Sphere projection
float GetSphereProjection_{ParameterName}(in float3 LocalPosition, in float3 SphereCenter, 
    in float SphereRadius, in int SphereIndex,
    inout float3 OutClosestPosition, inout float3 OutClosestNormal, 
    inout int OutElementIndex, inout float OutMinDistance)
{
    const float3 DeltaPosition = LocalPosition - SphereCenter;
    const float DeltaLength = length(DeltaPosition);
    const float SphereDistance = DeltaLength - SphereRadius;

    if (SphereDistance < OutMinDistance)
    {
        OutMinDistance = SphereDistance;
        OutElementIndex = SphereIndex;
        OutClosestNormal = (DeltaLength > SMALL_NUMBER) ? DeltaPosition / DeltaLength : float3(0,0,0);
        OutClosestPosition = LocalPosition - OutClosestNormal * SphereDistance;
    }
    return SphereDistance;
}

// Box projection with axis detection
float GetBoxProjection_{ParameterName}(in float3 LocalPosition, in float3 BoxExtent, 
    in int BoxIndex, ...)
{
    const float3 HalfExtent = 0.5 * BoxExtent;
    const float3 DeltaPosition = abs(LocalPosition) - HalfExtent;
    
    // Find dominant axis for normal
    const int ClosestAxis = ((DeltaPosition.x > DeltaPosition.y) && (DeltaPosition.x > DeltaPosition.z)) 
                            ? 0 : (DeltaPosition.y > DeltaPosition.z) ? 1 : 2;
    
    const float OutsideDistance = length(max(DeltaPosition, 0.0));
    const float BoxDistance = OutsideDistance + min(DeltaPosition[ClosestAxis], 0.0);
    // ...
}
```

### Distance Field Integration

```hlsl
void GetClosestPointMeshDistanceField_{ParameterName}(...)
{
    uint DFIndex = {ParameterName}_DFIndexBuffer[ElementIndex];
    
    if (DFIndex >= 0 && DFIndex < NumSceneObjects)
    {
        // Query global Distance Field system
        OutClosestDistance = GetDistanceToMeshDistanceField(DFIndex, LWCWorldPosition, MaxDistance);
        ComputeClosestPointMeshDistanceField(DFIndex, ..., OutClosestNormal, MaxEncodedDistance, NormalIsValid);
    }
}
```

### Velocity Calculation

```hlsl
// Transform velocity from previous frame
void GetElementPoint_{ParameterName}(...)
{
    const float3 PreviousPosition = mul(GetPreviousTransform_{ParameterName}(ElementIndex), 
                                        float4(CollisionPosition, 1.0)).xyz;
    const float3 CurrentPosition = mul(GetCurrentTransform_{ParameterName}(ElementIndex), 
                                       float4(CollisionPosition, 1.0)).xyz;
    
    OutClosestVelocity = (CurrentPosition - PreviousPosition) / DeltaTime;
}
```

---

## Landscape DataInterface

### Overview

`UNiagaraDataInterfaceLandscape` provides terrain height, normal, and material access via Virtual Textures or cached textures.

### Virtual Texture Integration

```hlsl
// Base Color VT
Texture2D {ParameterName}_BaseColorVirtualTexture;
Texture2D<uint4> {ParameterName}_BaseColorVirtualTexturePageTable;
uint {ParameterName}_BaseColorVirtualTextureEnabled;

// Height VT
Texture2D {ParameterName}_HeightVirtualTexture;
uint {ParameterName}_HeightVirtualTextureEnabled;

// Normal VT (may use 2 textures for BC3BC3/BC5BC1 encoding)
Texture2D {ParameterName}_NormalVirtualTexture0;
Texture2D {ParameterName}_NormalVirtualTexture1;
int {ParameterName}_NormalVirtualTextureUnpackMode;  // BC3BC3, BC5BC1, etc.
```

### Height Sampling

```hlsl
void GetHeight_{ParameterName}(float3 InWorldPos, out float OutHeight, out bool OutIsValid)
{
    if ({ParameterName}_HeightVirtualTextureEnabled != 0)
    {
        // Virtual Texture sampling
        FDFVector3 LwcWorldPos = DFFromTileOffset_Hack(MakeLWCVector3(GetEngineOwnerLWCTile(), InWorldPos));
        float2 SampleUv = VirtualTextureWorldToUV(LwcWorldPos, LwcHeightOrigin, UnpackHeightU, UnpackHeightV);
        
        VTPageTableResult PageTable = TextureLoadVirtualPageTableLevel(...);
        float4 PackedValue = TextureVirtualSample(...);
        
        OutHeight = VirtualTextureUnpackHeight(PackedValue, UnpackHeightScaleBias);
        OutIsValid = true;
    }
    else if ({ParameterName}_CachedHeightTextureEnabled != 0)
    {
        // Cached texture fallback
        float2 Uv = GetCachedHeightTextureUv_{ParameterName}(InWorldPos);
        OutHeight = {ParameterName}_CachedHeightTexture.SampleLevel(...).x;
        OutIsValid = true;
    }
}
```

### Normal from Height

```hlsl
// Compute normal from height samples using GatherRed
void GetWorldNormal_{ParameterName}(float3 InWorldPos, out float3 OutNormal, out bool OutIsValid)
{
    // Use Gather to get 4 samples at once
    float4 Red0 = {ParameterName}_CachedHeightTexture.GatherRed(Sampler, GatherLocation - TexelSize);
    float4 Red1 = {ParameterName}_CachedHeightTexture.GatherRed(Sampler, GatherLocation);
    
    // 7 height samples for triangle normal averaging
    float4 TL = float4(float2(-1, -1) * WorldGridSize, Red0.w, 1.0f);
    float4 TT = float4(float2(+0, -1) * WorldGridSize, Red0.z, 1.0f);
    float4 CC = float4(float2(+0, +0) * WorldGridSize, Red0.y, 1.0f);
    // ... etc
    
    // Compute 6 triangle normals and average
    float3 N0 = ComputeNullableTriangleNormal(CC, LL, TL);
    float3 N1 = ComputeNullableTriangleNormal(TL, TT, CC);
    // ...
    
    OutNormal = normalize(N0 + N1 + N2 + N3 + N4 + N5);
}
```

### Physical Material Query

```hlsl
Texture2D<uint> {ParameterName}_CachedPhysMatTexture;

void GetPhysicalMaterialIndex_{ParameterName}(float3 InWorldPos, out int OutIndex, out bool OutIsValid)
{
    float2 Uv = GetCachedHeightTextureUv_{ParameterName}(InWorldPos);
    int3 SampleIndex = int3(Uv * Dimension, 0);
    OutIndex = (int){ParameterName}_CachedPhysMatTexture.Load(SampleIndex).x;
    
    // 255 = invalid/no material
    OutIsValid = (OutIndex < 255);
}
```

---

## Export DataInterface

### Overview

`UNiagaraDataInterfaceExport` enables particle data export from GPU to CPU via callback handlers.

### GPU to CPU Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                    Export Pipeline                               │
├─────────────────────────────────────────────────────────────────┤
│  GPU Compute Shader                                              │
│  └─ ExportParticleData() writes to RWBuffer                      │
│      └─ Position (float3), Size (float), Velocity (float3)       │
│                                                                  │
│  PostStage Callback (Render Thread)                              │
│  └─ FNiagaraGpuReadbackManager::EnqueueReadback()                │
│      └─ Staging buffer for GPU→CPU transfer                      │
│                                                                  │
│  Readback Complete (Any Thread)                                  │
│  └─ EnqueueGameThreadCallback()                                  │
│      └─ INiagaraParticleCallbackHandler::ReceiveParticleData()   │
└─────────────────────────────────────────────────────────────────┘
```

### Shader Implementation

```hlsl
RWBuffer<uint> RWWriteBuffer;
uint WriteBufferSize;

void ExportParticleData_{ParameterName}(...)
{
    // First uint stores count
    uint Index;
    InterlockedAdd(RWWriteBuffer[0], 1, Index);
    
    if (Index < WriteBufferSize)
    {
        // Write 7 floats per particle: Position.xyz, Size, Velocity.xyz
        uint WriteOffset = 1 + Index * 7;  // +1 for count
        
        RWWriteBuffer[WriteOffset + 0] = asuint(Position.x);
        RWWriteBuffer[WriteOffset + 1] = asuint(Position.y);
        RWWriteBuffer[WriteOffset + 2] = asuint(Position.z);
        RWWriteBuffer[WriteOffset + 3] = asuint(Size);
        RWWriteBuffer[WriteOffset + 4] = asuint(Velocity.x);
        RWWriteBuffer[WriteOffset + 5] = asuint(Velocity.y);
        RWWriteBuffer[WriteOffset + 6] = asuint(Velocity.z);
    }
}
```

### CPU Callback

```cpp
void EnqueueGameThreadCallback(TWeakObjectPtr<UObject> WeakCallbackHandler, 
                                TArray<FBasicParticleData> ParticleData, 
                                TWeakObjectPtr<UNiagaraSystem> WeakSystem, 
                                FVector3f SystemTileOffset)
{
    FNiagaraWorldManager::EnqueueGlobalDeferredCallback(
        [=]()
        {
            UObject* CallbackHandler = WeakCallbackHandler.Get();
            UNiagaraSystem* System = WeakSystem.Get();
            
            // Interface call
            INiagaraParticleCallbackHandler::Execute_ReceiveParticleData(
                CallbackHandler, ParticleData, System, 
                FVector(SystemTileOffset) * FLargeWorldRenderScalar::GetTileSize());
        }
    );
}
```

### Allocation Modes

| Mode | Description |
|------|-------------|
| `FixedSize` | Pre-allocated buffer size |
| `PerParticleSize` | Scale with particle count |

---

## Comparison Matrix (Extended)

### GPU vs CPU Support (Extended)

| DataInterface | GPU Compute | CPU VM | Special Considerations |
|---------------|-------------|--------|------------------------|
| Audio | ✗ | ✓ | CPU-only, audio thread |
| Spline | ✓ | ✓ | LUT optional on both |
| Camera | ✓ | ✓ (partial) | View transforms GPU-only |
| Curve | ✓ | ✓ | LUT on both |
| ParticleRead | ✓ | ✓ | GPU needs unsafe-read flag |
| VectorField | ✓ | ✓ | ISPC on CPU |
| RigidMeshCollision | ✓ | ✓ | DF GPU-only |
| Landscape | ✓ | ✗ | VT GPU-only |
| Export | ✓ | ✓ | GPU→CPU readback |

### Memory Patterns (Extended)

| DataInterface | Primary Storage | Update Frequency |
|---------------|-----------------|------------------|
| Audio | Ring Buffer | Every audio frame |
| Spline | LUT Buffers | On spline change |
| Camera | View Uniform | Every frame |
| Curve | LUT Buffer | On curve edit |
| ParticleRead | Source DataSet | Every frame |
| RigidMeshCollision | Transform Buffers | Every frame (double-buffered) |
| Landscape | VT / Cached Texture | On region change |
| Export | RWBuffer | Per-stage |

---

## References

- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/`
- Shader Files: `Engine/Plugins/FX/Niagara/Shaders/Private/`
- Example DI: `Engine/Plugins/FX/ExampleCustomDataInterface/`
- Audio: `NiagaraDataInterfaceAudio*.cpp`
- Virtual Texture: `/Engine/Private/VirtualTextureCommon.ush`
