# Widgeteer

A Qt6 UI testing and automation framework designed for LLM agent control.

## Overview

Widgeteer is a minimally invasive library that enables Qt6 QWidgets applications to be controlled programmatically via a JSON-based HTTP API. Unlike traditional testing frameworks that assume pre-written test scripts, Widgeteer is designed with LLM agents in mind — providing full UI introspection so an agent can discover, explore, and interact with the application without prior knowledge of its structure.

## Features

- **LLM-friendly**: Structured JSON responses with enough context for an agent to reason
- **Discoverable**: Agent can explore UI without prior knowledge via `GET /tree`
- **Atomic & composite**: Support single actions and multi-step transactions with rollback
- **Synchronous by default**: Actions block until UI is stable, with configurable timeouts
- **Minimal integration**: 2-3 lines to add to any Qt6 app
- **Zero external dependencies**: Uses only Qt6 modules

## Quick Start

### Integration (2 lines)

```cpp
#include <widgeteer/Server.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    widgeteer::Server agent;
    agent.start(9000);

    MainWindow window;
    window.show();

    return app.exec();
}
```

### Python Client

```python
from widgeteer_client import WidgeteerClient

client = WidgeteerClient(port=9000)

# Get widget tree
tree = client.tree()

# Click a button
client.click("@name:submitButton")

# Type text
client.type_text("@name:nameEdit", "Hello World")

# Check property
result = client.assert_property("@name:nameEdit", "text", "==", "Hello World")
```

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Server status |
| GET | `/schema` | Full command schema (for LLM) |
| GET | `/tree` | Widget tree |
| GET | `/screenshot` | Screen capture |
| POST | `/command` | Execute single command |
| POST | `/transaction` | Execute multi-step |

## Element Addressing

```python
# Path-based (primary)
"mainWindow/sidebar/loadButton"

# By objectName
"@name:plotWidget"

# By class (returns first match)
"@class:QLineEdit"

# By text content
"@text:OK"

# By accessible name
"@accessible:Load Data Button"

# Index for duplicates
"mainWindow/buttonBox/QPushButton[1]"

# Wildcard
"mainWindow/*/saveButton"
```

## Commands

### Introspection
- `get_tree` - Get widget tree
- `find` - Find widgets by query
- `describe` - Detailed widget info
- `get_property` / `list_properties` - Property access

### Actions
- `click` / `double_click` / `right_click` - Mouse clicks
- `type` - Enter text
- `key` / `key_sequence` - Keyboard input
- `drag` / `scroll` / `hover` - Mouse operations
- `focus` - Set keyboard focus

### State
- `set_property` - Modify property
- `invoke` - Call method/slot
- `set_value` - Smart value setting (combo, spin, etc.)

### Verification
- `screenshot` - Capture image
- `assert` - Verify state
- `exists` / `is_visible` - Check element

### Synchronization
- `wait` - Wait for condition (exists, visible, enabled, etc.)
- `wait_idle` - Wait for event queue empty
- `wait_signal` - Wait for Qt signal
- `sleep` - Hard delay

## Building

### Requirements

- CMake 3.18+
- Qt6 (Core, Widgets, Gui, Test, Network)
- C++17 compiler

### Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run Sample Application

```bash
./build/sample/widgeteer_sample 9000
```

### Run Tests

```bash
cd tests
python test_executor.py sample_tests.json --app ../build/sample/widgeteer_sample
```

## Dependencies

| Dependency | Purpose |
|------------|---------|
| Qt6::Core | Base functionality |
| Qt6::Widgets | Widget introspection |
| Qt6::Gui | Screenshots |
| Qt6::Test | Event injection |
| Qt6::Network | HTTP API |

## License

MIT License
