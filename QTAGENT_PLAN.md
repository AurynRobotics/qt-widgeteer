# QtAgent

A Qt6 UI testing and automation framework designed for LLM agent control.

## Overview

QtAgent is a minimally invasive library that enables Qt6 QWidgets applications to be controlled programmatically via a JSON-based HTTP API. Unlike traditional testing frameworks that assume pre-written test scripts, QtAgent is designed with LLM agents in mind — providing full UI introspection so an agent can discover, explore, and interact with the application without prior knowledge of its structure.

### Design Goals

| Goal | Description |
|------|-------------|
| **LLM-friendly** | Structured JSON responses with enough context for an agent to reason |
| **Discoverable** | Agent can explore UI without prior knowledge via `get_tree` |
| **Atomic & composite** | Support single actions and multi-step transactions with rollback |
| **Synchronous by default** | Actions block until UI is stable, with configurable timeouts |
| **Minimal integration** | 2-3 lines to add to any Qt6 app |
| **Zero external dependencies** | Uses only Qt6 modules |

### Non-Goals

- QML/QtQuick support (QWidgets only)
- Record & playback functionality
- Visual test builder GUI

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         QtAgent::Server                             │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                    HTTP Layer (QHttpServer)                 │    │
│  │    GET  /tree        - UI introspection                     │    │
│  │    GET  /screenshot  - Screen capture                       │    │
│  │    POST /command     - Single command execution             │    │
│  │    POST /transaction - Multi-step execution                 │    │
│  │    GET  /health      - Server status                        │    │
│  │    GET  /schema      - Command schema for LLM               │    │
│  └──────────────────────────────┬──────────────────────────────┘    │
│                                 │                                   │
│  ┌──────────────────────────────▼──────────────────────────────┐    │
│  │                    Command Executor                         │    │
│  │  - Parse & validate JSON commands                           │    │
│  │  - Route to appropriate handler                             │    │
│  │  - Transaction management (multi-step)                      │    │
│  │  - Rollback stack                                           │    │
│  │  - Main thread dispatch                                     │    │
│  └──────────────────────────────┬──────────────────────────────┘    │
│                                 │                                   │
│  ┌─────────────┬────────────────┼────────────────┬─────────────┐    │
│  │             │                │                │             │    │
│  ▼             ▼                ▼                ▼             ▼    │
│ Element     Event           Screen           State        Synchro- │
│ Finder      Injector        Capture          Inspector    nizer    │
│                                                                     │
│ - Path      - QTest::       - grab()         - property()  - wait  │
│   resolution  mouse*        - image diff     - metaObject()  cond. │
│ - Queries   - QTest::key*   - base64 enc     - children()  - idle  │
│ - Caching                                                   - signal│
│  └─────────────┴────────────────┴────────────────┴─────────────┘    │
│                                 │                                   │
│                                 ▼                                   │
│                    ┌───────────────────────┐                        │
│                    │   Qt Application      │                        │
│                    │   (QWidget Tree)      │                        │
│                    └───────────────────────┘                        │
└─────────────────────────────────────────────────────────────────────┘
```

---

## JSON Protocol

### Request Format

#### Single Command

```json
{
  "id": "req-123",
  "command": "click",
  "params": {
    "target": "mainWindow/sidebar/loadButton"
  },
  "options": {
    "timeout_ms": 5000,
    "wait_for_idle": true
  }
}
```

#### Multi-step Transaction

```json
{
  "id": "req-456",
  "transaction": true,
  "rollback_on_failure": true,
  "steps": [
    {
      "command": "click",
      "params": { "target": "menuBar/fileMenu" }
    },
    {
      "command": "wait",
      "params": { "target": "fileMenu/openAction", "condition": "visible" }
    },
    {
      "command": "click",
      "params": { "target": "fileMenu/openAction" }
    },
    {
      "command": "wait",
      "params": { "target": "fileDialog", "condition": "exists" }
    },
    {
      "command": "type",
      "params": { "target": "fileDialog/pathEdit", "text": "/data/test.csv" }
    },
    {
      "command": "click",
      "params": { "target": "fileDialog/openButton" }
    }
  ]
}
```

### Response Format

#### Success

```json
{
  "id": "req-123",
  "success": true,
  "result": {
    "clicked": true,
    "target_geometry": {"x": 100, "y": 200, "width": 80, "height": 24}
  },
  "duration_ms": 45
}
```

#### Failure (with context for LLM recovery)

```json
{
  "id": "req-123",
  "success": false,
  "error": {
    "code": "ELEMENT_NOT_FOUND",
    "message": "No widget matching 'mainWindow/sidebar/loadBtn'",
    "details": {
      "searched_path": "mainWindow/sidebar/loadBtn",
      "partial_match": "mainWindow/sidebar",
      "available_children": ["loadButton", "saveButton", "plotArea"]
    }
  }
}
```

#### Transaction Response

```json
{
  "id": "req-456",
  "success": false,
  "completed_steps": 3,
  "total_steps": 6,
  "steps_results": [
    {"step": 0, "success": true, "command": "click"},
    {"step": 1, "success": true, "command": "wait"},
    {"step": 2, "success": true, "command": "click"},
    {"step": 3, "success": false, "command": "wait", "error": {"code": "TIMEOUT"}}
  ],
  "rollback_performed": true
}
```

---

## Element Addressing

Multiple addressing schemes are supported:

```json
// Path-based (primary) - slash-separated, recursive search
{"target": "mainWindow/plotArea/legend/item_0"}

