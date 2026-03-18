# Niagara Noise & Occlusion DataInterfaces

Procedural noise generation and visibility queries for particles.

---

## CurlNoise DataInterface

### Overview

`UNiagaraDataInterfaceCurlNoise` generates divergence-free vector fields using curl noise. Ideal for swirling, turbulent motion without particle clustering.

### Mathematical Foundation

Curl noise computes the curl (rotational) of a gradient noise field:

```
∇ × (∇φ) = 0  (curl of gradient is always zero)

CurlNoise(P) = ∇ × SimplexNoise(P)
             = (∂Nz/∂y - ∂Ny/∂z, ∂Nx/∂z - ∂Nz/∂x, ∂Ny/∂x - ∂Nx/∂y)
```

This guarantees a **divergence-free** velocity field, preventing particles from accumulating or forming voids.

### C++ Implementation

```cpp
void UNiagaraDataInterfaceCurlNoise::SampleNoiseField(FVectorVMExternalFunctionContext& Context)
{
    for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
    {
        const FVector3f InCoords = FVector3f(XParam.GetAndAdvance(), YParam.GetAndAdvance(), ZParam.GetAndAdvance());
        
        // Compute Jacobian of simplex noise
        FNiagaraMatrix3x4 J = JacobianSimplex_ALU(InCoords + OffsetFromSeed);
        
        // Curl = cross product of gradient components
        *OutSampleX.GetDestAndAdvance() = J[1][2] - J[2][1];  // ∂Nz/∂y - ∂Ny/∂z
        *OutSampleY.GetDestAndAdvance() = J[2][0] - J[0][2];  // ∂Nx/∂z - ∂Nz/∂x
        *OutSampleZ.GetDestAndAdvance() = J[0][1] - J[1][0];  // ∂Ny/∂x - ∂Nx/∂y
    }
}
```

### GPU Shader

```hlsl
float3 {ParameterName}_OffsetFromSeed;

void SampleNoiseField_{ParameterName}(float3 In_XYZ, out float3 Out_Value)
{
    // JacobianSimplex_ALU returns 3x4 matrix
    // J[row][col] where col 0-2 are gradient components
    float3x4 J = JacobianSimplex_ALU(In_XYZ + {ParameterName}_OffsetFromSeed, false, 1.0);
    
    // Compute curl from Jacobian
    Out_Value = float3(
        J[1][2] - J[2][1],
        J[2][0] - J[0][2],
        J[0][1] - J[1][0]
    );
}
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `Seed` | int | Random seed for variation |

### Usage Example

```hlsl
// Per-frame update
float3 NoiseVelocity;
SampleNoiseField_CurlNoiseDI(Position, NoiseVelocity);

// Scale by time for animation
float3 AnimatedPos = Position + float3(Time * 0.1, Time * 0.15, Time * 0.12);
SampleNoiseField_CurlNoiseDI(AnimatedPos, NoiseVelocity);

