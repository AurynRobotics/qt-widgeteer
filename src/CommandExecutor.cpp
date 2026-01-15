#include <widgeteer/CommandExecutor.h>

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QApplication>
#include <QBuffer>
#include <QCalendarWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDial>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMetaMethod>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScreen>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QTimeEdit>
#include <QTreeWidget>

namespace widgeteer
{

CommandExecutor::CommandExecutor(QObject* parent) : QObject(parent), synchronizer_(&finder_)
{
}

Response CommandExecutor::execute(const Command& cmd)
{
  QElapsedTimer timer;
  timer.start();

  // Check if state change tracking is requested
  bool trackChanges = cmd.options.value("track_changes").toBool(false);
  QWidget* targetWidget = nullptr;
  QJsonObject stateBefore;

  // Capture state before action if tracking is enabled
  if (trackChanges && cmd.params.contains("target"))
  {
    QString errorOut;
    targetWidget = resolveTarget(cmd.params, errorOut);
    if (targetWidget)
    {
      stateBefore = captureWidgetState(targetWidget);
    }
  }

  QJsonObject result = dispatch(cmd.name, cmd.params);

  Response response;
  response.id = cmd.id;
  response.durationMs = static_cast<int>(timer.elapsed());

  if (result.contains("error"))
  {
    response.success = false;
    response.error.code = result.value("error").toObject().value("code").toString();
    response.error.message = result.value("error").toObject().value("message").toString();
    response.error.details = result.value("error").toObject().value("details").toObject();
  }
  else
  {
    response.success = true;
    response.result = result;

    // Capture state after and compute changes if tracking is enabled
    if (trackChanges && targetWidget)
    {
      QJsonObject stateAfter = captureWidgetState(targetWidget);
      QJsonArray changes = computeStateChanges(stateBefore, stateAfter);
      if (!changes.isEmpty())
      {
        response.result["changes"] = changes;
      }
    }
  }

  return response;
}

TransactionResponse CommandExecutor::execute(const Transaction& tx)
{
  TransactionResponse response;
  response.id = tx.id;
  response.totalSteps = tx.steps.size();

  clearUndoStack();

  for (int i = 0; i < tx.steps.size(); ++i)
  {
    const Command& step = tx.steps[i];
    QJsonObject stepResult = dispatch(step.name, step.params);

    QJsonObject stepResponse;
    stepResponse["step"] = i;
    stepResponse["command"] = step.name;

    if (stepResult.contains("error"))
    {
      stepResponse["success"] = false;
      stepResponse["error"] = stepResult.value("error").toObject();
      response.stepsResults.append(stepResponse);

      if (tx.rollbackOnFailure)
      {
        rollback();
        response.rollbackPerformed = true;
      }

      response.completedSteps = i;
      response.success = false;
      emit transactionCompleted(tx.id, false);
      return response;
    }

    stepResponse["success"] = true;
    response.stepsResults.append(stepResponse);
    emit stepCompleted(i, true);
  }

  response.completedSteps = tx.steps.size();
  response.success = true;
  clearUndoStack();
  emit transactionCompleted(tx.id, true);
  return response;
}

QJsonObject CommandExecutor::dispatch(const QString& command, const QJsonObject& params)
{
  // Introspection commands
  if (command == "get_tree")
    return cmdGetTree(params);
  if (command == "find")
    return cmdFind(params);
  if (command == "describe")
    return cmdDescribe(params);
  if (command == "get_property")
    return cmdGetProperty(params);
  if (command == "list_properties")
    return cmdListProperties(params);
  if (command == "get_actions")
    return cmdGetActions(params);

  // Action commands
  if (command == "click")
    return cmdClick(params);
  if (command == "double_click")
    return cmdDoubleClick(params);
  if (command == "right_click")
    return cmdRightClick(params);
  if (command == "type")
    return cmdType(params);
  if (command == "key")
    return cmdKey(params);
  if (command == "key_sequence")
    return cmdKeySequence(params);
  if (command == "drag")
    return cmdDrag(params);
  if (command == "scroll")
    return cmdScroll(params);
  if (command == "hover")
    return cmdHover(params);
  if (command == "focus")
    return cmdFocus(params);

  // State commands
  if (command == "set_property")
    return cmdSetProperty(params);
  if (command == "invoke")
    return cmdInvoke(params);
  if (command == "set_value")
    return cmdSetValue(params);

  // Verification commands
  if (command == "screenshot")
    return cmdScreenshot(params);
  if (command == "assert")
    return cmdAssert(params);
  if (command == "exists")
    return cmdExists(params);
  if (command == "is_visible")
    return cmdIsVisible(params);

  // Synchronization commands
  if (command == "wait")
    return cmdWait(params);
  if (command == "wait_idle")
    return cmdWaitIdle(params);
  if (command == "wait_signal")
    return cmdWaitSignal(params);
  if (command == "sleep")
    return cmdSleep(params);

  // Extensibility commands
  if (command == "call")
    return cmdCall(params);
  if (command == "list_objects")
    return cmdListObjects(params);
  if (command == "list_custom_commands")
    return cmdListCustomCommands(params);

  // Check for custom command handlers
  if (customCommands_ && customCommands_->contains(command))
  {
    try
    {
      return customCommands_->value(command)(params);
    }
    catch (const std::exception& e)
    {
      QJsonObject error;
      error["code"] = ErrorCode::InvocationFailed;
      error["message"] =
          QStringLiteral("Custom command '%1' threw exception: %2").arg(command, e.what());
      return QJsonObject{ { "error", error } };
    }
  }

  // Unknown command
  QJsonObject error;
  error["code"] = ErrorCode::InvalidCommand;
  error["message"] = QStringLiteral("Unknown command: %1").arg(command);
  return QJsonObject{ { "error", error } };
}

QWidget* CommandExecutor::resolveTarget(const QJsonObject& params, QString& errorOut)
{
  QString target = params.value("target").toString();
  if (target.isEmpty())
  {
    errorOut = "Missing 'target' parameter";
    return nullptr;
  }

  auto result = finder_.find(target);
  if (!result.widget)
  {
    errorOut = result.error;
  }
  return result.widget;
}

// Introspection commands
QJsonObject CommandExecutor::cmdGetTree(const QJsonObject& params)
{
  UIIntrospector::TreeOptions opts;
  opts.maxDepth = params.value("depth").toInt(-1);
  opts.includeInvisible = params.value("include_invisible").toBool(false);
  opts.includeGeometry = params.value("include_geometry").toBool(true);
  opts.includeProperties = params.value("include_properties").toBool(false);

  QWidget* root = nullptr;
  if (params.contains("root"))
  {
    QString errorOut;
    root = resolveTarget(QJsonObject{ { "target", params.value("root") } }, errorOut);
  }

  return introspector_.getTree(root, opts);
}

QJsonObject CommandExecutor::cmdFind(const QJsonObject& params)
{
  QString query = params.value("query").toString();
  if (query.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'query' parameter";
    return QJsonObject{ { "error", error } };
  }

  ElementFinder::FindOptions opts;
  opts.maxResults = params.value("max_results").toInt(100);
  opts.visibleOnly = params.value("visible_only").toBool(false);

  auto results = finder_.findAll(query, opts);

  QJsonArray matches;
  for (const auto& r : results)
  {
    QJsonObject match;
    match["path"] = r.resolvedPath;
    match["class"] = QString::fromLatin1(r.widget->metaObject()->className());
    match["objectName"] = r.widget->objectName();
    matches.append(match);
  }

  return QJsonObject{ { "matches", matches }, { "count", matches.size() } };
}

QJsonObject CommandExecutor::cmdDescribe(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  return introspector_.describe(widget);
}

QJsonObject CommandExecutor::cmdGetProperty(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString propertyName = params.value("property").toString();
  if (propertyName.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'property' parameter";
    return QJsonObject{ { "error", error } };
  }

  QVariant value = widget->property(propertyName.toUtf8().constData());
  if (!value.isValid())
  {
    QJsonObject error;
    error["code"] = ErrorCode::PropertyNotFound;
    error["message"] = QStringLiteral("Property '%1' not found").arg(propertyName);
    return QJsonObject{ { "error", error } };
  }

  QJsonValue jsonValue;
  if (value.canConvert<QString>())
  {
    jsonValue = value.toString();
  }
  else if (value.canConvert<int>())
  {
    jsonValue = value.toInt();
  }
  else if (value.canConvert<bool>())
  {
    jsonValue = value.toBool();
  }
  else if (value.canConvert<double>())
  {
    jsonValue = value.toDouble();
  }

  return QJsonObject{ { "property", propertyName }, { "value", jsonValue } };
}

QJsonObject CommandExecutor::cmdListProperties(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "properties", introspector_.listProperties(widget) } };
}

