#include <widgeteer/ElementFinder.h>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

using namespace widgeteer;

class TestElementFinder : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Create a test widget hierarchy
    //   testWindow (QWidget)
    //     |-- button1 (QPushButton)
    //     |-- button2 (QPushButton)
    //     |-- container (QWidget)
    //           |-- label1 (QLabel)
    //           |-- nestedButton (QPushButton)

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

    testWindow_->show();
    QTest::qWait(100);  // Let widgets initialize
  }

  void cleanupTestCase()
  {
    delete testWindow_;
  }

  void testFindByObjectName()
  {
    ElementFinder finder;

    auto result = finder.find("@name:button1");
    QCOMPARE(result.widget, button1_);
    QVERIFY(result.error.isEmpty());
  }

  void testFindByObjectNameNested()
  {
    ElementFinder finder;

    auto result = finder.find("@name:nestedButton");
    QCOMPARE(result.widget, nestedButton_);
  }

  void testFindByClassName()
  {
    ElementFinder finder;

    auto result = finder.find("@class:QLabel");
    QCOMPARE(result.widget, label1_);
  }

  void testFindByText()
  {
    ElementFinder finder;

    auto result = finder.find("@text:Test Label");
    QCOMPARE(result.widget, label1_);
  }

  void testFindByPath()
  {
    ElementFinder finder;

    // Find nested button by path
    auto result = finder.find("testWindow/container/nestedButton");
    QCOMPARE(result.widget, nestedButton_);
  }

  void testFindNotFound()
  {
    ElementFinder finder;

    auto result = finder.find("@name:nonexistent");
    QCOMPARE(result.widget, nullptr);
    QVERIFY(!result.error.isEmpty());
  }

  void testFindAll()
  {
    ElementFinder finder;

    auto results = finder.findAll("@class:QPushButton");

    QCOMPARE(results.size(), 3);  // button1, button2, nestedButton
  }

  void testFindAllWithLimit()
  {
    ElementFinder finder;
    ElementFinder::FindOptions opts;
    opts.maxResults = 2;

    auto results = finder.findAll("@class:QPushButton", opts);

    QCOMPARE(results.size(), 2);
  }

  void testPathFor()
  {
    ElementFinder finder;

    QString path = finder.pathFor(nestedButton_);
    QVERIFY(!path.isEmpty());
    QVERIFY(path.contains("container"));
    QVERIFY(path.contains("nestedButton"));
  }

  void testCacheInvalidation()
  {
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

  void testFindVisibleOnly()
  {
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

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button1_ = nullptr;
  QPushButton* button2_ = nullptr;
  QWidget* container_ = nullptr;
  QLabel* label1_ = nullptr;
  QPushButton* nestedButton_ = nullptr;
};

QTEST_MAIN(TestElementFinder)
#include "test_element_finder.moc"
