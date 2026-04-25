# TAAgent 独立部署与使用手册

本文档的目标只有一个: 把 TAAgent 部署成只依赖当前仓库目录的本地工作区。

这套方案遵守以下原则:

- 所有 Python 依赖都安装在 TAAgent/.taagent-local/.venv 里。
- 所有 pip 缓存、临时文件、IPC 文件、运行日志都写在 TAAgent/.taagent-local 里。
- 不修改系统 Python。
- 不往系统级 MCP 配置里自动写入任何内容。
- 不往系统 AppData 安装 RenderDoc 扩展。
- 你删除整个 TAAgent 文件夹后，TAAgent 自己留下的内容也会一起消失。

需要提前说明的边界:

- 本项目不是一个纯离线小工具，它依赖外部宿主程序: RenderDoc 和 Unreal Engine。
- 这两个程序本身必须已经存在，或者由你另外准备。TAAgent 无法把完整的 RenderDoc/Unreal Engine 也打包进当前仓库。
- 但是，TAAgent 对这两个外部程序的接入方式已经被改成无污染模式: 不往系统目录安装扩展，不写系统级配置，只在当前仓库下保存自己的运行状态。

## 1. 这个项目到底是什么

TAAgent 不是单一程序，而是三块东西组合起来:

- RenderDoc MCP: 负责分析 .rdc 捕获、查 draw call、导出纹理和 mesh。
- Unreal Render MCP: 负责通过 Unreal 项目里的插件，和 UE 编辑器通信。
- Unreal CLI Harness: 负责用 UE Python Console 做文件式通信，适合命令行控制 UE。

对应目录:

- RenderDoc MCP: mcps/renderdoc_mcp
- Unreal MCP: mcps/unreal_render_mcp
- RenderDoc 扩展: src/extension
- Unreal 项目与插件: plugins/unreal/UnrealMCP/RenderingMCP
- Unreal CLI: unreal/agent-harness
- 本地部署脚本: tools

## 2. 独立部署后的目录布局

首次部署完成后，会新增这个目录:

- .taagent-local

里面主要是:

- .taagent-local/.venv: 本地 Python 虚拟环境
- .taagent-local/pip-cache: 本地 pip 缓存
- .taagent-local/tmp: 本地临时目录
- .taagent-local/tmp/ue_cli: Unreal CLI 通信目录
- .taagent-local/ipc/renderdoc: RenderDoc MCP 通信目录
- .taagent-local/appdata: RenderDoc 本地 profile
- .taagent-local/config/mcp_config.local.json: 本地 MCP 配置示例

## 3. 第一次部署

### 3.0 推荐方式: 双击 TAAgent.bat

仓库根目录有一个 TAAgent.bat，双击打开后会显示一个菜单，所有操作都可以通过输入编号完成:

- 首次安装环境
- 部署/卸载 UE 插件
- 启动 MCP 服务
- 生成 UE Python Console 代码
- 启动 RenderDoc
- 直接触发当前 RenderDoc Live Capture 抓帧
- 设置路径
- 完全卸载

你设置过的 UE 项目路径和 RenderDoc 路径会自动保存，下次打开不用重新输入。

如果你更习惯命令行，下面是对应的 PowerShell 命令。

### 3.1 命令行方式

在 PowerShell 里进入仓库根目录后运行:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\setup_local.ps1
```

这个脚本会做下面这些事:

- 在当前仓库下创建本地运行目录
- 创建本地虚拟环境
- 安装项目主依赖
- 安装 Unreal MCP 依赖
- 安装 Unreal CLI 命令行包
- 生成本地 MCP 配置文件

如果脚本报错 “未找到可用的 Python 3.10+”，先安装 Python 3.10 或更高版本，然后重新执行。

## 4. RenderDoc 模块怎么用

### 4.1 启动 RenderDoc 本地隔离模式

如果你的 RenderDoc 在默认路径:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\open_renderdoc_local.ps1
```

