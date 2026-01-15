#include <widgeteer/Server.h>

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace widgeteer
{

Server::Server(QObject* parent)
  : QObject(parent)
  , wsServer_(std::make_unique<QWebSocketServer>(QStringLiteral("Widgeteer"),
                                                 QWebSocketServer::NonSecureMode, this))
  , executor_(std::make_unique<CommandExecutor>())
  , recorder_(std::make_unique<ActionRecorder>(this))
  , broadcaster_(std::make_unique<EventBroadcaster>(this))
{
  // Default to localhost only
  allowedHosts_ = { "127.0.0.1", "::1", "localhost" };

  connect(wsServer_.get(), &QWebSocketServer::newConnection, this, &Server::onNewConnection);

  // Connect event broadcaster to send events to clients
  connect(broadcaster_.get(), &EventBroadcaster::eventReady, this, &Server::onEventReady);
}

Server::~Server()
{
  stop();
}

bool Server::start(quint16 port)
{
  if (running_)
  {
    return true;
  }

  port_ = port;

  if (!wsServer_->listen(QHostAddress::Any, port_))
  {
    emit serverError(QStringLiteral("Failed to start server on port %1: %2")
                         .arg(port_)
                         .arg(wsServer_->errorString()));
    return false;
  }

  port_ = wsServer_->serverPort();
  running_ = true;

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer WebSocket server started on port" << port_;
  }

  emit serverStarted(port_);
  return true;
}

void Server::stop()
{
  if (!running_)
  {
    return;
  }

  // Close all client connections
  for (auto it = clients_.begin(); it != clients_.end(); ++it)
  {
    if (it.value().socket)
    {
      it.value().socket->close();
    }
  }
  clients_.clear();
  socketToClientId_.clear();

  wsServer_->close();
  running_ = false;

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer server stopped";
  }

  emit serverStopped();
}

bool Server::isRunning() const
{
  return running_;
}

void Server::setPort(quint16 port)
{
  port_ = port;
}

quint16 Server::port() const
{
  return port_;
}

void Server::setAllowedHosts(const QStringList& hosts)
{
  allowedHosts_ = hosts;
}

void Server::enableLogging(bool enable)
{
  loggingEnabled_ = enable;
}

void Server::setApiKey(const QString& apiKey)
{
  apiKey_ = apiKey;
}

QString Server::apiKey() const
{
  return apiKey_;
}

void Server::setRootWidget(QWidget* root)
{
  rootWidget_ = root;
}

// ========== Recording API ==========

void Server::startRecording()
{
  recorder_->start();
  if (loggingEnabled_)
  {
    qDebug() << "Recording started";
  }
}

void Server::stopRecording()
{
  recorder_->stop();
  if (loggingEnabled_)
  {
    qDebug() << "Recording stopped, actions recorded:" << recorder_->actionCount();
  }
}

bool Server::isRecording() const
{
  return recorder_->isRecording();
}

QJsonObject Server::getRecording() const
{
  return recorder_->getRecording();
}

// ========== Event Broadcasting API ==========

void Server::setEventBroadcastingEnabled(bool enabled)
{
  broadcaster_->setEnabled(enabled);
}

bool Server::isEventBroadcastingEnabled() const
{
  return broadcaster_->isEnabled();
}

// ========== Extensibility API Implementation ==========

void Server::registerObject(const QString& name, QObject* object)
{
  if (!object)
  {
    qWarning() << "Widgeteer: Cannot register null object with name:" << name;
    return;
  }

  registeredObjects_[name] = object;

  // Update executor with the new registration
  executor_->setRegisteredObjects(&registeredObjects_);

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer: Registered object:" << name << "->"
             << object->metaObject()->className();
  }
}

void Server::unregisterObject(const QString& name)
{
  registeredObjects_.remove(name);

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer: Unregistered object:" << name;
  }
}

void Server::registerCommand(const QString& name, CommandHandler handler)
{
  if (!handler)
  {
    qWarning() << "Widgeteer: Cannot register null handler for command:" << name;
    return;
  }

  // Check for conflicts with built-in commands
  static const QSet<QString> builtinCommands = {
    "get_tree",    "find",         "describe",     "get_property", "list_properties",
    "get_actions", "click",        "double_click", "right_click",  "type",
    "key",         "key_sequence", "drag",         "scroll",       "hover",
    "focus",       "set_property", "invoke",       "set_value",    "screenshot",
    "assert",      "exists",       "is_visible",   "wait",         "wait_idle",
    "wait_signal", "sleep",        "call",         "list_objects", "list_custom_commands"
  };

  if (builtinCommands.contains(name))
  {
    qWarning() << "Widgeteer: Cannot override built-in command:" << name;
    return;
  }

  customCommands_[name] = handler;

  // Update executor with the new registration
  executor_->setCustomCommands(&customCommands_);

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer: Registered custom command:" << name;
  }
}

