# Niagara 常用模块详解

> 本文档整理自 UE5.7 引擎 Niagara 默认模板系统和模块源码。

---

## 核心模块 (所有模板必用)

### InitializeParticle

**路径**: `/Niagara/Modules/Spawn/Initialization/InitializeParticle`

**用途**: 初始化粒子常用属性。未勾选的属性将传递之前的值。

**参数**:

| 参数名 | 类型 | 说明 |
|--------|------|------|
| `Module.Lifetime` | float | 粒子生命周期 (秒) |
| `Module.Color` | LinearColor | 粒子颜色 (RGBA) |
| `Module.Mass` | float | 粒子质量 |
| `Module.Sprite Size` | Vector2f | Sprite 尺寸 (宽, 高) |
| `Module.Sprite Rotation` | float | Sprite 旋转角度 (度) |
| `Module.Mesh Scale` | float | Mesh 缩放 |
| `Module.Position` | Vector3f | 初始位置偏移 |
| `Module.Ribbon Width` | float | Ribbon 宽度 |
| `Module.Material Random` | float | 材质随机值 (0-1) |

**写入属性**:
- `Particles.Lifetime`
- `Particles.Color`
- `Particles.Mass`
- `Particles.SpriteSize`
- `Particles.SpriteRotation`
- `Particles.Scale`
- `Particles.Position`
- `Particles.MaterialRandom`

**内部实现**:
- 使用 `If` 节点判断是否写入 (`Module.Write xxx` 参数)
- 使用 `SimulationPosition` 函数获取默认位置
- 使用 `RandomRangeFloat` 函数生成材质随机值

---

### SolveForcesAndVelocity

**路径**: `/Niagara/Modules/Solvers/SolveForcesAndVelocity`

**用途**: 解算物理力和速度，更新粒子位置。**必须在所有力模块之后调用**。

**参数**:

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `Module.Speed Limit` | float | 1000 | 最大速度限制 (cm/s) |
| `Module.Acceleration Limit` | float | 9999 | 最大加速度限制 |
| `Module.Rotational Force` | Vector3f | (0,0,0) | 旋转力 |
| `Module.Rotational Velocity` | Vector3f | (0,0,0) | 旋转速度 |
| `Module.Mesh Orientation` | Quat4f | identity | Mesh 朝向四元数 |
| `Module.Manually Enable Rotational Solver` | bool | false | 手动启用旋转解算 |

**读取属性**:
- `Transient.PhysicsForce` - 累积的物理力
- `Transient.PhysicsDrag` - 累积的阻力
- `Particles.Mass` - 粒子质量
- `Particles.Velocity` - 当前速度

**写入属性**:
- `Particles.Velocity` - 更新后的速度
- `Particles.Position` - 更新后的位置
- `Particles.MeshOrientation` - 更新后的朝向
- `Particles.RotationalVelocity` - 更新后的旋转速度
- `Particles.DistanceTraveled` - 累计移动距离

**内部实现**:
- 计算逆质量 = 1 / max(Mass, ε)
- 应用速度限制: `Velocity = Direction * clamp(Length, 0, SpeedLimit)`
- 应用加速度限制
- 应用阻力: 调用 `DragVelocity` 函数
- 位置更新: `Position += Velocity * DeltaTime`
- 旋转解算: 调用 `ApplyRotationVector` 函数

---

## Spawn 阶段模块

### AddVelocity

**路径**: `/Niagara/Modules/Spawn/Velocity/AddVelocity`

**用途**: 为粒子添加初始速度。支持三种模式：线性、从点、锥形。

**参数**:

| 参数名 | 类型 | 说明 |
|--------|------|------|
| `Module.Velocity` | Vector3f | 速度向量 |
| `Module.Velocity Speed Scale` | float | 速度缩放因子 |
| `Module.Velocity Origin` | Position | 速度原点 (从点模式) |
| `Module.Origin Offset` | Vector3f | 原点偏移 |
| `Module.Cone Angle` | float | 锥形角度 (度) |
| `Module.Cone Axis` | Vector3f | 锥形轴向 |
| `Module.Inner Cone Angle` | float | 内锥角度 |
| `Module.Distribution Along Cone Axis` | float | 沿锥轴分布 |
| `Module.Speed Falloff From Cone Axis` | float | 锥轴速度衰减 |
| `Module.Random Seed` | int | 随机种子 |
| `Module.Rotation Angle` | float | 旋转角度 |
| `Module.Rotation Axis` | Vector3f | 旋转轴 |
| `Module.Rotation Quaternion` | Quat4f | 旋转四元数 |

**模式选择** (静态开关):
- **Linear**: 使用 `Velocity * VelocitySpeedScale`
- **From Point**: 从 `VelocityOrigin` 指向粒子位置
- **In Cone**: 在锥形范围内随机方向

**写入属性**:
- `Particles.Velocity`

**调用的函数**:
- `CalculateRandomPointInConeSphereIntersection` - 锥形随机点
- `CalculateRandomUnitVector` - 随机单位向量
- `DistanceBasedFalloff` - 距离衰减
- `TransformStack_Rotation` - 旋转变换

---

## Update 阶段模块

### GravityForce

**路径**: `/Niagara/Modules/Update/Forces/GravityForce`

**用途**: 施加重力 (cm/s²)。

**参数**:

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `Module.Gravity` | Vector3f | (0,0,-980) | 重力向量 |

**读取属性**:
- `Particles.Mass` - 用于计算力 = Gravity × Mass

**写入属性**:
- `Transient.PhysicsForce` - 累加重力到物理力

