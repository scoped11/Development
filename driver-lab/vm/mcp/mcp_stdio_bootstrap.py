"""Keep DriverLab MCP stderr quiet so Cursor does not mark the server as failed."""
from __future__ import annotations

import logging
import os
import sys


def configure_stdio_logging() -> None:
    os.environ.setdefault("FASTMCP_LOG_LEVEL", "ERROR")
    os.environ.setdefault("NO_COLOR", "1")
    level = logging.ERROR
    for name in (
        "",
        "mcp",
        "mcp.server",
        "mcp.server.fastmcp",
        "mcp.server.lowlevel.server",
        "fastmcp",
    ):
        logging.getLogger(name).setLevel(level)

    class _StderrInfoFilter:
        def __init__(self, stream):
            self._stream = stream

        def write(self, data: str) -> int:
            if not data or not data.strip():
                return 0
            stripped = data.strip()
            if stripped.startswith("INFO") or "Processing request of type" in data:
                return len(data)
            if "PingRequest" in data:
                return len(data)
            return self._stream.write(data)

        def flush(self) -> None:
            self._stream.flush()

        def __getattr__(self, name: str):
            return getattr(self._stream, name)

    if os.environ.get("DRIVERLAB_MCP_QUIET_STDERR", "1") == "1":
        sys.stderr = _StderrInfoFilter(sys.stderr)  # type: ignore[assignment]