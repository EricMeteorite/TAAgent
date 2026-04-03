# Niagara Hair Strands (Groom) 深度分析

## 概述

本文档详细分析 UE5 Niagara Hair Strands Data Interface 的实现机制，包括数据结构、物理模拟算法、约束求解和渲染流程。

---

## 架构设计

### 核心组件

```
┌─────────────────────────────────────────────────────────────────┐
│                    Niagara Hair Strands 架构                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CPU Layer (Game Thread)                                         │
│  ├── UGroomAsset (头发资源)                                      │
│  ├── UGroomComponent (场景组件)                                  │
│  ├── FHairGroupInstance (运行时实例)                             │
│  │   ├── Guides (导向发丝)                                       │
│  │   │   ├── RestResource (初始状态)                             │
│  │   │   └── DeformedResource (变形状态)                         │
│  │   └── Strands (渲染发丝)                                      │
│  └── UNiagaraDataInterfaceHairStrands (DI 接口)                  │
│                                                                  │
│  GPU Layer (Render Thread)                                       │
│  ├── RestPositionBuffer (SRV) - 初始位置                         │
│  ├── DeformedPositionBuffer (UAV) - 变形位置                     │
│  ├── CurvesOffsetsBuffer (SRV) - 曲线偏移                        │
│  ├── DeformedPositionOffset (SRV) - 位置偏移                     │
│  └── Skinning Buffers (可选)                                     │
│      ├── RestTrianglePositionBuffer                              │
│      ├── DeformedTrianglePositionBuffer                          │
│      ├── RootBarycentricCoordinatesBuffer                        │
│      └── RootToUniqueTriangleIndexBuffer                         │
│                                                                  │
│  Niagara Simulation                                              │
│  ├── Spawn Script (初始化节点位置)                               │
│  ├── Update Script (物理模拟)                                    │
│  │   ├── AdvectNodePosition (运动更新)                           │
│  │   ├── SolveStretchConstraint (拉伸约束)                       │
│  │   ├── SolveBendConstraint (弯曲约束)                          │
│  │   └── SolveCollisionConstraint (碰撞约束)                     │
│  └── Write Back (写回 DeformedPositionBuffer)                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 数据结构设计

### 1. 头发曲线 (FHairCurve)

```cpp
// 存储在 CurvesOffsetsBuffer
struct FHairCurve
{
    uint32 PointIndex;   // 曲线起始点在 PointBuffer 中的索引
    uint32 PointCount;   // 曲线上的控制点数量
};
```

### 2. 位置数据存储

```hlsl
// RestPositionBuffer - 压缩存储
struct FPackedHairPosition
{
    uint32 Data[2];  // 压缩的 XYZ 位置
};

// 解压后
struct FHairControlPoint
{
    float3 Position;  // 本地空间位置
};
```

### 3. GPU Buffer 布局

```hlsl
// Shader Parameter Struct
BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
    // 可写 - 变形后的位置
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, DeformedPositionBuffer)
    
    // 只读 - 初始位置
    SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, RestPositionBuffer)
    
    // 只读 - 曲线数据
    SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CurvesOffsetsBuffer)
    
    // 只读 - 皮肤绑定数据
    SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, RestTrianglePositionBuffer)
    SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, DeformedTrianglePositionBuffer)
    SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RootBarycentricCoordinatesBuffer)
    SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, RootToUniqueTriangleIndexBuffer)
    
    // 变换矩阵
    SHADER_PARAMETER(FMatrix44f, WorldTransform)
    SHADER_PARAMETER(FMatrix44f, WorldInverse)
    SHADER_PARAMETER(FQuat4f, WorldRotation)
    
    // 骨骼变换（局部模拟）
    SHADER_PARAMETER(FMatrix44f, BoneTransform)
    SHADER_PARAMETER(FMatrix44f, BoneInverse)
    SHADER_PARAMETER(FQuat4f, BoneRotation)
    
    // 参数
    SHADER_PARAMETER(int, NumStrands)       // 发束数量
    SHADER_PARAMETER(int, StrandSize)       // 每束节点数
    SHADER_PARAMETER(int, InterpolationMode)// 插值模式
END_SHADER_PARAMETER_STRUCT()
```

---

## 物理模拟算法

### 1. XPBD (Extended Position Based Dynamics)

Niagara Hair 使用 XPBD 算法进行物理模拟，相比 PBD 有更好的收敛性和稳定性。

#### 核心公式

```
// XPBD 约束求解
// C(x) = 0 是约束条件

