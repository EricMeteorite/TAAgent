#!/usr/bin/env python3
"""直接测试 UE Python 执行"""

import subprocess
import os
import sys
import tempfile

UE_PATH = r"E:\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
PROJECT_PATH = r"d:\CodeBuddy\rendering-mcp\plugins\unreal\UnrealMCP\RenderingMCP\RenderingMCP.uproject"

def test_ue():
    # 创建测试脚本
    script = '''
import unreal
import sys

# 强制刷新输出
sys.stdout.reconfigure(line_buffering=True)

print("=" * 60)
print("UE Python Environment Test")
print("=" * 60, flush=True)

try:
    version = unreal.SystemLibrary.get_engine_version()
    print(f"UE Version: {version}", flush=True)
except Exception as e:
    print(f"Version check failed: {e}", flush=True)

try:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        print(f"Current World: {world.get_name()}", flush=True)
    else:
        print("No world loaded", flush=True)
except Exception as e:
    print(f"World check failed: {e}", flush=True)

print("=" * 60)
print("Test Complete!", flush=True)
print("=" * 60)

# 强制退出
import os
os._exit(0)
'''
    
    # 写入临时文件
    with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False, encoding='utf-8') as f:
        f.write(script)
        script_path = f.name
    
    print(f"Script path: {script_path}")
    print(f"UE path: {UE_PATH}")
    print(f"Project path: {PROJECT_PATH}")
    
    # 检查文件是否存在
    if not os.path.exists(UE_PATH):
        print(f"ERROR: UE not found at {UE_PATH}")
        return
    
    if not os.path.exists(PROJECT_PATH):
        print(f"ERROR: Project not found at {PROJECT_PATH}")
        return
    
    # 构建命令
    cmd = [
        UE_PATH,
        PROJECT_PATH,
        f"-ExecutePythonScript={script_path}",
        "-unattended",
        "-nopause",
        "-log",
        "-stdout",
        "-forcelogflush",
        "-RenderOffscreen"  # 离屏渲染而不是 NullRHI
    ]
    
    print(f"\nExecuting: {' '.join(cmd)}")
    print("-" * 60)
    
    try:
        # 设置环境变量
        env = os.environ.copy()
        env['PYTHONIOENCODING'] = 'utf-8'
        env['PYTHONUTF8'] = '1'
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,  # 2分钟超时
            encoding='utf-8',
            errors='replace',
            env=env
        )
        
        print("=== STDOUT ===")
        print(result.stdout)
        print("\n=== STDERR ===")
        print(result.stderr)
        print(f"\nExit code: {result.returncode}")
        
    except subprocess.TimeoutExpired:
        print("ERROR: Timeout after 120 seconds")
    except Exception as e:
        print(f"ERROR: {e}")
    finally:
        # 清理临时文件
        try:
            os.unlink(script_path)
            print(f"\nCleaned up: {script_path}")
        except:
            pass


if __name__ == "__main__":
    test_ue()
