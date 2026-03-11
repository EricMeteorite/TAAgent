#!/usr/bin/env python3
"""
UE 命令文件执行器

通过文件系统通信:
1. 写入命令到 command.py
2. UE 通过 Python 插件的 "Execute Python Script" 功能定期检查执行
3. 结果写入 result.json

使用方法:
1. 在 UE 中运行 init_unreal.py 启动监听
2. 使用 CLI 写入命令
"""

import json
import os
import time
from pathlib import Path

# 命令目录
COMMAND_DIR = Path(r"d:\CodeBuddy\rendering-mcp\unreal\commands")
COMMAND_FILE = COMMAND_DIR / "command.py"
RESULT_FILE = COMMAND_DIR / "result.json"
LOCK_FILE = COMMAND_DIR / "lock"


def ensure_dir():
    """确保命令目录存在"""
    COMMAND_DIR.mkdir(parents=True, exist_ok=True)


def execute_command(script: str, timeout: float = 30.0) -> dict:
    """
    执行 Python 命令
    
    等待 UE 读取并执行命令,返回结果
    """
    ensure_dir()
    
    # 等待锁释放
    start = time.time()
    while LOCK_FILE.exists() and (time.time() - start) < timeout:
        time.sleep(0.1)
    
    # 创建锁
    LOCK_FILE.touch()
    
    try:
        # 写入命令
        with open(COMMAND_FILE, 'w', encoding='utf-8') as f:
            f.write(script)
        
        # 等待结果
        start = time.time()
        while not RESULT_FILE.exists() and (time.time() - start) < timeout:
            time.sleep(0.1)
        
        if RESULT_FILE.exists():
            with open(RESULT_FILE, 'r', encoding='utf-8') as f:
                result = json.load(f)
            RESULT_FILE.unlink()
            return result
        else:
            return {"status": "timeout", "message": "等待结果超时"}
            
    finally:
        # 释放锁
        if LOCK_FILE.exists():
            LOCK_FILE.unlink()


def test():
    """测试命令"""
    script = '''
import unreal
import json

result = {
    "status": "success",
    "engine_version": unreal.SystemLibrary.get_engine_version(),
    "project_name": unreal.SystemLibrary.get_project_name()
}

print(json.dumps(result))
'''
    
    result = execute_command(script)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return result


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        # 执行传入的命令
        script = " ".join(sys.argv[1:])
        result = execute_command(script)
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        # 测试模式
        test()
