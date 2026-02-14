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

    # Use .value for direct access to results
    result = await client.get_property("@name:label", "text")
    print(result.value)  # The text value directly

    # Boolean checks return bool via .value
    result = await client.exists("@name:button1")
    if result.value:  # True/False
        print("Button exists!")
```

### Sync Client
```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    client.click("@name:button1")

    # Same .value API works
    text = client.get_property("@name:label", "text").value
```

### Batch Commands (Reduced Latency)
```python
# Execute multiple commands in parallel - all sent before waiting for responses
results = await client.batch([
    {"command": "get_property", "params": {"target": "@name:edit1", "property": "text"}},
    {"command": "get_property", "params": {"target": "@name:edit2", "property": "text"}},
    {"command": "is_visible", "params": {"target": "@name:dialog"}},
])
text1, text2, visible = [r.value for r in results]

# Simpler tuple syntax
text1, text2 = await client.batch_commands(
    ("get_property", {"target": "@name:edit1", "property": "text"}),
    ("get_property", {"target": "@name:edit2", "property": "text"}),
)
```

## Selectors

The Python client supports both CSS-like syntax (simpler) and native syntax:

| CSS-like | Native | Matches |
|----------|--------|---------|
| `#saveBtn` | `@name:saveBtn` | objectName == "saveBtn" |
| `.QPushButton` | `@class:QPushButton` | Qt class name |
| `[text="OK"]` | `@text:OK` | Button/label text |
| `[accessible="Save"]` | `@accessible:Save` | Accessible name |

**Semantic Role Selectors** (for dialog buttons):
| Selector | Finds |
|----------|-------|
| `@accept`, `@ok` | OK button in QDialogButtonBox |
| `@reject`, `@cancel` | Cancel button in QDialogButtonBox |
| `@apply` | Apply button |
| `@help` | Help button |
| `@yes`, `@no` | Yes/No buttons |
| `@save`, `@discard` | Save/Discard buttons |
| `@close`, `@reset` | Close/Reset buttons |

These selectors find standard buttons in the active window's QDialogButtonBox:
```python
client.click("@accept")     # Click OK button in current dialog
client.click("@cancel")     # Click Cancel button
```

**Hierarchical paths** (both syntaxes work):
```python
"mainWindow/toolbar/saveBtn"      # Plain path
"mainWindow/#saveBtn"             # CSS-like in path
"dialog/.QPushButton"             # Class in path
"list/item[0]"                    # Index for duplicates
"container/*/label"               # Wildcard
```

**Examples:**
```python
client.click("#submitButton")           # By objectName
client.click(".QPushButton")            # First QPushButton
client.click('[text="OK"]')             # By text content
client.click("dialog/#okBtn")           # Path with CSS selector
```

## Commands

### Navigation & Inspection
```json
{"type":"command","command":"get_tree","params":{"depth":3,"include_invisible":false,"include_geometry":true}}
{"type":"command","command":"find","params":{"query":"@class:QPushButton","max_results":100,"visible_only":false}}
{"type":"command","command":"describe","params":{"target":"@name:widget"}}
{"type":"command","command":"exists","params":{"target":"@name:widget"}}
{"type":"command","command":"is_visible","params":{"target":"@name:widget"}}
{"type":"command","command":"get_form_fields","params":{"visible_only":true}}
```

Note: `params` can be omitted or partial - missing fields use defaults.

### get_form_fields (LLM-friendly form discovery)
Returns all form input widgets with labels, values, and suggested selectors:
```json
{"type":"command","command":"get_form_fields","params":{"root":"@name:formPanel","visible_only":true}}
```

Response includes for each field:
- `objectName`, `class`, `type` (text/checkbox/combobox/etc.)
- `label` - associated QLabel text (if found via buddy or proximity)
- `value` - current value
- `selector` - suggested selector for automation
- `toolTip`, `accessibleName` - accessibility hints

### Actions
```json
{"type":"command","command":"click","params":{"target":"@name:btn","button":"left"}}
{"type":"command","command":"double_click","params":{"target":"@name:item"}}
{"type":"command","command":"type","params":{"target":"@name:input","text":"hello","clear_first":true}}
{"type":"command","command":"key","params":{"target":"@name:input","key":"Enter"}}
{"type":"command","command":"key","params":{"target":"@name:dialog","key":"Escape","blocking":false}}
{"type":"command","command":"key_sequence","params":{"target":"@name:input","sequence":"Ctrl+S"}}
{"type":"command","command":"scroll","params":{"target":"@name:list","delta_y":-100}}
{"type":"command","command":"focus","params":{"target":"@name:input"}}
```

