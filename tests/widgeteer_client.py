#!/usr/bin/env python3
"""
Widgeteer Python Client

A reusable client for interacting with Widgeteer-enabled Qt applications
using WebSocket communication.
"""

import asyncio
import json
import re
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


# ========== Selector Syntax ==========

# Regex patterns for CSS-like selector syntax
_SELECTOR_PATTERNS = [
    # #name -> @name:name (by objectName, like CSS id)
    (re.compile(r'^#([a-zA-Z_][a-zA-Z0-9_]*)$'), r'@name:\1'),
    # .ClassName -> @class:ClassName (by Qt class)
    (re.compile(r'^\.([a-zA-Z_][a-zA-Z0-9_]*)$'), r'@class:\1'),
    # [text="value"] or [text='value'] -> @text:value
    (re.compile(r'^\[text=["\'](.+)["\']\]$'), r'@text:\1'),
    # [accessible="value"] or [accessible='value'] -> @accessible:value
    (re.compile(r'^\[accessible=["\'](.+)["\']\]$'), r'@accessible:\1'),
    # [name="value"] -> @name:value (alternative attribute syntax)
    (re.compile(r'^\[name=["\'](.+)["\']\]$'), r'@name:\1'),
    # [class="value"] -> @class:value (alternative attribute syntax)
    (re.compile(r'^\[class=["\'](.+)["\']\]$'), r'@class:\1'),
]


def normalize_selector(selector: str) -> str:
    """Convert CSS-like selector syntax to Widgeteer native format.

    Supports both CSS-like and native Widgeteer selectors:

    CSS-like (new):
        #submitButton           -> @name:submitButton
        .QPushButton            -> @class:QPushButton
        [text="Submit"]         -> @text:Submit
        [accessible="Label"]    -> @accessible:Label

    Native (still supported):
        @name:submitButton
        @class:QPushButton
        @text:Submit
        @accessible:Label
        mainWindow/formTab/btn  (hierarchical paths)

    Hierarchical paths with CSS-like selectors:
        mainWindow/#submitBtn   -> mainWindow/@name:submitBtn
        dialog/.QPushButton     -> dialog/@class:QPushButton

    Args:
        selector: Selector string in either CSS-like or native format.

    Returns:
        Selector in native Widgeteer format.
    """
    if not selector:
        return selector

    # Handle hierarchical paths by processing each segment
    if '/' in selector:
        segments = selector.split('/')
        normalized = [normalize_selector(seg) for seg in segments]
        return '/'.join(normalized)

    # Already in native format
    if selector.startswith('@'):
        return selector

    # Try CSS-like patterns
    for pattern, replacement in _SELECTOR_PATTERNS:
        match = pattern.match(selector)
        if match:
            return pattern.sub(replacement, selector)

    # Return as-is (probably a plain objectName for path segments)
    return selector


@dataclass
class Response:
    """Response from Widgeteer server.

    Attributes:
        success: Whether the command succeeded.
        data: Full result dict for advanced access.
        error: Human-readable error message (if failed).
        error_code: Structured error code like ELEMENT_NOT_FOUND (if failed).
        duration_ms: Command execution time in milliseconds.

    The `value` property provides unified access to the primary result:
        - get_property() -> the property value
        - exists() -> bool
        - is_visible() -> bool
        - assert_property() -> bool (passed)
        - find() -> list of matches
        - tree() -> list of widgets
        - screenshot() -> base64 image data
        - Action commands (click, type, etc.) -> True if succeeded
    """
    success: bool
    data: dict
    error: str | None = None
    error_code: str | None = None
    duration_ms: int = 0

    @property
    def value(self) -> Any:
        """Extract the primary result value based on response type.

        Returns None if the command failed.
        """
        if not self.success:
            return None

        # Explicit value field (get_property, set_property)
        if "value" in self.data:
            return self.data["value"]

        # Boolean state checks
        if "exists" in self.data and "visible" not in self.data:
            return self.data["exists"]
        if "visible" in self.data:
            return self.data["visible"]
        if "passed" in self.data:
            return self.data["passed"]
        if "enabled" in self.data:
            return self.data["enabled"]

        # Collection results
        if "matches" in self.data:
            return self.data["matches"]
        if "widgets" in self.data:
            return self.data["widgets"]

        # Screenshot
        if "screenshot" in self.data:
            return self.data["screenshot"]

        # Method invocation
        if "return" in self.data:
            return self.data["return"]

        # Action confirmations - return True for success
        action_keys = ("clicked", "typed", "focused", "scrolled", "hovered",
                       "waited", "invoked", "dragged", "set")
        for key in action_keys:
            if key in self.data:
                return self.data[key]

        # Fallback: return full data dict
        return self.data

    def raise_for_error(self) -> "Response":
        """Raise WidgeteerError if the command failed. Returns self for chaining."""
        if not self.success:
            raise WidgeteerError(self.error or "Unknown error", self.error_code, self.data)
        return self