如果不在默认路径:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\open_renderdoc_local.ps1 -RenderDocExe "C:\你的路径\qrenderdoc.exe"
```

这个脚本会:

- 把扩展安装到 TAAgent/.taagent-local/appdata/qrenderdoc/extensions
- 用 TAAgent 自己的本地 Temp 和 IPC 启动 RenderDoc
- 让 RenderDoc 和 MCP 通过 TAAgent/.taagent-local/ipc/renderdoc 通信

Windows 下需要额外注意:

- RenderDoc 1.43 的部分配置/扩展发现仍然会落到系统 AppData
- tools/open_renderdoc_local.ps1 会把扩展镜像到系统 qrenderdoc/extensions，避免 UI 里看不到扩展
- tools/open_renderdoc_local.ps1 也会把 renderdoc_mcp_bridge 写入系统 UI.config 的 AlwaysLoad_Extensions，避免每次重启后默认不加载
- TAAgent 仍然会把 Temp 和 RenderDoc MCP IPC 固定到 .taagent-local 下

### 4.2 启动 RenderDoc MCP 服务

另开一个 PowerShell 窗口，执行:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\start_renderdoc_mcp.ps1
```

这个窗口需要保持打开。

### 4.2.1 直接触发当前 Live Capture 抓帧

如果你已经在 RenderDoc 里连上了目标进程的 Live Capture 窗口，并且窗口状态显示 Established，可以直接执行:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\trigger_renderdoc_live_capture.ps1
```

或者直接在 TAAgent.bat 菜单里使用 [10]。

这个入口的特点:

- 不依赖 RenderDoc MCP 服务窗口是否正在运行
- 会先检查当前 Live Capture 按钮是否存在、可见、可点击
- 只有在连接状态为 Established 时才会真正发送抓帧指令
- 失败时会明确提示是没开 Live Capture 窗口、按钮不可用，还是当前 RenderDoc 需要重启加载新扩展

### 4.3 验证 RenderDoc 是否准备好

你需要在 RenderDoc 里:

- 确认扩展被加载
- 打开一个 .rdc capture

随后，MCP 客户端就可以调用这些能力:

- 获取 capture 状态
- 列 draw calls
- 查 shader
- 读取纹理
- 导出 mesh

## 5. Unreal 模块怎么用

Unreal 有两条路:

- MCP 插件模式: 适合 Unreal Render MCP
- Python Console 监听器模式: 适合 Unreal CLI

两者可以并存。

### 5.1 把插件部署到你自己的 UE 项目

TAAgent 仓库自带了一个演示项目 (plugins/unreal/UnrealMCP/RenderingMCP)，但更常见的做法是在你自己的项目里使用。

部署命令:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\deploy_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"
```

这个脚本做的事情:

- 把 TAAgent 自带的项目级插件复制到你项目的 Plugins/ 下
- 当前默认包括: `UnrealMCP` 和 `AssetValidation`
- 只复制源码和配置文件，不复制编译产物
- 不修改你的 .uproject 文件
- 不修改引擎目录的任何内容

部署后的影响范围 (仅限你的项目目录):

- 新增: YourProject/Plugins/UnrealMCP/ 和可能的其他 TAAgent 插件目录
- UE 首次打开时自动编译，会在该目录下产生 Binaries/ 和 Intermediate/

不会被影响的东西:

- 你的引擎源码目录 (包括 UnrealEngine56/Engine/ 下的所有文件)
- 你项目原有的 Source、Content、Config 等目录
- 系统环境变量
- 其他任何项目

### 5.1.1 从你的项目中卸载插件

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"
```

如果 UE 编辑器还开着，也可以让脚本先关闭编辑器再卸载:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径" -ForceCloseEditor
```

或者直接手动删除对应的 TAAgent 插件目录，例如: YourProject/Plugins/UnrealMCP/、YourProject/Plugins/AssetValidation/。

卸载后你的项目恢复到之前的状态，不会有任何残留。

如果 UE 编辑器曾在你的 .uproject 文件的 Plugins 数组里自动添加了相关条目 (取决于你是否在编辑器里手动启用/禁用过)，你也可以手动删除那些 TAAgent 插件条目。如果你从未通过编辑器插件管理界面操作过，.uproject 通常不会被修改。