// Apply as force
Velocity += NoiseVelocity * NoiseStrength * DeltaTime;
```

### Advantages Over Simple Noise

| Feature | Curl Noise | Simple Noise |
|---------|------------|--------------|
| Divergence | Zero (incompressible) | Non-zero (causes clustering) |
| Particle Distribution | Uniform | Can accumulate |
| Visual Quality | Smooth swirls | Chaotic |
| Performance | Slightly slower | Faster |

---

## Occlusion DataInterface

### Overview

`UNiagaraDataInterfaceOcclusion` queries depth buffers and cloud occlusion to determine particle visibility. GPU-only interface for performance-critical visibility tests.

### Query Methods

#### 1. Rectangle Occlusion Query

Samples depth buffer in a grid pattern:

```hlsl
void QueryOcclusionFactorWithRectangle(
    float3 SampleCenterWorldPos,
    float SampleWindowWidthWorld,
    float SampleWindowHeightWorld,
    float SampleStepsPerLine,
    out float VisibilityFraction,
    out float SampleFraction)
{
    // Transform to screen space
    float4 ClipPos = mul(float4(SampleCenterWorldPos, 1), View.WorldToClip);
    float2 ScreenPos = ClipPos.xy / ClipPos.w;
    
    // Sample grid
    int VisibleSamples = 0;
    int OnScreenSamples = 0;
    
    for (int y = 0; y < SampleStepsPerLine; y++)
    {
        for (int x = 0; x < SampleStepsPerLine; x++)
        {
            float2 Offset = float2(x, y) / (SampleStepsPerLine - 1) - 0.5;
            float2 SampleScreenPos = ScreenPos + Offset * float2(SampleWindowWidthWorld, SampleWindowHeightWorld) * View.ScreenToWorldScale;
            
            if (IsOnScreen(SampleScreenPos))
            {
                OnScreenSamples++;
                
                float SceneDepth = CalcSceneDepth(SampleScreenPos);
                float ParticleDepth = ClipPos.w;
                
                if (ParticleDepth < SceneDepth)
                {
                    VisibleSamples++;
                }
            }
        }
    }
    
    VisibilityFraction = OnScreenSamples > 0 ? float(VisibleSamples) / float(OnScreenSamples) : 0;
    SampleFraction = float(OnScreenSamples) / (SampleStepsPerLine * SampleStepsPerLine);
}
```

#### 2. Circle Occlusion Query

Samples in concentric rings for more natural coverage:

```hlsl
void QueryOcclusionFactorWithCircle(
    float3 WorldPosition,
    float WorldDiameter,
    bool IncludeCenterSample,
    int NumberOfRings,
    int SamplesPerRing,
    out float VisibilityFraction,
    out float OnScreenFraction,
    out float DepthPassFraction)
{
    // Transform to clip space
    float4 ClipPos = mul(float4(WorldPosition, 1), View.WorldToClip);
    float ParticleDepth = ClipPos.w;
    
    int TotalSamples = 0;
    int OnScreenSamples = 0;
    int DepthPassSamples = 0;
    
    // Center sample
    if (IncludeCenterSample)
    {
        TotalSamples++;
        float2 ScreenUV = ClipPos.xy / ClipPos.w * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;
        
        if (IsOnScreen(ScreenUV))
        {
            OnScreenSamples++;
            float SceneDepth = CalcSceneDepth(ScreenUV);
            if (ParticleDepth < SceneDepth)
                DepthPassSamples++;
        }
    }
    
    // Concentric rings
    for (int Ring = 1; Ring <= NumberOfRings; Ring++)
    {
        float RingRadius = WorldDiameter * 0.5 * float(Ring) / float(NumberOfRings);
        
        for (int Sample = 0; Sample < SamplesPerRing; Sample++)
        {
            TotalSamples++;
            
            float Angle = 2.0 * PI * float(Sample) / float(SamplesPerRing);
            float3 SampleOffset = float3(cos(Angle), sin(Angle), 0) * RingRadius;
            float3 SampleWorldPos = WorldPosition + SampleOffset;
            
            float4 SampleClipPos = mul(float4(SampleWorldPos, 1), View.WorldToClip);
            float2 ScreenUV = SampleClipPos.xy / SampleClipPos.w * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;
            
            if (IsOnScreen(ScreenUV))
            {
                OnScreenSamples++;
                float SceneDepth = CalcSceneDepth(ScreenUV);
                if (ParticleDepth < SceneDepth)
                    DepthPassSamples++;
            }
        }
    }
    
    VisibilityFraction = OnScreenSamples > 0 ? float(OnScreenSamples - DepthPassSamples) / float(OnScreenSamples) : 0;
    OnScreenFraction = float(OnScreenSamples) / float(TotalSamples);
    DepthPassFraction = OnScreenSamples > 0 ? float(DepthPassSamples) / float(OnScreenSamples) : 0;
}
```

#### 3. Cloud Occlusion Query

Queries volumetric cloud texture:

```hlsl
void QueryCloudOcclusionWithCircle(
    float3 WorldPosition,
    float WorldDiameter,
    bool IncludeCenterSample,
    int NumberOfRings,
    int SamplesPerRing,
    out float VisibilityFraction,
    out float SampleFraction,
    out float3 AtmosphereTransmittance)
{
    Texture2D CloudVolumetricTexture;
    float2 CloudTextureUVScale;
    float2 CloudTextureUVMax;
    
    // Sample cloud texture at world position
    float2 CloudUV = WorldPosition.xz * CloudTextureUVScale;
    CloudUV = clamp(CloudUV, 0, CloudTextureUVMax);
    
    // Cloud density stored in texture
    float CloudDensity = CloudVolumetricTexture.SampleLevel(CloudSampler, CloudUV, 0).r;
    
    VisibilityFraction = 1.0 - CloudDensity;
    SampleFraction = 1.0;  // Always on screen
    
    // Atmosphere transmittance from atmospheric scattering
    AtmosphereTransmittance = GetAtmosphereTransmittance(WorldPosition);
}
```

### Output Interpretation

| Output | Range | Meaning |
|--------|-------|---------|
| `VisibilityFraction` | 0-1 | 0 = fully visible, 1 = fully occluded |
| `OnScreenFraction` | 0-1 | Fraction of samples inside viewport |
| `DepthPassFraction` | 0-1 | Fraction passing depth test |
| `AtmosphereTransmittance` | float3 | Light transmission through atmosphere |

### Shader Parameters

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, CloudVolumetricTexture)
    SHADER_PARAMETER(FVector2f, CloudVolumetricTextureUVScale)
    SHADER_PARAMETER(FVector2f, CloudVolumetricTextureUVMax)
    SHADER_PARAMETER_SAMPLER(SamplerState, CloudVolumetricTextureSampler)
END_SHADER_PARAMETER_STRUCT()
```

### Use Cases

| Scenario | Query Type | Sample Count |
|----------|------------|--------------|
| Large sprite occlusion | Circle | 1-4 rings, 4-8 samples/ring |
| Precise visibility | Rectangle | 3x3 to 5x5 grid |
| Cloud particle fading | Cloud | 1-2 rings |
| Rain behind objects | Circle | 2-3 rings, 4 samples/ring |

### Performance Considerations

1. **Sample Count**: Total samples = `Rings × SamplesPerRing + CenterSample`
2. **Early Out**: Use `OnScreenFraction` to skip off-screen particles
3. **Temporal Consistency**: Results can flicker; consider temporal smoothing

---

## Comparison

### Noise Types

| Type | Divergence | Quality | Use Case |
|------|------------|---------|----------|
| Curl Noise | Zero | Smooth | Fluid-like motion |
| Perlin Noise | Non-zero | Natural | Terrain, clouds |
| Simplex Noise | Non-zero | Fast | General purpose |
| Gradient Noise | Non-zero | Sharp | Displacement |

### Occlusion Query Performance

| Method | Complexity | Accuracy | Best For |
|--------|------------|----------|----------|
| Rectangle | O(n²) | Grid-aligned | Regular shapes |
| Circle | O(rings × samples) | Radial | Sprites, spheres |
| Cloud | O(1) texture sample | Approximate | Weather effects |

---

## References

- Curl Noise Paper: "Curl-Noise for Procedural Fluid Flow" (Bridson et al.)
- Engine Source: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceCurlNoise.cpp`
- Occlusion: `Engine/Plugins/FX/Niagara/Source/Niagara/Private/NiagaraDataInterfaceOcclusion.cpp`
- Simplex Noise: `Engine/Shaders/Private/Random.ush`
