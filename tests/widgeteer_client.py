#!/usr/bin/env python3
"""
Widgeteer Python Client

A reusable client for interacting with Widgeteer-enabled Qt applications
using WebSocket communication.
"""

import asyncio
import json
import uuid
from dataclasses import dataclass, field
from typing import Any, Callable
from urllib.parse import urlencode

try:
    import websockets
    # Try newer websockets 13+ API first
    try:
        from websockets.asyncio.client import connect as ws_connect
    except (ImportError, ModuleNotFoundError):
        # Fall back to websockets 12.x and earlier
        from websockets import connect as ws_connect
except ImportError:
    websockets = None


@dataclass
class Response:
    """Response from Widgeteer server."""
    success: bool
    data: dict
    error: str | None = None
    duration_ms: int = 0


@dataclass
class Event:
    """Event from Widgeteer server."""
    event_type: str
    data: dict


class WidgeteerClient:
    """Async client for Widgeteer WebSocket API."""

    def __init__(self, host: str = "localhost", port: int = 9000, token: str | None = None):
        if websockets is None:
            raise ImportError("websockets package required. Install with: pip install websockets")

        self.host = host
        self.port = port
        self.token = token
        self._ws = None
        self._pending: dict[str, asyncio.Future] = {}
        self._event_handlers: dict[str, list[Callable]] = {}
        self._receive_task = None

    @property
    def ws_url(self) -> str:
        """Build WebSocket URL with optional token."""
        url = f"ws://{self.host}:{self.port}"
        if self.token:
            url += "?" + urlencode({"token": self.token})
        return url

    async def connect(self) -> None:
        """Connect to the WebSocket server."""
        self._ws = await ws_connect(self.ws_url)
        self._receive_task = asyncio.create_task(self._receive_loop())

    async def disconnect(self) -> None:
        """Disconnect from the server."""
        if self._receive_task:
            self._receive_task.cancel()
            try:
                await self._receive_task
            except asyncio.CancelledError:
                pass
        if self._ws:
            await self._ws.close()
            self._ws = None

    async def _receive_loop(self) -> None:
        """Background task to receive messages."""
        try:
            async for message in self._ws:
                data = json.loads(message)
                msg_type = data.get("type")

                if msg_type == "response":
                    # Match response to pending request
                    msg_id = data.get("id")
                    if msg_id in self._pending:
                        self._pending[msg_id].set_result(data)

                elif msg_type == "event":
                    # Dispatch to event handlers
                    event_type = data.get("event_type")
                    event_data = data.get("data", {})
                    if event_type in self._event_handlers:
                        for handler in self._event_handlers[event_type]:
                            try:
                                if asyncio.iscoroutinefunction(handler):
                                    await handler(Event(event_type, event_data))
                                else:
                                    handler(Event(event_type, event_data))
                            except Exception as e:
                                print(f"Event handler error: {e}")
        except asyncio.CancelledError:
            pass
        except Exception as e:
            print(f"Receive loop error: {e}")

    async def _send_and_wait(self, message: dict, timeout: float = 30.0) -> dict:
        """Send a message and wait for response."""
        if not self._ws:
            raise ConnectionError("Not connected to server")

        msg_id = message.get("id") or str(uuid.uuid4())
        message["id"] = msg_id

        future = asyncio.get_event_loop().create_future()
        self._pending[msg_id] = future

        try:
            await self._ws.send(json.dumps(message))
            result = await asyncio.wait_for(future, timeout=timeout)
            return result
        finally:
            self._pending.pop(msg_id, None)

    async def command(self, cmd: str, params: dict | None = None,
                      options: dict | None = None, cmd_id: str | None = None,
                      timeout: float = 30.0) -> Response:
        """Execute a single command."""
        message = {
            "type": "command",
            "id": cmd_id or str(uuid.uuid4()),
            "command": cmd,
            "params": params or {},
        }
        if options:
            message["options"] = options

        result = await self._send_and_wait(message, timeout)

        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=result.get("error", {}).get("message") if not result.get("success") else None,
            duration_ms=result.get("duration_ms", 0)
        )

    # ========== Event Subscriptions ==========

    async def subscribe(self, event_type: str, handler: Callable[[Event], None]) -> Response:
        """Subscribe to an event type."""
        if event_type not in self._event_handlers:
            self._event_handlers[event_type] = []
        self._event_handlers[event_type].append(handler)

        message = {
            "type": "subscribe",
            "id": str(uuid.uuid4()),
            "event_type": event_type,
        }
        result = await self._send_and_wait(message)

        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=result.get("error", {}).get("message") if not result.get("success") else None
        )

    async def unsubscribe(self, event_type: str | None = None) -> Response:
        """Unsubscribe from an event type (or all if None)."""
        if event_type:
            self._event_handlers.pop(event_type, None)
        else:
            self._event_handlers.clear()

        message = {
            "type": "unsubscribe",
            "id": str(uuid.uuid4()),
        }
        if event_type:
            message["event_type"] = event_type

        result = await self._send_and_wait(message)

        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=result.get("error", {}).get("message") if not result.get("success") else None
        )

    # ========== Recording ==========

    async def start_recording(self) -> Response:
        """Start recording commands."""
        message = {
            "type": "record_start",
            "id": str(uuid.uuid4()),
        }
        result = await self._send_and_wait(message)

        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=result.get("error", {}).get("message") if not result.get("success") else None
        )

    async def stop_recording(self) -> Response:
        """Stop recording and get recorded actions."""
        message = {
            "type": "record_stop",
            "id": str(uuid.uuid4()),
        }
        result = await self._send_and_wait(message)

        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=result.get("error", {}).get("message") if not result.get("success") else None
        )

    # ========== Convenience Methods ==========

    async def tree(self, root: str | None = None, depth: int = -1,
                   include_invisible: bool = False) -> Response:
        """Get widget tree."""
        params = {}
        if root:
            params["root"] = root
        if depth >= 0:
            params["depth"] = depth
        if include_invisible:
            params["include_invisible"] = True
        return await self.command("get_tree", params)

    async def screenshot(self, target: str | None = None, format: str = "png") -> Response:
        """Capture screenshot."""
        params = {"format": format}
        if target:
            params["target"] = target
        return await self.command("screenshot", params)

    async def click(self, target: str, button: str = "left", pos: dict | None = None) -> Response:
        """Click on a widget."""
        params = {"target": target, "button": button}
        if pos:
            params["pos"] = pos
        return await self.command("click", params)

    async def double_click(self, target: str, pos: dict | None = None) -> Response:
        """Double-click on a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return await self.command("double_click", params)

    async def right_click(self, target: str, pos: dict | None = None) -> Response:
        """Right-click on a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return await self.command("right_click", params)

    async def type_text(self, target: str, text: str, clear_first: bool = False) -> Response:
        """Type text into a widget."""
        return await self.command("type", {
            "target": target,
            "text": text,
            "clear_first": clear_first
        })

    async def key(self, target: str, key: str, modifiers: list[str] | None = None) -> Response:
        """Press a key."""
        params = {"target": target, "key": key}
        if modifiers:
            params["modifiers"] = modifiers
        return await self.command("key", params)

    async def key_sequence(self, target: str, sequence: str) -> Response:
        """Press a key sequence (e.g., 'Ctrl+Shift+S')."""
        return await self.command("key_sequence", {"target": target, "sequence": sequence})

    async def scroll(self, target: str, delta_x: int = 0, delta_y: int = 0) -> Response:
        """Scroll a widget."""
        return await self.command("scroll", {
            "target": target,
            "delta_x": delta_x,
            "delta_y": delta_y
        })

    async def hover(self, target: str, pos: dict | None = None) -> Response:
        """Hover over a widget."""
        params = {"target": target}
        if pos:
            params["pos"] = pos
        return await self.command("hover", params)

    async def focus(self, target: str) -> Response:
        """Set focus to a widget."""
        return await self.command("focus", {"target": target})

    async def get_property(self, target: str, property_name: str) -> Response:
        """Get a property value."""
        return await self.command("get_property", {
            "target": target,
            "property": property_name
        })

    async def set_property(self, target: str, property_name: str, value: Any) -> Response:
        """Set a property value."""
        return await self.command("set_property", {
            "target": target,
            "property": property_name,
            "value": value
        })

    async def set_value(self, target: str, value: Any) -> Response:
        """Smart value setter for common widgets."""
        return await self.command("set_value", {"target": target, "value": value})

    async def invoke(self, target: str, method: str) -> Response:
        """Invoke a method/slot."""
        return await self.command("invoke", {"target": target, "method": method})

    async def exists(self, target: str) -> Response:
        """Check if element exists."""
        return await self.command("exists", {"target": target})

    async def is_visible(self, target: str) -> Response:
        """Check if element is visible."""
        return await self.command("is_visible", {"target": target})

    async def describe(self, target: str) -> Response:
        """Get detailed info about a widget."""
        return await self.command("describe", {"target": target})

    async def find(self, query: str, max_results: int = 100, visible_only: bool = False) -> Response:
        """Find widgets matching a query."""
        return await self.command("find", {
            "query": query,
            "max_results": max_results,
            "visible_only": visible_only
        })

    async def wait(self, target: str, condition: str = "exists",
                   timeout_ms: int = 5000) -> Response:
        """Wait for a condition."""
        return await self.command("wait", {
            "target": target,
            "condition": condition,
            "timeout_ms": timeout_ms
        })

    async def wait_idle(self, timeout_ms: int = 5000) -> Response:
        """Wait for Qt event queue to be empty."""
        return await self.command("wait_idle", {"timeout_ms": timeout_ms})

    async def sleep(self, ms: int) -> Response:
        """Hard delay."""
        return await self.command("sleep", {"ms": ms})

    async def assert_property(self, target: str, property_name: str, operator: str,
                              value: Any) -> Response:
        """Assert a property condition."""
        return await self.command("assert", {
            "target": target,
            "property": property_name,
            "operator": operator,
            "value": value
        })

    # ========== Context Manager Support ==========

    async def __aenter__(self):
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.disconnect()


