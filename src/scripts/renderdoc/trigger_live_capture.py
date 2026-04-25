from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from mcps.renderdoc_mcp.mcp_server.bridge.client import RenderDocBridge, RenderDocBridgeError


def _find_widget(ui_state: dict, widget_name: str) -> dict | None:
    for widget in ui_state.get("widgets", []):
        if widget.get("name") == widget_name:
            return widget
    return None


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Trigger the current RenderDoc Live Capture button through TAAgent's bridge.",
    )
    parser.add_argument(
        "--button-name",
        default="triggerImmediateCapture",
        choices=(
            "triggerImmediateCapture",
            "triggerDelayedCapture",
            "queueCap",
            "cycleActiveWindow",
        ),
        help="RenderDoc Live Capture button objectName to click.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Emit machine-readable JSON instead of human-readable text.",
    )
    return parser.parse_args()


def _emit(payload: dict, as_json: bool) -> None:
    if as_json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return

    print(f"[TAAgent] Target: {payload['target']}")
    print(f"[TAAgent] Connection: {payload['connection_status']}")
    print(
        f"[TAAgent] Button: {payload['button_name']} "
        f"(enabled={payload['button_enabled']}, visible={payload['button_visible']})"
    )
    print(f"[TAAgent] Trigger dispatched successfully in window: {payload['window_title']}")


def main() -> int:
    args = _parse_args()
    bridge = RenderDocBridge()

    try:
        ui_state = bridge.call("get_live_capture_ui_state")
    except RenderDocBridgeError as exc:
        message = str(exc)
        if "Method not found" in message:
            print(
                "[TAAgent] Current RenderDoc instance does not expose live-capture trigger methods yet. "
                "Restart RenderDoc through menu [6] and reopen the Live Capture window.",
                file=sys.stderr,
            )
            return 2

        print(f"[TAAgent] Failed to query RenderDoc Live Capture state: {message}", file=sys.stderr)
        return 2

    button = _find_widget(ui_state, args.button_name)
    connection_status = _find_widget(ui_state, "connectionStatus")
    target = _find_widget(ui_state, "target")

    if button is None:
        print(
            f"[TAAgent] RenderDoc Live Capture button was not found: {args.button_name}. "
            "Open the Live Capture window for the running target first.",
            file=sys.stderr,
        )
        return 3

    if connection_status is None or connection_status.get("text") != "Established":
        current = None if connection_status is None else connection_status.get("text")
        print(
            f"[TAAgent] Live Capture connection is not ready. Current status: {current or 'missing'}. "
            "Wait until the window shows Established before triggering capture.",
            file=sys.stderr,
        )
        return 4

    if not button.get("visible"):
        print(
            f"[TAAgent] Live Capture button is currently hidden: {args.button_name}.",
            file=sys.stderr,
        )
        return 5

    if not button.get("enabled"):
        print(
            f"[TAAgent] Live Capture button is disabled: {args.button_name}. "
            "RenderDoc is connected, but the target is not yet in a capturable state.",
            file=sys.stderr,
        )
        return 6

    try:
        trigger_result = bridge.call("trigger_live_capture", {"button_name": args.button_name})
    except RenderDocBridgeError as exc:
        print(f"[TAAgent] Failed to trigger RenderDoc Live Capture: {exc}", file=sys.stderr)
        return 7

    if not trigger_result.get("success"):
        print(
            f"[TAAgent] RenderDoc rejected the trigger request: {trigger_result}",
            file=sys.stderr,
        )
        return 8

    payload = {
        "button_name": args.button_name,
        "button_enabled": bool(button.get("enabled")),
        "button_visible": bool(button.get("visible")),
        "connection_status": connection_status.get("text", ""),
        "target": "" if target is None else target.get("text", ""),
        "window_title": trigger_result.get("window_title", button.get("window_title", "")),
        "trigger_result": trigger_result,
    }
    _emit(payload, args.json)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())