**Non-blocking key events**: Use `blocking: false` when sending key events that might close a dialog. This sends the key asynchronously and returns immediately, preventing timeouts:
```python
# Close dialog without waiting (avoids timeout when dialog closes)
client.key("@name:dialog", "Escape", blocking=False)
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

Default timeout is 5000ms if `timeout_ms` is omitted.

Wait conditions: `exists`, `not_exists`, `visible`, `not_visible`, `enabled`, `disabled`, `stable`, `property:name=value`

### Screenshots
```json
{"type":"command","command":"screenshot","params":{"format":"png"}}
{"type":"command","command":"screenshot","params":{"target":"@name:dialog","format":"png"}}
{"type":"command","command":"screenshot","params":{"annotate":true,"format":"png"}}
```

Parameters: `target` (optional selector), `format` ("png"/"jpg"), `annotate` (bool).

**Annotated Screenshots** (`annotate: true`):
- Draws colored bounding boxes around interactive widgets
- Labels each widget with `[index] name/text`
- Returns `annotations` array with widget info:
  - `index` - number shown in image
  - `role` - widget type (button, textfield, etc.)
  - `selector` - ready-to-use selector
  - `bounds` - position in image `{x, y, width, height}`
  - Widget-specific: `text`, `value`, `currentText`

Useful for visual UI understanding before automation.

### Window and Dialog Commands

When working with applications that have modal dialogs, use these commands to inspect and interact with them:

```json
{"type":"command","command":"list_windows","params":{}}
{"type":"command","command":"is_dialog_open","params":{}}
{"type":"command","command":"accept_dialog","params":{}}
{"type":"command","command":"reject_dialog","params":{}}
{"type":"command","command":"close_window","params":{"target":"@name:optionalWidget"}}
```

**list_windows** - Returns all visible top-level windows including modal dialogs:
- `windows` array with: `objectName`, `windowTitle`, `className`, `isModal`, `isActiveWindow`, `geometry`
- `count` - number of windows

**is_dialog_open** - Check if a modal dialog is the active window:
- `is_open` - boolean
- `window_title`, `class_name` if open

**accept_dialog** / **reject_dialog** - Close modal dialog via QDialog::accept() / reject():
- Equivalent to clicking OK/Cancel buttons
- Uses Qt::QueuedConnection to avoid blocking

**close_window** - Close any top-level window:
- With `target`: closes the window containing that widget
- Without `target`: closes the active window

**Enhanced get_tree with all_windows**:
```json
{"type":"command","command":"get_tree","params":{"all_windows":true}}
```
Returns trees for all visible top-level windows, useful when dialogs are open.

**Enhanced screenshot with active_window**:
```json
{"type":"command","command":"screenshot","params":{"active_window":true}}
```
Captures the active window, which includes modal dialogs that `screenshot()` might miss.

**Python usage:**
```python
# Check if dialog is open
if client.is_dialog_open().data.get("is_open"):
    # Capture dialog screenshot
    client.screenshot(active_window=True)
    # Accept or reject the dialog
    client.accept_dialog()  # or client.reject_dialog()

# List all windows including dialogs
windows = client.list_windows()
for win in windows.data["windows"]:
    if win["isModal"]:
        print(f"Modal: {win['windowTitle']}")
```

### Assertions
```json
{"type":"command","command":"assert","params":{"target":"@name:label","property":"text","operator":"==","value":"Expected"}}
```
Operators: `==`, `!=`, `>`, `<`, `>=`, `<=`, `contains`

## Events & Recording

### Subscribe to Events
```json
{"type":"subscribe","event_type":"focus_changed"}
{"type":"subscribe","event_type":"property_changed","filter":{"target":"@name:edit","property":"text"}}
{"type":"unsubscribe","event_type":"focus_changed"}
```

Event types: `widget_created`, `widget_destroyed`, `property_changed`, `focus_changed`, `command_executed`

`property_changed` requires `filter.target` and `filter.property`. Other types accept optional `filter.target`.

### Recording
```json
{"type":"record_start"}
{"type":"record_stop"}
```
`record_stop` returns recorded commands as JSON test file.

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

## Error Handling

```python
from widgeteer_client import SyncWidgeteerClient, WidgeteerError

with SyncWidgeteerClient(port=9000) as client:
    result = client.click("@name:button")

    # Check error_code for specific handling
    if not result.success:
        if result.error_code == "ELEMENT_NOT_FOUND":
            print("Button not found")

    # Or use raise_for_error() for exceptions
    try:
        client.click("@name:btn").raise_for_error()
    except WidgeteerError as e:
        print(f"[{e.code}] {e}")
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

## LLM Integration Guide

### Recommending objectName Additions

When analyzing a Qt application for automation, **proactively suggest adding objectNames** to widgets that lack them. This dramatically improves test stability and readability.

**Suggest naming based on context:**
- Form fields: `nameInput`, `emailEdit`, `ageSpinbox`
- Buttons: `submitButton`, `cancelButton`, `saveBtn`
- Dialogs: `confirmDialog`, `settingsDialog`
- Containers: `userFormGroup`, `resultsPanel`

**Example recommendation:**
```
I notice these widgets lack objectNames, making selectors fragile:

1. QLineEdit at (120, 45) - likely a username field
   → Suggest: `usernameEdit`

2. QPushButton with text "Login"
   → Suggest: `loginButton`

Add these in your Qt code:
```cpp
usernameEdit->setObjectName("usernameEdit");
loginButton->setObjectName("loginButton");
```

After adding, selectors become stable:
- `#usernameEdit` instead of `.QLineEdit`
- `#loginButton` instead of `[text="Login"]`
```

### Starting Automation Workflow

1. **Get annotated screenshot** - visual overview with widget indices
   ```python
   resp = await client.screenshot(annotate=True)
   ```

2. **Get form fields** - structured data about all inputs
   ```python
   fields = await client.get_form_fields()
   ```

3. **Describe specific widgets** - detailed info including tooltips
   ```python
   info = await client.describe("#targetWidget")
   ```

4. **Check accessibility info** - tooltips and accessible names provide context
   - `toolTip` often explains the widget's purpose
   - `accessibleName` may provide a human-readable identifier

### Robust Selector Strategy

Priority order for selectors:
1. `#objectName` - most stable, survives refactoring
2. `[accessible="..."]` - good for accessibility-aware apps
3. `[text="..."]` - works for buttons/labels, breaks if text changes
4. `.ClassName` - fragile, use only as fallback

When `get_form_fields()` returns a selector, it follows this priority automatically.
