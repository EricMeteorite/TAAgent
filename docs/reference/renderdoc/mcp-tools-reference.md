# RenderDoc MCP 工具参考

## 帧分析工具

### get_capture_status

检查是否有捕获已加载。

```python
result = get_capture_status()
# 返回: {"loaded": True, "api": "D3D12"}
```

### get_frame_summary

获取当前帧的概览信息。

```python
result = get_frame_summary()
# 返回:
# {
#     "api": "D3D12",
#     "total_actions": 1500,
#     "statistics": {"drawcalls": 800, "dispatches": 50, ...},
#     "resources": {"textures": 200, "buffers": 150}
# }
```

### get_draw_calls

获取所有绘制调用列表。

```python
result = get_draw_calls(
    include_children=True,      # 包含子层级
    marker_filter="Opaque",     # 过滤标记
    exclude_markers=["UI"],     # 排除标记
    event_id_min=100,           # 最小事件ID
    event_id_max=500,           # 最大事件ID
    only_actions=True,          # 只返回 action，不含 marker
    flags_filter=["Drawcall"]   # 过滤类型
)
```

### get_action_timings

获取 GPU 时间信息。

```python
result = get_action_timings(
    event_ids=[100, 101, 102],  # 可选，指定事件
    marker_filter="MainPass"    # 可选，按标记过滤
)
# 返回: {"timings": [...], "total_duration_ms": 16.7}
```

## 查找工具

### find_draws_by_shader

按着色器名称查找绘制调用。

```python
result = find_draws_by_shader(
    shader_name="MainPS",  # 部分匹配
    stage="pixel"          # 可选: vertex, pixel, compute 等
)
```

### find_draws_by_texture

按纹理名称查找绘制调用。

```python
result = find_draws_by_texture(
    texture_name="Diffuse"  # 部分匹配
)
```

### find_draws_by_resource

按资源ID精确查找。

```python
result = find_draws_by_resource(
    resource_id="ResourceId::12345"  # 精确匹配
)
```

## 绘制调用详情

### get_draw_call_details

获取特定绘制调用的详细信息。

```python
result = get_draw_call_details(event_id=100)
# 返回: 顶点数、索引数、输出资源等
```

### get_pipeline_state

获取完整的管线状态。

```python
result = get_pipeline_state(event_id=100)
# 返回:
# {
#     "shaders": {"vertex": {...}, "pixel": {...}},
#     "srvs": [...],      # 纹理绑定
#     "uavs": [...],      # UAV 绑定
#     "samplers": [...],  # 采样器
#     "cbuffers": [...],  # 常量缓冲区
#     "render_targets": [...],
#     "depth_target": {...}
# }
```

## Shader 信息

### get_shader_info

获取着色器详情。

```python
result = get_shader_info(
    event_id=100,
    stage="pixel"  # vertex, hull, domain, geometry, pixel, compute
)
# 返回:
# {
#     "disassembly": "...",        # 反汇编代码
#     "constant_buffers": [...],   # 常量缓冲区值
#     "resources": [...]           # 资源绑定
# }
```

## 网格工具

### get_mesh_data

获取网格顶点数据。

```python
result = get_mesh_data(event_id=100)
# 返回: 顶点位置、法线、UV、切线、索引等
```

### export_mesh_csv

导出网格数据为 CSV。

```python
result = export_mesh_csv(
    event_id=100,
    output_path="D:/output/mesh.csv",
    stage="vs_output"  # "vs_input" 或 "vs_output"
)
```

### export_mesh_as_fbx

导出网格为 FBX。

```python
result = export_mesh_as_fbx(
    event_id=100,
    output_path="D:/output/mesh.fbx",
    attribute_mapping={
        "POSITION": "vs_input:ATTRIBUTE0",
        "NORMAL": "buffer:Buffer1",
        "UV": "vs_output:TEXCOORD0"
    },
    decode={
        "NORMAL": "normalize(itof(x) / 32768)"
    },
    buffer_config={
        "Buffer1": {
            "stride": 32,
            "normal_offset": 16,
            "format": "float4"
        }
    },
    coordinate_system="ue",      # ue, unity, blender, maya
    unit_scale=1,                # UE 用 cm
    flip_winding_order=False
)
```

## 纹理工具

### get_texture_info

获取纹理元数据。

```python
result = get_texture_info(resource_id="ResourceId::12345")
# 返回: 尺寸、格式、Mip 级别等
```

### get_texture_data

获取纹理像素数据。

```python
result = get_texture_data(
    resource_id="ResourceId::12345",
    mip=0,           # Mip 级别
    slice=0,         # 数组切片或 Cube face (0-5)
    sample=0         # MSAA 采样索引
)
```

### save_texture

保存纹理为图像文件。

```python
result = save_texture(
    resource_id="ResourceId::12345",
    output_path="D:/output/texture.png",
    file_type="png",  # png, tga, dds, jpg, hdr, bmp, exr
    mip=-1,           # -1 = 所有 mip (仅 DDS)
    slice_index=-1    # -1 = 所有切片 (仅 DDS)
)
```

## 缓冲区工具

### get_buffer_contents

读取缓冲区内容。

```python
result = get_buffer_contents(
    resource_id="ResourceId::12345",
    offset=0,     # 字节偏移
    length=0      # 0 = 整个缓冲区
)
# 返回: base64 编码的数据
```

## 文件操作

### list_captures

列出目录中的捕获文件。

```python
result = list_captures(directory="D:/captures")
# 返回: 文件名、路径、大小、修改时间
```

### open_capture

打开捕获文件。

```python
result = open_capture(
    capture_path="D:/captures/frame.rdc"
)
```

## 工具使用流程

### 性能分析流程

```
1. get_frame_summary() → 了解整体情况
2. get_action_timings() → 定位热点
3. get_draw_call_details() → 分析热点详情
4. get_pipeline_state() → 查看管线配置
```

### 网格提取流程

```
1. get_draw_calls() → 找到目标 Draw Call
2. export_mesh_csv() → 分析顶点格式
3. get_pipeline_state() → 确认 Buffer 绑定
4. export_mesh_as_fbx() → 使用正确映射导出
```

### 纹理提取流程

```
1. find_draws_by_texture() → 定位使用该纹理的 Draw Call
2. get_pipeline_state() → 获取 Resource ID
3. get_texture_info() → 了解纹理属性
4. save_texture() → 保存为所需格式
```

### Shader 分析流程

```
1. find_draws_by_shader() → 找到使用该 Shader 的 Draw Call
2. get_shader_info() → 获取反汇编和参数
3. get_pipeline_state() → 查看资源绑定
4. 分析常量缓冲区值 → 提取参数
```
