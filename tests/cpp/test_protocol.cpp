#include <widgeteer/Protocol.h>

#include <QJsonDocument>
#include <QTest>

using namespace widgeteer;

class TestProtocol : public QObject {
  Q_OBJECT

private slots:
  void testMessageTypeToString() {
    QCOMPARE(messageTypeToString(MessageType::Command), QString("command"));
    QCOMPARE(messageTypeToString(MessageType::Response), QString("response"));
    QCOMPARE(messageTypeToString(MessageType::Event), QString("event"));
    QCOMPARE(messageTypeToString(MessageType::Subscribe), QString("subscribe"));
    QCOMPARE(messageTypeToString(MessageType::Unsubscribe), QString("unsubscribe"));
    QCOMPARE(messageTypeToString(MessageType::RecordStart), QString("record_start"));
    QCOMPARE(messageTypeToString(MessageType::RecordStop), QString("record_stop"));
  }

  void testStringToMessageType() {
    QCOMPARE(stringToMessageType("command"), std::optional(MessageType::Command));
    QCOMPARE(stringToMessageType("response"), std::optional(MessageType::Response));
    QCOMPARE(stringToMessageType("event"), std::optional(MessageType::Event));
    QCOMPARE(stringToMessageType("subscribe"), std::optional(MessageType::Subscribe));
    QCOMPARE(stringToMessageType("unsubscribe"), std::optional(MessageType::Unsubscribe));
    QCOMPARE(stringToMessageType("record_start"), std::optional(MessageType::RecordStart));
    QCOMPARE(stringToMessageType("record_stop"), std::optional(MessageType::RecordStop));
    QCOMPARE(stringToMessageType("invalid"), std::nullopt);
  }

  void testCommandFromJson() {
    QJsonObject json;
    json["id"] = "test-123";
    json["command"] = "click";
    json["params"] = QJsonObject{ { "target", "@name:button1" } };
    json["options"] = QJsonObject{ { "timeout", 5000 } };

    Command cmd = Command::fromJson(json);

    QCOMPARE(cmd.id, QString("test-123"));
    QCOMPARE(cmd.name, QString("click"));
    QCOMPARE(cmd.params.value("target").toString(), QString("@name:button1"));
    QCOMPARE(cmd.options.value("timeout").toInt(), 5000);
  }

  void testCommandFromJsonWithoutParams() {
    // Commands like get_tree, get_form_fields, screenshot can omit params
    QJsonObject json;
    json["id"] = "test-no-params";
    json["command"] = "get_tree";
    // Intentionally no "params" or "options" keys

    Command cmd = Command::fromJson(json);

    QCOMPARE(cmd.id, QString("test-no-params"));
    QCOMPARE(cmd.name, QString("get_tree"));
    QVERIFY(cmd.params.isEmpty());  // Should be empty, not null
    QVERIFY(cmd.options.isEmpty());
  }

  void testCommandToJson() {
    Command cmd;
    cmd.id = "test-456";
    cmd.name = "type";
    cmd.params = QJsonObject{ { "target", "@name:textEdit" }, { "text", "Hello" } };
    cmd.options = QJsonObject{ { "delay", 50 } };

    QJsonObject json = cmd.toJson();

    QCOMPARE(json.value("id").toString(), QString("test-456"));
    QCOMPARE(json.value("command").toString(), QString("type"));
    QCOMPARE(json.value("params").toObject().value("target").toString(), QString("@name:textEdit"));
    QCOMPARE(json.value("params").toObject().value("text").toString(), QString("Hello"));
    QCOMPARE(json.value("options").toObject().value("delay").toInt(), 50);
  }

