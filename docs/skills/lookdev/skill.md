# Lookdev (Look Development) Skill

Lookdev 是资产检查和材质验证的核心工作流程，目标是创建统一的预览环境，确保资产在不同环境下表现一致。

## 核心原则

### 1. 曝光准确
- **EV100=0** 作为基准亮度
- 使用灰板测光：Linear 0.18（中灰，sRGB 0.46/119）
- 关闭自动曝光，使用手动曝光
- 灰板 Final Color 应达到 0.18 左右

### 2. 光比控制
光比决定材质的结构、纹理、质感表现：

| 光比类型 | 对比度 | 适用场景 | 明暗比 |
|---------|--------|---------|--------|
| 低反差 | 1.65x (0.7档) | 阴天 | 检查灰阶、色彩、明度 |
| 中反差 | 2.6x (1.5档) | 多云 | 兼顾质感表现 |
| 高反差 | 16x (4档) | 晴朗 | 极端情况检查、质感强化 |

### 3. 中性光源
- 色温：6500K（中性白）
- 避免偏色影响材质色彩定义

## 环境搭建

### 测光对象设置

```
灰板材质参数：
- Shading Model: Default Lit
- BaseColor: 0.18 (Linear) / 0.46 (sRGB)
- Specular: 0（关闭高光）
- Roughness: 0.5

放置方式：面朝上，水平放置
```

### 后期处理设置

```
必须关闭（影响测光）：
- Auto Exposure → 关闭
- Bloom → 关闭
- Vignette → 关闭
- SSAO → 关闭
- SSR → 关闭

曝光参数：
- Exposure Compensation: 0
- Min/Max Brightness: 相同值（锁定曝光）
- EV100: 0
```

### 项目设置

```
关闭 Pre Exposure：
Project Settings → Engine → Rendering → PostProcessing → 
  ExtendDefaultLuminanceRange = false
```

## 灯光布置流程

### 1. 设置曝光基准
```
Post Process Volume:
- Exposure Mode: Manual
- Exposure Compensation: 0 (EV100=0)
```

### 2. 调整环境光（天光/IBL）
```
1. 关闭直射光
2. 使用 Pixel Inspector 吸取灰板阴影区域
3. 调整天光强度，使阴影区达到目标亮度

目标亮度计算：
- 低反差: 0.18 / 1.65 ≈ 0.109
- 中反差: 0.18 / 2.6 ≈ 0.069
- 高反差: 0.18 / 16 ≈ 0.011
```

### 3. 调整直射光
```
1. 开启直射光
2. 使用 Pixel Inspector 吸取灰板亮部区域
3. 调整直射光强度，使亮部达到 0.18
```

## UE MCP 工具使用

### 创建 Lookdev 环境

```python
# 1. 创建测光灰板
create_static_mesh_from_data(
    name="GrayCard",
    positions=[[100,0,0], [0,0,0], [0,100,0], [100,100,0]],
    indices=[0,1,2, 0,2,3],
    normals=[[0,0,1]] * 4
)

# 2. 创建灰板材质
create_material(name="M_GrayCard")
set_material_properties(
    material_name="M_GrayCard",
    shading_model="DefaultLit"
)
# 添加参数节点并连接...

# 3. 创建三种光比环境

# 低反差环境 (1.65x)
create_light(
    light_type="directional",
    name="KeyLight_Low",
    intensity=5.0,
    color=[1.0, 1.0, 1.0],
    rotation=[-45, 0, 0]
)

# 中反差环境 (2.6x)
create_light(
    light_type="directional",
    name="KeyLight_Mid",
    intensity=8.0,
    color=[1.0, 1.0, 1.0],
    rotation=[-45, 0, 0]
)

# 高反差环境 (16x)
create_light(
    light_type="directional",
    name="KeyLight_High",
    intensity=16.0,
    color=[1.0, 1.0, 1.0],
    rotation=[-45, 0, 0]
)
```

### 灯光管理工具

```python
# 获取场景中的所有灯光
lights = get_lights()
lights = get_lights(light_type="directional")  # 按类型过滤

# 修改灯光属性
set_light_properties(
    name="KeyLight_Low",
    intensity=10.0,
    color=[1.0, 0.9, 0.8],
    temperature=6500,
    use_temperature=True
)

# 删除灯光
delete_light(name="KeyLight_Low")
```

### 获取视口截图验证

```python
# 捕获当前视口
result = get_viewport_screenshot(output_path="C:/temp/lookdev_verify.png", format="png", quality=95)
# 截图保存到指定路径用于对比验证
```

### 测光验证

```python
# 使用 Pixel Inspector（需手动操作）
# 吸取灰板区域，检查 Final Color 是否为 0.18
```

## DCC 同步

### Substance Painter 设置

```
显示设置：
- Environment: 使用相同 HDRI
- Environment Rotation: 270°
- EV: -0.45
- Temporal AA: 开启

Shader 设置：
- Tonemapping: ACES

色彩管理（7.4+）：
- 启用 OCIO
- Post Effects Exposure: 1.45（补偿 UE 的 0.45 Gain）
```

### UE 对应设置

```
关闭差异项：
- Directional Light: 关闭（仅用 IBL）
- Environment Color: 0.04
- Bloom, Vignette, AO, SSR: 关闭

目的：最小化差异，以 UE 为准
```

## 非PBR材质适配

### Eye Adaptation 材质节点

用于自发光材质、特效材质等非PBR资产：

```
原理：以 EV100=0 为基准 Bias，自动适应不同亮度环境

使用场景：
- 自发光材质
- 特效材质
- 自定义 Shading Model

效果：在不同 EV 环境下保持恒定"观感"
```

## 常见问题排查

| 问题 | 可能原因 | 解决方案 |
|-----|---------|---------|
| 材质照不亮 | 光比过大、环境光不足 | 增加天光/IBL 强度 |
| 过曝 | 曝光补偿过高 | 检查 EV100 和 Exposure Compensation |
| 对比过强 | LUT/Tonemapping 极端 | 使用渐变灰板检查 |
| 显色性低 | 光源色温偏移 | 使用 6500K 中性光 |
| 质感不佳 | 反射源动态不足 | 增加环境反射变化 |

## 资产色彩范围

```
sRGB 安全范围：38-238（经验值）
Linear 对应：约 0.02-0.85

超出范围风险：
- < 38: 易死黑
- > 238: 易过曝
```

## 工作流总结

```
1. 搭建环境
   └── 创建灰板 → 设置曝光 → 布置灯光

2. 验证环境
   └── Pixel Inspector 测光 → 确认 EV100=0 → 确认光比

3. 检查资产
   └── 三种光比切换 → 检查灰阶/色彩/质感 → 记录问题

4. DCC 同步
   └── 匹配设置 → 快速迭代 → 以 UE 为准
```

## 文件保存

### 作为地图
```
优点：光源丰富、可多资产对比
方式：Save Current Level as Template
路径：Content/Maps/Templates/
```

### 作为 Preview Profile
```
优点：任何编辑器可用
方式：Editor Preferences → Preview Scene Settings → Save as Profile
共享：勾选 Shared Profile，同步 Editor.ini
```

## 参考资源

- [UE Lookdev 官方博客](https://www.unrealengine.com/zh-CN/tech-blog/a-few-tips-for-building-unified-assets-reviewing-enviroment)
- [HDRI Haven](https://hdri-haven.com) - 高质量 HDRI 资源
- Gray Card Linear: 0.18 / sRGB: 119,119,119
