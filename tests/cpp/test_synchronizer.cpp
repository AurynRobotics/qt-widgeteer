#include <widgeteer/ElementFinder.h>
#include <widgeteer/Synchronizer.h>

#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QVBoxLayout>

using namespace widgeteer;

class TestSynchronizer : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Create test widget hierarchy
    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");

    auto* layout = new QVBoxLayout(testWindow_);

    button_ = new QPushButton("Test Button", testWindow_);
    button_->setObjectName("testButton");
    layout->addWidget(button_);

    enabledWidget_ = new QWidget(testWindow_);
    enabledWidget_->setObjectName("enabledWidget");
    enabledWidget_->setEnabled(true);
    layout->addWidget(enabledWidget_);

    disabledWidget_ = new QWidget(testWindow_);
    disabledWidget_->setObjectName("disabledWidget");
    disabledWidget_->setEnabled(false);
    layout->addWidget(disabledWidget_);

    testWindow_->show();
    QTest::qWait(100);
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  void cleanup() {
    // Process any pending events and wait a bit between tests
    // This helps ensure the waitForSignal tests don't affect each other
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QTest::qWait(50);  // Extra time to let timers settle
    QCoreApplication::processEvents(QEventLoop::AllEvents);
  }

  // === parseCondition() tests ===

  void testParseConditionExists() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("exists", propName, propValue),
             Synchronizer::Condition::Exists);
  }

  void testParseConditionNotExists() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("not_exists", propName, propValue),
             Synchronizer::Condition::NotExists);
  }

  void testParseConditionVisible() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("visible", propName, propValue),
             Synchronizer::Condition::Visible);
  }

  void testParseConditionNotVisible() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("not_visible", propName, propValue),
             Synchronizer::Condition::NotVisible);
  }

  void testParseConditionEnabled() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("enabled", propName, propValue),
             Synchronizer::Condition::Enabled);
  }

  void testParseConditionDisabled() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("disabled", propName, propValue),
             Synchronizer::Condition::Disabled);
  }

  void testParseConditionFocused() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("focused", propName, propValue),
             Synchronizer::Condition::Focused);
  }

  void testParseConditionStable() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("stable", propName, propValue),
             Synchronizer::Condition::Stable);
  }

  void testParseConditionIdle() {
    QString propName;
    QVariant propValue;
    QCOMPARE(Synchronizer::parseCondition("idle", propName, propValue),
             Synchronizer::Condition::Idle);
  }

  void testParseConditionPropertyEquals() {
    QString propName;
    QVariant propValue;
    auto result = Synchronizer::parseCondition("property:text=Hello", propName, propValue);
    QCOMPARE(result, Synchronizer::Condition::PropertyEquals);
    QCOMPARE(propName, QString("text"));
    QCOMPARE(propValue.toString(), QString("Hello"));
  }

  void testParseConditionPropertyEqualsNoValue() {
    QString propName;
    QVariant propValue;
    // Invalid property condition (no =) should default to Exists
    auto result = Synchronizer::parseCondition("property:text", propName, propValue);
    QCOMPARE(result, Synchronizer::Condition::Exists);
  }

  void testParseConditionUnknown() {
    QString propName;
    QVariant propValue;
    // Unknown condition defaults to Exists
    auto result = Synchronizer::parseCondition("unknown", propName, propValue);
    QCOMPARE(result, Synchronizer::Condition::Exists);
  }

  // === wait() tests ===

  void testWaitExistsSuccess() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:testButton";
    params.condition = Synchronizer::Condition::Exists;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testWaitExistsTimeout() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:nonexistent";
    params.condition = Synchronizer::Condition::Exists;
    params.timeoutMs = 100;

    auto result = sync.wait(params);
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
  }

  void testWaitNotExists() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:nonexistent";
    params.condition = Synchronizer::Condition::NotExists;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitVisible() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:testButton";
    params.condition = Synchronizer::Condition::Visible;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitNotVisible() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    // Create a hidden widget
    auto* hiddenWidget = new QWidget(testWindow_);
    hiddenWidget->setObjectName("hiddenWidget");
    hiddenWidget->hide();

    Synchronizer::WaitParams params;
    params.target = "@name:hiddenWidget";
    params.condition = Synchronizer::Condition::NotVisible;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);

    delete hiddenWidget;
  }

  void testWaitEnabled() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:enabledWidget";
    params.condition = Synchronizer::Condition::Enabled;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitDisabled() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:disabledWidget";
    params.condition = Synchronizer::Condition::Disabled;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitPropertyEquals() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:testButton";
    params.condition = Synchronizer::Condition::PropertyEquals;
    params.propertyName = "text";
    params.propertyValue = "Test Button";
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitPropertyEqualsNoProperty() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:testButton";
    params.condition = Synchronizer::Condition::PropertyEquals;
    params.propertyName = "";  // Empty property name
    params.timeoutMs = 100;

    auto result = sync.wait(params);
    QVERIFY(!result.success);
  }

  void testWaitIdle() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "";
    params.condition = Synchronizer::Condition::Idle;
    params.timeoutMs = 1000;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitStable() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    Synchronizer::WaitParams params;
    params.target = "@name:testButton";
    params.condition = Synchronizer::Condition::Stable;
    params.timeoutMs = 500;
    params.stabilityMs = 50;

    auto result = sync.wait(params);
    QVERIFY(result.success);
  }

  void testWaitStableWithGeometryChange() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    // Create widget that changes geometry
    auto* changingWidget = new QWidget(testWindow_);
    changingWidget->setObjectName("changingWidget");
    changingWidget->setGeometry(0, 0, 100, 100);

    // Schedule geometry change
    QTimer::singleShot(50, this,
                       [changingWidget]() { changingWidget->setGeometry(10, 10, 100, 100); });

    Synchronizer::WaitParams params;
    params.target = "@name:changingWidget";
    params.condition = Synchronizer::Condition::Stable;
    params.timeoutMs = 500;
    params.stabilityMs = 100;

    auto result = sync.wait(params);
    QVERIFY(result.success);

    delete changingWidget;
  }

  // === waitForIdle() tests ===

  void testWaitForIdleSuccess() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    auto result = sync.waitForIdle(1000);
    QVERIFY(result.success);
  }

  // === waitForSignal() tests ===

  void testWaitForSignalNullObject() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    auto result = sync.waitForSignal(nullptr, SIGNAL(destroyed()), 100);
    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Object is null"));
  }

  void testWaitForSignalSuccess() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    // Schedule a click after a short delay
    QTimer::singleShot(50, button_, &QPushButton::click);

    auto result = sync.waitForSignal(button_, SIGNAL(clicked()), 1000);
    QVERIFY(result.success);
  }

  void testWaitForSignalTimeout() {
    ElementFinder finder;
    Synchronizer sync(&finder);

    // Use a fresh button to avoid any residual state from previous test
    QPushButton tempButton("Temp", testWindow_);
    tempButton.show();
    QTest::qWait(10);

    auto result = sync.waitForSignal(&tempButton, SIGNAL(clicked()), 100);
    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Timeout waiting for signal"));
  }

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button_ = nullptr;
  QWidget* enabledWidget_ = nullptr;
  QWidget* disabledWidget_ = nullptr;
};

QTEST_MAIN(TestSynchronizer)
#include "test_synchronizer.moc"