void Server::unregisterCommand(const QString& name)
{
  customCommands_.remove(name);

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer: Unregistered custom command:" << name;
  }
}

QStringList Server::registeredObjects() const
{
  return registeredObjects_.keys();
}

QStringList Server::registeredCommands() const
{
  return customCommands_.keys();
}

// ========== WebSocket Connection Handling ==========

void Server::onNewConnection()
{
  while (wsServer_->hasPendingConnections())
  {
    QWebSocket* socket = wsServer_->nextPendingConnection();

    // Check allowed hosts
    QString remoteHost = socket->peerAddress().toString();
    if (!isAllowedHost(remoteHost))
    {
      if (loggingEnabled_)
      {
        qDebug() << "Rejected connection from disallowed host:" << remoteHost;
      }
      socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Forbidden");
      socket->deleteLater();
      continue;
    }

    // Validate API key if set
    if (!apiKey_.isEmpty() && !validateApiKey(socket->requestUrl()))
    {
      if (loggingEnabled_)
      {
        qDebug() << "Rejected connection: Invalid or missing API key";
      }
      socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Unauthorized");
      socket->deleteLater();
      continue;
    }

    // Generate unique client ID
    QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Store client info
    ClientInfo info;
    info.socket = socket;
    info.id = clientId;
    info.authenticated = true;
    clients_[clientId] = info;
    socketToClientId_[socket] = clientId;

    connect(socket, &QWebSocket::textMessageReceived, this, &Server::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected, this, &Server::onClientDisconnected);

    if (loggingEnabled_)
    {
      qDebug() << "Client connected:" << clientId << "from" << remoteHost;
    }

    emit clientConnected(clientId);
  }
}

void Server::onTextMessageReceived(const QString& message)
{
  QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket)
  {
    return;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError)
  {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "PARSE_ERROR" },
                     { "message",
                       QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()) } };
    sendResponse(socket, errorResponse);
    return;
  }

  handleMessage(socket, doc.object());
}

void Server::onClientDisconnected()
{
  QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket)
  {
    return;
  }

  QString clientId = socketToClientId_.value(socket);

  if (!clientId.isEmpty())
  {
    // Remove from event broadcaster subscriptions
    broadcaster_->removeClient(clientId);

    clients_.remove(clientId);
    socketToClientId_.remove(socket);

    if (loggingEnabled_)
    {
      qDebug() << "Client disconnected:" << clientId;
    }

    emit clientDisconnected(clientId);
  }

  socket->deleteLater();
}

void Server::onEventReady(const QString& eventType, const QJsonObject& data,
                          const QStringList& recipientClientIds)
{
  for (const QString& clientId : recipientClientIds)
  {
    if (clients_.contains(clientId))
    {
      QWebSocket* socket = clients_[clientId].socket;
      if (socket)
      {
        sendEvent(socket, eventType, data);
      }
    }
  }
}

// ========== Message Handling ==========

void Server::handleMessage(QWebSocket* client, const QJsonObject& message)
{
  QString typeStr = message.value("type").toString();
  auto type = stringToMessageType(typeStr);

  if (!type.has_value())
  {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "INVALID_TYPE" },
                     { "message", QStringLiteral("Unknown message type: %1").arg(typeStr) } };
    sendResponse(client, errorResponse);
    return;
  }

  switch (type.value())
  {
    case MessageType::Command:
      handleCommand(client, message);
      break;
    case MessageType::Subscribe:
      handleSubscribe(client, message);
      break;
    case MessageType::Unsubscribe:
      handleUnsubscribe(client, message);
      break;
    case MessageType::RecordStart:
      handleRecordStart(client, message);
      break;
    case MessageType::RecordStop:
      handleRecordStop(client, message);
      break;
    default: {
      QJsonObject errorResponse;
      errorResponse["type"] = messageTypeToString(MessageType::Response);
      errorResponse["success"] = false;
      errorResponse["error"] =
          QJsonObject{ { "code", "INVALID_TYPE" },
                       { "message",
                         QStringLiteral("Cannot handle message type: %1").arg(typeStr) } };
      sendResponse(client, errorResponse);
    }
  }
}

