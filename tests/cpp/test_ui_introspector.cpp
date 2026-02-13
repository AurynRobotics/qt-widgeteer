#include <widgeteer/UIIntrospector.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace widgeteer;

class TestUIIntrospector : public QObject {
  Q_OBJECT

private slots:
  // === describe() tests ===

  void testDescribeNullWidget() {
    UIIntrospector introspector;
    QJsonObject result = introspector.describe(nullptr);
    QVERIFY(result.isEmpty());
  }

  void testDescribeBasicWidget() {
    UIIntrospector introspector;
    QWidget widget;
    widget.setObjectName("testWidget");

    QJsonObject result = introspector.describe(&widget);

    QCOMPARE(result.value("objectName").toString(), QString("testWidget"));
    QCOMPARE(result.value("class").toString(), QString("QWidget"));
    QVERIFY(result.contains("geometry"));
    QVERIFY(result.contains("globalPosition"));
  }

  void testDescribeWidgetWithAccessibility() {
    UIIntrospector introspector;
    QWidget widget;
    widget.setToolTip("This is a tooltip");
    widget.setStatusTip("This is a status tip");
    widget.setWhatsThis("This is what's this");
    widget.setAccessibleName("Accessible Name");
    widget.setAccessibleDescription("Accessible Description");

    QJsonObject result = introspector.describe(&widget);

    QCOMPARE(result.value("toolTip").toString(), QString("This is a tooltip"));
    QCOMPARE(result.value("statusTip").toString(), QString("This is a status tip"));
    QCOMPARE(result.value("whatsThis").toString(), QString("This is what's this"));
    QCOMPARE(result.value("accessibleName").toString(), QString("Accessible Name"));
    QCOMPARE(result.value("accessibleDescription").toString(), QString("Accessible Description"));
  }

  void testDescribeWindow() {
    UIIntrospector introspector;
    QWidget window;
    window.setWindowTitle("Test Window");
    window.setWindowFlags(Qt::Window);

    QJsonObject result = introspector.describe(&window);

    QCOMPARE(result.value("windowTitle").toString(), QString("Test Window"));
    QCOMPARE(result.value("role").toString(), QString("window"));
  }

  // === listProperties() tests ===

  void testListPropertiesNullWidget() {
    UIIntrospector introspector;
    QJsonArray result = introspector.listProperties(nullptr);
    QVERIFY(result.isEmpty());
  }

  void testListPropertiesWidget() {
    UIIntrospector introspector;
    QWidget widget;

    QJsonArray result = introspector.listProperties(&widget);

    QVERIFY(!result.isEmpty());
    // Check that standard properties are listed
    bool foundObjectName = false;
    for (const QJsonValue& val : result) {
      QJsonObject prop = val.toObject();
      if (prop.value("name").toString() == "objectName") {
        foundObjectName = true;
        QVERIFY(prop.value("readable").toBool());
        QVERIFY(prop.value("writable").toBool());
        QCOMPARE(prop.value("type").toString(), QString("QString"));
        break;
      }
    }
    QVERIFY(foundObjectName);
  }

  // === listActions() tests ===

  void testListActionsNullWidget() {
    UIIntrospector introspector;
    QJsonArray result = introspector.listActions(nullptr);
    QVERIFY(result.isEmpty());
  }

  void testListActionsWidget() {
    UIIntrospector introspector;
    QPushButton button("Test");

    QJsonArray result = introspector.listActions(&button);

    // Button has signals/slots
    QVERIFY(!result.isEmpty());
    bool foundClicked = false;
    for (const QJsonValue& val : result) {
      QJsonObject action = val.toObject();
      if (action.value("name").toString() == "clicked") {
        foundClicked = true;
        QCOMPARE(action.value("type").toString(), QString("signal"));
        break;
      }
    }
    QVERIFY(foundClicked);
  }

  void testListActionsWithQActions() {
    UIIntrospector introspector;
    QWidget widget;
    auto* action = new QAction("Test Action", &widget);
    action->setObjectName("testAction");
    action->setCheckable(true);
    action->setChecked(true);
    widget.addAction(action);

    QJsonArray result = introspector.listActions(&widget);

    bool foundAction = false;
    for (const QJsonValue& val : result) {
      QJsonObject act = val.toObject();
      if (act.value("name").toString() == "testAction") {
        foundAction = true;
        QCOMPARE(act.value("type").toString(), QString("qaction"));
        QCOMPARE(act.value("text").toString(), QString("Test Action"));
        QVERIFY(act.value("checkable").toBool());
        QVERIFY(act.value("checked").toBool());
        break;
      }
    }
    QVERIFY(foundAction);
  }

