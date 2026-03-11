#!/usr/bin/env python3
"""
UE Remote Python 客户端

基于 UE 官方 remote_execution.py 协议实现
"""

import socket
import json
import uuid
import time
import struct
import threading
from typing import Optional, Dict, Any, Tuple

# 协议常量
_PROTOCOL_VERSION = 1
_PROTOCOL_MAGIC = 'ue_py'
_TYPE_PING = 'ping'
_TYPE_PONG = 'pong'
_TYPE_OPEN_CONNECTION = 'open_connection'
_TYPE_CLOSE_CONNECTION = 'close_connection'
_TYPE_COMMAND = 'command'
_TYPE_COMMAND_RESULT = 'command_result'

# 执行模式
MODE_EXEC_FILE = 'ExecuteFile'
MODE_EXEC_STATEMENT = 'ExecuteStatement'
MODE_EVAL_STATEMENT = 'EvaluateStatement'

# 默认配置
DEFAULT_MULTICAST_GROUP = '239.0.0.1'
DEFAULT_MULTICAST_PORT = 6766
DEFAULT_COMMAND_PORT = 6776


class UERemoteClient:
    """UE Remote Python 客户端"""
    
    def __init__(self, 
                 multicast_group: str = DEFAULT_MULTICAST_GROUP,
                 multicast_port: int = DEFAULT_MULTICAST_PORT,
                 command_port: int = DEFAULT_COMMAND_PORT):
        
        self.multicast_group = multicast_group
        self.multicast_port = multicast_port
        self.command_port = command_port
        self.node_id = str(uuid.uuid4())
        
        # Sockets
        self.udp_socket = None
        self.tcp_socket = None
        self.tcp_server = None
        
        # Remote nodes
        self.remote_nodes = {}
        
        # Connection state
        self.connected = False
        self.command_connection = None
        
    def start(self):
        """启动客户端"""
        print(f"启动 UE Remote Client (Node ID: {self.node_id[:8]}...)")
        
        # 创建 UDP socket
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.udp_socket.bind(("", self.multicast_port))
        
        # 加入组播组
        group_bytes = socket.inet_aton(self.multicast_group)
        mreq = struct.pack('4sL', group_bytes, socket.INADDR_ANY)
        self.udp_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        # 创建 TCP server
        self.tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.tcp_server.bind(("", self.command_port))
        self.tcp_server.listen(1)
        self.tcp_server.settimeout(1.0)
        
        print(f"  UDP 组播: {self.multicast_group}:{self.multicast_port}")
        print(f"  TCP 命令端口: {self.command_port}")
        
    def stop(self):
        """停止客户端"""
        if self.tcp_socket:
            self.tcp_socket.close()
            self.tcp_socket = None
        if self.tcp_server:
            self.tcp_server.close()
            self.tcp_server = None
        if self.udp_socket:
            self.udp_socket.close()
            self.udp_socket = None
        print("客户端已停止")
        
    def send_udp_message(self, msg: Dict[str, Any]):
        """发送 UDP 消息"""
        data = json.dumps(msg).encode('utf-8')
        self.udp_socket.sendto(data, (self.multicast_group, self.multicast_port))
        
    def receive_udp_message(self, timeout: float = 1.0) -> Optional[Tuple[Dict[str, Any], Tuple[str, int]]]:
        """接收 UDP 消息"""
        self.udp_socket.settimeout(timeout)
        try:
            data, addr = self.udp_socket.recvfrom(65536)
            return json.loads(data.decode('utf-8')), addr
        except socket.timeout:
            return None
        except Exception as e:
            print(f"  接收错误: {e}")
            return None
            
    def discover(self, timeout: float = 2.0) -> Dict[str, Any]:
        """发现 UE 实例"""
        print("发现 UE 实例...")
        
        # 发送 PING
        msg = {
            "protocol_version": _PROTOCOL_VERSION,
            "protocol_magic": _PROTOCOL_MAGIC,
            "message_type": _TYPE_PING,
            "node_id": self.node_id
        }
        self.send_udp_message(msg)
        
        # 收集 PONG 响应
        start_time = time.time()
        while time.time() - start_time < timeout:
            result = self.receive_udp_message(timeout=0.5)
            if result:
                response, addr = result
                if (response.get("message_type") == _TYPE_PONG and 
                    response.get("node_id") != self.node_id):
                    
                    remote_id = response.get("node_id")
                    self.remote_nodes[remote_id] = {
                        "address": addr,
                        "data": response
                    }
                    print(f"  发现 UE: {remote_id[:8]}... @ {addr}")
        
        print(f"共发现 {len(self.remote_nodes)} 个 UE 实例")
        return self.remote_nodes
        
    def open_connection(self, remote_node_id: str = None, timeout: float = 5.0) -> bool:
        """打开 TCP 命令连接"""
        if not self.remote_nodes:
            print("没有发现远程节点,请先调用 discover()")
            return False
            
        # 选择远程节点
        if remote_node_id is None:
            remote_node_id = list(self.remote_nodes.keys())[0]
            
        if remote_node_id not in self.remote_nodes:
            print(f"远程节点 {remote_node_id} 未找到")
            return False
            
        print(f"打开命令连接: {remote_node_id[:8]}...")
        
        # 发送 OPEN_CONNECTION
        msg = {
            "protocol_version": _PROTOCOL_VERSION,
            "protocol_magic": _PROTOCOL_MAGIC,
            "message_type": _TYPE_OPEN_CONNECTION,
            "node_id": self.node_id,
            "command_endpoint": ["127.0.0.1", self.command_port]
        }
        self.send_udp_message(msg)
        
        # 等待 TCP 连接
        print(f"  等待 TCP 连接...")
        try:
            self.tcp_server.settimeout(timeout)
            self.tcp_socket, addr = self.tcp_server.accept()
            print(f"  TCP 已连接: {addr}")
            self.connected = True
            return True
        except socket.timeout:
            print("  超时,连接失败")
            return False
        except Exception as e:
            print(f"  连接错误: {e}")
            return False
            
    def execute_command(self, command: str, mode: str = MODE_EXEC_STATEMENT, timeout: float = 10.0) -> Dict[str, Any]:
        """执行 Python 命令"""
        if not self.connected or not self.tcp_socket:
            print("未连接,请先调用 open_connection()")
            return {"error": "not connected"}
            
        print(f"执行命令: {command[:60]}...")
        
        # 构建命令消息
        msg = {
            "protocol_version": _PROTOCOL_VERSION,
            "protocol_magic": _PROTOCOL_MAGIC,
            "message_type": _TYPE_COMMAND,
            "node_id": self.node_id,
            "command": command,
            "execution_mode": mode
        }
        
        # 发送
        data = json.dumps(msg).encode('utf-8')
        self.tcp_socket.sendall(data)
        
        # 接收结果
        self.tcp_socket.settimeout(timeout)
        try:
            chunks = []
            while True:
                chunk = self.tcp_socket.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
                
                # 尝试解析
                try:
                    result_data = b''.join(chunks)
                    result = json.loads(result_data.decode('utf-8'))
                    if result.get("message_type") == _TYPE_COMMAND_RESULT:
                        return result
                except json.JSONDecodeError:
                    continue
                    
            return {"error": "no valid response"}
            
        except socket.timeout:
            return {"error": "timeout"}
        except Exception as e:
            return {"error": str(e)}
            
    def close_connection(self):
        """关闭命令连接"""
        if self.tcp_socket:
            self.tcp_socket.close()
            self.tcp_socket = None
        self.connected = False
        print("命令连接已关闭")