QJsonObject CommandExecutor::cmdGetActions(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "actions", introspector_.listActions(widget) } };
}

// Action commands
QJsonObject CommandExecutor::cmdClick(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  Qt::MouseButton button = Qt::LeftButton;
  QString btnStr = params.value("button").toString("left");
  if (btnStr == "right")
    button = Qt::RightButton;
  else if (btnStr == "middle")
    button = Qt::MiddleButton;

  QPoint pos;
  if (params.contains("pos"))
  {
    QJsonObject posObj = params.value("pos").toObject();
    pos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }

  // Note: We intentionally use EventInjector for all clicks instead of calling
  // QAbstractButton::click() directly. EventInjector posts mouse events asynchronously,
  // which is required to support clicking buttons that open blocking dialogs (exec()).
  // Direct click() would execute synchronously and block until the dialog closes.
  auto result = injector_.click(widget, button, pos);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  QRect geom = widget->geometry();
  QJsonObject geometry;
  geometry["x"] = geom.x();
  geometry["y"] = geom.y();
  geometry["width"] = geom.width();
  geometry["height"] = geom.height();

  return QJsonObject{ { "clicked", true }, { "target_geometry", geometry } };
}

QJsonObject CommandExecutor::cmdDoubleClick(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QPoint pos;
  if (params.contains("pos"))
  {
    QJsonObject posObj = params.value("pos").toObject();
    pos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }

  auto result = injector_.doubleClick(widget, pos);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "double_clicked", true } };
}

QJsonObject CommandExecutor::cmdRightClick(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QPoint pos;
  if (params.contains("pos"))
  {
    QJsonObject posObj = params.value("pos").toObject();
    pos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }

  auto result = injector_.rightClick(widget, pos);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "right_clicked", true } };
}

QJsonObject CommandExecutor::cmdType(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString text = params.value("text").toString();
  if (text.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'text' parameter";
    return QJsonObject{ { "error", error } };
  }

  bool clearFirst = params.value("clear_first").toBool(false);
  if (clearFirst)
  {
    // Select all and delete
    injector_.keyClick(widget, Qt::Key_A, Qt::ControlModifier);
    injector_.keyClick(widget, Qt::Key_Delete);
  }

  auto result = injector_.type(widget, text);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "typed", true }, { "text", text } };
}

