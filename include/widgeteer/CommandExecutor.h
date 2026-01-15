#pragma once

#include <widgeteer/ElementFinder.h>
#include <widgeteer/EventInjector.h>
#include <widgeteer/Export.h>
#include <widgeteer/Protocol.h>
#include <widgeteer/Synchronizer.h>
#include <widgeteer/UIIntrospector.h>

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStack>

#include <functional>

namespace widgeteer
{

// Type alias for custom command handlers
using CommandHandler = std::function<QJsonObject(const QJsonObject& params)>;

class WIDGETEER_EXPORT CommandExecutor : public QObject
{
  Q_OBJECT

public:
  explicit CommandExecutor(QObject* parent = nullptr);

  // Execute a single command
  Response execute(const Command& cmd);

  // Execute a transaction (multiple commands)
  TransactionResponse execute(const Transaction& tx);

  // Extensibility: set registered objects and custom commands
  void setRegisteredObjects(const QHash<QString, QPointer<QObject>>* objects);
  void setCustomCommands(const QHash<QString, CommandHandler>* commands);

signals:
  void stepCompleted(int index, bool success);
  void transactionCompleted(const QString& id, bool success);

private:
  // Command handlers - Introspection
  QJsonObject cmdGetTree(const QJsonObject& params);
  QJsonObject cmdFind(const QJsonObject& params);
  QJsonObject cmdDescribe(const QJsonObject& params);
  QJsonObject cmdGetProperty(const QJsonObject& params);
  QJsonObject cmdListProperties(const QJsonObject& params);
  QJsonObject cmdGetActions(const QJsonObject& params);

  // Command handlers - Actions
  QJsonObject cmdClick(const QJsonObject& params);
  QJsonObject cmdDoubleClick(const QJsonObject& params);
  QJsonObject cmdRightClick(const QJsonObject& params);
  QJsonObject cmdType(const QJsonObject& params);
  QJsonObject cmdKey(const QJsonObject& params);
  QJsonObject cmdKeySequence(const QJsonObject& params);
  QJsonObject cmdDrag(const QJsonObject& params);
  QJsonObject cmdScroll(const QJsonObject& params);
  QJsonObject cmdHover(const QJsonObject& params);
  QJsonObject cmdFocus(const QJsonObject& params);

  // Command handlers - State
  QJsonObject cmdSetProperty(const QJsonObject& params);
  QJsonObject cmdInvoke(const QJsonObject& params);
  QJsonObject cmdSetValue(const QJsonObject& params);

  // Command handlers - Verification
  QJsonObject cmdScreenshot(const QJsonObject& params);
  QJsonObject cmdAssert(const QJsonObject& params);
  QJsonObject cmdExists(const QJsonObject& params);
  QJsonObject cmdIsVisible(const QJsonObject& params);

  // Command handlers - Synchronization
  QJsonObject cmdWait(const QJsonObject& params);
  QJsonObject cmdWaitIdle(const QJsonObject& params);
  QJsonObject cmdWaitSignal(const QJsonObject& params);
  QJsonObject cmdSleep(const QJsonObject& params);

  // Command handlers - Extensibility
  QJsonObject cmdCall(const QJsonObject& params);
  QJsonObject cmdListObjects(const QJsonObject& params);
  QJsonObject cmdListCustomCommands(const QJsonObject& params);

  // Dispatch command by name
  QJsonObject dispatch(const QString& command, const QJsonObject& params);

  // Resolve target widget from params
  QWidget* resolveTarget(const QJsonObject& params, QString& errorOut);

  // Rollback support
  struct UndoAction
  {
    std::function<void()> undo;
    QString description;
  };
  QStack<UndoAction> undoStack_;
  void recordUndo(UndoAction action);
  void rollback();
  void clearUndoStack();

  // Helper for invoking Q_INVOKABLE methods
  QJsonObject invokeMethod(QObject* object, const QString& methodName, const QJsonArray& args);

  // Convert QVariant to QJsonValue
  static QJsonValue variantToJson(const QVariant& value);

  // Capture widget state for change tracking
  QJsonObject captureWidgetState(QWidget* widget);
  QJsonArray computeStateChanges(const QJsonObject& before, const QJsonObject& after);

  // Components
  ElementFinder finder_;
  UIIntrospector introspector_;
  EventInjector injector_;
  Synchronizer synchronizer_;

  // Pointers to extensibility data (owned by Server)
  const QHash<QString, QPointer<QObject>>* registeredObjects_ = nullptr;
  const QHash<QString, CommandHandler>* customCommands_ = nullptr;
};

}  // namespace widgeteer
