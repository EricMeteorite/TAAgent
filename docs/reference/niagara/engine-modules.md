# UE5.7 Niagara 引擎模块参考

本文档整理自本地 UE 源码目录中的 `Engine/Plugins/FX/Niagara/Content/Modules/`

模块路径格式：`/Niagara/Modules/<Category>/<SubCategory>/<ModuleName>`

---

## 目录

- [Spawn 模块](#spawn-模块) - 粒子生成阶段
- [Update 模块](#update-模块) - 每帧更新阶段
- [Emitter 模块](#emitter-模块) - 发射器控制
- [System 模块](#system-模块) - 系统级控制
- [Solvers 模块](#solvers-模块) - 物理求解器
- [Collision 模块](#collision-模块) - 碰撞检测
- [Events 模块](#events-模块) - 事件系统
- [Math 模块](#math-模块) - 数学工具
- [其他模块](#其他模块)

---

## Spawn 模块

粒子初始化阶段，设置粒子出生时的属性。

### Initialization (初始化)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| InitializeParticle | `/Niagara/Modules/Spawn/Initialization/InitializeParticle` | 基础粒子初始化 |
| InitializeParticle (V2) | `/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle` | V2 版本初始化 |
| InitializeRibbon | `/Niagara/Modules/Spawn/Initialization/InitializeRibbon` | Ribbon 初始化 |

### Location (位置)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| ShapeLocation (V2) | `/Niagara/Modules/Spawn/Location/V2/ShapeLocation` | **推荐** 通用形状位置 |
| SphereLocation (V2) | `/Niagara/Modules/Spawn/Location/V2/SphereLocation` | **推荐** 球形分布 |
| SkeletalMeshLocation (V2) | `/Niagara/Modules/Spawn/Location/V2/SkeletalMeshLocation` | **推荐** 骨骼网格位置 |
| GridLocation (V2) | `/Niagara/Modules/Spawn/Location/V2/GridLocation` | V2 网格位置 |
| RotateAroundPoint (V2) | `/Niagara/Modules/Spawn/Location/V2/RotateAroundPoint` | V2 绕点旋转 |
| BoxLocation | `/Niagara/Modules/Spawn/Location/BoxLocation` | 盒形分布 |
| ConeLocation | `/Niagara/Modules/Spawn/Location/ConeLocation` | 锥形分布 |
| CylinderLocation | `/Niagara/Modules/Spawn/Location/CylinderLocation` | 圆柱分布 |
| SphereLocation | `/Niagara/Modules/Spawn/Location/SphereLocation` | 球形分布 (旧版) |
| TorusLocation | `/Niagara/Modules/Spawn/Location/TorusLocation` | 环形分布 |
| GridLocation | `/Niagara/Modules/Spawn/Location/GridLocation` | 网格分布 |
| CurlNoiseLocation | `/Niagara/Modules/Spawn/Location/CurlNoiseLocation` | Curl Noise 位置 |
| SkeletalMeshSurfaceLocation | `/Niagara/Modules/Spawn/Location/SkeletalMeshSurfaceLocation` | 骨骼网格表面 |
| SkeletalMeshSkeletonLocation | `/Niagara/Modules/Spawn/Location/SkeletalMeshSkeletonLocation` | 骨骼位置 |
| SocketLocation | `/Niagara/Modules/Spawn/Location/SocketLocation` | Socket 位置 |
| StaticMeshLocation | `/Niagara/Modules/Spawn/Location/StaticMeshLocation` | 静态网格位置 |
| SystemLocation | `/Niagara/Modules/Spawn/Location/SystemLocation` | 系统位置 |

### Velocity (速度)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| AddVelocity | `/Niagara/Modules/Spawn/Velocity/AddVelocity` | **常用** 添加初始速度 |
| AddVelocityFromPoint | `/Niagara/Modules/Spawn/Velocity/AddVelocityFromPoint` | 从点向外发射 |
| AddVelocityInCone | `/Niagara/Modules/Spawn/Velocity/AddVelocityInCone` | 锥形速度 |
| LinearForce | `/Niagara/Modules/Spawn/Velocity/LinearForce` | 线性力 |
| PointForce | `/Niagara/Modules/Spawn/Velocity/PointForce` | 点力 |
| ScaleVelocity | `/Niagara/Modules/Spawn/Velocity/ScaleVelocity` | 缩放速度 |
| StaticMeshVelocity | `/Niagara/Modules/Spawn/Velocity/StaticMeshVelocity` | 继承静态网格速度 |

### Orientation (朝向)

```
/Niagara/Modules/Spawn/Orientation/
```

### MeshInterface

```
/Niagara/Modules/Spawn/MeshInterface/
```

---

## Update 模块

每帧执行的更新逻辑。

### Forces (力)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| CurlNoiseForce (V2) | `/Niagara/Modules/Update/Forces/V2/CurlNoiseForce` | **推荐** Curl Noise 力 |
| PointAttractionForce (V2) | `/Niagara/Modules/Update/Forces/V2/PointAttractionForce` | **推荐** 点吸引力 |
| GravityForce | `/Niagara/Modules/Update/Forces/GravityForce` | **常用** 重力 |
| Drag | `/Niagara/Modules/Update/Forces/Drag` | **常用** 阻力 |
| CurlNoiseForce | `/Niagara/Modules/Update/Forces/CurlNoiseForce` | Curl Noise 力 (旧版) |
| DragForce | `/Niagara/Modules/Update/Forces/DragForce` | 阻力 |
| AerodynamicDrag | `/Niagara/Modules/Update/Forces/AerodynamicDrag` | 空气阻力 |
| AccelerationForce | `/Niagara/Modules/Update/Forces/AccelerationForce` | 加速度力 |
| VortexForce | `/Niagara/Modules/Update/Forces/VortexForce` | 漩涡力 |
| WindForce | `/Niagara/Modules/Update/Forces/WindForce` | 风力 |
| SpringForce | `/Niagara/Modules/Update/Forces/SpringForce` | 弹簧力 |
| LineAttractionForce | `/Niagara/Modules/Update/Forces/LineAttractionForce` | 线吸引力 |
| PointAttractionForce | `/Niagara/Modules/Update/Forces/PointAttractionForce` | 点吸引力 (旧版) |
| VectorNoiseForce | `/Niagara/Modules/Update/Forces/VectorNoiseForce` | 向量噪声力 |
| LimitForce | `/Niagara/Modules/Update/Forces/LimitForce` | 限制力 |
| CalculateMassByVolume | `/Niagara/Modules/Update/Forces/CalculateMassByVolume` | 按体积计算质量 |
| FindKineticAndPotentialEnergy | `/Niagara/Modules/Update/Forces/FindKineticAndPotentialEnergy` | 计算动能势能 |

### Color (颜色)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| ScaleColor | `/Niagara/Modules/Update/Color/ScaleColor` | **常用** 缩放颜色 |
| Color | `/Niagara/Modules/Update/Color/Color` | 设置颜色 |
| ScaleColorBySpeed | `/Niagara/Modules/Update/Color/ScaleColorBySpeed` | 按速度缩放颜色 |
| ScaleColorByVelocity | `/Niagara/Modules/Update/Color/ScaleColorByVelocity` | 按速度向量缩放 |

### Size (大小)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| ScaleSpriteSize | `/Niagara/Modules/Update/Size/ScaleSpriteSize` | **常用** 缩放精灵大小 |
| ScaleMeshSize | `/Niagara/Modules/Update/Size/ScaleMeshSize` | 缩放网格大小 |
| SpriteSizeScale | `/Niagara/Modules/Update/Size/SpriteSizeScale` | 精灵大小缩放 |
| MeshSizeScale | `/Niagara/Modules/Update/Size/MeshSizeScale` | 网格大小缩放 |
| ScaleSpriteSizeBySpeed | `/Niagara/Modules/Update/Size/ScaleSpriteSizeBySpeed` | 按速度缩放精灵 |
| ScaleMeshSizeBySpeed | `/Niagara/Modules/Update/Size/ScaleMeshSizeBySpeed` | 按速度缩放网格 |
| SpriteSizeScaleBySpeed | `/Niagara/Modules/Update/Size/SpriteSizeScaleBySpeed` | 按速度缩放 |
| SpriteSizeScaleByVelocity | `/Niagara/Modules/Update/Size/SpriteSizeScaleByVelocity` | 按速度向量缩放 |

### Lifetime (生命周期)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| ParticleState | `/Niagara/Modules/Update/Lifetime/ParticleState` | **常用** 粒子状态管理 |
| UpdateAge | `/Niagara/Modules/Update/Lifetime/UpdateAge` | 更新年龄 |
| KillParticles | `/Niagara/Modules/Update/Lifetime/KillParticles` | 杀死粒子 |
| KillParticlesInVolume | `/Niagara/Modules/Update/Lifetime/KillParticlesInVolume` | 体积内杀死 |
| FirstFrame | `/Niagara/Modules/Update/Lifetime/FirstFrame` | 首帧检测 |

### Orientation (朝向)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| SpriteRotationRate | `/Niagara/Modules/Update/Orientation/SpriteRotationRate` | 精灵旋转速率 |
| MeshRotationRate | `/Niagara/Modules/Update/Orientation/MeshRotationRate` | 网格旋转速率 |
| MeshRotationForce | `/Niagara/Modules/Update/Orientation/MeshRotationForce` | 网格旋转力 |
| GenerateTorque | `/Niagara/Modules/Update/Orientation/GenerateTorque` | 生成扭矩 |
| GenerateMeshTorque | `/Niagara/Modules/Update/Orientation/GenerateMeshTorque` | 网格扭矩 |
| AlignSpriteToMeshOrientation | `/Niagara/Modules/Update/Orientation/AlignSpriteToMeshOrientation` | 对齐网格朝向 |
| MeshLookAt | `/Niagara/Modules/Update/Orientation/MeshLookAt` | 网格朝向 |
| OrientMeshToVector | `/Niagara/Modules/Update/Orientation/OrientMeshToVector` | 网格对齐向量 |
| InitialMeshRotationRate | `/Niagara/Modules/Update/Orientation/InitialMeshRotationRate` | 初始旋转速率 |

### Position (位置)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| OffsetPosition | `/Niagara/Modules/Update/Position/OffsetPosition` | 偏移位置 |
| JitterPosition | `/Niagara/Modules/Update/Position/JitterPosition` | 抖动位置 |
| InheritSourceMovement | `/Niagara/Modules/Update/Position/InheritSourceMovement` | 继承源运动 |
| ConstrainPositionToPlane | `/Niagara/Modules/Update/Position/ConstrainPositionToPlane` | 约束到平面 |
| RecreateCameraProjection | `/Niagara/Modules/Update/Position/RecreateCameraProjection` | 相机投影 |

### Velocity (速度)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| ScaleVelocity | `/Niagara/Modules/Update/Velocity/ScaleVelocity` | 缩放速度 |
| VortexVelocity | `/Niagara/Modules/Update/Velocity/VortexVelocity` | 漩涡速度 |
| InheritVelocity | `/Niagara/Modules/Update/Velocity/InheritVelocity` | 继承速度 |
| AlignVelocityToRandomAxis | `/Niagara/Modules/Update/Velocity/AlignVelocityToRandomAxis` | 对齐随机轴 |

### Utility (工具)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| CurlNoise | `/Niagara/Modules/Update/Utility/CurlNoise` | Curl Noise |
| VectorNoiseValue | `/Niagara/Modules/Update/Utility/VectorNoiseValue` | 向量噪声值 |
| DebugDraw | `/Niagara/Modules/Update/Utility/DebugDraw` | 调试绘制 |
| FrameCounter | `/Niagara/Modules/Update/Utility/FrameCounter` | 帧计数器 |
| EmitterFrameCounter | `/Niagara/Modules/Update/Utility/EmitterFrameCounter` | 发射器帧计数 |
| Timeline | `/Niagara/Modules/Update/Utility/Timeline` | 时间轴 |
| DoOnce | `/Niagara/Modules/Update/Utility/DoOnce` | 执行一次 |
| IncrementOverTime | `/Niagara/Modules/Update/Utility/IncrementOverTime` | 随时间递增 |
| InterpolateOverTime | `/Niagara/Modules/Update/Utility/InterpolateOverTime` | 随时间插值 |
| LerpParticleAttributes | `/Niagara/Modules/Update/Utility/LerpParticleAttributes` | 粒子属性插值 |
| TemporalLerp_Float | `/Niagara/Modules/Update/Utility/TemporalLerp_Float` | 时间插值 Float |
| TemporalLerp_Vector | `/Niagara/Modules/Update/Utility/TemporalLerp_Vector` | 时间插值 Vector |
| PartitionParticles | `/Niagara/Modules/Update/Utility/PartitionParticles` | 分区粒子 |
| AsyncGPUTrace | `/Niagara/Modules/Update/Utility/AsyncGPUTrace` | 异步 GPU 追踪 |
| QueryGBuffer | `/Niagara/Modules/Update/Utility/QueryGBuffer` | 查询 GBuffer |
| SampleGBuffer | `/Niagara/Modules/Update/Utility/SampleGBuffer` | 采样 GBuffer |
| ApplyOwnerScaleToAttributes | `/Niagara/Modules/Update/Utility/ApplyOwnerScaleToAttributes` | 应用 Owner 缩放 |

### StateChange (状态切换)

| 模块名 | 路径 | 说明 |
|--------|------|------|
| TimeBasedStateMachine | `/Niagara/Modules/Update/StateChange/TimeBasedStateMachine` | 时间状态机 |

### 其他 Update 子目录

- `BlendSpace/` - BlendSpace 支持
- `Camera/` - 相机相关
- `Decal/` - 贴花相关
- `Material/` - 材质相关
- `Neighbor/` - 邻居粒子
- `Renderers/` - 渲染器相关
- `Splines/` - 样条相关
- `SubUV/` - SubUV 动画
- `Textures/` - 纹理相关

---

## Emitter 模块

控制发射器行为的模块。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| SpawnRate | `/Niagara/Modules/Emitter/SpawnRate` | **常用** 持续发射率 |
| SpawnPerFrame | `/Niagara/Modules/Emitter/SpawnPerFrame` | 每帧发射数量 |
| SpawnPerUnit | `/Niagara/Modules/Emitter/SpawnPerUnit` | 每单位发射 |
| SpawnBurst_Instantaneous | `/Niagara/Modules/Emitter/SpawnBurst_Instantaneous` | 瞬发爆发 |
| EmitterState | `/Niagara/Modules/Emitter/EmitterState` | 发射器状态 |
| EmitterLifeCycle | `/Niagara/Modules/Emitter/EmitterLifeCycle` | 发射器生命周期 |
| EmitterBounds | `/Niagara/Modules/Emitter/EmitterBounds` | 发射器边界 |
| SpawnParticlesFromSimCache | `/Niagara/Modules/Emitter/SpawnParticlesFromSimCache` | 从 SimCache 生成 |

---

## System 模块

系统级控制。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| SystemState | `/Niagara/Modules/System/SystemState` | 系统状态 |
| SystemLifeCycle | `/Niagara/Modules/System/SystemLifeCycle` | 系统生命周期 |
| CompleteIfUnused | `/Niagara/Modules/System/CompleteIfUnused` | 未使用时完成 |

---

## Solvers 模块

物理求解器，通常放在 Update 脚本末尾。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| SolveForcesAndVelocity | `/Niagara/Modules/Solvers/SolveForcesAndVelocity` | **必需** 力与速度求解 |
| SolveRotationalForcesAndVelocity | `/Niagara/Modules/Solvers/SolveRotationalForcesAndVelocity` | 旋转力求解 |
| ApplyInitialForces | `/Niagara/Modules/Solvers/ApplyInitialForces` | 应用初始力 |
| ApplyRotationVector | `/Niagara/Modules/Solvers/ApplyRotationVector` | 应用旋转向量 |

---

## Collision 模块

碰撞检测与响应。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| Collision | `/Niagara/Modules/Collision/Collision` | **常用** 通用碰撞 |
| CollisionQuery | `/Niagara/Modules/Collision/CollisionQuery` | 碰撞查询 |
| CollisionQueryAndResponse | `/Niagara/Modules/Collision/CollisionQueryAndResponse` | 查询与响应 |
| CollisionRest | `/Niagara/Modules/Collision/CollisionRest` | 碰撞静止 |
| CollisionLinearImpulse | `/Niagara/Modules/Collision/CollisionLinearImpulse` | 线性冲量 |
| RayTrace | `/Niagara/Modules/Collision/RayTrace` | 射线检测 |
| SceneDepthTest | `/Niagara/Modules/Collision/SceneDepthTest` | 场景深度测试 |
| NiagaraDistanceFieldCollisions | `/Niagara/Modules/Collision/NiagaraDistanceFieldCollisions` | Distance Field 碰撞 |
| AnalyticalCollisionQuery | `/Niagara/Modules/Collision/AnalyticalCollisionQuery` | 解析碰撞查询 |
| PBD_IntraParticleCollision | `/Niagara/Modules/Collision/PBD_IntraParticleCollision` | PBD 粒子间碰撞 |
| InitializeNeighborGrid | `/Niagara/Modules/Collision/InitializeNeighborGrid` | 初始化邻居网格 |
| PopulateNeighborGrid | `/Niagara/Modules/Collision/PopulateNeighborGrid` | 填充邻居网格 |

---

## Events 模块

事件生成与接收。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| GenerateCollisionEvent | `/Niagara/Modules/Events/GenerateCollisionEvent` | 生成碰撞事件 |
| GenerateDeathEvent | `/Niagara/Modules/Events/GenerateDeathEvent` | 生成死亡事件 |
| GenerateLocationEvent | `/Niagara/Modules/Events/GenerateLocationEvent` | 生成位置事件 |
| ReceiveCollisionEvent | `/Niagara/Modules/Events/ReceiveCollisionEvent` | 接收碰撞事件 |
| ReceiveDeathEvent | `/Niagara/Modules/Events/ReceiveDeathEvent` | 接收死亡事件 |
| ReceiveLocationEvent | `/Niagara/Modules/Events/ReceiveLocationEvent` | 接收位置事件 |

---

## Math 模块

数学工具函数。

| 模块名 | 路径 | 说明 |
|--------|------|------|
| FadeOverTime | `/Niagara/Modules/Math/FadeOverTime` | 随时间淡出 |
| TrackDistanceTraveled | `/Niagara/Modules/Math/TrackDistanceTraveled` | 追踪移动距离 |
| FindScreenSpaceBoundingBox | `/Niagara/Modules/Math/FindScreenSpaceBoundingBox` | 屏幕空间边界框 |
| AvoidCone | `/Niagara/Modules/Math/AvoidCone` | 避开锥形区域 |
| ConeSphereIntersection | `/Niagara/Modules/Math/ConeSphereIntersection` | 锥球相交 |
| SpherePlaneIntersection | `/Niagara/Modules/Math/SpherePlaneIntersection` | 球面相交 |
| SlerpVector | `/Niagara/Modules/Math/SlerpVector` | 向量球面插值 |
| SolveFloatSpringConstraint | `/Niagara/Modules/Math/SolveFloatSpringConstraint` | 弹簧约束求解 |
| FindClosestPointOnLineSegment | `/Niagara/Modules/Math/FindClosestPointOnLineSegment` | 线段最近点 |
| FindClosestPointOnTriangle | `/Niagara/Modules/Math/FindClosestPointOnTriangle` | 三角形最近点 |
| ConstrainVectorToCone | `/Niagara/Modules/Math/ConstrainVectorToCone` | 约束向量到锥形 |
| Flight_Orientation | `/Niagara/Modules/Math/Flight_Orientation` | 飞行朝向 |
| PureRoll_Orientation | `/Niagara/Modules/Math/PureRoll_Orientation` | 纯滚动朝向 |
| MatchVelocity_ViaForce | `/Niagara/Modules/Math/MatchVelocity_ViaForce` | 通过力匹配速度 |

---

## 其他模块

### Light

```
/Niagara/Modules/Light/
└── Light_Attributes.uasset  # 灯光属性
```

### Mesh

```
/Niagara/Modules/Mesh/
├── TraverseSkeletalMesh.uasset
├── PrepareTraverseSkeletalMesh.uasset
├── InitializeTraverseSkeletalMesh.uasset
└── ApplyTraverseSkeletalMeshUpdate.uasset
```

### Beams

```
/Niagara/Modules/Beams/
├── SpawnBeam.uasset          # 生成光束
├── UpdateBeam.uasset         # 更新光束
├── BeamEmitterSetup.uasset   # 光束发射器设置
├── BeamWidth.uasset          # 光束宽度
├── BeamWidthScale.uasset     # 宽度缩放
└── ScaleBeamWidth.uasset     # 缩放光束宽度
```

### Ribbons

```
/Niagara/Modules/Ribbons/
├── FN_InitializeRibbonAttributes.uasset
├── RibbonWidth.uasset
├── RibbonWidthScale.uasset
└── ScaleRibbonWidth.uasset
```

### Arrays

```
/Niagara/Modules/Arrays/  # 数组操作
```

### Audio

```
/Niagara/Modules/Audio/  # 音频相关
```

### Debug

```
/Niagara/Modules/Debug/  # 调试工具
```

### Decal

```
/Niagara/Modules/Decal/  # 贴花相关
```

### Landscape

```
/Niagara/Modules/Landscape/  # 地形交互
```

### Constraints

```
/Niagara/Modules/Constraints/  # 约束
```

### PostSolvers

```
/Niagara/Modules/PostSolvers/  # 后处理求解器
```

### RVT (Runtime Virtual Texture)

```
/Niagara/Modules/RVT/  # RVT 交互
```

### Scalability

```
/Niagara/Modules/Scalability/  # 可扩展性设置
```

### Masks

```
/Niagara/Modules/Masks/  # 遮罩
```

### AttributeReader

```
/Niagara/Modules/AttributeReader/  # 属性读取器
```

### ExportParticleData

```
/Niagara/Modules/ExportParticleData/  # 导出粒子数据
```

---

## 常用模块组合

### 基础粒子效果

```
Spawn Script:
  1. InitializeParticle (V2)
  2. ShapeLocation (V2)
  3. AddVelocity

Update Script:
  1. ParticleState
  2. GravityForce
  3. Drag
  4. ScaleColor (按 NormalizedAge)
  5. ScaleSpriteSize (按 NormalizedAge)
  6. SolveForcesAndVelocity
```

### 火焰效果

```
Spawn Script:
  1. InitializeParticle (V2)
  2. ShapeLocation (V2) - Sphere, small radius
  3. AddVelocity - upward

Update Script:
  1. ParticleState
  2. CurlNoiseForce (V2) - turbulence
  3. Drag
  4. ScaleColor (fire gradient curve)
  5. ScaleSpriteSize (grow then shrink)
  6. SolveForcesAndVelocity
```

### 烟雾效果

```
Spawn Script:
  1. InitializeParticle (V2)
  2. ShapeLocation (V2)
  3. AddVelocity - slow upward

Update Script:
  1. ParticleState
  2. CurlNoiseForce (V2) - high turbulence
  3. Drag - high
  4. ScaleColor (fade out)
  5. ScaleSpriteSize (expand over time)
  6. SolveForcesAndVelocity
```

---

## V2 版本说明

UE5 推荐使用 V2 版本的模块，这些模块：
- API 更稳定
- 参数命名更清晰
- 性能优化更好
- 支持 Stateless 模式转换

优先使用：
- `InitializeParticle (V2)` 而非 `InitializeParticle`
- `ShapeLocation (V2)` 而非 `SphereLocation`/`BoxLocation` 等
- `CurlNoiseForce (V2)` 而非 `CurlNoiseForce`
