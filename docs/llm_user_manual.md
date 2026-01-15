# Widgeteer LLM User Manual

Context-optimized guide for AI agents to integrate and use Widgeteer with Qt applications.

## Quick Reference

**What**: Qt6 UI testing/automation library with WebSocket API
**Use case**: Automated testing, UI automation, AI-driven Qt app control
**Protocol**: WebSocket (ws://host:port?token=key)
**Message format**: JSON with `type` field

## Integration (C++ Side)

### Minimal Setup
```cpp
#include <widgeteer/Server.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    widgeteer::Server server;
    server.setApiKey("your-secret-key");  // Optional auth
    server.enableLogging(true);            // Debug logging
    server.start(9000);                    // Start on port 9000

    // Your Qt application code...
    MainWindow w;
    w.show();

    return app.exec();
}
```

### Key Server Methods
| Method | Purpose |
|--------|---------|
| `start(port)` | Start WebSocket server |
| `stop()` | Stop server |
| `setApiKey(key)` | Enable token auth |
| `setRootWidget(w)` | Limit scope to widget tree |
| `registerCommand(name, handler)` | Add custom command |
| `registerObject(name, obj)` | Expose QObject methods |
| `startRecording()` / `stopRecording()` | Record commands |
| `getRecording()` | Get recorded JSON |

## Client Usage (Python)

### Async Client
```python
from widgeteer_client import WidgeteerClient

async with WidgeteerClient(port=9000, token="key") as client:
    await client.click("@name:button1")
    await client.type_text("@name:input", "Hello")
    result = await client.get_property("@name:label", "text")
```

### Sync Client
```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    client.click("@name:button1")
```

## Selectors

| Pattern | Example | Matches |
|---------|---------|---------|
| `@name:X` | `@name:saveBtn` | objectName == "saveBtn" |
| `@class:X` | `@class:QPushButton` | Class name match |
| `@text:X` | `@text:OK` | Button/label text |
| `@accessible:X` | `@accessible:Save` | Accessible name |
| `path/to/widget` | `mainWindow/toolbar/saveBtn` | Hierarchy path |
| `path[n]` | `list/item[0]` | Index for duplicates |
| `*/child` | `container/*/label` | Wildcard |

## Commands

### Navigation & Inspection
```json
{"type":"command","command":"get_tree","params":{}}
{"type":"command","command":"find","params":{"query":"@class:QPushButton"}}
{"type":"command","command":"describe","params":{"target":"@name:widget"}}
{"type":"command","command":"exists","params":{"target":"@name:widget"}}
{"type":"command","command":"is_visible","params":{"target":"@name:widget"}}
```

### Actions
```json
{"type":"command","command":"click","params":{"target":"@name:btn","button":"left"}}
{"type":"command","command":"double_click","params":{"target":"@name:item"}}
{"type":"command","command":"type","params":{"target":"@name:input","text":"hello","clear_first":true}}
{"type":"command","command":"key","params":{"target":"@name:input","key":"Enter"}}
{"type":"command","command":"key_sequence","params":{"target":"@name:input","sequence":"Ctrl+S"}}
{"type":"command","command":"scroll","params":{"target":"@name:list","delta_y":-100}}
{"type":"command","command":"focus","params":{"target":"@name:input"}}
```

### State Modification
```json
{"type":"command","command":"set_property","params":{"target":"@name:spin","property":"value","value":42}}
{"type":"command","command":"set_value","params":{"target":"@name:combo","value":"Option2"}}
{"type":"command","command":"invoke","params":{"target":"@name:btn","method":"click"}}
```

### set_value Widget Support
| Widget | Value Type | Example |
|--------|------------|---------|
| QSpinBox/QDoubleSpinBox | number | `42`, `3.14` |
| QLineEdit/QTextEdit | string | `"text"` |
| QComboBox | string/int | `"Option"` or `1` |
| QCheckBox/QRadioButton | bool | `true` |
| QSlider/QDial | int | `50` |
| QDateEdit/QTimeEdit | ISO string | `"2024-01-15"`, `"14:30:00"` |
| QTabWidget/QStackedWidget | int | `0` |
| QListWidget | int/string | `0` or `"Item text"` |
| QTableWidget | object | `{"row":0,"column":1}` |
| QCalendarWidget | ISO date | `"2024-01-15"` |

### Synchronization
```json
{"type":"command","command":"wait","params":{"target":"@name:btn","condition":"visible","timeout_ms":5000}}
{"type":"command","command":"wait_idle","params":{"timeout_ms":1000}}
{"type":"command","command":"sleep","params":{"ms":500}}
```

Wait conditions: `exists`, `not_exists`, `visible`, `not_visible`, `enabled`, `disabled`, `stable`, `property:name=value`

### Assertions
```json
{"type":"command","command":"assert","params":{"target":"@name:label","property":"text","operator":"==","value":"Expected"}}
```
Operators: `==`, `!=`, `>`, `<`, `>=`, `<=`, `contains`

## Events & Recording

### Subscribe to Events
```json
{"type":"subscribe","event_type":"command_executed"}
{"type":"unsubscribe","event_type":"command_executed"}
```

Event types: `widget_created`, `widget_destroyed`, `property_changed`, `focus_changed`, `command_executed`

### Recording
```json
{"type":"record_start"}
{"type":"record_stop"}  // Returns recorded commands as JSON test file
```

## Response Format
```json
{
  "type": "response",
  "id": "request-id",
  "success": true,
  "result": {...},
  "duration_ms": 15
}
```

Error response:
```json
{
  "type": "response",
  "id": "request-id",
  "success": false,
  "error": {"code": "ELEMENT_NOT_FOUND", "message": "..."}
}
```

## CLI Tool

```bash
# Basic commands
widgeteer tree                           # Get widget tree
widgeteer find "@class:QPushButton"      # Find buttons
widgeteer click "@name:saveBtn"          # Click widget
widgeteer type "@name:input" "Hello"     # Type text
widgeteer get-property "@name:label" text

# With auth
widgeteer -t secret-token click "@name:btn"

# Run test file
widgeteer run tests/sample_tests.json

# Recording
widgeteer record start
# ... interact with app ...
widgeteer record stop -o recorded.json
```

## Common Patterns

### Wait for element then interact
```python
await client.wait("@name:dialog", condition="visible")
await client.click("@name:okBtn")
```

### Form filling
```python
await client.set_value("@name:nameInput", "John")
await client.set_value("@name:ageSpinbox", 25)
await client.set_value("@name:countryCombo", "USA")
await client.click("@name:submitBtn")
```

### Custom command (C++ side)
```cpp
server.registerCommand("loadProject", [&app](const QJsonObject& params) {
    QString path = params["path"].toString();
    bool ok = app.loadProject(path);
    return QJsonObject{{"success", ok}, {"path", path}};
});
```

### Expose QObject methods
```cpp
server.registerObject("dataService", &dataService);
// Then call via: {"command":"call","params":{"object":"dataService","method":"refresh"}}
```

## Error Codes
| Code | Meaning |
|------|---------|
| `ELEMENT_NOT_FOUND` | Selector didn't match |
| `INVALID_COMMAND` | Unknown command name |
| `INVALID_PARAMS` | Missing/wrong parameters |
| `INVOCATION_FAILED` | Method call failed |
| `TIMEOUT` | Wait condition timeout |
| `ASSERTION_FAILED` | Assert failed |

## Best Practices

1. **Use objectNames**: Set `setObjectName()` on key widgets for stable selectors
2. **Wait before interact**: Use `wait` for dynamic UI instead of `sleep`
3. **Scope with root**: Use `setRootWidget()` to limit search scope
4. **Use set_value**: Prefer `set_value` over `type` for form controls
5. **Check exists first**: Verify element exists before complex interactions
6. **Enable logging**: Use `enableLogging(true)` during development
