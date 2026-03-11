#!/usr/bin/env python3
"""
简单的 UE 测试脚本
测试 UE 的 Python Script Plugin 是否可用
"""

import subprocess
import os
import sys
import tempfile
import time

UE_PATH = r"E:\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
PROJECT_PATH = r"d:\CodeBuddy\rendering-mcp\plugins\unreal\UnrealMCP\RenderingMCP\RenderingMCP.uproject"


def test_ue_commandlet():
    """测试 UE Commandlet 模式"""
    print("=" * 60)
    print("测试 UE Commandlet 模式")
    print("=" * 60)
    
    # 创建测试脚本
    script = '''
import unreal
import sys

print("UE Python Test")
print(f"Version: {unreal.SystemLibrary.get_engine_version()}")
sys.exit(0)
'''
    
    with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False, encoding='utf-8') as f:
        f.write(script)
        script_path = f.name
    
    # 使用 commandlet 模式
    cmd = [
        UE_PATH,
        PROJECT_PATH,
        "-run=pythonscript",
        f"-script={script_path}",
        "-unattended",
        "-nopause",
        "-NullRHI",
        "-log"
    ]
    
    print(f"命令: {' '.join(cmd)}")
    print("-" * 60)
    
    try:
        # 在后台运行,设置超时
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        
        # 等待完成或超时
        try:
            stdout, _ = proc.communicate(timeout=60)
            print(stdout)
            print(f"返回码: {proc.returncode}")
        except subprocess.TimeoutExpired:
            proc.kill()
            print("超时,进程已终止")
            
    except Exception as e:
        print(f"错误: {e}")
    finally:
        os.unlink(script_path)


def check_ue_exists():
    """检查 UE 和项目是否存在"""
    print("检查 UE 路径...")
    if os.path.exists(UE_PATH):
        print(f"  ✓ UE 存在: {UE_PATH}")
    else:
        print(f"  ✗ UE 不存在: {UE_PATH}")
        return False
    
    print("检查项目路径...")
    if os.path.exists(PROJECT_PATH):
        print(f"  ✓ 项目存在: {PROJECT_PATH}")
    else:
        print(f"  ✗ 项目不存在: {PROJECT_PATH}")
        return False
    
    return True


def test_ue_version():
    """测试 UE 版本信息"""
    print("\n" + "=" * 60)
    print("获取 UE 版本信息")
    print("=" * 60)
    
    # 使用 UE 的版本查询
    cmd = [UE_PATH, "-version"]
    
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=10,
            encoding='utf-8',
            errors='replace'
        )
        print(result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
    except subprocess.TimeoutExpired:
        print("超时")
    except Exception as e:
        print(f"错误: {e}")


if __name__ == "__main__":
    print("UE 测试脚本")
    print("=" * 60)
    
    if not check_ue_exists():
        sys.exit(1)
    
    test_ue_version()
    
    # 测试 commandlet
    # test_ue_commandlet()  # 这个可能会启动很长时间
    
    print("\n完成!")
