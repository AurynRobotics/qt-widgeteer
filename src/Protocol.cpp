#include <widgeteer/Protocol.h>

#include <QJsonArray>

namespace widgeteer
{

// MessageType conversion
QString messageTypeToString(MessageType type)
{
  switch (type)
  {
    case MessageType::Command:
      return QStringLiteral("command");
    case MessageType::Response:
      return QStringLiteral("response");
    case MessageType::Event:
      return QStringLiteral("event");
    case MessageType::Subscribe:
      return QStringLiteral("subscribe");
    case MessageType::Unsubscribe:
      return QStringLiteral("unsubscribe");
    case MessageType::RecordStart:
      return QStringLiteral("record_start");
    case MessageType::RecordStop:
      return QStringLiteral("record_stop");
  }
  return QStringLiteral("unknown");
}

std::optional<MessageType> stringToMessageType(const QString& str)
{
  if (str == QLatin1String("command"))
    return MessageType::Command;
  if (str == QLatin1String("response"))
    return MessageType::Response;
  if (str == QLatin1String("event"))
    return MessageType::Event;
  if (str == QLatin1String("subscribe"))
    return MessageType::Subscribe;
  if (str == QLatin1String("unsubscribe"))
    return MessageType::Unsubscribe;
  if (str == QLatin1String("record_start"))
    return MessageType::RecordStart;
  if (str == QLatin1String("record_stop"))
    return MessageType::RecordStop;
  return std::nullopt;
}

// Command
Command Command::fromJson(const QJsonObject& json)
{
  Command cmd;
  cmd.id = json.value("id").toString();
  cmd.name = json.value("command").toString();
  cmd.params = json.value("params").toObject();
  cmd.options = json.value("options").toObject();
  return cmd;
}

QJsonObject Command::toJson() const
{
  QJsonObject json;
  json["id"] = id;
  json["command"] = name;
  json["params"] = params;
  json["options"] = options;
  return json;
}

// Transaction
Transaction Transaction::fromJson(const QJsonObject& json)
{
  Transaction tx;
  tx.id = json.value("id").toString();
  tx.rollbackOnFailure = json.value("rollback_on_failure").toBool(true);

  const QJsonArray steps = json.value("steps").toArray();
  for (const QJsonValue& step : steps)
  {
    Command cmd;
    QJsonObject stepObj = step.toObject();
    cmd.name = stepObj.value("command").toString();
    cmd.params = stepObj.value("params").toObject();
    tx.steps.append(cmd);
  }

  return tx;
}

QJsonObject Transaction::toJson() const
{
  QJsonObject json;
  json["id"] = id;
  json["transaction"] = true;
  json["rollback_on_failure"] = rollbackOnFailure;

  QJsonArray stepsArray;
  for (const Command& cmd : steps)
  {
    QJsonObject stepObj;
    stepObj["command"] = cmd.name;
    stepObj["params"] = cmd.params;
    stepsArray.append(stepObj);
  }
  json["steps"] = stepsArray;

  return json;
}

// ErrorDetails
QJsonObject ErrorDetails::toJson() const
{
  QJsonObject json;
  json["code"] = code;
  json["message"] = message;
  if (!details.isEmpty())
  {
    json["details"] = details;
  }
  return json;
}

// Response
QJsonObject Response::toJson() const
{
  QJsonObject json;
  json["id"] = id;
  json["success"] = success;

  if (success)
  {
    if (!result.isEmpty())
    {
      json["result"] = result;
    }
  }
  else
  {
    json["error"] = error.toJson();
  }

  if (durationMs > 0)
  {
    json["duration_ms"] = durationMs;
  }

  return json;
}

Response Response::ok(const QString& id, const QJsonObject& result)
{
  Response resp;
  resp.id = id;
  resp.success = true;
  resp.result = result;
  return resp;
}

Response Response::fail(const QString& id, const QString& code, const QString& message,
                        const QJsonObject& details)
{
  Response resp;
  resp.id = id;
  resp.success = false;
  resp.error.code = code;
  resp.error.message = message;
  resp.error.details = details;
  return resp;
}

// TransactionResponse
QJsonObject TransactionResponse::toJson() const
{
  QJsonObject json;
  json["id"] = id;
  json["success"] = success;
  json["completed_steps"] = completedSteps;
  json["total_steps"] = totalSteps;
  json["steps_results"] = stepsResults;
  json["rollback_performed"] = rollbackPerformed;
  return json;
}

// Helper function
QJsonObject buildElementNotFoundError(const QString& searchedPath, const QString& partialMatch,
                                      const QStringList& availableChildren)
{
  QJsonObject details;
  details["searched_path"] = searchedPath;

  if (!partialMatch.isEmpty())
  {
    details["partial_match"] = partialMatch;
  }

  if (!availableChildren.isEmpty())
  {
    QJsonArray children;
    for (const QString& child : availableChildren)
    {
      children.append(child);
    }
    details["available_children"] = children;
  }

  return details;
}

}  // namespace widgeteer
