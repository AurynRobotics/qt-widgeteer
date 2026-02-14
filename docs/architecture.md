# Widgeteer Architecture

Quick-reference architecture guide for LLMs modifying this codebase.

## Component Map

```
Server                      # WebSocket server, entry point
├── CommandExecutor         # Command dispatch & execution
│   ├── ElementFinder       # Widget lookup by selector
│   ├── UIIntrospector      # Widget tree → JSON
│   ├── EventInjector       # Mouse/keyboard simulation
│   └── Synchronizer        # Wait conditions
├── ActionRecorder          # Record commands for playback
└── EventBroadcaster        # Pub/sub event system

WidgeteerClient             # Fluent C++ API for in-process testing
└── CommandExecutor         # Owned or external executor
```

## File Layout

```
include/widgeteer/          # Public headers (API surface)
  ├── Result.h              # Result<T, E> template for error handling
  └── WidgeteerClient.h     # Fluent C++ testing API
src/                        # Implementation
tests/cpp/                  # Qt Test unit tests
tests/                      # Python integration tests
sample/                     # Demo Qt application
```

## Ownership Model

- **Server** owns all components, creates them in constructor
- **CommandExecutor** owns widget-manipulation components
- Components communicate via direct method calls (no signals between components)
- Server connects to Qt signals only for WebSocket events

---

## Command Flow

How a command travels from WebSocket to response:

```
1. Client sends JSON:  {"type":"command","command":"click","params":{...}}
                                          │
2. Server::onTextMessageReceived()        │
                                          ▼
3. Server::handleMessage() ─── routes either transaction envelopes (`transaction: true`) or
   typed messages to handlers
                                          │
4. QTimer::singleShot(0, ...) ◄───────────┘  (async queue for dialog support)
                                          │
5. CommandExecutor::execute(Command)      │
                                          ▼
6. CommandExecutor::dispatch(name, params) ─── looks up handler in registry
                                          │
7. Handler executes (e.g., cmdClick)      │
   └── Usually calls resolveTarget() → ElementFinder::find()
                                          │
8. Handler returns QJsonObject            │
                                          ▼
9. Server::sendResponse() ─── JSON back to client
```

**Why async (step 4)?** Qt blocking dialogs (`exec()`) create nested event loops. The `QTimer::singleShot(0)` allows commands to be processed even when a dialog is blocking.

---

## Adding New Commands

### Step 1: Add handler method in CommandExecutor

```cpp
// In CommandExecutor.h (private section)
QJsonObject cmdMyCommand(const QJsonObject& params);

// In CommandExecutor.cpp
QJsonObject CommandExecutor::cmdMyCommand(const QJsonObject& params) {
    // 1. Get target widget (if needed)
    auto [widget, error] = resolveTarget(params);
    if (!widget) {
        return error;
    }

    // 2. Do something
    QString value = params["value"].toString();

    // 3. Return result
    return QJsonObject{
        {"success_key", true},
        {"some_data", value}
    };
}
```

### Step 2: Register in constructor

```cpp
// In CommandExecutor constructor, add to the registry block:
registerCommand("my_command", [this](auto& p) { return cmdMyCommand(p); });
```

### Step 3: Add test

```cpp
// In tests/cpp/test_command_executor.cpp
void TestCommandExecutor::test_myCommand() {
    // Setup widget, execute command, verify result
}
```

### Common Patterns in Handlers

**Resolve target widget:**
```cpp
auto [widget, error] = resolveTarget(params);
if (!widget) return error;
```

**Return success:**
```cpp
return QJsonObject{{"result_key", value}};
```

**Return error:**
```cpp
return Protocol::errorResponse(ErrorCode::INVALID_PARAMS, "Missing 'value' parameter");
```

**Access other components:**
```cpp
elementFinder_->find(selector);
uiIntrospector_->serializeWidget(widget);
eventInjector_->click(widget, button, pos);
synchronizer_->wait(params);
```

---

## Element Selectors (ElementFinder)

### Selector Types

| Prefix | Example | Matches |
|--------|---------|---------|
| `@name:` | `@name:submitBtn` | `objectName == "submitBtn"` |
| `@class:` | `@class:QPushButton` | First widget of that class |
| `@text:` | `@text:Submit` | Widget with that text/label |
| `@accessible:` | `@accessible:Save` | Accessibility name |
| (path) | `parent/child/widget` | Hierarchical traversal |
| (index) | `list/item[2]` | Nth child with that name |
| (wildcard) | `dialog/*/button` | Any intermediate widget |