class WidgeteerError(Exception):
    """Exception raised when a Widgeteer command fails."""

    def __init__(self, message: str, code: str | None = None, details: dict | None = None):
        super().__init__(message)
        self.code = code
        self.details = details or {}

    def __str__(self) -> str:
        if self.code:
            return f"[{self.code}] {super().__str__()}"
        return super().__str__()


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

        loop = asyncio.get_running_loop()
        future = loop.create_future()
        self._pending[msg_id] = future

        try:
            await self._ws.send(json.dumps(message))
            result = await asyncio.wait_for(future, timeout=timeout)
            return result
        finally:
            self._pending.pop(msg_id, None)

    async def _send_batch(self, messages: list[dict], timeout: float = 30.0) -> list[dict]:
        """Send multiple messages in parallel and wait for all responses.

        This reduces latency by sending all commands before waiting for responses.
        """
        if not self._ws:
            raise ConnectionError("Not connected to server")

        if not messages:
            return []

        loop = asyncio.get_running_loop()
        futures: list[tuple[str, asyncio.Future]] = []

        # Assign IDs and create futures for all messages
        for message in messages:
            msg_id = message.get("id") or str(uuid.uuid4())
            message["id"] = msg_id
            future = loop.create_future()
            self._pending[msg_id] = future
            futures.append((msg_id, future))

        try:
            # Send all messages without waiting
            for message in messages:
                await self._ws.send(json.dumps(message))

            # Wait for all responses
            results = await asyncio.wait_for(
                asyncio.gather(*[f for _, f in futures]),
                timeout=timeout
            )
            return list(results)
        finally:
            # Clean up all pending futures
            for msg_id, _ in futures:
                self._pending.pop(msg_id, None)

    def _make_response(self, result: dict) -> Response:
        """Convert a raw result dict to a Response object."""
        error_obj = result.get("error", {}) if not result.get("success") else {}
        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=error_obj.get("message"),
            error_code=error_obj.get("code"),
            duration_ms=result.get("duration_ms", 0)
        )

    async def command(self, cmd: str, params: dict | None = None,
                      options: dict | None = None, cmd_id: str | None = None,
                      timeout: float = 30.0) -> Response:
        """Execute a single command.

        Selectors in params are automatically normalized, supporting both CSS-like
        and native Widgeteer syntax:
            #buttonName     -> @name:buttonName
            .QPushButton    -> @class:QPushButton
            [text="OK"]     -> @text:OK
        """
        # Normalize selectors in params
        normalized_params = self._normalize_params(params) if params else {}

        message = {
            "type": "command",
            "id": cmd_id or str(uuid.uuid4()),
            "command": cmd,
            "params": normalized_params,
        }
        if options:
            message["options"] = options

        result = await self._send_and_wait(message, timeout)
        return self._make_response(result)

    def _normalize_params(self, params: dict) -> dict:
        """Normalize selector parameters in a params dict."""
        result = params.copy()

        # Keys that contain selectors
        selector_keys = ("target", "from", "to", "root", "query")

        for key in selector_keys:
            if key in result and isinstance(result[key], str):
                result[key] = normalize_selector(result[key])

        return result

    async def batch(self, commands: list[dict], timeout: float = 30.0) -> list[Response]:
        """Execute multiple commands in parallel for reduced latency.

        All commands are sent before waiting for responses, minimizing round-trip overhead.
        CSS-like selectors are automatically normalized.

        Args:
            commands: List of command dicts, each with 'command' and 'params' keys.
                      Optional 'options' and 'id' keys are also supported.
            timeout: Timeout for all commands to complete (seconds).

        Returns:
            List of Response objects in the same order as input commands.

        Example:
            results = await client.batch([
                {"command": "get_property", "params": {"target": "#edit1", "property": "text"}},
                {"command": "get_property", "params": {"target": "#edit2", "property": "text"}},
                {"command": "is_visible", "params": {"target": "#dialog"}},
            ])
            text1, text2, visible = [r.value for r in results]
        """
        messages = []
        for cmd_spec in commands:
            params = cmd_spec.get("params", {})
            message = {
                "type": "command",
                "id": cmd_spec.get("id") or str(uuid.uuid4()),
                "command": cmd_spec["command"],
                "params": self._normalize_params(params) if params else {},
            }
            if "options" in cmd_spec:
                message["options"] = cmd_spec["options"]
            messages.append(message)

        results = await self._send_batch(messages, timeout)
        return [self._make_response(r) for r in results]

    async def batch_commands(self, *commands: tuple[str, dict | None], timeout: float = 30.0) -> list[Response]:
        """Execute multiple commands in parallel using a simpler API.

        Args:
            *commands: Variable arguments of (command_name, params) tuples.
            timeout: Timeout for all commands to complete (seconds).

        Returns:
            List of Response objects in the same order as input commands.

        Example:
            text1, text2, visible = await client.batch_commands(
                ("get_property", {"target": "@name:edit1", "property": "text"}),
                ("get_property", {"target": "@name:edit2", "property": "text"}),
                ("is_visible", {"target": "@name:dialog"}),
            )
            print(text1.value, text2.value, visible.value)
        """
        cmd_list = [{"command": cmd, "params": params or {}} for cmd, params in commands]
        return await self.batch(cmd_list, timeout)

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

        error_obj = result.get("error", {}) if not result.get("success") else {}
        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=error_obj.get("message"),
            error_code=error_obj.get("code"),
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

        error_obj = result.get("error", {}) if not result.get("success") else {}
        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=error_obj.get("message"),
            error_code=error_obj.get("code"),
        )

    # ========== Recording ==========

    async def start_recording(self) -> Response:
        """Start recording commands."""
        message = {
            "type": "record_start",
            "id": str(uuid.uuid4()),
        }
        result = await self._send_and_wait(message)

        error_obj = result.get("error", {}) if not result.get("success") else {}
        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=error_obj.get("message"),
            error_code=error_obj.get("code"),
        )

    async def stop_recording(self) -> Response:
        """Stop recording and get recorded actions."""
        message = {
            "type": "record_stop",
            "id": str(uuid.uuid4()),
        }
        result = await self._send_and_wait(message)

        error_obj = result.get("error", {}) if not result.get("success") else {}
        return Response(
            success=result.get("success", False),
            data=result.get("result", {}),
            error=error_obj.get("message"),
            error_code=error_obj.get("code"),
        )

    # ========== Transactions ==========

    async def transaction(self, steps: list[dict], rollback_on_failure: bool = True,
                          timeout: float = 60.0) -> Response:
        """Execute multiple commands atomically with optional rollback.

        If any step fails and rollback_on_failure is True, previously executed
        steps will be undone (where possible).

        Args:
            steps: List of command dicts, each with 'command' and 'params' keys.
                   Optionally include 'id' for step identification.
            rollback_on_failure: Whether to rollback on failure (default True).
            timeout: Timeout for entire transaction (seconds).

        Returns:
            Response with:
                - value: True if all steps succeeded
                - data['completed_steps']: Number of steps that completed
                - data['total_steps']: Total number of steps
                - data['steps_results']: Results for each step
                - data['rollback_performed']: Whether rollback was done

        Example:
            result = await client.transaction([
                {"command": "input_text", "params": {"target": "#name", "text": "John"}},
                {"command": "input_text", "params": {"target": "#email", "text": "john@example.com"}},
                {"command": "click", "params": {"target": "#submit"}},
            ])
            if result.success:
                print(f"All {result.data['completed_steps']} steps completed")
            else:
                print(f"Failed at step {result.data['completed_steps']}")
                if result.data.get('rollback_performed'):
                    print("Changes were rolled back")
        """
        # Normalize selectors in all steps
        normalized_steps = []
        for i, step in enumerate(steps):
            normalized_step = {
                "id": step.get("id", f"step-{i}"),
                "command": step["command"],
                "params": self._normalize_params(step.get("params", {})),
            }
            normalized_steps.append(normalized_step)

        message = {
            "id": str(uuid.uuid4()),
            "transaction": True,
            "rollback_on_failure": rollback_on_failure,
            "steps": normalized_steps,
        }

        result = await self._send_and_wait(message, timeout)

        # Transaction responses have a different structure
        return Response(
            success=result.get("success", False),
            data={
                "completed_steps": result.get("completed_steps", 0),
                "total_steps": result.get("total_steps", len(steps)),
                "steps_results": result.get("steps_results", []),
                "rollback_performed": result.get("rollback_performed", False),
            },
            error=result.get("error", {}).get("message") if not result.get("success") else None,
            error_code=result.get("error", {}).get("code") if not result.get("success") else None,
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

    async def click(self, target: str, button: str = "left", pos: dict | None = None,
                    track_changes: bool = False) -> Response:
        """Click on a widget.

        Args:
            target: Widget selector.
            button: Mouse button ("left", "right", "middle").
            pos: Optional click position relative to widget.
            track_changes: If True, response includes 'changes' array showing
                          what properties changed (e.g., text, value, checked).
        """
        params = {"target": target, "button": button}
        if pos:
            params["pos"] = pos
        options = {"track_changes": True} if track_changes else None
        return await self.command("click", params, options=options)

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

    async def input_text(self, target: str, text: str, clear_first: bool = False) -> Response:
        """Enter text into a widget (e.g., QLineEdit, QTextEdit).

        Args:
            target: Widget selector (e.g., "#nameEdit", ".QLineEdit").
            text: Text to enter.
            clear_first: If True, clear existing text before typing.

        Returns:
            Response with value=True if successful.
        """
        return await self.command("type", {
            "target": target,
            "text": text,
            "clear_first": clear_first
        })

    # Alias for backward compatibility
    async def type_text(self, target: str, text: str, clear_first: bool = False) -> Response:
        """Alias for input_text(). Prefer input_text() to avoid confusion with Python's type()."""
        return await self.input_text(target, text, clear_first)

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
        """Invoke a slot/method on a widget found by selector.

        Use this to call Qt slots on widgets in the UI tree.
        For calling methods on registered service objects, use call_service() instead.

        Args:
            target: Widget selector (e.g., "#myButton", ".QPushButton").
            method: Slot/method name (e.g., "click", "clear", "selectAll").

        Returns:
            Response with value=True if invoked successfully.

        Example:
            await client.invoke("#myButton", "click")
            await client.invoke("#textEdit", "clear")
        """
        return await self.command("invoke", {"target": target, "method": method})

    async def call_service(self, service: str, method: str, args: list | None = None) -> Response:
        """Call a Q_INVOKABLE method on a registered service object.

        Service objects are registered on the server via server.registerObject("name", &obj).
        For calling slots on widgets in the UI tree, use invoke() instead.

        Args:
            service: Registered object name (e.g., "dataService", "appController").
            method: Q_INVOKABLE method name.
            args: Optional list of arguments to pass to the method.

        Returns:
            Response with value containing the method's return value.

        Example:
            # On server: server.registerObject("calculator", &calculatorService)
            result = await client.call_service("calculator", "add", [2, 3])
            print(result.value)  # 5

            result = await client.call_service("dataService", "refresh")
        """
        params = {"object": service, "method": method}
        if args is not None:
            params["args"] = args
        return await self.command("call", params)

    async def list_services(self) -> Response:
        """List all registered service objects and their available methods.

        Returns:
            Response with value containing dict of {service_name: [methods]}.
        """
        return await self.command("list_objects", {})

    async def list_custom_commands(self) -> Response:
        """List all custom commands registered on the server.

        Returns:
            Response with value containing list of custom command names.
        """
        return await self.command("list_custom_commands", {})

    async def list_commands(self) -> dict:
        """List all available commands (built-in and custom).

        Returns:
            Dict with 'builtin' and 'custom' lists of command names.
        """
        custom_result = await self.list_custom_commands()
        custom_cmds = custom_result.data.get("commands", []) if custom_result.success else []

        return {
            "builtin": [
                # Introspection
                "get_tree", "find", "describe", "get_property", "list_properties", "get_actions",
                # Actions
                "click", "double_click", "right_click", "type", "key", "key_sequence",
                "drag", "scroll", "hover", "focus",
                # State
                "set_property", "set_value", "invoke",
                # Verification
                "screenshot", "assert", "exists", "is_visible",
                # Synchronization
                "wait", "wait_idle", "wait_signal", "sleep",
                # Extensibility
                "call", "list_objects", "list_custom_commands",
            ],
            "custom": list(custom_cmds),
        }

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
    """Synchronous wrapper for WidgeteerClient.

    This client manages its own event loop for running async operations synchronously.
    It properly cleans up the loop when closed.

    Note: Do not use this client from within async code - use WidgeteerClient directly instead.
    """

    def __init__(self, host: str = "localhost", port: int = 9000, token: str | None = None):
        self._client = WidgeteerClient(host, port, token)
        self._loop: asyncio.AbstractEventLoop | None = None
        self._owns_loop = False

    def _get_loop(self) -> asyncio.AbstractEventLoop:
        """Get or create an event loop for sync operations."""
        if self._loop is not None and not self._loop.is_closed():
            return self._loop

        # Check if we're inside an async context (which would be a misuse)
        try:
            asyncio.get_running_loop()
            # If we get here, there IS a running loop - raise error
            raise RuntimeError(
                "SyncWidgeteerClient cannot be used from async code. "
                "Use WidgeteerClient directly instead."
            )
        except RuntimeError as e:
            # Re-raise our own error, but ignore the "no running event loop" error
            if "cannot be used from async code" in str(e):
                raise
            # No running loop - this is the expected case for sync usage

        # Create a new event loop
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._owns_loop = True
        return self._loop

    def _run(self, coro):
        """Run a coroutine synchronously."""
        return self._get_loop().run_until_complete(coro)

    def connect(self) -> None:
        """Connect to the Widgeteer server."""
        self._run(self._client.connect())

    def disconnect(self) -> None:
        """Disconnect from the server."""
        self._run(self._client.disconnect())

    def close(self) -> None:
        """Close the client and clean up resources."""
        if self._client._ws:
            self._run(self._client.disconnect())
        if self._owns_loop and self._loop is not None and not self._loop.is_closed():
            self._loop.close()
            self._loop = None
            self._owns_loop = False

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

    def input_text(self, target: str, text: str, **kwargs) -> Response:
        """Enter text into a widget. Prefer this over type_text()."""
        return self._run(self._client.input_text(target, text, **kwargs))

    def type_text(self, target: str, text: str, **kwargs) -> Response:
        """Alias for input_text()."""
        return self.input_text(target, text, **kwargs)

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
        """Invoke a slot/method on a widget. For service objects, use call_service()."""
        return self._run(self._client.invoke(target, method))

    def call_service(self, service: str, method: str, args: list | None = None) -> Response:
        """Call a Q_INVOKABLE method on a registered service object."""
        return self._run(self._client.call_service(service, method, args))

    def list_services(self) -> Response:
        """List all registered service objects and their methods."""
        return self._run(self._client.list_services())

    def list_custom_commands(self) -> Response:
        """List all custom commands registered on the server."""
        return self._run(self._client.list_custom_commands())

    def list_commands(self) -> dict:
        """List all available commands (built-in and custom)."""
        return self._run(self._client.list_commands())

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

    def batch(self, commands: list[dict], timeout: float = 30.0) -> list[Response]:
        """Execute multiple commands in parallel for reduced latency.

        Example:
            results = client.batch([
                {"command": "get_property", "params": {"target": "@name:edit1", "property": "text"}},
                {"command": "is_visible", "params": {"target": "@name:dialog"}},
            ])
            text, visible = [r.value for r in results]
        """
        return self._run(self._client.batch(commands, timeout))

    def batch_commands(self, *commands: tuple[str, dict | None], timeout: float = 30.0) -> list[Response]:
        """Execute multiple commands in parallel using a simpler API.

        Example:
            text, visible = client.batch_commands(
                ("get_property", {"target": "@name:edit1", "property": "text"}),
                ("is_visible", {"target": "@name:dialog"}),
            )
            print(text.value, visible.value)
        """
        return self._run(self._client.batch_commands(*commands, timeout=timeout))

    def transaction(self, steps: list[dict], rollback_on_failure: bool = True,
                    timeout: float = 60.0) -> Response:
        """Execute multiple commands atomically with optional rollback.

        Example:
            result = client.transaction([
                {"command": "input_text", "params": {"target": "#name", "text": "John"}},
                {"command": "click", "params": {"target": "#submit"}},
            ])
        """
        return self._run(self._client.transaction(steps, rollback_on_failure, timeout))

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def __del__(self):
        """Clean up resources when garbage collected."""
        try:
            self.close()
        except Exception:
            pass  # Ignore errors during cleanup


async def main():
    """Quick test of the client demonstrating the Response API."""
    import sys

    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9000

    async with WidgeteerClient(port=port) as client:
        # Get widget tree - use .value for direct access
        resp = await client.tree(depth=2)
        if resp.success:
            print(f"Found {len(resp.value)} top-level widgets")
            print(json.dumps(resp.value, indent=2)[:500] + "...")
        else:
            print(f"Failed to get tree: [{resp.error_code}] {resp.error}")

        # Example: check existence - .value returns bool directly
        resp = await client.exists("@name:someWidget")
        print(f"Widget exists: {resp.value}")  # True or False

        # Example: get property - .value returns the property value directly
        resp = await client.get_property("@class:QWidget", "objectName")
        if resp.success:
            print(f"Object name: {resp.value}")  # The actual value, not nested dict

        # Example: using raise_for_error() for concise error handling
        try:
            resp = await client.click("@name:nonExistentButton")
            resp.raise_for_error()  # Raises WidgeteerError if failed
        except WidgeteerError as e:
            print(f"Expected error: {e}")


if __name__ == "__main__":
    asyncio.run(main())
