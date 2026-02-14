#include <widgeteer/Server.h>

#include <widgeteer/ElementFinder.h>

#include <QApplication>
#include <QChildEvent>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>

namespace widgeteer {
namespace {

QJsonValue variantToJsonValue(const QVariant& value) {
  switch (value.typeId()) {
    case QMetaType::Bool:
      return value.toBool();
    case QMetaType::Int:
    case QMetaType::LongLong:
      return value.toInt();
    case QMetaType::UInt:
    case QMetaType::ULongLong:
      return static_cast<qint64>(value.toLongLong());
    case QMetaType::Float:
    case QMetaType::Double:
      return value.toDouble();
    case QMetaType::QString:
      return value.toString();
    case QMetaType::QStringList: {
      QJsonArray arr;
      for (const QString& s : value.toStringList()) {
        arr.append(s);
      }
      return arr;
    }
    default:
      return QJsonValue::fromVariant(value);
  }
}

}  // namespace

Server::Server(QObject* parent)
  : QObject(parent)
  , wsServer_(std::make_unique<QWebSocketServer>(QStringLiteral("Widgeteer"),
                                                 QWebSocketServer::NonSecureMode, this))
  , executor_(std::make_unique<CommandExecutor>())
  , recorder_(std::make_unique<ActionRecorder>(this))
  , broadcaster_(std::make_unique<EventBroadcaster>(this)) {
  // Default to localhost only
  allowedHosts_ = { "127.0.0.1", "::1", "localhost" };

  connect(wsServer_.get(), &QWebSocketServer::newConnection, this, &Server::onNewConnection);

  // Connect event broadcaster to send events to clients
  connect(broadcaster_.get(), &EventBroadcaster::eventReady, this, &Server::onEventReady);
}

Server::~Server() {
  stop();
}

bool Server::start(quint16 port) {
  if (running_) {
    return true;
  }

  port_ = port;

  if (!wsServer_->listen(QHostAddress::Any, port_)) {
    emit serverError(QStringLiteral("Failed to start server on port %1: %2")
                         .arg(port_)
                         .arg(wsServer_->errorString()));
    return false;
  }

  port_ = wsServer_->serverPort();
  running_ = true;

  if (loggingEnabled_) {
    qDebug() << "Widgeteer WebSocket server started on port" << port_;
  }

  emit serverStarted(port_);
  return true;
}

void Server::stop() {
  if (!running_) {
    return;
  }

  // Close all client connections
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it.value().socket) {
      it.value().socket->close();
    }
  }
  clients_.clear();
  socketToClientId_.clear();

  wsServer_->close();
  running_ = false;

  if (loggingEnabled_) {
    qDebug() << "Widgeteer server stopped";
  }

  emit serverStopped();
}

bool Server::isRunning() const {
  return running_;
}

void Server::setPort(quint16 port) {
  port_ = port;
}

quint16 Server::port() const {
  return port_;
}

void Server::setAllowedHosts(const QStringList& hosts) {
  allowedHosts_ = hosts;
}

void Server::enableLogging(bool enable) {
  loggingEnabled_ = enable;
}

void Server::setApiKey(const QString& apiKey) {
  apiKey_ = apiKey;
}

QString Server::apiKey() const {
  return apiKey_;
}

void Server::setRootWidget(QWidget* root) {
  rootWidget_ = root;
}

// ========== Recording API ==========

void Server::startRecording() {
  recorder_->start();
  if (loggingEnabled_) {
    qDebug() << "Recording started";
  }
}

void Server::stopRecording() {
  recorder_->stop();
  if (loggingEnabled_) {
    qDebug() << "Recording stopped, actions recorded:" << recorder_->actionCount();
  }
}

bool Server::isRecording() const {
  return recorder_->isRecording();
}

QJsonObject Server::getRecording() const {
  return recorder_->getRecording();
}

// ========== Event Broadcasting API ==========

void Server::setEventBroadcastingEnabled(bool enabled) {
  broadcaster_->setEnabled(enabled);
  updateUiEventTrackingState();
}

bool Server::isEventBroadcastingEnabled() const {
  return broadcaster_->isEnabled();
}

// ========== Extensibility API Implementation ==========

