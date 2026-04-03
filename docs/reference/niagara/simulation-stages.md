# Niagara Simulation Stages 深度解析

Simulation Stages 允许在单帧内对粒子进行多次迭代处理，实现复杂的模拟效果如流体、布料、约束求解等。

---

## 核心概念

### 什么是 Simulation Stage？

Simulation Stage 是 Niagara 中的一个执行阶段，可以在一帧内多次运行同一个脚本：

```
普通更新流程：
Spawn Script → Update Script → Renderers

带 Simulation Stage 的更新流程：
Spawn Script → Update Script → SimStage[0] → SimStage[1] → ... → SimStage[N] → Renderers
                  ↑_____________↑_____________↑
                        可能多次迭代
```

### 迭代源类型

```cpp
UENUM()
enum class ENiagaraIterationSource : uint8
{
    Particles,      // 遍历所有粒子
    DataInterface,  // 遍历 DataInterface 元素 (如 Grid)
    DirectSet       // 直接设置迭代次数 (X, Y, Z)
};
```

---

## 类结构

### UNiagaraSimulationStageBase

```cpp
// 头文件: NiagaraSimulationStageBase.h
UCLASS(Abstract)
class NIAGARA_API UNiagaraSimulationStageBase : public UObject
{
    UPROPERTY()
    UNiagaraScript* Script;  // 关联的脚本
    
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    bool bEnabled = true;    // 是否启用
    
    // 编译哈希
    #if WITH_EDITORONLY_DATA
    virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const;
    #endif
    
    // 辅助方法
    FVersionedNiagaraEmitter GetOuterEmitter() const;
    FVersionedNiagaraEmitterData* GetEmitterData() const;
    
    static const FName ParticleSpawnUpdateName;  // "ParticleSpawnUpdate"
};
```

### UNiagaraSimulationStageGeneric

```cpp
UCLASS()
class UNiagaraSimulationStageGeneric : public UNiagaraSimulationStageBase
{
    // === 启用绑定 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraVariableAttributeBinding EnabledBinding;
    
    // === 迭代源 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    ENiagaraIterationSource IterationSource = ENiagaraIterationSource::Particles;
    
    // === 粒子迭代 ===
    // 当 IterationSource == Particles 时使用
    
    // === DataInterface 迭代 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraDataInterfaceBinding DataInterface;
    // 当 IterationSource == DataInterface 时使用
    
    // === 直接设置迭代 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraParameterBindingWithValue<int32> ElementCountX;
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraParameterBindingWithValue<int32> ElementCountY;
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraParameterBindingWithValue<int32> ElementCountZ;
    // 当 IterationSource == DirectSet 时使用
    
    // === 迭代次数 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraParameterBindingWithValue<int32> NumIterations;
    // 每帧运行此 Stage 的次数
    
    // === 执行行为 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    ENiagaraSimulationStageExecuteBehavior ExecuteBehavior;
    // - OnSpawnAndUpdate: 生成和更新时执行
    // - OnSpawnOnly: 仅生成时执行
    // - OnUpdateOnly: 仅更新时执行
    
    // === 粒子迭代状态 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    bool bParticleIterationStateEnabled = false;
    
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FNiagaraVariableAttributeBinding ParticleIterationStateBinding;
    
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FIntPoint ParticleIterationStateRange = FIntPoint(0, -1);
    // 限制只处理特定状态范围的粒子
    
    // === 部分更新控制 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    bool bDisablePartialParticleUpdate = false;
    // 禁用部分粒子更新优化
    
    // === GPU 调度控制 ===
    UPROPERTY(EditAnywhere, Category = "GPU")
    bool bGpuDispatchForceLinear = false;
    // 强制线性调度 (1D)
    
    UPROPERTY(EditAnywhere, Category = "GPU")
    ENiagaraGpuDispatchType DirectDispatchType = ENiagaraGpuDispatchType::OneD;
    // - OneD: 1D 调度
    // - TwoD: 2D 调度
    // - ThreeD: 3D 调度
    
    UPROPERTY(EditAnywhere, Category = "GPU")
    ENiagaraGpuDispatchElementType DirectDispatchElementType = ENiagaraGpuDispatchElementType::Int32;
    // 元素类型 (Int32, Float 等)
    
    UPROPERTY(EditAnywhere, Category = "GPU")
    bool bOverrideGpuDispatchNumThreads = false;
    
    UPROPERTY(EditAnywhere, Category = "GPU")
    FNiagaraParameterBindingWithValue<int32> OverrideGpuDispatchNumThreadsX;
    UPROPERTY(EditAnywhere, Category = "GPU")
    FNiagaraParameterBindingWithValue<int32> OverrideGpuDispatchNumThreadsY;
    UPROPERTY(EditAnywhere, Category = "GPU")
    FNiagaraParameterBindingWithValue<int32> OverrideGpuDispatchNumThreadsZ;
    
    // === Stage 名称 ===
    UPROPERTY(EditAnywhere, Category = "Simulation Stage")
    FName SimulationStageName;
    // 用于在模块中识别当前 Stage
};
```

