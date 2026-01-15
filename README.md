# Widgeteer

A Qt6 UI testing and automation framework designed for LLM agent control.

## Overview

Widgeteer is a minimally invasive library that enables Qt6 QWidgets applications to be controlled programmatically via a JSON-based WebSocket API. Unlike traditional testing frameworks that assume pre-written test scripts, Widgeteer is designed with LLM agents in mind — providing full UI introspection so an agent can discover, explore, and interact with the application without prior knowledge of its structure.

## Key Features

- **LLM-Friendly**: Structured JSON responses with enough context for an agent to reason about UI state
- **Discoverable**: Agents can explore the UI hierarchy without prior knowledge via `get_tree` command
- **Atomic & Composite**: Supports single actions and multi-step transactions with automatic rollback
- **Event Subscription**: Subscribe to UI events for reactive automation
- **Action Recording**: Record interactions for playback or test generation
- **Synchronous by Default**: Actions block until UI is stable, with configurable timeouts
- **Minimal Integration**: 2-3 lines of code to add to any Qt6 application
- **Zero External Dependencies**: Uses only Qt6 modules (Core, Widgets, Gui, Test, WebSockets)

## Quick Start

### Integration (2-3 lines)

```cpp
#include <widgeteer/Server.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Add these 2 lines to enable Widgeteer
    widgeteer::Server server;
    server.start(9000);

    MainWindow window;
    window.show();

    return app.exec();
}
```

### Python Client Example

```python
from widgeteer_client import SyncWidgeteerClient

# Synchronous client with context manager
with SyncWidgeteerClient(port=9000) as client:
    # Get the widget tree
    tree = client.tree()

    # Click a button
    client.click("@name:submitButton")

    # Type text into a field
    client.type_text("@name:nameEdit", "Hello World", clear_first=True)

    # Check a property value
    result = client.assert_property("@name:nameEdit", "text", "==", "Hello World")
    print(f"Assertion passed: {result.data['passed']}")

    # Set a combo box selection
    client.set_value("@name:roleComboBox", 2)

    # Wait for UI to be idle
    client.wait_idle(timeout_ms=1000)
```

### Async Client Example

```python
from widgeteer_client import WidgeteerClient
import asyncio

async def main():
    async with WidgeteerClient(port=9000) as client:
        await client.click("@name:submitButton")
        await client.type_text("@name:nameEdit", "Hello World")

asyncio.run(main())
```

### Using wscat (WebSocket CLI)

```bash
# Connect to server
wscat -c ws://localhost:9000

# Get widget tree
> {"type":"command","id":"1","command":"get_tree","params":{}}

# Click a button
> {"type":"command","id":"2","command":"click","params":{"target":"@name:submitButton"}}

# Type text
> {"type":"command","id":"3","command":"type","params":{"target":"@name:nameEdit","text":"Hello","clear_first":true}}

# Subscribe to events
> {"type":"subscribe","id":"4","event_type":"command_executed"}

# Start recording
> {"type":"record_start","id":"5"}
```

## WebSocket Protocol

All communication happens over WebSocket (`ws://host:port`). Messages are JSON objects with a `type` field.

| Message Type | Direction | Description |
|--------------|-----------|-------------|
| `command` | Client → Server | Execute a command |
| `response` | Server → Client | Command response |
| `event` | Server → Client | Subscribed event notification |
| `subscribe` | Client → Server | Subscribe to event type |
| `unsubscribe` | Client → Server | Unsubscribe from events |
| `record_start` | Client → Server | Start recording commands |
| `record_stop` | Client → Server | Stop recording, get recorded JSON |

## Element Selectors

Widgeteer supports multiple ways to address UI elements:

```python
# By objectName (recommended)
"@name:submitButton"

# By class type
"@class:QPushButton"

# By text content
"@text:Submit"

# By accessible name
"@accessible:Submit Form Button"

# By path (hierarchical)
"mainWindow/formTab/submitButton"

# With index for duplicates
"mainWindow/buttonBox/QPushButton[1]"

# With wildcard
"mainWindow/*/saveButton"
```

## Commands

### Introspection
| Command | Description |
|---------|-------------|
| `get_tree` | Get widget hierarchy |
| `find` | Search for widgets by query |
| `describe` | Detailed widget information |
| `get_property` | Read a property value |
| `list_properties` | List all properties |
| `get_actions` | List available actions/slots |

### Actions
| Command | Description |
|---------|-------------|
| `click` | Left/right/middle click |
| `double_click` | Double-click |
| `right_click` | Right-click (context menu) |
| `type` | Enter text with optional clear |
| `key` | Press a single key |
| `key_sequence` | Press key combination (e.g., Ctrl+S) |
| `drag` | Drag from one widget to another |
| `scroll` | Scroll a widget |
| `hover` | Move mouse over widget |
| `focus` | Set keyboard focus |

