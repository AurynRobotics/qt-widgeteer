# WidgeteerBot Implementation Plan

## Summary

Add a `WidgeteerBot` class providing a fluent C++ API for testing Qt applications with any test framework (QTest, GTest, Catch2). Includes a `Result<T, E>` template for clean error handling.

## Files to Create

| File | Purpose |
|------|---------|
| `include/widgeteer/Result.h` | Result<T, E> template with void specialization |
| `include/widgeteer/WidgeteerBot.h` | Bot class with Doxygen docs |
| `src/WidgeteerBot.cpp` | Implementation (~200 lines) |
| `tests/cpp/test_widgeteer_bot.cpp` | QTest unit tests |

## Files to Modify

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `src/WidgeteerBot.cpp` to WIDGETEER_SOURCES list (line ~69) |
| `tests/cpp/CMakeLists.txt` | Add `add_widgeteer_test(test_widgeteer_bot)` at end |

## Design Decisions

1. **Constructor overloads**: Own executor OR accept external
2. **Return type**: `Result<T, E = ErrorDetails>` template
3. **Error type default**: `ErrorDetails` from Protocol.h (no need to specify)
4. **API coverage**: Full parity with CommandExecutor (25+ commands)
5. **Introspection**: Returns `Result<QJsonObject>` (no typed structs)
6. **Test framework**: Works with any (QTest, GTest, Catch2)

---

## Implementation Details

### Step 1: Result<T, E> Template

Create `include/widgeteer/Result.h`:

```cpp
#pragma once

#include <widgeteer/Export.h>
#include <widgeteer/Protocol.h>  // For ErrorDetails

#include <variant>

namespace widgeteer {

template <typename T, typename E = ErrorDetails>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result fail(E error) { return Result(std::move(error), false); }

    bool success() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return success(); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }
    T valueOr(T defaultValue) const { return success() ? value() : defaultValue; }

    const E& error() const { return std::get<E>(data_); }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    Result(E error, bool) : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

// Void specialization
template <typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true); }
    static Result fail(E error) { return Result(std::move(error)); }

    bool success() const { return success_; }
    explicit operator bool() const { return success_; }

    const E& error() const { return error_; }

private:
    explicit Result(bool) : success_(true) {}
    explicit Result(E error) : success_(false), error_(std::move(error)) {}

    bool success_;
    E error_;
};

}  // namespace widgeteer
```

### Step 2: WidgeteerBot Header

Create `include/widgeteer/WidgeteerBot.h`:

**Key API methods to include:**

```cpp
// Actions
Result<void> click(const QString& target);
Result<void> doubleClick(const QString& target);
Result<void> rightClick(const QString& target);
Result<void> type(const QString& target, const QString& text, bool clearFirst = false);
Result<void> key(const QString& target, const QString& key, const QStringList& modifiers = {});
Result<void> keySequence(const QString& target, const QString& sequence);
Result<void> drag(const QString& from, const QString& to);
Result<void> scroll(const QString& target, int deltaX, int deltaY);
Result<void> hover(const QString& target);
Result<void> focus(const QString& target);

// State
Result<void> setValue(const QString& target, const QJsonValue& value);
Result<void> setProperty(const QString& target, const QString& property, const QJsonValue& value);
Result<QJsonValue> getProperty(const QString& target, const QString& property);
Result<void> invoke(const QString& target, const QString& method);

// Query
Result<bool> exists(const QString& target);
Result<bool> isVisible(const QString& target);
Result<QString> getText(const QString& target);
Result<QJsonArray> listProperties(const QString& target);

// Introspection
Result<QJsonObject> getTree(int depth = -1);
Result<QJsonObject> find(const QString& query, int maxResults = 100);
Result<QJsonObject> describe(const QString& target);
Result<QJsonObject> getActions(const QString& target);
Result<QJsonObject> getFormFields(const QString& root = QString());

// Sync
Result<void> waitFor(const QString& target, const QString& condition, int timeoutMs = 5000);
Result<void> waitIdle(int timeoutMs = 5000);
Result<void> waitSignal(const QString& target, const QString& signal, int timeoutMs = 5000);
Result<void> sleep(int ms);

// Screenshots
Result<QJsonObject> screenshot(const QString& target = QString());
Result<QJsonObject> screenshotAnnotated(const QString& target = QString());

// Extensibility
Result<QJsonObject> call(const QString& object, const QString& method, const QJsonArray& args = {});
Result<QJsonObject> listObjects();
Result<QJsonObject> listCustomCommands();

// Control
Result<void> quit();
```

**Constructor patterns:**

```cpp
// Owns its own CommandExecutor
explicit WidgeteerBot(QObject* parent = nullptr);

// Uses external CommandExecutor (non-owning)
explicit WidgeteerBot(CommandExecutor* executor, QObject* parent = nullptr);
```

### Step 3: WidgeteerBot Implementation

