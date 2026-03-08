# Niagara 仿真架构详解

本文档详细说明 Niagara 粒子系统的底层架构原理。

## CPU vs GPU 模式

Niagara 支持两种仿真目标（Simulation Target）：

```cpp
enum class ENiagaraSimTarget : uint8
{
    CPUSim,         // CPU 仿真，使用 VectorVM 虚拟机
    GPUComputeSim   // GPU 仿真，使用 Compute Shader
};
```

| 特性 | CPU 模式 | GPU 模式 |
|------|----------|----------|
| **执行方式** | VectorVM 解释执行字节码 | Compute Shader 直接执行 |
| **并行度** | 多线程（CPU 核心） | 大规模并行（GPU 线程） |
| **适合场景** | 复杂逻辑、数据接口访问、碰撞检测 | 大量粒子、简单物理模拟 |
| **数据访问** | 可访问任意引擎数据 | 需通过 Data Interface |
| **限制** | 粒子数量受限 | 不支持所有 Data Interface |

## 编译流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Niagara Script 编译流程                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Graph (UNiagaraGraph)                                              │
│       │                                                             │
│       ▼                                                             │
│  HLSL Translator (FNiagaraHlslTranslator)                          │
│       │                                                             │
│       ├─── CPU 模式 ───→ VectorVM Bytecode                         │
│       │                  (FNiagaraVMExecutableData)                 │
│       │                                                             │
│       └─── GPU 模式 ───→ Compute Shader HLSL                       │
│                          → Shader Compiler (DXC)                   │
│                          → GPU Bytecode                            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## VectorVM 虚拟机

CPU 模式使用 **VectorVM** 作为字节码解释器：

```cpp
// FNiagaraScriptExecutionContext 核心结构
struct FNiagaraScriptExecutionContextBase
{
    UNiagaraScript* Script;
    VectorVM::Runtime::FVectorVMState* VectorVMState;  // VM 状态
    
    // 外部函数表（Data Interface 调用）
    TArray<const FVMExternalFunction*> FunctionTable;
    TArray<void*> UserPtrTable;
    
    // 参数存储
    FNiagaraScriptInstanceParameterStore Parameters;
    
    // 数据集信息
    TArray<FNiagaraDataSetExecutionInfo> DataSetInfo;
};
```

**VectorVM 特点**：
- 基于寄存器的虚拟机
- 支持 SIMD 向量运算（SSE/AVX）
- 字节码在编辑器编译时生成
- 运行时解释执行，无需 JIT

## GPU Compute Shader 执行

GPU 模式使用 Compute Shader 进行并行计算：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    GPU 仿真管线                                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  FNiagaraGpuComputeDispatchInterface                               │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Compute Dispatch                                            │   │
│  │                                                              │   │
│  │  Dispatch(NumParticles / ThreadGroupSize)                   │   │
│  │      │                                                       │   │
│  │      ├──→ Spawn Kernel: 生成新粒子                           │   │
│  │      │                                                       │   │
│  │      ├──→ Update Kernel: 更新粒子状态                        │   │
│  │      │                                                       │   │
│  │      └──→ Event Kernel: 处理事件                             │   │
│  │                                                              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  数据流：                                                           │
│  [Particle Buffer UAV] ←→ [Constant Buffer] ←→ [Data Interface]   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## ParameterMap：核心数据结构

所有粒子数据存储在 **ParameterMap** 中：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    ParameterMap 结构                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Particles 命名空间（每个粒子实例）：                                │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Particles.Position      → float3[MaxParticles]              │   │
│  │ Particles.Velocity      → float3[MaxParticles]              │   │
│  │ Particles.Color         → float4[MaxParticles]              │   │
│  │ Particles.SpriteSize    → float2[MaxParticles]              │   │
│  │ Particles.Lifetime      → float[MaxParticles]               │   │
│  │ Particles.Age           → float[MaxParticles]               │   │
│  │ Particles.Mass          → float[MaxParticles]               │   │
│  │ Particles.SpriteRotation→ float[MaxParticles]               │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  Emitter 命名空间（每个发射器实例）：                                │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Emitter.SpawnRate        → float                            │   │
│  │ Emitter.Age              → float                            │   │
│  │ Emitter.SimulationTarget → ENiagaraSimTarget                │   │
│  │ Emitter.RandomSeed       → int32                            │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  Engine 命名空间（全局）：                                          │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ Engine.DeltaTime         → float                            │   │
│  │ Engine.SimTime           → float                            │   │
│  │ Engine.LODLevel          → float                            │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Module 执行模型