QJsonObject CommandExecutor::cmdKey(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString keyStr = params.value("key").toString();
  if (keyStr.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'key' parameter";
    return QJsonObject{ { "error", error } };
  }

  // Convert key string to Qt::Key (simplified)
  Qt::Key key = Qt::Key_unknown;
  if (keyStr.length() == 1)
  {
    key = static_cast<Qt::Key>(keyStr.toUpper().at(0).unicode());
  }
  else if (keyStr == "Enter" || keyStr == "Return")
  {
    key = Qt::Key_Return;
  }
  else if (keyStr == "Escape" || keyStr == "Esc")
  {
    key = Qt::Key_Escape;
  }
  else if (keyStr == "Tab")
  {
    key = Qt::Key_Tab;
  }
  else if (keyStr == "Backspace")
  {
    key = Qt::Key_Backspace;
  }
  else if (keyStr == "Delete")
  {
    key = Qt::Key_Delete;
  }
  else if (keyStr == "Space")
  {
    key = Qt::Key_Space;
  }

  Qt::KeyboardModifiers mods;
  QJsonArray modsArray = params.value("modifiers").toArray();
  for (const QJsonValue& mod : modsArray)
  {
    QString modStr = mod.toString().toLower();
    if (modStr == "ctrl" || modStr == "control")
      mods |= Qt::ControlModifier;
    else if (modStr == "shift")
      mods |= Qt::ShiftModifier;
    else if (modStr == "alt")
      mods |= Qt::AltModifier;
    else if (modStr == "meta")
      mods |= Qt::MetaModifier;
  }

  auto result = injector_.keyClick(widget, key, mods);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "key_pressed", true } };
}

QJsonObject CommandExecutor::cmdKeySequence(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString sequence = params.value("sequence").toString();
  if (sequence.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'sequence' parameter";
    return QJsonObject{ { "error", error } };
  }

  QKeySequence keySeq(sequence);
  auto result = injector_.shortcut(widget, keySeq);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "key_sequence_sent", true }, { "sequence", sequence } };
}

QJsonObject CommandExecutor::cmdDrag(const QJsonObject& params)
{
  QString errorOut;

  QJsonObject fromParams{ { "target", params.value("from") } };
  QWidget* fromWidget = resolveTarget(fromParams, errorOut);
  if (!fromWidget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = QStringLiteral("From: %1").arg(errorOut);
    return QJsonObject{ { "error", error } };
  }

  QJsonObject toParams{ { "target", params.value("to") } };
  QWidget* toWidget = resolveTarget(toParams, errorOut);
  if (!toWidget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = QStringLiteral("To: %1").arg(errorOut);
    return QJsonObject{ { "error", error } };
  }

  QPoint fromPos, toPos;
  if (params.contains("from_pos"))
  {
    QJsonObject posObj = params.value("from_pos").toObject();
    fromPos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }
  if (params.contains("to_pos"))
  {
    QJsonObject posObj = params.value("to_pos").toObject();
    toPos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }

  auto result = injector_.drag(fromWidget, fromPos, toWidget, toPos);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "dragged", true } };
}

QJsonObject CommandExecutor::cmdScroll(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  int deltaX = params.value("delta_x").toInt(0);
  int deltaY = params.value("delta_y").toInt(0);

  auto result = injector_.scroll(widget, deltaX, deltaY);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "scrolled", true } };
}

QJsonObject CommandExecutor::cmdHover(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QPoint pos;
  if (params.contains("pos"))
  {
    QJsonObject posObj = params.value("pos").toObject();
    pos = QPoint(posObj.value("x").toInt(), posObj.value("y").toInt());
  }

  auto result = injector_.hover(widget, pos);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "hovered", true } };
}

QJsonObject CommandExecutor::cmdFocus(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  auto result = injector_.setFocus(widget);
  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "focused", true } };
}

// State commands
QJsonObject CommandExecutor::cmdSetProperty(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString propertyName = params.value("property").toString();
  if (propertyName.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'property' parameter";
    return QJsonObject{ { "error", error } };
  }

  QJsonValue jsonValue = params.value("value");
  QVariant value;
  if (jsonValue.isBool())
    value = jsonValue.toBool();
  else if (jsonValue.isDouble())
    value = jsonValue.toDouble();
  else if (jsonValue.isString())
    value = jsonValue.toString();
  else
    value = jsonValue.toVariant();

  // Record undo for transaction rollback
  QVariant oldValue = widget->property(propertyName.toUtf8().constData());
  QPointer<QWidget> widgetPtr = widget;
  QByteArray propNameBytes = propertyName.toUtf8();
  recordUndo({ [widgetPtr, propNameBytes, oldValue]() {
                if (widgetPtr)
                {
                  widgetPtr->setProperty(propNameBytes.constData(), oldValue);
                }
              },
               QStringLiteral("Restore %1").arg(propertyName) });

  bool success = widget->setProperty(propertyName.toUtf8().constData(), value);
  if (!success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::PropertyReadOnly;
    error["message"] = QStringLiteral("Failed to set property '%1'").arg(propertyName);
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "property_set", true }, { "property", propertyName } };
}

