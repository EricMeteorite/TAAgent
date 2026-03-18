# Niagara Data Interface 详解

> 本文档整理自 UE5.7 引擎源码 `Engine/Plugins/FX/Niagara/`

---

## 1. 概述

Data Interface 是 Niagara 与外部数据源交互的核心机制。它允许粒子系统：
- 读取引擎数据（纹理、网格、相机、音频等）
- 写入数据（Render Target、数组等）
- 执行查询（碰撞、射线检测等）
- 与蓝图通信

### 核心架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                    UNiagaraDataInterface 继承体系                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  UNiagaraDataInterfaceBase                                          │
│       │                                                             │
│       └── UNiagaraDataInterface                                     │
│               │                                                     │
│               ├── UNiagaraDataInterfaceRWBase (可读写)              │
│               │       ├── Grid2DCollection                          │
│               │       ├── Grid3DCollection                          │
│               │       ├── NeighborGrid3D                            │
│               │       └── Array 系列                                │
│               │                                                     │
│               ├── Texture 系列 (只读)                               │
│               ├── Mesh 系列                                         │
│               ├── Curve 系列                                        │
│               ├── CollisionQuery                                    │
│               ├── Camera                                            │
│               ├── Audio 系列                                        │
│               └── Export (数据导出)                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 执行模型

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CPU 模式 (VectorVM)                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Game Thread                                                        │
│  ├── InitPerInstanceData()     - 初始化实例数据                     │
│  ├── PerInstanceTick()         - 每帧更新                           │
│  ├── PreStageTick()            - Stage 前处理                       │
│  ├── [VM 执行模块]                                                  │
│  │   └── GetVMExternalFunction() - 绑定 VM 函数                    │
│  ├── PostStageTick()           - Stage 后处理                       │
│  └── DestroyPerInstanceData()  - 销毁实例数据                       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    GPU 模式 (Compute Shader)                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Game Thread                                                        │
│  ├── InitPerInstanceData()                                          │
│  ├── PerInstanceTick()                                              │
│  └── ProvidePerInstanceDataForRenderThread()  - 传递数据到渲染线程  │
│                                                                     │
│  Render Thread (通过 Proxy)                                         │
│  ├── ResetData()               - 重置数据                           │
│  ├── PreStage()                - Stage 前处理                       │
│  ├── [Compute Shader 执行]                                          │
│  │   ├── BuildShaderParameters() - 构建着色器参数                   │
│  │   └── SetShaderParameters()   - 设置着色器参数                   │
│  ├── PostStage()               - Stage 后处理                       │
│  └── PostSimulate()            - 模拟后处理                         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Data Interface 分类

### 2.1 纹理类 (Texture)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceTexture` | 采样 2D 纹理 | ✅ | ❌ |
| `NiagaraDataInterfaceCubeTexture` | 采样 Cubemap | ✅ | ❌ |
| `NiagaraDataInterfaceVolumeTexture` | 采样 3D 纹理 | ✅ | ❌ |
| `NiagaraDataInterfaceRenderTarget2D` | 读写 2D Render Target | ✅ | ❌ |
| `NiagaraDataInterfaceRenderTarget2DArray` | 读写 Render Target 数组 | ✅ | ❌ |
| `NiagaraDataInterfaceRenderTargetCube` | 读写 Cubemap Render Target | ✅ | ❌ |
| `NiagaraDataInterfaceRenderTargetVolume` | 读写 3D Render Target | ✅ | ❌ |
| `NiagaraDataInterfaceIntRenderTarget2D` | 整数 Render Target | ✅ | ❌ |
| `NiagaraDataInterfaceSparseVolumeTexture` | 稀疏体积纹理 | ✅ | ❌ |
| `NiagaraDataInterfaceVirtualTextureSample` | 虚拟纹理采样 | ✅ | ❌ |
| `NiagaraDataInterface2DArrayTexture` | 2D 纹理数组 | ✅ | ❌ |

**典型用法**：
```
// 采样纹理
Texture.Sample(UV) → float4

// 获取尺寸
Texture.GetDimensions() → int2

// 写入 Render Target
RenderTarget.SetPixel(Position, Color)
```

---

### 2.2 网格类 (Mesh)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceSkeletalMesh` | 骨骼网格采样 | ✅ | ✅ |
| `NiagaraDataInterfaceStaticMesh` | 静态网格采样 | ✅ | ✅ |
| `NiagaraDataInterfaceDynamicMesh` | 动态网格 | ✅ | ✅ |
| `NiagaraDataInterfaceMeshRendererInfo` | 渲染器信息 | ✅ | ✅ |
| `NiagaraDataInterfaceSpriteRendererInfo` | Sprite 渲染器信息 | ✅ | ✅ |

