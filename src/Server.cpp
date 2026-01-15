#include <widgeteer/Server.h>

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QThread>
#include <QUrlQuery>

namespace widgeteer
{

Server::Server(QObject* parent)
  : QObject(parent)
  , server_(std::make_unique<QTcpServer>(this))
  , executor_(std::make_unique<CommandExecutor>())
{
  // Default to localhost only
  allowedHosts_ = { "127.0.0.1", "::1", "localhost" };

  connect(server_.get(), &QTcpServer::newConnection, this, &Server::onNewConnection);
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

  if (!server_->listen(QHostAddress::Any, port_))
  {
    emit serverError(QStringLiteral("Failed to start server on port %1: %2")
                         .arg(port_)
                         .arg(server_->errorString()));
    return false;
  }

  port_ = server_->serverPort();
  running_ = true;

  if (loggingEnabled_)
  {
    qDebug() << "Widgeteer server started on port" << port_;
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

  server_->close();
  requestBuffers_.clear();
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

void Server::enableCORS(bool enable)
{
  corsEnabled_ = enable;
}

void Server::enableLogging(bool enable)
{
  loggingEnabled_ = enable;
}

void Server::setRootWidget(QWidget* root)
{
  rootWidget_ = root;
}

void Server::onNewConnection()
{
  while (server_->hasPendingConnections())
  {
    QTcpSocket* client = server_->nextPendingConnection();
    requestBuffers_[client] = QByteArray();

    connect(client, &QTcpSocket::readyRead, this, &Server::onClientReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);
  }
}

void Server::onClientReadyRead()
{
  QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
  if (!client)
  {
    return;
  }

  requestBuffers_[client].append(client->readAll());

  // Check if we have a complete HTTP request
  QByteArray& buffer = requestBuffers_[client];

  // Look for end of headers
  int headerEnd = buffer.indexOf("\r\n\r\n");
  if (headerEnd == -1)
  {
    return;  // Headers not complete yet
  }

  // Parse headers to check for Content-Length
  QString headers = QString::fromUtf8(buffer.left(headerEnd));
  int contentLength = 0;

  static QRegularExpression contentLengthRegex("Content-Length:\\s*(\\d+)",
                                               QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatch match = contentLengthRegex.match(headers);
  if (match.hasMatch())
  {
    contentLength = match.captured(1).toInt();
  }

  // Check if we have the complete body
  int bodyStart = headerEnd + 4;
  if (buffer.size() < bodyStart + contentLength)
  {
    return;  // Body not complete yet
  }

  // Check allowed hosts
  QString remoteHost = client->peerAddress().toString();
  if (!isAllowedHost(remoteHost))
  {
    QByteArray response = buildResponse(403, "Forbidden", R"({"error":"Forbidden"})");
    client->write(response);
    client->disconnectFromHost();
    return;
  }

  // Parse and handle request
  HttpRequest req = parseRequest(buffer);
  QByteArray responseBody = handleRequest(req);

  // Clear buffer
  buffer.clear();

  // Send response
  client->write(responseBody);
  client->flush();
  client->disconnectFromHost();
}

void Server::onClientDisconnected()
{
  QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
  if (client)
  {
    requestBuffers_.remove(client);
    client->deleteLater();
  }
}

Server::HttpRequest Server::parseRequest(const QByteArray& data)
{
  HttpRequest req;

  int headerEnd = data.indexOf("\r\n\r\n");
  QString headerSection = QString::fromUtf8(data.left(headerEnd));
  QStringList lines = headerSection.split("\r\n");

  if (lines.isEmpty())
  {
    return req;
  }

  // Parse request line: METHOD /path HTTP/1.1
  QStringList requestLine = lines.first().split(' ');
  if (requestLine.size() >= 2)
  {
    req.method = requestLine[0];
    QString fullPath = requestLine[1];

    // Parse path and query params
    int queryStart = fullPath.indexOf('?');
    if (queryStart != -1)
    {
      req.path = fullPath.left(queryStart);
      QString queryString = fullPath.mid(queryStart + 1);
      QUrlQuery query(queryString);
      for (const auto& item : query.queryItems())
      {
        req.queryParams[item.first] = item.second;
      }
    }
    else
    {
      req.path = fullPath;
    }
  }

  // Parse headers
  for (int i = 1; i < lines.size(); ++i)
  {
    int colonPos = lines[i].indexOf(':');
    if (colonPos != -1)
    {
      QString key = lines[i].left(colonPos).trimmed();
      QString value = lines[i].mid(colonPos + 1).trimmed();
      req.headers[key] = value;
    }
  }

  // Extract body
  int bodyStart = headerEnd + 4;
  if (bodyStart < data.size())
  {
    req.body = data.mid(bodyStart);
  }

  return req;
}

QByteArray Server::buildResponse(int statusCode, const QString& statusText, const QByteArray& body,
                                 const QString& contentType)
{
  QByteArray response;
  response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
  response.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
  response.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());
  response.append("Connection: close\r\n");

  if (corsEnabled_)
  {
    response.append("Access-Control-Allow-Origin: *\r\n");
    response.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
    response.append("Access-Control-Allow-Headers: Content-Type\r\n");
  }

  response.append("\r\n");
  response.append(body);

  return response;
}

QByteArray Server::handleRequest(const HttpRequest& req)
{
  if (loggingEnabled_)
  {
    qDebug() << "Request:" << req.method << req.path;
  }

  // Handle CORS preflight
  if (req.method == "OPTIONS")
  {
    return buildResponse(200, "OK", "");
  }

  // Route requests
  if (req.path == "/health" && req.method == "GET")
  {
    return handleHealth();
  }
  if (req.path == "/schema" && req.method == "GET")
  {
    return handleSchema();
  }
  if (req.path == "/tree" && req.method == "GET")
  {
    return handleTree(req);
  }
  if (req.path == "/screenshot" && req.method == "GET")
  {
    return handleScreenshot(req);
  }
  if (req.path == "/command" && req.method == "POST")
  {
    return handleCommand(req);
  }
  if (req.path == "/transaction" && req.method == "POST")
  {
    return handleTransaction(req);
  }

  return errorResponse("Not Found", 404);
}

QByteArray Server::handleHealth()
{
  QJsonObject response;
  response["status"] = "ok";
  response["version"] = "0.1.0";

  return jsonResponse(response);
}

QByteArray Server::handleSchema()
{
  QJsonObject schema;

  // Introspection commands
  QJsonArray introspection;
  introspection.append(QJsonObject{
      { "name", "get_tree" },
      { "description", "Get the widget tree as JSON" },
      { "params", QJsonObject{ { "root", "optional target selector" },
                               { "depth", "max depth (-1 for unlimited)" },
                               { "include_invisible", "include invisible widgets" },
                               { "include_geometry", "include geometry info" },
                               { "include_properties", "include all properties" } } } });
  introspection.append(
      QJsonObject{ { "name", "find" },
                   { "description", "Find widgets matching a query" },
                   { "params", QJsonObject{ { "query", "selector query" },
                                            { "max_results", "limit results" } } } });
  introspection.append(QJsonObject{ { "name", "describe" },
                                    { "description", "Get detailed info about a widget" },
                                    { "params", QJsonObject{ { "target", "selector" } } } });
  introspection.append(QJsonObject{
      { "name", "get_property" },
      { "description", "Get a property value" },
      { "params", QJsonObject{ { "target", "selector" }, { "property", "property name" } } } });
  introspection.append(QJsonObject{ { "name", "list_properties" },
                                    { "description", "List all properties of a widget" },
                                    { "params", QJsonObject{ { "target", "selector" } } } });
  introspection.append(QJsonObject{ { "name", "get_actions" },
                                    { "description", "List available actions/slots" },
                                    { "params", QJsonObject{ { "target", "selector" } } } });
  schema["introspection"] = introspection;

  // Action commands
  QJsonArray actions;
  actions.append(
      QJsonObject{ { "name", "click" },
                   { "description", "Click on a widget" },
                   { "params", QJsonObject{ { "target", "selector" },
                                            { "button", "left/right/middle (default: left)" },
                                            { "pos", "optional {x, y} within widget" } } } });
  actions.append(QJsonObject{ { "name", "double_click" },
                              { "description", "Double-click on a widget" },
                              { "params", QJsonObject{ { "target", "selector" } } } });
  actions.append(QJsonObject{ { "name", "right_click" },
                              { "description", "Right-click on a widget" },
                              { "params", QJsonObject{ { "target", "selector" } } } });
  actions.append(QJsonObject{
      { "name", "type" },
      { "description", "Type text into a widget" },
      { "params",
        QJsonObject{ { "target", "selector" },
                     { "text", "text to type" },
                     { "clear_first", "clear existing text first (default: false)" } } } });
  actions.append(QJsonObject{
      { "name", "key" },
      { "description", "Press a single key" },
      { "params",
        QJsonObject{ { "target", "selector" },
                     { "key", "key name (e.g., 'Enter', 'Escape', 'Tab')" },
                     { "modifiers", "array of modifiers ['ctrl', 'shift', 'alt']" } } } });
  actions.append(
      QJsonObject{ { "name", "key_sequence" },
                   { "description", "Press a key sequence" },
                   { "params", QJsonObject{ { "target", "selector" },
                                            { "sequence", "e.g., 'Ctrl+Shift+S'" } } } });
  actions.append(QJsonObject{ { "name", "drag" },
                              { "description", "Drag from one widget to another" },
                              { "params", QJsonObject{ { "from", "source selector" },
                                                       { "to", "destination selector" },
                                                       { "from_pos", "optional start position" },
                                                       { "to_pos", "optional end position" } } } });
  actions.append(QJsonObject{ { "name", "scroll" },
                              { "description", "Scroll a widget" },
                              { "params", QJsonObject{ { "target", "selector" },
                                                       { "delta_x", "horizontal" },
                                                       { "delta_y", "vertical" } } } });
  actions.append(QJsonObject{ { "name", "hover" },
                              { "description", "Move mouse over widget" },
                              { "params", QJsonObject{ { "target", "selector" } } } });
  actions.append(QJsonObject{ { "name", "focus" },
                              { "description", "Set keyboard focus" },
                              { "params", QJsonObject{ { "target", "selector" } } } });
  schema["actions"] = actions;

  // State commands
  QJsonArray state;
  state.append(QJsonObject{ { "name", "set_property" },
                            { "description", "Set a property value" },
                            { "params", QJsonObject{ { "target", "selector" },
                                                     { "property", "property name" },
                                                     { "value", "new value" } } } });
  state.append(QJsonObject{
      { "name", "invoke" },
      { "description", "Invoke a method/slot" },
      { "params", QJsonObject{ { "target", "selector" }, { "method", "method name" } } } });
  state.append(QJsonObject{
      { "name", "set_value" },
      { "description", "Smart value setter for common widgets" },
      { "params", QJsonObject{ { "target", "selector" }, { "value", "value to set" } } } });
  schema["state"] = state;

  // Verification commands
  QJsonArray verification;
  verification.append(QJsonObject{
      { "name", "screenshot" },
      { "description", "Capture screenshot" },
      { "params", QJsonObject{ { "target", "optional selector" }, { "format", "png or jpg" } } } });
  verification.append(
      QJsonObject{ { "name", "assert" },
                   { "description", "Assert a property condition" },
                   { "params", QJsonObject{ { "target", "selector" },
                                            { "property", "property name" },
                                            { "operator", "==, !=, >, <, >=, <=, contains" },
                                            { "value", "expected value" } } } });
  verification.append(QJsonObject{ { "name", "exists" },
                                   { "description", "Check if element exists" },
                                   { "params", QJsonObject{ { "target", "selector" } } } });
  verification.append(QJsonObject{ { "name", "is_visible" },
                                   { "description", "Check if element is visible" },
                                   { "params", QJsonObject{ { "target", "selector" } } } });
  schema["verification"] = verification;

  // Synchronization commands
  QJsonArray sync;
  sync.append(QJsonObject{
      { "name", "wait" },
      { "description", "Wait for a condition" },
      { "params",
        QJsonObject{ { "target", "selector" },
                     { "condition", "exists, not_exists, visible, not_visible, enabled, disabled, "
                                    "stable, property:name=value" },
                     { "timeout_ms", "timeout in milliseconds (default: 5000)" } } } });
  sync.append(QJsonObject{ { "name", "wait_idle" },
                           { "description", "Wait for Qt event queue to be empty" },
                           { "params", QJsonObject{ { "timeout_ms", "timeout" } } } });
  sync.append(QJsonObject{ { "name", "wait_signal" },
                           { "description", "Wait for a Qt signal" },
                           { "params", QJsonObject{ { "target", "selector" },
                                                    { "signal", "signal signature" },
                                                    { "timeout_ms", "timeout" } } } });
  sync.append(QJsonObject{ { "name", "sleep" },
                           { "description", "Hard delay" },
                           { "params", QJsonObject{ { "ms", "milliseconds" } } } });
  schema["synchronization"] = sync;

  // Selector syntax
  QJsonObject selectors;
  selectors["path"] = "parent/child/grandchild - slash-separated path";
  selectors["@name:"] = "@name:objectName - by objectName";
  selectors["@class:"] = "@class:QPushButton - by class name";
  selectors["@text:"] = "@text:OK - by text content";
  selectors["@accessible:"] = "@accessible:Name - by accessible name";
  selectors["index"] = "parent/child[0] - index for duplicates";
  selectors["wildcard"] = "parent/*/child - wildcard matching";
  schema["selectors"] = selectors;

  return jsonResponse(schema);
}

QByteArray Server::handleTree(const HttpRequest& req)
{
  emit requestReceived("tree", "get_tree");

  QJsonObject params;
  if (req.queryParams.contains("root"))
  {
    params["root"] = req.queryParams.value("root");
  }
  if (req.queryParams.contains("depth"))
  {
    params["depth"] = req.queryParams.value("depth").toInt();
  }
  if (req.queryParams.contains("include_invisible"))
  {
    params["include_invisible"] = req.queryParams.value("include_invisible") == "true";
  }

  Command cmd;
  cmd.id = "tree";
  cmd.name = "get_tree";
  cmd.params = params;

  QJsonObject result =
      executeOnMainThread([this, &cmd]() { return executor_->execute(cmd).toJson(); });

  emit responseReady("tree", result.value("success").toBool(true));

  return jsonResponse(result);
}

QByteArray Server::handleScreenshot(const HttpRequest& req)
{
  emit requestReceived("screenshot", "screenshot");

  QJsonObject params;
  if (req.queryParams.contains("target"))
  {
    params["target"] = req.queryParams.value("target");
  }
  if (req.queryParams.contains("format"))
  {
    params["format"] = req.queryParams.value("format");
  }

  Command cmd;
  cmd.id = "screenshot";
  cmd.name = "screenshot";
  cmd.params = params;

  QJsonObject result =
      executeOnMainThread([this, &cmd]() { return executor_->execute(cmd).toJson(); });

  emit responseReady("screenshot", result.value("success").toBool(true));

  return jsonResponse(result);
}

QByteArray Server::handleCommand(const HttpRequest& req)
{
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseError);
  if (parseError.error != QJsonParseError::NoError)
  {
    return errorResponse(QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()), 400);
  }

  Command cmd = Command::fromJson(doc.object());
  emit requestReceived(cmd.id, cmd.name);

  if (loggingEnabled_)
  {
    qDebug() << "Command:" << cmd.name << "Target:" << cmd.params.value("target").toString();
  }

  QJsonObject result =
      executeOnMainThread([this, &cmd]() { return executor_->execute(cmd).toJson(); });

  emit responseReady(cmd.id, result.value("success").toBool(false));

  return jsonResponse(result);
}

QByteArray Server::handleTransaction(const HttpRequest& req)
{
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(req.body, &parseError);
  if (parseError.error != QJsonParseError::NoError)
  {
    return errorResponse(QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()), 400);
  }

