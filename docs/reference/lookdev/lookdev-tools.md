# LookDev 辅助工具

## 一、标准灰值

### 线性与 sRGB 对照

| 名称 | 线性值 | sRGB值 | 十六进制 | 用途 |
|------|--------|--------|----------|------|
| 纯黑 | 0.0 | 0 | #000000 | 暗部参考 |
| 深灰 | 0.05 | 36 | #242424 | 阴影参考 |
| 暗灰 | 0.10 | 77 | #4D4D4D | 暗部细节 |
| 中灰 (18%) | 0.18 | 117 | #757575 | **标准中灰** |
| 亮灰 | 0.30 | 150 | #969696 | 亮部过渡 |
| 浅灰 | 0.50 | 188 | #BCBCBC | 高光过渡 |
| 白灰 | 0.80 | 234 | #EAEAEA | 接近白色 |
| 纯白 | 1.0 | 255 | #FFFFFF | 高光参考 |

### 换算公式

```
线性 → sRGB:
  if (linear <= 0.0031308)
    sRGB = linear × 12.92
  else
    sRGB = 1.055 × linear^(1/2.4) - 0.055

sRGB → 线性:
  if (sRGB <= 0.04045)
    linear = sRGB / 12.92
  else
    linear = ((sRGB + 0.055) / 1.055)^2.4
```

## 二、UE 对照材质

### 引擎内置资源

```
路径: /Engine/EditorMeshes/ColorCalibrator/SM_ColorCalibrator

包含:
- 灰阶色块
- 彩色色块
- 用于快速色彩校验
```

### 自定义灰球材质

```
材质类型: Material
Shading Model: Default Lit

参数:
  BaseColor: (0.18, 0.18, 0.18)
  Metallic: 0.0
  Roughness: 0.95
  Normal: (0.5, 0.5, 1.0) # 平面法线

用途: 漫反射质感参考
```

### 自定义金属球材质

```
材质类型: Material
Shading Model: Default Lit

参数:
  BaseColor: (0.95, 0.95, 0.95)
  Metallic: 1.0
  Roughness: 0.1
  Normal: (0.5, 0.5, 1.0)

用途: 反射参考、高光形状
```

### 不同金属 BaseColor 参考

| 金属类型 | BaseColor RGB | 说明 |
|----------|---------------|------|
| 铝 | (0.913, 0.921, 0.925) | 亮银色 |
| 银 | (0.972, 0.960, 0.915) | 略带暖色 |
| 铜 | (0.955, 0.637, 0.538) | 橙红色 |
| 金 | (1.000, 0.766, 0.336) | 黄金色 |
| 铁 | (0.560, 0.570, 0.580) | 深灰色 |
| 钛 | (0.617, 0.597, 0.576) | 灰白色 |

## 三、色卡参考

### X-Rite ColorChecker

| 色块 | sRGB 值 | 说明 |
|------|---------|------|
| 深肤色 | 115 82 68 | |
| 浅肤色 | 194 150 130 | |
| 蓝天 | 98 122 157 | |
| 植物绿 | 87 108 67 | |
| 蓝花 | 133 128 177 | |
| 偏蓝绿 | 103 189 170 | |
| 橙黄 | 214 126 44 | |
| 紫蓝 | 80 91 166 | |
| 中红 | 193 82 82 | |
| 紫 | 94 60 108 | |
| 黄绿 | 157 188 64 | |
| 橙红 | 224 163 46 | |
| 蓝绿 | 56 61 150 | |
| 红 | 70 148 73 | |
| 黄 | 175 54 60 | |
| 品红 | 231 199 174 | |
| 青 | 187 86 149 | |
| 白 | 243 243 242 | |
| 中灰 | 200 200 200 | |
| 中灰 | 160 160 160 | |
| 中灰 | 122 122 121 | |
| 中灰 | 80 80 80 | |
| 黑 | 49 49 48 | |

### UE 中使用色卡

```
创建方法:
1. 创建 Material
2. 使用 Texture Sample 引用色卡贴图
3. 设置 UV 平铺对应各色块

对比方法:
1. 在场景中放置色卡 3D 模型
2. 渲染并与实际照片对比
3. 调整后期/灯光直至各色块匹配
```

## 四、场景设置

### LookDev 场景模板

```
基础配置:
1. DirectionalLight: 150000 lux, 6500K
2. SkyLight: HDRI, Intensity = 2^EV
3. AtmosphericFog: 可选，模拟大气散射
4. PostProcessVolume: 
   - Exposure: Manual, EV = 13
   - White Balance: 6500K

标准布局:
- 灰球: 场景中心，地面
- 金属球: 灰球旁边
- 色卡: 后方墙上或支架
- 灰阶: 色卡旁边
- 相机: 正对灰球
```

### 灯光角度测试

```
测试配置:
- 主光角度: 0°, 30°, 45°, 60°, 90°
- 记录每种角度的阴影效果
- 用于匹配真实照片的光位

UE 设置:
- DirectionalLight 可旋转测试
- 使用 Sequencer 记录多个角度
```

## 五、材质校准流程

### 步骤 1: 设置物理灯光

```
1. 根据场景类型设置 EV
2. DirectionalLight: 150000 lux (晴天)
3. SkyLight: HDRI + 2^EV 强度
4. 相机: Manual Exposure, EV 对应值
```

### 步骤 2: 放置参考物体

```
1. 放置灰球 (BaseColor=0.18, Roughness=0.95)
2. 放置金属球 (BaseColor=0.95, Roughness=0.1)
3. 放置色卡模型
```

### 步骤 3: 对比调整

```
与 Ground Truth 照片对比:

灰球:
  - 检查高光亮度
  - 检查阴影亮度
  - 检查明暗过渡 (Roughness 影响)
  - 调整: 灯光强度、角度、Roughness

金属球:
  - 检查反射内容
  - 检查高光形状 (光源大小)
  - 调整: 光源尺寸、环境反射

色卡:
  - 对比各色块颜色
  - 检查白平衡
  - 调整: 灯光色温、后期白平衡
```

### 步骤 4: 创建目标材质

```
灯光校准完成后:
1. 创建目标材质
2. 在相同灯光下渲染
3. 与真实物体照片对比
4. 迭代调整材质参数
```

## 六、实用工具

### UE Console Commands

```
// 显示曝光信息
r.EyeAdaptation.VisualizeDebug 1

// 显示光照复杂度
r.ShowMaterialDrawEvents 1

// Lumen 调试
r.LumenScene.EmitterIntensityScale 1.0

// 屏幕百分比
r.ScreenPercentage 100
```

### 材质验证节点

```
Material Editor:
- 使用 DebugScalarNodes 查看中间值
- 使用 Constant 控制参数
- 参数命名规范:
  - BaseColor
  - Roughness
  - Metallic
  - Normal
```

### 渲染对比

```
Movie Render Queue 设置:
- Resolution: 1920×1080 或更高
- Anti-Aliasing: TemporalAA
- Color Space: sRGB (输出)
- EXR 格式用于后期对比 (线性)
```
