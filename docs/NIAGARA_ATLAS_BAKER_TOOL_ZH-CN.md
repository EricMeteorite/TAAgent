# Niagara Atlas Baker Tool

位置：

plugins/unreal/NiagaraAtlasBakerTool

这是一个独立的 Unreal Editor 插件包，用来把 Niagara System 一键烘焙成 atlas，并直接导入为 Unreal 可查看的贴图资产。

## 设计目标

- 不修改现有 `TAAgent`
- 不修改目标项目现有代码结构
- 不修改 Unreal Engine
- 不依赖 Python / Pillow / MCP / 系统环境变量
- 即插即用
- 随时可卸载
- 面向美术直接操作

## 交付内容

插件包包含：

- `.uplugin`
- 独立 `Editor` 模块
- 一个可停靠的工具窗口
- Niagara atlas 烘焙实现
- 贴图导入实现
- 中文使用说明

## 使用方式

1. 复制插件到目标项目：

`<Project>/Plugins/NiagaraAtlasBakerTool`

2. 打开项目并编译插件。

3. 进入：

`Window -> Niagara Atlas Baker`

4. 选择 Niagara，设置尺寸与参数，点击 `Bake Atlas`。

## 输出结果

工具会直接生成：

- 磁盘上的 atlas PNG
- UE 里的 `Texture2D` atlas 资产

默认放在源 Niagara 所在目录。

## 当前实现范围

这个插件针对“把 Niagara 烘成可直接查看的 atlas”这个任务做了封装，重点是稳定和干净，不包含 TAAgent 的其他控制能力。

如果后续要继续扩展，可以在这个独立插件上再加：

- 自动创建 Flipbook 材质
- 自动创建材质实例
- 自动创建 Niagara 播放模板
- 批量烘焙多个 Niagara
