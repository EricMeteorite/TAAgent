#!/usr/bin/env python3
"""
直接向 UE 发送消息 (不通过组播)

根据诊断结果，UE 监听在 127.0.0.1:6766
"""

import socket
import json
import time

UE_HOST = "127.0.0.1"
UE_PORT = 6766

print("=" * 60)
print(f"直接连接 UE Remote Execution: {UE_HOST}:{UE_PORT}")
print("=" * 60)

# 创建 UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2.0)

# 绑定到随机端口接收响应
sock.bind(("127.0.0.1", 0))
local_port = sock.getsockname()[1]
print(f"本地端口: {local_port}")

# 发送 PING
ping_msg = {
    "protocol_version": 1,
    "protocol_magic": "ue_py",
    "message_type": "ping",
    "node_id": "test-client-direct"
}

print(f"\n发送 PING...")
sock.sendto(json.dumps(ping_msg).encode('utf-8'), (UE_HOST, UE_PORT))

# 等待响应
print("等待响应 (5秒)...")
start = time.time()
while time.time() - start < 5:
    try:
        data, addr = sock.recvfrom(65536)
        msg = json.loads(data.decode('utf-8'))
        msg_type = msg.get('message_type')
        node_id = msg.get('node_id', 'unknown')
        
        print(f"\n收到消息来自 {addr}:")
        print(f"  类型: {msg_type}")
        print(f"  node_id: {node_id}")
        print(f"  完整消息:\n{json.dumps(msg, indent=2)}")
        
        if node_id != "test-client-direct":
            print("\n✓ 这是来自 UE 的响应!")
            
    except socket.timeout:
        continue

sock.close()
print("\n完成")