---

## 迭代模式详解

### 1. Particles 迭代

遍历所有粒子，每个粒子执行一次脚本：

```cpp
// 配置
IterationSource = ENiagaraIterationSource::Particles;

// 执行流程 (伪代码)
for (int32 Iteration = 0; Iteration < NumIterations; Iteration++)
{
    for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
    {
        if (ShouldProcessParticle(ParticleIndex, Iteration))
        {
            ExecuteScript(ParticleIndex);
        }
    }
}
```

**粒子状态过滤**：

```cpp
// 只处理状态在 [Min, Max] 范围内的粒子
bParticleIterationStateEnabled = true;
ParticleIterationStateBinding = "Particles.StateIndex";
ParticleIterationStateRange = FIntPoint(0, 2);  // 只处理状态 0-2 的粒子
```

### 2. DataInterface 迭代

遍历 DataInterface 中的元素：

```cpp
// 配置
IterationSource = ENiagaraIterationSource::DataInterface;
DataInterface = MyGridDataInterface;  // 例如 Grid3DCollection

// 执行流程 (伪代码)
int32 ElementCount = DataInterface->GetNumElements();
for (int32 Iteration = 0; Iteration < NumIterations; Iteration++)
{
    for (int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
    {
        ExecuteScript(ElementIndex);
    }
}
```

**常用 DataInterface**：
- `Grid2DCollection` / `Grid3DCollection` - 网格模拟
- `NeighborGrid3D` - 邻居查询网格
- `RenderTarget2D` - 像素处理

### 3. DirectSet 迭代

直接指定迭代范围：

```cpp
// 配置
IterationSource = ENiagaraIterationSource::DirectSet;
ElementCountX = 64;
ElementCountY = 64;
ElementCountZ = 1;

// 执行流程 (伪代码)
for (int32 Iteration = 0; Iteration < NumIterations; Iteration++)
{
    for (int32 Z = 0; Z < ElementCountZ; Z++)
    {
        for (int32 Y = 0; Y < ElementCountY; Y++)
        {
            for (int32 X = 0; X < ElementCountX; X++)
            {
                int32 Index = ComputeIndex(X, Y, Z);
                ExecuteScript(Index);
            }
        }
    }
}
```

**GPU 调度类型**：

```cpp
// 1D 调度 - 线性索引
DirectDispatchType = ENiagaraGpuDispatchType::OneD;
// Dispatch(ElementCountX * ElementCountY * ElementCountZ)

// 2D 调度 - 网格
DirectDispatchType = ENiagaraGpuDispatchType::TwoD;
// Dispatch(ElementCountX, ElementCountY)

// 3D 调度 - 体积
DirectDispatchType = ENiagaraGpuDispatchType::ThreeD;
// Dispatch(ElementCountX, ElementCountY, ElementCountZ)
```

---

## 编译数据

### FNiagaraSimulationStageCompilationData

```cpp
struct FNiagaraSimulationStageCompilationData
{
    // Stage 标识
    FGuid StageGuid;
    FName StageName;
    
    // 启用绑定
    FName EnabledBinding;
    
    // 元素计数
    FIntVector ElementCount;
    FName ElementCountXBinding;
    FName ElementCountYBinding;
    FName ElementCountZBinding;
    
    // 迭代次数
    int32 NumIterations;
    FName NumIterationsBinding;
    
    // 迭代源
    ENiagaraIterationSource IterationSourceType;
    FName IterationDataInterface;
    
    // 执行行为
    ENiagaraSimulationStageExecuteBehavior ExecuteBehavior;
    
    // 部分更新
    bool PartialParticleUpdate;
    
    // 粒子迭代状态
    bool bParticleIterationStateEnabled;
    FName ParticleIterationStateBinding;
    FIntPoint ParticleIterationStateRange;
    
    // GPU 调度
    bool bGpuDispatchForceLinear;
    ENiagaraGpuDispatchType DirectDispatchType;
    ENiagaraGpuDispatchElementType DirectDispatchElementType;
    bool bOverrideGpuDispatchNumThreads;
    FIntVector OverrideGpuDispatchNumThreads;
    FName OverrideGpuDispatchNumThreadsXBinding;
    FName OverrideGpuDispatchNumThreadsYBinding;
    FName OverrideGpuDispatchNumThreadsZBinding;
};
```

