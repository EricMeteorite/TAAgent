#!/usr/bin/env python3
"""
UE Python Remote Execution 客户端

基于 UE 官方协议实现:
- UDP 组播用于发现和连接建立
- TCP 用于实际命令执行

协议参考: PythonScriptRemoteExecution.cpp
"""

import socket
import json
import uuid
import time
import struct
import threading
from typing import Optional, Dict, Any, Tuple

# ============================================================================
# 协议常量 (来自 PythonScriptRemoteExecution.cpp)
# ============================================================================
_PROTOCOL_VERSION = 1
_PROTOCOL_MAGIC = 'ue_py'

# UDP 消息类型
_TYPE_PING = 'ping'                    # 发现请求
_TYPE_PONG = 'pong'                    # 发现响应
_TYPE_OPEN_CONNECTION = 'open_connection'   # 打开 TCP 连接
_TYPE_CLOSE_CONNECTION = 'close_connection' # 关闭 TCP 连接

# TCP 消息类型
_TYPE_COMMAND = 'command'              # 执行 Python 命令
_TYPE_COMMAND_RESULT = 'command_result' # 命令结果

# 执行模式
MODE_EXEC_FILE = 'ExecuteFile'         # 作为文件执行
MODE_EXEC_STATEMENT = 'ExecuteStatement'  # 作为语句执行
MODE_EVAL_STATEMENT = 'EvaluateStatement' # 作为表达式求值

# 默认配置
DEFAULT_MULTICAST_GROUP = '239.0.0.1'
DEFAULT_MULTICAST_PORT = 6766
DEFAULT_COMMAND_PORT = 6776


class UERemoteExecution:
    """
    UE Python Remote Execution 客户端
    
    使用方法:
        client = UERemoteExecution()
        client.start()
        client.remote_exec("print('Hello UE!')")
        client.stop()
    """
    
    def __init__(self,
                 multicast_group: str = DEFAULT_MULTICAST_GROUP,
                 multicast_port: int = DEFAULT_MULTICAST_PORT,
                 command_port: int = DEFAULT_COMMAND_PORT):
        
        self.multicast_group = multicast_group
        self.multicast_port = multicast_port
        self.command_port = command_port
        self.node_id = str(uuid.uuid4())
        
        # Socket
        self.udp_socket: Optional[socket.socket] = None
        self.tcp_server: Optional[socket.socket] = None
        self.tcp_client: Optional[socket.socket] = None
        
        # 状态
        self.running = False
        self.connected = False
        self.remote_node_id: Optional[str] = None
        
        # 结果回调
        self._result_callback = None
        self._last_result = None
        
    def start(self):
        """启动客户端"""
        print(f"[RemoteExec] 启动客户端 (ID: {self.node_id[:8]}...)")
        
        # 创建 UDP socket
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        # 绑定到组播端口
        self.udp_socket.bind(("", self.multicast_port))
        
        # 加入组播组
        group_bytes = socket.inet_aton(self.multicast_group)
        mreq = struct.pack('4sL', group_bytes, socket.INADDR_ANY)
        self.udp_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        # 创建 TCP 服务器 (等待 UE 连接)
        self.tcp_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.tcp_server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.tcp_server.bind(("", self.command_port))
        self.tcp_server.listen(1)
        self.tcp_server.setblocking(False)
        
        self.running = True
        
        # 启动监听线程
        self._listen_thread = threading.Thread(target=self._listen_loop, daemon=True)
        self._listen_thread.start()
        
        print(f"[RemoteExec] UDP 组播: {self.multicast_group}:{self.multicast_port}")
        print(f"[RemoteExec] TCP 命令端口: {self.command_port}")
        
    def stop(self):
        """停止客户端"""
        self.running = False
        
        if self.tcp_client:
            try:
                self.tcp_client.close()
            except:
                pass
            self.tcp_client = None
            
        if self.tcp_server:
            try:
                self.tcp_server.close()
            except:
                pass
            self.tcp_server = None
            
        if self.udp_socket:
            try:
                self.udp_socket.close()
            except:
                pass
            self.udp_socket = None
            
        self.connected = False
        print("[RemoteExec] 客户端已停止")
        
    def _send_udp(self, msg: Dict[str, Any]):
        """发送 UDP 消息"""
        data = json.dumps(msg).encode('utf-8')
        self.udp_socket.sendto(data, (self.multicast_group, self.multicast_port))
        
    def _listen_loop(self):
        """监听循环"""
        while self.running:
            try:
                # 检查 TCP 连接
                if not self.connected and self.tcp_server:
                    try:
                        client, addr = self.tcp_server.accept()
                        self.tcp_client = client
                        self.tcp_client.setblocking(False)
                        self.connected = True
                        print(f"[RemoteExec] TCP 已连接: {addr}")
                    except BlockingIOError:
                        pass
                        
                # 接收 UDP 消息
                if self.udp_socket:
                    try:
                        self.udp_socket.setblocking(False)
                        data, addr = self.udp_socket.recvfrom(65536)
                        msg = json.loads(data.decode('utf-8'))
                        self._handle_udp_message(msg, addr)
                    except BlockingIOError:
                        pass
                    except Exception as e:
                        pass
                        
                # 接收 TCP 消息
                if self.connected and self.tcp_client:
                    try:
                        data = self.tcp_client.recv(65536)
                        if data:
                            msg = json.loads(data.decode('utf-8'))
                            self._handle_tcp_message(msg)
                    except BlockingIOError:
                        pass
                    except Exception as e:
                        pass
                        
                time.sleep(0.01)
                
            except Exception as e:
                print(f"[RemoteExec] 监听错误: {e}")
                
    def _handle_udp_message(self, msg: Dict[str, Any], addr: Tuple[str, int]):
        """处理 UDP 消息"""
        msg_type = msg.get('message_type')
        
        if msg_type == _TYPE_PONG:
            # 收到 PONG，记录远程节点
            remote_id = msg.get('node_id')
            if remote_id and remote_id != self.node_id:
                print(f"[RemoteExec] 发现 UE: {remote_id[:8]}... @ {addr}")
                self.remote_node_id = remote_id
                
    def _handle_tcp_message(self, msg: Dict[str, Any]):
        """处理 TCP 消息"""
        msg_type = msg.get('message_type')
        
        if msg_type == _TYPE_COMMAND_RESULT:
            # 命令结果
            self._last_result = msg
            print(f"[RemoteExec] 命令结果: {msg.get('result', '')[:100]}...")
            
    def discover(self, timeout: float = 2.0) -> bool:
        """发现 UE 实例"""
        print(f"[RemoteExec] 发现 UE (超时: {timeout}s)...")
        
        # 发送 PING
        msg = {
            'protocol_version': _PROTOCOL_VERSION,
            'protocol_magic': _PROTOCOL_MAGIC,
            'message_type': _TYPE_PING,
            'node_id': self.node_id
        }
        self._send_udp(msg)
        
        # 等待 PONG
        start = time.time()
        while time.time() - start < timeout:
            if self.remote_node_id:
                print(f"[RemoteExec] 发现 UE: {self.remote_node_id[:8]}...")
                return True
            time.sleep(0.1)
            
        print("[RemoteExec] 未发现 UE 实例")
        return False
        
    def open_connection(self, timeout: float = 5.0) -> bool:
        """打开 TCP 命令连接"""
        print(f"[RemoteExec] 打开 TCP 连接...")
        
        # 发送 OPEN_CONNECTION
        msg = {
            'protocol_version': _PROTOCOL_VERSION,
            'protocol_magic': _PROTOCOL_MAGIC,
            'message_type': _TYPE_OPEN_CONNECTION,
            'node_id': self.node_id,
            'command_endpoint': ['127.0.0.1', self.command_port]
        }
        self._send_udp(msg)
        
        # 等待 TCP 连接
        start = time.time()
        while time.time() - start < timeout:
            if self.connected:
                print("[RemoteExec] TCP 连接成功!")
                return True
            time.sleep(0.1)
            
        print("[RemoteExec] TCP 连接超时")
        return False
        
    def remote_exec(self, command: str, mode: str = MODE_EXEC_STATEMENT, timeout: float = 10.0) -> Optional[Dict[str, Any]]:
        """执行 Python 命令"""
        if not self.connected:
            print("[RemoteExec] 未连接，请先调用 open_connection()")
            return None
            
        print(f"[RemoteExec] 执行: {command[:60]}...")
        
        # 发送命令
        msg = {
            'protocol_version': _PROTOCOL_VERSION,
            'protocol_magic': _PROTOCOL_MAGIC,
            'message_type': _TYPE_COMMAND,
            'node_id': self.node_id,
            'command': command,
            'execution_mode': mode
        }
        
        data = json.dumps(msg).encode('utf-8')
        self.tcp_client.sendall(data)
        
        # 等待结果
        self._last_result = None
        start = time.time()
        while time.time() - start < timeout:
            if self._last_result:
                return self._last_result
            time.sleep(0.1)
            
        print("[RemoteExec] 等待结果超时")
        return None
        
    def close_connection(self):
        """关闭 TCP 连接"""
        if self.tcp_client:
            self.tcp_client.close()
            self.tcp_client = None
        self.connected = False
        print("[RemoteExec] TCP 连接已关闭")


