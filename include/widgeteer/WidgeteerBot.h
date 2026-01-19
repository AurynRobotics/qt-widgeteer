#pragma once

#include <widgeteer/CommandExecutor.h>
#include <widgeteer/Export.h>
#include <widgeteer/Result.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace widgeteer {

/**
 * @brief Fluent C++ API for testing Qt applications.
 *
 * WidgeteerBot provides a high-level, test-framework-agnostic API for
 * automating Qt widget interactions. It wraps CommandExecutor with a
 * Result-based error handling approach that works with QTest, GTest,
 * Catch2, or any other test framework.
 *
 * Example usage with QTest:
 * @code
 * void MyTest::testLogin() {
 *     widgeteer::WidgeteerBot bot;
 *
 *     QVERIFY(bot.type("@name:username", "admin"));
 *     QVERIFY(bot.type("@name:password", "secret"));
 *     QVERIFY(bot.click("@name:loginButton"));
 *     QVERIFY(bot.waitFor("@name:dashboard", "visible"));
 *
 *     auto welcome = bot.getText("@name:welcomeLabel");
 *     QVERIFY(welcome);
 *     QCOMPARE(welcome.value(), QString("Welcome, admin!"));
 * }
 * @endcode
 *
 * Element selectors:
 * - @c \@name:objectName - Find by QObject::objectName()
 * - @c \@class:ClassName - Find by class name (e.g., QPushButton)
 * - @c \@text:ButtonText - Find by visible text
 * - @c \@accessible:Name - Find by accessible name
 * - @c parent/child/widget - Find by path
 * - @c parent/ * /widget - Find with wildcard
 * - @c parent/items[1] - Find with index
 */
