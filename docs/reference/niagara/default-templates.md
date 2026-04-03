# Niagara 默认模板

UE5.7 引擎内置的 Niagara 模板资源路径。

## Emitter 模板

**路径前缀**: `/Niagara/DefaultAssets/Templates/Emitters/`

**完整引用格式**: `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/<Name>.<Name>'`

例如: `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain'`

| 模板名 | 简短路径 | 完整引用路径 |
|--------|----------|--------------|
| Minimal | `/Niagara/DefaultAssets/Templates/Emitters/Minimal` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal'` |
| Fountain | `/Niagara/DefaultAssets/Templates/Emitters/Fountain` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain'` |
| OmnidirectionalBurst | `/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst'` |
| DirectionalBurst | `/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst.DirectionalBurst'` |
| SimpleSpriteBurst | `/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst'` |
| ConfettiBurst | `/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst.ConfettiBurst'` |
| UpwardMeshBurst | `/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst.UpwardMeshBurst'` |
| BlowingParticles | `/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles.BlowingParticles'` |
| HangingParticulates | `/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates.HangingParticulates'` |
| SingleLoopingParticle | `/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle.SingleLoopingParticle'` |
| RecycleParticlesInView | `/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView.RecycleParticlesInView'` |
| DynamicBeam | `/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam.DynamicBeam'` |
| StaticBeam | `/Niagara/DefaultAssets/Templates/Emitters/StaticBeam` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/StaticBeam.StaticBeam'` |
| LocationBasedRibbon | `/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon` | `/Script/Niagara.NiagaraEmitter'/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon'` |

## System 模板

**路径前缀**: `/Niagara/DefaultAssets/Templates/Systems/`

| 模板名 | 简短路径 | 用途 |
|--------|----------|------|
| SimpleExplosion | `/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion` | 简单爆炸效果 |
| RadialBurst | `/Niagara/DefaultAssets/Templates/Systems/RadialBurst` | 径向爆发 |
| DirectionalBurst | `/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst` | 定向爆发系统 |
| DirectionalBurstLightweight | `/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight` | 轻量级定向爆发 |
| FountainLightweight | `/Niagara/DefaultAssets/Templates/Systems/FountainLightweight` | 轻量级喷泉 |
| MinimalLightweight | `/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight` | 轻量级最小模板 |
| AttributeReaderTrails | `/Niagara/DefaultAssets/Templates/Systems/AttributeReaderTrails` | 属性读取拖尾 |

## 使用方式

### 创建 Niagara System (自动初始化)

```python
create_asset(
    asset_type="NiagaraSystem",
    name="NS_MyEffect",
    path="/Game/Effects/"
)
```

> 注意: `create_asset` 会自动调用 `InitializeSystem` 初始化 NiagaraSystem 的 EditorData。

### 向现有 System 添加 Emitter

```python
update_niagara_emitter(
    asset_path="/Game/Effects/NS_MyEffect",
    operations=[
        {
            "target": "emitter",
            "name": "NewEmitter",
            "action": "add",
            "template": "/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst"
        }
    ]
)
```

## Behavior Examples

路径前缀: `/Niagara/DefaultAssets/Templates/BehaviorExamples/`

用于学习特定技术实现的示例：

- GridLocation - 网格位置
- InfiniteParticleLifetime - 无限粒子生命周期
- InterpolateValueOverTime - 随时间插值
- KillParticles - 杀死粒子
- MeshArrays - 网格数组
- MeshOrientation - 网格朝向
- MeshRotationForce - 网格旋转力
- NormalizedExecutionIndex - 归一化执行索引
- ParticlesUniqueID - 粒子唯一ID
- PendulumConstraint - 钟摆约束
- PlayAudio - 播放音频
- RendererBindingOverrides - 渲染器绑定覆盖
- RenderersWithNoParticles - 无粒子渲染器
- RendererVisibility - 渲染器可见性
- RenderTargetTexturePainter - 渲染目标纹理绘制
- RibbonID - 飘带ID
- RibbonLinkOrder - 飘带链接顺序
- RibbonShapes - 飘带形状
- SpawnGroups - 生成组
- SpawnGroupsFromMultipleBones - 多骨骼生成组
- SpawnInterpolation - 生成插值
- SpriteFacingAndAlignment - 精灵朝向与对齐
- SubUVAnimation - SubUV动画
- WaveForms - 波形
- WorkingWithAttributes - 使用属性
