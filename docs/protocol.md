# Widgeteer JSON Protocol Reference

This document describes the complete HTTP/JSON API protocol for Widgeteer.

## Table of Contents

- [Overview](#overview)
- [HTTP Endpoints](#http-endpoints)
- [Element Selectors](#element-selectors)
- [Commands](#commands)
- [Response Format](#response-format)
- [Error Codes](#error-codes)
- [Transactions](#transactions)

---

## Overview

Widgeteer exposes an HTTP API on a configurable port (default: 9000). All requests and responses use JSON format with `Content-Type: application/json`.

### Base URL

```
http://localhost:9000
```

### Common Headers

```
Content-Type: application/json
```

---

## HTTP Endpoints

### GET /health

Check server status.

**Response:**
```json
{
  "status": "ok",
  "version": "0.1.0",
  "qt_version": "6.5.3"
}
```

---

### GET /schema

Get the complete command schema. Useful for LLM agents to understand available operations.

**Response:**
```json
{
  "commands": {
    "click": {
      "description": "Click on a widget",
      "params": {
        "target": {"type": "string", "required": true},
        "button": {"type": "string", "enum": ["left", "right", "middle"], "default": "left"},
        "pos": {"type": "object", "properties": {"x": "int", "y": "int"}}
      }
    }
    // ... more commands
  },
  "selectors": {
    "@name:": "Select by objectName",
    "@class:": "Select by class name",
    "@text:": "Select by text content",
    "@accessible:": "Select by accessible name"
  }
}
```

---

### GET /tree

Get the widget hierarchy as JSON.

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `root` | string | - | Root widget selector (default: all top-level widgets) |
| `depth` | int | -1 | Maximum depth (-1 = unlimited) |
| `include_invisible` | bool | false | Include invisible widgets |
| `include_properties` | bool | false | Include property values |
| `include_geometry` | bool | true | Include geometry information |

**Example Request:**
```
GET /tree?depth=3&include_properties=true
```

**Response:**
```json
{
  "widgets": [
    {
      "objectName": "mainWindow",
      "className": "QMainWindow",
      "path": "mainWindow",
      "visible": true,
      "enabled": true,
      "geometry": {"x": 100, "y": 100, "width": 800, "height": 600},
      "children": [
        {
          "objectName": "centralWidget",
          "className": "QWidget",
          "path": "mainWindow/centralWidget",
          "visible": true,
          "enabled": true,
          "children": [...]
        }
      ]
    }
  ]
}
```

---

### GET /screenshot

Capture a screenshot.

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `target` | string | - | Widget selector (default: entire screen) |
| `format` | string | png | Image format (png, jpg) |

**Example Request:**
```
GET /screenshot?target=@name:mainWindow&format=png
```

**Response:**
```json
{
  "screenshot": "iVBORw0KGgoAAAANSUhEUgAA...",
  "format": "png",
  "width": 800,
  "height": 600
}
```

The `screenshot` field contains base64-encoded image data.

---

### POST /command

Execute a single command.

**Request Body:**
```json
{
  "id": "unique-command-id",
  "command": "command_name",
  "params": {
    "target": "@name:widgetName",
    // command-specific parameters
  },
  "options": {
    "timeout_ms": 5000
  }
}
```

**Response (Success):**
```json
{
  "id": "unique-command-id",
  "success": true,
  "result": {
    // command-specific result
  },
  "duration_ms": 42
}
```

**Response (Failure):**
```json
{
  "id": "unique-command-id",
  "success": false,
  "error": {
    "code": "ELEMENT_NOT_FOUND",
    "message": "Widget '@name:nonExistent' not found",
    "details": {}
  },
  "duration_ms": 5
}
```

---

### POST /transaction

Execute multiple commands atomically with optional rollback on failure.

**Request Body:**
```json
{
  "id": "transaction-id",
  "transaction": true,
  "rollback_on_failure": true,
  "steps": [
    {"id": "step-1", "command": "click", "params": {"target": "@name:nameEdit"}},
    {"id": "step-2", "command": "type", "params": {"target": "@name:nameEdit", "text": "Hello"}},
    {"id": "step-3", "command": "click", "params": {"target": "@name:submitButton"}}
  ]
}
```

**Response:**
```json
{
  "id": "transaction-id",
  "success": true,
  "completed_steps": 3,
  "total_steps": 3,
  "steps_results": [
    {"step": 0, "command": "click", "success": true},
    {"step": 1, "command": "type", "success": true},
    {"step": 2, "command": "click", "success": true}
  ],
  "rollback_performed": false
}
```

---

## Element Selectors

Selectors are strings that identify widgets in the UI hierarchy.

### By objectName (Recommended)

```
@name:widgetObjectName
```

Finds a widget by its Qt `objectName` property. This is the most reliable method.

**Example:**
```
@name:submitButton
@name:nameEdit
@name:mainWindow
```

### By Class

```
@class:ClassName
```

Finds the first widget of the specified Qt class.

**Example:**
```
@class:QPushButton
@class:QLineEdit
@class:QComboBox
```

### By Text Content

```
@text:TextContent
```

Finds a widget containing the specified text (button text, label text, etc.).

**Example:**
```
@text:Submit
@text:Cancel
@text:Open File
```

### By Accessible Name

```
@accessible:AccessibleName
```

Finds a widget by its accessibility name.

**Example:**
```
@accessible:Submit Form Button
@accessible:User Name Input
```

### By Path

```
parent/child/grandchild
```

Hierarchical path using objectNames.

**Example:**
```
mainWindow/centralWidget/formTab
mainWindow/tabWidget/controlsTab/slider
```

### With Index

```
parent/child[index]
```

Use index when multiple siblings have the same name or class.

**Example:**
```
mainWindow/buttonBox/QPushButton[0]
mainWindow/buttonBox/QPushButton[1]
```

### With Wildcard

```
parent/*/child
```

Match any intermediate widget.

**Example:**
```
mainWindow/*/submitButton
```

---

## Commands

### Introspection Commands

#### get_tree

Get widget hierarchy.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `root` | string | No | Root widget selector |
| `depth` | int | No | Maximum depth (-1 = unlimited) |
| `include_invisible` | bool | No | Include invisible widgets |
| `include_properties` | bool | No | Include property values |
| `include_geometry` | bool | No | Include geometry (default: true) |

**Example:**
```json
{"command": "get_tree", "params": {"depth": 2, "include_properties": true}}
```

---

#### find

Search for widgets matching a query.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `query` | string | Yes | Selector query |
| `max_results` | int | No | Maximum results (default: 100) |
| `visible_only` | bool | No | Only visible widgets |

**Example:**
```json
{"command": "find", "params": {"query": "@class:QPushButton", "max_results": 10}}
```

**Response:**
```json
{
  "matches": [
    {"path": "mainWindow/submitButton", "class": "QPushButton", "objectName": "submitButton"},
    {"path": "mainWindow/clearButton", "class": "QPushButton", "objectName": "clearButton"}
  ],
  "count": 2
}
```

---

#### describe

Get detailed information about a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

**Example:**
```json
{"command": "describe", "params": {"target": "@name:submitButton"}}
```

**Response:**
```json
{
  "objectName": "submitButton",
  "className": "QPushButton",
  "path": "mainWindow/formTab/submitButton",
  "visible": true,
  "enabled": true,
  "focused": false,
  "geometry": {"x": 10, "y": 200, "width": 100, "height": 30},
  "properties": {
    "text": "Submit",
    "checkable": false,
    "checked": false
  },
  "actions": ["click", "animateClick", "toggle"]
}
```

---

#### get_property

Read a property value.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `property` | string | Yes | Property name |

**Example:**
```json
{"command": "get_property", "params": {"target": "@name:nameEdit", "property": "text"}}
```

**Response:**
```json
{
  "property": "text",
  "value": "Hello World"
}
```

---

#### list_properties

List all properties of a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

**Example:**
```json
{"command": "list_properties", "params": {"target": "@name:nameEdit"}}
```

---

#### get_actions

List available actions/slots for a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

---

### Action Commands

#### click

Click on a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `button` | string | No | Mouse button: "left", "right", "middle" (default: "left") |
| `pos` | object | No | Click position {x, y} relative to widget (default: center) |

**Example:**
```json
{"command": "click", "params": {"target": "@name:submitButton"}}
```

**Response:**
```json
{
  "clicked": true,
  "target_geometry": {"x": 10, "y": 200, "width": 100, "height": 30}
}
```

---

#### double_click

Double-click on a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `pos` | object | No | Click position {x, y} |

---

#### right_click

Right-click (context menu) on a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `pos` | object | No | Click position {x, y} |

---

#### type

Type text into a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `text` | string | Yes | Text to type |
| `clear_first` | bool | No | Clear existing text first (default: false) |

**Example:**
```json
{"command": "type", "params": {"target": "@name:nameEdit", "text": "John Doe", "clear_first": true}}
```

**Response:**
```json
{
  "typed": true,
  "text": "John Doe"
}
```

---

#### key

Press a single key.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `key` | string | Yes | Key name (e.g., "Enter", "Escape", "Tab", "a", "1") |
| `modifiers` | array | No | Modifier keys: ["ctrl", "shift", "alt", "meta"] |

**Example:**
```json
{"command": "key", "params": {"target": "@name:nameEdit", "key": "a", "modifiers": ["ctrl"]}}
```

---

#### key_sequence

Press a key combination.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `sequence` | string | Yes | Key sequence (e.g., "Ctrl+S", "Ctrl+Shift+N") |

**Example:**
```json
{"command": "key_sequence", "params": {"target": "@name:mainWindow", "sequence": "Ctrl+S"}}
```

---

#### drag

Drag from one widget to another.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `from` | string | Yes | Source widget selector |
| `to` | string | Yes | Destination widget selector |
| `from_pos` | object | No | Start position {x, y} |
| `to_pos` | object | No | End position {x, y} |

---

#### scroll

Scroll a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `delta_x` | int | No | Horizontal scroll amount |
| `delta_y` | int | No | Vertical scroll amount |

---

#### hover

Move mouse over a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `pos` | object | No | Position {x, y} |

---

#### focus

Set keyboard focus to a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

---

### State Modification Commands

#### set_property

Set a widget property.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `property` | string | Yes | Property name |
| `value` | any | Yes | New value |

**Example:**
```json
{"command": "set_property", "params": {"target": "@name:nameEdit", "property": "text", "value": "New Text"}}
```

---

#### set_value

Smart value setter for common widget types.

Automatically handles:
- **QComboBox**: Sets current index (int) or current text (string)
- **QSpinBox/QDoubleSpinBox**: Sets numeric value
- **QSlider/QScrollBar**: Sets slider value
- **QCheckBox/QRadioButton**: Sets checked state
- **QLineEdit**: Sets text
- **QTextEdit**: Sets plain text
- **QTabWidget**: Sets current tab index

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `value` | any | Yes | Value to set |

**Example:**
```json
{"command": "set_value", "params": {"target": "@name:roleComboBox", "value": 2}}
```

---

#### invoke

Call a method/slot on a widget.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `method` | string | Yes | Method/slot name |

**Example:**
```json
{"command": "invoke", "params": {"target": "@name:submitButton", "method": "click"}}
```

---

### Verification Commands

#### screenshot

Capture a screenshot.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | No | Widget selector (default: entire screen) |
| `format` | string | No | Image format: "png", "jpg" (default: "png") |

**Response:**
```json
{
  "screenshot": "base64-encoded-image-data",
  "format": "png",
  "width": 800,
  "height": 600
}
```

---

#### assert

Verify a property condition.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `property` | string | Yes | Property name |
| `operator` | string | Yes | Comparison operator |
| `value` | any | Yes | Expected value |

**Operators:**
- `==`, `equals` - Equal to
- `!=`, `not_equals` - Not equal to
- `>`, `gt` - Greater than
- `>=`, `gte` - Greater than or equal
- `<`, `lt` - Less than
- `<=`, `lte` - Less than or equal
- `contains` - String contains

**Example:**
```json
{"command": "assert", "params": {"target": "@name:nameEdit", "property": "text", "operator": "==", "value": "John"}}
```

**Response:**
```json
{
  "passed": true,
  "property": "text",
  "operator": "==",
  "expected": "John",
  "actual": "John"
}
```

---

#### exists

Check if an element exists.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

**Response:**
```json
{
  "exists": true,
  "target": "@name:submitButton"
}
```

---

#### is_visible

Check if an element is visible.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |

**Response:**
```json
{
  "visible": true,
  "exists": true
}
```

---

### Synchronization Commands

#### wait

Wait for a condition to be met.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `condition` | string | No | Condition type (default: "exists") |
| `timeout_ms` | int | No | Timeout in milliseconds (default: 5000) |
| `poll_interval_ms` | int | No | Polling interval (default: 50) |
| `stability_ms` | int | No | Stability duration (default: 200) |

**Conditions:**
- `exists` - Widget exists
- `visible` - Widget is visible
- `enabled` - Widget is enabled
- `focused` - Widget has focus
- `property:name=value` - Property equals value

**Example:**
```json
{"command": "wait", "params": {"target": "@name:loadingSpinner", "condition": "!visible", "timeout_ms": 10000}}
```

**Response:**
```json
{
  "waited": true,
  "elapsed_ms": 1234
}
```

---

#### wait_idle

Wait for the Qt event queue to be empty.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `timeout_ms` | int | No | Timeout in milliseconds (default: 5000) |

**Example:**
```json
{"command": "wait_idle", "params": {"timeout_ms": 1000}}
```

---

#### wait_signal

Wait for a Qt signal to be emitted.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `target` | string | Yes | Widget selector |
| `signal` | string | Yes | Signal signature |
| `timeout_ms` | int | No | Timeout in milliseconds (default: 5000) |

**Example:**
```json
{"command": "wait_signal", "params": {"target": "@name:submitButton", "signal": "clicked()", "timeout_ms": 5000}}
```

---

#### sleep

Hard delay. Use sparingly - prefer `wait` or `wait_idle`.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `ms` | int | Yes | Duration in milliseconds |

**Example:**
```json
{"command": "sleep", "params": {"ms": 500}}
```

---

### Extensibility Commands

These commands allow interaction with application-registered objects and custom command handlers.

#### call

Call a Q_INVOKABLE method on a registered QObject.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `object` | string | Yes | Registered object name |
| `method` | string | Yes | Method name |
| `args` | array | No | Method arguments |

**Example:**
```json
{"command": "call", "params": {"object": "myService", "method": "getCounter", "args": []}}
```

**Response:**
```json
{
  "invoked": true,
  "method": "getCounter",
  "return": 42
}
```

**Example with arguments:**
```json
{"command": "call", "params": {"object": "myService", "method": "setCounter", "args": [100]}}
```

---

#### list_objects

List all registered QObjects and their available methods.

**Parameters:** None

**Example:**
```json
{"command": "list_objects", "params": {}}
```

**Response:**
```json
{
  "objects": [
    {
      "name": "myService",
      "class": "SampleService",
      "methods": [
        {
          "name": "getCounter",
          "signature": "getCounter()",
          "returnType": "int",
          "parameterTypes": []
        },
        {
          "name": "setCounter",
          "signature": "setCounter(int)",
          "returnType": "void",
          "parameterTypes": ["int"]
        }
      ]
    }
  ],
  "count": 1
}
```

---

#### list_custom_commands

List all registered custom command handlers.

**Parameters:** None

**Example:**
```json
{"command": "list_custom_commands", "params": {}}
```

**Response:**
```json
{
  "commands": ["echo", "get_app_info", "load_project"],
  "count": 3
}
```

---

#### Custom Commands

Custom commands registered via `registerCommand()` can be invoked directly by name.

**Example (assuming "echo" command is registered):**
```json
{"command": "echo", "params": {"message": "Hello World"}}
```

**Response:**
```json
{
  "echo": "Hello World",
  "length": 11
}
```

---

## Response Format

### Success Response

```json
{
  "id": "command-id",
  "success": true,
  "result": {
    // Command-specific result data
  },
  "duration_ms": 42
}
```

### Error Response

```json
{
  "id": "command-id",
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable error message",
    "details": {
      // Additional error context
    }
  },
  "duration_ms": 5
}
```

---

## Error Codes

| Code | Description |
|------|-------------|
| `ELEMENT_NOT_FOUND` | Target widget not found in the widget tree |
| `ELEMENT_NOT_VISIBLE` | Widget exists but is not currently visible |
| `ELEMENT_NOT_ENABLED` | Widget exists but is disabled |
| `PROPERTY_NOT_FOUND` | Requested property does not exist on widget |
| `PROPERTY_READ_ONLY` | Cannot modify a read-only property |
| `INVALID_SELECTOR` | Malformed selector syntax |
| `INVALID_COMMAND` | Unknown command name |
| `INVALID_PARAMS` | Missing required parameters or invalid parameter values |
| `TIMEOUT` | Wait condition was not met within timeout |
| `INVOCATION_FAILED` | Method/slot invocation failed |
| `SCREENSHOT_FAILED` | Failed to capture screenshot |
| `TRANSACTION_FAILED` | Transaction failed (check steps_results for details) |
| `INTERNAL_ERROR` | Unexpected internal error |

---

## Transactions

Transactions allow executing multiple commands atomically with optional rollback on failure.

### Request Format

```json
{
  "id": "tx-1",
  "transaction": true,
  "rollback_on_failure": true,
  "steps": [
    {"id": "step-1", "command": "click", "params": {"target": "@name:field1"}},
    {"id": "step-2", "command": "type", "params": {"target": "@name:field1", "text": "value1"}},
    {"id": "step-3", "command": "click", "params": {"target": "@name:field2"}},
    {"id": "step-4", "command": "type", "params": {"target": "@name:field2", "text": "value2"}},
    {"id": "step-5", "command": "click", "params": {"target": "@name:submitButton"}}
  ]
}
```

### Response Format

**Success:**
```json
{
  "id": "tx-1",
  "success": true,
  "completed_steps": 5,
  "total_steps": 5,
  "steps_results": [
    {"step": 0, "command": "click", "success": true},
    {"step": 1, "command": "type", "success": true},
    {"step": 2, "command": "click", "success": true},
    {"step": 3, "command": "type", "success": true},
    {"step": 4, "command": "click", "success": true}
  ],
  "rollback_performed": false
}
```

**Failure with Rollback:**
```json
{
  "id": "tx-1",
  "success": false,
  "completed_steps": 2,
  "total_steps": 5,
  "steps_results": [
    {"step": 0, "command": "click", "success": true},
    {"step": 1, "command": "type", "success": true},
    {"step": 2, "command": "click", "success": false, "error": {"code": "ELEMENT_NOT_FOUND", "message": "..."}}
  ],
  "rollback_performed": true
}
```

### Rollback Behavior

When `rollback_on_failure` is `true` (default), Widgeteer attempts to undo state changes made by previous steps. This works best for:

- Property changes (restored to original values)
- Text input (cleared)

Some actions cannot be rolled back:
- Button clicks
- Signal emissions
- External side effects