QJsonObject CommandExecutor::cmdInvoke(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString methodName = params.value("method").toString();
  if (methodName.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'method' parameter";
    return QJsonObject{ { "error", error } };
  }

  // Find the method
  const QMetaObject* meta = widget->metaObject();
  int methodIndex = -1;
  for (int i = 0; i < meta->methodCount(); ++i)
  {
    if (QString::fromLatin1(meta->method(i).name()) == methodName)
    {
      methodIndex = i;
      break;
    }
  }

  if (methodIndex < 0)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = QStringLiteral("Method '%1' not found").arg(methodName);
    return QJsonObject{ { "error", error } };
  }

  QMetaMethod method = meta->method(methodIndex);
  bool success = method.invoke(widget);

  if (!success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = QStringLiteral("Failed to invoke method '%1'").arg(methodName);
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "invoked", true }, { "method", methodName } };
}

QJsonObject CommandExecutor::cmdSetValue(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QJsonValue value = params.value("value");

  // Try to set value based on widget type
  if (auto* tabWidget = qobject_cast<QTabWidget*>(widget))
  {
    tabWidget->setCurrentIndex(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* combo = qobject_cast<QComboBox*>(widget))
  {
    if (value.isDouble())
    {
      combo->setCurrentIndex(value.toInt());
    }
    else
    {
      combo->setCurrentText(value.toString());
    }
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* spin = qobject_cast<QSpinBox*>(widget))
  {
    spin->setValue(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(widget))
  {
    doubleSpin->setValue(value.toDouble());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* slider = qobject_cast<QAbstractSlider*>(widget))
  {
    slider->setValue(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* check = qobject_cast<QAbstractButton*>(widget))
  {
    if (check->isCheckable())
    {
      check->setChecked(value.toBool());
      return QJsonObject{ { "value_set", true } };
    }
  }

  if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
  {
    lineEdit->setText(value.toString());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* textEdit = qobject_cast<QTextEdit*>(widget))
  {
    textEdit->setPlainText(value.toString());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(widget))
  {
    plainTextEdit->setPlainText(value.toString());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* dateTimeEdit = qobject_cast<QDateTimeEdit*>(widget))
  {
    // Try parsing as ISO datetime first, then date, then time
    QString str = value.toString();
    QDateTime dt = QDateTime::fromString(str, Qt::ISODate);
    if (dt.isValid())
    {
      dateTimeEdit->setDateTime(dt);
    }
    else
    {
      QDate d = QDate::fromString(str, Qt::ISODate);
      if (d.isValid())
      {
        dateTimeEdit->setDate(d);
      }
      else
      {
        QTime t = QTime::fromString(str, Qt::ISODate);
        if (t.isValid())
        {
          dateTimeEdit->setTime(t);
        }
        else
        {
          QJsonObject error;
          error["code"] = ErrorCode::InvalidParams;
          error["message"] = "Invalid date/time format. Use ISO format (YYYY-MM-DD, HH:MM:SS, or "
                             "YYYY-MM-DDTHH:MM:SS)";
          return QJsonObject{ { "error", error } };
        }
      }
    }
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* progressBar = qobject_cast<QProgressBar*>(widget))
  {
    progressBar->setValue(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* dial = qobject_cast<QDial*>(widget))
  {
    dial->setValue(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* label = qobject_cast<QLabel*>(widget))
  {
    if (value.isDouble())
    {
      label->setNum(value.toDouble());
    }
    else
    {
      label->setText(value.toString());
    }
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* groupBox = qobject_cast<QGroupBox*>(widget))
  {
    if (groupBox->isCheckable())
    {
      groupBox->setChecked(value.toBool());
      return QJsonObject{ { "value_set", true } };
    }
  }

  if (auto* stackedWidget = qobject_cast<QStackedWidget*>(widget))
  {
    stackedWidget->setCurrentIndex(value.toInt());
    return QJsonObject{ { "value_set", true } };
  }

  if (auto* calendarWidget = qobject_cast<QCalendarWidget*>(widget))
  {
    QString str = value.toString();
    QDate d = QDate::fromString(str, Qt::ISODate);
    if (d.isValid())
    {
      calendarWidget->setSelectedDate(d);
      return QJsonObject{ { "value_set", true } };
    }
    else
    {
      QJsonObject error;
      error["code"] = ErrorCode::InvalidParams;
      error["message"] = "Invalid date format. Use ISO format (YYYY-MM-DD)";
      return QJsonObject{ { "error", error } };
    }
  }

  if (auto* listWidget = qobject_cast<QListWidget*>(widget))
  {
    // Value can be index (int) or text (string)
    if (value.isDouble())
    {
      int row = value.toInt();
      if (row >= 0 && row < listWidget->count())
      {
        listWidget->setCurrentRow(row);
        return QJsonObject{ { "value_set", true } };
      }
    }
    else
    {
      QString text = value.toString();
      QList<QListWidgetItem*> items = listWidget->findItems(text, Qt::MatchExactly);
      if (!items.isEmpty())
      {
        listWidget->setCurrentItem(items.first());
        return QJsonObject{ { "value_set", true } };
      }
    }
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Item not found in list";
    return QJsonObject{ { "error", error } };
  }

  if (auto* treeWidget = qobject_cast<QTreeWidget*>(widget))
  {
    // Value can be text (search in first column) or object with row/column indices
    if (value.isString())
    {
      QString text = value.toString();
      QList<QTreeWidgetItem*> items =
          treeWidget->findItems(text, Qt::MatchExactly | Qt::MatchRecursive);
      if (!items.isEmpty())
      {
        treeWidget->setCurrentItem(items.first());
        return QJsonObject{ { "value_set", true } };
      }
      QJsonObject error;
      error["code"] = ErrorCode::InvalidParams;
      error["message"] = "Item not found in tree";
      return QJsonObject{ { "error", error } };
    }
  }

  if (auto* tableWidget = qobject_cast<QTableWidget*>(widget))
  {
    // Value should be object with "row" and "column" or "text"
    if (value.isObject())
    {
      QJsonObject obj = value.toObject();
      if (obj.contains("row") && obj.contains("column"))
      {
        int row = obj.value("row").toInt();
        int col = obj.value("column").toInt();
        if (row >= 0 && row < tableWidget->rowCount() && col >= 0 &&
            col < tableWidget->columnCount())
        {
          tableWidget->setCurrentCell(row, col);
          return QJsonObject{ { "value_set", true } };
        }
      }
      if (obj.contains("text"))
      {
        QString text = obj.value("text").toString();
        QList<QTableWidgetItem*> items = tableWidget->findItems(text, Qt::MatchExactly);
        if (!items.isEmpty())
        {
          tableWidget->setCurrentItem(items.first());
          return QJsonObject{ { "value_set", true } };
        }
      }
    }
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] =
        "Invalid value for table. Use {\"row\": n, \"column\": m} or {\"text\": \"value\"}";
    return QJsonObject{ { "error", error } };
  }

  QJsonObject error;
  error["code"] = ErrorCode::InvocationFailed;
  error["message"] = "Widget type does not support set_value";
  return QJsonObject{ { "error", error } };
}

// Verification commands
QJsonObject CommandExecutor::cmdScreenshot(const QJsonObject& params)
{
  QWidget* target = nullptr;
  if (params.contains("target"))
  {
    QString errorOut;
    target = resolveTarget(params, errorOut);
    if (!target)
    {
      QJsonObject error;
      error["code"] = ErrorCode::ElementNotFound;
      error["message"] = errorOut;
      return QJsonObject{ { "error", error } };
    }
  }

  QPixmap pixmap;
  if (target)
  {
    pixmap = target->grab();
  }
  else
  {
    // Capture primary screen
    QScreen* screen = QApplication::primaryScreen();
    if (screen)
    {
      pixmap = screen->grabWindow(0);
    }
  }

  if (pixmap.isNull())
  {
    QJsonObject error;
    error["code"] = ErrorCode::ScreenshotFailed;
    error["message"] = "Failed to capture screenshot";
    return QJsonObject{ { "error", error } };
  }

  QString format = params.value("format").toString("png").toLower();

  QByteArray bytes;
  QBuffer buffer(&bytes);
  buffer.open(QIODevice::WriteOnly);
  pixmap.save(&buffer, format.toUtf8().constData());

  return QJsonObject{ { "screenshot", QString::fromLatin1(bytes.toBase64()) },
                      { "format", format },
                      { "width", pixmap.width() },
                      { "height", pixmap.height() } };
}

QJsonObject CommandExecutor::cmdAssert(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString propertyName = params.value("property").toString();
  QString op = params.value("operator").toString("==");
  QJsonValue expected = params.value("value");

  QVariant actual = widget->property(propertyName.toUtf8().constData());

  // Convert QVariant to QJsonValue for proper comparison and reporting
  QJsonValue actualJson;
  if (!actual.isValid())
  {
    actualJson = QJsonValue::Null;
  }
  else if (actual.typeId() == QMetaType::Bool)
  {
    actualJson = actual.toBool();
  }
  else if (actual.typeId() == QMetaType::Int || actual.typeId() == QMetaType::LongLong)
  {
    actualJson = actual.toInt();
  }
  else if (actual.typeId() == QMetaType::Double || actual.typeId() == QMetaType::Float)
  {
    actualJson = actual.toDouble();
  }
  else if (actual.canConvert<QString>())
  {
    actualJson = actual.toString();
  }
  else
  {
    actualJson = actual.toString();
  }

  // For comparison, convert expected value to match actual type when possible
  bool passed = false;
  if (op == "==" || op == "equals")
  {
    if (actualJson.isBool())
    {
      // Handle bool comparison - expected might be string "true"/"false" or bool
      bool expectedBool =
          expected.isBool() ? expected.toBool() : (expected.toString().toLower() == "true");
      passed = actualJson.toBool() == expectedBool;
    }
    else if (actualJson.isDouble())
    {
      // Handle numeric comparison
      double expectedNum =
          expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
      passed = actualJson.toDouble() == expectedNum;
    }
    else
    {
      // String comparison
      passed = actualJson.toString() == expected.toString();
    }
  }
  else if (op == "!=" || op == "not_equals")
  {
    if (actualJson.isDouble())
    {
      double expectedNum =
          expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
      passed = actualJson.toDouble() != expectedNum;
    }
    else
    {
      passed = actualJson.toString() != expected.toString();
    }
  }
  else if (op == ">" || op == "gt")
  {
    double expectedNum = expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
    passed = actualJson.toDouble() > expectedNum;
  }
  else if (op == ">=" || op == "gte")
  {
    double expectedNum = expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
    passed = actualJson.toDouble() >= expectedNum;
  }
  else if (op == "<" || op == "lt")
  {
    double expectedNum = expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
    passed = actualJson.toDouble() < expectedNum;
  }
  else if (op == "<=" || op == "lte")
  {
    double expectedNum = expected.isDouble() ? expected.toDouble() : expected.toString().toDouble();
    passed = actualJson.toDouble() <= expectedNum;
  }
  else if (op == "contains")
  {
    passed = actualJson.toString().contains(expected.toString());
  }

  QJsonObject result;
  result["passed"] = passed;
  result["property"] = propertyName;
  result["operator"] = op;
  result["expected"] = expected;
  result["actual"] = actualJson;

  return result;
}

QJsonObject CommandExecutor::cmdExists(const QJsonObject& params)
{
  QString target = params.value("target").toString();
  auto result = finder_.find(target);

  return QJsonObject{ { "exists", result.widget != nullptr }, { "target", target } };
}

QJsonObject CommandExecutor::cmdIsVisible(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);

  if (!widget)
  {
    return QJsonObject{ { "visible", false }, { "exists", false } };
  }

  return QJsonObject{ { "visible", widget->isVisible() }, { "exists", true } };
}

// Synchronization commands
QJsonObject CommandExecutor::cmdWait(const QJsonObject& params)
{
  Synchronizer::WaitParams waitParams;
  waitParams.target = params.value("target").toString();
  waitParams.timeoutMs = params.value("timeout_ms").toInt(5000);
  waitParams.pollIntervalMs = params.value("poll_interval_ms").toInt(50);
  waitParams.stabilityMs = params.value("stability_ms").toInt(200);

  QString conditionStr = params.value("condition").toString("exists");
  waitParams.condition =
      Synchronizer::parseCondition(conditionStr, waitParams.propertyName, waitParams.propertyValue);

  auto result = synchronizer_.wait(waitParams);

  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::Timeout;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "waited", true }, { "elapsed_ms", result.elapsedMs } };
}

QJsonObject CommandExecutor::cmdWaitIdle(const QJsonObject& params)
{
  int timeoutMs = params.value("timeout_ms").toInt(5000);
  auto result = synchronizer_.waitForIdle(timeoutMs);

  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::Timeout;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "idle", true }, { "elapsed_ms", result.elapsedMs } };
}

