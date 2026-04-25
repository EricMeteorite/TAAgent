"""
Capture management service for RenderDoc.
"""

import renderdoc as rd

try:
    from PySide2.QtWidgets import QApplication, QAbstractButton, QLabel
except ImportError:  # pragma: no cover - only available inside RenderDoc UI
    QApplication = None
    QAbstractButton = None
    QLabel = None


class CaptureManager:
    """Capture management service"""

    def __init__(self, ctx, invoke_fn):
        self.ctx = ctx
        self._invoke = invoke_fn

    def get_capture_status(self):
        """Check if a capture is loaded and get API info"""
        if not self.ctx.IsCaptureLoaded():
            return {"loaded": False}

        result = {"loaded": True, "api": None, "filename": None}

        try:
            result["filename"] = self.ctx.GetCaptureFilename()
        except Exception:
            pass

        # Get API type via replay
        def callback(controller):
            try:
                props = controller.GetAPIProperties()
                result["api"] = str(props.pipelineType)
            except Exception:
                pass

        self._invoke(callback)
        return result

    def list_captures(self, directory):
        """
        List all .rdc files in the specified directory.

        Args:
            directory: Directory path to search

        Returns:
            dict with 'captures' list containing file info
        """
        import os
        import datetime

        # Validate directory exists
        if not os.path.isdir(directory):
            raise ValueError("Directory not found: %s" % directory)

        captures = []

        try:
            for filename in os.listdir(directory):
                if filename.lower().endswith(".rdc"):
                    filepath = os.path.join(directory, filename)
                    if os.path.isfile(filepath):
                        stat = os.stat(filepath)
                        # Format timestamp as ISO 8601
                        mtime = datetime.datetime.fromtimestamp(stat.st_mtime)
                        captures.append({
                            "filename": filename,
                            "path": filepath,
                            "size_bytes": stat.st_size,
                            "modified_time": mtime.isoformat(),
                        })
        except Exception as e:
            raise ValueError("Failed to list directory: %s" % str(e))

        # Sort by modified time (newest first)
        captures.sort(key=lambda x: x["modified_time"], reverse=True)

        return {
            "directory": directory,
            "count": len(captures),
            "captures": captures,
        }

    def open_capture(self, capture_path):
        """
        Open a capture file in RenderDoc.

        Args:
            capture_path: Full path to the .rdc file

        Returns:
            dict with success status and capture info
        """
        import os

        # Validate file exists
        if not os.path.isfile(capture_path):
            raise ValueError("Capture file not found: %s" % capture_path)

        # Validate extension
        if not capture_path.lower().endswith(".rdc"):
            raise ValueError("Invalid file type. Expected .rdc file: %s" % capture_path)

        # Create ReplayOptions with defaults
        opts = rd.ReplayOptions()

        # Open the capture
        # LoadCapture will automatically close any existing capture
        try:
            self.ctx.LoadCapture(
                capture_path,   # captureFile
                opts,           # ReplayOptions
                capture_path,   # origFilename (same as capture path)
                False,          # temporary (False = permanent load)
                True,           # local (True = local file)
            )
        except Exception as e:
            raise ValueError("Failed to open capture: %s" % str(e))

        # Verify the capture was loaded
        if not self.ctx.IsCaptureLoaded():
            raise ValueError("Failed to load capture (unknown error)")

        # Get capture info
        result = {
            "success": True,
            "capture_path": capture_path,
            "filename": os.path.basename(capture_path),
        }

        # Get API type if possible (may require replay thread)
        try:
            api_result = {"api": None}

            def callback(controller):
                try:
                    props = controller.GetAPIProperties()
                    api_result["api"] = str(props.pipelineType)
                except Exception:
                    pass

            self._invoke(callback)
            if api_result["api"]:
                result["api"] = api_result["api"]
        except Exception:
            pass

        return result

    def get_live_capture_ui_state(self):
        """Inspect live-capture related widgets in the current RenderDoc UI."""
        if QApplication is None:
            raise ValueError("PySide2 is not available in this RenderDoc environment")

        app = QApplication.instance()
        if app is None:
            raise ValueError("QApplication is not available")

        widget_names = {
            "triggerImmediateCapture",
            "triggerDelayedCapture",
            "queueCap",
            "cycleActiveWindow",
            "connectionStatus",
            "target",
        }

        widgets = []
        for widget in app.allWidgets():
            name = widget.objectName()
            if name not in widget_names:
                continue

            entry = {
                "name": name,
                "class": type(widget).__name__,
                "enabled": bool(widget.isEnabled()),
                "visible": bool(widget.isVisible()),
                "window_title": widget.window().windowTitle(),
            }

            if QAbstractButton is not None and isinstance(widget, QAbstractButton):
                entry["text"] = widget.text()
            elif QLabel is not None and isinstance(widget, QLabel):
                entry["text"] = widget.text()
            else:
                try:
                    entry["text"] = widget.text()
                except Exception:
                    entry["text"] = ""

            widgets.append(entry)

        widgets.sort(key=lambda item: (item["window_title"], item["name"]))
        return {"widgets": widgets, "count": len(widgets)}

    def trigger_live_capture(self, button_name="triggerImmediateCapture"):
        """Programmatically click a live capture button."""
        if QApplication is None or QAbstractButton is None:
            raise ValueError("PySide2 button interaction is not available")

        app = QApplication.instance()
        if app is None:
            raise ValueError("QApplication is not available")

        candidates = []
        for widget in app.allWidgets():
            if not isinstance(widget, QAbstractButton):
                continue
            if widget.objectName() != button_name:
                continue
            candidates.append(widget)

        if not candidates:
            return {
                "success": False,
                "reason": "button_not_found",
                "button_name": button_name,
            }

        preferred = None
        for widget in candidates:
            if widget.isVisible() and widget.isEnabled():
                preferred = widget
                break

        if preferred is None:
            preferred = candidates[0]

        result = {
            "button_name": button_name,
            "enabled": bool(preferred.isEnabled()),
            "visible": bool(preferred.isVisible()),
            "window_title": preferred.window().windowTitle(),
            "text": preferred.text(),
        }

        if not preferred.isEnabled():
            result["success"] = False
            result["reason"] = "button_disabled"
            return result

        preferred.click()
        result["success"] = True
        return result