**内部实现**:
```
PhysicsForce += Gravity × Mass
```
- 支持不同坐标空间 (Simulation/World/Local)
- 自动变换到正确的坐标空间

---

### Drag

**路径**: `/Niagara/Modules/Update/Forces/Drag`

**用途**: 施加阻力，直接作用于速度，不受质量影响。

**参数**:

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `Module.Drag` | float | 0 | 阻力系数 |
| `Module.Rotational Drag` | float | 0 | 旋转阻力系数 |
| `Module.Ignore Mass` | bool | false | 忽略质量 (默认不忽略) |

**写入属性**:
- `Transient.PhysicsDrag` - 累加阻力
- `Transient.PhysicsRotationalDrag` - 累加旋转阻力
- `Transient.DragIgnoreMass` - 是否忽略质量

**内部实现**:
```
PhysicsDrag += Drag
PhysicsRotationalDrag += RotationalDrag × 0.01  // cm to m
```

---

### ScaleColor

**路径**: `/Niagara/Modules/Update/Color/ScaleColor`

**用途**: 缩放粒子颜色 (RGB 和 Alpha 分开控制)。

**参数**:

| 参数名 | 类型 | 说明 |
|--------|------|------|
| `Module.Color Value To Scale` | LinearColor | 要缩放的颜色值 |
| `Module.Scale RGB` | Vector3f | RGB 缩放因子 |
| `Module.Scale Alpha` | float | Alpha 缩放因子 |
| `Module.Scale RGBA` | Vector4f | RGBA 统一缩放因子 |
| `Module.Linear Color Curve` | ColorCurve | 颜色曲线资源 |
| `Module.Curve Index` | float | 曲线索引 |
| `Module.Color Scale` | LinearColor | 颜色缩放值 |

**读取属性**:
- `Particles.Initial.Color` - 初始颜色
- `Particles.NormalizedAge` - 归一化年龄 (用于曲线采样)
- `Emitter.NormalizedLoopAge` - 发射器循环年龄
- `System.NormalizedLoopAge` - 系统循环年龄

**写入属性**:
- `Particles.Color` - 更新后的颜色
- `Transient.ParticleColorScaleFactor` - 颜色缩放因子

**内部实现**:
- 支持四种缩放模式 (静态开关):
  1. RGB + Alpha 分开缩放
  2. RGBA 统一缩放
  3. 使用 ColorCurve 曲线
  4. 使用 Color Scale 值

---

## 事件模块

### GenerateLocationEvent

**用途**: 生成位置事件，供其他发射器接收。

**参数**:

| 参数名 | 类型 | 说明 |
|--------|------|------|
| `Module.Event Send Rate` | float | 事件发送频率 (个/秒) |
| `Module.Event Probability` | float | 事件概率 (0-1) |
| `Module.Delay Before Sending Events` | float | 发送前延迟 (秒) |
| `Module.Delay Age Attribute` | float | 延迟年龄属性 |
| `Module.Vector to Send as Vector 1 (Position)` | Position | 位置数据 |
| `Module.Vector to Send as Vector 2 (Velocity)` | Vector3f | 速度数据 |
| `Module.Color to Send as Linear Color (ParticleColor)` | LinearColor | 颜色数据 |

---

### ReceiveLocationEvent

**用途**: 接收位置事件，创建跟随粒子。

**参数**:

| 参数名 | 类型 | 说明 |
|--------|------|------|
| `Module.Inherited Velocity Scale` | float | 继承速度缩放 |

---

## 常用模块组合模式

### 基础粒子喷射 (Burst)

```yaml
Spawn Script:
  1. InitializeParticle      # 初始化属性
  2. ShapeLocation           # 设置位置
  3. AddVelocity             # 添加速度

Update Script:
  1. GravityForce            # 重力
  2. Drag                    # 阻力
  3. ScaleColor              # 颜色变化
  4. SolveForcesAndVelocity  # 解算 (必须最后)
```

### Ribbon 拖尾效果

```yaml
Leader Emitter:
  Spawn: InitializeParticle + ShapeLocation + AddVelocity
  Update: GravityForce + Drag + GenerateLocationEvent + SolveForcesAndVelocity

Follower Emitter (Ribbon):
  Event Handler: ReceiveLocationEvent
  Spawn: InitializeParticle
  Update: SolveForcesAndVelocity
  Renderer: NiagaraRibbonRenderer
```

### 爆炸效果 (Explosion)

```yaml
Spawn Script:
  1. InitializeParticle
  2. ShapeLocation (Sphere)  # 球形分布
  3. AddVelocity (Omnidirectional)  # 全向速度

Update Script:
  1. GravityForce
  2. Drag
  3. ScaleColor (淡出)
  4. FloatFromCurve (尺寸变化)
  5. SolveForcesAndVelocity
```

---

## 模块参数命名规范

参数名格式: `Constants.{EmitterName}.{ModuleName}.{ParameterName}`

示例:
- `Constants.OmnidirectionalBurst.InitializeParticle.Lifetime Max`
- `Constants.OmnidirectionalBurst.GravityForce.Gravity`
- `Constants.OmnidirectionalBurst.Drag.Drag`

---

## 注意事项

1. **SolveForcesAndVelocity 必须在所有力模块之后调用**
2. **重力默认值 -980 cm/s² (UE 单位)**
3. **阻力不受质量影响，但会被 SolveForcesAndVelocity 使用**
4. **InitializeParticle 中未勾选的属性会传递已有值**
5. **V2 版本模块是更新版本，推荐优先使用**
