# TA Agent Workspace

**AI-Powered Technical Artist** - 一个具备专业 TA 能力的智能代理。

## 核心理念

TA Agent 不只是被动响应工具调用，而是具备：
- **TA 领域知识**（材质、渲染、性能、管线）
- **问题分解能力**（分析→诊断→方案→执行）
- **工具编排能力**（选择合适的 MCP 工具组合）
- **学习迭代能力**（从结果中学习）


## 架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            TA Agent Core                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                   │
│  │   Persona    │  │  Knowledge   │  │    Skills    │                   │
│  │   TA 身份    │  │  领域知识     │  │  工作流 SOP  │                   │
│  └──────────────┘  └──────────────┘  └──────────────┘                   │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
         ┌──────────────────────┼──────────────────────┐
         ▼                      ▼                      ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  Analysis MCP   │    │   Creation MCP  │    │  Validation MCP │
│  分析类工具      │    │   创作类工具     │    │   验证类工具     │
├─────────────────┤    ├─────────────────┤    ├─────────────────┤
│ • RenderDoc     │    │ • Unreal Engine │    │ • Lookdev       │
│ • PIX (未来)    │    │ • Unity (未来)  │    │ • Performance   │
│ • Nsight (未来) │    │ • Blender (未来)│    │ • Quality Check │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

---

## 项目结构

```
TA Agent
|
├── mcps/                         # MCP Server
│   ├── renderdoc_mcp/            # RenderDoc 分析 MCP
│   │   ├── mcp_server/           # 服务端实现
│   │   └── README.md
│   │
│   └── unreal_render_mcp/        # UE 创作类 MCP
│       ├── server.py             # 主入口
│       ├── handlers/             # 请求处理器
│       └── tools/                # 工具实现
│
├── src/extension/                # RenderDoc Python 扩展
│
├── docs/                         # 用户文档与技术参考
│   ├── DEPLOYMENT_ZH-CN.md       # 独立部署与使用手册
│   └── reference/                # 从旧工作流迁移出的项目参考资料
│
├── plugins/unreal/               # UE C++ 插件
│   └── UnrealMCP/RenderingMCP/   # UE 项目
│       └── Plugins/
│           ├── UnrealMCP/        # MCP 插件源码
│           └── AssetValidation/  # Niagara 验证与 Overdraw 分析插件
│
├── config/                       # 配置模板
├── unreal/                       # Unreal CLI harness 与辅助脚本
├── .taagent-local/               # 本地自包含运行时（生成）
└── tools/                        # 辅助工具
```

---

## MCP 概览

### RenderDoc MCP - 分析类

GPU 捕获分析与资产提取工具。

| 类别 | 工具 |
|------|------|
| **捕获** | `get_capture_status`, `open_capture`, `list_captures`, `get_frame_summary` |
| **Draw Call** | `get_draw_calls`, `get_draw_call_details`, `find_draws_by_*`, `get_action_timings` |
| **Shader** | `get_shader_info`, `get_pipeline_state` |
| **纹理** | `get_texture_info`, `get_texture_data`, `save_texture` |
| **网格** | `get_mesh_data`, `export_mesh_as_fbx`, `export_mesh_csv` |

### Unreal Render MCP - 创作类

UE 资产与场景操作工具。

| 类别 | 工具 |
|------|------|
| **通用资产** | `create_asset`, `delete_asset`, `get_assets`, `set_asset_properties`, `batch_*` |
| **通用 Actor** | `spawn_actor`, `delete_actor`, `get_actors`, `set_actor_properties`, `batch_*` |
| **Blueprint 变量** | `create_blueprint_variable`, `delete_blueprint_variable`, `set_blueprint_variable_properties(properties={...})` |
| **材质图** | `build_material_graph`, `get_material_graph` |
| **纹理** | `import_texture` |
| **网格** | `import_fbx` |
| **Niagara** | `get_niagara_emitter`, `get_niagara_graph`, `update_niagara_emitter`, `update_niagara_graph` |
| **视口** | `get_viewport_screenshot` |

> 详细文档见 `docs/reference/mcp-tools/`

---

## 技术参考主题

| 主题 | 用途 | 适用场景 |
|------|------|----------|
| **renderdoc** | GPU 捕获逆向分析 | 分析渲染技术、提取资产、复现效果 |
| **materials** | UE 材质系统操作 | 创建/修改材质、分析材质图 |
| **niagara** | Niagara 粒子系统 | 创建/优化粒子效果、Stateless 转换 |
| **lookdev** | 物理灯光与材质校准 | HDRI 处理、灯光匹配、材质校准 |

> 详细文档见 `docs/reference/`

---

## 初始化

### 环境要求

| 依赖 | 版本 |
|------|------|
| Python | 3.10+ |
| RenderDoc | 1.20+ |
| Unreal Engine | 5.3+ (推荐 5.7) |

### 首次设置

```bash
# 推荐方式：运行自包含本地部署脚本
powershell -ExecutionPolicy Bypass -File .\tools\setup_local.ps1

# 或直接双击仓库根目录下的 TAAgent.bat
```

这会在 `.taagent-local/` 下创建本地虚拟环境、缓存、IPC 目录和 MCP 配置，不污染系统 Python 与系统级配置。

### MCP 配置

任意支持自定义 MCP Server 的客户端都可以使用下面的配置结构：

```json
{
  "mcpServers": {
    "renderdoc": {
      "command": "python",
      "args": ["-m", "mcp_server.server"],
      "cwd": "D:/ABSOLUTE/PATH/TO/TAAgent/mcps/renderdoc_mcp"
    },
    "unreal-render": {
      "command": "python",
      "args": ["server.py"],
      "cwd": "D:/ABSOLUTE/PATH/TO/TAAgent/mcps/unreal_render_mcp"
    }
  }
}
```

也可以直接运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\generate_local_mcp_config.ps1
```

它会基于当前仓库路径生成本地可用的 MCP 配置，输出到 `.taagent-local/config/mcp_config.local.json`。

### 开发提醒

| 修改了... | 需要操作 |
|-----------|----------|
| MCP 服务器代码 | `Ctrl+Shift+P` → `Reload Window` |
| UE 插件代码 | 重新编译 UE 项目 |
| RenderDoc 扩展 | 重启 RenderDoc |

---

## 编译

```bash
# Windows 推荐入口
TAAgent.bat

# 或直接运行 tools/ 下的 PowerShell 脚本：
# tools/setup_local.ps1
# tools/open_renderdoc_local.ps1
# tools/trigger_renderdoc_live_capture.ps1
# tools/start_renderdoc_mcp.ps1
# tools/start_unreal_mcp.ps1
```

---

## 许可证

MIT License
