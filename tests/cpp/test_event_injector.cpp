#include <widgeteer/EventInjector.h>

#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSlider>
#include <QTest>
#include <QVBoxLayout>

using namespace widgeteer;

class TestEventInjector : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");

    auto* layout = new QVBoxLayout(testWindow_);

    button_ = new QPushButton("Click Me", testWindow_);
    button_->setObjectName("testButton");
    layout->addWidget(button_);

    checkbox_ = new QCheckBox("Check Me", testWindow_);
    checkbox_->setObjectName("testCheckbox");
    layout->addWidget(checkbox_);

    lineEdit_ = new QLineEdit(testWindow_);
    lineEdit_->setObjectName("testLineEdit");
    layout->addWidget(lineEdit_);

    slider_ = new QSlider(Qt::Horizontal, testWindow_);
    slider_->setObjectName("testSlider");
    slider_->setRange(0, 100);
    slider_->setValue(50);
    layout->addWidget(slider_);

    disabledButton_ = new QPushButton("Disabled", testWindow_);
    disabledButton_->setObjectName("disabledButton");
    disabledButton_->setEnabled(false);
    layout->addWidget(disabledButton_);

    testWindow_->show();
    QTest::qWait(100);
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  // === click() tests ===

  void testClickSuccess() {
    EventInjector injector;
    QSignalSpy spy(button_, &QPushButton::clicked);

    auto result = injector.click(button_);

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());

    // Wait for scheduled click to execute
    QVERIFY(spy.wait(100));
    QCOMPARE(spy.count(), 1);
  }

  void testClickNullWidget() {
    EventInjector injector;
    auto result = injector.click(nullptr);

    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Target widget is null"));
  }

  void testClickDisabledWidget() {
    EventInjector injector;
    auto result = injector.click(disabledButton_);

    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Target widget is not enabled"));
  }

  void testClickHiddenWidget() {
    EventInjector injector;
    auto* hiddenWidget = new QPushButton("Hidden", testWindow_);
    hiddenWidget->hide();

    auto result = injector.click(hiddenWidget);

    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Target widget is not visible"));

    delete hiddenWidget;
  }

  void testClickWithPosition() {
    EventInjector injector;

    // Click at specific position
    auto result = injector.click(button_, Qt::LeftButton, QPoint(5, 5));

    QVERIFY(result.success);
    QTest::qWait(50);
  }

  void testClickWithModifiers() {
    EventInjector injector;

    auto result = injector.click(button_, Qt::LeftButton, QPoint(), Qt::ControlModifier);

    QVERIFY(result.success);
    QTest::qWait(50);
  }

  // === doubleClick() tests ===

  void testDoubleClickSuccess() {
    EventInjector injector;

    auto result = injector.doubleClick(lineEdit_);

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testDoubleClickNullWidget() {
    EventInjector injector;
    auto result = injector.doubleClick(nullptr);

    QVERIFY(!result.success);
  }

  // === rightClick() tests ===

  void testRightClickSuccess() {
    EventInjector injector;

    auto result = injector.rightClick(button_);

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testRightClickNullWidget() {
    EventInjector injector;
    auto result = injector.rightClick(nullptr);

    QVERIFY(!result.success);
  }

  // === press() and release() tests ===

  void testPressAndRelease() {
    EventInjector injector;

    auto pressResult = injector.press(button_, Qt::LeftButton);
    QVERIFY(pressResult.success);

    auto releaseResult = injector.release(button_, Qt::LeftButton);
    QVERIFY(releaseResult.success);
  }

  void testPressNullWidget() {
    EventInjector injector;
    auto result = injector.press(nullptr, Qt::LeftButton);

    QVERIFY(!result.success);
  }

  void testReleaseNullWidget() {
    EventInjector injector;
    auto result = injector.release(nullptr, Qt::LeftButton);

    QVERIFY(!result.success);
  }

  // === move() tests ===

  void testMoveSuccess() {
    EventInjector injector;

    auto result = injector.move(button_, QPoint(10, 10));

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testMoveNullWidget() {
    EventInjector injector;
    auto result = injector.move(nullptr, QPoint(10, 10));

    QVERIFY(!result.success);
  }

  // === hover() tests ===

  void testHoverSuccess() {
    EventInjector injector;

    auto result = injector.hover(button_, QPoint(10, 10));

    QVERIFY(result.success);
  }

  // === drag() tests ===

  void testDragSuccess() {
    EventInjector injector;

    auto result = injector.drag(slider_, QPoint(10, 10), slider_, QPoint(50, 10));

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testDragNullSource() {
    EventInjector injector;
    auto result = injector.drag(nullptr, QPoint(), button_, QPoint());

    QVERIFY(!result.success);
  }

  void testDragNullDest() {
    EventInjector injector;
    auto result = injector.drag(button_, QPoint(), nullptr, QPoint());

    QVERIFY(!result.success);
  }

  // === scroll() tests ===

  void testScrollSuccess() {
    EventInjector injector;

    auto result = injector.scroll(testWindow_, 0, 120);

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
  }

  void testScrollNullWidget() {
    EventInjector injector;
    auto result = injector.scroll(nullptr, 0, 120);

    QVERIFY(!result.success);
  }

  void testScrollWithPosition() {
    EventInjector injector;

    auto result = injector.scroll(testWindow_, 0, 120, QPoint(50, 50));

    QVERIFY(result.success);
  }

  // === type() tests ===

  void testTypeSuccess() {
    EventInjector injector;
    lineEdit_->clear();

    auto result = injector.type(lineEdit_, "Hello World");

    QVERIFY(result.success);
    QCOMPARE(lineEdit_->text(), QString("Hello World"));
  }

  void testTypeNullWidget() {
    EventInjector injector;
    auto result = injector.type(nullptr, "text");

    QVERIFY(!result.success);
  }

  // === keyPress/keyRelease/keyClick() tests ===

  void testKeyPressSuccess() {
    EventInjector injector;

    auto result = injector.keyPress(lineEdit_, Qt::Key_A);

    QVERIFY(result.success);
  }

  void testKeyPressNullWidget() {
    EventInjector injector;
    auto result = injector.keyPress(nullptr, Qt::Key_A);

    QVERIFY(!result.success);
  }

  void testKeyReleaseSuccess() {
    EventInjector injector;

    auto result = injector.keyRelease(lineEdit_, Qt::Key_A);

    QVERIFY(result.success);
  }

  void testKeyReleaseNullWidget() {
    EventInjector injector;
    auto result = injector.keyRelease(nullptr, Qt::Key_A);

    QVERIFY(!result.success);
  }

  void testKeyClickSuccess() {
    EventInjector injector;

    auto result = injector.keyClick(lineEdit_, Qt::Key_A);

    QVERIFY(result.success);
  }

  void testKeyClickNullWidget() {
    EventInjector injector;
    auto result = injector.keyClick(nullptr, Qt::Key_A);

    QVERIFY(!result.success);
  }

  void testKeyClickWithModifiers() {
    EventInjector injector;
    lineEdit_->clear();
    lineEdit_->setFocus();

    // Ctrl+A typically selects all text
    auto result = injector.keyClick(lineEdit_, Qt::Key_A, Qt::ControlModifier);

    QVERIFY(result.success);
  }

  // === shortcut() tests ===

  void testShortcutSuccess() {
    EventInjector injector;

    QKeySequence seq("Ctrl+A");
    auto result = injector.shortcut(lineEdit_, seq);

    QVERIFY(result.success);
  }

  void testShortcutNullWidget() {
    EventInjector injector;
    auto result = injector.shortcut(nullptr, QKeySequence("Ctrl+A"));

    QVERIFY(!result.success);
  }

  void testShortcutMultiKey() {
    EventInjector injector;

    // Multi-key sequence like Ctrl+K, Ctrl+C
    QKeySequence seq("Ctrl+K, Ctrl+C");
    auto result = injector.shortcut(lineEdit_, seq);

    QVERIFY(result.success);
  }

  // === setFocus() tests ===

  void testSetFocusSuccess() {
    EventInjector injector;

    auto result = injector.setFocus(lineEdit_);

    QVERIFY(result.success);
    QVERIFY(lineEdit_->hasFocus());
  }

  void testSetFocusNullWidget() {
    EventInjector injector;
    auto result = injector.setFocus(nullptr);

    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Target widget is null"));
  }

  void testSetFocusHiddenWidget() {
    EventInjector injector;
    auto* hiddenWidget = new QLineEdit(testWindow_);
    hiddenWidget->hide();

    auto result = injector.setFocus(hiddenWidget);

    QVERIFY(!result.success);
    QCOMPARE(result.error, QString("Target widget is not visible"));

    delete hiddenWidget;
  }

  // === clearFocus() tests ===

  void testClearFocusSuccess() {
    EventInjector injector;

    // First set focus
    lineEdit_->setFocus();
    QApplication::processEvents();
    QVERIFY(lineEdit_->hasFocus());

    // Now clear it
    auto result = injector.clearFocus();

    QVERIFY(result.success);
  }

  void testClearFocusNoFocusedWidget() {
    EventInjector injector;

    // Clear focus when nothing has focus
    if (QWidget* focused = QApplication::focusWidget()) {
      focused->clearFocus();
      QApplication::processEvents();
    }

    auto result = injector.clearFocus();

    QVERIFY(result.success);  // Should succeed even if nothing focused
  }

  // === checkbox click (QAbstractButton) ===

  void testClickCheckbox() {
    EventInjector injector;
    checkbox_->setChecked(false);
    QSignalSpy spy(checkbox_, &QCheckBox::toggled);

    auto result = injector.click(checkbox_);

    QVERIFY(result.success);
    QVERIFY(spy.wait(100));
    QVERIFY(checkbox_->isChecked());
  }

  // === Additional tests for full coverage ===

  void testClickNonButtonWidget() {
    EventInjector injector;

    // Click on a non-button widget (QWidget container) uses QTest::mouseClick path
    auto result = injector.click(testWindow_, Qt::LeftButton, QPoint(10, 10));

    QVERIFY(result.success);
    QTest::qWait(50);
  }

  void testClickButtonWithSpecificPosition() {
    EventInjector injector;

    // Click at a non-center position takes the QTest::mouseClick branch
    // even for buttons
    auto result = injector.click(button_, Qt::LeftButton, QPoint(1, 1));

    QVERIFY(result.success);
    QTest::qWait(50);
  }

  void testClickButtonWithRightButton() {
    EventInjector injector;

    // Using right button on a QAbstractButton takes the QTest::mouseClick path
    auto result = injector.click(button_, Qt::RightButton);

    QVERIFY(result.success);
    QTest::qWait(50);
  }

  void testTypeWithFocusAlreadySet() {
    EventInjector injector;
    lineEdit_->clear();

    // Pre-set focus before typing
    lineEdit_->setFocus();
    QApplication::processEvents();
    QVERIFY(lineEdit_->hasFocus());

    auto result = injector.type(lineEdit_, "Already Focused");

    QVERIFY(result.success);
    QCOMPARE(lineEdit_->text(), QString("Already Focused"));
  }

  void testSetFocusOnWidgetInNonActiveWindow() {
    EventInjector injector;

    // Create a second window that's not active
    auto* secondWindow = new QWidget();
    secondWindow->setObjectName("secondWindow");
    auto* editInSecond = new QLineEdit(secondWindow);
    editInSecond->setObjectName("editInSecond");
    secondWindow->show();
    QTest::qWait(50);

    // Focus on widget in second window - should trigger window activation code
    auto result = injector.setFocus(editInSecond);

    // In offscreen/headless mode focus activation can fail, but the function
    // should either succeed cleanly or return a concrete error.
    QVERIFY(result.success || !result.error.isEmpty());
    if (result.success) {
      QVERIFY(editInSecond->hasFocus());
    } else {
      QCOMPARE(result.error, QString("Failed to set focus on widget"));
      QVERIFY(!editInSecond->hasFocus());
    }

    delete secondWindow;
  }

  void testClickWidgetDeletedBeforeTimerFires() {
    EventInjector injector;

    // Create a button that we'll delete after scheduling click
    auto* tempButtonRaw = new QPushButton("Temp", testWindow_);
    QPointer<QPushButton> tempButton = tempButtonRaw;
    tempButtonRaw->setObjectName("tempButton");
    tempButtonRaw->show();
    QTest::qWait(20);

    // Schedule the click
    auto result = injector.click(tempButtonRaw);
    QVERIFY(result.success);

    // Delete the button BEFORE the timer fires (timer is 10ms)
    delete tempButtonRaw;
    QVERIFY(tempButton.isNull());

    // Wait for timer to fire; process should remain stable.
    QTest::qWait(50);
  }

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button_ = nullptr;
  QCheckBox* checkbox_ = nullptr;
  QLineEdit* lineEdit_ = nullptr;
  QSlider* slider_ = nullptr;
  QPushButton* disabledButton_ = nullptr;
};

QTEST_MAIN(TestEventInjector)
#include "test_event_injector.moc"