### 5.1.2 使用仓库自带的演示项目 (可选)

如果你不想在自己的项目里安装插件，也可以直接用仓库里的演示项目:

- plugins/unreal/UnrealMCP/RenderingMCP/RenderingMCP.uproject

这个项目已经自带插件，打开即用。演示项目标记的引擎版本是 5.7。

如果你需要 Niagara 静态验证和 Quad Overdraw 分析，新拉下来的 `AssetValidation` 插件也会一起随项目提供。

### 5.2 启动 Unreal MCP 服务

在 PowerShell 里执行:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\start_unreal_mcp.ps1
```

默认连接:

- Host: 127.0.0.1
- Port: 55557

这个端口由 Unreal 项目里的插件监听。

如果你改了端口，也可以这样启动:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\start_unreal_mcp.ps1 -Host 127.0.0.1 -Port 55557
```

这个窗口也需要保持打开。

### 5.3 启动 Unreal CLI 监听器

进入 UE 编辑器后:

- 打开 Window -> Developer Tools -> Python Console
- 在 PowerShell 里先输出可直接粘贴的脚本:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\show_unreal_python_console_snippet.ps1
```

- 把输出的整段 Python 代码完整复制到 UE 的 Python Console 中执行

这段代码会做两件事:

- 把 UE CLI 通信目录设为 TAAgent/.taagent-local/tmp/ue_cli
- 加载仓库里的 ue_cli_listener_full_auto.py，并自动开始监听

这样 Unreal CLI 的通信文件也不会写到系统 Temp。

## 6. 如何接到 MCP 客户端里

本地配置文件会自动生成到这里:

- .taagent-local/config/mcp_config.local.json

这个文件不会自动写入你的客户端，只是供你复制内容使用。

如果你的 MCP 客户端支持自定义 server config，把里面的内容拷进去即可。

它的逻辑是:

- RenderDoc MCP 通过 powershell 调 tools/start_renderdoc_mcp.ps1
- Unreal MCP 通过 powershell 调 tools/start_unreal_mcp.ps1

## 7. 推荐使用顺序

如果你主要做 RenderDoc 分析:

1. 运行 tools/setup_local.ps1
2. 运行 tools/open_renderdoc_local.ps1
3. 运行 tools/start_renderdoc_mcp.ps1
4. 在 RenderDoc 里打开 capture
5. 在你的 MCP 客户端里连接 RenderDoc server

如果你主要做 Unreal 操作 (用自己的项目):

1. 运行 tools/setup_local.ps1
2. 运行 tools/deploy_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"
3. 用你的引擎打开项目，等 UE 编译插件并进入编辑器
4. 运行 tools/start_unreal_mcp.ps1
5. 把 tools/show_unreal_python_console_snippet.ps1 输出的内容粘贴到 UE Python Console
6. 在 MCP 客户端里连接 unreal-render server

如果你主要做 Unreal 操作 (用仓库自带的演示项目):

1. 运行 tools/setup_local.ps1
2. 打开 plugins/unreal/UnrealMCP/RenderingMCP/RenderingMCP.uproject
3. 等 UE 编译并进入编辑器
4. 运行 tools/start_unreal_mcp.ps1
5. 把 tools/show_unreal_python_console_snippet.ps1 输出的内容粘贴到 UE Python Console
6. 在 MCP 客户端里连接 unreal-render server

如果你两个都要用:

1. 先做一次 tools/setup_local.ps1
2. RenderDoc 和 Unreal 分别按上面流程启动
3. 在 MCP 客户端里同时加载两个 server

## 8. 常见问题

### 8.1 我删掉整个 TAAgent 文件夹，会残留什么吗

按当前这套部署方案，TAAgent 自己生成的内容都在当前仓库里。

删除 TAAgent 文件夹后，以下内容会一起消失:

- 虚拟环境
- pip 缓存
- RenderDoc 本地 profile
- Unreal CLI 本地临时文件
- MCP 本地配置
- 本地日志

但如果你用了 deploy_ue_plugin.ps1 把插件复制到了你自己的 UE 项目，那份拷贝在你项目的 Plugins/UnrealMCP/ 里，不会因为删除 TAAgent 而消失。你需要单独处理:

- 在删除 TAAgent 之前先运行 tools/remove_ue_plugin.ps1
- 或手动删除 YourProject/Plugins/UnrealMCP/ 文件夹

TAAgent 不会额外卸载或删除你系统里原本就安装好的 RenderDoc 或 Unreal Engine，因为它们本来就不是 TAAgent 安装的。

### 8.2 为什么还需要外部安装好的 RenderDoc 和 Unreal

因为这个项目本质上是它们的桥接层和自动化层，不是替代品。

TAAgent 自己可以做到独立，但宿主软件仍然要存在。

### 8.3 如果 MCP 连不上 Unreal

先检查:

- UE 项目是否已经打开
- 插件是否已成功编译
- UE 是否监听了 127.0.0.1:55557
- tools/start_unreal_mcp.ps1 对应窗口有没有报错

### 8.4 如果 RenderDoc MCP 连不上

先检查:

- RenderDoc 是否通过 tools/open_renderdoc_local.ps1 启动
- Windows 下 Tools > Manage Extensions 里是否能看到 RenderDoc MCP Bridge
- 扩展是否已经在 RenderDoc 中加载
- RenderDoc 是否已经打开一个 capture
- tools/start_renderdoc_mcp.ps1 对应窗口有没有报错

### 8.5 插件会不会修改我的引擎源码

不会。UnrealMCP 是一个纯项目级 Editor 插件，它:

- 只放在你项目的 Plugins/ 目录下
- 编译产物 (Binaries/、Intermediate/) 也在插件自己的目录里
- 不修改引擎目录的任何文件
- 不修改你项目的 Source/、Content/、Config/ 下的任何已有文件
- 不注册系统服务、不写注册表

插件运行时做的事:

- 在 127.0.0.1:55557 监听 TCP 连接 (仅本机回环，外部无法访问)
- 接收 MCP 客户端的指令后操作编辑器 (创建材质、导入资产等)

这些操作是你主动发起的，不是插件自动执行的。

### 8.6 我用完了想彻底卸载怎么做

完整卸载步骤:

1. 关闭 UE 编辑器
2. 从你项目中移除插件:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"
```