  // === inferWidgetRole() tests ===

  void testInferWidgetRoleNull() {
    UIIntrospector introspector;
    QCOMPARE(introspector.inferWidgetRole(nullptr), QString("unknown"));
  }

  void testInferWidgetRolePushButton() {
    UIIntrospector introspector;
    QPushButton btn;
    QCOMPARE(introspector.inferWidgetRole(&btn), QString("button"));
  }

  void testInferWidgetRoleRadioButton() {
    UIIntrospector introspector;
    QRadioButton radio;
    QCOMPARE(introspector.inferWidgetRole(&radio), QString("radio"));
  }

  void testInferWidgetRoleCheckBox() {
    UIIntrospector introspector;
    QCheckBox check;
    QCOMPARE(introspector.inferWidgetRole(&check), QString("checkbox"));
  }

  void testInferWidgetRoleLineEdit() {
    UIIntrospector introspector;
    QLineEdit edit;
    QCOMPARE(introspector.inferWidgetRole(&edit), QString("textfield"));
  }

  void testInferWidgetRoleTextEdit() {
    UIIntrospector introspector;
    QTextEdit edit;
    QCOMPARE(introspector.inferWidgetRole(&edit), QString("textarea"));
  }

  void testInferWidgetRoleComboBox() {
    UIIntrospector introspector;
    QComboBox combo;
    QCOMPARE(introspector.inferWidgetRole(&combo), QString("combobox"));
  }

  void testInferWidgetRoleSpinBox() {
    UIIntrospector introspector;
    QSpinBox spin;
    QCOMPARE(introspector.inferWidgetRole(&spin), QString("spinbox"));
  }

  void testInferWidgetRoleSlider() {
    UIIntrospector introspector;
    QSlider slider;
    QCOMPARE(introspector.inferWidgetRole(&slider), QString("slider"));
  }

  void testInferWidgetRoleProgressBar() {
    UIIntrospector introspector;
    QProgressBar progress;
    QCOMPARE(introspector.inferWidgetRole(&progress), QString("progressbar"));
  }

  void testInferWidgetRoleLabel() {
    UIIntrospector introspector;
    QLabel label;
    QCOMPARE(introspector.inferWidgetRole(&label), QString("label"));
  }

  void testInferWidgetRoleListWidget() {
    UIIntrospector introspector;
    QListWidget list;
    QCOMPARE(introspector.inferWidgetRole(&list), QString("list"));
  }

  void testInferWidgetRoleTreeWidget() {
    UIIntrospector introspector;
    QTreeWidget tree;
    QCOMPARE(introspector.inferWidgetRole(&tree), QString("tree"));
  }

  void testInferWidgetRoleTableWidget() {
    UIIntrospector introspector;
    QTableWidget table;
    QCOMPARE(introspector.inferWidgetRole(&table), QString("table"));
  }

  void testInferWidgetRoleTabWidget() {
    UIIntrospector introspector;
    QTabWidget tab;
    QCOMPARE(introspector.inferWidgetRole(&tab), QString("tabwidget"));
  }

  void testInferWidgetRoleMenuBar() {
    UIIntrospector introspector;
    QMenuBar menubar;
    QCOMPARE(introspector.inferWidgetRole(&menubar), QString("menubar"));
  }

  void testInferWidgetRoleMenu() {
    UIIntrospector introspector;
    QMenu menu;
    QCOMPARE(introspector.inferWidgetRole(&menu), QString("menu"));
  }

  void testInferWidgetRoleToolBar() {
    UIIntrospector introspector;
    QToolBar toolbar;
    QCOMPARE(introspector.inferWidgetRole(&toolbar), QString("toolbar"));
  }

  void testInferWidgetRoleStatusBar() {
    UIIntrospector introspector;
    QStatusBar statusbar;
    QCOMPARE(introspector.inferWidgetRole(&statusbar), QString("statusbar"));
  }

  void testInferWidgetRoleGroupBox() {
    UIIntrospector introspector;
    QGroupBox group;
    QCOMPARE(introspector.inferWidgetRole(&group), QString("group"));
  }

  void testInferWidgetRoleContainer() {
    UIIntrospector introspector;
    // Create as child of another widget to avoid it being a window
    QWidget parent;
    auto* container = new QWidget(&parent);
    auto* child = new QWidget(container);
    Q_UNUSED(child);
    QCOMPARE(introspector.inferWidgetRole(container), QString("container"));
  }