// 1. 计算约束值和梯度
dL = -(C + alpha * L) / (dC * invM * dCt + alpha)

// 2. 更新拉格朗日乘子
L += dL

// 3. 更新位置
dx = dL * dCt * invM
x += dx
```

### 2. 拉伸约束 (Stretch Constraint)

```hlsl
// NiagaraHookeSpringMaterial.ush

void UpdateStretchSpringMultiplier(
    in float RestLength,
    in float DeltaTime,
    in bool ProjectConstraint,
    in float MaterialDamping,
    in float MaterialCompliance,
    in float MaterialWeight,
    inout float OutMaterialMultiplier,
    in int NodeOffset)
{
    // 计算边方向和长度
    float3 EdgeDirection = SharedNodePosition[GGroupThreadId.x] 
                         - SharedNodePosition[GGroupThreadId.x-1-NodeOffset];
    float EdgeLength = length(EdgeDirection);
    EdgeDirection /= EdgeLength;
    
    // 计算速度差
    const float3 DeltaVelocity = (EdgeDirection - (SharedPreviousPosition[GGroupThreadId.x] 
                                 - SharedPreviousPosition[GGroupThreadId.x-1-NodeOffset])) / DeltaTime;
    
    // XPBD 拉格朗日乘子更新
    // dL = -(C + compliance*L) / (dC * invM * dCt + alpha)
    const float DeltaLambda = -(
        (EdgeLength - RestLength) / RestLength           // 约束值 C
        + OutMaterialMultiplier * MaterialCompliance     // alpha * L
        + MaterialDamping * dot(EdgeDirection, DeltaVelocity) / RestLength  // 阻尼
    ) * MaterialWeight;
    
    // L += dL
    OutMaterialMultiplier += DeltaLambda;
    
    // 位置更新: dx = dL * dCt * invM
    const float3 PositionDelta = EdgeDirection * DeltaLambda / RestLength;
    
    // 应用位置修正
    SharedNodePosition[GGroupThreadId.x] += PositionDelta * SharedInverseMass[GGroupThreadId.x];
    if(!ProjectConstraint)
    {
        SharedNodePosition[PreviousIndex] -= PositionDelta * SharedInverseMass[PreviousIndex];
    }
}
```

#### Compliance 计算

```hlsl
// Compliance = 1.0 / (k * dt * dt)
// k = L * L * (Y * A / L) = L * Y * A
// A = PI * R * R (横截面积)
// Y = YoungModulus (杨氏模量)

OutMaterialCompliance = 4.0 / (YoungModulus * PI * RodThickness * RestLength 
                      * RodThickness * DeltaTime * DeltaTime);
```

### 3. 弯曲约束 (Bend Constraint)

弯曲约束使用与拉伸约束相同的代码，但作用于隔一个节点的边：

```hlsl
// Bend constraint uses NodeOffset = 1
// 连接节点 i 和 i-2（跳过中间节点）
ResetStretchSpringMaterial(..., NodeOffset = 1);
UpdateStretchSpringMultiplier(..., NodeOffset = 1);
```

### 4. Cosserat Rod 材料模型

对于更精确的毛发模拟，使用 Cosserat Rod 模型，考虑方向和扭转：

```hlsl
// NiagaraCosseratRodMaterial.ush

// Stretch Rod - 带方向的拉伸
void UpdateStretchRodMultiplier(...)
{
    // 从四元数获取目标方向
    float4 q0 = SharedNodeOrientation[GGroupThreadId.x-1];
    TargetDirection[0] = 2.0 * (q0.x * q0.z + q0.w * q0.y);
    TargetDirection[1] = 2.0 * (q0.y * q0.z - q0.w * q0.x);
    TargetDirection[2] = q0.w * q0.w - q0.x * q0.x - q0.y * q0.y + q0.z * q0.z;
    
    // 计算边方向与目标方向的差异
    const float3 DeltaLambda = -(
        EdgeDirection / RestLength - TargetDirection  // 方向差异
        + OutMaterialMultiplier * MaterialCompliance
        + MaterialDamping * DeltaVelocity / RestLength
    ) * MaterialWeight;
    
    // 更新位置...
}