// By objectName (global search)
{"target": "@name:plotWidget"}

// By class (returns first match)
{"target": "@class:QLineEdit"}

// By accessible name
{"target": "@accessible:Load Data Button"}

// By text content (buttons, labels)
{"target": "@text:Open File"}

// Combined filters
{"target": "@class:QPushButton@text:OK"}

// Index for duplicates
{"target": "mainWindow/buttonBox/QPushButton[1]"}

// Wildcard
{"target": "mainWindow/*/saveButton"}
```

---

## Command Catalog

### Introspection Commands

| Command | Params | Returns |
|---------|--------|---------|
| `get_tree` | `root?`, `depth?`, `filter?` | Full/partial widget tree as JSON |
| `find` | `query`, `match_type?` | List of matching elements |
| `describe` | `target` | Detailed single element info |
| `get_property` | `target`, `property` | Property value |
| `list_properties` | `target` | All properties with types |
| `get_actions` | `target` | Available actions/slots |

### Action Commands

| Command | Params | Description |
|---------|--------|-------------|
| `click` | `target`, `button?`, `modifiers?`, `pos?` | Mouse click |
| `double_click` | `target`, `pos?` | Double click |
| `right_click` | `target`, `pos?` | Context menu click |
| `type` | `target`, `text`, `clear_first?` | Enter text |
| `key` | `target`, `key`, `modifiers?` | Single key press |
| `key_sequence` | `target`, `sequence` | Key combo, e.g., "Ctrl+Shift+S" |
| `drag` | `from`, `to`, `from_pos?`, `to_pos?` | Drag and drop |
| `scroll` | `target`, `delta_x`, `delta_y` | Mouse wheel |
| `hover` | `target`, `pos?` | Move mouse over element |
| `focus` | `target` | Set keyboard focus |

### State Commands

| Command | Params | Description |
|---------|--------|-------------|
| `set_property` | `target`, `property`, `value` | Modify property |
| `invoke` | `target`, `method`, `args?` | Call Q_INVOKABLE or slot |
| `set_value` | `target`, `value` | Smart value setting (combo, spin, etc.) |

### Verification Commands

| Command | Params | Description |
|---------|--------|-------------|
| `screenshot` | `target?`, `format?` | Capture PNG/JPG as base64 |
| `compare_screenshot` | `target`, `reference`, `threshold?` | Visual diff |
| `assert` | `target`, `property`, `operator`, `value` | Verify state |
| `exists` | `target` | Check if element exists |
| `is_visible` | `target` | Check visibility |

### Synchronization Commands

| Command | Params | Description |
|---------|--------|-------------|
| `wait` | `target`, `condition`, `timeout_ms?` | Wait for condition |
| `wait_idle` | `timeout_ms?` | Wait for event queue empty |
| `wait_signal` | `target`, `signal`, `timeout_ms?` | Wait for Qt signal |
| `sleep` | `ms` | Hard delay (avoid if possible) |

#### Wait Conditions

- `exists` — widget found in tree
- `not_exists` — widget removed
- `visible` / `not_visible`
- `enabled` / `disabled`
- `property:name=value` — property matches value
- `stable` — geometry unchanged for N ms

---

## UI Tree Output Format

The `get_tree` command returns a hierarchical JSON structure:

```json
{
  "objectName": "mainWindow",
  "class": "QMainWindow",
  "role": "window",
  "geometry": {"x": 0, "y": 0, "width": 1200, "height": 800},
  "visible": true,
  "enabled": true,
  "children": [
    {
      "objectName": "centralWidget",
      "class": "QWidget",
      "role": "container",
      "children": [
        {
          "objectName": "loadButton",
          "class": "QPushButton",
          "role": "button",
          "text": "Load Data",
          "enabled": true,
          "geometry": {"x": 10, "y": 10, "width": 100, "height": 30}
        },
        {
          "objectName": "plotWidget",
          "class": "PlotWidget",
          "role": "custom",
          "geometry": {"x": 10, "y": 50, "width": 800, "height": 600},
          "children": []
        }
      ]
    }
  ]
}
```

---

## Component Specifications

### ElementFinder

Responsible for resolving element selectors to QWidget pointers.

```cpp
class ElementFinder {
public:
    struct FindResult {
        QWidget* widget = nullptr;
        QString resolvedPath;
        QString error;
    };

