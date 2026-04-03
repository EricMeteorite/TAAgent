# 破碎特效导出流程 (Houdini → UE)

## 概述

本文档描述从 Houdini 导出破碎数据到 UE Niagara 的完整流程。

---

## 方案选择

### 推荐方案：合并网格 + ChunkID 属性

```
┌─────────────────────────────────────────────────────────────┐
│                    Houdini 导出                              │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Voronoi Fracture                                           │
│       │                                                      │
│       ▼                                                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  合并为单个几何体                                    │    │
│  │  每个顶点添加:                                       │    │
│  │  - @chunkid (int)    碎片索引                        │    │
│  │  - @chunknormal (vector) 碎片初始法线方向            │    │
│  └─────────────────────────────────────────────────────┘    │
│       │                                                      │
│       ├──────────────────────┐                              │
│       ▼                      ▼                              │
│  FBX 导出                JSON/CSV 导出                       │
│  (网格数据)              (碎片元数据)                        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Houdini 节点设置

### Step 1: Voronoi Fracture

```
geometry_object
└── voronoifracture
    ├── Input: 要破碎的模型
    └── Piece Count: 50 (或使用 scatter 控制点)
```

### Step 2: 计算碎片属性

添加 **Attribute Wrangle** (Run Over: Primitives):

```c
// chunkid.vfl - 运行在 Primitive 上
i@chunkid = @primnum;

// 计算质心 (用于连接锚点)
v@chunkcenter = getbbox_center(0);

// 计算质量 (基于体积)
f@chunkmass = volume(0);

// 计算边界盒
vector min, max;
getbbox(0, min, max);
v@chunkboundsmin = min;
v@chunkboundsmax = max;

// 计算惯性张量简化 (边界盒近似)
vector size = max - min;
f@chunkinertia = pow(size.x * size.y * size.z, 1.0/3.0);
```

### Step 3: 传播 ChunkID 到顶点

添加 **Attribute Transfer** 或 **Attribute Wrangle** (Run Over: Points):

```c
// 从 primitive 传递到 vertex
// Houdini 会在导出时自动插值
i@chunkid = prim(0, "chunkid", @primnum);
```

### Step 4: 计算连接关系

添加 **Attribute Wrangle** (Run Over: Primitives):

```c
// connections.vfl - 找到相邻碎片
int neighbors[] = neighbours(0, @primnum);
i[]@neighborchunks = neighbors;

// 为每对相邻碎片计算连接锚点
// 需要找到共享边缘的中点
```

更精确的连接计算使用 **Connect Adjacent Pieces** 节点：

```
connectadjacentpieces
├── Input: 破碎后的几何体
├── Connection Type: Adjacent Pieces
└── Output: 连接线几何体
```

### Step 5: 合并网格并导出

```
# 合并所有碎片为一个几何体
geometry_wrangle (Run Over: Detail)
    // 确保所有 primitive 都有 chunkid
    // 不需要分离，直接保持合并状态

# 导出 FBX
rop_fbx
├── Output File: FractureMesh.fbx
└── 勾选 "Export Vertex Colors" (用于 chunkid)
```

---

## 数据导出格式

### 1. FBX 网格文件

导出**单个合并网格**，包含：
- 所有碎片的顶点和面
- Vertex Color 存储 ChunkID (红色通道)

### 2. JSON 元数据文件

```json
{
  "chunk_count": 50,
  "chunks": [
    {
      "id": 0,
      "center": [10.5, 20.0, 5.5],
      "bounds_min": [8.0, 18.0, 3.0],
      "bounds_max": [13.0, 22.0, 8.0],
      "mass": 1.5,
      "volume": 120.0,
      "initial_position": [10.5, 20.0, 5.5],
      "initial_rotation": [0.0, 0.0, 0.0, 1.0]
    },
    // ... 更多碎片
  ],
  "connections": [
    {
      "chunk_a": 0,
      "chunk_b": 1,
      "anchor_a": [12.0, 20.0, 5.5],
      "anchor_b": [12.0, 20.0, 5.5],
      "break_distance": 0.5,
      "break_force": 100.0,
      "stiffness": 0.8
    },
    // ... 更多连接
  ]
}
```

### 3. Houdini Python 导出脚本

```python
# export_fracture_data.py
import hou
import json