### Adding a New Selector Type

1. **Add parsing** in `ElementFinder::parseSelector()`:
```cpp
if (selector.startsWith("@newtype:")) {
    return findByNewType(selector.mid(9));  // Skip prefix
}
```

2. **Implement finder**:
```cpp
QWidget* ElementFinder::findByNewType(const QString& value) {
    return findWidgetRecursive(rootWidget(), [&](QWidget* w) {
        return /* condition using value */;
    });
}
```

### Caching

ElementFinder caches lookups in `QHash<QString, QPointer<QWidget>>`. The `QPointer` auto-nulls if widget is deleted. Cache is cleared on `clearCache()` or when root widget changes.

---

## Widget Introspection (UIIntrospector)

### Tree Serialization

`serializeTree()` recursively builds JSON:

```json
{
  "objectName": "mainWindow",
  "class": "QMainWindow",
  "role": "window",
  "visible": true,
  "enabled": true,
  "geometry": {"x": 0, "y": 0, "width": 800, "height": 600},
  "children": [...]
}
```

### Adding Widget-Specific Properties

In `UIIntrospector::serializeWidget()`, add class-specific handling:

```cpp
if (auto* myWidget = qobject_cast<QMyWidget*>(widget)) {
    obj["myProperty"] = myWidget->myProperty();
}
```

### Role Inference

`inferRole()` maps Qt classes to semantic roles:

```cpp
if (qobject_cast<QPushButton*>(w)) return "button";
if (qobject_cast<QLineEdit*>(w)) return "textfield";
// Add new mappings here
```

---

## Event Injection (EventInjector)

### Available Methods

| Method | Purpose |
|--------|---------|
| `click(widget, button, pos, mods)` | Mouse click |
| `doubleClick(widget, pos)` | Double click |
| `type(widget, text)` | Type text string |
| `keyClick(widget, key, mods)` | Single key press |
| `keySequence(widget, sequence)` | Key combo (e.g., "Ctrl+S") |
| `drag(from, fromPos, to, toPos)` | Drag operation |
| `scroll(widget, deltaX, deltaY)` | Scroll wheel |
| `hover(widget, pos)` | Mouse move |

### Implementation Pattern

All methods follow this pattern:

1. Verify widget is valid and visible
2. Calculate position (default: widget center)
3. Use `QTest::` functions or construct `QEvent` manually
4. Post event via `QCoreApplication::postEvent()` or `sendEvent()`

### Adding New Input Types

```cpp
EventInjector::Result EventInjector::myInput(QWidget* target, /* params */) {
    if (!target || !target->isVisible()) {
        return {false, "Widget not visible"};
    }

    // Create and post event
    QMyEvent event(/* ... */);
    QCoreApplication::sendEvent(target, &event);

    return {true, ""};
}
```

---

## Synchronization (Synchronizer)

### Wait Conditions

| Condition | Waits until... |
|-----------|----------------|
| `exists` | Widget found by selector |
| `not_exists` | Widget no longer found |
| `visible` | Widget is visible |
| `not_visible` | Widget hidden |
| `enabled` | Widget enabled |
| `disabled` | Widget disabled |
| `focused` | Widget has focus |
| `property:name=value` | Property equals value |
| `stable` | Widget state unchanged for N ms |
| `idle` | Qt event queue empty |

### Polling Loop

```cpp
while (elapsed < timeout) {
    if (checkCondition()) return success;
    QCoreApplication::processEvents();  // Keep UI responsive
    QThread::msleep(pollInterval);
}
return timeout_error;
```

### Adding New Conditions

In `Synchronizer::checkCondition()`:

```cpp
case Condition::MyCondition:
    return /* boolean check */;
```

---

## Protocol (Protocol.h/cpp)

### Message Types

| Type | Direction | Purpose |
|------|-----------|---------|
| `command` | Client→Server | Execute command |
| `response` | Server→Client | Command result |
| `event` | Server→Client | Broadcast event |
| `subscribe` | Client→Server | Subscribe to events |
| `unsubscribe` | Client→Server | Unsubscribe |
| `record_start` | Client→Server | Start recording |
| `record_stop` | Client→Server | Stop, get recording |