QJsonObject CommandExecutor::cmdWaitSignal(const QJsonObject& params)
{
  QString errorOut;
  QWidget* widget = resolveTarget(params, errorOut);
  if (!widget)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = errorOut;
    return QJsonObject{ { "error", error } };
  }

  QString signalName = params.value("signal").toString();
  int timeoutMs = params.value("timeout_ms").toInt(5000);

  QByteArray signalSignature = QMetaObject::normalizedSignature(signalName.toUtf8().constData());
  auto result = synchronizer_.waitForSignal(widget, signalSignature.constData(), timeoutMs);

  if (!result.success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::Timeout;
    error["message"] = result.error;
    return QJsonObject{ { "error", error } };
  }

  return QJsonObject{ { "signal_received", true }, { "elapsed_ms", result.elapsedMs } };
}

QJsonObject CommandExecutor::cmdSleep(const QJsonObject& params)
{
  int ms = params.value("ms").toInt(1000);
  QThread::msleep(static_cast<unsigned long>(ms));
  return QJsonObject{ { "slept", true }, { "ms", ms } };
}

// Rollback support
void CommandExecutor::recordUndo(UndoAction action)
{
  undoStack_.push(action);
}

void CommandExecutor::rollback()
{
  while (!undoStack_.isEmpty())
  {
    UndoAction action = undoStack_.pop();
    action.undo();
  }
}

