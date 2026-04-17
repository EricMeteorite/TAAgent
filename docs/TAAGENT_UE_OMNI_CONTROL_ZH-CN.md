# TAAgent Unreal 全能控制改造说明

## 目标

让 TAAgent 不只会“按资产路径调用某个专用工具”，而是先理解 Unreal Editor 当前上下文，再基于自然语言去操作用户正在看的窗口、资产和关卡。

这次改造补的是这条主链最关键的一层：

- 读取当前 Editor 上下文
- 读取当前打开的资产编辑器
- 读取当前选中的资产和 Actor
- 打开资产编辑器
- 聚焦已有资产编辑器
- 关闭资产编辑器
- 保存资产

这些能力是 Blueprint、Material、Niagara、DataTable、Widget、StateTree 之类“按当前窗口继续深度操作”的基础。

另外，这次把 Blueprint 变量生命周期也补齐到了插件侧：除了创建和改属性，现在还支持直接删除成员变量，适合清理自动化过程中产生的重复 `_0` 残留变量。

## 这次已经补上的能力

### Unreal 插件命令

新增命令：

- `get_editor_context`
- `get_open_asset_editors`
- `get_selected_assets`
- `get_selected_actors`
- `open_asset`
- `focus_asset_editor`
- `close_asset_editors`
- `save_asset`
- `delete_variable`

它们位于 Unreal 插件的 Editor Commands 中：

- `plugins/unreal/UnrealMCP/RenderingMCP/Plugins/UnrealMCP/Source/UnrealMCP/Public/Commands/EpicUnrealMCPEditorCommands.h`
- `plugins/unreal/UnrealMCP/RenderingMCP/Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/EpicUnrealMCPEditorCommands.cpp`
- `plugins/unreal/UnrealMCP/RenderingMCP/Plugins/UnrealMCP/Source/UnrealMCP/Private/EpicUnrealMCPBridge.cpp`

### Python MCP 工具

新增 Python 工具封装：

- `create_blueprint_variable(blueprint_name, variable_name, variable_type, ...)`
- `delete_blueprint_variable(blueprint_name, variable_name)`
- `set_blueprint_variable_properties(blueprint_name, variable_name, properties={...})`
- `get_editor_context()`
- `get_open_asset_editors()`
- `get_selected_assets()`
- `get_selected_actors()`
- `open_asset(asset_path)`
- `focus_asset_editor(asset_path)`
- `close_asset_editors(asset_path=None, close_all=False)`
- `save_asset(asset_path)`

位于：

- `mcps/unreal_render_mcp/tools/editor.py`
- `mcps/unreal_render_mcp/tools/__init__.py`
- `mcps/unreal_render_mcp/server.py`

## `get_editor_context` 当前会返回什么

`get_editor_context` 的核心价值，是让 TAAgent 从“只会按路径点对点调用”变成“先读当前工作现场，再决定怎么操作”。

当前返回内容包括：

- `current_level`
- `active_global_tab`
- `selected_assets`
- `selected_asset_count`
- `selected_actors`
- `selected_actor_count`
- `open_assets`
- `open_asset_count`
- `open_asset_editors`
- `open_asset_editor_count`
- `active_asset_editor`

其中 `active_asset_editor` 是一个“最佳推断”：

- 优先看全局活动 Tab
- 再结合资产编辑器自己的 TabManager
- 再参考 `GetLastActivationTime()`

它不是“100% 官方唯一真相接口”，但已经比“完全不知道你当前打开了什么”强很多，足够作为自然语言工作流的第一版主上下文。

## 现在能支持的工作方式

### 0. 清理 Blueprint 变量残留

例如：

- 删除自动化误生成的重复变量，例如 `SequenceHarvestEnabled_0`
- 在保留现有图逻辑的前提下清掉未使用的成员变量
- 配合 `create_variable`、`set_blueprint_variable_properties` 完成 Blueprint 变量增删改闭环

`delete_variable` 的输入很直接：

- `blueprint_name`
- `variable_name`

插件内部会调用 `FBlueprintEditorUtils::RemoveMemberVariable`，随后标记 Blueprint dirty、刷新属性面板并重新编译蓝图。

### 1. 读当前正在工作的资产编辑器上下文

例如：

- “看看我现在 UE 里打开了哪些资产编辑器”
- “看看当前焦点大概率在哪个资产窗口上”
- “看看 Content Browser 当前选中了哪些资产”
- “看看当前关卡里我选中了哪些 Actor”

### 2. 让 TAAgent 主动切换到某个资产

例如：