### 编译流程

```cpp
bool UNiagaraSimulationStageGeneric::FillCompilationData(
    TArray<FNiagaraSimulationStageCompilationData>& CompilationSimStageData) const
{
    FNiagaraSimulationStageCompilationData& SimStageData = CompilationSimStageData.AddDefaulted_GetRef();
    
    SimStageData.StageGuid = Script->GetUsageId();
    SimStageData.StageName = SimulationStageName;
    SimStageData.EnabledBinding = EnabledBinding.GetName();
    
    // 元素计数
    SimStageData.ElementCount = FIntVector(
        ElementCountX.GetDefaultValue<int32>(),
        ElementCountY.GetDefaultValue<int32>(),
        ElementCountZ.GetDefaultValue<int32>()
    );
    
    // 迭代源
    SimStageData.IterationSourceType = IterationSource;
    if (IterationSource == ENiagaraIterationSource::DataInterface)
    {
        SimStageData.IterationDataInterface = DataInterface.BoundVariable.GetName();
    }
    
    return true;
}
```

---

## 典型应用场景

### 1. 流体模拟

使用 Grid3DCollection 和多 Simulation Stage 实现 Navier-Stokes 方程：

```
Stage 0: "Advection"     - 平流
Stage 1: "Diffusion"     - 扩散
Stage 2: "Pressure"      - 压力求解 (多次迭代)
Stage 3: "Projection"    - 速度投影
```

```cpp
// Emitter 设置
SimulationStages = 
[
    {
        SimulationStageName: "Advection",
        IterationSource: DataInterface,
        DataInterface: VelocityGrid,
        NumIterations: 1
    },
    {
        SimulationStageName: "PressureSolve",
        IterationSource: DataInterface,
        DataInterface: PressureGrid,
        NumIterations: 20  // 雅可比迭代
    }
];
```

### 2. 约束求解

使用粒子迭代解决距离约束：

```cpp
// Stage 配置
IterationSource = Particles;
NumIterations = 4;  // 多次松弛迭代

// 在脚本中
for (int32 i = 0; i < NumIterations; i++)
{
    SolveDistanceConstraints();
}
```

### 3. 像素处理

使用 DirectSet 迭代处理 RenderTarget：

```cpp
// Stage 配置
IterationSource = DirectSet;
ElementCountX = RenderTargetWidth;
ElementCountY = RenderTargetHeight;
ElementCountZ = 1;

// GPU 优化
DirectDispatchType = TwoD;
bOverrideGpuDispatchNumThreads = true;
OverrideGpuDispatchNumThreadsX = 8;  // 8x8 线程组
OverrideGpuDispatchNumThreadsY = 8;
```

### 4. 邻居搜索

使用 NeighborGrid3D 进行空间划分：

```cpp
// Stage 1: 填充网格
SimulationStageName = "PopulateGrid";
IterationSource = Particles;
NumIterations = 1;

// Stage 2: 遍历网格处理邻居
SimulationStageName = "ProcessNeighbors";
IterationSource = DataInterface;
DataInterface = NeighborGrid;
NumIterations = 1;
```

---

## GPU 优化

### 线程组大小

```cpp
// 默认线程组大小 (取决于 DispatchType)
// OneD:   64 threads
// TwoD:   8x8 = 64 threads
// ThreeD: 4x4x4 = 64 threads

// 自定义线程组
bOverrideGpuDispatchNumThreads = true;
OverrideGpuDispatchNumThreadsX = 32;  // 32x32 = 1024 threads (最大)
OverrideGpuDispatchNumThreadsY = 32;
OverrideGpuDispatchNumThreadsZ = 1;
```

### 线性调度优化

```cpp
// 某些情况下强制线性调度更高效
bGpuDispatchForceLinear = true;

// 适用于：
// - 内存访问模式连续
// - 避免线程发散
```

### 部分更新优化

```cpp
// 默认启用部分更新：只处理活着的粒子
// 禁用后处理所有粒子 (适用于需要清理死亡的粒子)
bDisablePartialParticleUpdate = false;  // 默认

// 当需要处理死亡粒子时
bDisablePartialParticleUpdate = true;
```