void CommandExecutor::clearUndoStack()
{
  undoStack_.clear();
}

// Extensibility support
void CommandExecutor::setRegisteredObjects(const QHash<QString, QPointer<QObject>>* objects)
{
  registeredObjects_ = objects;
}

void CommandExecutor::setCustomCommands(const QHash<QString, CommandHandler>* commands)
{
  customCommands_ = commands;
}

QJsonObject CommandExecutor::cmdCall(const QJsonObject& params)
{
  QString objectName = params.value("object").toString();
  if (objectName.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'object' parameter";
    return QJsonObject{ { "error", error } };
  }

  QString methodName = params.value("method").toString();
  if (methodName.isEmpty())
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvalidParams;
    error["message"] = "Missing 'method' parameter";
    return QJsonObject{ { "error", error } };
  }

  if (!registeredObjects_ || !registeredObjects_->contains(objectName))
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = QStringLiteral("Registered object '%1' not found").arg(objectName);
    return QJsonObject{ { "error", error } };
  }

  QObject* object = registeredObjects_->value(objectName);
  if (!object)
  {
    QJsonObject error;
    error["code"] = ErrorCode::ElementNotFound;
    error["message"] = QStringLiteral("Registered object '%1' has been deleted").arg(objectName);
    return QJsonObject{ { "error", error } };
  }

  QJsonArray args = params.value("args").toArray();
  return invokeMethod(object, methodName, args);
}

