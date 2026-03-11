# Unreal Engine CLI Full

完整的 Unreal Engine 5 命令行接口 - 控制 UE 的一切！

## 功能概览

### 10 个命令组，50+ 个命令

| 命令组 | 命令数量 | 功能 |
|--------|----------|------|
| `actor` | 10 | Actor 管理（生成、删除、变换、属性、标签、组件） |
| `level` | 4 | 关卡操作（信息、打开、保存、创建） |
| `material` | 4 | 材质编辑（创建、实例、参数、应用） |
| `blueprint` | 3 | 蓝图操作（创建、编译、生成） |
| `asset` | 7 | 资源管理（列出、导入、导出、重命名、复制、删除、引用） |
| `select` | 5 | 选择工具（获取、设置、按类、按标签） |
| `build` | 4 | 构建操作（光照、导航、反射、全部） |
| `screenshot` | 2 | 截图（视口、相机） |
| `ui` | 2 | 编辑器 UI（通知、对话框） |
| `system` | 2 | 系统命令（信息、控制台） |

## 快速开始

### 1. 安装

```bash
cd unreal/agent-harness
pip install -e .
```

### 2. 启动 UE 监听器

在 Unreal Editor Python Console 中运行：

```python
# 完整版（推荐）
exec(open(r"D:/CodeBuddy/rendering-mcp/unreal/agent-harness/ue_cli_listener_full_auto.py").read())
start_ue_cli()
```

### 3. 使用 CLI

```bash
# 查看帮助
ue-cli-full --help

# Actor 管理
ue-cli-full actor list
ue-cli-full actor spawn -t PointLight -n MyLight -l 100,200,300
ue-cli-full actor info MyLight
ue-cli-full actor tag add MyLight Important
ue-cli-full actor components MyLight

# 材质
ue-cli-full material create -n RedMat --color 1,0,0,1
ue-cli-full material instance -n RedInstance --parent /Game/Materials/RedMat
ue-cli-full material param /Game/Materials/RedInstance -n Color -v 0,1,0,1 -t vector

# 蓝图
ue-cli-full blueprint create -n MyBP --parent Actor
ue-cli-full blueprint spawn /Game/Blueprints/MyBP -n SpawnedBP -l 0,0,100

# 资源
ue-cli-full asset list -p /Game --class StaticMesh
ue-cli-full asset import C:/model.fbx /Game/Models
ue-cli-full asset refs /Game/Materials/RedMat

# 选择
ue-cli-full select by-class PointLight
ue-cli-full select by-tag Important

# 构建
ue-cli-full build lighting --quality Production
ue-cli-full build all

# 截图
ue-cli-full screenshot capture -o C:/screenshot.png -r 3840,2160
ue-cli-full screenshot camera MyCamera -o C:/camera_shot.png

# 系统
ue-cli-full system info
ue-cli-full system cmd "stat fps"
```

## 完整命令列表

### Actor 命令

```bash
# 列出 Actor（支持过滤）
ue-cli-full actor list [--class CLASS] [--tag TAG] [--name NAME]

# 生成 Actor
ue-cli-full actor spawn -t TYPE -n NAME -l x,y,z -r pitch,yaw,roll -s x,y,z [--tag TAG ...]

# 删除 Actor
ue-cli-full actor delete NAME

# 移动/旋转/缩放
ue-cli-full actor move NAME [-l x,y,z] [-r pitch,yaw,roll] [-s x,y,z]

# 获取详细信息
ue-cli-full actor info NAME

# 设置属性
ue-cli-full actor set NAME -p PROPERTY -v VALUE

# 标签管理
ue-cli-full actor tag add NAME TAG
ue-cli-full actor tag remove NAME TAG

# 组件操作
ue-cli-full actor components NAME
```

### Level 命令

```bash
ue-cli-full level info
ue-cli-full level open PATH
ue-cli-full level save
ue-cli-full level create PATH
```

### Material 命令

