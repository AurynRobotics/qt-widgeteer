#pragma once

#include <widgeteer/CommandExecutor.h>
#include <widgeteer/Export.h>

#include <QObject>
#include <QTcpServer>
#include <QWidget>

#include <memory>

namespace widgeteer
{

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
  bool running_ = false;

  // Buffer for incomplete requests
  QMap<QTcpSocket*, QByteArray> requestBuffers_;
};

}  // namespace widgeteer