  void testInferWidgetRoleGenericWidget() {
    UIIntrospector introspector;
    // Create as child of another widget to avoid it being a window
    QWidget parent;
    auto* widget = new QWidget(&parent);
    QCOMPARE(introspector.inferWidgetRole(widget), QString("widget"));
  }

  void testInferWidgetRoleWindow() {
    UIIntrospector introspector;
    QWidget window;
    // Top-level widgets without parents are windows
    QCOMPARE(introspector.inferWidgetRole(&window), QString("window"));
  }

  // === getWidgetSpecificProps() tests ===

  void testSpecificPropsButton() {
    UIIntrospector introspector;
    QPushButton btn("Click Me");
    btn.setCheckable(true);
    btn.setChecked(true);

    QJsonObject result = introspector.describe(&btn);

    QCOMPARE(result.value("text").toString(), QString("Click Me"));
    QVERIFY(result.value("checkable").toBool());
    QVERIFY(result.value("checked").toBool());
  }

  void testSpecificPropsLabel() {
    UIIntrospector introspector;
    QLabel label("Label Text");

    QJsonObject result = introspector.describe(&label);

    QCOMPARE(result.value("text").toString(), QString("Label Text"));
  }

  void testSpecificPropsLineEdit() {
    UIIntrospector introspector;
    QLineEdit edit;
    edit.setText("Input text");
    edit.setPlaceholderText("Enter value");
    edit.setReadOnly(true);

    QJsonObject result = introspector.describe(&edit);

    QCOMPARE(result.value("text").toString(), QString("Input text"));
    QCOMPARE(result.value("placeholderText").toString(), QString("Enter value"));
    QVERIFY(result.value("readOnly").toBool());
  }

  void testSpecificPropsTextEdit() {
    UIIntrospector introspector;
    QTextEdit edit;
    edit.setPlainText("Multi-line text");
    edit.setReadOnly(true);

    QJsonObject result = introspector.describe(&edit);

    QCOMPARE(result.value("plainText").toString(), QString("Multi-line text"));
    QVERIFY(result.value("readOnly").toBool());
  }

  void testSpecificPropsComboBox() {
    UIIntrospector introspector;
    QComboBox combo;
    combo.addItems({ "Item 1", "Item 2", "Item 3" });
    combo.setCurrentIndex(1);

    QJsonObject result = introspector.describe(&combo);

    QCOMPARE(result.value("currentIndex").toInt(), 1);
    QCOMPARE(result.value("currentText").toString(), QString("Item 2"));
    QCOMPARE(result.value("count").toInt(), 3);
    QCOMPARE(result.value("items").toArray().size(), 3);
  }

  void testSpecificPropsSlider() {
    UIIntrospector introspector;
    QSlider slider;
    slider.setMinimum(0);
    slider.setMaximum(100);
    slider.setValue(50);

    QJsonObject result = introspector.describe(&slider);

    QCOMPARE(result.value("value").toInt(), 50);
    QCOMPARE(result.value("minimum").toInt(), 0);
    QCOMPARE(result.value("maximum").toInt(), 100);
  }

  void testSpecificPropsProgressBar() {
    UIIntrospector introspector;
    QProgressBar progress;
    progress.setMinimum(0);
    progress.setMaximum(100);
    progress.setValue(75);

    QJsonObject result = introspector.describe(&progress);

    QCOMPARE(result.value("value").toInt(), 75);
    QCOMPARE(result.value("minimum").toInt(), 0);
    QCOMPARE(result.value("maximum").toInt(), 100);
  }

  void testSpecificPropsGroupBox() {
    UIIntrospector introspector;
    QGroupBox group("Group Title");
    group.setCheckable(true);
    group.setChecked(true);

    QJsonObject result = introspector.describe(&group);

    QCOMPARE(result.value("title").toString(), QString("Group Title"));
    QVERIFY(result.value("checkable").toBool());
    QVERIFY(result.value("checked").toBool());
  }

  void testSpecificPropsTabWidget() {
    UIIntrospector introspector;
    QTabWidget tab;
    tab.addTab(new QWidget(), "Tab 1");
    tab.addTab(new QWidget(), "Tab 2");
    tab.setCurrentIndex(1);

    QJsonObject result = introspector.describe(&tab);

    QCOMPARE(result.value("currentIndex").toInt(), 1);
    QCOMPARE(result.value("count").toInt(), 2);
    QCOMPARE(result.value("tabs").toArray().size(), 2);
  }

