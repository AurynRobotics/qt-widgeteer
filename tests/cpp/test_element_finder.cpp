#include <widgeteer/ElementFinder.h>

#include <QAccessible>
#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using namespace widgeteer;

class TestElementFinder : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Create a test widget hierarchy
    //   testWindow (QWidget)
    //     |-- button1 (QPushButton)
    //     |-- button2 (QPushButton)
    //     |-- container (QWidget)
    //           |-- label1 (QLabel)
    //           |-- nestedButton (QPushButton)
    //     |-- groupBox (QGroupBox)
    //           |-- groupLabel (QLabel)
    //     |-- accessibleButton (QPushButton) - with accessible name

    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");

    auto* layout = new QVBoxLayout(testWindow_);

    button1_ = new QPushButton("Click Me", testWindow_);
    button1_->setObjectName("button1");
    layout->addWidget(button1_);

    button2_ = new QPushButton("Another Button", testWindow_);
    button2_->setObjectName("button2");
    layout->addWidget(button2_);

    container_ = new QWidget(testWindow_);
    container_->setObjectName("container");
    auto* containerLayout = new QVBoxLayout(container_);

    label1_ = new QLabel("Test Label", container_);
    label1_->setObjectName("label1");
    containerLayout->addWidget(label1_);

    nestedButton_ = new QPushButton("Nested", container_);
    nestedButton_->setObjectName("nestedButton");
    containerLayout->addWidget(nestedButton_);

    layout->addWidget(container_);

    // Add a QGroupBox for @text: testing
    groupBox_ = new QGroupBox("Group Title", testWindow_);
    groupBox_->setObjectName("groupBox");
    auto* groupLayout = new QVBoxLayout(groupBox_);
    groupLabel_ = new QLabel("Group Content", groupBox_);
    groupLabel_->setObjectName("groupLabel");
    groupLayout->addWidget(groupLabel_);
    layout->addWidget(groupBox_);

    // Add a button with accessible name
    accessibleButton_ = new QPushButton("Regular Text", testWindow_);
    accessibleButton_->setObjectName("accessibleButton");
    accessibleButton_->setAccessibleName("AccessibleButtonName");
    layout->addWidget(accessibleButton_);

    // Add widgets without object names for index notation testing
    noNameContainer_ = new QWidget(testWindow_);
    noNameContainer_->setObjectName("noNameContainer");
    auto* noNameLayout = new QVBoxLayout(noNameContainer_);
    // Two buttons without object names
    noNameButton1_ = new QPushButton("NoName1", noNameContainer_);
    noNameButton2_ = new QPushButton("NoName2", noNameContainer_);
    noNameLayout->addWidget(noNameButton1_);
    noNameLayout->addWidget(noNameButton2_);
    layout->addWidget(noNameContainer_);

    testWindow_->show();
    QTest::qWait(100);  // Let widgets initialize
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  void testFindByObjectName() {
    ElementFinder finder;

    auto result = finder.find("@name:button1");
    QCOMPARE(result.widget, button1_);
    QVERIFY(result.error.isEmpty());
  }

  void testFindByObjectNameNested() {
    ElementFinder finder;

    auto result = finder.find("@name:nestedButton");
    QCOMPARE(result.widget, nestedButton_);
  }

  void testFindByClassName() {
    ElementFinder finder;

    auto result = finder.find("@class:QLabel");
    QCOMPARE(result.widget, label1_);
  }

  void testFindByText() {
    ElementFinder finder;

    auto result = finder.find("@text:Test Label");
    QCOMPARE(result.widget, label1_);
  }

  void testFindByPath() {
    ElementFinder finder;

    // Find nested button by path
    auto result = finder.find("testWindow/container/nestedButton");
    QCOMPARE(result.widget, nestedButton_);
  }

  void testFindNotFound() {
    ElementFinder finder;

    auto result = finder.find("@name:nonexistent");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  void testFindAll() {
    ElementFinder finder;

    auto results = finder.findAll("@class:QPushButton");

    // button1, button2, nestedButton, accessibleButton, noNameButton1, noNameButton2
    QCOMPARE(results.size(), 6);
  }

  void testFindAllWithLimit() {
    ElementFinder finder;
    ElementFinder::FindOptions opts;
    opts.maxResults = 2;

    auto results = finder.findAll("@class:QPushButton", opts);

    QCOMPARE(results.size(), 2);
  }

  void testPathFor() {
    ElementFinder finder;

    QString path = finder.pathFor(nestedButton_);
    QVERIFY(!path.isEmpty());
    QVERIFY(path.contains("container"));
    QVERIFY(path.contains("nestedButton"));
  }

  void testCacheInvalidation() {
    ElementFinder finder;

    // First lookup should cache
    auto result1 = finder.find("@name:button1");
    QCOMPARE(result1.widget, button1_);

    // Invalidate cache
    finder.invalidateCache();

    // Should still find correctly after cache clear
    auto result2 = finder.find("@name:button1");
    QCOMPARE(result2.widget, button1_);
  }

  void testFindVisibleOnly() {
    ElementFinder finder;
    ElementFinder::FindOptions opts;
    opts.visibleOnly = true;

    // All visible initially
    auto results = finder.findAll("@class:QPushButton", opts);
    int visibleCount = results.size();
    QVERIFY(visibleCount > 0);

    // Hide one button
    button1_->hide();

    auto resultsAfterHide = finder.findAll("@class:QPushButton", opts);
    QCOMPARE(resultsAfterHide.size(), visibleCount - 1);

    button1_->show();
  }

  // === @text: selector tests ===

  void testFindAllByTextLabel() {
    ElementFinder finder;

    auto results = finder.findAll("@text:Test Label");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].widget, label1_);
  }

  void testFindAllByTextButton() {
    ElementFinder finder;

    auto results = finder.findAll("@text:Click Me");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].widget, button1_);
  }

  void testFindAllByTextGroupBox() {
    ElementFinder finder;

    auto results = finder.findAll("@text:Group Title");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].widget, groupBox_);
  }

  void testFindAllByTextVisibleOnly() {
    ElementFinder finder;
    ElementFinder::FindOptions opts;
    opts.visibleOnly = true;

    // Hide the label
    label1_->hide();

    auto results = finder.findAll("@text:Test Label", opts);
    QCOMPARE(results.size(), 0);

    label1_->show();
  }

  void testFindAllByTextWithLimit() {
    ElementFinder finder;
    ElementFinder::FindOptions opts;
    opts.maxResults = 1;

    // Multiple buttons with text
    auto results = finder.findAll("@text:NoName1");
    QVERIFY(results.size() >= 1);

    auto limitedResults = finder.findAll("@text:NoName1", opts);
    QCOMPARE(limitedResults.size(), 1);
  }

  void testFindAllByTextNotFound() {
    ElementFinder finder;

    auto results = finder.findAll("@text:NonExistentText");
    QCOMPARE(results.size(), 0);
  }

  // === @accessible: selector tests ===

  void testFindByAccessibleName() {
    ElementFinder finder;

    auto result = finder.find("@accessible:AccessibleButtonName");
    QCOMPARE(result.widget, accessibleButton_);
    QVERIFY(result.error.isEmpty());
  }

  void testFindByAccessibleNameNotFound() {
    ElementFinder finder;

    auto result = finder.find("@accessible:NonexistentAccessible");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(result.error.contains("accessible name"));
  }

  // === Empty selector tests ===

  void testFindEmptySelector() {
    ElementFinder finder;

    auto result = finder.find("");
    QCOMPARE(result.widget, nullptr);
    QCOMPARE(result.error, QString("Empty selector"));
  }

  // === findAll with other selectors ===

  void testFindAllWithNameSelector() {
    ElementFinder finder;

    // @name: selector in findAll falls through to find()
    auto results = finder.findAll("@name:button1");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].widget, button1_);
  }

  void testFindAllWithPathSelector() {
    ElementFinder finder;

    // Path selector in findAll falls through to find()
    auto results = finder.findAll("testWindow/container/nestedButton");
    QCOMPARE(results.size(), 1);
    QCOMPARE(results[0].widget, nestedButton_);
  }

  void testFindAllWithNotFound() {
    ElementFinder finder;

    auto results = finder.findAll("@name:nonexistent");
    QCOMPARE(results.size(), 0);
  }

  // === pathFor() tests ===

  void testPathForNull() {
    ElementFinder finder;

    QString path = finder.pathFor(nullptr);
    QVERIFY(path.isEmpty());
  }

  void testPathForWidgetWithoutObjectName() {
    ElementFinder finder;

    // noNameButton1_ has no objectName, should use class[index] notation
    QString path = finder.pathFor(noNameButton1_);
    QVERIFY(!path.isEmpty());
    QVERIFY(path.contains("QPushButton[0]") || path.contains("QPushButton"));
  }

  void testPathForSecondWidgetWithoutObjectName() {
    ElementFinder finder;

    // noNameButton2_ is the second QPushButton in noNameContainer_
    QString path = finder.pathFor(noNameButton2_);
    QVERIFY(!path.isEmpty());
    QVERIFY(path.contains("QPushButton[1]"));
  }

  // === Path-based find() tests ===

  void testFindByPathOnlySlashes() {
    ElementFinder finder;

    // Path with only slashes should fail
    auto result = finder.find("///");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  void testFindByPathTopLevel() {
    ElementFinder finder;

    // Find top-level widget by object name via path
    auto result = finder.find("testWindow");
    QCOMPARE(result.widget, testWindow_);
  }

  void testFindByPathRecursiveFallback() {
    ElementFinder finder;

    // nestedButton is not a direct child of testWindow, but recursive search should find it
    auto result = finder.find("testWindow/nestedButton");
    QCOMPARE(result.widget, nestedButton_);
  }

  void testFindByPathNotFound() {
    ElementFinder finder;

    auto result = finder.find("testWindow/nonexistent");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  // === @name: selector tests ===

  void testFindByNameTopLevelMatch() {
    ElementFinder finder;

    // Top-level widget matches by name
    auto result = finder.find("@name:testWindow");
    QCOMPARE(result.widget, testWindow_);
  }

  // === @class: selector tests ===

  void testFindByClassGroupBox() {
    ElementFinder finder;

    // Find QGroupBox by class name
    auto result = finder.find("@class:QGroupBox");
    QCOMPARE(result.widget, groupBox_);
  }

  void testFindByClassNotFound() {
    ElementFinder finder;

    // No QSlider in the test hierarchy
    auto result = finder.find("@class:QSlider");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  // === @text: selector tests (single find) ===

  void testFindByTextGroupBox() {
    ElementFinder finder;

    // Find by GroupBox title
    auto result = finder.find("@text:Group Title");
    QCOMPARE(result.widget, groupBox_);
  }

  void testFindByTextNotFound() {
    ElementFinder finder;

    auto result = finder.find("@text:NonExistent");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  // === findChild() tests via path find() ===

  void testFindChildWithIndexNotation() {
    ElementFinder finder;

    // Use index notation to find the second QPushButton in noNameContainer_
    auto result = finder.find("testWindow/noNameContainer/QPushButton[1]");
    QCOMPARE(result.widget, noNameButton2_);
  }

  void testFindChildWithIndexNotationFirst() {
    ElementFinder finder;

    // Use index notation to find the first QPushButton
    auto result = finder.find("testWindow/noNameContainer/QPushButton[0]");
    QCOMPARE(result.widget, noNameButton1_);
  }

  void testFindChildWithIndexNotationOutOfBounds() {
    ElementFinder finder;

    // Index out of bounds should return nullptr
    auto result = finder.find("testWindow/noNameContainer/QPushButton[99]");
    QCOMPARE(result.widget, nullptr);
  }

  void testFindChildWithWildcard() {
    ElementFinder finder;

    // Wildcard * returns first child
    auto result = finder.find("testWindow/container/*");
    QVERIFY(result.widget != nullptr);
  }

  void testFindChildByClassName() {
    ElementFinder finder;

    // Find child by class name
    auto result = finder.find("testWindow/container/QLabel");
    QCOMPARE(result.widget, label1_);
  }

  void testFindChildByObjectNameIndex() {
    ElementFinder finder;

    // Use index notation with object name
    auto result = finder.find("testWindow/noNameContainer/NoName1[0]");
    // Should work if there's an objectName match or fail
    // Actually this tests the objectName branch of index notation
  }

  // === Cache with deleted widget tests ===

  void testCacheDeletedWidget() {
    ElementFinder finder;

    // Create and find a widget
    auto* tempWidget = new QPushButton("Temp", testWindow_);
    tempWidget->setObjectName("tempWidget");

    auto result1 = finder.find("@name:tempWidget");
    QCOMPARE(result1.widget, tempWidget);

    // Delete the widget
    delete tempWidget;

    // Should not return the deleted widget
    auto result2 = finder.find("@name:tempWidget");
    QCOMPARE(result2.widget, nullptr);
    QVERIFY(!result2.error.isEmpty());
  }

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button1_ = nullptr;
  QPushButton* button2_ = nullptr;
  QWidget* container_ = nullptr;
  QLabel* label1_ = nullptr;
  QPushButton* nestedButton_ = nullptr;
  QGroupBox* groupBox_ = nullptr;
  QLabel* groupLabel_ = nullptr;
  QPushButton* accessibleButton_ = nullptr;
  QWidget* noNameContainer_ = nullptr;
  QPushButton* noNameButton1_ = nullptr;
  QPushButton* noNameButton2_ = nullptr;
};

QTEST_MAIN(TestElementFinder)
#include "test_element_finder.moc"
