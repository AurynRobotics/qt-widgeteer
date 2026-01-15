# Widgeteer User Manual

A comprehensive guide to using Widgeteer for Qt6 UI testing and automation.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Integration Guide](#integration-guide)
3. [Element Selection](#element-selection)
4. [Common Operations](#common-operations)
5. [Testing Patterns](#testing-patterns)
6. [Synchronization](#synchronization)
7. [Transactions](#transactions)
8. [Extensibility](#extensibility)
9. [Debugging Tips](#debugging-tips)
10. [Best Practices](#best-practices)

---

## Getting Started

### Prerequisites

- Qt6 (6.2 or later)
- C++17 compatible compiler
- CMake 3.18+
- Python 3.8+ (for test client)

### Building Widgeteer

```bash
git clone https://github.com/user/widgeteer.git
cd widgeteer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### Running the Sample Application

```bash
./build/sample/widgeteer_sample 9000
```

This starts a sample Qt application with Widgeteer enabled on port 9000.

### Verifying the Connection

```bash
# Using wscat
wscat -c ws://localhost:9000

# Then send a command
> {"type":"command","id":"1","command":"get_tree","params":{"depth":1}}
```

Expected response:
```json
{"type":"response","id":"1","success":true,"result":{"widgets":[...]}}
```

Or using Python:
```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    tree = client.tree(depth=1)
    print("Connected!" if tree.success else "Connection failed")
```

---

## Integration Guide

### Minimal Integration

Add just 2-3 lines to enable Widgeteer in your Qt application:

```cpp
#include <widgeteer/Server.h>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Create and start Widgeteer server
    widgeteer::Server server;
    server.start(9000);  // Port is configurable

    // Your normal application code
    MainWindow window;
    window.show();

    return app.exec();
}
```

### CMake Configuration

**Option 1: Installed library**
```cmake
find_package(widgeteer REQUIRED)
target_link_libraries(your_app PRIVATE widgeteer::widgeteer)
```

**Option 2: Subdirectory**
```cmake
add_subdirectory(vendor/widgeteer)
target_link_libraries(your_app PRIVATE widgeteer)
```

### Server Configuration Options

```cpp
widgeteer::Server server;

// Change port (default: 9000)
server.setPort(9001);

// Enable detailed request logging
server.enableLogging(true);

// Enable CORS headers (default: true)
server.enableCORS(true);

// Restrict access to specific hosts
server.setAllowedHosts({"localhost", "127.0.0.1"});

// Limit introspection to specific widget subtree
server.setRootWidget(&mainWindow);

// Start the server
if (!server.start()) {
    qWarning() << "Failed to start Widgeteer server";
}
```

### Conditional Compilation

For production builds, you may want to exclude Widgeteer:

```cpp
#ifdef WIDGETEER_ENABLED
#include <widgeteer/Server.h>
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

#ifdef WIDGETEER_ENABLED
    widgeteer::Server server;
    server.start(9000);
#endif

    MainWindow window;
    window.show();
    return app.exec();
}
```

And in CMake:
```cmake
option(ENABLE_WIDGETEER "Enable Widgeteer testing server" ON)
if(ENABLE_WIDGETEER)
    target_compile_definitions(your_app PRIVATE WIDGETEER_ENABLED)
    target_link_libraries(your_app PRIVATE widgeteer)
endif()
```

---

## Element Selection

### Setting Up objectNames

The most reliable way to identify widgets is by their `objectName`. Set these in your code:

```cpp
// In constructor or setup code
submitButton->setObjectName("submitButton");
nameEdit->setObjectName("nameEdit");
tabWidget->setObjectName("mainTabs");
```

Or in Qt Designer, set the `objectName` property for each widget.

### Selector Types

#### By objectName (Recommended)
```
@name:submitButton
@name:userNameEdit
@name:mainWindow
```

Best for: Reliable, stable element identification.

#### By Class
```
@class:QPushButton
@class:QLineEdit
@class:QComboBox
```

Best for: Finding the first widget of a type, generic operations.

#### By Text Content
```
@text:Submit
@text:Cancel
@text:Open File...
```

Best for: Buttons and labels with stable text.

#### By Accessible Name
```
@accessible:Submit Form Button
@accessible:User Name Input Field
```

Best for: Applications with proper accessibility support.

#### By Path
```
mainWindow/centralWidget/formTab/submitButton
mainWindow/tabWidget/controlsTab/slider
```

Best for: When objectNames aren't unique or available.

### Finding Elements

Use the `find` command to discover elements:

```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    result = client.find("@class:QPushButton")
    for match in result.data.get("matches", []):
        print(f"{match['objectName']}: {match['path']}")
```

Or via WebSocket:
```json
{"type":"command","id":"1","command":"find","params":{"query":"@class:QPushButton"}}
```

Response:
```json
{
  "type": "response",
  "id": "1",
  "success": true,
  "result": {
    "matches": [
      {"path": "mainWindow/submitButton", "class": "QPushButton", "objectName": "submitButton"},
      {"path": "mainWindow/clearButton", "class": "QPushButton", "objectName": "clearButton"}
    ],
    "count": 2
  }
}
```

### Exploring the Widget Tree

```python
result = client.tree(depth=3)
```

Or via WebSocket:
```json
{"type":"command","id":"1","command":"get_tree","params":{"depth":3}}
```

This returns the complete widget hierarchy, helping you understand what selectors to use.

---

## Common Operations

### Clicking Buttons

```python
from widgeteer_client import WidgeteerClient
client = WidgeteerClient(port=9000)

# Simple click
client.click("@name:submitButton")

# Right-click for context menu
client.right_click("@name:treeWidget")

# Double-click
client.double_click("@name:listItem")

# Click at specific position
client.click("@name:canvas", pos={"x": 100, "y": 50})
```

### Text Input

```python
# Type text (appends to existing)
client.type_text("@name:nameEdit", "John Doe")

# Clear first, then type
client.type_text("@name:nameEdit", "John Doe", clear_first=True)

# Type into currently focused widget
client.key("@name:nameEdit", "Enter")  # Press Enter
```

### Working with Form Controls

```python
# Combo box - by index
client.set_value("@name:roleComboBox", 2)

# Combo box - by text
client.set_value("@name:countryComboBox", "Germany")

# Spin box
client.set_value("@name:quantitySpinBox", 42)

# Slider
client.set_value("@name:volumeSlider", 75)

# Checkbox
client.set_value("@name:rememberMeCheckbox", True)
client.click("@name:rememberMeCheckbox")  # Toggle

# Tab widget
client.set_value("@name:mainTabWidget", 1)  # Switch to second tab
```

### Reading Values

```python
# Get a property
result = client.get_property("@name:nameEdit", "text")
print(result.data["result"]["value"])  # "John Doe"

# Check existence
result = client.exists("@name:submitButton")
print(result.data["result"]["exists"])  # True

# Check visibility
result = client.is_visible("@name:loadingSpinner")
print(result.data["result"]["visible"])  # False
```

### Assertions

```python
# Verify property value
result = client.assert_property("@name:nameEdit", "text", "==", "John Doe")
if result.data["result"]["passed"]:
    print("Assertion passed!")
else:
    print(f"Expected: John Doe, Got: {result.data['result']['actual']}")

# Various operators
client.assert_property("@name:progressBar", "value", ">=", 50)
client.assert_property("@name:titleLabel", "text", "contains", "Welcome")
client.assert_property("@name:counter", "value", "!=", 0)
```

### Screenshots

```python
# Capture entire screen
result = client.screenshot()

# Capture specific widget
result = client.screenshot(target="@name:chartWidget")

# Save to file
import base64
image_data = base64.b64decode(result.data["result"]["screenshot"])
with open("screenshot.png", "wb") as f:
    f.write(image_data)
```

### Handling Dialogs (Error/Warning Messages)

Qt applications often show dialog boxes for errors, warnings, or confirmations. Here's how to detect and handle them.

#### Blocking vs Non-Blocking Dialogs

Widgeteer supports both types of dialogs:

- **Blocking dialogs** (`dialog.exec()`): The application code waits for the dialog to close before continuing. This is common with `QMessageBox::question()`, `QMessageBox::warning()`, etc.
- **Non-blocking dialogs** (`dialog.show()`): The application shows the dialog and continues executing. The dialog result is typically handled via signals.

**Important**: When working with blocking dialogs, use `sleep` instead of `wait` commands. The `wait` command uses Qt's `processEvents()` internally, which can block when nested inside a dialog's `exec()` call.

```python
# For blocking dialogs, use sleep to wait for the dialog to appear
client.click("@name:showDialogButton")
client.command("sleep", {"ms": 100})  # Give dialog time to appear
client.is_visible("@name:confirmDialog")
client.key("@name:confirmDialog", "Escape")
```

#### Detecting Dialogs

```python
# Find any QMessageBox dialogs (standard error/warning/info)
result = client.find("@class:QMessageBox")
if result.success and result.data.get("count", 0) > 0:
    print("Message box is active!")
    for match in result.data["matches"]:
        print(f"  Found: {match['path']}")

# Find any QDialog (custom dialogs)
result = client.find("@class:QDialog")

# Check for specific dialog by objectName
result = client.exists("@name:errorDialog")
if result.data.get("exists"):
    print("Error dialog is showing")
```

#### Getting Dialog Content

```python
# Describe the message box to see its content
result = client.describe("@class:QMessageBox")
if result.success:
    props = result.data.get("properties", {})
    print(f"Title: {props.get('windowTitle')}")
    print(f"Text: {props.get('text')}")
    print(f"Informative text: {props.get('informativeText')}")
```

#### Closing Dialogs

**Click a button in the dialog:**
```python
# Click OK button
client.click("@class:QMessageBox/@text:OK")

# Click Cancel button
client.click("@class:QMessageBox/@text:Cancel")

# Click any button (first one found)
client.click("@class:QMessageBox/@class:QPushButton")
```

**Press Escape key:**
```python
# Press Escape to close/cancel the dialog
client.key("@class:QMessageBox", "Escape")
```

**Invoke close method directly:**
```python
# Call close() on the dialog
client.invoke("@class:QDialog", "close")

# Or use accept/reject for QDialog
client.invoke("@class:QDialog", "accept")  # Like clicking OK
client.invoke("@class:QDialog", "reject")  # Like clicking Cancel
```

#### Wait for Dialog to Appear

```python
# Wait for a dialog before interacting
client.wait("@class:QMessageBox", condition="exists", timeout_ms=5000)
client.click("@class:QMessageBox/@text:OK")
```

#### Helper Function to Close Any Dialog

```python
def close_any_dialog(client):
    """Close any active dialog/message box. Returns True if dialog was found."""
    # Check for QMessageBox first (most common for errors/warnings)
    result = client.find("@class:QMessageBox")
    if result.success and result.data.get("count", 0) > 0:
        # Try clicking common buttons
        for btn_text in ["OK", "Close", "Yes", "Cancel"]:
            btn_result = client.exists(f"@class:QMessageBox/@text:{btn_text}")
            if btn_result.success and btn_result.data.get("exists"):
                client.click(f"@class:QMessageBox/@text:{btn_text}")
                return True
        # Fallback: press Escape
        client.key("@class:QMessageBox", "Escape")
        return True

    # Check for generic QDialog
    result = client.find("@class:QDialog")
    if result.success and result.data.get("count", 0) > 0:
        client.key("@class:QDialog", "Escape")
        return True

    return False  # No dialog found
```

#### Common Dialog Classes

| Class | Description |
|-------|-------------|
| `QMessageBox` | Standard message dialogs (info, warning, error, question) |
| `QDialog` | Base class for custom dialogs |
| `QErrorMessage` | Error message dialogs with "don't show again" option |
| `QProgressDialog` | Progress dialogs with cancel button |
| `QInputDialog` | Simple input dialogs (text, number, item selection) |
| `QFileDialog` | File open/save dialogs |
| `QColorDialog` | Color picker dialogs |
| `QFontDialog` | Font selection dialogs |

---

## Testing Patterns

### JSON Test Files

Widgeteer supports JSON-based test definitions:

```json
{
  "name": "Login Flow Test",
  "description": "Tests the complete login flow",
  "setup": [
    {"command": "wait", "params": {"target": "@name:mainWindow", "condition": "visible"}}
  ],
  "tests": [
    {
      "name": "Enter credentials and submit",
      "steps": [
        {"command": "type", "params": {"target": "@name:usernameEdit", "text": "testuser", "clear_first": true}},
        {"command": "type", "params": {"target": "@name:passwordEdit", "text": "password123", "clear_first": true}},
        {"command": "click", "params": {"target": "@name:loginButton"}},
        {"command": "wait_idle", "params": {"timeout_ms": 2000}}
      ],
      "assertions": [
        {"target": "@name:welcomeLabel", "property": "visible", "operator": "==", "value": "true"},
        {"target": "@name:welcomeLabel", "property": "text", "operator": "contains", "value": "Welcome"}
      ]
    }
  ],
  "teardown": [
    {"command": "click", "params": {"target": "@name:logoutButton"}}
  ]
}
```

### Running Tests

```bash
# Run against running application
python test_executor.py login_tests.json --port 9000

# Launch application and run tests
python test_executor.py login_tests.json --app ./my_app --port 9000

# Verbose output
python test_executor.py login_tests.json --port 9000 --verbose

# List tests without running
python test_executor.py login_tests.json --list-tests
```

### Page Object Pattern

For complex applications, organize selectors into reusable components:

```python
class LoginPage:
    USERNAME_FIELD = "@name:usernameEdit"
    PASSWORD_FIELD = "@name:passwordEdit"
    LOGIN_BUTTON = "@name:loginButton"
    ERROR_LABEL = "@name:errorLabel"

    def __init__(self, client):
        self.client = client

    def login(self, username, password):
        self.client.type_text(self.USERNAME_FIELD, username, clear_first=True)
        self.client.type_text(self.PASSWORD_FIELD, password, clear_first=True)
        self.client.click(self.LOGIN_BUTTON)
        self.client.wait_idle(timeout_ms=2000)

    def get_error_message(self):
        result = self.client.get_property(self.ERROR_LABEL, "text")
        return result.data["result"]["value"]

class MainPage:
    WELCOME_LABEL = "@name:welcomeLabel"
    LOGOUT_BUTTON = "@name:logoutButton"

    def __init__(self, client):
        self.client = client

    def is_logged_in(self):
        result = self.client.is_visible(self.WELCOME_LABEL)
        return result.data["result"]["visible"]

    def logout(self):
        self.client.click(self.LOGOUT_BUTTON)
        self.client.wait_idle()

# Usage
client = WidgeteerClient(port=9000)
login_page = LoginPage(client)
main_page = MainPage(client)

login_page.login("testuser", "password123")
assert main_page.is_logged_in()
main_page.logout()
```

---

## Synchronization

### Wait for Condition

```python
# Wait for element to exist
client.wait("@name:loadingComplete", condition="exists", timeout_ms=10000)

# Wait for element to become visible
client.wait("@name:resultsPanel", condition="visible", timeout_ms=5000)

# Wait for element to be enabled
client.wait("@name:submitButton", condition="enabled", timeout_ms=3000)

# Wait for element to disappear
client.wait("@name:loadingSpinner", condition="!visible", timeout_ms=10000)
```

### Wait for Idle

After actions that trigger async operations:

```python
client.click("@name:loadDataButton")
client.wait_idle(timeout_ms=5000)  # Wait for all events to process
# Now check results
```

### Wait for Signal

```python
# Wait for specific signal
client.command("wait_signal", {
    "target": "@name:dataLoader",
    "signal": "dataLoaded()",
    "timeout_ms": 10000
})
```

### Avoid Hard Delays

```python
# BAD - Don't use sleep unless absolutely necessary
client.sleep(2000)

# GOOD - Wait for specific condition
client.wait("@name:resultsTable", condition="visible", timeout_ms=5000)

# GOOD - Wait for UI to stabilize
client.wait_idle(timeout_ms=2000)
```

---

## Transactions

Execute multiple commands atomically with automatic rollback:

```python
# Using the client
steps = [
    {"id": "1", "command": "type", "params": {"target": "@name:field1", "text": "value1", "clear_first": True}},
    {"id": "2", "command": "type", "params": {"target": "@name:field2", "text": "value2", "clear_first": True}},
    {"id": "3", "command": "click", "params": {"target": "@name:submitButton"}}
]

result = client.transaction(steps, rollback_on_failure=True)

if result.success:
    print(f"All {result.data['completed_steps']} steps completed")
else:
    print(f"Failed at step {result.data['completed_steps']}")
    if result.data['rollback_performed']:
        print("Changes were rolled back")
```

### When to Use Transactions

- Form submissions where all fields must be filled
- Multi-step wizards
- Operations that should be atomic
- When you need automatic cleanup on failure

---

## Extensibility

Widgeteer provides mechanisms to extend functionality beyond UI interactions.

### Custom Command Handlers

Register application-specific commands that can be invoked via the API:

```cpp
#include <widgeteer/Server.h>

class MyApplication : public QObject {
    Q_OBJECT
public:
    bool loadProject(const QString& path);
    bool saveProject();
    QVariantMap getStatistics();
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MyApplication myApp;

    widgeteer::Server server;

    // Register custom commands
    server.registerCommand("loadProject", [&myApp](const QJsonObject& params) {
        QString path = params["path"].toString();
        bool success = myApp.loadProject(path);
        return QJsonObject{
            {"success", success},
            {"path", path}
        };
    });

    server.registerCommand("saveProject", [&myApp](const QJsonObject& params) {
        bool success = myApp.saveProject();
        return QJsonObject{{"saved", success}};
    });

    server.registerCommand("getAppState", [&myApp](const QJsonObject& params) {
        auto stats = myApp.getStatistics();
        return QJsonObject{
            {"projectLoaded", stats["projectLoaded"].toBool()},
            {"itemCount", stats["itemCount"].toInt()},
            {"modified", stats["modified"].toBool()}
        };
    });

    server.start(9000);

    MainWindow window;
    window.show();
    return app.exec();
}
```

Invoke custom commands:

```python
# Load a project
result = client.command("loadProject", {"path": "/home/user/project.proj"})

# Get application state
result = client.command("getAppState", {})
print(f"Items: {result.data['result']['itemCount']}")

# Save
result = client.command("saveProject", {})
```

### Enhanced Q_INVOKABLE Methods

Mark methods as Q_INVOKABLE for direct invocation:

```cpp
class DataManager : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE bool importData(const QString& source);
    Q_INVOKABLE bool exportData(const QString& destination);
    Q_INVOKABLE int getRecordCount();
    Q_INVOKABLE void clearCache();
};
```

Then invoke via the API:

```python
# Call Q_INVOKABLE method
result = client.invoke("@name:dataManager", "clearCache")
```

### Event Hooks

Register callbacks for specific events:

```cpp
server.onBeforeCommand([](const QString& command, const QJsonObject& params) {
    qDebug() << "About to execute:" << command;
    return true;  // Allow execution (return false to block)
});

server.onAfterCommand([](const QString& command, const QJsonObject& result, bool success) {
    if (!success) {
        qWarning() << "Command failed:" << command;
    }
});
```

### Use Cases for Custom Commands

1. **Data Operations**: Load/save files, import/export data
2. **Backend Calls**: Trigger API requests, database operations
3. **State Management**: Get/set application state not visible in UI
4. **Testing Utilities**: Reset state, inject test data, mock services
5. **Performance**: Bypass UI for bulk operations

---

## Debugging Tips

### Enable Server Logging

```cpp
server.enableLogging(true);
```

This prints all incoming requests and responses to the console.

### Explore the Widget Tree

```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(port=9000) as client:
    # Get full tree
    tree = client.tree()

    # Limited depth
    tree = client.tree(depth=2)

    # Include invisible widgets
    tree = client.tree(include_invisible=True)
```

### Describe Specific Widget

```python
result = client.describe("@name:submitButton")
print(f"Class: {result.data['className']}")
print(f"Properties: {result.data['properties']}")
```

### List Available Properties

```python
result = client.command("list_properties", {"target": "@name:myWidget"})
for prop in result.data.get("properties", []):
    print(f"  {prop['name']}: {prop['type']}")
```

### Take Screenshots for Visual Debugging

```python
# Capture state at each step
client.click("@name:openButton")
result = client.screenshot()
save_screenshot(result, "step1_after_open.png")

client.type_text("@name:searchEdit", "test query")
result = client.screenshot()
save_screenshot(result, "step2_after_search.png")
```

### Common Issues

**Element Not Found**
- Check objectName is set correctly
- Verify widget is created at the time of query
- Try `@class:` selector to see if widget exists
- Use `tree` endpoint to explore hierarchy

**Element Not Visible**
- Widget might be in a different tab
- Parent widget might be hidden
- Widget might be off-screen
- Check `is_visible` before interacting

**Actions Not Working**
- Use `wait_idle` after actions
- Check if widget is enabled
- Verify widget has focus for keyboard input
- Try `focus` command before typing

---

## Best Practices

### 1. Use Stable Selectors

```cpp
// Good - explicit objectNames
button->setObjectName("submitFormButton");

// Avoid - relying on text that might change
// @text:Submit  (might be localized)
```

### 2. Wait, Don't Sleep

```python
# Good
client.wait("@name:results", condition="visible", timeout_ms=5000)

# Avoid
client.sleep(5000)
```

### 3. Use Transactions for Multi-Step Operations

```python
# Good - atomic operation with rollback
client.transaction([
    {"command": "type", "params": {"target": "@name:field1", "text": "value1"}},
    {"command": "type", "params": {"target": "@name:field2", "text": "value2"}},
    {"command": "click", "params": {"target": "@name:submit"}}
])

# Avoid - partial state if something fails
client.type_text("@name:field1", "value1")
client.type_text("@name:field2", "value2")
client.click("@name:submit")
```

### 4. Organize Tests with Page Objects

Keep selectors organized and reusable (see [Page Object Pattern](#page-object-pattern)).

### 5. Handle Errors Gracefully

```python
result = client.click("@name:optionalButton")
if not result.success:
    if result.error == "ELEMENT_NOT_FOUND":
        print("Button not present, skipping...")
    else:
        raise Exception(f"Unexpected error: {result.error}")
```

### 6. Clean Up After Tests

```json
{
  "teardown": [
    {"command": "click", "params": {"target": "@name:resetButton"}},
    {"command": "wait_idle", "params": {"timeout_ms": 1000}}
  ]
}
```

### 7. Use Meaningful Command IDs

```python
# Good - traceable
client.command("click", {"target": "@name:submit"}, cmd_id="login_test_submit_click")

# Avoid
client.command("click", {"target": "@name:submit"}, cmd_id="1")
```

### 8. Secure Production Builds

```cmake
# Disable Widgeteer in production
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ENABLE_WIDGETEER OFF)
endif()
```

Or restrict access:

```cpp
server.setAllowedHosts({"localhost", "127.0.0.1"});
```

---

## Appendix: Python Client Reference

### Async Client (WidgeteerClient)

```python
from widgeteer_client import WidgeteerClient
import asyncio

async def main():
    async with WidgeteerClient(host="localhost", port=9000, token="optional") as client:
        result = await client.click("@name:button")

asyncio.run(main())
```

### Sync Client (SyncWidgeteerClient)

```python
from widgeteer_client import SyncWidgeteerClient

with SyncWidgeteerClient(host="localhost", port=9000) as client:
    result = client.click("@name:button")
```

### Methods

| Method | Description |
|--------|-------------|
| `connect()` | Connect to server |
| `disconnect()` | Disconnect from server |
| `tree(root, depth, include_invisible)` | Get widget tree |
| `screenshot(target, format)` | Capture screenshot |
| `command(cmd, params, options, cmd_id)` | Execute command |
| `click(target, button, pos)` | Click widget |
| `double_click(target, pos)` | Double-click |
| `right_click(target, pos)` | Right-click |
| `type_text(target, text, clear_first)` | Type text |
| `key(target, key, modifiers)` | Press key |
| `key_sequence(target, sequence)` | Press key combo |
| `scroll(target, delta_x, delta_y)` | Scroll widget |
| `hover(target, pos)` | Hover over widget |
| `focus(target)` | Set focus |
| `get_property(target, property)` | Get property value |
| `set_property(target, property, value)` | Set property |
| `set_value(target, value)` | Smart value setter |
| `invoke(target, method)` | Invoke method |
| `exists(target)` | Check existence |
| `is_visible(target)` | Check visibility |
| `describe(target)` | Get widget details |
| `find(query, max_results, visible_only)` | Find widgets |
| `wait(target, condition, timeout_ms)` | Wait for condition |
| `wait_idle(timeout_ms)` | Wait for idle |
| `sleep(ms)` | Hard delay |
| `assert_property(target, property, operator, value)` | Assert condition |
| `start_recording()` | Start recording commands |
| `stop_recording()` | Stop recording, get results |
| `subscribe(event_type, handler)` | Subscribe to events (async only) |
| `unsubscribe(event_type)` | Unsubscribe from events |
