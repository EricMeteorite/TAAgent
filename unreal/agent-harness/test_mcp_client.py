#!/usr/bin/env python3
"""
测试 MCP 连接

连接到 UE MCP 插件 (TCP 55557)
"""

import socket
import json

HOST = "127.0.0.1"
PORT = 55557

print("=" * 60)
print(f"连接到 UE MCP: {HOST}:{PORT}")
print("=" * 60)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.settimeout(10.0)

try:
    sock.connect((HOST, PORT))
    print("✓ TCP 连接成功!")
    
    # MCP 协议: 发送命令
    # 参考 unreal_render_mcp/connection.py
    command = {
        "type": "get_actors",
        "params": {}
    }
    
    msg = json.dumps(command)
    sock.sendall(msg.encode('utf-8'))
    print(f"发送: {msg}")
    
    # 接收响应
    chunks = []
    while True:
        chunk = sock.recv(8192)
        if not chunk:
            break
        chunks.append(chunk)
        
        # 尝试解析
        try:
            data = b''.join(chunks)
            response = json.loads(data.decode('utf-8'))
            print(f"\n响应:")
            print(json.dumps(response, indent=2, ensure_ascii=False)[:2000])
            break
        except json.JSONDecodeError:
            continue
            
except Exception as e:
    print(f"错误: {e}")
finally:
    sock.close()