// Align Rod - 方向对齐约束
void UpdateAlignRodMultiplier(...)
{
    // 确保边方向与节点方向对齐
    float4 qebar = float4(-q0.y, q0.x, -q0.w, q0.z);
    
    // 计算四元数阻尼
    const float4 DeltaQuat = (SharedNodeOrientation[GGroupThreadId.x-1] 
                            - SharedPreviousOrientation[GGroupThreadId.x-1]) / DeltaTime;
    const float3 QuatDamping = -2.0 * (eqbar.xyz * DeltaQuat.w + eqbar.w * DeltaQuat.xyz 
                            - cross(eqbar.xyz, DeltaQuat.xyz));
    
    // XPBD 更新...
}
```

### 5. 红-黑排序并行求解 (Red-Black Ordering)

为了避免并行写入冲突，使用红-黑排序：

```hlsl
void SolveStretchSpringMaterial(...)
{
    if(EnableConstraint)
    {
        const int LocalIndex = (GGroupThreadId.x % StrandSize);
        if(LocalIndex > 1)
        {
            // 红节点 (偶数索引)
            const int IsRed = (GGroupThreadId.x % 2) == 0;
            if (IsRed)
            {    
                UpdateStretchSpringMultiplier(...);
            }
            GroupMemoryBarrier();  // 等待红节点完成
            
            // 黑节点 (奇数索引)
            if (!IsRed)
            {
                UpdateStretchSpringMultiplier(...);
            }
            GroupMemoryBarrier();  // 等待黑节点完成
        }
    }
}
```

---

## 皮肤绑定与插值

### 三种插值模式

```cpp
enum class EHairSimulationInterpolationMode : uint8
{
    Rigid = 0,      // 刚性绑定
    Skinned = 1,    // 骨骼皮肤绑定
    RBF = 2         // 径向基函数插值
};
```

### Skinned 模式实现

```hlsl
// 1. 获取根部绑定信息
const float2 ProjectionUV = UnpackBarycentrics(RootBarycentricCoordinatesBuffer[StrandIndex]);
const uint TriangleIndex = RootToUniqueTriangleIndexBuffer[StrandIndex];

// 2. 构建 Rest 状态的三角形
BuildRestTriangle_HairStrands(ProjectionUV, TriangleIndex, 
    RestTrianglePosition, RestTriangleOrientation);

// 3. 构建 Deformed 状态的三角形
BuildDeformedTriangle_HairStrands(ProjectionUV, TriangleIndex,
    DeformedTrianglePosition, DeformedTriangleOrientation);

// 4. 将世界位置转换到三角形局部空间
float3 LocalRestPosition = TriangleLocalPosition(
    RestTrianglePosition, RestTriangleOrientation, WorldRestPosition);

// 5. 从局部空间转换到变形后的世界空间
float3 DeformedWorldPosition = TriangleWorldPosition(
    DeformedTrianglePosition, DeformedTriangleOrientation, LocalRestPosition);
```

### 三角形插值计算

```hlsl
void ComputeProjectionTriangle_HairStrands(
    in float2 ProjectionUV,
    in float3 TriangleVertex0,
    in float3 TriangleVertex1,
    in float3 TriangleVertex2,
    in float3 RootOffset,
    out float3 OutTrianglePosition,
    out float4 OutTriangleOrientation)
{
    // 计算重心坐标
    const float U = ProjectionUV.x;
    const float V = ProjectionUV.y;
    const float W = 1.0 - U - V;
    
    // 插值位置
    OutTrianglePosition = TriangleVertex0 * U 
                        + TriangleVertex1 * V 
                        + TriangleVertex2 * W 
                        + RootOffset;
    
    // 从三角形边计算旋转
    const float3 Edge0 = normalize(TriangleVertex1 - TriangleVertex0);
    const float3 Edge2 = normalize(TriangleVertex2 - TriangleVertex0);
    const float3 Normal = normalize(cross(Edge0, Edge2));
    const float3 Edge1 = cross(Normal, Edge0);
    
    // 构建旋转矩阵 -> 四元数
    const float3x3 RotationMatrix = float3x3(Edge0, Edge1, Normal);
    OutTriangleOrientation = RotationMatrixToQuaternion(RotationMatrix);
}
```

---

## 数据流与函数接口

### 核心 HLSL 函数

#### 初始化函数

```hlsl
// 计算节点初始位置（从 RestPositionBuffer）
void ComputeNodePosition_HairStrands(out float3 OutNodePosition);

// 计算节点方向
void ComputeNodeOrientation_HairStrands(in float3 NodePosition, out float4 OutNodeOrientation);