def test():
    """测试 UE Remote Python"""
    print("=" * 60)
    print("UE Remote Python 客户端测试")
    print("=" * 60)
    
    client = UERemoteClient()
    
    try:
        # 启动
        client.start()
        
        # 发现
        client.discover(timeout=3.0)
        
        if not client.remote_nodes:
            print("没有发现 UE 实例")
            return
            
        # 打开连接
        if not client.open_connection(timeout=5.0):
            print("连接失败")
            return
            
        # 测试命令
        print("\n" + "=" * 60)
        print("测试 1: 获取 UE 版本")
        print("=" * 60)
        result = client.execute_command(
            "import unreal; print(unreal.SystemLibrary.get_engine_version())"
        )
        print(f"结果: {json.dumps(result, indent=2, ensure_ascii=False)}")
        
        print("\n" + "=" * 60)
        print("测试 2: 获取项目名称")
        print("=" * 60)
        result = client.execute_command(
            "import unreal; print(unreal.SystemLibrary.get_project_name())"
        )
        print(f"结果: {json.dumps(result, indent=2, ensure_ascii=False)}")
        
        print("\n" + "=" * 60)
        print("测试 3: 列出 /Game 资产")
        print("=" * 60)
        result = client.execute_command(
            "import unreal; assets = unreal.EditorAssetLibrary.list_assets('/Game', recursive=False); print(f'Found {len(assets)} assets'); [print(a) for a in assets[:5]]"
        )
        print(f"结果: {json.dumps(result, indent=2, ensure_ascii=False)}")
        
        print("\n" + "=" * 60)
        print("测试 4: 创建材质")
        print("=" * 60)
        result = client.execute_command('''
import unreal

# 创建材质
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
try:
    material = asset_tools.create_asset(
        asset_name="CLI_TestMaterial",
        package_path="/Game/Materials",
        asset_class=unreal.Material,
        factory=unreal.MaterialFactoryNew()
    )
    if material:
        unreal.EditorAssetLibrary.save_loaded_asset(material)
        print(f"Created material: /Game/Materials/CLI_TestMaterial")
    else:
        print("Failed to create material")
except Exception as e:
    print(f"Error: {e}")
''', mode=MODE_EXEC_FILE)
        print(f"结果: {json.dumps(result, indent=2, ensure_ascii=False)}")
        
    finally:
        client.stop()
    
    print("\n测试完成!")


if __name__ == "__main__":
    test()
