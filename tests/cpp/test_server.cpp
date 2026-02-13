#include <widgeteer/Server.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>

using namespace widgeteer;

class TestServer : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Create a test widget for the server
    testWidget_ = new QWidget();
    testWidget_->setObjectName("testWidget");
    testWidget_->show();
    QTest::qWait(50);
  }

  void cleanupTestCase() {
    delete testWidget_;
  }

  void cleanup() {
    // Process events between tests
    QCoreApplication::processEvents();
  }

  // === Basic Server Lifecycle ===

  void testServerConstruction() {
    Server server;
    QVERIFY(!server.isRunning());
    // Default port is 9000
    QCOMPARE(server.port(), static_cast<quint16>(9000));
  }

  void testServerStartStop() {
    Server server;
    QSignalSpy startedSpy(&server, &Server::serverStarted);
    QSignalSpy stoppedSpy(&server, &Server::serverStopped);

    QVERIFY(server.start(0));  // Let OS assign port
    QVERIFY(server.isRunning());
    QVERIFY(server.port() > 0);
    QCOMPARE(startedSpy.count(), 1);

    server.stop();
    QVERIFY(!server.isRunning());
    QCOMPARE(stoppedSpy.count(), 1);
  }

  void testServerStartTwice() {
    Server server;
    QVERIFY(server.start(0));
    QVERIFY(server.start(0));  // Should return true (already running)
    QVERIFY(server.isRunning());
    server.stop();
  }

  void testServerStopWhenNotRunning() {
    Server server;
    server.stop();  // Should not crash
    QVERIFY(!server.isRunning());
  }

  void testSetPort() {
    Server server;
    server.setPort(12345);
    QCOMPARE(server.port(), static_cast<quint16>(12345));
  }

  // === Configuration ===

  void testSetAllowedHosts() {
    Server server;
    QStringList hosts = { "127.0.0.1", "192.168.1.1" };
    server.setAllowedHosts(hosts);
    // No direct getter, but should not crash
  }

  void testEnableLogging() {
    Server server;
    server.enableLogging(true);
    server.enableLogging(false);
    // Should not crash
  }

  void testApiKey() {
    Server server;
    server.setApiKey("test-key-123");
    QCOMPARE(server.apiKey(), QString("test-key-123"));
  }

  void testSetRootWidget() {
    Server server;
    server.setRootWidget(testWidget_);
    // Should not crash
  }

  // === Recording API ===

  void testRecordingStartStop() {
    Server server;
    QVERIFY(!server.isRecording());

    server.startRecording();
    QVERIFY(server.isRecording());

    server.stopRecording();
    QVERIFY(!server.isRecording());
  }

  void testGetRecording() {
    Server server;
    server.startRecording();
    server.stopRecording();

    QJsonObject recording = server.getRecording();
    QVERIFY(recording.contains("name"));  // Recording has name, description, tests
  }

  // === Event Broadcasting ===

  void testEventBroadcasting() {
    Server server;
    QVERIFY(!server.isEventBroadcastingEnabled());

    server.setEventBroadcastingEnabled(true);
    QVERIFY(server.isEventBroadcastingEnabled());

    server.setEventBroadcastingEnabled(false);
    QVERIFY(!server.isEventBroadcastingEnabled());
  }

  // === Object Registration ===

  void testRegisterObject() {
    Server server;
    QObject obj;
    obj.setObjectName("testObj");

    server.registerObject("myObject", &obj);
    QVERIFY(server.registeredObjects().contains("myObject"));

    server.unregisterObject("myObject");
    QVERIFY(!server.registeredObjects().contains("myObject"));
  }

  void testRegisterNullObject() {
    Server server;
    server.registerObject("nullObj", nullptr);  // Should warn but not crash
    QVERIFY(!server.registeredObjects().contains("nullObj"));
  }

  // === Command Registration ===

  void testRegisterCommand() {
    Server server;

    bool called = false;
    server.registerCommand("myCmd", [&called](const QJsonObject&) {
      called = true;
      return QJsonObject{ { "result", "ok" } };
    });

    QVERIFY(server.registeredCommands().contains("myCmd"));

    server.unregisterCommand("myCmd");
    QVERIFY(!server.registeredCommands().contains("myCmd"));
  }

  void testRegisterNullHandler() {
    Server server;
    server.registerCommand("nullCmd", nullptr);  // Should warn but not crash
    QVERIFY(!server.registeredCommands().contains("nullCmd"));
  }

  void testRegisterBuiltinCommand() {
    Server server;

    // Try to override a built-in command
    server.registerCommand("click", [](const QJsonObject&) {
      return QJsonObject{ { "result", "overridden" } };
    });

    // Should not be registered (built-in cannot be overridden)
    QVERIFY(!server.registeredCommands().contains("click"));
  }

  // === WebSocket Client Connection ===

  void testClientConnection() {
    Server server;
    server.enableLogging(false);  // Reduce noise
    QVERIFY(server.start(0));

    QSignalSpy connectedSpy(&server, &Server::clientConnected);
    QSignalSpy disconnectedSpy(&server, &Server::clientDisconnected);

    // Connect a client
    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));

    QVERIFY(clientConnectedSpy.wait(2000));
    QCOMPARE(connectedSpy.count(), 1);

    // Disconnect
    client.close();
    QVERIFY(disconnectedSpy.wait(2000));

    server.stop();
  }

  void testClientSendCommand() {
    Server server;
    server.enableLogging(false);
    server.setRootWidget(testWidget_);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Send a command - use get_tree which always succeeds
    QJsonObject cmd;
    cmd["type"] = "command";
    cmd["id"] = "test-1";
    cmd["command"] = "get_tree";
    cmd["params"] = QJsonObject{};

    client.sendTextMessage(QString::fromUtf8(QJsonDocument(cmd).toJson()));

    // Wait for response - may need extra time for async processing
    QVERIFY(messageReceivedSpy.wait(3000));

    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject respObj = doc.object();

    // Response should have type "response"
    QCOMPARE(respObj.value("type").toString(), QString("response"));
    QVERIFY(respObj.value("success").toBool());
    QVERIFY(respObj.value("result").isObject());

    client.close();
    server.stop();
  }

  void testClientSendTransaction() {
    Server server;
    server.enableLogging(false);
    server.setRootWidget(testWidget_);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    QJsonObject tx;
    tx["id"] = "tx-1";
    tx["transaction"] = true;
    tx["steps"] =
        QJsonArray{ QJsonObject{ { "command", "get_tree" }, { "params", QJsonObject{} } } };

    client.sendTextMessage(QString::fromUtf8(QJsonDocument(tx).toJson()));
    QVERIFY(messageReceivedSpy.wait(3000));

    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject respObj = doc.object();

    QCOMPARE(respObj.value("type").toString(), QString("response"));
    QVERIFY(respObj.value("success").toBool());
    QCOMPARE(respObj.value("completed_steps").toInt(), 1);
    QCOMPARE(respObj.value("total_steps").toInt(), 1);

    client.close();
    server.stop();
  }

  void testClientSendInvalidJson() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Send invalid JSON
    client.sendTextMessage("not valid json{");

    // Wait for error response
    QVERIFY(messageReceivedSpy.wait(2000));

    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("PARSE_ERROR"));

    client.close();
    server.stop();
  }

  void testClientSendInvalidType() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Send message with invalid type
    QJsonObject msg;
    msg["type"] = "unknown_type";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));

    // Wait for error response
    QVERIFY(messageReceivedSpy.wait(2000));

    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("INVALID_TYPE"));

    client.close();
    server.stop();
  }

  void testSubscribeUnsubscribe() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Subscribe to an event
    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-1";
    subscribeMsg["event_type"] = "command_executed";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(doc.object().value("success").toBool());

    // Unsubscribe
    messageReceivedSpy.clear();
    QJsonObject unsubscribeMsg;
    unsubscribeMsg["type"] = "unsubscribe";
    unsubscribeMsg["id"] = "unsub-1";
    unsubscribeMsg["event_type"] = "command_executed";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(unsubscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    response = messageReceivedSpy.at(0).at(0).toString();
    doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(doc.object().value("success").toBool());

    client.close();
    server.stop();
  }

  void testSubscribeUnsupportedEventType() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-unsupported";
    subscribeMsg["event_type"] = "click";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QJsonDocument doc = QJsonDocument::fromJson(messageReceivedSpy.at(0).at(0).toString().toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("INVALID_PARAMS"));

    client.close();
    server.stop();
  }

  void testSubscribeMissingEventType() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Subscribe without event_type
    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-1";
    // Missing event_type
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("MISSING_PARAM"));

    client.close();
    server.stop();
  }

  void testSubscribeRejectsPropertyFilterOnNonPropertyEvent() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-prop-invalid";
    subscribeMsg["event_type"] = "command_executed";
    subscribeMsg["filter"] =
        QJsonObject{ { "target", "@name:testWidget" }, { "property", "enabled" } };
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QJsonDocument doc = QJsonDocument::fromJson(messageReceivedSpy.at(0).at(0).toString().toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("INVALID_PARAMS"));

    client.close();
    server.stop();
  }

  void testSubscribeRequiresPropertyChangedFilterFields() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-prop-missing";
    subscribeMsg["event_type"] = "property_changed";
    subscribeMsg["filter"] = QJsonObject{ { "target", "@name:testWidget" } };  // missing property
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QJsonDocument doc = QJsonDocument::fromJson(messageReceivedSpy.at(0).at(0).toString().toUtf8());
    QVERIFY(!doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("error").toObject().value("code").toString(),
             QString("INVALID_PARAMS"));

    client.close();
    server.stop();
  }

  void testSubscribeAcceptsPropertyChangedFilter() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    QJsonObject subscribeMsg;
    subscribeMsg["type"] = "subscribe";
    subscribeMsg["id"] = "sub-prop-valid";
    subscribeMsg["event_type"] = "property_changed";
    subscribeMsg["filter"] =
        QJsonObject{ { "target", "@name:testWidget" }, { "property", "windowTitle" } };
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(subscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QJsonDocument doc = QJsonDocument::fromJson(messageReceivedSpy.at(0).at(0).toString().toUtf8());
    QVERIFY(doc.object().value("success").toBool());
    QCOMPARE(doc.object().value("result").toObject().value("subscribed").toString(),
             QString("property_changed"));

    client.close();
    server.stop();
  }

  void testUnsubscribeAll() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Unsubscribe from all (no event_type)
    QJsonObject unsubscribeMsg;
    unsubscribeMsg["type"] = "unsubscribe";
    unsubscribeMsg["id"] = "unsub-all";
    // No event_type = unsubscribe all
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(unsubscribeMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(doc.object().value("success").toBool());

    client.close();
    server.stop();
  }

  void testRecordStartStopViaWebSocket() {
    Server server;
    server.enableLogging(false);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy clientConnectedSpy(&client, &QWebSocket::connected);
    QSignalSpy messageReceivedSpy(&client, &QWebSocket::textMessageReceived);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    client.open(QUrl(url));
    QVERIFY(clientConnectedSpy.wait(2000));

    // Start recording
    QJsonObject startMsg;
    startMsg["type"] = "record_start";
    startMsg["id"] = "rec-1";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(startMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    QString response = messageReceivedSpy.at(0).at(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(doc.object().value("success").toBool());
    QVERIFY(server.isRecording());

    // Stop recording
    messageReceivedSpy.clear();
    QJsonObject stopMsg;
    stopMsg["type"] = "record_stop";
    stopMsg["id"] = "rec-2";
    client.sendTextMessage(QString::fromUtf8(QJsonDocument(stopMsg).toJson()));

    QVERIFY(messageReceivedSpy.wait(2000));
    response = messageReceivedSpy.at(0).at(0).toString();
    doc = QJsonDocument::fromJson(response.toUtf8());
    QVERIFY(doc.object().value("success").toBool());
    QVERIFY(!server.isRecording());

    client.close();
    server.stop();
  }

  void testApiKeyValidation() {
    Server server;
    server.enableLogging(false);
    server.setApiKey("secret-key");
    QVERIFY(server.start(0));

    // Try to connect without API key
    QWebSocket clientNoKey;
    QSignalSpy clientNoKeyConnectedSpy(&clientNoKey, &QWebSocket::connected);
    QSignalSpy clientNoKeyDisconnectedSpy(&clientNoKey, &QWebSocket::disconnected);

    QString url = QString("ws://127.0.0.1:%1").arg(server.port());
    clientNoKey.open(QUrl(url));

    // Should be rejected
    QVERIFY(clientNoKeyDisconnectedSpy.wait(2000));
    QVERIFY(clientNoKey.state() != QAbstractSocket::ConnectedState);

    // Connect with valid API key
    QWebSocket clientWithKey;
    QSignalSpy clientWithKeyConnectedSpy(&clientWithKey, &QWebSocket::connected);

    QString urlWithKey = QString("ws://127.0.0.1:%1?token=secret-key").arg(server.port());
    clientWithKey.open(QUrl(urlWithKey));

    QVERIFY(clientWithKeyConnectedSpy.wait(2000));

    clientWithKey.close();
    server.stop();
  }

private:
  QWidget* testWidget_ = nullptr;
};

QTEST_MAIN(TestServer)
#include "test_server.moc"
