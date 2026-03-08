# UE 材质函数参考

材质函数是可复用的材质逻辑单元，可在多个材质中共享。

## 内置材质函数路径

### 数学运算

| 函数名 | 路径 | 用途 |
|--------|------|------|
| CheapContrast | `/Engine/Functions/Engine_MaterialFunctions01/ImageAdjustment/CheapContrast` | 对比度调整 |
| CheapContrast_RGB | `/Engine/Functions/Engine_MaterialFunctions01/ImageAdjustment/CheapContrast_RGB` | RGB 对比度 |
| SceneTextureWorldUV | `/Engine/Functions/Engine_MaterialFunctions02/Texturing/SceneTextureWorldUV` | 世界坐标 UV |

### 向量操作

| 函数名 | 路径 | 用途 |
|--------|------|------|
| BreakOutFloat3 | `/Engine/Functions/Engine_MaterialFunctions02/Math/BreakOutFloat3` | 分解 float3 |
| MakeFloat3 | `/Engine/Functions/Engine_MaterialFunctions02/Math/MakeFloat3` | 组合 float3 |
| BlendAngleCorrectedNormals | `/Engine/Functions/Engine_MaterialFunctions02/Math/BlendAngleCorrectedNormals` | 正确混合法线 |

### 纹理与坐标

| 函数名 | 路径 | 用途 |
|--------|------|------|
| WorldAlignedTexture | `/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture` | 三向投影纹理 |
| WorldAlignedNormal | `/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal` | 三向投影法线 |
| ObjectScale | `/Engine/Functions/Engine_MaterialFunctions02/WorldPositionEffects/ObjectScale` | 获取物体缩放 |
| ObjectLocalBounds | `/Engine/Functions/Engine_MaterialFunctions02/WorldPositionEffects/ObjectLocalBounds` | 物体局部边界 |

### 摄像机与视图

| 函数名 | 路径 | 用途 |
|--------|------|------|
| CameraDistanceFade | `/Engine/Functions/Engine_MaterialFunctions01/DistanceFade/CameraDistanceFade` | 距离淡入 |
| CameraPositionFade | `/Engine/Functions/Engine_MaterialFunctions01/DistanceFade/CameraPositionFade` | 位置淡入 |
| DistanceFromCamera | `/Engine/Functions/Engine_MaterialFunctions02/WorldPositionEffects/DistanceFromCamera` | 到相机距离 |

### 深度与透明

| 函数名 | 路径 | 用途 |
|--------|------|------|
| DepthFade | `/Engine/Functions/Engine_MaterialFunctions01/Opacity/DepthFade` | 深度淡入 |
| SoftOpacity | `/Engine/Functions/Engine_MaterialFunctions01/Opacity/SoftOpacity` | 柔和透明 |

### 时间与动画

| 函数名 | 路径 | 用途 |
|--------|------|------|
| TimeNode_WithSpeedVariable | `/Engine/Functions/Engine_MaterialFunctions02/Math/TimeNode_WithSpeedVariable` | 可变速时间 |
| Panner | 内置节点 | UV 平移 |

### 环境与背景

| 函数名 | 路径 | 用途 |
|--------|------|------|
| AOMaterialBlend | `/Engine/Functions/Engine_MaterialFunctions02/Environment/AOMaterialBlend` | AO 材质混合 |
| CloudOpacityAltitude | `/Engine/Functions/Engine_MaterialFunctions02/Environment/CloudOpacityAltitude` | 云层透明度 |

## 常用函数详解

### WorldAlignedTexture

三向投影纹理，无 UV 拉伸。

```
输入:
  - TextureObject: 纹理对象 (TextureObject 节点)
  - TextureSize: 纹理世界尺寸
  - WorldPosition: 世界坐标 (可选)
  - WorldSpaceNormal: 世界法线 (可选)

输出:
  - XYZ Texture: 投影结果
  - XY Texture
  - Z Texture
```

使用示例：

