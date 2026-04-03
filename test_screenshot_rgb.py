"""Manual helper for testing viewport screenshot RGB output."""

from __future__ import annotations

import base64
import importlib.util
from pathlib import Path


def _load_viewport_tool():
    repo_root = Path(__file__).resolve().parent
    viewport_path = repo_root / "mcps" / "unreal_render_mcp" / "tools" / "viewport.py"
    spec = importlib.util.spec_from_file_location("taagent_viewport_tool", viewport_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load viewport tool from {viewport_path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    viewport = _load_viewport_tool()
    output_dir = Path(__file__).resolve().parent / "output"
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 50)
    print("Test 1: RGB screenshot output")
    result = viewport.get_viewport_screenshot(
        output_path=str(output_dir / "test_rgb.png"),
        output_mode="rgb",
        format="png",
    )

    if result.get("success"):
        print("Screenshot captured successfully")
        print(f"Image size: {result.get('width')} x {result.get('height')}")

        pixel_data_b64 = result.get("pixel_data")
        if pixel_data_b64:
            raw_bytes = base64.b64decode(pixel_data_b64)
            width = result.get("width", 0)
            height = result.get("height", 0)
            center_idx = ((height // 2) * width + (width // 2)) * 3

            if center_idx + 2 < len(raw_bytes):
                r = raw_bytes[center_idx]
                g = raw_bytes[center_idx + 1]
                b = raw_bytes[center_idx + 2]
                print(f"Center pixel RGB: ({r}, {g}, {b})")
                print(f"Center pixel normalized: {r / 255:.3f}, {g / 255:.3f}, {b / 255:.3f}")
    else:
        print(f"Screenshot failed: {result.get('error', 'unknown error')}")

    print("\n" + "=" * 50)
    print("Test 2: file screenshot output")
    result2 = viewport.get_viewport_screenshot(
        output_path=str(output_dir / "test_file.png"),
        output_mode="file",
        format="png",
    )

    if result2.get("success"):
        print(f"File saved: {result2.get('output_path')}")
    else:
        print(f"Screenshot failed: {result2.get('error', 'unknown error')}")

    print("\nDone")


if __name__ == "__main__":
    main()