def export_fracture_data(geo_node, output_path):
    """导出破碎数据到 JSON"""
    geo = geo_node.geometry()
    
    data = {
        "chunk_count": 0,
        "chunks": [],
        "connections": []
    }
    
    # 收集碎片数据
    chunk_map = {}
    for prim in geo.prims():
        chunk_id = prim.attribValue("chunkid")
        if chunk_id not in chunk_map:
            chunk_map[chunk_id] = {
                "id": chunk_id,
                "center": list(prim.attribValue("chunkcenter")),
                "mass": prim.attribValue("chunkmass"),
                "bounds_min": list(prim.attribValue("chunkboundsmin")),
                "bounds_max": list(prim.attribValue("chunkboundsmax"))
            }
    
    data["chunk_count"] = len(chunk_map)
    data["chunks"] = list(chunk_map.values())
    
    # 收集连接数据
    connection_set = set()
    for prim in geo.prims():
        chunk_id = prim.attribValue("chunkid")
        neighbors = prim.attribValue("neighborchunks")
        
        for neighbor_id in neighbors:
            # 避免重复连接
            key = (min(chunk_id, neighbor_id), max(chunk_id, neighbor_id))
            if key not in connection_set:
                connection_set.add(key)
                # 计算锚点 (简化：两碎片中心的中点)
                center_a = chunk_map[chunk_id]["center"]
                center_b = chunk_map[neighbor_id]["center"]
                anchor = [(center_a[i] + center_b[i]) / 2 for i in range(3)]
                
                data["connections"].append({
                    "chunk_a": chunk_id,
                    "chunk_b": neighbor_id,
                    "anchor_a": anchor,
                    "anchor_b": anchor,
                    "break_distance": 0.5,
                    "break_force": 100.0,
                    "stiffness": 0.8
                })
    
    # 写入文件
    with open(output_path, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"Exported {data['chunk_count']} chunks with {len(data['connections'])} connections")

# 使用方法
# geo_node = hou.node("/obj/geo1/voronoifracture1")
# export_fracture_data(geo_node, "FractureData.json")
```

---

## UE 导入流程

### Step 1: 导入 FBX

```
1. 拖拽 FractureMesh.fbx 到 Content Browser
2. 导入设置:
   - 勾选 "Import Vertex Colors"
   - Normal Import Method: Compute Normals
```

### Step 2: 创建 NiagaraDataInterfaceFracture

```cpp
// 在 UE 中创建资产
UNiagaraDataInterfaceFracture* DI = NewObject<UNiagaraDataInterfaceFracture>();
DI->SetFractureMesh(LoadedMesh);
DI->LoadFractureData("FractureData.json");
```

### Step 3: Niagara System 设置

```
Niagara System
├── Emitter: FractureChunks
│   ├── Spawn Rate: 0 (使用 Initialize Particle)
│   ├── Data Interface: Fracture DI
│   └── Render: Mesh Renderer
│       └── Mesh: 使用粒子变换，但渲染同一个网格
```

---

## 渲染方案详解

### 方案 A: GPU Instancing (推荐)

**原理**：一个 DrawCall 渲染所有碎片，每个实例有独立变换

```
┌─────────────────────────────────────────────────────────────┐
│                    GPU Instancing 流程                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Mesh Renderer (Niagara)                                    │
│  ├── Mesh: FractureMesh (合并网格)                          │
│  ├── Override Materials: M_Fracture                         │
│  └── Instance Count: ChunkCount                             │
│                                                              │
│  Per-Instance Data (从粒子属性):                             │
│  ├── Position[i] → 实例 i 的位置                            │
│  ├── Rotation[i] → 实例 i 的旋转                            │
│  └── Scale[i] → 实例 i 的缩放                               │
│                                                              │
│  Vertex Shader:                                              │
│  ```hlsl                                                     │
│  float3 WorldPos = mul(InstanceMatrix, LocalPos);           │
│  // 每个 chunk 的顶点根据实例变换移动                        │
│  ```                                                         │
│                                                              │
│  问题: 如何让每个 chunk 的顶点跟随自己的实例变换？            │
│                                                              │
│  解决方案:                                                   │
│  1. 将顶点按 chunk 分组                                     │
│  2. 在 Vertex Shader 中根据 chunkid 查找实例变换            │
│  3. 或使用 Per-Draw 实例 ID                                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