---

## 运行时控制

### 动态启用/禁用

```cpp
// 通过参数绑定控制 Stage 启用
EnabledBinding = "Engine.Emitter.EnableSimulationStage";

// 在 Blueprint 或另一个 Stage 中设置
SetNiagaraVariableBool("Engine.Emitter.EnableSimulationStage", false);
```

### 动态迭代次数

```cpp
// 绑定迭代次数到参数
NumIterations = "Engine.SimStage.IterationCount";

// 动态调整
SetNiagaraVariableInt("Engine.SimStage.IterationCount", NewCount);
```

### 检测当前 Stage

在脚本中使用 Stage 名称进行条件判断：

```cpp
// 在 Update Script 中
if (SimulationStageName == "PressureSolve")
{
    // 只在压力求解阶段执行
    SolvePressure();
}
```

---

## 调试与性能

### Console Variables

```ini
; 查看当前激活的 Simulation Stages
fx.Niagara.Debug.SimulationStages 1

; 限制最大迭代次数
fx.Niagara.SimStage.MaxIterations 100
```

### 性能分析

Simulation Stage 的性能开销主要来自：
1. **迭代次数** - 每帧执行的次数
2. **元素数量** - 每次迭代处理的元素数
3. **脚本复杂度** - 每个元素执行的操作

```
性能估算：
每帧执行次数 ≈ NumIterations × ElementCount × ScriptCost

示例：
- NumIterations = 10
- ElementCount = 64×64 = 4096
- ScriptCost = 100 cycles

每帧开销 ≈ 10 × 4096 × 100 = 4,096,000 cycles ≈ ~1ms @ 4GHz
```

### 常见性能问题

| 问题 | 症状 | 解决方案 |
|------|------|----------|
| 过多迭代 | 帧率下降 | 减少 NumIterations 或使用 LOD |
| 元素过多 | GPU 峰值 | 降低分辨率或使用空间划分 |
| 内存带宽 | ALU 利用率低 | 优化数据布局，使用共享内存 |
| 线程发散 | GPU 效率低 | 重新设计算法避免分支 |

---

## 与其他系统配合

### 与 DataInterface 配合

```cpp
// Simulation Stage 可以访问 DataInterface 的特定功能
if (SimulationStageName == "GridUpdate")
{
    // 使用 Grid 的 3D 索引
    int3 GridCoord = GetGridCoord(ScriptIndex);
    float Value = GridDI.GetGridValue(GridCoord);
}
```

### 与 Event 配合

```cpp
// 在 Simulation Stage 中触发事件
if (SimulationStageName == "CollisionCheck")
{
    if (DetectCollision())
    {
        GenerateEvent("CollisionEvent");
    }
}
```

### 与 Data Channel 配合

```cpp
// 从 Data Channel 读取数据
if (SimulationStageName == "ExternalForces")
{
    FVector Force = DataChannelRead("ForceChannel", "Force");
    ApplyForce(Force);
}
```

---

## 最佳实践

### 1. 合理设置迭代次数

```cpp
// ✅ 根据质量要求动态调整
NumIterations.Bind("QualityLevel.Iterations");

// ❌ 硬编码高迭代次数
NumIterations = 100;
```

### 2. 使用粒子状态过滤

```cpp
// 只处理需要求解的粒子
bParticleIterationStateEnabled = true;
ParticleIterationStateRange = FIntPoint(1, 1);  // 状态 1 = 需要求解
```

### 3. 优化 GPU 调度

```cpp
// 2D 网格处理使用 2D 调度
DirectDispatchType = TwoD;
OverrideGpuDispatchNumThreadsX = 8;
OverrideGpuDispatchNumThreadsY = 8;
```

### 4. LOD 控制

```cpp
// 根据 LOD 级别减少迭代
int32 LODIterations = BaseIterations >> LODLevel;
NumIterations = LODIterations;
```

### 5. 分阶段处理

```cpp
// 将复杂模拟分解为多个阶段
// Stage 1: 低频更新 (每 N 帧一次)
// Stage 2: 高频更新 (每帧)
// Stage 3: 最终整合
```

---

## 扩展阅读

- [Grid DataInterfaces](data-interfaces-grid.md) - 网格模拟
- [Fluid Simulation](fluid-simulation.md) - 流体模拟实现
- [Data Channels](data-channels.md) - 跨系统通信
