#!/usr/bin/env python3
"""
UE Remote Python 客户端测试

连接到 UE 的 Python Remote Execution (UDP 组播)
"""

import socket
import json
import time

# UE Remote Python 配置
# 默认 UDP 组播地址
MULTICAST_GROUP = "239.0.0.1"
MULTICAST_PORT = 6766


def test_remote_python():
    """测试 UE Remote Python 连接"""
    
    print("=" * 60)
    print("UE Remote Python 测试")
    print("=" * 60)
    print(f"组播地址: {MULTICAST_GROUP}:{MULTICAST_PORT}")
    
    # 创建 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    
    # 设置 socket 选项
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # 绑定到端口
    sock.bind(("", MULTICAST_PORT))
    
    # 加入组播组
    import struct
    group = socket.inet_aton(MULTICAST_GROUP)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    # 设置超时
    sock.settimeout(5.0)
    
    print("\n发送发现命令...")
    
    # 发送发现命令
    # UE Remote Python 使用特定的协议
    discover_cmd = json.dumps({
        "command": "discover",
        "version": 1
    })
    
    sock.sendto(discover_cmd.encode('utf-8'), (MULTICAST_GROUP, MULTICAST_PORT))
    
    print("等待响应...")
    
    try:
        while True:
            try:
                data, addr = sock.recvfrom(8192)
                print(f"\n收到响应来自 {addr}:")
                print(data.decode('utf-8'))
            except socket.timeout:
                print("\n超时,没有更多响应")
                break
    except KeyboardInterrupt:
        print("\n用户中断")
    finally:
        sock.close()
    
    print("\n" + "=" * 60)


def send_python_command(command: str):
    """发送 Python 命令到 UE"""
    
    print("=" * 60)
    print(f"发送 Python 命令: {command[:50]}...")
    print("=" * 60)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", MULTICAST_PORT))
    
    import struct
    group = socket.inet_aton(MULTICAST_GROUP)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    sock.settimeout(10.0)
    
    # 构建命令
    cmd = json.dumps({
        "command": "exec",
        "code": command
    })
    
    sock.sendto(cmd.encode('utf-8'), (MULTICAST_GROUP, MULTICAST_PORT))
    
    print("等待执行结果...")
    
    try:
        data, addr = sock.recvfrom(65536)
        result = data.decode('utf-8')
        print(f"\n结果:\n{result}")
        return result
    except socket.timeout:
        print("超时,没有响应")
        return None
    finally:
        sock.close()


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        # 执行传入的命令
        cmd = " ".join(sys.argv[1:])
        send_python_command(cmd)
    else:
        # 测试发现
        test_remote_python()
        
        # 测试简单命令
        print("\n测试简单 Python 命令...")
        send_python_command("import unreal; print('Hello from CLI!'); print(unreal.SystemLibrary.get_engine_version())")