**关键问题**：Niagara Mesh Renderer 不直接支持 "一个粒子 = 网格的一部分"

### 方案 B: 使用 Submesh Index (实用)

**原理**：使用 Mesh Renderer 的子网格功能，但需要调整网格结构

```
Houdini 导出时:
1. 每个碎片作为一个 Submesh (Material Slot)
2. UE 导入后使用 Multi Material

Niagara 渲染:
- 问题: Standard Mesh Renderer 不支持 Per-Particle Submesh 选择
- 需要自定义 Renderer 或使用多个 Emitter
```

### 方案 C: 自定义渲染方案 (最灵活)

**原理**：使用 Niagara GPU Sprites + 自定义 Material

```
┌─────────────────────────────────────────────────────────────┐
│                    自定义渲染流程                             │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  方法 1: 粒子位置 = 碎片中心，使用 Imposter/Proxy 网格       │
│                                                              │
│  方法 2: 自定义 GPUParticles Renderer                        │
│  - 在 HLSL 中实现顶点变形                                    │
│  - 读取 ChunkID，应用对应的粒子变换                          │
│                                                              │
│  方法 3: 使用 Render Target 渲染                             │
│  - 预先渲染每个碎片的深度/法线到 RT                          │
│  - 运行时根据粒子变换采样 RT                                 │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 推荐实现方案

### 最终方案：分离网格 + Mesh Renderer Array

考虑到 Niagara 的限制，**最实用的方案**是：

```
┌─────────────────────────────────────────────────────────────┐
│                    实用方案: 网格数组                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Houdini 导出时拆分为 50 个独立网格                       │
│     (或使用 UE 的 Mesh Merge 后分离)                         │
│                                                              │
│  2. UE 中使用 Static Mesh Array                              │
│     - 存储所有碎片网格                                       │
│     - 粒子属性: MeshIndex = ChunkID                         │
│                                                              │
│  3. Niagara Mesh Renderer                                    │
│     - Mesh Binding: User Parameter Array                    │
│     - 或使用 Material Slot 切换                              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 简化方案：使用相同网格 + 程序化变形

如果碎片形状相似（如立方体），可以：

```
1. 使用单个代理网格 (如 Cube)
2. 每个粒子渲染这个代理网格
3. 在 Material 中根据 ChunkID 变形顶点
4. 存储每个 chunk 的"变形参数"到纹理
```

---

## Houdini 导出脚本 (完整版)