void Server::handleCommand(QWebSocket* client, const QJsonObject& message)
{
  Command cmd = Command::fromJson(message);
  emit requestReceived(cmd.id, cmd.name);

  if (loggingEnabled_)
  {
    qDebug() << "Command:" << cmd.name << "Target:" << cmd.params.value("target").toString();
  }

  // Schedule command execution asynchronously.
  // This allows the handler to return immediately, enabling commands to be
  // processed even when a blocking dialog (exec()) is open. The nested event
  // loop from exec() will process subsequent timer events and socket events.
  QTimer::singleShot(0, this, [this, client, cmd]() {
    // Check if client is still connected
    if (!socketToClientId_.contains(client))
    {
      if (loggingEnabled_)
      {
        qDebug() << "Command" << cmd.name << "skipped: client disconnected";
      }
      return;
    }

    if (loggingEnabled_)
    {
      qDebug() << "Executing command:" << cmd.name;
    }

    Response result;
    if (QThread::currentThread() == qApp->thread())
    {
      result = executor_->execute(cmd);
    }
    else
    {
      QMetaObject::invokeMethod(
          qApp, [&]() { result = executor_->execute(cmd); }, Qt::BlockingQueuedConnection);
    }

    // Record the command if recording
    if (recorder_->isRecording())
    {
      recorder_->recordCommand(cmd, result);
    }

    // Emit command_executed event if broadcasting is enabled
    if (broadcaster_->isEnabled() && broadcaster_->hasSubscribers("command_executed"))
    {
      QJsonObject eventData;
      eventData["command"] = cmd.name;
      eventData["params"] = cmd.params;
      eventData["success"] = result.success;
      eventData["duration_ms"] = result.durationMs;
      broadcaster_->emitEvent("command_executed", eventData);
    }

    emit responseReady(cmd.id, result.success);

    // Check again if client is still connected before sending response
    if (!socketToClientId_.contains(client))
    {
      if (loggingEnabled_)
      {
        qDebug() << "Command" << cmd.name << "completed but client disconnected";
      }
      return;
    }

    QJsonObject responseJson = result.toJson();
    responseJson["type"] = messageTypeToString(MessageType::Response);
    sendResponse(client, responseJson);

    if (loggingEnabled_)
    {
      qDebug() << "Response sent for command:" << cmd.name;
    }
  });
}

void Server::handleSubscribe(QWebSocket* client, const QJsonObject& message)
{
  QString clientId = clientIdForSocket(client);
  QString eventType = message.value("event_type").toString();

  if (eventType.isEmpty())
  {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["id"] = message.value("id").toString();
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "MISSING_PARAM" }, { "message", "Missing event_type parameter" } };
    sendResponse(client, errorResponse);
    return;
  }

  broadcaster_->subscribe(clientId, eventType);

  if (loggingEnabled_)
  {
    qDebug() << "Client" << clientId << "subscribed to" << eventType;
  }

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = QJsonObject{ { "subscribed", eventType } };
  sendResponse(client, response);
}

void Server::handleUnsubscribe(QWebSocket* client, const QJsonObject& message)
{
  QString clientId = clientIdForSocket(client);
  QString eventType = message.value("event_type").toString();

  if (eventType.isEmpty())
  {
    broadcaster_->unsubscribeAll(clientId);
    if (loggingEnabled_)
    {
      qDebug() << "Client" << clientId << "unsubscribed from all events";
    }
  }
  else
  {
    broadcaster_->unsubscribe(clientId, eventType);
    if (loggingEnabled_)
    {
      qDebug() << "Client" << clientId << "unsubscribed from" << eventType;
    }
  }

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = QJsonObject{ { "unsubscribed", eventType.isEmpty() ? "all" : eventType } };
  sendResponse(client, response);
}

void Server::handleRecordStart(QWebSocket* client, const QJsonObject& message)
{
  startRecording();

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = QJsonObject{ { "recording", true } };
  sendResponse(client, response);
}

void Server::handleRecordStop(QWebSocket* client, const QJsonObject& message)
{
  stopRecording();

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = getRecording();
  sendResponse(client, response);
}

// ========== Response/Event Sending ==========

void Server::sendResponse(QWebSocket* client, const QJsonObject& response)
{
  if (!client)
  {
    return;
  }

  QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
  client->sendTextMessage(QString::fromUtf8(data));
}

void Server::sendEvent(QWebSocket* client, const QString& eventType, const QJsonObject& data)
{
  if (!client)
  {
    return;
  }

  QJsonObject event;
  event["type"] = messageTypeToString(MessageType::Event);
  event["event_type"] = eventType;
  event["data"] = data;

  QByteArray eventData = QJsonDocument(event).toJson(QJsonDocument::Compact);
  client->sendTextMessage(QString::fromUtf8(eventData));
}

// ========== Utility Functions ==========

bool Server::isAllowedHost(const QString& remoteHost) const
{
  if (allowedHosts_.isEmpty())
  {
    return true;  // No restrictions
  }

  QString host = remoteHost;

  // Handle IPv4-mapped IPv6 addresses
  if (host.startsWith("::ffff:"))
  {
    host = host.mid(7);
  }

  return allowedHosts_.contains(host);
}

bool Server::validateApiKey(const QUrl& requestUrl) const
{
  if (apiKey_.isEmpty())
  {
    return true;  // No API key required
  }

  QUrlQuery query(requestUrl.query());
  QString token = query.queryItemValue("token");

  return token == apiKey_;
}

QString Server::clientIdForSocket(QWebSocket* socket) const
{
  return socketToClientId_.value(socket);
}

}  // namespace widgeteer