每个 Module 是一个 HLSL 函数，通过 ParameterMap 读写数据：

```hlsl
// CPU 模式：VectorVM 字节码
// GPU 模式：Compute Shader

// Module 函数签名
void ModuleFunction(
    inout FParticleParameterMap ParameterMap,  // 读写粒子数据
    const FConstantParameters Constants,       // 常量参数
    FDataInterfaceParameters DataInterfaces    // 数据接口
)
{
    // 读取: float3 pos = ParameterMap.Position;
    // 计算: pos += Velocity * DeltaTime;
    // 写入: ParameterMap.Position = pos;
}
```

## 数据流示例

以 `OmnidirectionalBurst` 为例：

```
┌──────────────────────────────────────────────────────────────────────┐
│                        Spawn Script (一次性执行)                      │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  [InputMap: 空]                                                      │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ InitializeParticle│  写入: Lifetime(1-2.25s), Color(white),      │
│  │                   │        SpriteSize(2-6), Mass(0.4-5.0)        │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ ShapeLocation     │  写入: Particles.Position                    │
│  │ (Sphere, R=40)    │        在半径40的球体内随机分布               │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ AddVelocity       │  写入: Particles.Velocity                    │
│  │                   │        向外方向 * 随机速度(75-500)            │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  [OutputMap] → 粒子带着初始属性诞生                                  │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│                     Update Script (每帧执行)                          │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  [InputMap: 当前所有粒子状态]                                        │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ ParticleState     │  检查: if (Age > Lifetime) Kill()            │
│  │                   │  更新: Age += DeltaTime                      │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ GravityForce      │  累加: Force += [0,0,-980] * Mass            │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ Drag              │  累加: Force -= Velocity * Drag(0.75)        │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ ScaleSpriteSize   │  修改: SpriteSize *= Curve(NormalizedAge)    │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────┐                                               │
│  │ ScaleColor        │  修改: Color.a *= Curve(NormalizedAge)       │
│  └───────────────────┘                                               │
│       │                                                              │
│       ▼                                                              │
│  ┌───────────────────────┐                                           │
│  │ SolveForcesAndVelocity│  最终结算:                               │
│  │                       │    Acceleration = Force / Mass           │
│  │                       │    Velocity += Acceleration * dt         │
│  │                       │    Position += Velocity * dt             │
│  │                       │    清空 Force 累加器                     │
│  └───────────────────────┘                                           │
│       │                                                              │
│       ▼                                                              │
│  [OutputMap] → 更新后的粒子状态 → Renderer                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

## Renderer 绑定

Renderer 作为消费者，从 ParameterMap 读取需要的属性：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    NiagaraSpriteRenderer                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  绑定的粒子属性:                                                    │
│  ┌─────────────────┐    ┌─────────────────┐                        │
│  │ Particles.      │    │                 │                        │
│  │ Position    ────┼──→ │ Sprite 世界位置 │                        │
│  │ Color       ────┼──→ │ Sprite 顶点颜色 │                        │
│  │ SpriteSize  ────┼──→ │ Sprite 缩放     │                        │
│  │ SpriteRotation ─┼──→ │ Sprite 旋转     │                        │
│  │ MaterialRandom ─┼──→ │ 材质随机值      │                        │
│  └─────────────────┘    └─────────────────┘                        │
│                                                                     │
│  GPU 渲染流程:                                                      │
│  1. 每个粒子生成一个 Billboard (始终面向相机)                        │
│  2. Vertex Shader: Position → Clip Space                           │
│  3. Pixel Shader: 采样材质 * Color                                  │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## CPU vs GPU 选择指南

| 场景 | 推荐模式 | 原因 |
|------|----------|------|
| 大量粒子 (>10,000) | GPU | 大规模并行优势 |
| 复杂数据接口访问 | CPU | GPU 有限制 |
| 需要碰撞检测 | CPU | GPU Collision 有额外开销 |
| 简单物理模拟 | GPU | 性能更优 |
| 需要事件系统 | CPU/GPU | 都支持，但 CPU 更灵活 |
| 需要与蓝图交互 | CPU | 更好的数据访问 |

## Module 执行顺序

Update Script 中的模块有依赖顺序：

```
ParticleState → Forces → Drag → Scale* → SolveForcesAndVelocity
```

**关键**: `SolveForcesAndVelocity` 必须是最后一个模块！