# ============================================================================
# 测试
# ============================================================================
def test():
    print("=" * 60)
    print("UE Python Remote Execution 测试")
    print("=" * 60)
    
    client = UERemoteExecution()
    
    try:
        # 启动
        client.start()
        time.sleep(0.5)
        
        # 发现
        if not client.discover(timeout=3.0):
            print("\n未发现 UE，请确保:")
            print("1. UE 编辑器正在运行")
            print("2. 已启用 Python Script Plugin")
            print("3. 已启用 Remote Execution (Edit -> Project Settings -> Python -> Remote Execution)")
            return
            
        # 打开连接
        if not client.open_connection(timeout=5.0):
            print("连接失败")
            return
            
        # 测试命令
        print("\n" + "=" * 60)
        print("测试 1: 获取引擎版本")
        print("=" * 60)
        result = client.remote_exec("import unreal; print(unreal.SystemLibrary.get_engine_version())")
        print(f"结果: {result}")
        
        print("\n" + "=" * 60)
        print("测试 2: 获取项目名称")
        print("=" * 60)
        result = client.remote_exec("import unreal; print(unreal.SystemLibrary.get_project_name())")
        print(f"结果: {result}")
        
        print("\n" + "=" * 60)
        print("测试 3: 列出 /Game 资产")
        print("=" * 60)
        result = client.remote_exec('''
import unreal
assets = unreal.EditorAssetLibrary.list_assets('/Game', recursive=False)
print(f'Found {len(assets)} assets')
for a in assets[:5]:
    print(f'  - {a}')
''', mode=MODE_EXEC_FILE)
        print(f"结果: {result}")
        
    finally:
        client.stop()
    
    print("\n测试完成!")


if __name__ == "__main__":
    test()