```python
# houdini_export_fracture.py
"""
Houdini 破碎导出脚本
导出两个文件:
1. FBX: 合并网格 (带 ChunkID 顶点属性)
2. JSON: 碎片元数据和连接关系
"""

import hou
import json
import os

def export_fracture(node_path, output_dir):
    """
    导出破碎数据
    
    Args:
        node_path: Houdini 节点路径 (如 "/obj/geo1/voronoifracture1")
        output_dir: 输出目录
    """
    node = hou.node(node_path)
    geo = node.geometry()
    
    # === 1. 收集碎片数据 ===
    chunks = []
    chunk_centers = {}
    
    for prim in geo.prims():
        chunk_id = prim.intAttribValue("chunkid")
        
        if chunk_id not in chunk_centers:
            # 计算碎片边界和中心
            points = prim.points()
            positions = [pt.position() for pt in points]
            
            min_pos = hou.Vector3(
                min(p.x() for p in positions),
                min(p.y() for p in positions),
                min(p.z() for p in positions)
            )
            max_pos = hou.Vector3(
                max(p.x() for p in positions),
                max(p.y() for p in positions),
                max(p.z() for p in positions)
            )
            center = (min_pos + max_pos) * 0.5
            
            # 计算体积 (近似)
            size = max_pos - min_pos
            volume = size.x() * size.y() * size.z()
            
            chunk_data = {
                "id": chunk_id,
                "center": [center.x(), center.y(), center.z()],
                "bounds_min": [min_pos.x(), min_pos.y(), min_pos.z()],
                "bounds_max": [max_pos.x(), max_pos.y(), max_pos.z()],
                "mass": volume * 0.001,  # 假设密度
                "volume": volume
            }
            
            chunks.append(chunk_data)
            chunk_centers[chunk_id] = center
    
    # === 2. 收集连接数据 ===
    connections = []
    connection_set = set()
    
    # 使用 proximity 查找相邻碎片
    # 这里简化实现，实际应使用 shared edges
    for i, chunk_a in enumerate(chunks):
        for chunk_b in chunks[i+1:]:
            # 检查边界是否相交
            # 简化: 基于距离
            center_a = hou.Vector3(chunk_a["center"])
            center_b = hou.Vector3(chunk_b["center"])
            dist = (center_a - center_b).length()
            
            # 如果碎片中心距离小于阈值，建立连接
            size_a = hou.Vector3(chunk_a["bounds_max"]) - hou.Vector3(chunk_a["bounds_min"])
            size_b = hou.Vector3(chunk_b["bounds_max"]) - hou.Vector3(chunk_b["bounds_min"])
            max_size = max(size_a.length(), size_b.length())
            
            if dist < max_size * 1.5:  # 阈值
                # 计算锚点 (两碎片边界的中点)
                anchor = [
                    (chunk_a["center"][i] + chunk_b["center"][i]) * 0.5
                    for i in range(3)
                ]
                
                connections.append({
                    "chunk_a": chunk_a["id"],
                    "chunk_b": chunk_b["id"],
                    "anchor_a": anchor,
                    "anchor_b": anchor,
                    "break_distance": max_size * 0.1,
                    "break_force": 100.0,
                    "stiffness": 0.9,
                    "damping": 0.1
                })
    
    # === 3. 写入 JSON ===
    output_data = {
        "chunk_count": len(chunks),
        "chunks": chunks,
        "connections": connections
    }
    
    json_path = os.path.join(output_dir, "FractureData.json")
    with open(json_path, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"Exported {len(chunks)} chunks with {len(connections)} connections to {json_path}")
    
    # === 4. 导出 FBX ===
    # 在 Houdini 中使用 ROP FBX 节点
    # 或通过 Python 调用
    fbx_path = os.path.join(output_dir, "FractureMesh.fbx")
    
    # 创建临时 ROP 节点
    parent = node.parent()
    rop = parent.createNode("rop_fbx", "temp_fbx_export")
    rop.parm("sopoutput").set(fbx_path)
    rop.parm("exec").pressButton()
    rop.destroy()
    
    print(f"Exported mesh to {fbx_path}")

# === 使用方法 ===
# export_fracture("/obj/geo1/voronoifracture1", "D:/MyProject/Content/Fracture")
```

---

## 总结

### 推荐流程

| 步骤 | Houdini | UE |
|------|---------|-----|
| 1 | Voronoi Fracture | - |
| 2 | 计算 Chunk 属性 | - |
| 3 | 导出 FBX + JSON | - |
| 4 | - | 导入 FBX |
| 5 | - | 创建 NiagaraDataInterfaceFracture |
| 6 | - | 设置 Niagara System |
| 7 | - | 配置物理模拟模块 |

### 渲染策略

对于 **50 个碎片**，建议：

1. **方案 A (简单)**: 50 个独立网格 + Mesh Renderer 的 Mesh Variable
2. **方案 B (高效)**: 合并网格 + 自定义 Shader 根据 ChunkID 变换
3. **方案 C (折中)**: 使用 HISM (Hierarchical Instanced Static Mesh)

对于 **100+ 碎片**，必须使用方案 B 或 GPU 粒子渲染。