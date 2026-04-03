# Niagara 流体模拟原理

本文档整理自 Stable Fluids 论文及 UE Niagara Fluids 实现。

---

## 1. 理论基础

### 1.1 Navier-Stokes 方程

描述不可压缩牛顿流体运动的基本方程：

**动量方程**：
$$\frac{\partial \mathbf{u}}{\partial t} + (\mathbf{u} \cdot \nabla) \mathbf{u} = -\frac{1}{\rho} \nabla p + \nu \nabla^2 \mathbf{u} + \mathbf{f}$$

**连续性方程（不可压缩条件）**：
$$\nabla \cdot \mathbf{u} = 0$$

参数说明：
- **u**：速度场
- **p**：压力
- **ρ**：密度
- **ν**：运动黏性系数
- **f**：外力（如重力）

### 1.2 Stable Fluids 算法 (Jos Stam, 1999)

实时流体模拟的经典方法，核心思想是**算子分裂**：

```
时间步循环:
  1. 外力步 (Add Forces)
  2. 平流步 (Advection) - 半拉格朗日方法
  3. 黏性步 (Diffusion) - 可选
  4. 投影步 (Projection) - 保证不可压缩
```

---

## 2. 投影方法详解

### 2.1 物理意义

投影步保证速度场满足**不可压缩条件**，即流体体积不变（质量守恒）。

**直观理解**：
- 想象一个气球，如果你压缩它，空气会向其他方向流动
- 投影步计算这种"压力"对速度的影响

### 2.2 数学推导

**Step 1**: 假设投影前的速度为 u\*\*\*，目标速度 u^{n+1} 无散度

**Step 2**: 将速度分解：
$$\mathbf{u}^{***} = \mathbf{u}^{n+1} + \frac{\Delta t}{\rho} \nabla p$$

**Step 3**: 对两边求散度，利用 ∇·u^{n+1} = 0：
$$\nabla^2 p = \frac{\rho}{\Delta t} \nabla \cdot \mathbf{u}^{***}$$

这是**压力泊松方程**（Poisson equation for pressure）。

**Step 4**: 求解压力后，更新速度：
$$\mathbf{u}^{n+1} = \mathbf{u}^{***} - \frac{\Delta t}{\rho} \nabla p$$

---

## 3. Niagara Fluids 实现分析

### 3.1 Grid2D_ProjectPressure 模块

**路径**: `/NiagaraFluids/Modules/Grid2D/Grid2D_ProjectPressure`

**功能**: 执行投影步，用压力梯度修正速度场

#### 图结构分析

```
输入参数:
├── VelocityGrid (Grid2D)    - 当前速度场
├── PressureGrid (Grid2D)    - 压力场
├── PressureGradient         - 压力梯度 (∇p)
├── dt                       - 时间步长
├── density                  - 流体密度 (ρ)
├── Velocity                 - 当前速度值
├── Boundary                 - 边界条件
└── SolidVelocity            - 固体速度（边界交互）

计算流程:
  1. PressureGradient * dt / density
  2. Velocity - 上一步结果
  3. CustomHlsl: 处理边界条件

输出:
└── 更新后的 Velocity
```

#### 核心公式实现

```
Velocity_new = Velocity - (PressureGradient * dt / density)
```

这正是投影公式：$\mathbf{u}^{n+1} = \mathbf{u} - \frac{\Delta t}{\rho} \nabla p$


---

## 4. 流体模拟完整流程

### 4.1 标准管线

```
         ┌──────────────────────────────────────────┐
         │           Navier-Stokes Solver           │
         └──────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        ▼                     ▼                     ▼
   ┌─────────┐          ┌─────────┐          ┌─────────┐
   │ 外力步  │    →     │ 平流步  │    →     │ 投影步  │
   │ (Force) │          │(Advect) │          │(Project)│
   └─────────┘          └─────────┘          └─────────┘
        │                     │                     │
        ▼                     ▼                     ▼
   u += f*dt            u = advect(u)         u -= ∇p*dt/ρ
                                               求解 ∇²p = ρ∇·u/dt
```

### 4.2 Niagara Fluids 模块映射

| 步骤 | Niagara 模块 | 说明 |
|------|-------------|------|
| 平流 | Grid2D_Advect | 半拉格朗日平流 |
| 外力 | Grid2D_AddForce | 添加重力/浮力 |
| 散度 | Grid2D_ComputeDivergence | 计算 ∇·u |
| 压力求解 | Grid2D_SolvePressure | Jacobi 迭代 |
| **投影** | **Grid2D_ProjectPressure** | **速度修正** |
| 边界 | Grid2D_ApplyBoundary | 边界条件 |

---

## 5. 边界条件

### 5.1 固体边界

**无滑移条件**：流体速度等于固体速度
$$\mathbf{u}_{boundary} = \mathbf{u}_{solid}$$

**压力边界**：法向压力梯度为零
$$\frac{\partial p}{\partial n} = 0$$

### 5.2 自由表面

**压力条件**：自由表面压力为大气压（通常设为 0）
$$p = 0 \quad \text{(at free surface)}$$

---

## 6. 数值方法

### 6.1 压力泊松方程求解

通常使用 **Jacobi 迭代** 或 **Gauss-Seidel** 方法：

```hlsl
// Jacobi 迭代
float p_new = (p_left + p_right + p_up + p_down - divergence * dx * dx) / 4.0;
```

迭代次数通常 20-80 次，取决于精度需求。

### 6.2 半拉格朗日平流

追踪粒子路径，从"上游"采样：

```
1. 计算回溯位置: x_back = x - u * dt
2. 采样该位置的值
3. 插值得到新值
```

优点：无条件稳定，无 CFL 限制。

---

## 7. 性能优化

### 7.1 GPU 实现

- Grid2D 使用 Texture 或 RWBuffer
- 每个线程处理一个网格单元
- 使用 Compute Shader

### 7.2 迭代次数

| 分辨率 | 推荐 Jacobi 迭代 |
|--------|-----------------|
| 64x64 | 20-40 次 |
| 128x128 | 40-60 次 |
| 256x256 | 60-80 次 |

---

## 8. 参考资源

### 经典论文

1. **Stable Fluids** (Jos Stam, 1999) - 实时流体模拟基础
2. **Real-Time Fluid Dynamics for Games** (Jos Stam, 2003) - 简化版教程
3. **Fluid Simulation for Computer Graphics** (Robert Bridson, 2008) - 经典教材

### GPU 实现

- GPU Gems 3, Chapter 30: Real-Time Simulation and Rendering of 3D Fluids
- NVIDIA FleX SDK

### UE 资源

- Niagara Fluids 插件: `Engine/Plugins/FX/NiagaraFluids/`
- 示例系统: `/NiagaraFluids/Systems/`