void Server::registerObject(const QString& name, QObject* object) {
  if (!object) {
    qWarning() << "Widgeteer: Cannot register null object with name:" << name;
    return;
  }

  registeredObjects_[name] = object;

  // Update executor with the new registration
  executor_->setRegisteredObjects(&registeredObjects_);

  if (loggingEnabled_) {
    qDebug() << "Widgeteer: Registered object:" << name << "->"
             << object->metaObject()->className();
  }
}

void Server::unregisterObject(const QString& name) {
  registeredObjects_.remove(name);

  if (loggingEnabled_) {
    qDebug() << "Widgeteer: Unregistered object:" << name;
  }
}

void Server::registerCommand(const QString& name, CommandHandler handler) {
  if (!handler) {
    qWarning() << "Widgeteer: Cannot register null handler for command:" << name;
    return;
  }

  // Check for conflicts with built-in commands
  static const QSet<QString> builtinCommands = { "get_tree",
                                                 "find",
                                                 "describe",
                                                 "get_property",
                                                 "list_properties",
                                                 "get_actions",
                                                 "get_form_fields",
                                                 "list_windows",
                                                 "click",
                                                 "double_click",
                                                 "right_click",
                                                 "type",
                                                 "key",
                                                 "key_sequence",
                                                 "drag",
                                                 "scroll",
                                                 "hover",
                                                 "focus",
                                                 "set_property",
                                                 "invoke",
                                                 "set_value",
                                                 "screenshot",
                                                 "assert",
                                                 "exists",
                                                 "is_visible",
                                                 "wait",
                                                 "wait_idle",
                                                 "wait_signal",
                                                 "sleep",
                                                 "quit",
                                                 "accept_dialog",
                                                 "reject_dialog",
                                                 "close_window",
                                                 "is_dialog_open",
                                                 "call",
                                                 "list_objects",
                                                 "list_custom_commands" };

  if (builtinCommands.contains(name)) {
    qWarning() << "Widgeteer: Cannot override built-in command:" << name;
    return;
  }

  customCommands_[name] = handler;

  // Update executor with the new registration
  executor_->setCustomCommands(&customCommands_);

  if (loggingEnabled_) {
    qDebug() << "Widgeteer: Registered custom command:" << name;
  }
}

void Server::unregisterCommand(const QString& name) {
  customCommands_.remove(name);

  if (loggingEnabled_) {
    qDebug() << "Widgeteer: Unregistered custom command:" << name;
  }
}

QStringList Server::registeredObjects() const {
  return registeredObjects_.keys();
}

QStringList Server::registeredCommands() const {
  return customCommands_.keys();
}

// ========== WebSocket Connection Handling ==========

