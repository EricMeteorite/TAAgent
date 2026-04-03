# TAAgent Niagara 烘焙说明

## 目标

让 TAAgent 能执行这样的自然语言任务：

- 给 `xxx` 这个 Niagara System 做一次烘焙
- 把烘焙好的序列帧放到这个 Niagara 资产同目录
- 保持当前看到的最终颜色效果
- 输出可直接用于播放的带透明度序列帧

当前实现对应的底层命令是 `bake_niagara_system`。

## 当前实现方式

这次实现没有去改目标项目里的 Niagara 资产本身，也不要求手工复制一个 bake 版本资产。命令会在编辑器里直接调用 Unreal 自带的 Niagara Baker 渲染链路，按每一帧执行两次捕获：

1. `Beauty Pass`
   - 使用 `ESceneCaptureSource::SCS_FinalToneCurveHDR`
   - 这一遍负责拿到和 Niagara Baker 面板里 `Final Color (with tone curve)` 一致的最终观感 RGB

2. `Alpha Pass`
   - 使用 `ESceneCaptureSource::SCS_SceneColorHDR`
   - Unreal 的 Niagara Baker 自己会把这一路的 `Inv Opacity` 反相成可用 alpha

3. `Merge`
   - 最终输出使用 `Beauty.RGB + Alpha.A`
   - 默认导出为 `PNG`

这条路的目的很直接：保住最终观感，同时拿到可用透明度。

## 默认行为

如果你只告诉 TAAgent：

- “给当前打开的 Niagara 做一次烘焙”
- “给 `NS_Map_Smoke_03` 做一次烘焙”

默认行为是：

1. 优先使用你当前活动/已打开的 Niagara 资产
2. 默认把序列帧输出到该 Niagara 资产所在磁盘目录
3. 默认输出前缀为资产名
4. 默认复用该 Niagara 当前 Baker 设置里的：
   - `StartSeconds`
   - `DurationSeconds`
   - `FramesPerSecond`
   - `FramesPerDimension`
   - `bRenderComponentOnly`
5. 如果当前 Baker 没有显式帧尺寸配置，则回退到 `512x512`

## 输出路径规则

假设资产路径是：

`/Game/VFX/Niagara/Systems/NS_Test_Smoke_01.NS_Test_Smoke_01`

默认会落到类似：

`<Project>/Content/VFX/Niagara/Systems/`

文件名格式默认是：

`NS_Test_Smoke_01_0000.png`
`NS_Test_Smoke_01_0001.png`
`NS_Test_Smoke_01_0002.png`

你也可以显式指定：

- `output_dir`
- `output_prefix`
- `file_extension`

## Python 调用示例

```python
from mcps.unreal_render_mcp.tools.niagara import bake_niagara_system

result = bake_niagara_system(
   asset_path="/Game/VFX/Niagara/Systems/NS_Test_Smoke_01.NS_Test_Smoke_01"
)
print(result)
```

使用当前打开的 Niagara：

```python
from mcps.unreal_render_mcp.tools.niagara import bake_niagara_system

result = bake_niagara_system()
print(result)
```

指定输出目录和尺寸：

```python
from mcps.unreal_render_mcp.tools.niagara import bake_niagara_system

result = bake_niagara_system(
   asset_path="/Game/VFX/Niagara/Systems/NS_Test_Smoke_01.NS_Test_Smoke_01",
   output_dir=r"<output-dir>/NiagaraBake",
   output_prefix="NS_Test_Smoke_01_Bake",
    frame_width=1024,
    frame_height=1024,
    file_extension=".png"
)
print(result)
```

## MCP / 自然语言使用方式

有了这个命令之后，TAAgent 可以把自然语言翻译成同一类操作。例如：

- 给当前打开的 Niagara 做一次烘焙，把序列帧放在资产同目录
- 给 `NS_Test_Smoke_01` 做一次 1024 分辨率烘焙
- 给 `NS_Test_Smoke_01` 做一次烘焙，输出前缀改成 `NS_Test_Smoke_01_Bake`
- 给这个 Niagara 重新烘焙，用当前 Baker 设置，覆盖同目录旧序列

注意：

- `asset_name` 只会在当前已打开的 Niagara 资产里做匹配
- 如果你给的是完整资产路径，优先使用资产路径

## 命令参数

`bake_niagara_system` 支持这些参数：

- `asset_path`
- `asset_name`
- `output_dir`
- `output_prefix`
- `frame_width`
- `frame_height`
- `start_seconds`
- `duration_seconds`
- `frames_per_second`
- `frames_x`
- `frames_y`
- `render_component_only`
- `file_extension`

其中最常用的是：

- `asset_path`
- `output_dir`
- `frame_width`
- `frame_height`

## 结果结构

命令成功后会返回：

- `asset_path`
- `asset_name`
- `output_dir`
- `output_prefix`
- `file_extension`
- `frame_width`
- `frame_height`
- `frame_count`
- `frames_x`
- `frames_y`
- `frames_per_second`
- `start_seconds`
- `duration_seconds`
- `render_component_only`
- `files`

`files` 是实际导出的序列帧绝对路径数组。

## 与原始资产的关系

这次实现的目标是“执行烘焙”，不是“改资产内容”。

命令执行时会临时读取并覆盖内存中的 Baker 设置用于本次烘焙，但完成后会恢复原值，不会主动保存 Niagara 资产，不会把资产标脏作为默认行为。

## 当前边界

这条命令已经足够支撑“自然语言触发一次正确的 Niagara 序列帧烘焙”，但有几个边界要明确：

1. 它依赖 Unreal 插件已重新部署并重新编译
2. 它依赖当前编辑器里的 Niagara 能被正常加载
3. 它默认解决的是 `Final Color` 与 `Alpha` 分离的问题，不是自动替你决定最佳镜头构图
4. 如果某些效果强依赖场景内容，`render_component_only` 可能需要设为 `false`

## 部署要求

因为这次新增的是 C++ 插件命令，所以使用前需要：

1. 从 `TAAgent` 重新部署 `UnrealMCP` 插件到你的 UE 项目
2. 重新编译项目
3. 打开 UE 项目
4. 启动 TAAgent 的 Unreal MCP

如果只是继续使用旧的文件监听 Python 方案，这个新命令本身并不依赖那条 Python Console snippet。
