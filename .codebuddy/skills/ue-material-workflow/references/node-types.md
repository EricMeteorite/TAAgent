# UE 材质节点类型参考

## 输入参数节点

### ScalarParameter

单个浮点参数，可在材质实例中调整。

```python
{
    "type": "ScalarParameter",
    "name": "Roughness",
    "properties": {
        "DefaultValue": 0.5,
        "SliderMin": 0.0,
        "SliderMax": 1.0
    },
    "position": [-400, 0]
}
```

### VectorParameter

颜色或向量参数。

```python
{
    "type": "VectorParameter",
    "name": "BaseColor",
    "properties": {
        "DefaultValue": [0.5, 0.5, 0.5, 1.0]  # RGBA
    },
    "position": [-400, 100]
}
```

### TextureSampleParameter2D

可参数化的 2D 纹理采样。

```python
{
    "type": "TextureSampleParameter2D",
    "name": "DiffuseMap",
    "properties": {
        "Texture": "/Game/Textures/T_Diffuse"
    },
    "position": [-600, 0]
}
```

输出引脚: `RGB`, `R`, `G`, `B`, `A`

### TextureSampleParameterCube

立方体贴图参数。

```python
{
    "type": "TextureSampleParameterCube",
    "name": "EnvironmentMap",
    "properties": {
        "Texture": "/Game/Textures/T_EnvCube"
    }
}
```

### StaticSwitchParameter

静态开关，用于编译时分支。

```python
{
    "type": "StaticSwitchParameter",
    "name": "UseNormalMap",
    "properties": {
        "DefaultValue": True
    }
}
```

## 常量节点

### Constant

单个常量值。

```python
{"type": "Constant", "properties": {"R": 0.5}}
```

### Constant2Vector

2D 向量常量。

```python
{"type": "Constant2Vector", "properties": {"R": 1.0, "G": 0.0}}
```

### Constant3Vector

3D 向量常量。

```python
{"type": "Constant3Vector", "properties": {"R": 1.0, "G": 0.5, "B": 0.0}}
```

### Constant4Vector

4D 向量常量（颜色）。

```python
{"type": "Constant4Vector", "properties": {"R": 1.0, "G": 1.0, "B": 1.0, "A": 1.0}}
```

## 数学运算节点

### Multiply

乘法运算。

```python
{
    "type": "Multiply",
    "position": [-200, 0]
    # 输入: A, B
    # 输出: Result = A × B
}
```

### Add

加法运算。

```python
{
    "type": "Add",
    # 输入: A, B
    # 输出: Result = A + B
}
```

### Subtract

减法运算。

```python
{
    "type": "Subtract",
    # 输入: A, B
    # 输出: Result = A - B
}
```

### Divide

除法运算。

```python
{
    "type": "Divide",
    # 输入: A, B
    # 输出: Result = A / B
}
```

### Lerp

线性插值。

```python
{
    "type": "Lerp",
    # 输入: A, B, Alpha
    # 输出: Result = A + (B - A) * Alpha
}
```

### Clamp

值范围限制。

```python
{
    "type": "Clamp",
    # 输入: Input, Min, Max
    # 输出: Result = clamp(Input, Min, Max)
}
```

### Saturate

限制到 [0, 1] 范围。

```python
{
    "type": "Saturate",
    # 输入: Input
    # 输出: Result = clamp(Input, 0, 1)
}
```

### Power

幂运算。

```python
{
    "type": "Power",
    # 输入: Base, Exp
    # 输出: Result = Base ^ Exp
}
```

### Abs

绝对值。

```python
{"type": "Abs"}
```

### Floor / Ceil

向下/向上取整。

```python
{"type": "Floor"}  # 向下取整
{"type": "Ceil"}   # 向上取整
```

### Fmod

取模运算。

```python
{
    "type": "Fmod",
    # 输入: A, B
    # 输出: Result = A % B
}
```

## 纹理采样节点

### TextureSample

基础纹理采样。

```python
{
    "type": "TextureSample",
    "properties": {
        "Texture": "/Game/Textures/T_Normal",
        "SamplerSource": "SS_Wrap_WorldGroupSettings"
    },
    "position": [-600, 200]
}
```

输入引脚: `UVs`
输出引脚: `RGB`, `R`, `G`, `B`, `A`

### TextureObject

纹理对象引用（不采样）。

```python
{
    "type": "TextureObject",
    "properties": {
        "Texture": "/Game/Textures/T_Diffuse"
    }
}
```

## 坐标与几何节点

### TextureCoordinate

纹理坐标。

```python
{
    "type": "TextureCoordinate",
    "properties": {
        "CoordinateIndex": 0,
        "UTiling": 1.0,
        "VTiling": 1.0
    }
}
```

### WorldPosition

世界坐标。

```python
{"type": "WorldPosition"}
# 输出: 世界空间位置 (float3)
```

### ObjectPosition

物体原点位置。

```python
{"type": "ObjectPosition"}
```

### ActorPosition

Actor 位置。

```python
{"type": "ActorPosition"}
```

### CameraPosition

相机位置。