### Key Structs

```cpp
struct Command {
    QString id;
    QString name;
    QJsonObject params;
    QJsonObject options;  // timeout_ms, track_changes
};

struct Response {
    QString id;
    bool success;
    QJsonObject result;   // On success
    QJsonObject error;    // On failure: {code, message, details}
    int durationMs;
};
```

### Error Codes

Defined in `namespace ErrorCode`:
- `ELEMENT_NOT_FOUND` - Selector matched nothing
- `INVALID_COMMAND` - Unknown command name
- `INVALID_PARAMS` - Missing/wrong parameters
- `TIMEOUT` - Wait condition not met
- `INVOCATION_FAILED` - Method call failed

---

## Server (Server.h/cpp)

### Key Responsibilities

1. **WebSocket management** - Accept connections, handle auth
2. **Message routing** - Parse JSON, dispatch to handlers
3. **Extensibility** - Custom commands and objects
4. **Recording** - Delegate to ActionRecorder
5. **Events** - Delegate to EventBroadcaster

### Authentication

```cpp
server.setApiKey("secret");
// Clients must connect with: ws://host:port?token=secret
// Invalid token → connection closed with code 4001
```

### Registering Custom Commands

```cpp
// In application code:
server.registerCommand("myCustomCmd", [](const QJsonObject& params) {
    return QJsonObject{{"result", "value"}};
});
```

These are stored in `customCommands_` hash and checked in `CommandExecutor::dispatch()` after built-in commands.

### Registering Objects

```cpp
server.registerObject("myService", &serviceInstance);
// Then callable via: {"command":"call","params":{"object":"myService","method":"foo"}}
```

Methods must be `Q_INVOKABLE`. Introspected via Qt meta-object system.

---

## Event Broadcasting (EventBroadcaster)

### Event Types

- `widget_created` - Widget added to tree (via `QEvent::ChildAdded`)
- `widget_destroyed` - Widget removed from tree (via `QObject::destroyed`)
- `property_changed` - Property value changed (via polling at 100ms)
- `focus_changed` - Focus moved between widgets (via `QApplication::focusChanged`)
- `command_executed` - After each command or transaction

### Subscription Model

Each subscription carries an optional filter. Internally stored as:
```cpp
struct SubscriptionEntry {
  QString eventType;
  QJsonObject filter;  // e.g. {"target": "@name:btn", "property": "text"}
};

QHash<QString, QList<SubscriptionEntry>> clientSubscriptions_;  // client → entries
QHash<QString, QSet<QString>> eventSubscribers_;                // event → clients
```

Filters support `target` (with `@name:`, `@class:`, path prefix matching) and
`property` (for `property_changed` only). An empty filter matches all events of
that type.

### Activation

Event tracking is activated lazily via `updateUiEventTrackingState()`. When no
clients subscribe to UI events, no event filter is installed and no timer runs.
When the first subscription arrives the server installs a `QApplication` event
filter and (for `property_changed`) starts a 100ms poll timer.

### Emitting Events

```cpp
// Server hooks into Qt event system and calls:
broadcaster_->emitEvent("widget_created", eventData);

// EventBroadcaster checks filters, determines recipients, emits signal:
emit eventReady(eventType, data, recipientClientIds);

// Server catches signal and sends to subscribed clients
```

---

## Action Recording (ActionRecorder)

### Recording Flow

1. Client sends `record_start`
2. Server sets `recording_ = true`
3. Each command execution → `ActionRecorder::recordCommand()`
4. Client sends `record_stop`
5. Server returns JSON in `sample_tests.json` format

### Output Format

```json
{
  "name": "Recorded Session",
  "tests": [{
    "name": "Recorded Test",
    "steps": [
      {"command": "click", "params": {"target": "@name:btn"}},
      {"command": "type", "params": {"target": "@name:input", "text": "hello"}}
    ]
  }]
}
```

---

## Transaction Support

### Purpose

Execute multiple commands atomically with rollback on failure.

### Request Format

```json
{
  "transaction": true,
  "steps": [
    {"command": "type", "params": {"target": "@name:field", "text": "value"}},
    {"command": "click", "params": {"target": "@name:submit"}}
  ],
  "rollback_on_failure": true
}
```

