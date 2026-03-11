#!/usr/bin/env python3
"""
UE Remote Execution 完整实现

工作流程:
1. 客户端创建 TCP 服务器 (等待 UE 连接)
2. 客户端发送 UDP OPEN_CONNECTION 消息
3. UE 收到后主动连接客户端的 TCP 端口
4. 通过 TCP 发送 Python 命令
"""

import socket
import json
import time
import struct
import threading

UE_HOST = "127.0.0.1"
UE_PORT = 6766
TCP_PORT = 6776  # 客户端 TCP 端口

print("=" * 60)
print("UE Remote Execution 测试")
print("=" * 60)

# 创建 TCP 服务器
print(f"\n[1] 创建 TCP 服务器 (端口 {TCP_PORT})...")
tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
tcp_server.bind(("127.0.0.1", TCP_PORT))
tcp_server.listen(1)
tcp_server.settimeout(10.0)  # 10秒超时
print("  ✓ TCP 服务器已启动")

# 创建 UDP socket
print(f"\n[2] 创建 UDP socket...")
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.settimeout(2.0)
print("  ✓ UDP socket 已创建")

# 发送 OPEN_CONNECTION
print(f"\n[3] 发送 OPEN_CONNECTION 到 {UE_HOST}:{UE_PORT}...")
open_msg = {
    "protocol_version": 1,
    "protocol_magic": "ue_py",
    "message_type": "open_connection",
    "node_id": "python-client-001",
    "command_endpoint": ["127.0.0.1", TCP_PORT]
}

udp_sock.sendto(json.dumps(open_msg).encode('utf-8'), (UE_HOST, UE_PORT))
print(f"  已发送: {json.dumps(open_msg)}")

# 等待 TCP 连接
print(f"\n[4] 等待 UE 连接 TCP (超时 10秒)...")
try:
    tcp_client, addr = tcp_server.accept()
    print(f"  ✓ UE 已连接! 来自 {addr}")
    
    # 连接成功，可以发送命令了
    print(f"\n[5] 发送 Python 命令...")
    
    cmd_msg = {
        "protocol_version": 1,
        "protocol_magic": "ue_py",
        "message_type": "command",
        "node_id": "python-client-001",
        "command": "import unreal; print('Hello from CLI!'); print(unreal.SystemLibrary.get_engine_version())",
        "execution_mode": "ExecuteStatement"
    }
    
    tcp_client.sendall(json.dumps(cmd_msg).encode('utf-8'))
    print(f"  已发送命令")
    
    # 接收结果
    print(f"\n[6] 等待执行结果...")
    tcp_client.settimeout(10.0)
    
    chunks = []
    while True:
        chunk = tcp_client.recv(8192)
        if not chunk:
            break
        chunks.append(chunk)
        
        try:
            data = b''.join(chunks)
            result = json.loads(data.decode('utf-8'))
            print(f"\n结果:")
            print(json.dumps(result, indent=2, ensure_ascii=False))
            break
        except json.JSONDecodeError:
            continue
    
    tcp_client.close()
    
except socket.timeout:
    print("  ✗ 超时! UE 没有连接")
    print("\n可能的原因:")
    print("  1. UE Remote Execution 未正确配置")
    print("  2. Multicast Bind Address 设置限制了连接")
    print("  3. 防火墙阻止了连接")
except Exception as e:
    print(f"  ✗ 错误: {e}")

# 清理
udp_sock.close()
tcp_server.close()

print("\n" + "=" * 60)
print("测试完成")
print("=" * 60)