    struct FindOptions {
        bool recursive = true;
        int maxResults = 100;
        bool visibleOnly = false;
    };

    FindResult find(const QString& selector);
    QList<FindResult> findAll(const QString& selector, FindOptions opts = {});
    QString pathFor(QWidget* widget);  // Reverse lookup

private:
    QWidget* byPath(const QString& path);
    QWidget* byName(const QString& name);
    QWidget* byClass(const QString& className);
    QWidget* byText(const QString& text);
    QWidget* byAccessible(const QString& name);
    QWidget* byFilter(const QVariantMap& filters);

    QHash<QString, QPointer<QWidget>> cache;
    void invalidateCache();
};
```

### UIIntrospector

Responsible for generating JSON representations of the widget tree.

```cpp
class UIIntrospector {
public:
    struct TreeOptions {
        int maxDepth = -1;              // -1 = unlimited
        bool includeInvisible = false;
        bool includeGeometry = true;
        bool includeProperties = false; // All props (verbose)
        QStringList classFilter;        // Only include these classes
    };

    QJsonObject getTree(QWidget* root = nullptr, TreeOptions opts = {});
    QJsonObject describe(QWidget* widget);
    QJsonArray listProperties(QWidget* widget);
    QJsonArray listActions(QWidget* widget);

private:
    QJsonObject widgetToJson(QWidget* w, int depth, const TreeOptions& opts);
    QJsonValue propertyToJson(const QVariant& value);
    QString inferWidgetRole(QWidget* w);  // "button", "input", "container", etc.
};
```

### EventInjector

Wraps QTest functions for event injection.

```cpp
class EventInjector {
public:
    struct Result {
        bool success;
        QString error;
    };

