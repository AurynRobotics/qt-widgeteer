#!/usr/bin/env python3
"""
Widgeteer Python Client

A reusable client for interacting with Widgeteer-enabled Qt applications.
"""

import json
import urllib.request
import urllib.error
from dataclasses import dataclass
from typing import Any


@dataclass
class Response:
    """Response from Widgeteer server."""
    success: bool
    data: dict
    error: str | None = None


class WidgeteerClient:
    """Client for Widgeteer HTTP API."""

    def __init__(self, host: str = "localhost", port: int = 9000, timeout: float = 30.0):
        self.base_url = f"http://{host}:{port}"
        self.timeout = timeout

    def _request(self, method: str, path: str, data: dict | None = None) -> Response:
        """Make an HTTP request to the server."""
        url = f"{self.base_url}{path}"

        headers = {"Content-Type": "application/json"}
        body = json.dumps(data).encode() if data else None

        req = urllib.request.Request(url, data=body, headers=headers, method=method)

        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                result = json.loads(resp.read().decode())
                return Response(
                    success=result.get("success", True),
                    data=result,
                    error=result.get("error", {}).get("message") if not result.get("success", True) else None
                )
        except urllib.error.HTTPError as e:
            error_body = e.read().decode()
            try:
                error_data = json.loads(error_body)
            except json.JSONDecodeError:
                error_data = {"error": error_body}
            return Response(success=False, data=error_data, error=error_data.get("error"))
        except urllib.error.URLError as e:
            return Response(success=False, data={}, error=str(e.reason))
        except Exception as e:
            return Response(success=False, data={}, error=str(e))

    def health(self) -> Response:
        """Check server health."""
        return self._request("GET", "/health")

    def schema(self) -> Response:
        """Get command schema."""
        return self._request("GET", "/schema")

    def tree(self, root: str | None = None, depth: int = -1,
             include_invisible: bool = False) -> Response:
        """Get widget tree."""
        params = []
        if root:
            params.append(f"root={root}")
        if depth >= 0:
            params.append(f"depth={depth}")
        if include_invisible:
            params.append("include_invisible=true")

        path = "/tree"
        if params:
            path += "?" + "&".join(params)

        return self._request("GET", path)

    def screenshot(self, target: str | None = None, format: str = "png") -> Response:
        """Capture screenshot."""
        params = [f"format={format}"]
        if target:
            params.append(f"target={target}")

        path = "/screenshot?" + "&".join(params)
        return self._request("GET", path)

    def command(self, cmd: str, params: dict | None = None,
                options: dict | None = None, cmd_id: str | None = None) -> Response:
        """Execute a single command."""
        data = {
            "id": cmd_id or f"cmd-{id(params)}",
            "command": cmd,
            "params": params or {},
        }
        if options:
            data["options"] = options

        return self._request("POST", "/command", data)

    def transaction(self, steps: list[dict], tx_id: str | None = None,
                    rollback_on_failure: bool = True) -> Response:
        """Execute a transaction (multiple commands)."""
        data = {
            "id": tx_id or f"tx-{id(steps)}",
            "transaction": True,
            "rollback_on_failure": rollback_on_failure,
            "steps": steps,
        }

        return self._request("POST", "/transaction", data)

    # Convenience methods for common commands

    def click(self, target: str, button: str = "left", pos: dict | None = None) -> Response:
        """Click on a widget."""
        params = {"target": target, "button": button}
        if pos:
            params["pos"] = pos
        return self.command("click", params)

    def double_click(self, target: str, pos: dict | None = None) -> Response:
        """Double-click on a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return self.command("double_click", params)

    def right_click(self, target: str, pos: dict | None = None) -> Response:
        """Right-click on a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return self.command("right_click", params)

    def type_text(self, target: str, text: str, clear_first: bool = False) -> Response:
        """Type text into a widget."""
        return self.command("type", {
            "target": target,
            "text": text,
            "clear_first": clear_first
        })

    def key(self, target: str, key: str, modifiers: list[str] | None = None) -> Response:
        """Press a key."""
        params = {"target": target, "key": key}
        if modifiers:
            params["modifiers"] = modifiers
        return self.command("key", params)

    def key_sequence(self, target: str, sequence: str) -> Response:
        """Press a key sequence (e.g., 'Ctrl+Shift+S')."""
        return self.command("key_sequence", {"target": target, "sequence": sequence})

    def scroll(self, target: str, delta_x: int = 0, delta_y: int = 0) -> Response:
        """Scroll a widget."""
        return self.command("scroll", {
            "target": target,
            "delta_x": delta_x,
            "delta_y": delta_y
        })

    def hover(self, target: str, pos: dict | None = None) -> Response:
        """Hover over a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return self.command("hover", params)

    def focus(self, target: str) -> Response:
        """Set focus to a widget."""
        return self.command("focus", {"target": target})

    def get_property(self, target: str, property_name: str) -> Response:
        """Get a property value."""
        return self.command("get_property", {
            "target": target,
            "property": property_name
        })

    def set_property(self, target: str, property_name: str, value: Any) -> Response:
        """Set a property value."""
        return self.command("set_property", {
            "target": target,
            "property": property_name,
            "value": value
        })

    def set_value(self, target: str, value: Any) -> Response:
        """Smart value setter for common widgets."""
        return self.command("set_value", {"target": target, "value": value})

    def invoke(self, target: str, method: str) -> Response:
        """Invoke a method/slot."""
        return self.command("invoke", {"target": target, "method": method})

    def exists(self, target: str) -> Response:
        """Check if element exists."""
        return self.command("exists", {"target": target})

    def is_visible(self, target: str) -> Response:
        """Check if element is visible."""
        return self.command("is_visible", {"target": target})

    def describe(self, target: str) -> Response:
        """Get detailed info about a widget."""
        return self.command("describe", {"target": target})

    def find(self, query: str, max_results: int = 100, visible_only: bool = False) -> Response:
        """Find widgets matching a query."""
        return self.command("find", {
            "query": query,
            "max_results": max_results,
            "visible_only": visible_only
        })

    def wait(self, target: str, condition: str = "exists",
             timeout_ms: int = 5000) -> Response:
        """Wait for a condition."""
        return self.command("wait", {
            "target": target,
            "condition": condition,
            "timeout_ms": timeout_ms
        })

    def wait_idle(self, timeout_ms: int = 5000) -> Response:
        """Wait for Qt event queue to be empty."""
        return self.command("wait_idle", {"timeout_ms": timeout_ms})

    def sleep(self, ms: int) -> Response:
        """Hard delay."""
        return self.command("sleep", {"ms": ms})

    def assert_property(self, target: str, property_name: str, operator: str,
                        value: Any) -> Response:
        """Assert a property condition."""
        return self.command("assert", {
            "target": target,
            "property": property_name,
            "operator": operator,
            "value": value
        })


def main():
    """Quick test of the client."""
    import sys

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000
    client = WidgeteerClient(port=port)

    # Check health
    resp = client.health()
    if not resp.success:
        print(f"Server not available: {resp.error}")
        sys.exit(1)

    print(f"Server status: {resp.data}")

    # Get widget tree
    resp = client.tree()
    if resp.success:
        print("\nWidget tree:")
        print(json.dumps(resp.data, indent=2)[:500] + "...")
    else:
        print(f"Failed to get tree: {resp.error}")


if __name__ == "__main__":
    main()