```python
nodes = [
    {
        "type": "TextureObject",
        "name": "BaseColorTexture",
        "properties": {"Texture": "/Game/Textures/T_Ground"}
    },
    {
        "type": "MaterialFunctionCall",
        "name": "WorldAlignedTexture",
        "properties": {
            "Function": "/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"
        }
    },
    {
        "type": "Constant",
        "name": "TextureSize",
        "properties": {"R": 100.0}
    }
]

connections = [
    {"from_node": "BaseColorTexture", "from_output": "Output", "to_node": "WorldAlignedTexture", "to_input": "TextureObject"},
    {"from_node": "TextureSize", "from_output": "Output", "to_node": "WorldAlignedTexture", "to_input": "TextureSize"},
    {"from_node": "WorldAlignedTexture", "from_output": "XYZ Texture", "to_node": "MaterialGraphNode_0", "to_input": "Base Color"}
]
```

### DepthFade

解决透明物体与不透明物体相交处的硬边缘。

```
输入:
  - FadeDistance: 淡入距离 (默认 100)
  - Opacity: 原始透明度

输出:
  - Opacity: 淡入后的透明度
```

### BlendAngleCorrectedNormals

正确混合两层法线贴图。

```
输入:
  - BaseNormal: 基础法线
  - AdditionalNormal: 叠加法线

输出:
  - Result: 混合后的法线
```

### CameraDistanceFade

根据到相机距离淡入淡出。

```
输入:
  - FadeLength: 淡入距离
  - FadeStart: 开始距离
  - FadeEnd: 结束距离

输出:
  - Opacity: 淡入系数 [0, 1]
```

## 自定义材质函数

### 创建材质函数

```python
create_asset(
    asset_type="MaterialFunction",
    name="MF_MyFunction",
    path="/Game/Materials/Functions/"
)
```

### 材质函数结构

材质函数包含：
- **Input 节点**: 定义输入参数
- **Output 节点**: 定义输出值
- **内部逻辑**: 计算节点

### 获取材质函数内容

```python
result = get_material_graph(
    asset_path="/Game/Materials/Functions/MF_MyFunction"
)

# 返回:
{
    "asset_type": "MaterialFunction",
    "inputs": [
        {"name": "BaseColor", "type": "float3", "default": [0,0,0]},
        {"name": "Roughness", "type": "float", "default": 0.5}
    ],
    "outputs": [
        {"name": "Result", "type": "float3"}
    ],
    "nodes": [...],
    "connections": [...]
}
```

### 在材质中使用函数

```python
{
    "type": "MaterialFunctionCall",
    "name": "MyFunctionCall",
    "properties": {
        "Function": "/Game/Materials/Functions/MF_MyFunction"
    },
    "position": [-200, 0]
}
```

## 材质函数最佳实践

### 命名规范

| 类型 | 前缀 | 示例 |
|------|------|------|
| 材质函数 | MF_ | MF_TriplanarProjection |
| 输入参数 | In_ | In_BaseColor, In_Roughness |
| 输出参数 | Out_ | Out_Result, Out_Normal |

### 设计原则

1. **单一职责**: 每个函数只做一件事
2. **合理默认值**: 输入参数提供合理的默认值
3. **文档注释**: 在函数描述中说明用途和用法
4. **避免循环依赖**: 函数间不要互相引用

### 常见材质函数模式

**颜色调整函数**

```python
# MF_ColorAdjust
Inputs:
  - In_Color: float3
  - In_Saturation: float (default 1.0)
  - In_Brightness: float (default 1.0)
  - In_Contrast: float (default 1.0)
Outputs:
  - Out_Color: float3
```

**UV 平铺函数**

```python
# MF_TileUV
Inputs:
  - In_UV: float2
  - In_TileU: float (default 1.0)
  - In_TileV: float (default 1.0)
Outputs:
  - Out_UV: float2
```

**遮罩混合函数**

```python
# MF_MaskBlend
Inputs:
  - In_A: float3
  - In_B: float3
  - In_Mask: float
Outputs:
  - Out_Result: float3
```

## 材质函数库结构

推荐的项目材质函数目录结构：

```
/Game/Materials/Functions/
├── Math/              # 数学运算
│   ├── MF_LerpWithCurve
│   └── MF_RemapValue
├── Texturing/         # 纹理处理
│   ├── MF_TriplanarProjection
│   └── MF_TextureBlend
├── Utility/           # 工具函数
│   ├── MF_CameraFade
│   └── MF_DistanceCull
└── Effects/           # 特效函数
    ├── MF_FresnelRim
    └── MF_ParallaxOcclusion
```
