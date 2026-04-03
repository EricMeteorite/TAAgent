#!/usr/bin/env python3
"""
Directly ping UE remote execution over UDP without using multicast discovery.
"""

from __future__ import annotations

import json
import socket
import time

UE_HOST = "127.0.0.1"
UE_PORT = 6766


def main() -> None:
    print("=" * 60)
    print(f"Direct UE remote execution probe: {UE_HOST}:{UE_PORT}")
    print("=" * 60)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    try:
        sock.bind(("127.0.0.1", 0))
        local_port = sock.getsockname()[1]
        print(f"Local port: {local_port}")

        ping_msg = {
            "protocol_version": 1,
            "protocol_magic": "ue_py",
            "message_type": "ping",
            "node_id": "test-client-direct",
        }

        print("\nSending PING...")
        sock.sendto(json.dumps(ping_msg).encode("utf-8"), (UE_HOST, UE_PORT))

        print("Waiting for responses (5s)...")
        start = time.time()
        while time.time() - start < 5:
            try:
                data, addr = sock.recvfrom(65536)
                msg = json.loads(data.decode("utf-8"))
                msg_type = msg.get("message_type")
                node_id = msg.get("node_id", "unknown")

                print(f"\nReceived message from {addr}:")
                print(f"  type: {msg_type}")
                print(f"  node_id: {node_id}")
                print(f"  payload:\n{json.dumps(msg, indent=2)}")

                if node_id != "test-client-direct":
                    print("\nResponse appears to come from Unreal.")
            except socket.timeout:
                continue
            except ConnectionResetError as exc:
                print(f"\nConnection reset by peer: {exc}")
                break
    finally:
        sock.close()

    print("\nDone")


if __name__ == "__main__":
    main()