### Undo Stack

Commands can record undo actions:
```cpp
recordUndo("Undo typing", [widget, oldText]() {
    widget->setText(oldText);
});
```

On failure with `rollback_on_failure`, undo stack executes in reverse order.

---

## Testing

### C++ Unit Tests (Qt Test)

Location: `tests/cpp/`

```bash
ctest --test-dir build --output-on-failure
```

Key test files:
- `test_protocol.cpp` - JSON serialization
- `test_element_finder.cpp` - Selector parsing, widget lookup
- `test_command_executor.cpp` - Command handlers
- `test_server.cpp` - Server lifecycle, client handling

### Python Integration Tests

Location: `tests/`

```bash
./build/sample/widgeteer_sample 9000 &
python tests/test_executor.py tests/sample_tests.json --port 9000
```

### Headless Testing

```bash
QT_QPA_PLATFORM=offscreen ctest --test-dir build
xvfb-run -a python3 tests/test_executor.py tests/sample_tests.json
```

---

## Common Modification Scenarios

### "Add a new command"
1. `CommandExecutor.h` - Declare `cmdMyCommand()`
2. `CommandExecutor.cpp` - Implement handler, register in constructor
3. `WidgeteerClient.h/cpp` - Add wrapper method (optional, for C++ API)
4. `tests/cpp/test_command_executor.cpp` - Add test
5. `docs/protocol.md` - Document the command

### "Support a new widget type in set_value"
1. `CommandExecutor::cmdSetValue()` - Add `qobject_cast` branch
2. `UIIntrospector::serializeWidget()` - Add properties if needed
3. `docs/protocol.md` - Document in set_value table

### "Add a new selector type"
1. `ElementFinder.cpp` - Add parsing in `parseSelector()`, implement finder
2. `tests/cpp/test_element_finder.cpp` - Add test
3. `docs/protocol.md` - Document selector syntax

### "Add a new event type"
1. `EventBroadcaster.h` - Add constant
2. Emit event from appropriate location
3. `docs/protocol.md` - Document event

### "Add a new wait condition"
1. `Synchronizer.h` - Add to `Condition` enum
2. `Synchronizer.cpp` - Handle in `checkCondition()`
3. `docs/protocol.md` - Document condition

---

## Build & Development

### Build Commands

```bash
# Debug build (with sanitizers + coverage)
cmake -B build
cmake --build build --parallel

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Before Committing

```bash
pre-commit run --all-files   # Format, lint
cmake --build build          # Build
ctest --test-dir build       # Test
```

### Code Style

- C++17, Qt6 idioms
- Namespace: `widgeteer::`
- clang-format (Google style, 100 char lines)
- `-Werror` (warnings as errors)

---

## WidgeteerClient (C++ Testing API)

### Purpose

`WidgeteerClient` provides a fluent C++ API for in-process widget testing. Unlike the WebSocket-based Server, it runs in the same process as the application, making it ideal for unit tests.

### Architecture

```
WidgeteerClient
├── owns CommandExecutor (if default constructor)
└── OR uses external CommandExecutor (if passed in)
```

### Result<T, E> Template

All methods return `Result<T, E>` for clean error handling:

```cpp
Result<void> click(const QString& target);      // Actions return Result<void>
Result<QString> getText(const QString& target); // Queries return Result<T>
Result<QJsonObject> getTree();                  // Introspection returns JSON
```

Usage pattern:
```cpp
auto result = client.click("@name:button");
if (result) {
    // success
} else {
    qDebug() << result.error().code << result.error().message;
}
```

### Adding New Methods

1. Add declaration in `WidgeteerClient.h`
2. Implement in `WidgeteerClient.cpp` using the pattern:

```cpp
Result<void> WidgeteerClient::myMethod(const QString& target) {
    QJsonObject params;
    params["target"] = target;
    return execute("command_name", params);  // For void results
}

Result<QString> WidgeteerClient::myQuery(const QString& target) {
    QJsonObject params;
    params["target"] = target;
    auto result = executeForResult("command_name", params);
    if (!result) {
        return Result<QString>::fail(result.error());
    }
    return Result<QString>::ok(result.value()["key"].toString());
}
```

3. Add test in `tests/cpp/test_widgeteer_client.cpp`
