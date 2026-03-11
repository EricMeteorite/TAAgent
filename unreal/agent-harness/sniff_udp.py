#!/usr/bin/env python3
"""
监听 UE 发送的所有 UDP 消息
看看 UE 到底在发什么
"""

import socket
import struct
import json
import time

# 监听所有接口
MULTICAST_GROUP = "239.0.0.1"
MULTICAST_PORT = 6766

print(f"监听 UDP 组播: {MULTICAST_GROUP}:{MULTICAST_PORT}")
print("按 Ctrl+C 停止\n")

# 创建 UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

# 方式1: 绑定到组播端口
sock.bind(("", MULTICAST_PORT))

# 加入组播组
group = socket.inet_aton(MULTICAST_GROUP)
mreq = struct.pack('4sL', group, socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

# 设置超时
sock.settimeout(1.0)

# 记录收到的消息
messages = []
start_time = time.time()

print("监听中... (等待 10 秒)\n")

while time.time() - start_time < 10:
    try:
        data, addr = sock.recvfrom(65536)
        elapsed = time.time() - start_time
        
        try:
            msg = json.loads(data.decode('utf-8'))
            print(f"[{elapsed:.1f}s] 来自 {addr}:")
            print(f"  类型: {msg.get('message_type', 'unknown')}")
            print(f"  完整消息: {json.dumps(msg, indent=2)[:500]}")
            
            # 记录非本地消息
            if msg.get('node_id') != 'test-client':
                messages.append((elapsed, addr, msg))
        except:
            print(f"[{elapsed:.1f}s] 来自 {addr}: 非JSON数据 ({len(data)} bytes)")
            
    except socket.timeout:
        continue
    except KeyboardInterrupt:
        break

sock.close()

print(f"\n\n总结:")
print(f"收到 {len(messages)} 条来自 UE 的消息")

if messages:
    print("\n消息列表:")
    for elapsed, addr, msg in messages:
        print(f"  [{elapsed:.1f}s] {addr}: {msg.get('message_type')}")
