#include <widgeteer/WidgeteerClient.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using namespace widgeteer;

class TestWidgeteerClient : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Create a test widget hierarchy:
    //   testWindow (QWidget)
    //     |-- button (QPushButton)
    //     |-- input (QLineEdit)
    //     |-- label (QLabel)
    //     |-- spinBox (QSpinBox)

    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");

    auto* layout = new QVBoxLayout(testWindow_);

    button_ = new QPushButton("Click Me", testWindow_);
    button_->setObjectName("button");
    layout->addWidget(button_);

    input_ = new QLineEdit(testWindow_);
    input_->setObjectName("input");
    input_->setPlaceholderText("Enter text...");
    layout->addWidget(input_);

    label_ = new QLabel("Hello World", testWindow_);
    label_->setObjectName("label");
    layout->addWidget(label_);

    spinBox_ = new QSpinBox(testWindow_);
    spinBox_->setObjectName("spinBox");
    spinBox_->setRange(0, 100);
    spinBox_->setValue(42);
    layout->addWidget(spinBox_);

    testWindow_->show();
    QTest::qWait(100);
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  // ==================== Result Template Tests ====================

  void testResultOk() {
    auto result = Result<int>::ok(42);
    QVERIFY(result.success());
    QVERIFY(result);
    QCOMPARE(result.value(), 42);
  }

  void testResultFail() {
    ErrorDetails error;
    error.code = "TEST_ERROR";
    error.message = "Test error message";

    auto result = Result<int>::fail(error);
    QVERIFY(!result.success());
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("TEST_ERROR"));
  }

  void testResultValueOr() {
    auto okResult = Result<int>::ok(42);
    QCOMPARE(okResult.valueOr(0), 42);

    ErrorDetails error;
    error.code = "ERR";
    auto failResult = Result<int>::fail(error);
    QCOMPARE(failResult.valueOr(99), 99);
  }

  void testResultVoidOk() {
    auto result = Result<void>::ok();
    QVERIFY(result.success());
    QVERIFY(result);
  }

  void testResultVoidFail() {
    ErrorDetails error;
    error.code = "VOID_ERROR";
    error.message = "Void error";

    auto result = Result<void>::fail(error);
    QVERIFY(!result.success());
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("VOID_ERROR"));
  }

  // ==================== Client Constructor Tests ====================

  void testClientConstructorOwned() {
    WidgeteerClient client;
    QVERIFY(client.executor() != nullptr);
  }

  void testClientConstructorExternal() {
    CommandExecutor executor;
    WidgeteerClient client(&executor);
    QCOMPARE(client.executor(), &executor);
  }

  // ==================== Action Tests ====================

  void testClick() {
    WidgeteerClient client;

    bool clicked = false;
    connect(button_, &QPushButton::clicked, this, [&clicked]() { clicked = true; });

    auto result = client.click("@name:button");
    QVERIFY(result);
    QTest::qWait(50);
    QVERIFY(clicked);
  }

  void testClickNotFound() {
    WidgeteerClient client;
    auto result = client.click("@name:nonexistent");
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("ELEMENT_NOT_FOUND"));
  }

  void testType() {
    WidgeteerClient client;
    input_->clear();

    auto result = client.type("@name:input", "Hello");
    QVERIFY(result);
    QTest::qWait(50);
    QCOMPARE(input_->text(), QString("Hello"));
  }

  void testTypeClearFirst() {
    WidgeteerClient client;
    input_->setText("Old text");

    auto result = client.type("@name:input", "New text", true);
    QVERIFY(result);
    QTest::qWait(50);
    QCOMPARE(input_->text(), QString("New text"));
  }

  void testFocus() {
    WidgeteerClient client;

    auto result = client.focus("@name:input");
    QVERIFY(result);
    QTest::qWait(50);
    QVERIFY(input_->hasFocus());
  }

  // ==================== State Tests ====================

  void testSetValue() {
    WidgeteerClient client;

    auto result = client.setValue("@name:spinBox", 77);
    QVERIFY(result);
    QCOMPARE(spinBox_->value(), 77);
  }

  void testSetProperty() {
    WidgeteerClient client;

    auto result = client.setProperty("@name:label", "text", "New Label Text");
    QVERIFY(result);
    QCOMPARE(label_->text(), QString("New Label Text"));
  }

  void testGetProperty() {
    WidgeteerClient client;
    label_->setText("Test Property");

    auto result = client.getProperty("@name:label", "text");
    QVERIFY(result);
    QCOMPARE(result.value().toString(), QString("Test Property"));
  }

  void testGetPropertyNotFound() {
    WidgeteerClient client;
    auto result = client.getProperty("@name:label", "nonexistentProperty");
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("PROPERTY_NOT_FOUND"));
  }

  // ==================== Query Tests ====================

  void testExists() {
    WidgeteerClient client;

    auto existsResult = client.exists("@name:button");
    QVERIFY(existsResult);
    QVERIFY(existsResult.value());

    auto notExistsResult = client.exists("@name:nonexistent");
    QVERIFY(notExistsResult);
    QVERIFY(!notExistsResult.value());
  }

  void testIsVisible() {
    WidgeteerClient client;

    auto visibleResult = client.isVisible("@name:button");
    QVERIFY(visibleResult);
    QVERIFY(visibleResult.value());

    button_->hide();
    auto hiddenResult = client.isVisible("@name:button");
    QVERIFY(hiddenResult);
    QVERIFY(!hiddenResult.value());
    button_->show();
  }

  void testGetText() {
    WidgeteerClient client;
    label_->setText("Sample Text");

    auto result = client.getText("@name:label");
    QVERIFY(result);
    QCOMPARE(result.value(), QString("Sample Text"));
  }

  void testGetTextFromLineEdit() {
    WidgeteerClient client;
    input_->setText("Input Text");

    auto result = client.getText("@name:input");
    QVERIFY(result);
    QCOMPARE(result.value(), QString("Input Text"));
  }

  void testListProperties() {
    WidgeteerClient client;

    auto result = client.listProperties("@name:button");
    QVERIFY(result);
    QVERIFY(result.value().size() > 0);
  }

  // ==================== Introspection Tests ====================

  void testGetTree() {
    WidgeteerClient client;

    auto result = client.getTree();
    QVERIFY(result);
    QVERIFY(result.value().contains("children") || result.value().contains("objectName"));
  }

  void testGetTreeWithDepth() {
    WidgeteerClient client;

    auto result = client.getTree(1);
    QVERIFY(result);
  }

  void testFind() {
    WidgeteerClient client;

    auto result = client.find("@class:QPushButton");
    QVERIFY(result);
    QVERIFY(result.value().contains("matches"));
    QVERIFY(result.value().value("count").toInt() >= 1);
  }

  void testDescribe() {
    WidgeteerClient client;

    auto result = client.describe("@name:button");
    QVERIFY(result);
    QVERIFY(result.value().contains("objectName") || result.value().contains("class"));
  }

  void testGetFormFields() {
    WidgeteerClient client;

    auto result = client.getFormFields();
    QVERIFY(result);
    QVERIFY(result.value().contains("fields"));
  }

  // ==================== Sync Tests ====================

  void testWaitFor() {
    WidgeteerClient client;

    auto result = client.waitFor("@name:button", "exists", 1000);
    QVERIFY(result);
  }

  void testWaitForTimeout() {
    WidgeteerClient client;

    auto result = client.waitFor("@name:nonexistent", "exists", 100);
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("TIMEOUT"));
  }

  void testSleep() {
    WidgeteerClient client;

    QElapsedTimer timer;
    timer.start();
    auto result = client.sleep(50);
    QVERIFY(result);
    QVERIFY(timer.elapsed() >= 50);
  }

  // ==================== Screenshot Tests ====================

  void testScreenshot() {
    WidgeteerClient client;

    auto result = client.screenshot();
    QVERIFY(result);
    QVERIFY(result.value().contains("screenshot"));
    QVERIFY(result.value().contains("width"));
    QVERIFY(result.value().contains("height"));
  }

  void testScreenshotAnnotated() {
    WidgeteerClient client;

    auto result = client.screenshotAnnotated();
    QVERIFY(result);
    QVERIFY(result.value().contains("screenshot"));
    QVERIFY(result.value().contains("annotations"));
  }

  // ==================== Extensibility Tests ====================

  void testListObjects() {
    WidgeteerClient client;

    auto result = client.listObjects();
    QVERIFY(result);
    QVERIFY(result.value().contains("objects"));
  }

  void testListCustomCommands() {
    WidgeteerClient client;

    auto result = client.listCustomCommands();
    QVERIFY(result);
    QVERIFY(result.value().contains("commands"));
  }

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button_ = nullptr;
  QLineEdit* input_ = nullptr;
  QLabel* label_ = nullptr;
  QSpinBox* spinBox_ = nullptr;
};

QTEST_MAIN(TestWidgeteerClient)
#include "test_widgeteer_client.moc"
