#pragma once

#include <widgeteer/ActionRecorder.h>
#include <widgeteer/CommandExecutor.h>
#include <widgeteer/EventBroadcaster.h>
#include <widgeteer/Export.h>
#include <widgeteer/Protocol.h>

#include <QObject>
#include <QEvent>
#include <QSet>
#include <QPointer>
#include <QVariant>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QWidget>
#include <QTimer>

#include <functional>
#include <memory>

namespace widgeteer {

// Type alias for custom command handlers
using CommandHandler = std::function<QJsonObject(const QJsonObject& params)>;

// Client connection info
struct ClientInfo {
  QWebSocket* socket = nullptr;
  QString id;
  bool authenticated = false;
};

class WIDGETEER_EXPORT Server : public QObject {
  Q_OBJECT

public:
  explicit Server(QObject* parent = nullptr);
  ~Server() override;

  // Start the WebSocket server on the specified port
  bool start(quint16 port = 9000);

  // Stop the server
  void stop();

  // Check if server is running
  bool isRunning() const;

  // Configuration
  void setPort(quint16 port);
  quint16 port() const;

  void setAllowedHosts(const QStringList& hosts);
  void enableLogging(bool enable);

  // Authentication - set to enable API key authentication
  // Clients must connect with ws://host:port?token=<apiKey>
  void setApiKey(const QString& apiKey);
  QString apiKey() const;

  // Limit introspection scope to specific widget tree
  void setRootWidget(QWidget* root);

  // ========== Recording API ==========

  // Start recording commands (output in sample_tests.json format)
  void startRecording();
  void stopRecording();
  bool isRecording() const;
  QJsonObject getRecording() const;

  // ========== Event Broadcasting API ==========

  // Enable/disable event broadcasting
  void setEventBroadcastingEnabled(bool enabled);
  bool isEventBroadcastingEnabled() const;

  // ========== Extensibility API ==========

  /**
   * Register a QObject to expose its Q_INVOKABLE methods via the API.
   *
   * Usage:
   *   server.registerObject("myService", &myServiceInstance);
   *
   * Then call via API:
   *   {"type": "command", "command": "call", "params": {"object": "myService", "method":
   * "doSomething", "args": [...]}}
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
   *   {"type": "command", "command": "loadProject", "params": {"path": "/path/to/project"}}
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
  void clientConnected(const QString& clientId);
  void clientDisconnected(const QString& clientId);
  void requestReceived(const QString& id, const QString& command);
  void responseReady(const QString& id, bool success);
  void serverStarted(quint16 port);
  void serverStopped();
  void serverError(const QString& error);

private slots:
  void onNewConnection();
  void onTextMessageReceived(const QString& message);
  void onClientDisconnected();
  void onEventReady(const QString& eventType, const QJsonObject& data,
                    const QStringList& recipientClientIds);
  void onFocusChanged(QWidget* oldWidget, QWidget* newWidget);
  void onPropertyPollTimeout();

private:
  bool eventFilter(QObject* watched, QEvent* event) override;

  // Message handling
  void handleMessage(QWebSocket* client, const QJsonObject& message);
  void handleCommand(QWebSocket* client, const QJsonObject& message);
  void handleTransaction(QWebSocket* client, const QJsonObject& message);
  void handleSubscribe(QWebSocket* client, const QJsonObject& message);
  void handleUnsubscribe(QWebSocket* client, const QJsonObject& message);
  void handleRecordStart(QWebSocket* client, const QJsonObject& message);
  void handleRecordStop(QWebSocket* client, const QJsonObject& message);

  // Send response to client
  void sendResponse(QWebSocket* client, const QJsonObject& response);
  void sendEvent(QWebSocket* client, const QString& eventType, const QJsonObject& data);

  // Check if request is from allowed host
  bool isAllowedHost(const QString& remoteHost) const;

  // Validate API key from connection URL
  bool validateApiKey(const QUrl& requestUrl) const;

  // Get client ID from socket
  QString clientIdForSocket(QWebSocket* socket) const;
  void updateUiEventTrackingState();
  void refreshPropertyWatches();
  void registerWidgetLifecycle(QObject* object);

  std::unique_ptr<QWebSocketServer> wsServer_;
  std::unique_ptr<CommandExecutor> executor_;
  std::unique_ptr<ActionRecorder> recorder_;
  std::unique_ptr<EventBroadcaster> broadcaster_;

  quint16 port_ = 9000;
  QString apiKey_;
  QStringList allowedHosts_;
  bool loggingEnabled_ = false;
  QPointer<QWidget> rootWidget_;
  bool running_ = false;

  // Connected clients: clientId -> ClientInfo
  QHash<QString, ClientInfo> clients_;

  // Socket to clientId mapping for quick lookup
  QHash<QWebSocket*, QString> socketToClientId_;

  // Extensibility: registered objects and custom commands
  QHash<QString, QPointer<QObject>> registeredObjects_;
  QHash<QString, CommandHandler> customCommands_;

  struct PropertyWatch {
    QString selector;
    QString property;
    QPointer<QWidget> widget;
    QVariant lastValue;
    bool initialized = false;
  };

  bool uiEventTrackingActive_ = false;
  QMetaObject::Connection focusChangedConnection_;
  QTimer propertyPollTimer_;
  QList<PropertyWatch> propertyWatches_;
  QSet<QObject*> lifecycleTrackedWidgets_;
};

}  // namespace widgeteer
