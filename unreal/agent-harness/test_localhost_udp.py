#!/usr/bin/env python3
"""测试直接往 127.0.0.1:6766 发送 UDP 消息"""

import socket
import struct
import json
import time
import threading

# UE Remote Execution 协议常量
_PROTOCOL_VERSION = 1
_PROTOCOL_MAGIC = b'uepy'

# 消息类型
_TYPE_PING = b'ping'
_TYPE_PONG = b'pong'
_TYPE_OPEN_CONNECTION = b'open'
_TYPE_CLOSE_CONNECTION = b'clos'
_TYPE_COMMAND = b'comm'
_TYPE_COMMAND_RESULT = b'resl'

def pack_message(msg_type, data):
    """打包消息"""
    payload = json.dumps(data).encode('utf-8')
    # 格式: magic(4) + version(1) + type(4) + payload_len(4) + payload
    header = struct.pack('<4s B 4s I', _PROTOCOL_MAGIC, _PROTOCOL_VERSION, msg_type, len(payload))
    return header + payload

def unpack_message(data):
    """解包消息"""
    if len(data) < 13:
        return None
    magic, version, msg_type, payload_len = struct.unpack('<4s B 4s I', data[:13])
    if magic != _PROTOCOL_MAGIC or version != _PROTOCOL_VERSION:
        return None
    payload = data[13:13+payload_len]
    return {
        'type': msg_type,
        'data': json.loads(payload.decode('utf-8')) if payload else {}
    }

def send_ping():
    """发送 PING 到 127.0.0.1:6766"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5)
    
    # 绑定到本地端口用于接收响应
    sock.bind(('127.0.0.1', 0))
    local_port = sock.getsockname()[1]
    print(f"本地端口: {local_port}")
    
    # 发送 PING 到 127.0.0.1:6766
    ping_data = {
        'version': _PROTOCOL_VERSION,
        'source': f'127.0.0.1:{local_port}'
    }
    ping_msg = pack_message(_TYPE_PING, ping_data)
    
    print(f"发送 PING 到 127.0.0.1:6766")
    print(f"消息: {ping_data}")
    sock.sendto(ping_msg, ('127.0.0.1', 6766))
    
    # 等待响应
    try:
        data, addr = sock.recvfrom(4096)
        print(f"\n收到来自 {addr} 的响应:")
        msg = unpack_message(data)
        if msg:
            print(f"  类型: {msg['type']}")
            print(f"  数据: {msg['data']}")
            return msg
        else:
            print(f"  原始数据: {data[:100]}")
    except socket.timeout:
        print("超时: 没有收到响应")
    
    sock.close()
    return None

if __name__ == '__main__':
    print("=" * 60)
    print("测试直接往 127.0.0.1:6766 发送 UDP 消息")
    print("=" * 60)
    
    result = send_ping()
    
    if result and result['type'] == _TYPE_PONG:
        print("\n✓ 成功! UE Remote Execution 响应了 PING")
    else:
        print("\n✗ 失败! UE 没有响应")
        print("\n可能的原因:")
        print("1. UE 的 Remote Execution 没有正确启动")
        print("2. 需要重启 UE 编辑器使配置生效")
        print("3. Python Script Plugin 可能没有启用")
