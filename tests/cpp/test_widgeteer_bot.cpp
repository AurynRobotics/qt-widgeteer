#include <widgeteer/WidgeteerBot.h>

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using namespace widgeteer;

class TestWidgeteerBot : public QObject {
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

  // ==================== Bot Constructor Tests ====================

  void testBotConstructorOwned() {
    WidgeteerBot bot;
    QVERIFY(bot.executor() != nullptr);
  }

  void testBotConstructorExternal() {
    CommandExecutor executor;
    WidgeteerBot bot(&executor);
    QCOMPARE(bot.executor(), &executor);
  }

  // ==================== Action Tests ====================

  void testClick() {
    WidgeteerBot bot;

    bool clicked = false;
    connect(button_, &QPushButton::clicked, this, [&clicked]() { clicked = true; });

    auto result = bot.click("@name:button");
    QVERIFY(result);
    QTest::qWait(50);
    QVERIFY(clicked);
  }

  void testClickNotFound() {
    WidgeteerBot bot;
    auto result = bot.click("@name:nonexistent");
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("ELEMENT_NOT_FOUND"));
  }

  void testType() {
    WidgeteerBot bot;
    input_->clear();

    auto result = bot.type("@name:input", "Hello");
    QVERIFY(result);
    QTest::qWait(50);
    QCOMPARE(input_->text(), QString("Hello"));
  }

  void testTypeClearFirst() {
    WidgeteerBot bot;
    input_->setText("Old text");

    auto result = bot.type("@name:input", "New text", true);
    QVERIFY(result);
    QTest::qWait(50);
    QCOMPARE(input_->text(), QString("New text"));
  }

  void testFocus() {
    WidgeteerBot bot;

    auto result = bot.focus("@name:input");
    QVERIFY(result);
    QTest::qWait(50);
    QVERIFY(input_->hasFocus());
  }

  // ==================== State Tests ====================

  void testSetValue() {
    WidgeteerBot bot;

    auto result = bot.setValue("@name:spinBox", 77);
    QVERIFY(result);
    QCOMPARE(spinBox_->value(), 77);
  }

  void testSetProperty() {
    WidgeteerBot bot;

    auto result = bot.setProperty("@name:label", "text", "New Label Text");
    QVERIFY(result);
    QCOMPARE(label_->text(), QString("New Label Text"));
  }

  void testGetProperty() {
    WidgeteerBot bot;
    label_->setText("Test Property");

    auto result = bot.getProperty("@name:label", "text");
    QVERIFY(result);
    QCOMPARE(result.value().toString(), QString("Test Property"));
  }

  void testGetPropertyNotFound() {
    WidgeteerBot bot;
    auto result = bot.getProperty("@name:label", "nonexistentProperty");
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("PROPERTY_NOT_FOUND"));
  }

  // ==================== Query Tests ====================

  void testExists() {
    WidgeteerBot bot;

    auto existsResult = bot.exists("@name:button");
    QVERIFY(existsResult);
    QVERIFY(existsResult.value());

    auto notExistsResult = bot.exists("@name:nonexistent");
    QVERIFY(notExistsResult);
    QVERIFY(!notExistsResult.value());
  }

  void testIsVisible() {
    WidgeteerBot bot;

    auto visibleResult = bot.isVisible("@name:button");
    QVERIFY(visibleResult);
    QVERIFY(visibleResult.value());

    button_->hide();
    auto hiddenResult = bot.isVisible("@name:button");
    QVERIFY(hiddenResult);
    QVERIFY(!hiddenResult.value());
    button_->show();
  }

  void testGetText() {
    WidgeteerBot bot;
    label_->setText("Sample Text");

    auto result = bot.getText("@name:label");
    QVERIFY(result);
    QCOMPARE(result.value(), QString("Sample Text"));
  }

  void testGetTextFromLineEdit() {
    WidgeteerBot bot;
    input_->setText("Input Text");

    auto result = bot.getText("@name:input");
    QVERIFY(result);
    QCOMPARE(result.value(), QString("Input Text"));
  }

  void testListProperties() {
    WidgeteerBot bot;

    auto result = bot.listProperties("@name:button");
    QVERIFY(result);
    QVERIFY(result.value().size() > 0);
  }

  // ==================== Introspection Tests ====================

  void testGetTree() {
    WidgeteerBot bot;

    auto result = bot.getTree();
    QVERIFY(result);
    QVERIFY(result.value().contains("children") || result.value().contains("objectName"));
  }

  void testGetTreeWithDepth() {
    WidgeteerBot bot;

    auto result = bot.getTree(1);
    QVERIFY(result);
  }

  void testFind() {
    WidgeteerBot bot;

    auto result = bot.find("@class:QPushButton");
    QVERIFY(result);
    QVERIFY(result.value().contains("matches"));
    QVERIFY(result.value().value("count").toInt() >= 1);
  }

  void testDescribe() {
    WidgeteerBot bot;

    auto result = bot.describe("@name:button");
    QVERIFY(result);
    QVERIFY(result.value().contains("objectName") || result.value().contains("class"));
  }

  void testGetFormFields() {
    WidgeteerBot bot;

    auto result = bot.getFormFields();
    QVERIFY(result);
    QVERIFY(result.value().contains("fields"));
  }

  // ==================== Sync Tests ====================

  void testWaitFor() {
    WidgeteerBot bot;

    auto result = bot.waitFor("@name:button", "exists", 1000);
    QVERIFY(result);
  }

  void testWaitForTimeout() {
    WidgeteerBot bot;

    auto result = bot.waitFor("@name:nonexistent", "exists", 100);
    QVERIFY(!result);
    QCOMPARE(result.error().code, QString("TIMEOUT"));
  }

  void testSleep() {
    WidgeteerBot bot;

    QElapsedTimer timer;
    timer.start();
    auto result = bot.sleep(50);
    QVERIFY(result);
    QVERIFY(timer.elapsed() >= 50);
  }

  // ==================== Screenshot Tests ====================

  void testScreenshot() {
    WidgeteerBot bot;

    auto result = bot.screenshot();
    QVERIFY(result);
    QVERIFY(result.value().contains("screenshot"));
    QVERIFY(result.value().contains("width"));
    QVERIFY(result.value().contains("height"));
  }

  void testScreenshotAnnotated() {
    WidgeteerBot bot;

    auto result = bot.screenshotAnnotated();
    QVERIFY(result);
    QVERIFY(result.value().contains("screenshot"));
    QVERIFY(result.value().contains("annotations"));
  }

  // ==================== Extensibility Tests ====================

  void testListObjects() {
    WidgeteerBot bot;

    auto result = bot.listObjects();
    QVERIFY(result);
    QVERIFY(result.value().contains("objects"));
  }

  void testListCustomCommands() {
    WidgeteerBot bot;

    auto result = bot.listCustomCommands();
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

QTEST_MAIN(TestWidgeteerBot)
#include "test_widgeteer_bot.moc"
