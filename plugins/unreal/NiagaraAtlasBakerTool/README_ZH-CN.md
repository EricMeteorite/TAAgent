# Niagara Atlas Baker Tool

一个独立的 Unreal Editor 插件，给美术直接使用。

它的目标很单一：

- 选择一个 `Niagara System`
- 设置单帧分辨率和 atlas 布局/时间参数
- 点击 `Bake Atlas`
- 直接输出一张带透明度的 atlas PNG
- 可选地自动导入为 Unreal `Texture2D` 资产并自动打开查看

这个插件不依赖：

- TAAgent MCP
- Python
- Pillow
- 外部安装器
- 系统环境变量

它只在 Unreal Editor 内工作，删除插件目录即可完整卸载。

## 安装

1. 把整个目录复制到你的项目插件目录：

`<YourProject>/Plugins/NiagaraAtlasBakerTool`

2. 打开项目。

3. 如果提示编译插件，选择编译。

4. 进入菜单：

`Window -> Niagara Atlas Baker`

## 使用

1. 在 Content Browser 选中一个 `Niagara System`，或者先打开该 Niagara。
2. 打开 `Window -> Niagara Atlas Baker`。
3. 点击 `Use Selected/Open Niagara`，自动带入当前 Niagara。
4. 如果你需要调整 atlas 里特效的放大缩小、取景范围或构图，不是在这个窗口里直接拖，而是点 `Open Niagara Editor`，到 Niagara 原生 Baker 预览里调整相机/缩放。
5. 回到这个窗口，设置：
   - `FrameWidth / FrameHeight`
   - 是否沿用 Niagara 自带 Baker 时间
   - 是否沿用 Niagara 自带 atlas 布局
   - 输出目录
   - 是否导入回项目
   - 导入资产路径和资产名
   - `Import Optimization` 下的压缩模式、Mip 和 Streaming 策略
6. 点击 `Bake Atlas`。

完成后会得到：

- 一张 atlas PNG 文件
- 一个可直接在 UE 中打开查看的 `Texture2D` 资产（如果勾选导入）

## 默认行为

- 默认输出到源 Niagara 资产所在磁盘目录
- 默认导入到源 Niagara 资产所在 Unreal 目录
- 默认 atlas 资产名：`T_<NiagaraName>_Atlas`
- 默认 atlas 文件名：`<NiagaraName>_Atlas.png`
- 默认打开导入后的 atlas 贴图
- atlas 的取景、缩放和构图跟随 Niagara 原生 Baker 当前相机设置
- 默认使用平台压缩格式导入 atlas，并生成 mip、允许 streaming，整体行为更接近常规 DXT 风格贴图

## 导入优化

- `Compression Mode = Platform Default Compressed`：默认选项，优先走项目常规压缩链路，通常会得到接近 DXT5 的资源形态。
- `Compression Mode = High Quality BC7 (Desktop)`：桌面平台更高质量的压缩方案，显存占用和 DXT5 同级，但边缘和渐变通常更稳。
- `Compression Mode = Lossless BGRA8`：无损兜底模式，保留未压缩导入，适合排查压缩伪影或处理极端敏感的特效 atlas。
- `Generate Mipmaps`：开启后 atlas 会生成 mip，通常更省运行时显存，也能参与 streaming；如果远处出现帧串色，可以关掉再试。
- `Allow Texture Streaming`：仅在生成 mip 时生效，让 atlas 进入纹理流送池，减少常驻内存压力。

## 卸载

1. 关闭 Unreal Editor。
2. 删除项目里的：

`<YourProject>/Plugins/NiagaraAtlasBakerTool`

如果还想删掉生成内容，再手动删除：

- 生成的 atlas PNG
- 导入出的 atlas `Texture2D`

插件本身不会修改引擎，也不会往系统里安装任何东西。

## 说明

当前实现直接在插件内部完成：

- Niagara 预览渲染
- Beauty + Alpha 合成
- atlas 拼图
- PNG 导出
- `Texture2D` 资产导入

不会生成中间序列帧，不会依赖项目里的 Python 工具链。
