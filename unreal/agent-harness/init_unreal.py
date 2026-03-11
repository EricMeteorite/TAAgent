"""
UE 命令监听器

在 UE 编辑器中运行此脚本,监听文件命令并执行

使用方法:
1. 在 UE 编辑器中打开 Python 控制台 (Window -> Developer Tools -> Output Log -> Python)
2. 执行: exec(open(r'd:\CodeBuddy\rendering-mcp\unreal\agent-harness\init_unreal.py').read())
3. 或者: import sys; sys.path.append(r'd:\CodeBuddy\rendering-mcp\unreal\agent-harness'); from cli_anything.unreal.init_unreal import start_listener; start_listener()
"""

import unreal
import json
import os
import time
import threading
from pathlib import Path

# 命令目录
COMMAND_DIR = Path(r"d:\CodeBuddy\rendering-mcp\unreal\commands")
COMMAND_FILE = COMMAND_DIR / "command.py"
RESULT_FILE = COMMAND_DIR / "result.json"
LOCK_FILE = COMMAND_DIR / "lock"

# 监听状态
_running = False


def ensure_dir():
    """确保命令目录存在"""
    COMMAND_DIR.mkdir(parents=True, exist_ok=True)


def execute_command_file():
    """执行命令文件"""
    ensure_dir()
    
    if not COMMAND_FILE.exists():
        return None
    
    try:
        # 读取命令
        with open(COMMAND_FILE, 'r', encoding='utf-8') as f:
            script = f.read()
        
        # 删除命令文件
        COMMAND_FILE.unlink()
        
        # 执行命令
        local_vars = {'unreal': unreal}
        exec_result = {}
        
        try:
            exec(script, globals(), exec_result)
            
            # 尝试获取结果
            if 'result' in exec_result:
                result = exec_result['result']
            else:
                result = {"status": "success", "message": "命令执行完成"}
                
        except Exception as e:
            result = {"status": "error", "message": str(e)}
        
        # 写入结果
        with open(RESULT_FILE, 'w', encoding='utf-8') as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
        
        return result
        
    except Exception as e:
        return {"status": "error", "message": f"执行失败: {str(e)}"}


def listener_loop(interval: float = 0.5):
    """监听循环"""
    global _running
    _running = True
    
    print(f"[UE Listener] 开始监听命令文件: {COMMAND_FILE}")
    
    while _running:
        try:
            if COMMAND_FILE.exists() and not LOCK_FILE.exists():
                print(f"[UE Listener] 检测到命令文件,执行中...")
                result = execute_command_file()
                print(f"[UE Listener] 执行结果: {result}")
        except Exception as e:
            print(f"[UE Listener] 错误: {e}")
        
        time.sleep(interval)


def start_listener(interval: float = 0.5):
    """启动监听器"""
    global _running
    
    if _running:
        print("[UE Listener] 监听器已在运行")
        return
    
    # 创建线程
    thread = threading.Thread(target=listener_loop, args=(interval,), daemon=True)
    thread.start()
    
    print(f"[UE Listener] 监听器已启动 (线程 ID: {thread.ident})")


def stop_listener():
    """停止监听器"""
    global _running
    _running = False
    print("[UE Listener] 监听器已停止")


def test():
    """测试功能"""
    print("=" * 60)
    print("UE Python 环境测试")
    print("=" * 60)
    
    print(f"引擎版本: {unreal.SystemLibrary.get_engine_version()}")
    print(f"项目名称: {unreal.SystemLibrary.get_project_name()}")
    print(f"项目目录: {unreal.SystemLibrary.get_project_directory()}")
    
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world:
        print(f"当前世界: {world.get_name()}")
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        print(f"Actor 数量: {len(actors)}")
    else:
        print("没有加载的世界")
    
    print("=" * 60)


# 如果直接执行
if __name__ == "__main__":
    test()