如果你不想手动先关编辑器，也可以改用:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径" -ForceCloseEditor
```

3. 删除 TAAgent 整个文件夹

之后你的系统、引擎、项目都恢复到安装前的状态。

## 9. 你现在最应该执行的命令

按顺序执行下面三类命令:

### 9.1 先完成本地部署

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\setup_local.ps1
```

### 9.2 如果你先要用 RenderDoc

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\open_renderdoc_local.ps1
powershell -ExecutionPolicy Bypass -File .\tools\start_renderdoc_mcp.ps1
```

### 9.3 如果你先要用 Unreal (你自己的项目)

```powershell
# 部署插件到你的项目 (只需要做一次)
powershell -ExecutionPolicy Bypass -File .\tools\deploy_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"

# 用你的引擎打开项目后，启动 MCP 服务
powershell -ExecutionPolicy Bypass -File .\tools\start_unreal_mcp.ps1

# 输出 UE Python Console 粘贴用的代码
powershell -ExecutionPolicy Bypass -File .\tools\show_unreal_python_console_snippet.ps1
```

最后一条命令会输出一段 Python，把它粘贴到 UE Python Console 执行。

### 9.4 用完后卸载

```powershell
# 从你的项目中移除插件
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径"

# 如果 UE 还开着，可让脚本自动关闭编辑器后再卸载
powershell -ExecutionPolicy Bypass -File .\tools\remove_ue_plugin.ps1 -ProjectDir "D:\你的项目路径" -ForceCloseEditor

# 然后删除 TAAgent 整个文件夹即可
```