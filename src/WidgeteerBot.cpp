#include <widgeteer/WidgeteerBot.h>

#include <QUuid>

namespace widgeteer {

WidgeteerBot::WidgeteerBot(QObject* parent)
  : QObject(parent), ownedExecutor_(std::make_unique<CommandExecutor>(this)) {
  executor_ = ownedExecutor_.get();
}

WidgeteerBot::WidgeteerBot(CommandExecutor* executor, QObject* parent)
  : QObject(parent), executor_(executor) {
}

// ==================== Action Commands ====================

Result<void> WidgeteerBot::click(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return execute("click", params);
}

Result<void> WidgeteerBot::doubleClick(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return execute("double_click", params);
}

Result<void> WidgeteerBot::rightClick(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return execute("right_click", params);
}

Result<void> WidgeteerBot::type(const QString& target, const QString& text, bool clearFirst) {
  QJsonObject params;
  params["target"] = target;
  params["text"] = text;
  if (clearFirst) {
    params["clear_first"] = true;
  }
  return execute("type", params);
}

Result<void> WidgeteerBot::key(const QString& target, const QString& key,
                               const QStringList& modifiers) {
  QJsonObject params;
  params["target"] = target;
  params["key"] = key;
  if (!modifiers.isEmpty()) {
    QJsonArray modsArray;
    for (const QString& mod : modifiers) {
      modsArray.append(mod);
    }
    params["modifiers"] = modsArray;
  }
  return execute("key", params);
}

Result<void> WidgeteerBot::keySequence(const QString& target, const QString& sequence) {
  QJsonObject params;
  params["target"] = target;
  params["sequence"] = sequence;
  return execute("key_sequence", params);
}

Result<void> WidgeteerBot::drag(const QString& from, const QString& to) {
  QJsonObject params;
  params["from"] = from;
  params["to"] = to;
  return execute("drag", params);
}

Result<void> WidgeteerBot::scroll(const QString& target, int deltaX, int deltaY) {
  QJsonObject params;
  params["target"] = target;
  params["delta_x"] = deltaX;
  params["delta_y"] = deltaY;
  return execute("scroll", params);
}

Result<void> WidgeteerBot::hover(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return execute("hover", params);
}

Result<void> WidgeteerBot::focus(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return execute("focus", params);
}

// ==================== State Commands ====================

Result<void> WidgeteerBot::setValue(const QString& target, const QJsonValue& value) {
  QJsonObject params;
  params["target"] = target;
  params["value"] = value;
  return execute("set_value", params);
}

Result<void> WidgeteerBot::setProperty(const QString& target, const QString& property,
                                       const QJsonValue& value) {
  QJsonObject params;
  params["target"] = target;
  params["property"] = property;
  params["value"] = value;
  return execute("set_property", params);
}

Result<QJsonValue> WidgeteerBot::getProperty(const QString& target, const QString& property) {
  QJsonObject params;
  params["target"] = target;
  params["property"] = property;

  auto result = executeForResult("get_property", params);
  if (!result) {
    return Result<QJsonValue>::fail(result.error());
  }
  return Result<QJsonValue>::ok(result.value().value("value"));
}

Result<void> WidgeteerBot::invoke(const QString& target, const QString& method) {
  QJsonObject params;
  params["target"] = target;
  params["method"] = method;
  return execute("invoke", params);
}

// ==================== Query Commands ====================

Result<bool> WidgeteerBot::exists(const QString& target) {
  QJsonObject params;
  params["target"] = target;

  auto result = executeForResult("exists", params);
  if (!result) {
    return Result<bool>::fail(result.error());
  }
  return Result<bool>::ok(result.value().value("exists").toBool());
}

Result<bool> WidgeteerBot::isVisible(const QString& target) {
  QJsonObject params;
  params["target"] = target;

  auto result = executeForResult("is_visible", params);
  if (!result) {
    return Result<bool>::fail(result.error());
  }
  return Result<bool>::ok(result.value().value("visible").toBool());
}

Result<QString> WidgeteerBot::getText(const QString& target) {
  // Use get_property to get "text" property
  QJsonObject params;
  params["target"] = target;
  params["property"] = "text";

  auto result = executeForResult("get_property", params);
  if (!result) {
    return Result<QString>::fail(result.error());
  }
  return Result<QString>::ok(result.value().value("value").toString());
}

Result<QJsonArray> WidgeteerBot::listProperties(const QString& target) {
  QJsonObject params;
  params["target"] = target;

  auto result = executeForResult("list_properties", params);
  if (!result) {
    return Result<QJsonArray>::fail(result.error());
  }
  return Result<QJsonArray>::ok(result.value().value("properties").toArray());
}

// ==================== Introspection Commands ====================

Result<QJsonObject> WidgeteerBot::getTree(int depth, bool includeInvisible) {
  QJsonObject params;
  if (depth >= 0) {
    params["depth"] = depth;
  }
  if (includeInvisible) {
    params["include_invisible"] = true;
  }
  return executeForResult("get_tree", params);
}

Result<QJsonObject> WidgeteerBot::find(const QString& query, int maxResults) {
  QJsonObject params;
  params["query"] = query;
  params["max_results"] = maxResults;
  return executeForResult("find", params);
}

Result<QJsonObject> WidgeteerBot::describe(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return executeForResult("describe", params);
}

Result<QJsonObject> WidgeteerBot::getActions(const QString& target) {
  QJsonObject params;
  params["target"] = target;
  return executeForResult("get_actions", params);
}

Result<QJsonObject> WidgeteerBot::getFormFields(const QString& root) {
  QJsonObject params;
  if (!root.isEmpty()) {
    params["root"] = root;
  }
  return executeForResult("get_form_fields", params);
}

// ==================== Synchronization Commands ====================

Result<void> WidgeteerBot::waitFor(const QString& target, const QString& condition, int timeoutMs) {
  QJsonObject params;
  params["target"] = target;
  params["condition"] = condition;
  params["timeout_ms"] = timeoutMs;
  return execute("wait", params);
}

Result<void> WidgeteerBot::waitIdle(int timeoutMs) {
  QJsonObject params;
  params["timeout_ms"] = timeoutMs;
  return execute("wait_idle", params);
}

Result<void> WidgeteerBot::waitSignal(const QString& target, const QString& signal, int timeoutMs) {
  QJsonObject params;
  params["target"] = target;
  params["signal"] = signal;
  params["timeout_ms"] = timeoutMs;
  return execute("wait_signal", params);
}

Result<void> WidgeteerBot::sleep(int ms) {
  QJsonObject params;
  params["ms"] = ms;
  return execute("sleep", params);
}

// ==================== Screenshot Commands ====================

Result<QJsonObject> WidgeteerBot::screenshot(const QString& target) {
  QJsonObject params;
  if (!target.isEmpty()) {
    params["target"] = target;
  }
  return executeForResult("screenshot", params);
}

Result<QJsonObject> WidgeteerBot::screenshotAnnotated(const QString& target) {
  QJsonObject params;
  if (!target.isEmpty()) {
    params["target"] = target;
  }
  params["annotate"] = true;
  return executeForResult("screenshot", params);
}

// ==================== Extensibility Commands ====================

Result<QJsonObject> WidgeteerBot::call(const QString& object, const QString& method,
                                       const QJsonArray& args) {
  QJsonObject params;
  params["object"] = object;
  params["method"] = method;
  if (!args.isEmpty()) {
    params["args"] = args;
  }
  return executeForResult("call", params);
}

Result<QJsonObject> WidgeteerBot::listObjects() {
  return executeForResult("list_objects", QJsonObject());
}

Result<QJsonObject> WidgeteerBot::listCustomCommands() {
  return executeForResult("list_custom_commands", QJsonObject());
}

// ==================== Control Commands ====================

Result<void> WidgeteerBot::quit() {
  return execute("quit", QJsonObject());
}

// ==================== Private Helpers ====================

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

Result<QJsonObject> WidgeteerBot::executeForResult(const QString& command,
                                                   const QJsonObject& params) {
  Command cmd;
  cmd.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  cmd.name = command;
  cmd.params = params;

  Response response = executor_->execute(cmd);

  if (response.success) {
    return Result<QJsonObject>::ok(response.result);
  }
  return Result<QJsonObject>::fail(response.error);
}

}  // namespace widgeteer
