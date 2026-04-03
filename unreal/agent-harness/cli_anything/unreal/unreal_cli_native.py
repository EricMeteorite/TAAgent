#!/usr/bin/env python3
"""
Native UE CLI - 直接通过 UE 命令行和 Python 脚本操作

启动 UE 编辑器实例,通过 Python Script Plugin 执行命令
"""

import click
import json
import subprocess
import os
import sys
import time
import tempfile
import socket
from pathlib import Path
from typing import Optional, Dict, Any


# 全局 UE 进程
UE_PROCESS = None
UE_PORT = 9876
RPC_SCRIPT_PATH = None


def _repo_root() -> Path:
    """Resolve the repository root from this file location."""
    return Path(__file__).resolve().parents[4]


def get_project_path():
    """获取工程路径"""
    env_path = os.environ.get("TAAGENT_UE_UPROJECT")
    if env_path:
        return env_path

    sample_project = (
        _repo_root()
        / "plugins"
        / "unreal"
        / "UnrealMCP"
        / "RenderingMCP"
        / "RenderingMCP.uproject"
    )
    return str(sample_project)


def get_ue_path():
    """获取 UE 路径"""
    configured_path = os.environ.get("UE_EDITOR_PATH")
    if configured_path:
        return configured_path

    for candidate in (
        r"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe",
        r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
    ):
        if Path(candidate).exists():
            return candidate

    return "UnrealEditor.exe"


def start_ue_editor(project_path: str, ue_path: str, headless: bool = False):
    """启动 UE 编辑器"""
    global UE_PROCESS
    
    if UE_PROCESS and UE_PROCESS.poll() is None:
        print("UE 编辑器已在运行")
        return UE_PROCESS
    
    cmd = [
        ue_path,
        project_path,
        '-game' if headless else '',
    ]
    
    # 过滤空参数
    cmd = [c for c in cmd if c]
    
    print(f"启动 UE 编辑器: {' '.join(cmd)}")
    
    UE_PROCESS = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        universal_newlines=True
    )
    
    # 等待 UE 启动
    print("等待 UE 启动...")
    time.sleep(15)  # UE 启动需要时间
    
    return UE_PROCESS


def stop_ue_editor():
    """停止 UE 编辑器"""
    global UE_PROCESS
    
    if UE_PROCESS:
        UE_PROCESS.terminate()
        try:
            UE_PROCESS.wait(timeout=5)
        except subprocess.TimeoutExpired:
            UE_PROCESS.kill()
        UE_PROCESS = None
        print("UE 编辑器已停止")


def execute_python_in_ue(script: str, timeout: int = 30) -> str:
    """
    通过 Python 脚本文件在 UE 中执行代码
    
    使用 UnrealEditor-Cmd 模式执行 Python 脚本
    """
    ue_path = get_ue_path()
    project_path = get_project_path()
    
    # 创建临时脚本文件
    with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False, encoding='utf-8') as f:
        f.write(script)
        script_path = f.name
    
    try:
        # 使用 UnrealEditor 带项目执行 Python 脚本
        cmd = [
            ue_path,
            project_path,
            f'-ExecutePythonScript={script_path}',
            '-unattended',
            '-nopause',
            '-NullRHI',  # 无渲染模式
            '-log',
            '-stdout',   # 输出到标准输出
        ]
        
        print(f"执行命令: {' '.join(cmd)}")
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding='utf-8',
            errors='replace'
        )
        
        return f"=== STDOUT ===\n{result.stdout}\n=== STDERR ===\n{result.stderr}"
        
    except subprocess.TimeoutExpired:
        return "错误: 执行超时"
    except Exception as e:
        return f"错误: {str(e)}"
    finally:
        # 清理临时文件
        try:
            os.unlink(script_path)
        except:
            pass


@click.group()
def cli():
    """Native UE CLI - 直接操作 UE,无需 MCP Server"""
    pass


@cli.command()
@click.option('--project', '-p', default=None, help='项目路径')
@click.option('--ue', '-u', default=None, help='UE 编辑器路径')
@click.option('--headless', is_flag=True, help='无界面模式')
def start(project, ue, headless):
    """启动 UE 编辑器"""
    project_path = project or get_project_path()
    ue_path = ue or get_ue_path()
    
    if not os.path.exists(project_path):
        print(f"错误: 项目不存在 - {project_path}")
        return
    
    if not os.path.exists(ue_path):
        print(f"错误: UE 不存在 - {ue_path}")
        return
    
    proc = start_ue_editor(project_path, ue_path, headless)
    print(f"UE 编辑器已启动 (PID: {proc.pid})")


