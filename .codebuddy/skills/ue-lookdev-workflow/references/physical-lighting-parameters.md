# 物理灯光参数参考

## 一、曝光值 (EV) 对应表

### EV 与场景类型对应

| EV值 | 场景类型 | 照度范围 (lux) |
|------|----------|----------------|
| 15-16 | 雪地/沙滩强阳光 | 100000-130000 |
| 13-14 | 晴天直射阳光 | 50000-100000 |
| 11-12 | 阴天 | 2000-10000 |
| 8-10 | 室内明亮 | 200-500 |
| 5-7 | 室内昏暗 | 20-100 |
| 2-4 | 夜景有灯光 | 1-20 |
| -1 to 1 | 月光/极暗 | 0.1-1 |

### EV 与光圈/快门/ISO 关系

阳光16法则：晴天 f/16, 1/ISO秒

```
EV = log2(N²/t) - log2(ISO/100)

N = 光圈值
t = 快门时间 (秒)
```

示例 (ISO 100):
| EV | 光圈 | 快门 |
|----|------|------|
| 13 | f/16 | 1/125s |
| 13 | f/8 | 1/500s |
| 13 | f/4 | 1/2000s |
| 11 | f/11 | 1/125s |
| 9 | f/5.6 | 1/125s |

## 二、灯光强度单位换算

### UE 支持的单位

| 单位 | 全称 | 说明 | 适用灯光 |
|------|------|------|----------|
| lux | 勒克斯 | 照度单位 | DirectionalLight |
| lm | 流明 | 光通量 | PointLight, SpotLight, RectLight |
| cd | 坎德拉 | 发光强度 | PointLight, SpotLight, RectLight |

### 换算关系

```
1 lux = 1 lm/m² (被照面)
1 cd = 1 lm/sr (单位立体角)

对于点光源:
  照度 E(lux) = 光强 I(cd) / 距离²(m)

流明与坎德拉:
  lm = cd × 2π (半球光源)
  lm = cd × 4π (全向点光源)
```

### 常见光源强度参考

| 光源类型 | 流明 (lm) | 坎德拉 (cd) |
|----------|-----------|-------------|
| 蜡烛 | 12 | 1 |
| 40W 白炽灯 | 450 | 35 |
| 60W 白炽灯 | 800 | 60 |
| 100W 白炽灯 | 1600 | 130 |
| LED 灯泡 (9W) | 800 | 60 |
| 汽车远光灯 | 3000 | 240 |
| 汽车近光灯 | 1000 | 80 |
| 路灯 (高压钠灯) | 15000 | 1200 |

## 三、色温参考

### 自然光源

| 光源 | 色温 (K) | 说明 |
|------|----------|------|
| 烛光 | 1800-1900 | 暖黄 |
| 日出/日落 | 2000-3000 | 金黄色 |
| 早晨/黄昏 | 3000-4000 | 暖白 |
| 中午阳光 | 5000-5500 | 白光 |
| 晴天阴影 | 7000-8000 | 偏蓝 |
| 阴天 | 6500-7500 | 蓝白 |
| 晴天天空 | 10000-15000 | 深蓝 |

### 人造光源

| 光源 | 色温 (K) |
|------|----------|
| 火柴火焰 | 1700 |
| 高压钠灯 | 2100-2500 |
| 钨丝灯 (40W) | 2600 |
| 钨丝灯 (100W) | 2800 |
| 卤素灯 | 3000 |
| 暖白荧光灯 | 3000 |
| 冷白荧光灯 | 4000 |
| 日光荧光灯 | 5000-6500 |
| LED 暖白 | 2700-3000 |
| LED 中性白 | 4000-4500 |
| LED 日光 | 5000-6500 |
| 影视灯光 (钨丝) | 3200 |
| HMI 灯 | 5600-6000 |
| 闪光灯 | 5500-6000 |

## 四、UE 灯光参数详解

### DirectionalLight (方向光)

```
Intensity: 150000 lux (晴天)
Light Color: #FFFFFF (6500K)
Source Angle: 0.5 (晴天张角)
Soft Source Radius: 0 (硬阴影) - 10+ (柔和)
```

### PointLight (点光源)

```
Intensity Unit: Lumens 或 Candelas
Attenuation Radius: 根据场景设置
Source Radius: 0 (点光源) - 更大 (面光源)
Soft Source Radius: 控制阴影柔和度
Use Inverse Squared Falloff: True (物理衰减)
```

### SpotLight (聚光灯)

```
Intensity Unit: Lumens 或 Candelas
Inner Cone Angle: 聚光核心角度
Outer Cone Angle: 聚光扩散角度
Source Radius: 光源尺寸
```

### SkyLight (天光)

```
Source Type: SLS_CapturedScene 或 SLS_SpecifiedCubemap
Cubemap: HDRI 贴图
Intensity: 通常不设置，由 HDRI 决定
```

## 五、太阳位置计算

### 太阳角度参数

| 参数 | 晴天 | 多云 | 阴天 |
|------|------|------|------|
| Source Angle | 0.5° | 1-3° | 5-10° |
| 阴影边缘 | 锐利 | 柔和 | 极柔和 |

### 地理位置影响

```
使用 Sun Position Calculator 插件:
- Latitude: 纬度
- Longitude: 经度
- Time Zone: 时区
- Date: 日期
- Time: 时间

自动计算:
- 太阳方向
- 太阳高度角
- 太阳方位角
```

## 六、Pre-Exposure 设置

### UE Project Settings

```
Engine → Rendering → Post Processing:
  ☑ Apply Pre-Exposure before writing to the scene color
  
  作用: 将高于 65504 的亮度 Remap 到 16bit 范围
  必要性: EV > 15 时必须开启
```

### 自动曝光扩展

```
Engine → Rendering → Post Processing:
  ☑ Extend default luminance range in Auto Exposure settings
  
  作用: 扩展自动曝光适应的动态范围
  必要性: EV > 4 时需要开启
```

## 七、IES 文件使用

### IES 文件内容

IES 光源描述文件包含:
- 光源强度 (可选)
- 光强分布 (方向性)
- 衰减曲线

### UE 中使用

```
1. 导入 IES 文件到项目
2. 在灯光组件中:
   - IES Texture: 选择 IES 文件
   - Use IES Intensity: True (使用文件中的强度)
   - IES Intensity Scale: 1.0 (缩放系数)
```

### 注意事项

- IES 不包含色温信息
- IES 不包含光源尺寸信息
- 需要手动设置色温和 Source Radius