// 计算质量和惯性
void ComputeNodeMass_HairStrands(in float StrandsDensity, in float NodeThickness, out float OutNodeMass);
void ComputeNodeInertia_HairStrands(in float StrandsDensity, in float NodeThickness, out float3 OutNodeInertia);
```

#### 绑定函数

```hlsl
// 将 Rest 位置绑定到世界空间
void AttachNodePosition_HairStrands(in float3 RestPosition, out float3 OutNodePosition);

// 将 Rest 方向绑定到世界空间
void AttachNodeOrientation_HairStrands(in float4 RestOrientation, out float4 OutNodeOrientation);

// 计算局部状态（用于皮肤绑定）
void ComputeLocalState_HairStrands(
    in float3 RestPosition, in float4 RestOrientation,
    out float3 OutLocalPosition, out float4 OutLocalOrientation);

// 从局部状态应用变形
void AttachNodeState_HairStrands(
    in float3 LocalPosition, in float4 LocalOrientation,
    out float3 OutNodePosition, out float4 OutNodeOrientation);
```

#### 积分函数

```hlsl
// 位置积分（外力 -> 速度 -> 位置）
void AdvectNodePosition_HairStrands(
    in float NodeMass,
    in bool IsPositionMobile,
    in float3 ExternalForce,
    in float3 ForceGradient,
    in float DeltaTime,
    inout float3 OutLinearVelocity,
    inout float3 OutNodePosition);

// 方向积分（扭矩 -> 角速度 -> 方向）
void AdvectNodeOrientation_HairStrands(
    in float3 NodeInertia,
    in bool IsOrientationMobile,
    in float3 ExternalTorque,
    in float3 TorqueGradient,
    in float DeltaTime,
    inout float3 OutAngularVelocity,
    inout float4 OutNodeOrientation);
```

#### 约束求解函数

```hlsl
// 距离弹簧约束（拉伸）
void SetupDistanceSpringMaterial_HairStrands(...);
void SolveDistanceSpringMaterial_HairStrands(...);
void ProjectDistanceSpringMaterial_HairStrands(...);

// 角度弹簧约束（弯曲）
void SetupAngularSpringMaterial_HairStrands(...);
void SolveAngularSpringMaterial_HairStrands(...);
void ProjectAngularSpringMaterial_HairStrands(...);

// Cosserat Rod 约束
void SetupStretchRodMaterial_HairStrands(...);
void SolveStretchRodMaterial_HairStrands(...);
void ProjectStretchRodMaterial_HairStrands(...);

// 碰撞约束
void SolveHardCollisionConstraint_HairStrands(...);
void ProjectHardCollisionConstraint_HairStrands(...);
```

#### 写回函数

```hlsl
// 将节点位置写回 DeformedPositionBuffer
void UpdatePointPosition_HairStrands(
    in float3 NodePosition,
    in float3 RestPosition,
    out bool OutReportStatus);

// 重置到初始位置
void ResetPointPosition_HairStrands(out bool OutReportStatus);
```

---

## Niagara System 工作流程

### 1. Spawn Script

```hlsl
// 初始化粒子属性
void Spawn()
{
    // 获取节点初始位置
    float3 NodePosition;
    HairStrands.ComputeNodePosition(NodePosition);
    
    // 设置粒子位置
    Position = NodePosition;
    
    // 计算 Rest Position（用于绑定）
    float3 RestPosition;
    HairStrands.ComputeRestPosition(NodePosition, RestPosition);
    
    // 初始化物理属性
    float NodeMass;
    HairStrands.ComputeNodeMass(StrandsDensity, NodeThickness, NodeMass);
    
    // 初始化速度
    LinearVelocity = float3(0, 0, 0);
}
```

### 2. Update Script - 物理模拟循环

```hlsl
void Update()
{
    // Step 1: 应用外力（重力、风力等）
    float3 ExternalForce = GravityVector + AirDragForce;
    
    // Step 2: 位置积分（预测）
    HairStrands.AdvectNodePosition(
        NodeMass, IsPositionMobile, ExternalForce, ForceGradient,
        DeltaTime, LinearVelocity, Position);
    
    // Step 3: 约束求解（多次迭代）
    for(int i = 0; i < IterationCount; i++)
    {
        // 拉伸约束
        HairStrands.SolveDistanceSpringMaterial(...);
        
        // 弯曲约束
        HairStrands.SolveAngularSpringMaterial(...);
        
        // 碰撞约束
        HairStrands.SolveHardCollisionConstraint(...);
    }
    
    // Step 4: 约束投影（可选）
    HairStrands.ProjectDistanceSpringMaterial(...);
    HairStrands.ProjectAngularSpringMaterial(...);
    
    // Step 5: 更新速度
    HairStrands.UpdateLinearVelocity(PreviousPosition, Position, DeltaTime, LinearVelocity);
    
    // Step 6: 写回 Groom Buffer（用于渲染）
    bool Status;
    HairStrands.UpdatePointPosition(Position, RestPosition, Status);
}
```

---

## 性能优化

### 1. 线程组配置

```hlsl
// 每束头发使用一个线程组
#define NIAGARA_HAIR_STRANDS_THREAD_COUNT_INTERPOLATE 32