**SkeletalMesh 功能**：
- 采样顶点位置（蒙皮后）
- 获取骨骼变换
- UV 映射
- 三角形连通性
- 双缓冲支持速度计算

```
// 采样蒙皮顶点
SkeletalMesh.GetSkinnedPosition(VertexIndex) → float3

// 获取骨骼矩阵
SkeletalMesh.GetBoneMatrix(BoneIndex) → float4x4

// 随机采样表面
SkeletalMesh.GetRandomTrianglePosition() → float3
```

---

### 2.3 Grid 类 (流体/网格数据)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceGrid2D` | 2D 网格基类 | ✅ | ❌ |
| `NiagaraDataInterfaceGrid2DCollection` | 2D 网格集合 (多属性) | ✅ | ❌ |
| `NiagaraDataInterfaceGrid2DCollectionReader` | 只读 2D 网格 | ✅ | ❌ |
| `NiagaraDataInterfaceGrid3D` | 3D 网格基类 | ✅ | ❌ |
| `NiagaraDataInterfaceGrid3DCollection` | 3D 网格集合 | ✅ | ❌ |
| `NiagaraDataInterfaceNeighborGrid3D` | 邻居搜索网格 (SPH) | ✅ | ❌ |
| `NiagaraDataInterfaceRasterizationGrid3D` | 光栅化 3D 网格 | ✅ | ❌ |

**核心功能**：
- Simulation ↔ Unit ↔ Index 坐标转换
- 属性读写（密度、速度、温度等）
- 迭代源 (Iteration Source) 支持

```
// 坐标转换
Grid.SimulationToUnit(WorldPos) → float3
Grid.UnitToIndex(UnitPos) → int3
Grid.IndexToLinear(Index) → int

// 属性访问
Grid.GetAttribute(Name, Index) → float4
Grid.SetAttribute(Name, Index, Value)
```

**NeighborGrid3D** (SPH 流体关键)：
```
// 填充邻居网格
FillNeighborGrid3D()

// 遍历邻居
For each neighbor in cell:
    ComputeDensity(Particle, Neighbor)
```

---

### 2.4 曲线类 (Curve)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceCurve` | Float 曲线 | ✅ | ✅ |
| `NiagaraDataInterfaceColorCurve` | 颜色曲线 | ✅ | ✅ |
| `NiagaraDataInterfaceVectorCurve` | Vector 曲线 | ✅ | ✅ |
| `NiagaraDataInterfaceVector2DCurve` | Vector2 曲线 | ✅ | ✅ |
| `NiagaraDataInterfaceVector4Curve` | Vector4 曲线 | ✅ | ✅ |
| `NiagaraDataInterfaceCurlNoise` | Curl Noise | ✅ | ✅ |

**实现原理**：
- 预计算 LUT (Look-Up Table)
- GPU 通过 Buffer 传输
- 可优化 LUT 大小

```
// 采样曲线
Curve.Sample(Time) → float

// Curl Noise (无散度噪声)
CurlNoise.Sample(Position) → float3
```

---

### 2.5 数组类 (Array)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceArrayFloat` | Float 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayInt` | Int 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayBool` | Bool 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayVector` | Vector 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayVector2D` | Vector2D 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayVector4` | Vector4 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayColor` | Color 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayQuaternion` | Quat 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayMatrix` | Matrix 数组 | ✅ | ✅ |
| `NiagaraDataInterfaceArrayNiagaraID` | NiagaraID 数组 | ✅ | ✅ |

**典型用法**：
```
// 读取
Array.GetElement(Index) → T

// 写入
Array.SetElement(Index, Value)

// 添加
Array.Add(Value) → NewIndex

// 长度
Array.Length() → int
```

---

### 2.6 碰撞/查询类 (Collision/Query)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceCollisionQuery` | 碰撞查询 | ✅ | ✅ |
| `NiagaraDataInterfaceRigidMeshCollisionQuery` | 刚体网格碰撞 | ✅ | ✅ |
| `NiagaraDataInterfaceOcclusion` | 遮挡查询 | ✅ | ❌ |
| `NiagaraDataInterfaceAsyncGpuTrace` | 异步 GPU 射线 | ✅ | ❌ |

**CollisionQuery 功能**：
- Ray Trace (射线检测)
- Distance Field 查询
- Depth Buffer 采样
- SDF (Signed Distance Field) 查询

```
// 射线检测
CollisionQuery.Raycast(Start, End, Channel) → HitResult

// Distance Field 查询
CollisionQuery.QueryDistanceField(Position) → float

// 获取深度
CollisionQuery.GetDepth(UV) → float
```

