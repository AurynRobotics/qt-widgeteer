# Qt Widgeteer

[![CI](https://github.com/AurynRobotics/qt-widgeteer/actions/workflows/ci.yml/badge.svg)](https://github.com/AurynRobotics/qt-widgeteer/actions/workflows/ci.yml)
![Coverage](https://img.shields.io/endpoint?url=https://gist.githubusercontent.com/facontidavide/2385955c346efb7e3396ac06d4c40364/raw/coverage.json)

A Qt6 UI testing and automation framework designed for LLM agent control, GUI automation and integration testing.

## Why Widgeteer?

Traditional UI testing frameworks assume you write test scripts upfront. But what if an AI agent needs to explore and interact with your application dynamically?

Widgeteer exposes your Qt6 application's UI through a WebSocket API that lets any client — human or AI — discover widgets, read their state, and perform actions. An LLM agent can call `get_tree` to see the entire UI hierarchy, then click buttons, fill forms, and verify results without any prior knowledge of your application's structure.

**Use cases:**
- Let AI agents automate complex workflows in your Qt application
- Build test scripts interactively by exploring the live UI
- Record user interactions for playback or test generation
- Remote control Qt applications from any language with WebSocket support

## Key Features

- **Discoverable UI**: Agents explore the widget hierarchy without prior knowledge
- **Minimal Integration**: 2-3 lines of code to instrument any Qt6 application
- **Zero Dependencies**: Uses only Qt6 modules (no external libraries)
- **Language Agnostic**: JSON over WebSocket works from Python, JavaScript, or raw CLI

## What Can You Do?

Non comprehensive list of things that you can do with Widgeteer:

1. **Discover the UI** — Get the full widget tree with types, properties, and hierarchy
2. **Click anything** — Buttons, checkboxes, menu items, table cells, tree nodes
3. **Type text** — Fill line edits, text areas, spin boxes, combo box searches
4. **Read properties** — Check text, values, enabled state, visibility, geometry
5. **Wait for conditions** — Element exists, becomes visible, property changes, signal emits
6. **Take screenshots** — Capture widgets or full windows as base64 PNG
7. **Invoke methods** — Call slots and Q_INVOKABLE methods directly
8. **Subscribe to events** — Get notified when buttons are clicked, values change, windows open
9. **Record sessions** — Capture interactions as replayable JSON test scripts
10. **Assert state** — Verify property values with operators (==, !=, contains, >=, etc.)

## Quick Start

Add two lines to your Qt application:

```cpp
#include <widgeteer/Server.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    widgeteer::Server server;
    server.start(9000);  // WebSocket server on port 9000

    MainWindow window;
    window.show();
    return app.exec();
}
```

Then connect and interact:

```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    tree = client.tree()                              # Discover all widgets
    client.click("@name:submitButton")                # Click by object name
    client.type_text("@name:emailField", "test@example.com")
    client.wait("@name:statusLabel", condition="visible")  # Wait for UI update
    status = client.get_property("@name:statusLabel", "text").value
```

Or send raw JSON commands via any WebSocket client:

```bash
wscat -c ws://localhost:9000
> {"type":"command","id":"1","command":"get_tree","params":{}}
> {"type":"command","id":"2","command":"click","params":{"target":"@name:submitButton"}}
> {"type":"command","id":"3","command":"type","params":{"target":"@name:emailField","text":"test@example.com"}}
> {"type":"command","id":"4","command":"wait","params":{"target":"@name:statusLabel","condition":"visible"}}
> {"type":"command","id":"5","command":"get_property","params":{"target":"@name:statusLabel","property":"text"}}
```

## Building

```bash
git clone https://github.com/user/widgeteer.git
cd widgeteer && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Requires CMake 3.18+, Qt6, and a C++17 compiler.

## Documentation

- [User Manual](docs/user-manual.md) — Setup, selectors, and usage examples
- [Protocol Reference](docs/protocol.md) — Complete WebSocket API specification
- [LLM Integration Guide](docs/llm_user_manual.md) — Optimized reference for AI agents (add to system prompt)
- [Architecture](docs/architecture.md) — Internals guide for modifying the codebase

## Disclaimer

This application was 100% vibe-coded by Claude Code and it was specifically optimized for LLM.

Hopefully the API, protocol and documentation are also human-friendly,
but my primary focus is to create a harness that LLM can use to debug and create automated tests
for Qt C++ applications as autonomously as possible.

## License

MIT License