### State Modification
| Command | Description |
|---------|-------------|
| `set_property` | Modify a property value |
| `set_value` | Smart value setter (combo, spin, slider, etc.) |
| `invoke` | Call a method/slot |

### Verification
| Command | Description |
|---------|-------------|
| `screenshot` | Capture widget or screen image |
| `assert` | Verify property condition |
| `exists` | Check if element exists |
| `is_visible` | Check if element is visible |

### Synchronization
| Command | Description |
|---------|-------------|
| `wait` | Wait for condition (exists, visible, enabled, property) |
| `wait_idle` | Wait for Qt event queue to be empty |
| `wait_signal` | Wait for a Qt signal |
| `sleep` | Hard delay (use sparingly) |

## Building

### Requirements

- CMake 3.18+
- Qt6 (Core, Widgets, Gui, Test, WebSockets)
- C++17 compatible compiler

### Build Commands

```bash
# Clone and build
git clone https://github.com/user/widgeteer.git
cd widgeteer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run sample application
./sample/widgeteer_sample 9000
```

### CMake Integration

```cmake
find_package(widgeteer REQUIRED)
target_link_libraries(your_app PRIVATE widgeteer::widgeteer)
```

Or add as subdirectory:

```cmake
add_subdirectory(widgeteer)
target_link_libraries(your_app PRIVATE widgeteer)
```

## Running Tests

The project includes a Python test executor and sample tests:

```bash
cd tests
python test_executor.py sample_tests.json --app ../build/sample/widgeteer_sample --port 9000
```

Or connect to a running application:

```bash
python test_executor.py sample_tests.json --port 9000
```

## Documentation

- [JSON Protocol Reference](docs/protocol.md) - Complete API specification
- [User Manual](docs/user-manual.md) - Detailed usage guide and examples

## Project Structure

```
widgeteer/
├── include/widgeteer/     # Public headers
│   ├── Server.h           # Main server class
│   ├── Protocol.h         # JSON protocol structures
│   ├── ElementFinder.h    # Widget lookup
│   ├── UIIntrospector.h   # Widget tree serialization
│   ├── EventInjector.h    # Input simulation
│   ├── Synchronizer.h     # Wait conditions
│   └── CommandExecutor.h  # Command dispatch
├── src/                   # Implementation
├── sample/                # Sample Qt application
├── tests/                 # Python test framework
│   ├── widgeteer_client.py
│   ├── test_executor.py
│   └── sample_tests.json
└── docs/                  # Documentation
```

## Server Configuration

```cpp
widgeteer::Server server;

// Set port (default: 9000)
server.setPort(9001);

// Enable request logging
server.enableLogging(true);

// Set API key for authentication (clients connect with ws://host:port?token=key)
server.setApiKey("your-secret-key");

// Restrict to specific hosts (default: localhost only)
server.setAllowedHosts({"localhost", "127.0.0.1"});

// Limit introspection to specific widget tree
server.setRootWidget(mainWindow);

// Start recording commands for playback
server.startRecording();

// Register custom commands
server.registerCommand("myCommand", [](const QJsonObject& params) {
    return QJsonObject{{"result", "success"}};
});

// Expose QObject for method calls
server.registerObject("myService", myServiceObject);

server.start();
```

## Error Handling

All responses include structured error information when operations fail:

```json
{
  "id": "cmd-1",
  "success": false,
  "error": {
    "code": "ELEMENT_NOT_FOUND",
    "message": "Widget '@name:nonExistent' not found",
    "details": {
      "searched": "@name:nonExistent",
      "suggestions": ["nameEdit", "emailEdit"]
    }
  }
}
```

### Error Codes

| Code | Description |
|------|-------------|
| `ELEMENT_NOT_FOUND` | Target widget not found |
| `ELEMENT_NOT_VISIBLE` | Widget exists but not visible |
| `ELEMENT_NOT_ENABLED` | Widget exists but disabled |
| `PROPERTY_NOT_FOUND` | Requested property doesn't exist |
| `PROPERTY_READ_ONLY` | Cannot modify read-only property |
| `INVALID_SELECTOR` | Malformed selector syntax |
| `INVALID_COMMAND` | Unknown command name |
| `INVALID_PARAMS` | Missing or invalid parameters |
| `TIMEOUT` | Wait condition timed out |
| `INVOCATION_FAILED` | Method/slot invocation failed |

## License

MIT License

## Contributing

Contributions are welcome! Please ensure:
- Code follows the existing style (use clang-format)
- All tests pass
- New features include tests
- Documentation is updated