```bash
# 创建材质
ue-cli-full material create -n NAME -p PATH --color r,g,b,a

# 创建材质实例
ue-cli-full material instance -n NAME --parent PARENT_PATH -p PATH

# 设置参数
ue-cli-full material param MATERIAL -n NAME -v VALUE -t TYPE

# 应用材质
ue-cli-full material apply ACTOR MATERIAL [-s SLOT]
```

### Blueprint 命令

```bash
ue-cli-full blueprint create -n NAME --parent CLASS -p PATH
ue-cli-full blueprint compile PATH
ue-cli-full blueprint spawn PATH [-n NAME] [-l x,y,z] [-r pitch,yaw,roll]
```

### Asset 命令

```bash
ue-cli-full asset list -p PATH [--class CLASS] [--recursive/--no-recursive]
ue-cli-full asset import SOURCE DESTINATION
ue-cli-full asset export ASSET_PATH EXPORT_PATH
ue-cli-full asset rename SOURCE DESTINATION
ue-cli-full asset duplicate SOURCE DESTINATION
ue-cli-full asset delete PATH
ue-cli-full asset refs PATH
```

### Select 命令

```bash
ue-cli-full select actors
ue-cli-full select assets
ue-cli-full select set NAME [NAME ...]
ue-cli-full select by-class CLASS
ue-cli-full select by-tag TAG
```

### Build 命令

```bash
ue-cli-full build lighting [--quality Preview/Medium/High/Production]
ue-cli-full build navigation
ue-cli-full build reflections
ue-cli-full build all
```

### Screenshot 命令

```bash
ue-cli-full screenshot capture [-o PATH] [-r width,height]
ue-cli-full screenshot camera CAMERA_NAME -o PATH
```

### UI 命令

```bash
ue-cli-full ui notify MESSAGE [-t info/warning/error] [-d SECONDS]
ue-cli-full ui dialog -t TITLE -m MESSAGE
```

### Python 命令

```bash
ue-cli-full python exec "CODE"
ue-cli-full python file FILE_PATH
```

### System 命令

```bash
ue-cli-full system info
ue-cli-full system cmd "CONSOLE_COMMAND"
```

## 交互式 REPL

```bash
ue-cli-full repl
```

在 REPL 模式下：
- 直接输入命令，无需 `ue-cli-full` 前缀
- 支持所有命令组
- 输入 `help` 查看帮助
- 输入 `exit` 退出

## 特性

- ✅ **50+ 个命令** - 覆盖 Actor、材质、蓝图、资源、构建等
- ✅ **自动重载** - 修改监听器代码后自动生效
- ✅ **非阻塞** - 使用 UE Slate Tick，不冻结编辑器
- ✅ **JSON 输出** - `--json` 标志支持机器可读输出
- ✅ **过滤器** - 支持按类、标签、名称过滤
- ✅ **批量操作** - 支持批量选择和操作

## 文件说明

| 文件 | 说明 |
|------|------|
| `ue_cli_listener_full.py` | 完整功能监听器 |
| `ue_cli_listener_full_auto.py` | 自动重载版本（推荐） |
| `cli_anything/unreal/unreal_cli_full.py` | 完整版 CLI 主程序 |
| `cli_anything/unreal/unreal_cli.py` | 基础版 CLI |

## 与基础版对比

| 功能 | 基础版 | 完整版 |
|------|--------|--------|
| Actor 管理 | 基础 CRUD | + 属性、标签、组件 |
| 材质 | 创建、应用 | + 实例、参数编辑 |
| 蓝图 | ❌ | ✅ 完整支持 |
| 资源 | 列出、导入 | + 导出、重命名、引用 |
| 选择工具 | ❌ | ✅ 完整支持 |
| 构建 | ❌ | ✅ 光照、导航、反射 |
| UI 交互 | ❌ | ✅ 通知、对话框 |
| 命令总数 | 13 | 50+ |

## 建议使用完整版！

```bash
# 完整版
ue-cli-full actor list --tag Enemy --class StaticMeshActor
ue-cli-full build lighting --quality Production
ue-cli-full screenshot camera MainCamera -o C:/shot.png

# 基础版
ue-cli actor list
```