  Transaction tx = Transaction::fromJson(doc.object());
  emit requestReceived(tx.id, "transaction");

  if (loggingEnabled_)
  {
    qDebug() << "Transaction:" << tx.id << "Steps:" << tx.steps.size();
  }

  QJsonObject result =
      executeOnMainThread([this, &tx]() { return executor_->execute(tx).toJson(); });

  emit responseReady(tx.id, result.value("success").toBool(false));

  return jsonResponse(result);
}

template <typename Func>
QJsonObject Server::executeOnMainThread(Func&& func)
{
  QJsonObject result;

  if (QThread::currentThread() == qApp->thread())
  {
    result = func();
  }
  else
  {
    QMetaObject::invokeMethod(
        qApp, [&]() { result = func(); }, Qt::BlockingQueuedConnection);
  }

  return result;
}

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

QByteArray Server::jsonResponse(const QJsonObject& json, int statusCode)
{
  QByteArray body = QJsonDocument(json).toJson(QJsonDocument::Compact);
  QString statusText = statusCode == 200 ? "OK" : "Error";
  return buildResponse(statusCode, statusText, body);
}

QByteArray Server::errorResponse(const QString& message, int statusCode)
{
  QJsonObject error;
  error["error"] = message;
  return jsonResponse(error, statusCode);
}

}  // namespace widgeteer