    // Mouse
    Result click(QWidget* target, Qt::MouseButton btn = Qt::LeftButton,
                 QPoint pos = {}, Qt::KeyboardModifiers mods = {});
    Result doubleClick(QWidget* target, QPoint pos = {});
    Result press(QWidget* target, Qt::MouseButton btn, QPoint pos = {});
    Result release(QWidget* target, Qt::MouseButton btn, QPoint pos = {});
    Result move(QWidget* target, QPoint pos);
    Result drag(QWidget* source, QPoint from, QWidget* dest, QPoint to);
    Result scroll(QWidget* target, int deltaX, int deltaY, QPoint pos = {});

    // Keyboard
    Result type(QWidget* target, const QString& text);
    Result keyPress(QWidget* target, Qt::Key key, Qt::KeyboardModifiers mods = {});
    Result keyRelease(QWidget* target, Qt::Key key, Qt::KeyboardModifiers mods = {});
    Result shortcut(QWidget* target, const QKeySequence& seq);

    // Focus
    Result setFocus(QWidget* target);
    Result clearFocus();

private:
    void ensureVisible(QWidget* target);
    void ensureFocusable(QWidget* target);
    QPoint resolvePosition(QWidget* target, QPoint pos);
};
```

### Synchronizer

Handles wait conditions and synchronization.

```cpp
class Synchronizer {
public:
    enum class Condition {
        Exists,
        NotExists,
        Visible,
        NotVisible,
        Enabled,
        Disabled,
        Focused,
        PropertyEquals,
        Stable,
        Idle
    };

    struct WaitParams {
        QString target;
        Condition condition;
        QString propertyName;
        QVariant propertyValue;
        int timeoutMs = 5000;
        int pollIntervalMs = 50;
        int stabilityMs = 200;
    };

    struct WaitResult {
        bool success;
        int elapsedMs;
        QString error;
    };

    WaitResult wait(const WaitParams& params);
    WaitResult waitForIdle(int timeoutMs = 5000);
    WaitResult waitForSignal(QObject* obj, const char* signal, int timeoutMs = 5000);

private:
    bool checkCondition(const WaitParams& params);
};
```

### CommandExecutor

Central dispatcher for all commands.

```cpp
class CommandExecutor : public QObject {
    Q_OBJECT
public:
    struct Command {
        QString name;
        QJsonObject params;
        QJsonObject options;
    };

    struct Transaction {
        QString id;
        QList<Command> steps;
        bool rollbackOnFailure = true;
    };

    QJsonObject execute(const Command& cmd);
    QJsonObject execute(const Transaction& tx);

signals:
    void stepCompleted(int index, bool success);
    void transactionCompleted(const QString& id, bool success);

private:
    // Command handlers
    QJsonObject cmdGetTree(const QJsonObject& params);
    QJsonObject cmdFind(const QJsonObject& params);
    QJsonObject cmdDescribe(const QJsonObject& params);
    QJsonObject cmdClick(const QJsonObject& params);
    QJsonObject cmdType(const QJsonObject& params);
    QJsonObject cmdScreenshot(const QJsonObject& params);
    QJsonObject cmdWait(const QJsonObject& params);
    QJsonObject cmdGetProperty(const QJsonObject& params);
    QJsonObject cmdSetProperty(const QJsonObject& params);
    QJsonObject cmdInvoke(const QJsonObject& params);

    // Rollback support
    struct UndoAction {
        std::function<void()> undo;
        QString description;
    };
    QStack<UndoAction> undoStack;
    void recordUndo(UndoAction action);
    void rollback();

    // Components
    ElementFinder finder;
    UIIntrospector introspector;
    EventInjector injector;
    Synchronizer synchronizer;
};
```

### AgentServer

HTTP server exposing the API.

```cpp
class AgentServer : public QObject {
    Q_OBJECT
public:
    explicit AgentServer(QObject* parent = nullptr);

    bool start(quint16 port = 9000);
    void stop();

    void setAllowedHosts(const QStringList& hosts);
    void enableCORS(bool enable);
    void setRootWidget(QWidget* root);  // Limit scope

signals:
    void requestReceived(const QString& id, const QString& command);
    void responseReady(const QString& id, bool success);

private:
    QHttpServer server;
    CommandExecutor executor;