QJsonObject CommandExecutor::cmdListObjects(const QJsonObject& params)
{
  Q_UNUSED(params);

  QJsonArray objects;

  if (registeredObjects_)
  {
    for (auto it = registeredObjects_->constBegin(); it != registeredObjects_->constEnd(); ++it)
    {
      QJsonObject obj;
      obj["name"] = it.key();

      QObject* qobj = it.value();
      if (qobj)
      {
        obj["class"] = QString::fromLatin1(qobj->metaObject()->className());

        // List Q_INVOKABLE methods
        QJsonArray methods;
        const QMetaObject* meta = qobj->metaObject();
        for (int i = meta->methodOffset(); i < meta->methodCount(); ++i)
        {
          QMetaMethod method = meta->method(i);
          if (method.methodType() == QMetaMethod::Method ||
              method.methodType() == QMetaMethod::Slot)
          {
            // Check if invokable (public and accessible)
            if (method.access() == QMetaMethod::Public)
            {
              QJsonObject methodInfo;
              methodInfo["name"] = QString::fromLatin1(method.name());
              methodInfo["signature"] = QString::fromLatin1(method.methodSignature());
              methodInfo["returnType"] = QString::fromLatin1(method.typeName());

              QJsonArray paramTypes;
              for (int j = 0; j < method.parameterCount(); ++j)
              {
                paramTypes.append(QString::fromLatin1(method.parameterTypeName(j)));
              }
              methodInfo["parameterTypes"] = paramTypes;

              methods.append(methodInfo);
            }
          }
        }
        obj["methods"] = methods;
      }
      else
      {
        obj["class"] = "null";
        obj["methods"] = QJsonArray();
      }

      objects.append(obj);
    }
  }

  return QJsonObject{ { "objects", objects }, { "count", objects.size() } };
}

QJsonObject CommandExecutor::cmdListCustomCommands(const QJsonObject& params)
{
  Q_UNUSED(params);

  QJsonArray commands;

  if (customCommands_)
  {
    for (auto it = customCommands_->constBegin(); it != customCommands_->constEnd(); ++it)
    {
      commands.append(it.key());
    }
  }

  return QJsonObject{ { "commands", commands }, { "count", commands.size() } };
}