- “打开 `/Game/Foo/Bar.Bar`”
- “把 `/Game/FX/NS_Fire.NS_Fire` 的编辑器切到前台”
- “保存当前这个材质资产”

### 3. 让专用工具链基于当前上下文继续工作

这是最重要的用途。

例如：

- 先用 `get_editor_context` 判断当前活动资产是否是 Blueprint
- 如果是，就走 Blueprint 信息读取 / 更新命令
- 如果是 Material，就走 Material graph 命令
- 如果是 Niagara，就走 Niagara graph / emitter 命令

也就是说，这次补的是“通用感知层”，它会成为后面所有细粒度编辑能力的入口。

## 当前仍然没有完全做到的部分

这部分必须说清楚，不然会把“基础层打通”误以为成“已经全能”。

### 1. 还没有做到“UE 所有窗口都统一深度编辑”

现在已经能：

- 识别当前活动全局 Tab
- 识别打开的资产编辑器
- 识别已打开资产
- 识别当前选择

但还没有做到“所有 Editor 窗口都统一抽象成可读写对象并深度编辑内部结构”。

例如这些还需要继续补专用命令：

- DataTable 行级读写
- StateTree 图与节点修改
- Widget Blueprint 层级树和控件树编辑
- Material Instance 批量参数面板级控制
- Animation Blueprint、Control Rig、Behavior Tree、PCG、MetaSound 等专用编辑器
- 输出日志、控制台变量、PIE、Live Coding、Insights、Build

### 2. 还没有完全达到 `soft-ue-cli` 的全部能力

当前 TAAgent 已有一部分功能覆盖，但离“完全具备隔壁所有功能并更多”还有明确差距。

当前差距主要在：

- Blueprint 图编辑覆盖面
- Widget / StateTree / DataTable / PIE / Logs / CVar
- Build / Live Coding / Insights
- 更强的实例发现和会话管理

### 3. 当前活动窗口识别仍然是 best-effort

UE 并没有一个“所有窗口统一暴露为稳定高层 API”的简单接口。

这次做法是基于：

- `UAssetEditorSubsystem`
- `IAssetEditorInstance`
- `FGlobalTabmanager`
- `SDockTab`

它对资产编辑器上下文已经够用，但对某些非资产窗口仍然需要单独继续补。

## 为什么先补这层，而不是直接一次性补所有编辑器

因为如果不先有通用上下文层，后面每个资产类型都要靠用户手动提供路径，无法形成真正自然语言工作流。

先有这层以后，后续才可以稳定做：

1. 读取“你现在开着什么”
2. 识别资产类型
3. 路由到对应专用编辑命令
4. 修改后保存
5. 继续下一轮上下文感知

这才是“全面自然语言控制 Unreal”的正确架构。

## 接下来建议的补强顺序

建议按下面顺序继续做，不要乱铺功能面。

### 第一阶段：把通用上下文做到更稳

- 增加“打开的非资产窗口”识别
- 增加“当前主要工作资产”的更稳判断
- 增加“当前关卡、当前世界、当前 PIE 状态、当前模式”信息

### 第二阶段：补 `soft-ue-cli` 已有而 TAAgent 还缺的核心能力

- DataTable 读写
- 更完整的 Blueprint 图编辑
- Widget Blueprint 结构操作
- StateTree 操作
- PIE / 日志 / CVar / Live Coding / Build

### 第三阶段：做 TAAgent 自己的增强能力

- 结合当前窗口自动判断任务意图
- 多资产联动修改
- 图编辑 + 视口截图 + 资产检查 + 结果总结的一体化闭环
- 加入 RenderDoc / 视口 / 材质 / Niagara 联动分析

## 当前验证情况

已通过的本地验证：

- `python -m compileall mcps/unreal_render_mcp`
- `.\.venv\Scripts\python.exe -m pytest -q`

当前阻塞的运行时验证：

- Unreal 插件的新 C++ 命令需要重新编译后才能在目标 UE 项目里生效
- 我在尝试直接命令行编译时被 UE 的 Live Coding 阻止了，因此没法在你当前开着编辑器的状态下自动完成这一步

## 使用建议

完成 Unreal 插件重编译后，优先从下面这些自然语言入口开始：

- “读取我当前 UE 的 editor context”
- “看看我现在开了哪些资产编辑器”
- “告诉我当前焦点大概率在哪个资产窗口上”
- “打开 `/Game/...`”
- “保存 `/Game/...`”
- “把当前打开的 Blueprint 详细信息读出来”
- “把当前打开的 Niagara graph 读出来”

后续再继续往各资产类型的专用深度编辑推进。