void Server::onNewConnection() {
  while (wsServer_->hasPendingConnections()) {
    QWebSocket* socket = wsServer_->nextPendingConnection();

    // Check allowed hosts
    QString remoteHost = socket->peerAddress().toString();
    if (!isAllowedHost(remoteHost)) {
      if (loggingEnabled_) {
        qDebug() << "Rejected connection from disallowed host:" << remoteHost;
      }
      socket->close(QWebSocketProtocol::CloseCodePolicyViolated, "Forbidden");
      socket->deleteLater();
      continue;
    }

    // Validate API key if set
    if (!apiKey_.isEmpty() && !validateApiKey(socket->requestUrl())) {
      if (loggingEnabled_) {
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

    if (loggingEnabled_) {
      qDebug() << "Client connected:" << clientId << "from" << remoteHost;
    }

    emit clientConnected(clientId);
  }
}

void Server::onTextMessageReceived(const QString& message) {
  QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket) {
    return;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError) {
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

void Server::onClientDisconnected() {
  QWebSocket* socket = qobject_cast<QWebSocket*>(sender());
  if (!socket) {
    return;
  }

  QString clientId = socketToClientId_.value(socket);

  if (!clientId.isEmpty()) {
    // Remove from event broadcaster subscriptions
    broadcaster_->removeClient(clientId);
    updateUiEventTrackingState();

    clients_.remove(clientId);
    socketToClientId_.remove(socket);

    if (loggingEnabled_) {
      qDebug() << "Client disconnected:" << clientId;
    }

    emit clientDisconnected(clientId);
  }

  socket->deleteLater();
}

void Server::onEventReady(const QString& eventType, const QJsonObject& data,
                          const QStringList& recipientClientIds) {
  for (const QString& clientId : recipientClientIds) {
    if (clients_.contains(clientId)) {
      QWebSocket* socket = clients_[clientId].socket;
      if (socket) {
        sendEvent(socket, eventType, data);
      }
    }
  }
}

void Server::onFocusChanged(QWidget* oldWidget, QWidget* newWidget) {
  if (!broadcaster_->isEnabled() || !broadcaster_->hasSubscribers("focus_changed")) {
    return;
  }

  ElementFinder finder;

  QJsonObject eventData;
  eventData["oldPath"] = oldWidget ? finder.pathFor(oldWidget) : QString();
  eventData["newPath"] = newWidget ? finder.pathFor(newWidget) : QString();
  eventData["oldObjectName"] = oldWidget ? oldWidget->objectName() : QString();
  eventData["newObjectName"] = newWidget ? newWidget->objectName() : QString();

  broadcaster_->emitEvent("focus_changed", eventData);
}

void Server::onPropertyPollTimeout() {
  if (!broadcaster_->isEnabled() || !broadcaster_->hasSubscribers("property_changed")) {
    return;
  }

  ElementFinder finder;

  for (PropertyWatch& watch : propertyWatches_) {
    auto findResult = finder.find(watch.selector);
    QWidget* widget = findResult.widget;

    if (widget != watch.widget) {
      watch.widget = widget;
      watch.initialized = false;
    }

    if (!watch.widget) {
      continue;
    }

    QVariant currentValue = watch.widget->property(watch.property.toUtf8().constData());
    if (!currentValue.isValid()) {
      continue;
    }

    if (!watch.initialized) {
      watch.lastValue = currentValue;
      watch.initialized = true;
      continue;
    }

    if (currentValue == watch.lastValue) {
      continue;
    }

    QJsonObject eventData;
    eventData["path"] = finder.pathFor(watch.widget);
    eventData["objectName"] = watch.widget->objectName();
    eventData["class"] = QString::fromLatin1(watch.widget->metaObject()->className());
    eventData["property"] = watch.property;
    eventData["old"] = variantToJsonValue(watch.lastValue);
    eventData["new"] = variantToJsonValue(currentValue);

    watch.lastValue = currentValue;
    broadcaster_->emitEvent("property_changed", eventData);
  }
}

// ========== Message Handling ==========

void Server::handleMessage(QWebSocket* client, const QJsonObject& message) {
  // Transaction requests are sent without a "type" field and are routed directly.
  // This also supports clients that include both {"type":"command","transaction":true}.
  if (message.value("transaction").toBool(false)) {
    handleTransaction(client, message);
    return;
  }

  QString typeStr = message.value("type").toString();
  auto type = stringToMessageType(typeStr);

  if (!type.has_value()) {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "INVALID_TYPE" },
                     { "message", QStringLiteral("Unknown message type: %1").arg(typeStr) } };
    sendResponse(client, errorResponse);
    return;
  }

  switch (type.value()) {
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

void Server::handleCommand(QWebSocket* client, const QJsonObject& message) {
  Command cmd = Command::fromJson(message);
  emit requestReceived(cmd.id, cmd.name);

  if (loggingEnabled_) {
    qDebug() << "Command:" << cmd.name << "Target:" << cmd.params.value("target").toString();
  }

  // Schedule command execution asynchronously.
  // This allows the handler to return immediately, enabling commands to be
  // processed even when a blocking dialog (exec()) is open. The nested event
  // loop from exec() will process subsequent timer events and socket events.
  QTimer::singleShot(0, this, [this, client, cmd]() {
    // Check if client is still connected
    if (!socketToClientId_.contains(client)) {
      if (loggingEnabled_) {
        qDebug() << "Command" << cmd.name << "skipped: client disconnected";
      }
      return;
    }

    if (loggingEnabled_) {
      qDebug() << "Executing command:" << cmd.name;
    }

    Response result;
    if (QThread::currentThread() == qApp->thread()) {
      result = executor_->execute(cmd);
    } else {
      QMetaObject::invokeMethod(
          qApp, [&]() { result = executor_->execute(cmd); }, Qt::BlockingQueuedConnection);
    }

    // Record the command if recording
    if (recorder_->isRecording()) {
      recorder_->recordCommand(cmd, result);
    }

    // Emit command_executed event if broadcasting is enabled
    if (broadcaster_->isEnabled() && broadcaster_->hasSubscribers("command_executed")) {
      QJsonObject eventData;
      eventData["command"] = cmd.name;
      eventData["params"] = cmd.params;
      eventData["success"] = result.success;
      eventData["duration_ms"] = result.durationMs;
      broadcaster_->emitEvent("command_executed", eventData);
    }

    emit responseReady(cmd.id, result.success);

    // Check again if client is still connected before sending response
    if (!socketToClientId_.contains(client)) {
      if (loggingEnabled_) {
        qDebug() << "Command" << cmd.name << "completed but client disconnected";
      }
      return;
    }

    QJsonObject responseJson = result.toJson();
    responseJson["type"] = messageTypeToString(MessageType::Response);
    sendResponse(client, responseJson);

    if (loggingEnabled_) {
      qDebug() << "Response sent for command:" << cmd.name;
    }
  });
}

void Server::handleTransaction(QWebSocket* client, const QJsonObject& message) {
  Transaction tx = Transaction::fromJson(message);
  emit requestReceived(tx.id, "transaction");

  if (loggingEnabled_) {
    qDebug() << "Transaction:" << tx.id << "steps:" << tx.steps.size();
  }

  // Schedule transaction execution asynchronously for consistency with command handling.
  QTimer::singleShot(0, this, [this, client, tx]() {
    if (!socketToClientId_.contains(client)) {
      if (loggingEnabled_) {
        qDebug() << "Transaction" << tx.id << "skipped: client disconnected";
      }
      return;
    }

    TransactionResponse result;
    if (QThread::currentThread() == qApp->thread()) {
      result = executor_->execute(tx);
    } else {
      QMetaObject::invokeMethod(
          qApp, [&]() { result = executor_->execute(tx); }, Qt::BlockingQueuedConnection);
    }

    if (broadcaster_->isEnabled() && broadcaster_->hasSubscribers("command_executed")) {
      QJsonObject eventData;
      eventData["command"] = "transaction";
      eventData["steps"] = tx.steps.size();
      eventData["success"] = result.success;
      eventData["completed_steps"] = result.completedSteps;
      broadcaster_->emitEvent("command_executed", eventData);
    }

    emit responseReady(tx.id, result.success);

    if (!socketToClientId_.contains(client)) {
      if (loggingEnabled_) {
        qDebug() << "Transaction" << tx.id << "completed but client disconnected";
      }
      return;
    }

    QJsonObject responseJson = result.toJson();
    responseJson["type"] = messageTypeToString(MessageType::Response);
    sendResponse(client, responseJson);
  });
}

void Server::handleSubscribe(QWebSocket* client, const QJsonObject& message) {
  QString clientId = clientIdForSocket(client);
  QString eventType = message.value("event_type").toString();
  QJsonObject filter = message.value("filter").toObject();

  if (eventType.isEmpty()) {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["id"] = message.value("id").toString();
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "MISSING_PARAM" }, { "message", "Missing event_type parameter" } };
    sendResponse(client, errorResponse);
    return;
  }

  static const QStringList supportedEventList = EventBroadcaster::availableEventTypes();
  static QSet<QString> supportedEvents;
  if (supportedEvents.isEmpty()) {
    for (const QString& evt : supportedEventList) {
      supportedEvents.insert(evt);
    }
  }
  if (!supportedEvents.contains(eventType)) {
    QJsonObject errorResponse;
    errorResponse["type"] = messageTypeToString(MessageType::Response);
    errorResponse["id"] = message.value("id").toString();
    errorResponse["success"] = false;
    errorResponse["error"] =
        QJsonObject{ { "code", "INVALID_PARAMS" },
                     { "message", QStringLiteral("Unsupported event_type: %1").arg(eventType) } };
    sendResponse(client, errorResponse);
    return;
  }

  if (!filter.isEmpty()) {
    const bool hasTarget = filter.contains("target");
    const bool hasProperty = filter.contains("property");
    if (eventType == "property_changed") {
      if (!hasTarget || !hasProperty || !filter.value("target").isString() ||
          !filter.value("property").isString()) {
        QJsonObject errorResponse;
        errorResponse["type"] = messageTypeToString(MessageType::Response);
        errorResponse["id"] = message.value("id").toString();
        errorResponse["success"] = false;
        errorResponse["error"] = QJsonObject{
          { "code", "INVALID_PARAMS" },
          { "message", "property_changed subscriptions require filter.target and filter.property" }
        };
        sendResponse(client, errorResponse);
        return;
      }
    } else if (hasProperty) {
      QJsonObject errorResponse;
      errorResponse["type"] = messageTypeToString(MessageType::Response);
      errorResponse["id"] = message.value("id").toString();
      errorResponse["success"] = false;
      errorResponse["error"] =
          QJsonObject{ { "code", "INVALID_PARAMS" },
                       { "message",
                         "filter.property is only valid for property_changed subscriptions" } };
      sendResponse(client, errorResponse);
      return;
    }
  }

  broadcaster_->subscribe(clientId, eventType, filter);
  updateUiEventTrackingState();

  if (loggingEnabled_) {
    qDebug() << "Client" << clientId << "subscribed to" << eventType << "filter" << filter;
  }

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = QJsonObject{ { "subscribed", eventType } };
  sendResponse(client, response);
}

void Server::handleUnsubscribe(QWebSocket* client, const QJsonObject& message) {
  QString clientId = clientIdForSocket(client);
  QString eventType = message.value("event_type").toString();

  if (eventType.isEmpty()) {
    broadcaster_->unsubscribeAll(clientId);
    updateUiEventTrackingState();
    if (loggingEnabled_) {
      qDebug() << "Client" << clientId << "unsubscribed from all events";
    }
  } else {
    broadcaster_->unsubscribe(clientId, eventType);
    updateUiEventTrackingState();
    if (loggingEnabled_) {
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

bool Server::eventFilter(QObject* watched, QEvent* event) {
  if (!uiEventTrackingActive_) {
    return QObject::eventFilter(watched, event);
  }

  if (event->type() == QEvent::ChildAdded) {
    auto* childEvent = static_cast<QChildEvent*>(event);
    QObject* child = childEvent ? childEvent->child() : nullptr;
    if (auto* childWidget = qobject_cast<QWidget*>(child)) {
      registerWidgetLifecycle(childWidget);

      if (broadcaster_->isEnabled() && broadcaster_->hasSubscribers("widget_created")) {
        ElementFinder finder;
        QJsonObject eventData;
        eventData["path"] = finder.pathFor(childWidget);
        eventData["objectName"] = childWidget->objectName();
        eventData["class"] = QString::fromLatin1(childWidget->metaObject()->className());
        if (auto* parentWidget = qobject_cast<QWidget*>(watched)) {
          eventData["parentPath"] = finder.pathFor(parentWidget);
        }
        broadcaster_->emitEvent("widget_created", eventData);
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

void Server::updateUiEventTrackingState() {
  const bool needsCoreTracking =
      broadcaster_->isEnabled() && (broadcaster_->hasSubscribers("widget_created") ||
                                    broadcaster_->hasSubscribers("widget_destroyed") ||
                                    broadcaster_->hasSubscribers("focus_changed") ||
                                    broadcaster_->hasSubscribers("property_changed"));

  if (needsCoreTracking && !uiEventTrackingActive_) {
    uiEventTrackingActive_ = true;
    qApp->installEventFilter(this);
    focusChangedConnection_ =
        connect(qApp, &QApplication::focusChanged, this, &Server::onFocusChanged);

    // Track existing widgets for lifecycle events.
    for (QWidget* topLevel : QApplication::topLevelWidgets()) {
      registerWidgetLifecycle(topLevel);
      const QList<QWidget*> children = topLevel->findChildren<QWidget*>();
      for (QWidget* child : children) {
        registerWidgetLifecycle(child);
      }
    }
  } else if (!needsCoreTracking && uiEventTrackingActive_) {
    uiEventTrackingActive_ = false;
    qApp->removeEventFilter(this);
    if (focusChangedConnection_) {
      disconnect(focusChangedConnection_);
      focusChangedConnection_ = QMetaObject::Connection();
    }
    lifecycleTrackedWidgets_.clear();
  }

  if (broadcaster_->isEnabled() && broadcaster_->hasSubscribers("property_changed")) {
    refreshPropertyWatches();
    if (!propertyPollTimer_.isActive()) {
      propertyPollTimer_.setInterval(100);
      connect(&propertyPollTimer_, &QTimer::timeout, this, &Server::onPropertyPollTimeout,
              Qt::UniqueConnection);
      propertyPollTimer_.start();
    }
  } else {
    propertyPollTimer_.stop();
    propertyWatches_.clear();
  }
}

void Server::refreshPropertyWatches() {
  QList<PropertyWatch> watches;
  const QList<QJsonObject> filters = broadcaster_->filtersForEvent("property_changed");
  QSet<QString> dedupe;

  for (const QJsonObject& filter : filters) {
    const QString selector = filter.value("target").toString();
    const QString property = filter.value("property").toString();
    if (selector.isEmpty() || property.isEmpty()) {
      continue;
    }
    const QString key = selector + "|" + property;
    if (dedupe.contains(key)) {
      continue;
    }
    dedupe.insert(key);
    watches.append(PropertyWatch{ selector, property, nullptr, QVariant(), false });
  }

  propertyWatches_ = watches;
}

void Server::registerWidgetLifecycle(QObject* object) {
  if (!object || lifecycleTrackedWidgets_.contains(object)) {
    return;
  }

  auto* widget = qobject_cast<QWidget*>(object);
  if (!widget) {
    return;
  }

  lifecycleTrackedWidgets_.insert(widget);

  ElementFinder finder;
  const QString path = finder.pathFor(widget);
  const QString objectName = widget->objectName();
  const QString className = QString::fromLatin1(widget->metaObject()->className());

  connect(widget, &QObject::destroyed, this, [this, widget, path, objectName, className]() {
    lifecycleTrackedWidgets_.remove(widget);
    if (!broadcaster_->isEnabled() || !broadcaster_->hasSubscribers("widget_destroyed")) {
      return;
    }

    QJsonObject eventData;
    eventData["path"] = path;
    eventData["objectName"] = objectName;
    eventData["class"] = className;
    broadcaster_->emitEvent("widget_destroyed", eventData);
  });
}

void Server::handleRecordStart(QWebSocket* client, const QJsonObject& message) {
  startRecording();

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = QJsonObject{ { "recording", true } };
  sendResponse(client, response);
}

void Server::handleRecordStop(QWebSocket* client, const QJsonObject& message) {
  stopRecording();

  QJsonObject response;
  response["type"] = messageTypeToString(MessageType::Response);
  response["id"] = message.value("id").toString();
  response["success"] = true;
  response["result"] = getRecording();
  sendResponse(client, response);
}

// ========== Response/Event Sending ==========

void Server::sendResponse(QWebSocket* client, const QJsonObject& response) {
  if (!client) {
    return;
  }

  QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
  client->sendTextMessage(QString::fromUtf8(data));
}

void Server::sendEvent(QWebSocket* client, const QString& eventType, const QJsonObject& data) {
  if (!client) {
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

bool Server::isAllowedHost(const QString& remoteHost) const {
  if (allowedHosts_.isEmpty()) {
    return true;  // No restrictions
  }

  QString host = remoteHost;

  // Handle IPv4-mapped IPv6 addresses
  if (host.startsWith("::ffff:")) {
    host = host.mid(7);
  }

  return allowedHosts_.contains(host);
}

bool Server::validateApiKey(const QUrl& requestUrl) const {
  if (apiKey_.isEmpty()) {
    return true;  // No API key required
  }

  QUrlQuery query(requestUrl.query());
  QString token = query.queryItemValue("token");

  return token == apiKey_;
}

QString Server::clientIdForSocket(QWebSocket* socket) const {
  return socketToClientId_.value(socket);
}

}  // namespace widgeteer
