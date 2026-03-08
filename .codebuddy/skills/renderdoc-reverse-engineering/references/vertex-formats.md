# 引擎顶点格式参考

识别不同引擎的顶点数据布局，用于正确导出网格。

## Unreal Engine 5

### GPU-Driven Rendering (Nanite)

UE5 使用 GPU-Driven 渲染，顶点数据存储在 Buffer 中：

```
Buffer 布局 (典型):
┌─────────────────────────────────────────────────────────────┐
│ Offset 0:   Position (float3, 12 bytes)                    │
│ Offset 12:  Packed Normal/Tangent (uint32, 4 bytes)         │
│ Offset 16:  UV (float2, 8 bytes)                            │
│ Offset 24:  Color (uint32, 4 bytes)                         │
│ Stride: 28-32 bytes                                         │
└─────────────────────────────────────────────────────────────┘
```

### 顶点属性映射

```python
attribute_mapping = {
    "POSITION": "vs_input:ATTRIBUTE0",  # 或 "buffer:VertexBuffer"
    "NORMAL": "buffer:VertexBuffer",    # 需要从 Buffer 读取
    "TANGENT": "buffer:VertexBuffer",
    "UV": "vs_output:TEXCOORD0",        # VS 输出通常有插值后的 UV
}

buffer_config = {
    "VertexBuffer": {
        "stride": 32,
        "normal_offset": 16,
        "tangent_offset": 20,
        "uv_offset": 24,
        "format": "float4"
    }
}
```

### 压缩法线解码

UE5 常用 Octahedral 编码或 16-bit 整数：

```python
# 16-bit 整数编码 (常见)
decode = {
    "NORMAL": "normalize(itof(x) / 32768)",
    "TANGENT": "x * 2 - 1"  # [0,1] → [-1,1]
}

# 或使用内置解码
decode = {
    "NORMAL": "decode_octahedral(x)"
}
```

### Shader 输入 (VS Input)

```
ATTRIBUTE0: Position (float3)
ATTRIBUTE1: Tangent (float4, .w = sign for binormal)
ATTRIBUTE2: Color (float4)
ATTRIBUTE3: UV0 (float2)
ATTRIBUTE4: UV1 (float2)
...
```

## Unity

### 标准顶点格式

Unity 使用更传统的顶点格式：

```python
attribute_mapping = {
    "POSITION": "vs_input:POSITION",
    "NORMAL": "vs_input:NORMAL",
    "TANGENT": "vs_input:TANGENT",
    "UV": "vs_input:TEXCOORD0",
    "UV2": "vs_input:TEXCOORD1",
    "COLOR": "vs_input:COLOR"
}
```

### 语义名称对照

| HLSL 语义 | Unity 属性 |
|-----------|-----------|
| POSITION | 顶点位置 |
| NORMAL | 法线 |
| TANGENT | 切线 |
| TEXCOORD0 | UV0 |
| TEXCOORD1 | UV1 |
| COLOR | 顶点色 |

### 无需解码

Unity 通常使用 float32 存储，不需要解码：

```python
decode = {}  # 无需解码
```

## 自定义引擎

### 分析步骤

1. **导出 CSV 分析**

```python
export_mesh_csv(event_id, output_path, stage="vs_input")
export_mesh_csv(event_id, output_path, stage="vs_output")
```

2. **检查 CSV 列名**

```
vs_input 列:
- ATTRIBUTE0, ATTRIBUTE1, ... (原始输入)

vs_output 列:
- SV_Position, TEXCOORD0, TEXCOORD1, ... (VS 输出)
```

3. **分析数据范围**

```
如果值在 [0, 65535] 范围 → 可能是 16-bit 整数编码
如果值在 [-1, 1] 范围 → 可能是归一化浮点
如果值很大 (>100) → 可能是位置数据
```

4. **查看 Shader 反汇编**

```python
get_shader_info(event_id, "vertex")
# 找到输入声明:
# dcl_input v0.xyz  ; 位置
# dcl_input v1.xyz  ; 法线
# dcl_input v2.xy   ; UV
```

### 常见编码格式

| 数据类型 | 编码 | 解码公式 |
|----------|------|----------|
| 法线 (压缩) | 16-bit int | `normalize(itof(x) / 32768)` |
| 法线 (Octahedral) | 2x 16-bit | `decode_octahedral(x)` |
| 切线 (压缩) | [0,1] | `x * 2 - 1` |
| UV (半精度) | float16 | 直接使用 |
| 颜色 (RGBA) | uint8 | `x / 255` |

## 判断引擎类型

### 通过 Shader 特征

```
UE5 特征:
- Input: ATTRIBUTE0, ATTRIBUTE1...
- CBuffers: View, Primitive, Material...
- 函数名: GetWorldPosition, GetCameraPosition...

Unity 特征:
- Input: appdata_base, appdata_tan...
- CBuffers: UnityPerDraw, UnityPerMaterial...
- 函数名: UnityObjectToClipPos, ComputeGrabScreenPos...
```

### 通过纹理命名

```
UE5:
- T_Diffuse, T_Normal, T_Metallic...
- /Game/Textures/...

Unity:
- _MainTex, _BumpMap, _MetallicGlossMap...
- Assets/Textures/...
```

## 导出检查清单

导出前确认：

- [ ] Position 是否正确？(在 3D 软件中检查位置)
- [ ] 法线方向是否一致？(检查是否反向)
- [ ] UV 是否正确？(检查贴图映射)
- [ ] 切线是否正确？(检查法线贴图效果)

如果法线反向：
```python
export_mesh_as_fbx(..., flip_winding_order=True)
```

## 坐标系统转换

| 目标软件 | coordinate_system | unit_scale | 说明 |
|----------|-------------------|------------|------|
| Unreal Engine | "ue" | 1 | Z-up, 左手, cm |
| Unity | "unity" | 1 | Y-up, 左手, m |
| Blender | "blender" | 100 | Z-up, 右手, m |
| Maya | "maya" | 100 | Y-up, 右手, cm |