# Synchronous wrapper for simpler use cases
class SyncWidgeteerClient:
    """Synchronous wrapper for WidgeteerClient."""

    def __init__(self, host: str = "localhost", port: int = 9000, token: str | None = None):
        self._client = WidgeteerClient(host, port, token)
        self._loop = asyncio.new_event_loop()

    def _run(self, coro):
        return self._loop.run_until_complete(coro)

    def connect(self) -> None:
        self._run(self._client.connect())

    def disconnect(self) -> None:
        self._run(self._client.disconnect())

    def command(self, cmd: str, params: dict | None = None, **kwargs) -> Response:
        return self._run(self._client.command(cmd, params, **kwargs))

    def tree(self, **kwargs) -> Response:
        return self._run(self._client.tree(**kwargs))

    def screenshot(self, **kwargs) -> Response:
        return self._run(self._client.screenshot(**kwargs))

    def click(self, target: str, **kwargs) -> Response:
        return self._run(self._client.click(target, **kwargs))

    def double_click(self, target: str, **kwargs) -> Response:
        return self._run(self._client.double_click(target, **kwargs))

    def right_click(self, target: str, **kwargs) -> Response:
        return self._run(self._client.right_click(target, **kwargs))

    def type_text(self, target: str, text: str, **kwargs) -> Response:
        return self._run(self._client.type_text(target, text, **kwargs))

    def key(self, target: str, key: str, **kwargs) -> Response:
        return self._run(self._client.key(target, key, **kwargs))

    def key_sequence(self, target: str, sequence: str) -> Response:
        return self._run(self._client.key_sequence(target, sequence))

    def scroll(self, target: str, **kwargs) -> Response:
        return self._run(self._client.scroll(target, **kwargs))

    def hover(self, target: str, **kwargs) -> Response:
        return self._run(self._client.hover(target, **kwargs))

    def focus(self, target: str) -> Response:
        return self._run(self._client.focus(target))

    def get_property(self, target: str, property_name: str) -> Response:
        return self._run(self._client.get_property(target, property_name))

    def set_property(self, target: str, property_name: str, value: Any) -> Response:
        return self._run(self._client.set_property(target, property_name, value))

    def set_value(self, target: str, value: Any) -> Response:
        return self._run(self._client.set_value(target, value))

    def invoke(self, target: str, method: str) -> Response:
        return self._run(self._client.invoke(target, method))

    def exists(self, target: str) -> Response:
        return self._run(self._client.exists(target))

    def is_visible(self, target: str) -> Response:
        return self._run(self._client.is_visible(target))

    def describe(self, target: str) -> Response:
        return self._run(self._client.describe(target))

    def find(self, query: str, **kwargs) -> Response:
        return self._run(self._client.find(query, **kwargs))

    def wait(self, target: str, **kwargs) -> Response:
        return self._run(self._client.wait(target, **kwargs))

    def wait_idle(self, **kwargs) -> Response:
        return self._run(self._client.wait_idle(**kwargs))

    def sleep(self, ms: int) -> Response:
        return self._run(self._client.sleep(ms))

    def start_recording(self) -> Response:
        return self._run(self._client.start_recording())

    def stop_recording(self) -> Response:
        return self._run(self._client.stop_recording())

    def assert_property(self, target: str, property_name: str, operator: str,
                        value: Any) -> Response:
        return self._run(self._client.assert_property(target, property_name, operator, value))

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


async def main():
    """Quick test of the client."""
    import sys

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000

    async with WidgeteerClient(port=port) as client:
        # Get widget tree
        resp = await client.tree()
        if resp.success:
            print("Widget tree:")
            print(json.dumps(resp.data, indent=2)[:500] + "...")
        else:
            print(f"Failed to get tree: {resp.error}")


if __name__ == "__main__":
    asyncio.run(main())