    void setupRoutes();

    QHttpServerResponse handleCommand(const QHttpServerRequest& req);
    QHttpServerResponse handleTransaction(const QHttpServerRequest& req);
    QHttpServerResponse handleTree(const QHttpServerRequest& req);
    QHttpServerResponse handleScreenshot(const QHttpServerRequest& req);
    QHttpServerResponse handleHealth(const QHttpServerRequest& req);
    QHttpServerResponse handleSchema(const QHttpServerRequest& req);

    template<typename Func>
    QJsonObject executeOnMainThread(Func&& func);
};
```

---

## Thread Safety

HTTP requests arrive on background threads. All Qt GUI operations must execute on the main thread.

```
HTTP Thread(s)                     Main/GUI Thread
      │                                   │
      │  ┌─────────────────────────────┐  │
      │  │  QMetaObject::invokeMethod  │  │
      ├──│  (BlockingQueuedConnection) │──▶  Execute command
      │  └─────────────────────────────┘  │         │
      │                                   │         ▼
      │◀──────────────────────────────────│──── Return result
      │                                   │
      ▼                                   │
 Send HTTP response                       │
```

```cpp
template<typename Func>
QJsonObject AgentServer::executeOnMainThread(Func&& func) {
    QJsonObject result;

    if (QThread::currentThread() == qApp->thread()) {
        result = func();
    } else {
        QMetaObject::invokeMethod(qApp, [&]() {
            result = func();
        }, Qt::BlockingQueuedConnection);
    }

    return result;
}
```

---

## Integration API

### Minimal Integration (2 lines)

```cpp
#include <qtagent/Server>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QtAgent::Server agent;
    agent.start(9000);

    MainWindow window;
    window.show();

    return app.exec();
}
```

### With Configuration

```cpp
QtAgent::Server agent;
agent.setPort(9000);
agent.setAllowedHosts({"127.0.0.1", "::1"});  // Localhost only
agent.enableLogging(true);
agent.setRootWidget(&window);  // Limit introspection scope
agent.start();
```

---

## HTTP Endpoints

| Method | Endpoint | Body | Description |
|--------|----------|------|-------------|
| GET | `/health` | — | Server status |
| GET | `/schema` | — | Full command schema (for LLM) |
| GET | `/tree` | Query params | Widget tree |
| GET | `/screenshot` | Query params | Screen capture |
| POST | `/command` | Command JSON | Execute single command |
| POST | `/transaction` | Transaction JSON | Execute multi-step |

---

## File Structure

```
qtagent/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── include/
│   └── qtagent/
│       ├── Server.h              # Main public API
│       ├── CommandExecutor.h
│       ├── ElementFinder.h
│       ├── EventInjector.h
│       ├── UIIntrospector.h
│       ├── Synchronizer.h
│       ├── Protocol.h            # JSON structures & error codes
│       └── Export.h              # DLL export macros
├── src/
│   ├── Server.cpp
│   ├── CommandExecutor.cpp
│   ├── ElementFinder.cpp
│   ├── EventInjector.cpp
│   ├── UIIntrospector.cpp
│   ├── Synchronizer.cpp
│   └── Protocol.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_finder.cpp
│   ├── test_introspector.cpp
│   ├── test_injector.cpp
│   └── test_commands.cpp
└── examples/
    ├── CMakeLists.txt
    ├── simple/
    │   ├── CMakeLists.txt
    │   └── main.cpp
    └── demo_app/
        ├── CMakeLists.txt
        └── main.cpp
```

---

## Dependencies

| Dependency | Purpose | Required |
|------------|---------|----------|
| Qt6::Core | Base functionality | ✅ |
| Qt6::Widgets | Widget introspection | ✅ |
| Qt6::Gui | Screenshots | ✅ |
| Qt6::Test | Event injection | ✅ |
| Qt6::HttpServer | HTTP API | ✅ |

### CMake

```cmake
cmake_minimum_required(VERSION 3.18)
project(qtagent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Test HttpServer)