---

### 2.7 相机类 (Camera)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceCamera` | 相机查询 | ✅ | ✅ |

**功能**：
```
// 获取相机属性
Camera.GetCameraProperties() → Position, Rotation, FOV

// 计算粒子距离
Camera.CalculateParticleDistances()

// 获取最近粒子
Camera.GetClosestParticles(Count) → ParticleIDs

// 分屏信息
Camera.GetSplitScreenInfo()
```

---

### 2.8 音频类 (Audio)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceAudioSubmix` | 音频 Submix 采样 | ❌ | ✅ |
| `NiagaraDataInterfaceAudioOscilloscope` | 示波器 | ❌ | ✅ |
| `NiagaraDataInterfaceAudioSpectrum` | 频谱分析 | ❌ | ✅ |
| `NiagaraDataInterfaceAudioPlayer` | 音频播放 | ❌ | ✅ |

**实现原理**：
- 通过 `ISubmixBufferListener` 捕获音频
- CPU 模式下直接访问音频缓冲
- 支持 FFT 频谱分析

```
// 获取音频数据
Audio.GetAmplitude() → float
Audio.GetSpectrumData(BucketIndex) → float
```

---

### 2.9 数据导出类 (Export)

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceExport` | 导出到蓝图 | ✅ | ✅ |
| `NiagaraDataInterfaceParticleRead` | 读取其他 Emitter 粒子 | ✅ | ✅ |

**Export 工作流**：
```
1. 在 Niagara 中调用 Export.StoreData(Position, Velocity, Size)
2. 每帧结束后自动回调蓝图的 ReceiveParticleData
3. 蓝图接收 TArray<FBasicParticleData>
```

**蓝图接口**：
```cpp
UINTERFACE(BlueprintType)
class UNiagaraParticleCallbackHandler : public UInterface
{
    // 蓝图实现此接口
    void ReceiveParticleData(TArray<FBasicParticleData> Data);
};
```

---

### 2.10 其他常用 Data Interface

| 类名 | 用途 | GPU | CPU |
|------|------|-----|-----|
| `NiagaraDataInterfaceSpline` | 样条线采样 | ✅ | ✅ |
| `NiagaraDataInterfaceLandscape` | 地形采样 | ✅ | ✅ |
| `NiagaraDataInterfaceVectorField` | 向量场 | ✅ | ✅ |
| `NiagaraDataInterfaceDataTable` | 数据表 | ✅ | ✅ |
| `NiagaraDataInterfaceDataChannelRead` | 数据通道读取 | ✅ | ✅ |
| `NiagaraDataInterfaceDataChannelWrite` | 数据通道写入 | ✅ | ✅ |
| `NiagaraDataInterfaceSimpleCounter` | 简单计数器 | ✅ | ✅ |
| `NiagaraDataInterfaceVolumeCache` | 体积缓存 | ✅ | ✅ |

---

## 3. 自定义 Data Interface

### 3.1 基本结构

```cpp
// 1. 定义类
UCLASS(EditInlineNew, Category = "Custom", meta = (DisplayName = "My Data Interface"))
class UNiagaraDataInterfaceMyDI : public UNiagaraDataInterface
{
    GENERATED_UCLASS_BODY()
    
    // Shader 参数结构
    BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
        SHADER_PARAMETER(float, MyValue)
        SHADER_PARAMETER_SRV(Buffer<float>, MyBuffer)
    END_SHADER_PARAMETER_STRUCT()
    
public:
    // 必须实现的方法
    virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, 
                                        void* InstanceData, 
                                        FVMExternalFunction& OutFunc) override;
    virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;
    
    // GPU 支持
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& Builder) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
    
    // 实例数据
    virtual int32 PerInstanceDataSize() const override;
    virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    
protected:
#if WITH_EDITORONLY_DATA
    virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif
};
```

### 3.2 实例数据管理

```cpp
// 定义实例数据结构
struct FMyDIInstanceData
{
    float CachedValue;
    TArray<float> TempData;
};

// 初始化
bool UNiagaraDataInterfaceMyDI::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    new (PerInstanceData) FMyDIInstanceData();
    return true;
}

// 销毁
void UNiagaraDataInterfaceMyDI::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    static_cast<FMyDIInstanceData*>(PerInstanceData)->~FMyDIInstanceData();
}

int32 UNiagaraDataInterfaceMyDI::PerInstanceDataSize() const
{
    return sizeof(FMyDIInstanceData);
}
```

### 3.3 VM 函数绑定

```cpp
// 注册函数签名
void UNiagaraDataInterfaceMyDI::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
    FNiagaraFunctionSignature Sig;
    Sig.Name = FName("MyFunction");
    Sig.bMemberFunction = true;
    Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DI")));
    Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Param")));
    Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Result")));
    OutFunctions.Add(Sig);
}