// 发束大小通常为 8-32 个节点
// 线程组大小与 StrandSize 匹配以实现高效共享内存访问
```

### 2. 共享内存使用

```hlsl
// 使用 groupshared 内存缓存节点数据
groupshared float3 SharedNodePosition[THREAD_COUNT];
groupshared float4 SharedNodeOrientation[THREAD_COUNT];
groupshared float SharedInverseMass[THREAD_COUNT];
groupshared float3 SharedInverseInertia[THREAD_COUNT];
groupshared float3 SharedPreviousPosition[THREAD_COUNT];
groupshared float4 SharedPreviousOrientation[THREAD_COUNT];
```

### 3. 迭代优化

```cpp
// CPU 端参数
int32 SubSteps = 5;          // 子步数
int32 IterationCount = 20;   // 每子步迭代次数

// 总模拟步数 = SubSteps * IterationCount
// 可以在运行时动态调整以平衡质量和性能
```

---

## 对破碎 DI 设计的启示

### 可借鉴的模式

| Groom Hair | Fracture DI | 应用方式 |
|-----------|-------------|----------|
| Rest/Deformed Position Buffer | Chunk Initial/State Buffer | 双缓冲设计 |
| Strands (发束) | Fracture Groups (破碎组) | 逻辑分组 |
| Nodes (节点) | Particles (粒子) | 物理模拟单元 |
| Curve Offsets | Chunk Data Array | 数据索引 |
| XPBD Constraints | Connection Constraints | 约束求解 |
| Red-Black Ordering | Connection Iteration | 并行优化 |
| Skinning Bind | Anchor Point | 绑定机制 |
| UpdatePointPosition | UpdateChunkTransform | 写回函数 |

### 建议的破碎 DI 架构

```hlsl
// 基于 Groom 模式的破碎 DI 设计

// Buffer 定义
RWStructuredBuffer<FChunkState> ChunkStateBuffer;       // UAV - 运行时状态
StructuredBuffer<FChunkData> ChunkDataBuffer;           // SRV - 初始数据
StructuredBuffer<FConnectionData> ConnectionBuffer;     // SRV - 连接数据
Buffer<uint> ChunkNeighborsBuffer;                      // SRV - 邻居索引

// 核心函数

// 1. 初始化
void ComputeChunkTransform(out float3 OutPosition, out float4 OutRotation);

// 2. 绑定
void AttachChunkPosition(in float3 RestPosition, out float3 OutPosition);

// 3. 积分
void AdvectChunkPosition(in float ChunkMass, in float3 ExternalForce, 
    in float DeltaTime, inout float3 OutVelocity, inout float3 OutPosition);

// 4. 约束求解
void SolveConnectionConstraint(in int ConnectionIndex, 
    in float RestLength, in float Stiffness, in float DeltaTime);

// 5. 写回
void UpdateChunkTransform(in float3 Position, in float4 Rotation, 
    out bool OutStatus);
```

---

## 参考文件

| 文件 | 描述 |
|------|------|
| `NiagaraDataInterfaceHairStrands.cpp` | C++ DI 实现 |
| `NiagaraDataInterfaceHairStrands.h` | 头文件定义 |
| `NiagaraDataInterfaceHairStrandsTemplate.ush` | HLSL 模板函数 |
| `NiagaraHookeSpringMaterial.ush` | 弹簧约束实现 |
| `NiagaraCosseratRodMaterial.ush` | Cosserat Rod 材料 |
| `NiagaraAngularSpringMaterial.ush` | 角度约束 |
| `NiagaraStaticCollisionConstraint.ush` | 碰撞约束 |

---

## 扩展阅读

- [XPBD: Position-Based Simulation of Compliant Constrained Dynamics](https://matthias-research.github.io/pages/publications/XPBD.pdf)
- [Cosserat Rods](https://en.wikipedia.org/wiki/Cosserat_rod_theory)
- [Hair Simulation in Computer Graphics](https://www.cs.cornell.edu/~srm/publications/EG-Hair-07.pdf)
