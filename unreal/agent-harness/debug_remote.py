#!/usr/bin/env python3
"""调试 UE Remote Python"""

import socket
import json
import struct

MULTICAST_GROUP = "239.0.0.1"
MULTICAST_PORT = 6766

# 创建 UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("", MULTICAST_PORT))

# 加入组播组
group = socket.inet_aton(MULTICAST_GROUP)
mreq = struct.pack('4sL', group, socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
sock.settimeout(2.0)

print(f"监听 {MULTICAST_GROUP}:{MULTICAST_PORT}")
print("发送 PING...")

# 发送 PING
ping_msg = {
    "protocol_version": 1,
    "protocol_magic": "ue_py",
    "message_type": "ping",
    "node_id": "test-client-12345"
}

sock.sendto(json.dumps(ping_msg).encode('utf-8'), (MULTICAST_GROUP, MULTICAST_PORT))

print("等待响应 (5秒)...\n")

# 接收所有响应
import time
start = time.time()
while time.time() - start < 5:
    try:
        data, addr = sock.recvfrom(65536)
        try:
            msg = json.loads(data.decode('utf-8'))
            print(f"收到消息来自 {addr}:")
            print(json.dumps(msg, indent=2, ensure_ascii=False))
            print()
        except:
            print(f"收到非JSON消息来自 {addr}: {data[:100]}")
            print()
    except socket.timeout:
        continue

sock.close()
print("完成")