// 绑定 VM 函数
void UNiagaraDataInterfaceMyDI::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, 
                                                       void* InstanceData, 
                                                       FVMExternalFunction& OutFunc)
{
    if (BindingInfo.Name == FName("MyFunction"))
    {
        OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) 
        {
            this->MyFunctionVM(Context);
        });
    }
}

// VM 函数实现
void UNiagaraDataInterfaceMyDI::MyFunctionVM(FVectorVMExternalFunctionContext& Context)
{
    // 获取实例数据
    VectorVM::FUserPtrHandler<FMyDIInstanceData> InstData(Context);
    
    // 输入参数
    FNDIInputParam<float> Param(Context);
    
    // 输出参数
    FNDIOutputParam<float> Result(Context);
    
    // 遍历粒子
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        float P = Param.GetAndAdvance();
        Result.SetAndAdvance(P * InstData.Get()->CachedValue);
    }
}
```

### 3.4 GPU 支持

```cpp
// 构建着色器参数
void UNiagaraDataInterfaceMyDI::BuildShaderParameters(FNiagaraShaderParametersBuilder& Builder) const
{
    Builder.AddNestedStruct<FShaderParameters>();
}

// 设置着色器参数
void UNiagaraDataInterfaceMyDI::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FShaderParameters* Params = Context.GetParameterNestedStruct<FShaderParameters>();
    Params->MyValue = ComputeValue();
    Params->MyBuffer = GetBufferSRV();
}
```

### 3.5 HLSL 模板

```hlsl
// NiagaraDataInterfaceMyDI.ush

// 参数定义 (由 GetParameterDefinitionHLSL 注入)
struct FMyDIParameters
{
    float MyValue;
    Buffer<float> MyBuffer;
};

// 函数实现
float MyDI_MyFunction(FMyDIParameters DI, float Param)
{
    return Param * DI.MyValue;
}
```

---

## 4. GPU vs CPU 支持矩阵

| Data Interface | GPU 支持 | CPU 支持 | 备注 |
|---------------|---------|---------|------|
| Texture | ✅ | ❌ | 纯 GPU |
| RenderTarget | ✅ | ❌ | 需要 UAV |
| SkeletalMesh | ✅ | ✅ | 蒙皮数据双缓冲 |
| StaticMesh | ✅ | ✅ | - |
| Grid2DCollection | ✅ | ❌ | Compute Shader 专用 |
| Grid3DCollection | ✅ | ❌ | Compute Shader 专用 |
| NeighborGrid3D | ✅ | ❌ | SPH 流体专用 |
| Curve 系列 | ✅ | ✅ | LUT 预计算 |
| Array 系列 | ✅ | ✅ | UAV/Buffer |
| CollisionQuery | ✅ | ✅ | GPU 需 DF/Depth |
| Camera | ✅ | ✅ | - |
| Audio | ❌ | ✅ | 仅 CPU |
| Export | ✅ | ✅ | GPU 需要 Readback |
| Spline | ✅ | ✅ | - |
| Landscape | ✅ | ✅ | - |

---

## 5. 性能优化建议

### 5.1 CPU vs GPU 选择

| 场景 | 推荐 | 原因 |
|------|------|------|
| 大量粒子 (>10000) | GPU | 并行优势 |
| 复杂数据访问 | CPU | GPU Data Interface 有限制 |
| 需要蓝图交互 | CPU | 更好的数据访问 |
| 流体模拟 | GPU | Grid2D/3D 需要 GPU |
| 碰撞检测 | CPU | 更灵活，GPU 有额外开销 |

### 5.2 常见陷阱

1. **GPU 模式使用不支持的数据接口**
   - 检查 `CanExecuteOnTarget(ENiagaraSimTarget::GPUComputeSim)`
   
2. **频繁的数据传输**
   - GPU Readback 是昂贵的操作
   - 使用 `PostSimulateCanOverlapFrames()` 优化

3. **实例数据未正确管理**
   - 必须实现 `PerInstanceDataSize()` 返回正确大小
   - 使用 placement new 构造对象

---

## 6. 参考资源

| 资源 | 位置 |
|------|------|
| 引擎源码 | `Engine/Plugins/FX/Niagara/Source/Niagara/Classes/` |
| 示例插件 | `Engine/Plugins/FX/ExampleCustomDataInterface/` |
| Shader 模板 | `Engine/Plugins/FX/Niagara/Shaders/` |