  // === getTree() tests ===

  void testGetTreeNullRoot() {
    UIIntrospector introspector;
    // With no visible widgets, should return empty
    QJsonObject result = introspector.getTree(nullptr);
    // Result depends on what widgets exist - just verify it doesn't crash
    Q_UNUSED(result);
  }

  void testGetTreeExplicitRoot() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = true;  // Must include invisible since we're offscreen

    QJsonObject result = introspector.getTree(&root, opts);

    QCOMPARE(result.value("objectName").toString(), QString("root"));
  }

  void testGetTreeWithChildren() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");
    auto* child1 = new QPushButton("Button", &root);
    child1->setObjectName("child1");
    auto* child2 = new QLabel("Label", &root);
    child2->setObjectName("child2");

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = true;

    QJsonObject result = introspector.getTree(&root, opts);

    QCOMPARE(result.value("objectName").toString(), QString("root"));
    QJsonArray children = result.value("children").toArray();
    QCOMPARE(children.size(), 2);
  }

  void testGetTreeWithMaxDepth() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");
    auto* child = new QWidget(&root);
    child->setObjectName("child");
    auto* grandchild = new QWidget(child);
    grandchild->setObjectName("grandchild");

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = true;
    opts.maxDepth = 1;

    QJsonObject result = introspector.getTree(&root, opts);

    // Should have root and child, but not grandchild
    QJsonArray children = result.value("children").toArray();
    QCOMPARE(children.size(), 1);
    QJsonObject childObj = children.first().toObject();
    QVERIFY(!childObj.contains("children"));
  }

  void testGetTreeWithGeometry() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");

    UIIntrospector::TreeOptions opts;
    opts.includeGeometry = true;
    opts.includeInvisible = true;

    QJsonObject result = introspector.getTree(&root, opts);

    QVERIFY(result.contains("geometry"));
  }

  void testGetTreeWithoutGeometry() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");

    UIIntrospector::TreeOptions opts;
    opts.includeGeometry = false;
    opts.includeInvisible = true;

    QJsonObject result = introspector.getTree(&root, opts);

    QVERIFY(!result.contains("geometry"));
  }

  void testGetTreeWithProperties() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");

    UIIntrospector::TreeOptions opts;
    opts.includeProperties = true;
    opts.includeInvisible = true;

    QJsonObject result = introspector.getTree(&root, opts);

    QVERIFY(result.contains("properties"));
    QVERIFY(result.value("properties").toArray().size() > 0);
  }

  void testGetTreeWithClassFilter() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");
    auto* button = new QPushButton("Button", &root);
    button->setObjectName("btn");
    auto* label = new QLabel("Label", &root);
    label->setObjectName("lbl");

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = true;
    opts.classFilter = QStringList{ "QPushButton" };

    QJsonObject result = introspector.getTree(&root, opts);

    QVERIFY(!result.isEmpty());
    QCOMPARE(result.value("class").toString(), QString("QPushButton"));
    QCOMPARE(result.value("objectName").toString(), QString("btn"));
    QVERIFY(result.value("children").toArray().isEmpty());
  }

  void testGetTreeWithClassFilterNoMatch() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");
    auto* label = new QLabel("Label", &root);
    label->setObjectName("lbl");

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = true;
    opts.classFilter = QStringList{ "QPushButton" };

    QJsonObject result = introspector.getTree(&root, opts);

    // No matching widgets
    QVERIFY(result.isEmpty());
  }

  void testGetTreeVisibilityFilter() {
    UIIntrospector introspector;
    QWidget root;
    root.setObjectName("root");
    auto* visibleChild = new QPushButton("Visible", &root);
    visibleChild->setObjectName("visible");
    visibleChild->setVisible(true);
    auto* hiddenChild = new QPushButton("Hidden", &root);
    hiddenChild->setObjectName("hidden");
    hiddenChild->setVisible(false);
    root.show();
    QTest::qWait(50);

    UIIntrospector::TreeOptions opts;
    opts.includeInvisible = false;

    QJsonObject result = introspector.getTree(&root, opts);

    QVERIFY(!result.isEmpty());
    QJsonArray children = result.value("children").toArray();
    QCOMPARE(children.size(), 1);
    QCOMPARE(children[0].toObject().value("objectName").toString(), QString("visible"));
  }
};

QTEST_MAIN(TestUIIntrospector)
#include "test_ui_introspector.moc"