@cli.command()
def stop():
    """停止 UE 编辑器"""
    stop_ue_editor()


@cli.command()
def status():
    """检查 UE 编辑器状态"""
    global UE_PROCESS
    
    if UE_PROCESS and UE_PROCESS.poll() is None:
        print(f"UE 编辑器运行中 (PID: {UE_PROCESS.pid})")
    else:
        print("UE 编辑器未运行")


@cli.command()
@click.option('--timeout', '-t', default=60, help='执行超时时间(秒)')
def test(timeout):
    """测试 UE Python 执行环境"""
    script = '''
import unreal
import sys

print("=" * 50)
print("UE Python 环境测试")
print("=" * 50)

# 获取引擎版本
try:
    version = unreal.SystemLibrary.get_engine_version()
    print(f"UE 版本: {version}")
except Exception as e:
    print(f"获取版本失败: {e}")

# 获取当前世界
try:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        print(f"当前世界: {world.get_name()}")
    else:
        print("没有加载的世界")
except Exception as e:
    print(f"获取世界失败: {e}")

# 列出一些资产
try:
    asset_lib = unreal.EditorAssetLibrary
    all_assets = asset_lib.list_assets("/Game", recursive=False)
    print(f"/Game 下资产数量: {len(all_assets)}")
    for asset_path in all_assets[:5]:
        print(f"  - {asset_path}")
except Exception as e:
    print(f"列出资产失败: {e}")

print("=" * 50)
print("测试完成!")
print("=" * 50)
'''
    
    result = execute_python_in_ue(script, timeout)
    print(result)


@cli.command()
@click.option('--path', '-p', default='/Game/Materials', help='材质路径')
@click.option('--name', '-n', default='TestMaterial', help='材质名称')
def create_material(path, name):
    """创建新材质"""
    script = f'''
import unreal

print(f"创建材质: {path}/{name}")

# 确保目录存在
asset_lib = unreal.EditorAssetLibrary
try:
    asset_lib.make_directory("{path}")
    print(f"创建目录: {path}")
except:
    pass

# 创建材质
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    material = asset_tools.create_asset(
        asset_name="{name}",
        package_path="{path}",
        asset_class=unreal.Material,
        factory=unreal.MaterialFactoryNew()
    )
    
    if material:
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        print(f"成功创建材质: {path}/{name}")
    else:
        print("创建材质失败: 返回 None")
except Exception as e:
    print(f"创建材质异常: {e}")
'''
    
    result = execute_python_in_ue(script, timeout=60)
    print(result)


@cli.command()
@click.option('--type', '-t', default='PointLight', help='Actor 类型')
@click.option('--location', '-l', default='0,0,100', help='位置 (x,y,z)')
@click.option('--name', '-n', default=None, help='Actor 名称')
def spawn_actor(type, location, name):
    """在当前关卡中生成 Actor"""
    loc = [float(x) for x in location.split(',')]
    actor_name = name or f"CLI_{type}"
    
    script = f'''
import unreal

print(f"生成 Actor: {type} at ({loc[0]}, {loc[1]}, {loc[2]})")

# 获取当前世界
world = unreal.EditorLevelLibrary.get_editor_world()
if not world:
    print("错误: 没有加载的世界")
    sys.exit(1)

# 尝试获取 Actor 类
actor_class = None

# 尝试作为内置类
try:
    actor_class = getattr(unreal, "{type}", None)
    if actor_class:
        print(f"使用内置类: {type}")
except:
    pass

if actor_class:
    location = unreal.Vector({loc[0]}, {loc[1]}, {loc[2]})
    rotation = unreal.Rotator(0, 0, 0)
    
    try:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class,
            location,
            rotation
        )
        
        if actor:
            actor.set_actor_name("{actor_name}")
            print(f"成功生成 Actor: {actor_name}")
        else:
            print("生成 Actor 失败: spawn 返回 None")
    except Exception as e:
        print(f"生成 Actor 异常: {e}")
else:
    print(f"错误: 找不到 Actor 类 {type}")
'''
    
    result = execute_python_in_ue(script, timeout=60)
    print(result)


