"""Read-only Unreal Insights trace analysis tools.

The tools in this module use Unreal Insights' own headless export commands.  They
do not parse ``.utrace`` files themselves and never modify the source trace.
"""

from __future__ import annotations

import csv
import fnmatch
import json
import logging
import os
import platform
import re
import subprocess
import time
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional


logger = logging.getLogger("UnrealRenderMCP")

_REPO_ROOT = Path(__file__).resolve().parents[3]
_TIME_COLUMNS = {"Incl", "I.Min", "I.Max", "I.Avg", "I.Med", "Excl", "E.Min", "E.Max", "E.Avg", "E.Med", "Duration"}
_NUMERIC_COLUMNS = _TIME_COLUMNS | {"Count", "C.Avg", "ThreadId", "TimerId", "StartTime", "EndTime", "Duration", "Depth"}


def _powershell_json(script: str) -> Any:
    if platform.system() != "Windows":
        return None
    try:
        completed = subprocess.run(
            ["powershell", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
            check=False,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        if completed.returncode != 0 or not completed.stdout.strip():
            return None
        return json.loads(completed.stdout)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError):
        return None


def _running_insights_sessions() -> List[Dict[str, Any]]:
    data = _powershell_json(
        "@(Get-CimInstance Win32_Process -Filter \"Name='UnrealInsights.exe'\" | "
        "Select-Object ProcessId,ExecutablePath,CommandLine) | ConvertTo-Json -Compress"
    )
    if not data:
        return []
    rows = data if isinstance(data, list) else [data]
    sessions: List[Dict[str, Any]] = []
    for row in rows:
        command_line = row.get("CommandLine") or ""
        trace_id_match = re.search(r"-OpenTraceId=(?:\"([^\"]+)\"|(\S+))", command_line, re.IGNORECASE)
        trace_file_match = re.search(r"-OpenTraceFile=(?:\"([^\"]+)\"|(\S+))", command_line, re.IGNORECASE)
        store_match = re.search(r"-Store=(?:\"([^\"]+)\"|(\S+))", command_line, re.IGNORECASE)
        sessions.append(
            {
                "process_id": row.get("ProcessId"),
                "executable_path": row.get("ExecutablePath"),
                "trace_id": next((value for value in trace_id_match.groups() if value), None) if trace_id_match else None,
                "trace_file": next((value for value in trace_file_match.groups() if value), None) if trace_file_match else None,
                "store": next((value for value in store_match.groups() if value), None) if store_match else None,
                "command_line": command_line,
            }
        )
    return sessions


def _read_project_dir() -> Optional[Path]:
    candidates = []
    runtime_root = os.environ.get("TAAGENT_RUNTIME_ROOT")
    if runtime_root:
        candidates.append(Path(runtime_root) / "config" / "taagent_user.cfg")
    candidates.append(_REPO_ROOT / ".taagent-local" / "config" / "taagent_user.cfg")
    for config_path in candidates:
        try:
            for line in config_path.read_text(encoding="utf-8-sig").splitlines():
                if line.startswith("PROJECT_DIR="):
                    project_dir = Path(line.split("=", 1)[1].strip())
                    if project_dir.is_dir():
                        return project_dir
        except OSError:
            continue
    return None


def _trace_roots(extra_roots: Optional[List[str]] = None) -> List[Path]:
    roots: List[Path] = []
    configured_store = os.environ.get("UNREAL_TRACE_STORE_DIR")
    if configured_store:
        roots.append(Path(configured_store))
    local_app_data = os.environ.get("LOCALAPPDATA")
    if local_app_data:
        roots.append(Path(local_app_data) / "UnrealEngine" / "Common" / "UnrealTrace" / "Store")
    project_dir = _read_project_dir()
    if project_dir:
        roots.extend((project_dir / "Saved" / "Profiling", project_dir / "Saved" / "TraceSessions"))
    roots.extend(Path(root) for root in (extra_roots or []))

    unique: List[Path] = []
    seen = set()
    for root in roots:
        try:
            resolved = root.expanduser().resolve()
        except OSError:
            resolved = root.expanduser().absolute()
        key = os.path.normcase(str(resolved))
        if key not in seen:
            seen.add(key)
            unique.append(resolved)
    return unique


def _find_traces(extra_roots: Optional[List[str]] = None) -> List[Path]:
    traces: List[Path] = []
    for root in _trace_roots(extra_roots):
        if not root.is_dir():
            continue
        try:
            traces.extend(path for path in root.rglob("*.utrace") if path.is_file())
        except OSError:
            logger.warning("Unable to enumerate Unreal trace root: %s", root)
    return sorted(traces, key=lambda path: path.stat().st_mtime, reverse=True)


def _find_unreal_insights(explicit_path: Optional[str] = None) -> Optional[Path]:
    candidates: List[Path] = []
    if explicit_path:
        candidates.append(Path(explicit_path))
    configured = os.environ.get("UNREAL_INSIGHTS_EXE")
    if configured:
        candidates.append(Path(configured))
    for session in _running_insights_sessions():
        if session.get("executable_path"):
            candidates.append(Path(session["executable_path"]))

    project_dir = _read_project_dir()
    if project_dir:
        source_root = project_dir.parent
        candidates.extend(source_root.glob("UnrealEngine*/Engine/Binaries/Win64/UnrealInsights.exe"))
        candidates.extend(source_root.glob("UE_*/Engine/Binaries/Win64/UnrealInsights.exe"))

    if platform.system() == "Windows":
        program_files = Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        candidates.extend(program_files.glob("Epic Games/UE_*/Engine/Binaries/Win64/UnrealInsights.exe"))

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    return None


def _safe_command_value(value: Optional[str], field_name: str) -> Optional[str]:
    if value is None:
        return None
    if any(char in value for char in ('"', "\r", "\n")):
        raise ValueError(f"{field_name} cannot contain quotes or newlines")
    return value


def _output_paths(prefix: str, output_dir: Optional[str]) -> tuple[Path, Path]:
    root = (
        Path(output_dir)
        if output_dir
        else Path(os.environ.get("TAAGENT_RUNTIME_ROOT", str(_REPO_ROOT / ".taagent-local"))) / "output" / "insights"
    )
    root = root.expanduser().resolve()
    root.mkdir(parents=True, exist_ok=True)
    stem = f"{prefix}_{time.strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex[:8]}"
    return root / f"{stem}.csv", root / f"{stem}.log"


def _run_export(
    trace_path: Path,
    command: str,
    csv_path: Path,
    log_path: Path,
    unreal_insights_path: Optional[str],
    timeout_seconds: int,
) -> Dict[str, Any]:
    insights_exe = _find_unreal_insights(unreal_insights_path)
    if not insights_exe:
        return {
            "success": False,
            "message": "UnrealInsights executable was not found. Set UNREAL_INSIGHTS_EXE or pass unreal_insights_path.",
        }
    if not trace_path.is_file():
        return {"success": False, "message": f"Trace file does not exist: {trace_path}"}

    # Use Insights' response-file support so Windows command-line quoting cannot
    # truncate an export command containing spaces or quoted output paths.
    response_path = log_path.with_suffix(".rsp")
    response_path.write_text(command + "\n", encoding="utf-8")
    if platform.system() == "Windows":
        # Unreal reads these values from its raw command line, so the quotes must
        # begin after '=' exactly as in Epic's own AutomationTool invocation.
        args: Any = (
            f'"{insights_exe}" '
            f'-OpenTraceFile="{trace_path}" -unattended -AutoQuit -NoUI -NullRHI '
            f'-ABSLOG="{log_path}" -log '
            f'-ExecOnAnalysisCompleteCmd="@={response_path}"'
        )
    else:
        args = [
            str(insights_exe),
            f"-OpenTraceFile={trace_path}",
            "-unattended",
            "-AutoQuit",
            "-NoUI",
            "-NullRHI",
            f"-ABSLOG={log_path}",
            "-log",
            f"-ExecOnAnalysisCompleteCmd=@={response_path}",
        ]
    started = time.monotonic()
    try:
        completed = subprocess.run(
            args,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=max(10, min(int(timeout_seconds), 1800)),
            check=False,
            cwd=str(response_path.parent),
            executable=str(insights_exe) if platform.system() == "Windows" else None,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
    except subprocess.TimeoutExpired:
        return {
            "success": False,
            "message": "Unreal Insights analysis timed out. The trace may still be recording or may need a longer timeout.",
            "trace_path": str(trace_path),
            "log_path": str(log_path),
            "response_path": str(response_path),
        }
    except OSError as exc:
        return {"success": False, "message": f"Failed to start Unreal Insights: {exc}"}

    elapsed = time.monotonic() - started
    if completed.returncode != 0 or not csv_path.is_file():
        log_tail = ""
        try:
            log_tail = "\n".join(log_path.read_text(encoding="utf-8", errors="replace").splitlines()[-40:])
        except OSError:
            pass
        return {
            "success": False,
            "message": "Unreal Insights did not produce the requested CSV export.",
            "return_code": completed.returncode,
            "trace_path": str(trace_path),
            "log_path": str(log_path),
            "response_path": str(response_path),
            "log_tail": log_tail,
        }
    return {
        "success": True,
        "trace_path": str(trace_path),
        "unreal_insights_path": str(insights_exe),
        "csv_path": str(csv_path),
        "log_path": str(log_path),
        "response_path": str(response_path),
        "analysis_seconds": round(elapsed, 3),
    }


def _read_csv_rows(csv_path: Path) -> List[Dict[str, Any]]:
    rows: List[Dict[str, Any]] = []
    with csv_path.open("r", encoding="utf-8-sig", errors="replace", newline="") as handle:
        for row in csv.DictReader(handle):
            converted: Dict[str, Any] = {}
            for key, raw_value in row.items():
                value = raw_value.strip() if isinstance(raw_value, str) else raw_value
                if key in _NUMERIC_COLUMNS and value not in (None, ""):
                    try:
                        number = float(value)
                        converted[key] = int(number) if key in {"Count", "ThreadId", "TimerId", "Depth"} and number.is_integer() else number
                    except ValueError:
                        converted[key] = value
                else:
                    converted[key] = value
            for key in _TIME_COLUMNS:
                if isinstance(converted.get(key), (int, float)):
                    converted[f"{key}_ms"] = converted[key] * 1000.0
            rows.append(converted)
    return rows


def _matches_name(name: str, name_filter: Optional[str]) -> bool:
    if not name_filter:
        return True
    lowered = name.casefold()
    for pattern in (part.strip() for part in name_filter.split(",")):
        if not pattern:
            continue
        folded_pattern = pattern.casefold()
        if any(char in folded_pattern for char in "*?["):
            if fnmatch.fnmatchcase(lowered, folded_pattern):
                return True
        elif folded_pattern in lowered:
            return True
    return False


def _summarize_timing_events(rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    grouped: Dict[tuple[str, str, str], List[float]] = {}
    for row in rows:
        duration = row.get("Duration")
        if not isinstance(duration, (int, float)):
            continue
        key = (
            str(row.get("ThreadName", "")),
            str(row.get("TimerId", "")),
            str(row.get("TimerName", "")),
        )
        grouped.setdefault(key, []).append(float(duration))

    summaries: List[Dict[str, Any]] = []
    for (thread_name, timer_id, timer_name), durations in grouped.items():
        durations.sort()
        count = len(durations)
        total = sum(durations)
        median = (
            durations[count // 2]
            if count % 2
            else (durations[count // 2 - 1] + durations[count // 2]) / 2.0
        )
        summaries.append(
            {
                "thread_name": thread_name,
                "timer_id": timer_id,
                "timer_name": timer_name,
                "count": count,
                "total_ms": total * 1000.0,
                "average_ms": (total / count) * 1000.0,
                "median_ms": median * 1000.0,
                "p95_ms": durations[min(count - 1, int(count * 0.95))] * 1000.0,
                "max_ms": durations[-1] * 1000.0,
            }
        )
    summaries.sort(key=lambda row: row["total_ms"], reverse=True)
    return summaries


def _select_trace(trace_path: Optional[str], search_roots: Optional[List[str]] = None) -> Optional[Path]:
    if trace_path:
        return Path(trace_path).expanduser().resolve()
    traces = _find_traces(search_roots)
    return traces[0] if traces else None


def list_unreal_insights_traces(
    search_roots: Optional[List[str]] = None,
    limit: int = 20,
) -> Dict[str, Any]:
    """List local ``.utrace`` files and currently running Unreal Insights sessions.

    Args:
        search_roots: Optional additional directories to search recursively.
        limit: Maximum number of traces to return (1-200).
    """
    now = time.time()
    traces = []
    all_traces = _find_traces(search_roots)
    for path in all_traces[: max(1, min(int(limit), 200))]:
        stat = path.stat()
        traces.append(
            {
                "path": str(path),
                "size_bytes": stat.st_size,
                "modified_unix": stat.st_mtime,
                "seconds_since_modified": round(max(0.0, now - stat.st_mtime), 3),
                "possibly_live": now - stat.st_mtime < 3.0,
            }
        )
    insights_path = _find_unreal_insights()
    return {
        "success": True,
        "trace_roots": [str(root) for root in _trace_roots(search_roots)],
        "trace_count": len(all_traces),
        "traces": traces,
        "running_sessions": _running_insights_sessions(),
        "unreal_insights_path": str(insights_path) if insights_path else None,
    }


def get_unreal_insights_timing_summary(
    trace_path: Optional[str] = None,
    name_filter: Optional[str] = None,
    threads: Optional[str] = None,
    start_time: Optional[float] = None,
    end_time: Optional[float] = None,
    top_n: int = 50,
    sort_by: str = "Incl",
    unreal_insights_path: Optional[str] = None,
    output_dir: Optional[str] = None,
    timeout_seconds: int = 180,
) -> Dict[str, Any]:
    """Analyze a local trace and return aggregated CPU/GPU Timing timer statistics.

    Times in Unreal's CSV are seconds; matching ``*_ms`` fields are added for
    convenient performance review.  The source ``.utrace`` is read-only.

    Args:
        trace_path: Trace file to analyze. Uses the newest local trace when omitted.
        name_filter: Case-insensitive substring or wildcard patterns, comma separated.
        threads: Unreal Insights thread wildcard filter, for example ``GameThread`` or ``GPU*``.
        start_time: Optional interval start in seconds.
        end_time: Optional interval end in seconds.
        top_n: Number of matching rows to return (1-500).
        sort_by: CSV field used for result sorting, such as Incl, Excl, I.Max, I.Avg, or Count.
        unreal_insights_path: Optional explicit UnrealInsights executable path.
        output_dir: Optional directory for generated CSV/log files.
        timeout_seconds: Headless analysis timeout (10-1800 seconds).
    """
    try:
        safe_threads = _safe_command_value(threads, "threads")
        selected_trace = _select_trace(trace_path)
        if not selected_trace:
            return {"success": False, "message": "No local .utrace file was found."}
        if start_time is not None and end_time is not None and end_time <= start_time:
            return {"success": False, "message": "end_time must be greater than start_time."}

        csv_path, log_path = _output_paths("timer_statistics", output_dir)
        command_parts = [
            "TimingInsights.ExportTimerStatistics",
            f'"{csv_path.as_posix()}"',
            "-sortBy=TotalInclusiveTime",
            "-sortOrder=Descending",
        ]
        if safe_threads:
            command_parts.append(f"-threads={safe_threads}")
        if start_time is not None:
            command_parts.append(f"-startTime={float(start_time)}")
        if end_time is not None:
            command_parts.append(f"-endTime={float(end_time)}")

        export_result = _run_export(
            selected_trace,
            " ".join(command_parts),
            csv_path,
            log_path,
            unreal_insights_path,
            timeout_seconds,
        )
        if not export_result.get("success"):
            return export_result

        rows = _read_csv_rows(csv_path)
        matching = [row for row in rows if _matches_name(str(row.get("Name", "")), name_filter)]
        selected_sort = sort_by if sort_by in (matching[0].keys() if matching else {"Incl"}) else "Incl"
        matching.sort(
            key=lambda row: float(row.get(selected_sort, 0.0)) if isinstance(row.get(selected_sort), (int, float)) else 0.0,
            reverse=True,
        )
        return {
            **export_result,
            "source_row_count": len(rows),
            "matching_row_count": len(matching),
            "name_filter": name_filter,
            "threads": threads,
            "interval_seconds": {"start": start_time, "end": end_time},
            "sort_by": selected_sort,
            "timers": matching[: max(1, min(int(top_n), 500))],
        }
    except (OSError, ValueError, csv.Error) as exc:
        logger.exception("Unreal Insights timing summary failed")
        return {"success": False, "message": str(exc)}


def export_unreal_insights_timing_events(
    timers: str,
    trace_path: Optional[str] = None,
    threads: Optional[str] = None,
    start_time: Optional[float] = None,
    end_time: Optional[float] = None,
    max_return_rows: int = 200,
    unreal_insights_path: Optional[str] = None,
    output_dir: Optional[str] = None,
    timeout_seconds: int = 180,
) -> Dict[str, Any]:
    """Export individual Timing events for selected timer-name wildcards.

    ``timers`` is required to avoid accidentally exporting an entire large trace.
    The full filtered CSV is retained while the MCP response returns a bounded
    sample of rows.
    """
    try:
        safe_timers = _safe_command_value(timers, "timers")
        safe_threads = _safe_command_value(threads, "threads")
        if not safe_timers or safe_timers.strip() == "*":
            return {"success": False, "message": "Provide a specific timer name or wildcard instead of '*'."}
        selected_trace = _select_trace(trace_path)
        if not selected_trace:
            return {"success": False, "message": "No local .utrace file was found."}
        if start_time is not None and end_time is not None and end_time <= start_time:
            return {"success": False, "message": "end_time must be greater than start_time."}

        csv_path, log_path = _output_paths("timing_events", output_dir)
        command_parts = [
            "TimingInsights.ExportTimingEvents",
            f'"{csv_path.as_posix()}"',
            "-columns=ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth",
            f"-timers={safe_timers}",
        ]
        if safe_threads:
            command_parts.append(f"-threads={safe_threads}")
        if start_time is not None:
            command_parts.append(f"-startTime={float(start_time)}")
        if end_time is not None:
            command_parts.append(f"-endTime={float(end_time)}")

        export_result = _run_export(
            selected_trace,
            " ".join(command_parts),
            csv_path,
            log_path,
            unreal_insights_path,
            timeout_seconds,
        )
        if not export_result.get("success"):
            return export_result
        rows = _read_csv_rows(csv_path)
        return {
            **export_result,
            "timers": timers,
            "threads": threads,
            "interval_seconds": {"start": start_time, "end": end_time},
            "event_count": len(rows),
            "event_groups": _summarize_timing_events(rows),
            "events": rows[: max(1, min(int(max_return_rows), 2000))],
            "response_truncated": len(rows) > max(1, min(int(max_return_rows), 2000)),
        }
    except (OSError, ValueError, csv.Error) as exc:
        logger.exception("Unreal Insights timing event export failed")
        return {"success": False, "message": str(exc)}
