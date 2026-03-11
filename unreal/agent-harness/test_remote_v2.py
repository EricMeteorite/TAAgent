#!/usr/bin/env python3
"""
UE Remote Python 客户端 v2

使用 UE 官方的 remote_execution 协议
"""

import socket
import json
import time
import struct
import uuid

# UE Remote Python 配置
MULTICAST_GROUP = "239.0.0.1"
MULTICAST_PORT = 6766
BUFFER_SIZE = 65536


class UERemoteClient:
    """UE Remote Python 客户端"""
    
    def __init__(self, group: str = MULTICAST_GROUP, port: int = MULTICAST_PORT):
        self.group = group
        self.port = port
        self.client_id = str(uuid.uuid4())
        self.socket = None
        
    def connect(self):
        """创建并配置 socket"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        
        # 绑定端口
        self.socket.bind(("", self.port))
        
        # 加入组播组
        group_bytes = socket.inet_aton(self.group)
        mreq = struct.pack('4sL', group_bytes, socket.INADDR_ANY)
        self.socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        print(f"已连接到组播组 {self.group}:{self.port}")
        
    def disconnect(self):
        """关闭连接"""
        if self.socket:
            self.socket.close()
            self.socket = None
            
    def discover(self, timeout: float = 2.0) -> list:
        """发现 UE 实例"""
        print("发现 UE 实例...")
        
        if not self.socket:
            self.connect()
            
        # 发送发现命令
        # UE 使用特定的协议格式
        msg = {
            "id": self.client_id,
            "command": "discover"
        }
        
        self.socket.sendto(json.dumps(msg).encode('utf-8'), (self.group, self.port))
        
        # 收集响应
        self.socket.settimeout(timeout)
        instances = []
        
        try:
            while True:
                try:
                    data, addr = self.socket.recvfrom(BUFFER_SIZE)
                    response = json.loads(data.decode('utf-8'))
                    print(f"  发现: {addr} - {response}")
                    instances.append((addr, response))
                except socket.timeout:
                    break
        except Exception as e:
            print(f"  错误: {e}")
            
        return instances
    
    def execute_python(self, code: str, timeout: float = 10.0) -> dict:
        """执行 Python 代码"""
        print(f"执行 Python: {code[:60]}...")
        
        if not self.socket:
            self.connect()
            
        # 构建 UE Remote Execution 协议消息
        # 参考: https://docs.unrealengine.com/5.0/en-US/PythonAPI/
        msg = {
            "id": self.client_id,
            "command": "exec",
            "code": code
        }
        
        self.socket.sendto(json.dumps(msg).encode('utf-8'), (self.group, self.port))
        
        # 等待响应
        self.socket.settimeout(timeout)
        
        try:
            data, addr = self.socket.recvfrom(BUFFER_SIZE)
            response = json.loads(data.decode('utf-8'))
            print(f"  响应: {response}")
            return response
        except socket.timeout:
            print("  超时")
            return {"error": "timeout"}
        except Exception as e:
            print(f"  错误: {e}")
            return {"error": str(e)}
    
    def test(self):
        """测试连接"""
        print("=" * 60)
        print("UE Remote Python 连接测试")
        print("=" * 60)
        
        # 发现实例
        instances = self.discover()
        
        if not instances:
            print("没有发现 UE 实例")
            return False
            
        print(f"\n发现 {len(instances)} 个 UE 实例")
        
        # 测试 Python 执行
        print("\n测试 Python 执行...")
        result = self.execute_python("print('Hello from CLI!')")
        
        return True


def main():
    client = UERemoteClient()
    
    try:
        client.connect()
        
        # 发现 UE
        instances = client.discover()
        
        if instances:
            print(f"\n发现 {len(instances)} 个 UE 实例")
            
            # 测试简单命令
            print("\n" + "=" * 60)
            print("测试 1: 获取 UE 版本")
            print("=" * 60)
            result = client.execute_python(
                "import unreal; print(unreal.SystemLibrary.get_engine_version())"
            )
            
            print("\n" + "=" * 60)
            print("测试 2: 获取项目名称")
            print("=" * 60)
            result = client.execute_python(
                "import unreal; print(unreal.SystemLibrary.get_project_name())"
            )
            
            print("\n" + "=" * 60)
            print("测试 3: 列出 /Game 资产")
            print("=" * 60)
            result = client.execute_python(
                "import unreal; assets = unreal.EditorAssetLibrary.list_assets('/Game', recursive=False); print(f'Found {len(assets)} assets'); [print(f'  - {a}') for a in assets[:5]]"
            )
            
    finally:
        client.disconnect()
    
    print("\n测试完成!")


if __name__ == "__main__":
    main()
