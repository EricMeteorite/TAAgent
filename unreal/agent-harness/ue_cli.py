#!/usr/bin/env python3
"""
UE CLI - 通过 MCP 控制虚幻引擎

支持命令:
- info: 获取引擎信息
- actors: 列出所有 Actor
- assets: 列出资产
- create_material: 创建材质
- spawn: 生成 Actor
- screenshot: 截图
"""

import socket
import json
import click

# MCP 配置
MCP_HOST = "127.0.0.1"
MCP_PORT = 55557


class UEClient:
    """UE MCP 客户端"""
    
    def __init__(self, host: str = MCP_HOST, port: int = MCP_PORT):
        self.host = host
        self.port = port
        self.socket = None
        
    def connect(self) -> bool:
        """连接到 UE"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)
            self.socket.connect((self.host, self.port))
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
            
    def disconnect(self):
        """断开连接"""
        if self.socket:
            self.socket.close()
            self.socket = None
            
    def send_command(self, command_type: str, params: dict = None) -> dict:
        """发送命令"""
        if not self.socket:
            if not self.connect():
                return {"status": "error", "error": "连接失败"}
                
        try:
            # 发送
            msg = json.dumps({
                "type": command_type,
                "params": params or {}
            })
            self.socket.sendall(msg.encode('utf-8'))
            
            # 接收
            chunks = []
            while True:
                chunk = self.socket.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
                
                try:
                    data = b''.join(chunks)
                    return json.loads(data.decode('utf-8'))
                except json.JSONDecodeError:
                    continue
                    
            return {"status": "error", "error": "无响应"}
            
        except Exception as e:
            return {"status": "error", "error": str(e)}
        finally:
            self.disconnect()


@click.group()
def cli():
    """UE CLI - 虚幻引擎命令行工具"""
    pass


@cli.command()
def test():
    """测试 MCP 连接"""
    print("=" * 60)
    print("测试 UE MCP 连接")
    print("=" * 60)
    
    client = UEClient()
    
    if client.connect():
        print("连接成功!")
        
        # 获取 actors
        result = client.send_command("get_actors")
        if result.get("status") == "success":
            actors = result.get("result", {}).get("actors", [])
            print(f"\n场景中有 {len(actors)} 个 Actor")
        else:
            print(f"错误: {result}")
    else:
        print("连接失败")


@cli.command()
def info():
    """获取引擎信息"""
    client = UEClient()
    
    # 获取当前关卡
    result = client.send_command("get_current_level")
    print(json.dumps(result, indent=2, ensure_ascii=False))


@cli.command()
def actors():
    """列出所有 Actor"""
    client = UEClient()
    result = client.send_command("get_actors")
    
    if result.get("status") == "success":
        actors = result.get("result", {}).get("actors", [])
        print(f"场景中有 {len(actors)} 个 Actor:\n")
        for actor in actors[:20]:  # 只显示前20个
            print(f"  - {actor['name']} ({actor['class']})")
        if len(actors) > 20:
            print(f"  ... 还有 {len(actors) - 20} 个")
    else:
        print(f"错误: {result}")


@cli.command()
@click.option('--path', '-p', default='/Game', help='资产路径')
@click.option('--recursive/--no-recursive', default=False, help='递归搜索')
def assets(path, recursive):
    """列出资产"""
    client = UEClient()
    result = client.send_command("get_assets", {
        "path": path,
        "recursive": recursive
    })
    
    if result.get("status") == "success":
        assets = result.get("result", {}).get("assets", [])
        print(f"{path} 中有 {len(assets)} 个资产:\n")
        for asset in assets[:20]:
            print(f"  - {asset}")
        if len(assets) > 20:
            print(f"  ... 还有 {len(assets) - 20} 个")
    else:
        print(f"错误: {result}")


@cli.command()
@click.option('--path', '-p', default='/Game/Materials', help='材质路径')
@click.option('--name', '-n', default='CLI_Material', help='材质名称')
def create_material(path, name):
    """创建材质"""
    client = UEClient()
    result = client.send_command("create_asset", {
        "asset_type": "Material",
        "name": name,
        "path": path
    })
    
    print(json.dumps(result, indent=2, ensure_ascii=False))


@cli.command()
@click.option('--type', '-t', default='PointLight', help='Actor 类型')
@click.option('--location', '-l', default='0,0,100', help='位置 (x,y,z)')
@click.option('--name', '-n', default=None, help='Actor 名称')
def spawn(type, location, name):
    """生成 Actor"""
    loc = [float(x) for x in location.split(',')]
    
    client = UEClient()
    result = client.send_command("spawn_actor", {
        "actor_class": type,
        "location": {"x": loc[0], "y": loc[1], "z": loc[2]},
        "actor_name": name
    })
    
    print(json.dumps(result, indent=2, ensure_ascii=False))


@cli.command()
@click.option('--output', '-o', default='output/cli_screenshot.png', help='输出路径')
def screenshot(output):
    """截取视口"""
    client = UEClient()
    result = client.send_command("get_viewport_screenshot", {
        "output_path": output
    })
    
    print(json.dumps(result, indent=2, ensure_ascii=False))


@cli.command()
@click.option('--name', '-n', default='CLI_Level', help='关卡名称')
def create_level(name):
    """创建关卡"""
    client = UEClient()
    result = client.send_command("create_level", {
        "level_name": name
    })
    
    print(json.dumps(result, indent=2, ensure_ascii=False))


@cli.command()
def save():
    """保存当前关卡"""
    client = UEClient()
    result = client.send_command("save_current_level")
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    cli()
