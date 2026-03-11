#!/usr/bin/env python3
"""
诊断 UE Remote Execution 连接问题
"""

import socket
import struct
import json
import time
import subprocess

print("=" * 60)
print("UE Remote Execution 诊断工具")
print("=" * 60)

# 1. 检查网络端口
print("\n[1] 检查网络端口...")
result = subprocess.run(['netstat', '-an'], capture_output=True, text=True)
lines = result.stdout.split('\n')

print("监听的 UDP 端口:")
for line in lines:
    if 'UDP' in line and (':6766' in line or ':6776' in line):
        print(f"  {line.strip()}")

print("\n监听的 TCP 端口:")
for line in lines:
    if 'LISTENING' in line and (':6766' in line or ':6776' in line or ':55557' in line):
        print(f"  {line.strip()}")

# 2. 测试 UDP 组播
print("\n[2] 测试 UDP 组播通信...")

# 尝试不同的绑定方式
configs = [
    ("绑定到 0.0.0.0", "0.0.0.0"),
    ("绑定到 127.0.0.1", "127.0.0.1"),
]

for desc, bind_addr in configs:
    print(f"\n  {desc}:")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        if bind_addr == "0.0.0.0":
            sock.bind(("", 6766))
        else:
            sock.bind((bind_addr, 6766))
        
        # 加入组播组
        group = socket.inet_aton("239.0.0.1")
        mreq = struct.pack('4sL', group, socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        sock.settimeout(1.0)
        
        # 发送 PING
        ping_msg = {
            "protocol_version": 1,
            "protocol_magic": "ue_py",
            "message_type": "ping",
            "node_id": "diagnostic-client"
        }
        sock.sendto(json.dumps(ping_msg).encode('utf-8'), ("239.0.0.1", 6766))
        
        # 等待响应
        start = time.time()
        while time.time() - start < 2.0:
            try:
                data, addr = sock.recvfrom(65536)
                msg = json.loads(data.decode('utf-8'))
                msg_type = msg.get('message_type')
                node_id = msg.get('node_id', 'unknown')
                
                if node_id != "diagnostic-client":
                    print(f"    ✓ 收到 {msg_type} 来自 {addr}")
                    print(f"      node_id: {node_id}")
                else:
                    print(f"    - 收到自己的消息 (回环)")
            except socket.timeout:
                break
            except Exception as e:
                pass
        
        sock.close()
        
    except Exception as e:
        print(f"    ✗ 错误: {e}")

# 3. 测试直接发送到 127.0.0.1
print("\n[3] 测试直接发送到 127.0.0.1...")
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)
    
    ping_msg = {
        "protocol_version": 1,
        "protocol_magic": "ue_py",
        "message_type": "ping",
        "node_id": "diagnostic-client"
    }
    sock.sendto(json.dumps(ping_msg).encode('utf-8'), ("127.0.0.1", 6766))
    
    try:
        data, addr = sock.recvfrom(65536)
        msg = json.loads(data.decode('utf-8'))
        print(f"  ✓ 收到响应: {msg.get('message_type')}")
    except socket.timeout:
        print(f"  ✗ 超时，无响应")
    
    sock.close()
except Exception as e:
    print(f"  ✗ 错误: {e}")

# 4. 测试 TCP 连接
print("\n[4] 测试 TCP 连接...")
for port in [6766, 6776, 55557]:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(1.0)
        result = sock.connect_ex(("127.0.0.1", port))
        if result == 0:
            print(f"  ✓ TCP 端口 {port} 开放")
        else:
            print(f"  ✗ TCP 端口 {port} 关闭")
        sock.close()
    except Exception as e:
        print(f"  ✗ TCP 端口 {port} 错误: {e}")

# 5. 建议
print("\n" + "=" * 60)
print("诊断完成!")
print("=" * 60)

print("""
如果 UDP 组播没有收到 UE 的 PONG 响应，请检查:

1. UE 中是否正确启用了 Remote Execution:
   - Edit -> Project Settings -> Python
   - 勾选 "Enable Remote Execution"
   
2. 检查配置是否正确:
   - Multicast Group Endpoint: 239.0.0.1:6766
   - Multicast Bind Address: 127.0.0.1 或 0.0.0.0
   
3. 重启 UE 编辑器后再试

4. 检查防火墙是否阻止了 UDP 组播

5. 如果使用 MCP 插件 (TCP 55557)，可以直接用 MCP 方式连接
""")

print("\n当前可用的连接方式:")
if subprocess.run(['netstat', '-an'], capture_output=True, text=True).stdout.find(':55557') != -1:
    print("  ✓ MCP 插件 (TCP 55557) - 可用")
else:
    print("  ✗ MCP 插件 (TCP 55557) - 不可用")
