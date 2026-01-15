# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Widgeteer is a Qt6 UI testing and automation framework designed for LLM agent control. It provides a JSON-based WebSocket API that enables programmatic control of Qt6 QWidgets applications without prior knowledge of their structure.

## Build Commands

```bash
# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run C++ unit tests (Qt Test framework)
cd build && ctest --output-on-failure

# Run integration tests (Python, requires running sample app)
./build/sample/widgeteer_sample 9000 &
cd tests && python test_executor.py sample_tests.json --port 9000

# Headless testing (CI or no display)
QT_QPA_PLATFORM=offscreen ctest --output-on-failure
xvfb-run -a python3 test_executor.py sample_tests.json --port 9000
```

## Architecture

**Core Components (src/):**
- `Server.cpp` - WebSocket server, client management, public API
- `CommandExecutor.cpp` - Command dispatch (25+ commands), custom command registration
- `Protocol.cpp` - JSON message types (Command, Response, Event, Subscribe, Record)
- `ElementFinder.cpp` - Widget lookup via selectors (@name:, @class:, @text:, paths)
- `UIIntrospector.cpp` - Widget tree serialization to JSON
- `EventInjector.cpp` - Mouse/keyboard input simulation
- `Synchronizer.cpp` - Wait conditions (exists, visible, enabled, property, idle)
- `ActionRecorder.cpp` - Record interactions for playback
- `EventBroadcaster.cpp` - Event subscription system

**Element Selectors:**
```
@name:buttonName       # By objectName (preferred)
@class:QPushButton     # By class type
@text:Submit           # By text content
@accessible:Label      # By accessible name
parent/child/widget    # Hierarchical path
parent/*/widget        # With wildcard
parent/items[1]        # With index
```

**Command Categories:**
- Introspection: get_tree, find, describe, get_property, list_properties
- Actions: click, double_click, type, key, key_sequence, drag, scroll, hover, focus
- State: set_property, set_value, invoke
- Verification: screenshot, assert, exists, is_visible
- Sync: wait, wait_idle, wait_signal, sleep
- Extensibility: call, list_objects, list_custom_commands

## Code Style

- C++17, Qt6 idioms
- Namespace: `widgeteer::`
- Formatter: clang-format (Google style, 100 char line width)
- Warnings as errors (-Werror)
- Qt MOC-enabled (CMAKE_AUTOMOC ON)

## Testing

**C++ Unit Tests (tests/cpp/):**
- test_protocol.cpp - JSON protocol validation
- test_element_finder.cpp - Widget lookup
- test_action_recorder.cpp - Recording functionality
- test_event_broadcaster.cpp - Event system

**Python Integration Tests (tests/):**
- widgeteer_client.py - Async/sync Python client
- test_executor.py - Test runner supporting JSON test format
- sample_tests.json - Example test suite

## Sample Application

`sample/main.cpp` provides a comprehensive Qt widgets demo for testing. Run with:
```bash
./build/sample/widgeteer_sample [port]  # Default port: 9000
```