Create `src/WidgeteerBot.cpp`:

**Pattern for each method:**

```cpp
Result<void> WidgeteerBot::click(const QString& target) {
    QJsonObject params;
    params["target"] = target;
    return execute("click", params);
}

Result<QJsonValue> WidgeteerBot::getProperty(const QString& target, const QString& property) {
    QJsonObject params;
    params["target"] = target;
    params["property"] = property;
    return executeWithResult<QJsonValue>("get_property", params, "value");
}
```

**Private helper:**

```cpp
Result<void> WidgeteerBot::execute(const QString& command, const QJsonObject& params) {
    Command cmd;
    cmd.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cmd.name = command;
    cmd.params = params;

    Response response = executor_->execute(cmd);

    if (response.success) {
        return Result<void>::ok();
    }
    return Result<void>::fail(response.error);
}
```

### Step 4: CMake Integration

**In `CMakeLists.txt` (around line 69):**
```cmake
set(WIDGETEER_SOURCES
  src/Protocol.cpp
  src/ElementFinder.cpp
  src/UIIntrospector.cpp
  src/EventInjector.cpp
  src/Synchronizer.cpp
  src/CommandExecutor.cpp
  src/ActionRecorder.cpp
  src/EventBroadcaster.cpp
  src/Server.cpp
  src/WidgeteerBot.cpp  # ADD THIS
)
```

**In `tests/cpp/CMakeLists.txt` (at end):**
```cmake
add_widgeteer_test(test_widgeteer_bot)
```

### Step 5: Unit Tests

Create `tests/cpp/test_widgeteer_bot.cpp`:

**Test fixture pattern (from test_element_finder.cpp):**

```cpp
class TestWidgeteerBot : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Create test widget hierarchy
    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");
    // ... add button, input, label
    testWindow_->show();
    QTest::qWait(100);
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  // Test methods
  void testClick();
  void testType();
  void testGetText();
  void testExists();
  void testIsVisible();
  void testWaitFor();
  void testGetProperty();
  void testSetProperty();
  void testGetTree();
  void testFind();
  void testErrorHandling();

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button_ = nullptr;
  QLineEdit* input_ = nullptr;
  QLabel* label_ = nullptr;
};

QTEST_MAIN(TestWidgeteerBot)
#include "test_widgeteer_bot.moc"
```

---

## Existing Codebase Patterns

### Export Macro
All public classes use `WIDGETEER_EXPORT`:
```cpp
class WIDGETEER_EXPORT WidgeteerBot : public QObject {
```

### ErrorDetails Structure (from Protocol.h)
```cpp
struct WIDGETEER_EXPORT ErrorDetails {
  QString code;
  QString message;
  QJsonObject details;
  QJsonObject toJson() const;
};
```

### Command Execution Pattern (from CommandExecutor)
```cpp
Response CommandExecutor::execute(const Command& cmd) {
  // ... timing, dispatch, response building
  if (result.contains("error")) {
    response.success = false;
    response.error.code = result.value("error").toObject().value("code").toString();
    response.error.message = result.value("error").toObject().value("message").toString();
  } else {
    response.success = true;
    response.result = result;
  }
  return response;
}
```

### Test Pattern (from test_element_finder.cpp)
- Use `initTestCase()` and `cleanupTestCase()` for setup/teardown
- Use `QTest::qWait(100)` after showing widgets
- Use `QCOMPARE`, `QVERIFY` for assertions
- Include `.moc` file at end

### Code Style
- C++17
- Namespace: `widgeteer::`
- Google style formatting (clang-format)
- 100 char line width
- Braces on all control statements

---

## Verification Commands

```bash
# 1. Run pre-commit hooks (formatting, linting)
pre-commit run --all-files

# 2. Build
cmake --build build --parallel

# 3. Run all tests
ctest --test-dir build --output-on-failure

# 4. Run specific test
./build/tests/cpp/test_widgeteer_bot
```

---

## Usage Example

```cpp
#include <widgeteer/WidgeteerBot.h>
#include <QtTest>

void MyTest::testLogin() {
    widgeteer::WidgeteerBot bot;

    QVERIFY(bot.type("@name:username", "admin"));
    QVERIFY(bot.type("@name:password", "secret"));
    QVERIFY(bot.click("@name:loginButton"));
    QVERIFY(bot.waitFor("@name:dashboard", "visible"));

    auto welcome = bot.getText("@name:welcomeLabel");
    QVERIFY(welcome);
    QCOMPARE(welcome.value(), QString("Welcome, admin!"));
}
```

---

## Notes

- The `Result<T, E>` template provides clean error handling without exceptions
- `operator bool()` allows natural `if (result)` checks
- Void specialization uses a bool flag instead of variant
- All methods delegate to CommandExecutor - no duplicated logic
- getText() extracts "text" property, works for QLabel, QLineEdit, QAbstractButton