@cli.command()
@click.option('--output', '-o', default='output/screenshot.png', help='输出路径')
@click.option('--width', '-w', default=1920, help='宽度')
@click.option('--height', '-h', default=1080, help='高度')
def screenshot(output, width, height):
    """截取视口截图"""
    abs_output = os.path.abspath(output)
    os.makedirs(os.path.dirname(abs_output), exist_ok=True)
    
    script = f'''
import unreal

print(f"截图: {abs_output} ({width}x{height})")

try:
    # 使用高分辨率截图
    unreal.AutomationLibrary.take_high_res_screenshot(
        {width},
        {height},
        r"{abs_output}"
    )
    print(f"成功截图: {abs_output}")
except Exception as e:
    print(f"截图异常: {e}")
    # 尝试备用方法
    try:
        import os
        os.makedirs(r"{os.path.dirname(abs_output)}", exist_ok=True)
        unreal.SystemLibrary.take_screenshot_for_game_viewport(r"{abs_output}")
        print(f"备用截图成功: {abs_output}")
    except Exception as e2:
        print(f"备用截图失败: {e2}")
'''
    
    result = execute_python_in_ue(script, timeout=60)
    print(result)


@cli.command()
@click.option('--level', '-l', required=True, help='关卡路径')
def load_level(level):
    """加载关卡"""
    script = f'''
import unreal

print(f"加载关卡: {level}")

try:
    asset_data = unreal.EditorAssetLibrary.find_asset_data("{level}")
    if asset_data:
        soft_obj_path = asset_data.get_soft_object_path()
        unreal.EditorLoadingAndSavingUtils.load_map(soft_obj_path)
        print(f"成功加载关卡: {level}")
    else:
        print(f"错误: 找不到关卡 {level}")
except Exception as e:
    print(f"加载关卡异常: {e}")
'''
    
    result = execute_python_in_ue(script, timeout=60)
    print(result)


@cli.command()
def list_assets():
    """列出 /Game 目录下的资产"""
    script = '''
import unreal

print("=" * 50)
print("/Game 目录资产列表")
print("=" * 50)

asset_lib = unreal.EditorAssetLibrary

# 递归列出所有资产
all_assets = asset_lib.list_assets("/Game", recursive=True)

print(f"总资产数量: {len(all_assets)}")

# 按类型分类
by_type = {}
for asset_path in all_assets:
    try:
        asset_data = asset_lib.find_asset_data(asset_path)
        asset_class = asset_data.asset_class_path.asset_name
        if asset_class not in by_type:
            by_type[asset_class] = []
        by_type[asset_class].append(asset_path)
    except:
        pass

for asset_class, assets in sorted(by_type.items()):
    print(f"\\n{asset_class}: {len(assets)}")
    for asset in assets[:3]:
        print(f"  - {asset}")
    if len(assets) > 3:
        print(f"  ... 还有 {len(assets) - 3} 个")
'''
    
    result = execute_python_in_ue(script, timeout=120)
    print(result)


@cli.command()
def info():
    """显示 UE 和项目信息"""
    script = '''
import unreal
import sys
import os

print("=" * 50)
print("UE 环境信息")
print("=" * 50)

# 引擎信息
print(f"引擎版本: {unreal.SystemLibrary.get_engine_version()}")
print(f"Python 版本: {sys.version}")

# 项目信息
project_dir = unreal.SystemLibrary.get_project_directory()
project_name = unreal.SystemLibrary.get_project_name()
print(f"项目目录: {project_dir}")
print(f"项目名称: {project_name}")

# 内容目录
content_dir = unreal.SystemLibrary.get_project_content_directory()
print(f"内容目录: {content_dir}")

# 当前世界
world = unreal.EditorLevelLibrary.get_editor_world()
if world:
    print(f"当前世界: {world.get_name()}")
else:
    print("没有加载的世界")

#  Actor 数量
if world:
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    print(f"关卡 Actor 数量: {len(actors)}")

print("=" * 50)
'''
    
    result = execute_python_in_ue(script, timeout=60)
    print(result)


def main():
    cli(obj={})


if __name__ == '__main__':
    main()