  void testResponseOk() {
    QJsonObject result;
    result["clicked"] = true;

    Response resp = Response::ok("cmd-1", result);

    QCOMPARE(resp.id, QString("cmd-1"));
    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("clicked").toBool(), true);
  }

  void testResponseFail() {
    Response resp = Response::fail("cmd-2", "NOT_FOUND", "Element not found");

    QCOMPARE(resp.id, QString("cmd-2"));
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("NOT_FOUND"));
    QCOMPARE(resp.error.message, QString("Element not found"));
  }

  void testResponseToJson() {
    Response resp = Response::ok("cmd-3", QJsonObject{ { "value", 42 } });
    resp.durationMs = 15;

    QJsonObject json = resp.toJson();

    QCOMPARE(json.value("id").toString(), QString("cmd-3"));
    QVERIFY(json.value("success").toBool());
    QCOMPARE(json.value("result").toObject().value("value").toInt(), 42);
    QCOMPARE(json.value("duration_ms").toInt(), 15);
  }

  void testErrorResponseToJson() {
    Response resp =
        Response::fail("cmd-4", "ERR_CODE", "Error message", QJsonObject{ { "extra", "info" } });

    QJsonObject json = resp.toJson();

    QCOMPARE(json.value("id").toString(), QString("cmd-4"));
    QVERIFY(!json.value("success").toBool());
    QCOMPARE(json.value("error").toObject().value("code").toString(), QString("ERR_CODE"));
    QCOMPARE(json.value("error").toObject().value("message").toString(), QString("Error message"));
    QCOMPARE(json.value("error").toObject().value("details").toObject().value("extra").toString(),
             QString("info"));
  }

  void testTransactionFromJson() {
    QJsonObject json;
    json["id"] = "tx-1";
    json["rollback_on_failure"] = true;
    json["steps"] = QJsonArray{
      QJsonObject{ { "command", "click" }, { "params", QJsonObject{ { "target", "btn1" } } } },
      QJsonObject{ { "command", "type" },
                   { "params", QJsonObject{ { "target", "input" }, { "text", "test" } } } }
    };

    Transaction tx = Transaction::fromJson(json);

    QCOMPARE(tx.id, QString("tx-1"));
    QVERIFY(tx.rollbackOnFailure);
    QCOMPARE(tx.steps.size(), 2);
    QCOMPARE(tx.steps[0].name, QString("click"));
    QCOMPARE(tx.steps[1].name, QString("type"));
  }

  void testTransactionToJson() {
    Transaction tx;
    tx.id = "tx-2";
    tx.rollbackOnFailure = false;

    Command step1;
    step1.name = "focus";
    step1.params = QJsonObject{ { "target", "widget1" } };
    tx.steps.append(step1);

    QJsonObject json = tx.toJson();

    QCOMPARE(json.value("id").toString(), QString("tx-2"));
    QVERIFY(json.value("transaction").toBool());
    QVERIFY(!json.value("rollback_on_failure").toBool());
    QCOMPARE(json.value("steps").toArray().size(), 1);
  }

  void testBuildElementNotFoundError() {
    QJsonObject error =
        buildElementNotFoundError("path/to/widget", "path/to", QStringList{ "child1", "child2" });

    QCOMPARE(error.value("searched_path").toString(), QString("path/to/widget"));
    QCOMPARE(error.value("partial_match").toString(), QString("path/to"));
    QCOMPARE(error.value("available_children").toArray().size(), 2);
  }

  void testBuildElementNotFoundErrorMinimal() {
    // Test with empty partialMatch and empty availableChildren
    QJsonObject error = buildElementNotFoundError("widget", "", QStringList{});

    QCOMPARE(error.value("searched_path").toString(), QString("widget"));
    QVERIFY(!error.contains("partial_match"));
    QVERIFY(!error.contains("available_children"));
  }

  void testTransactionResponseToJson() {
    TransactionResponse resp;
    resp.id = "tx-response-1";
    resp.success = true;
    resp.completedSteps = 3;
    resp.totalSteps = 3;
    resp.rollbackPerformed = false;
    resp.stepsResults = QJsonArray{ QJsonObject{ { "step", 0 }, { "success", true } },
                                    QJsonObject{ { "step", 1 }, { "success", true } },
                                    QJsonObject{ { "step", 2 }, { "success", true } } };

    QJsonObject json = resp.toJson();

    QCOMPARE(json.value("id").toString(), QString("tx-response-1"));
    QVERIFY(json.value("success").toBool());
    QCOMPARE(json.value("completed_steps").toInt(), 3);
    QCOMPARE(json.value("total_steps").toInt(), 3);
    QVERIFY(!json.value("rollback_performed").toBool());
    QCOMPARE(json.value("steps_results").toArray().size(), 3);
  }

  void testResponseToJsonWithoutDuration() {
    // Response with durationMs == 0 should not include duration_ms
    Response resp = Response::ok("cmd-no-duration", QJsonObject{ { "done", true } });
    resp.durationMs = 0;

    QJsonObject json = resp.toJson();

    QVERIFY(!json.contains("duration_ms"));
    QVERIFY(json.value("success").toBool());
  }

  void testResponseToJsonWithEmptyResult() {
    // Success response with empty result should not include result key
    Response resp = Response::ok("cmd-empty", QJsonObject{});

    QJsonObject json = resp.toJson();

    QVERIFY(json.value("success").toBool());
    QVERIFY(!json.contains("result"));
  }

  void testErrorDetailsToJsonWithoutDetails() {
    // Error without details should not include details key
    Response resp = Response::fail("cmd-err", "ERR", "Error message");

    QJsonObject json = resp.toJson();
    QJsonObject errorJson = json.value("error").toObject();

    QCOMPARE(errorJson.value("code").toString(), QString("ERR"));
    QCOMPARE(errorJson.value("message").toString(), QString("Error message"));
    QVERIFY(!errorJson.contains("details"));
  }
};

QTEST_MAIN(TestProtocol)
#include "test_protocol.moc"