class WIDGETEER_EXPORT WidgeteerBot : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Construct a WidgeteerBot that owns its own CommandExecutor.
   * @param parent Optional parent QObject
   */
  explicit WidgeteerBot(QObject* parent = nullptr);

  /**
   * @brief Construct a WidgeteerBot using an external CommandExecutor.
   * @param executor Pointer to existing CommandExecutor (not owned)
   * @param parent Optional parent QObject
   */
  explicit WidgeteerBot(CommandExecutor* executor, QObject* parent = nullptr);

  ~WidgeteerBot() override = default;

  // ==================== Action Commands ====================

  /**
   * @brief Click on an element.
   * @param target Element selector
   * @return Result indicating success or failure
   */
  Result<void> click(const QString& target);

  /**
   * @brief Double-click on an element.
   * @param target Element selector
   * @return Result indicating success or failure
   */
  Result<void> doubleClick(const QString& target);

  /**
   * @brief Right-click on an element.
   * @param target Element selector
   * @return Result indicating success or failure
   */
  Result<void> rightClick(const QString& target);

  /**
   * @brief Type text into an element.
   * @param target Element selector
   * @param text Text to type
   * @param clearFirst If true, clear existing content first
   * @return Result indicating success or failure
   */
  Result<void> type(const QString& target, const QString& text, bool clearFirst = false);

  /**
   * @brief Press a key on an element.
   * @param target Element selector
   * @param key Key name (e.g., "Enter", "Escape", "Tab", or single char)
   * @param modifiers Optional modifiers ("ctrl", "shift", "alt", "meta")
   * @return Result indicating success or failure
   */
  Result<void> key(const QString& target, const QString& key,
                   const QStringList& modifiers = QStringList());

  /**
   * @brief Send a key sequence to an element.
   * @param target Element selector
   * @param sequence Key sequence (e.g., "Ctrl+C", "Ctrl+Shift+S")
   * @return Result indicating success or failure
   */
  Result<void> keySequence(const QString& target, const QString& sequence);

  /**
   * @brief Drag from one element to another.
   * @param from Source element selector
   * @param to Destination element selector
   * @return Result indicating success or failure
   */
  Result<void> drag(const QString& from, const QString& to);

  /**
   * @brief Scroll an element.
   * @param target Element selector
   * @param deltaX Horizontal scroll amount
   * @param deltaY Vertical scroll amount
   * @return Result indicating success or failure
   */
  Result<void> scroll(const QString& target, int deltaX, int deltaY);

  /**
   * @brief Hover over an element.
   * @param target Element selector
   * @return Result indicating success or failure
   */
  Result<void> hover(const QString& target);

  /**
   * @brief Set focus to an element.
   * @param target Element selector
   * @return Result indicating success or failure
   */
  Result<void> focus(const QString& target);

  // ==================== State Commands ====================

  /**
   * @brief Set the value of an element (type-aware).
   * @param target Element selector
   * @param value Value to set (type depends on widget)
   * @return Result indicating success or failure
   */
  Result<void> setValue(const QString& target, const QJsonValue& value);

  /**
   * @brief Set a property on an element.
   * @param target Element selector
   * @param property Property name
   * @param value Property value
   * @return Result indicating success or failure
   */
  Result<void> setProperty(const QString& target, const QString& property, const QJsonValue& value);

  /**
   * @brief Get a property value from an element.
   * @param target Element selector
   * @param property Property name
   * @return Result containing the property value or error
   */
  Result<QJsonValue> getProperty(const QString& target, const QString& property);

  /**
   * @brief Invoke a method on an element.
   * @param target Element selector
   * @param method Method name (must be Q_INVOKABLE or slot)
   * @return Result indicating success or failure
   */
  Result<void> invoke(const QString& target, const QString& method);

  // ==================== Query Commands ====================

  /**
   * @brief Check if an element exists.
   * @param target Element selector
   * @return Result containing true if element exists
   */
  Result<bool> exists(const QString& target);

  /**
   * @brief Check if an element is visible.
   * @param target Element selector
   * @return Result containing true if element is visible
   */
  Result<bool> isVisible(const QString& target);

  /**
   * @brief Get the text content of an element.
   * @param target Element selector
   * @return Result containing the text or error
   *
   * Works with QLabel, QLineEdit, QTextEdit, QAbstractButton, etc.
   */
  Result<QString> getText(const QString& target);

  /**
   * @brief List all properties of an element.
   * @param target Element selector
   * @return Result containing array of property names
   */
  Result<QJsonArray> listProperties(const QString& target);

  // ==================== Introspection Commands ====================

  /**
   * @brief Get the widget tree.
   * @param depth Maximum depth (-1 for unlimited)
   * @param includeInvisible Include invisible widgets
   * @return Result containing tree structure as JSON
   */
  Result<QJsonObject> getTree(int depth = -1, bool includeInvisible = false);

  /**
   * @brief Find elements matching a query.
   * @param query Search query (uses same selector syntax)
   * @param maxResults Maximum number of results
   * @return Result containing matches array
   */
  Result<QJsonObject> find(const QString& query, int maxResults = 100);

  /**
   * @brief Get detailed description of an element.
   * @param target Element selector
   * @return Result containing element description
   */
  Result<QJsonObject> describe(const QString& target);

  /**
   * @brief Get available actions for an element.
   * @param target Element selector
   * @return Result containing actions array
   */
  Result<QJsonObject> getActions(const QString& target);

  /**
   * @brief Get form fields in a container.
   * @param root Optional root element selector
   * @return Result containing form fields array
   */
  Result<QJsonObject> getFormFields(const QString& root = QString());

  // ==================== Synchronization Commands ====================

  /**
   * @brief Wait for an element to meet a condition.
   * @param target Element selector
   * @param condition Condition: "exists", "visible", "enabled", "property:name=value"
   * @param timeoutMs Timeout in milliseconds
   * @return Result indicating success or timeout
   */
  Result<void> waitFor(const QString& target, const QString& condition, int timeoutMs = 5000);

  /**
   * @brief Wait for the application to be idle.
   * @param timeoutMs Timeout in milliseconds
   * @return Result indicating success or timeout
   */
  Result<void> waitIdle(int timeoutMs = 5000);

  /**
   * @brief Wait for a signal to be emitted.
   * @param target Element selector
   * @param signal Signal signature (e.g., "clicked()")
   * @param timeoutMs Timeout in milliseconds
   * @return Result indicating success or timeout
   */
  Result<void> waitSignal(const QString& target, const QString& signal, int timeoutMs = 5000);

  /**
   * @brief Sleep for a specified duration.
   * @param ms Duration in milliseconds
   * @return Result (always succeeds)
   */
  Result<void> sleep(int ms);

  // ==================== Screenshot Commands ====================

  /**
   * @brief Take a screenshot.
   * @param target Optional element selector (default: entire window)
   * @return Result containing screenshot data (base64 PNG)
   */
  Result<QJsonObject> screenshot(const QString& target = QString());

  /**
   * @brief Take an annotated screenshot with element labels.
   * @param target Optional element selector (default: entire window)
   * @return Result containing screenshot data and annotations
   */
  Result<QJsonObject> screenshotAnnotated(const QString& target = QString());

  // ==================== Extensibility Commands ====================

  /**
   * @brief Call a method on a registered object.
   * @param object Registered object name
   * @param method Method name
   * @param args Method arguments
   * @return Result containing method return value
   */
  Result<QJsonObject> call(const QString& object, const QString& method,
                           const QJsonArray& args = QJsonArray());

  /**
   * @brief List registered objects.
   * @return Result containing objects array
   */
  Result<QJsonObject> listObjects();

  /**
   * @brief List custom commands.
   * @return Result containing commands array
   */
  Result<QJsonObject> listCustomCommands();

  // ==================== Control Commands ====================

  /**
   * @brief Request application quit.
   * @return Result indicating success
   */
  Result<void> quit();

  /**
   * @brief Get the underlying CommandExecutor.
   * @return Pointer to the CommandExecutor
   */
  CommandExecutor* executor() const {
    return executor_;
  }

private:
  /**
   * @brief Execute a command and return void result.
   */
  Result<void> execute(const QString& command, const QJsonObject& params);

  /**
   * @brief Execute a command and return the full result object.
   */
  Result<QJsonObject> executeForResult(const QString& command, const QJsonObject& params);

  std::unique_ptr<CommandExecutor> ownedExecutor_;
  CommandExecutor* executor_ = nullptr;
};

}  // namespace widgeteer