```python
{"type": "CameraPosition"}
```

### ViewSize

视图尺寸。

```python
{"type": "ViewSize"}
```

### PixelNormal

像素法线。

```python
{"type": "PixelNormal"}
```

### VertexNormal

顶点法线。

```python
{"type": "VertexNormal"}
```

## 特效节点

### Fresnel

菲涅尔效果。

```python
{
    "type": "Fresnel",
    "properties": {
        "ExponentIn": 5.0,
        "BaseReflectFractionIn": 0.04,
        "Normal": None  # 可选，连接自定义法线
    }
}
# 输入: ExponentIn, BaseReflectFractionIn, Normal
# 输出: 菲涅尔强度 [0, 1]
```

### DepthFade

深度淡入（用于粒子、透明物体）。

```python
{
    "type": "DepthFade",
    "properties": {
        "FadeDistance": 100.0
    }
}
# 输入: FadeDistance, Opacity
# 输出: 淡入后的 Opacity
```

### Distance

计算距离。

```python
{
    "type": "Distance",
    # 输入: A, B
    # 输出: |A - B|
}
```

### SceneDepth

场景深度。

```python
{"type": "SceneDepth"}
# 输出: 场景深度值
```

### PixelDepth

像素深度。

```python
{"type": "PixelDepth"}
```

## 向量操作节点

### Normalize

归一化。

```python
{"type": "Normalize"}
```

### DotProduct

点积。

```python
{
    "type": "DotProduct",
    # 输入: A, B
    # 输出: dot(A, B)
}
```

### CrossProduct

叉积。

```python
{
    "type": "CrossProduct",
    # 输入: A, B
    # 输出: cross(A, B)
}
```

### Reflection

反射向量。

```python
{
    "type": "Reflection",
    # 输入: IncidentVector, Normal
    # 输出: 反射向量
}
```

### RotateAboutAxis

绕轴旋转。

```python
{
    "type": "RotateAboutAxis",
    # 输入: NormalizedRotationAxis, RotationAngle, Position, PivotPoint
}
```

## 分量操作节点

### BreakOutFloat3

分解 3D 向量。

```python
{"type": "BreakOutFloat3"}
# 输入: float3
# 输出: R, G, B
```

### MakeFloat3

组合 3D 向量。

```python
{"type": "MakeFloat3"}
# 输入: R, G, B
# 输出: float3
```

### ComponentMask

分量遮罩。

```python
{
    "type": "ComponentMask",
    "properties": {
        "R": True,
        "G": True,
        "B": False,
        "A": False
    }
}
```

### AppendVector

追加向量。

```python
{
    "type": "AppendVector",
    # 输入: A, B
    # 输出: (A, B)
}
```

## 流程控制节点

### If

条件分支。

```python
{
    "type": "If",
    "properties": {
        "Threshold": 0.0001
    }
}
# 输入: A, B, A > B, A == B, A < B
# 输出: 根据 A 与 B 的比较结果选择输出
```

### StaticSwitch

静态开关。

```python
{
    "type": "StaticSwitch",
    "properties": {
        "Value": True
    }
}
# 输入: A, B
# 输出: Value ? A : B (编译时决定)
```

### BumpOffset

视差偏移。

```python
{
    "type": "BumpOffset",
    "properties": {
        "HeightRatio": 0.05
    }
}
# 输入: Coordinate, Height, ReferencePlane
# 输出: 偏移后的 UV
```

## 材质属性连接

主材质节点的主要输入引脚：

| 引脚名 | 类型 | 说明 |
|--------|------|------|
| Base Color | float3 | 漫反射颜色 |
| Metallic | float | 金属度 [0, 1] |
| Specular | float | 高光强度 [0, 1] |
| Roughness | float | 粗糙度 [0, 1] |
| Normal | float3 | 切线空间法线 |
| World Position Offset | float3 | 世界坐标偏移 |
| World Displacement | float3 | 世界置换 (Tessellation) |
| Tessellation Multiplier | float | 细分倍增 |
| Subsurface Color | float3 | 次表面散射颜色 |
| Clear Coat | float | 清漆强度 |
| Clear Coat Roughness | float | 清漆粗糙度 |
| Ambient Occlusion | float | 环境光遮蔽 |
| Refraction | float | 折射率 |
| Pixel Depth Offset | float | 像素深度偏移 |
| Shading Model | int | 着色模型 ID |
| Emissive Color | float3 | 自发光颜色 |
| Opacity | float | 不透明度 |
| Opacity Mask | float | 不透明遮罩 |

## 材质属性设置

```python
build_material_graph(
    material_name="M_MyMaterial",
    nodes=[...],
    connections=[...],
    properties={
        "BlendMode": "BLEND_Opaque",           # Opaque, Masked, Translucent, Additive, Modulate
        "ShadingModel": "MSM_DefaultLit",      # Unlit, DefaultLit, Subsurface, PreintegratedSkin, ClearCoat, etc.
        "TwoSided": True,
        "DitheredLODTransition": True,
        "DepthTest": True,
        "CastRayTracedShadows": True
    }
)
```
