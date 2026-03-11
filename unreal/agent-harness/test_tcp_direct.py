#!/usr/bin/env python3
"""
直接连接 UE Remote Python TCP

假设 UE 在 127.0.0.1:6766 监听 TCP
"""

import socket
import json

# 尝试连接到 UE
HOST = "127.0.0.1"
PORT = 6766

print(f"尝试连接到 {HOST}:{PORT}...")

try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((HOST, PORT))
    print("TCP 连接成功!")
    
    # 尝试发送命令
    msg = {
        "protocol_version": 1,
        "protocol_magic": "ue_py",
        "message_type": "command",
        "node_id": "test-client",
        "command": "print('Hello from CLI!')",
        "execution_mode": "ExecuteStatement"
    }
    
    data = json.dumps(msg).encode('utf-8')
    sock.sendall(data)
    print(f"发送: {msg}")
    
    # 接收响应
    sock.settimeout(10.0)
    response = sock.recv(8192)
    print(f"响应: {response.decode('utf-8')}")
    
except ConnectionRefusedError:
    print("连接被拒绝 - 端口可能不是TCP")
except socket.timeout:
    print("连接超时")
except Exception as e:
    print(f"错误: {e}")
finally:
    try:
        sock.close()
    except:
        pass

# 也尝试扫描常见端口
print("\n扫描常见 UE Python 端口...")
for port in [6766, 6776, 55557, 9999, 8080]:
    try:
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(0.5)
        result = test_sock.connect_ex((HOST, port))
        if result == 0:
            print(f"  端口 {port}: 开放")
        test_sock.close()
    except:
        pass
