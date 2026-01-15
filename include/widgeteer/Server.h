#pragma once

#include <widgeteer/CommandExecutor.h>
#include <widgeteer/Export.h>

#include <QObject>
#include <QTcpServer>
#include <QWidget>

#include <functional>
#include <memory>

namespace widgeteer
{

// Type alias for custom command handlers
using CommandHandler = std::function<QJsonObject(const QJsonObject& params)>;

class WIDGETEER_EXPORT Server : public QObject
{
  Q_OBJECT

public:
  explicit Server(QObject* parent = nullptr);
  ~Server() override;

  // Start the HTTP server on the specified port
  bool start(quint16 port = 9000);

  // Stop the server
  void stop();

  // Check if server is running
  bool isRunning() const;

  // Configuration
  void setPort(quint16 port);
  quint16 port() const;

  void setAllowedHosts(const QStringList& hosts);
  void enableCORS(bool enable);
  void enableLogging(bool enable);

  // Limit introspection scope to specific widget tree
  void setRootWidget(QWidget* root);

  // ========== Extensibility API ==========

  /**
   * Register a QObject to expose its Q_INVOKABLE methods via the API.
   *
   * Usage:
   *   server.registerObject("myService", &myServiceInstance);
   *
   * Then call via API:
   *   {"command": "call", "params": {"object": "myService", "method": "doSomething", "args":
   * [...]}}
   *
   * @param name Unique name to identify this object in API calls
   * @param object Pointer to QObject (must outlive the server)
   */
  void registerObject(const QString& name, QObject* object);

  /**
   * Unregister a previously registered QObject.
   */
  void unregisterObject(const QString& name);

  /**
   * Register a custom command handler (lambda/function).
   *
   * Usage:
   *   server.registerCommand("loadProject", [&app](const QJsonObject& params) {
   *       QString path = params["path"].toString();
   *       bool ok = app.loadProject(path);
   *       return QJsonObject{{"success", ok}, {"path", path}};
   *   });
   *
   * Then call via API:
   *   {"command": "loadProject", "params": {"path": "/path/to/project"}}
   *
   * @param name Command name (must not conflict with built-in commands)
   * @param handler Function that receives params and returns result
   */
  void registerCommand(const QString& name, CommandHandler handler);

  /**
   * Unregister a previously registered command handler.
   */
  void unregisterCommand(const QString& name);

  /**
   * Get list of registered object names.
   */
  QStringList registeredObjects() const;

  /**
   * Get list of registered custom command names.
   */
  QStringList registeredCommands() const;

signals:
  void requestReceived(const QString& id, const QString& command);
  void responseReady(const QString& id, bool success);
  void serverStarted(quint16 port);
  void serverStopped();
  void serverError(const QString& error);

private slots:
  void onNewConnection();
  void onClientReadyRead();
  void onClientDisconnected();

private:
  // HTTP request/response handling
  struct HttpRequest
  {
    QString method;
    QString path;
    QMap<QString, QString> headers;
    QMap<QString, QString> queryParams;
    QByteArray body;
  };

  HttpRequest parseRequest(const QByteArray& data);
  QByteArray buildResponse(int statusCode, const QString& statusText, const QByteArray& body,
                           const QString& contentType = "application/json");

  // Route handlers
  QByteArray handleRequest(const HttpRequest& req);
  QByteArray handleHealth();
  QByteArray handleSchema();
  QByteArray handleTree(const HttpRequest& req);
  QByteArray handleScreenshot(const HttpRequest& req);
  QByteArray handleCommand(const HttpRequest& req);
  QByteArray handleTransaction(const HttpRequest& req);

  // Execute function on main thread and return result
  template <typename Func>
  QJsonObject executeOnMainThread(Func&& func);

  // Check if request is from allowed host
  bool isAllowedHost(const QString& remoteHost) const;

  // Build JSON error response
  QByteArray errorResponse(const QString& message, int statusCode = 400);
  QByteArray jsonResponse(const QJsonObject& json, int statusCode = 200);

  std::unique_ptr<QTcpServer> server_;
  std::unique_ptr<CommandExecutor> executor_;
  quint16 port_ = 9000;
  QStringList allowedHosts_;
  bool corsEnabled_ = true;
  bool loggingEnabled_ = false;
  QPointer<QWidget> rootWidget_;

  // Extensibility: registered objects and custom commands
  QHash<QString, QPointer<QObject>> registeredObjects_;
  QHash<QString, CommandHandler> customCommands_;
  bool running_ = false;

  // Buffer for incomplete requests
  QMap<QTcpSocket*, QByteArray> requestBuffers_;
};

}  // namespace widgeteer