add_library(qtagent
    src/Server.cpp
    src/CommandExecutor.cpp
    src/ElementFinder.cpp
    src/EventInjector.cpp
    src/UIIntrospector.cpp
    src/Synchronizer.cpp
    src/Protocol.cpp
)

target_include_directories(qtagent PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(qtagent PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
    Qt6::HttpServer
)
```

---

## Implementation Phases

### Phase 1: Core Foundation (MVP)

**Goal:** Basic introspection + simple actions

- [ ] `ElementFinder` — path-based lookup
- [ ] `UIIntrospector` — `get_tree`, `describe`
- [ ] `EventInjector` — `click`, `type`, `key`
- [ ] `Server` — HTTP with `/command`, `/tree`, `/health`
- [ ] Basic JSON protocol

**Deliverable:** Can explore UI and perform basic interactions

**Estimated time:** 2-3 days

---

### Phase 2: Robustness

**Goal:** Reliable enough for real testing

- [ ] `Synchronizer` — `wait` conditions
- [ ] `ElementFinder` — all selector types (@name, @class, @text, etc.)
- [ ] Screenshots — `screenshot` command
- [ ] Error handling — detailed error responses with context
- [ ] Thread safety — proper main thread dispatch
- [ ] Logging

**Deliverable:** Can write reliable multi-step tests

**Estimated time:** 3-4 days

---

### Phase 3: Transactions

**Goal:** Complex multi-step operations

- [ ] `CommandExecutor` — transaction support
- [ ] Rollback mechanism
- [ ] Step-by-step results
- [ ] `invoke` command for slots/methods
- [ ] Property setters
- [ ] `compare_screenshot` with threshold

**Deliverable:** Full workflow automation

**Estimated time:** 2-3 days

---

### Phase 4: LLM Optimization

**Goal:** Best experience for AI agents

- [ ] `/schema` endpoint — full JSON schema for all commands
- [ ] Rich `describe` output — suggest possible actions per widget type
- [ ] Helpful error messages with suggestions
- [ ] Widget role inference ("button", "input", "list", etc.)
- [ ] Caching and performance optimization

**Deliverable:** Agent can autonomously explore and test

**Estimated time:** 2-3 days

---

### Phase 5: Polish & Release

**Goal:** Production ready

- [ ] Comprehensive unit tests
- [ ] Integration tests with example app
- [ ] Documentation
- [ ] CMake install targets
- [ ] Conan recipe (optional)
- [ ] CI/CD setup

**Estimated time:** 2-3 days

---

## Error Codes

| Code | Description |
|------|-------------|
| `ELEMENT_NOT_FOUND` | No widget matching selector |
| `ELEMENT_NOT_VISIBLE` | Widget exists but not visible |
| `ELEMENT_NOT_ENABLED` | Widget is disabled |
| `PROPERTY_NOT_FOUND` | Property doesn't exist on widget |
| `PROPERTY_READ_ONLY` | Cannot set read-only property |
| `INVALID_SELECTOR` | Malformed selector syntax |
| `INVALID_COMMAND` | Unknown command name |
| `INVALID_PARAMS` | Missing or invalid parameters |
| `TIMEOUT` | Wait condition not met in time |
| `INVOCATION_FAILED` | Method/slot invocation failed |
| `SCREENSHOT_FAILED` | Could not capture screenshot |
| `TRANSACTION_FAILED` | Multi-step transaction failed |

---

## Future Considerations

Items not in scope for initial release but worth considering:

1. **WebSocket support** — Streaming for large trees or real-time updates
2. **Authentication** — Token-based auth for non-localhost access
3. **Recording mode** — Capture user actions as JSON commands
4. **Accessibility tree** — Expose QAccessible as alternative view
5. **Plugin system** — Custom commands for specific applications
6. **Headless mode** — Run without display (QOffscreenSurface)

---

## License

MIT License (recommended for maximum adoption)