QJsonObject CommandExecutor::invokeMethod(QObject* object, const QString& methodName,
                                          const QJsonArray& args)
{
  const QMetaObject* meta = object->metaObject();

  // Find a matching method by name and parameter count
  int methodIndex = -1;
  QMetaMethod matchedMethod;

  for (int i = 0; i < meta->methodCount(); ++i)
  {
    QMetaMethod method = meta->method(i);
    if (QString::fromLatin1(method.name()) == methodName &&
        method.parameterCount() == args.size() && method.access() == QMetaMethod::Public)
    {
      methodIndex = i;
      matchedMethod = method;
      break;
    }
  }

  if (methodIndex < 0)
  {
    // Try to find a method with matching name (for better error message)
    bool foundName = false;
    int foundParamCount = -1;
    for (int i = 0; i < meta->methodCount(); ++i)
    {
      if (QString::fromLatin1(meta->method(i).name()) == methodName)
      {
        foundName = true;
        foundParamCount = meta->method(i).parameterCount();
        break;
      }
    }

    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    if (foundName)
    {
      error["message"] =
          QStringLiteral(
              "Method '%1' found but parameter count mismatch (got %2 args, expected %3)")
              .arg(methodName)
              .arg(args.size())
              .arg(foundParamCount);
    }
    else
    {
      error["message"] = QStringLiteral("Method '%1' not found").arg(methodName);
    }
    return QJsonObject{ { "error", error } };
  }

  // Check return type
  int returnTypeId = matchedMethod.returnType();
  QString returnTypeName = QString::fromLatin1(matchedMethod.typeName());
  bool hasReturn = returnTypeId != QMetaType::Void && !returnTypeName.isEmpty();

  // Build the invocation - we need to handle types properly
  bool success = false;
  QVariant returnValue;

  // For methods with no arguments, use simpler invocation
  if (args.isEmpty())
  {
    if (!hasReturn)
    {
      success = matchedMethod.invoke(object, Qt::DirectConnection);
    }
    else
    {
      // Create appropriate return storage based on return type
      switch (returnTypeId)
      {
        case QMetaType::Int: {
          int ret = 0;
          success = matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(int, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::Bool: {
          bool ret = false;
          success = matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(bool, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::Double: {
          double ret = 0.0;
          success = matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(double, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::QString: {
          QString ret;
          success = matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(QString, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::QVariantMap: {
          QVariantMap ret;
          success =
              matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(QVariantMap, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::QVariantList: {
          QVariantList ret;
          success =
              matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(QVariantList, ret));
          returnValue = ret;
          break;
        }
        case QMetaType::QJsonObject: {
          QJsonObject ret;
          success =
              matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(QJsonObject, ret));
          returnValue = ret;
          break;
        }
        default: {
          // Try generic QVariant return
          QVariant ret;
          success = matchedMethod.invoke(object, Qt::DirectConnection, Q_RETURN_ARG(QVariant, ret));
          returnValue = ret;
          break;
        }
      }
    }
  }
  else
  {
    // For methods with arguments, use QMetaObject::metacall which works across Qt versions
    // This bypasses the QGenericArgument/QMetaMethodArgument API differences between Qt 6.4/6.5

    // Convert JSON args to appropriate types based on method signature
    QVariantList variantStorage;

    for (int i = 0; i < args.size() && i < matchedMethod.parameterCount(); ++i)
    {
      int paramType = matchedMethod.parameterType(i);
      QVariant argVariant = args.at(i).toVariant();

      // Convert to expected type
      if (paramType != QMetaType::QVariant && argVariant.typeId() != paramType)
      {
        argVariant.convert(QMetaType(paramType));
      }
      variantStorage.append(argVariant);
    }

    // Build args array for metacall: args[0] = return value, args[1..n] = parameters
    constexpr int MAX_ARGS = 10;
    void* callArgs[MAX_ARGS + 1] = { nullptr };

    // Set up return value storage if method has a return type
    QVariant returnStorage;
    if (hasReturn)
    {
      int returnType = matchedMethod.returnType();
      returnStorage = QVariant(QMetaType(returnType));
      callArgs[0] = returnStorage.data();
    }

    // Set up parameter pointers
    for (int i = 0; i < variantStorage.size() && i < MAX_ARGS; ++i)
    {
      callArgs[i + 1] = variantStorage[i].data();
    }

    // Get method index relative to this class
    int methodIndex = matchedMethod.methodIndex();

    // Invoke using metacall
    int result =
        QMetaObject::metacall(object, QMetaObject::InvokeMetaMethod, methodIndex, callArgs);

    success = (result < 0);  // metacall returns -1 on success, >= 0 on failure

    if (hasReturn && success)
    {
      returnValue = returnStorage;
    }
  }

  if (!success)
  {
    QJsonObject error;
    error["code"] = ErrorCode::InvocationFailed;
    error["message"] = QStringLiteral("Failed to invoke method '%1'").arg(methodName);
    return QJsonObject{ { "error", error } };
  }

  QJsonObject result;
  result["invoked"] = true;
  result["method"] = methodName;

  if (hasReturn && returnValue.isValid())
  {
    result["return"] = variantToJson(returnValue);
  }

  return result;
}

QJsonValue CommandExecutor::variantToJson(const QVariant& value)
{
  if (!value.isValid())
  {
    return QJsonValue::Null;
  }

  switch (value.typeId())
  {
    case QMetaType::Bool:
      return value.toBool();
    case QMetaType::Int:
    case QMetaType::LongLong:
      return value.toInt();
    case QMetaType::UInt:
    case QMetaType::ULongLong:
      return value.toLongLong();
    case QMetaType::Float:
    case QMetaType::Double:
      return value.toDouble();
    case QMetaType::QString:
      return value.toString();
    case QMetaType::QStringList: {
      QJsonArray arr;
      for (const QString& s : value.toStringList())
      {
        arr.append(s);
      }
      return arr;
    }
    case QMetaType::QVariantList: {
      QJsonArray arr;
      for (const QVariant& v : value.toList())
      {
        arr.append(variantToJson(v));
      }
      return arr;
    }
    case QMetaType::QVariantMap: {
      QJsonObject obj;
      QVariantMap map = value.toMap();
      for (auto it = map.constBegin(); it != map.constEnd(); ++it)
      {
        obj[it.key()] = variantToJson(it.value());
      }
      return obj;
    }
    case QMetaType::QJsonValue:
      return value.toJsonValue();
    case QMetaType::QJsonObject:
      return value.toJsonObject();
    case QMetaType::QJsonArray:
      return value.toJsonArray();
    default:
      // Try string conversion as fallback
      if (value.canConvert<QString>())
      {
        return value.toString();
      }
      return QJsonValue::Null;
  }
}

QJsonObject CommandExecutor::captureWidgetState(QWidget* widget)
{
  QJsonObject state;
  if (!widget)
    return state;

  // Capture common properties that might change during actions
  state["enabled"] = widget->isEnabled();
  state["visible"] = widget->isVisible();

  // Text content
  if (auto* label = qobject_cast<QLabel*>(widget))
  {
    state["text"] = label->text();
  }
  else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
  {
    state["text"] = lineEdit->text();
  }
  else if (auto* textEdit = qobject_cast<QTextEdit*>(widget))
  {
    state["text"] = textEdit->toPlainText();
  }
  else if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(widget))
  {
    state["text"] = plainTextEdit->toPlainText();
  }
  else if (auto* button = qobject_cast<QAbstractButton*>(widget))
  {
    state["text"] = button->text();
    state["checked"] = button->isChecked();
  }

  // Numeric values
  if (auto* spinBox = qobject_cast<QSpinBox*>(widget))
  {
    state["value"] = spinBox->value();
  }
  else if (auto* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(widget))
  {
    state["value"] = doubleSpinBox->value();
  }
  else if (auto* slider = qobject_cast<QAbstractSlider*>(widget))
  {
    state["value"] = slider->value();
  }
  else if (auto* progressBar = qobject_cast<QProgressBar*>(widget))
  {
    state["value"] = progressBar->value();
  }

  // Selection
  if (auto* comboBox = qobject_cast<QComboBox*>(widget))
  {
    state["currentIndex"] = comboBox->currentIndex();
    state["currentText"] = comboBox->currentText();
  }
  else if (auto* tabWidget = qobject_cast<QTabWidget*>(widget))
  {
    state["currentIndex"] = tabWidget->currentIndex();
  }
  else if (auto* listWidget = qobject_cast<QListWidget*>(widget))
  {
    state["currentRow"] = listWidget->currentRow();
  }

  return state;
}

QJsonArray CommandExecutor::computeStateChanges(const QJsonObject& before, const QJsonObject& after)
{
  QJsonArray changes;

  // Find properties that changed
  for (auto it = after.constBegin(); it != after.constEnd(); ++it)
  {
    const QString& key = it.key();
    QJsonValue newVal = it.value();
    QJsonValue oldVal = before.value(key);

    if (oldVal != newVal)
    {
      QJsonObject change;
      change["property"] = key;
      change["old"] = oldVal;
      change["new"] = newVal;
      changes.append(change);
    }
  }

  return changes;
}

}  // namespace widgeteer
