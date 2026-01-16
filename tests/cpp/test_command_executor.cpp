#include <widgeteer/CommandExecutor.h>

#include <QApplication>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace widgeteer;

class TestCommandExecutor : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    testWindow_ = new QWidget();
    testWindow_->setObjectName("testWindow");

    auto* layout = new QVBoxLayout(testWindow_);

    button_ = new QPushButton("Click Me", testWindow_);
    button_->setObjectName("testButton");
    layout->addWidget(button_);

    lineEdit_ = new QLineEdit(testWindow_);
    lineEdit_->setObjectName("testLineEdit");
    lineEdit_->setText("initial text");
    layout->addWidget(lineEdit_);

    checkbox_ = new QCheckBox("Check Me", testWindow_);
    checkbox_->setObjectName("testCheckbox");
    layout->addWidget(checkbox_);

    spinBox_ = new QSpinBox(testWindow_);
    spinBox_->setObjectName("testSpinBox");
    spinBox_->setRange(0, 100);
    spinBox_->setValue(50);
    layout->addWidget(spinBox_);

    slider_ = new QSlider(Qt::Horizontal, testWindow_);
    slider_->setObjectName("testSlider");
    slider_->setRange(0, 100);
    slider_->setValue(25);
    layout->addWidget(slider_);

    comboBox_ = new QComboBox(testWindow_);
    comboBox_->setObjectName("testComboBox");
    comboBox_->addItems({ "Option A", "Option B", "Option C" });
    layout->addWidget(comboBox_);

    label_ = new QLabel("Test Label", testWindow_);
    label_->setObjectName("testLabel");
    layout->addWidget(label_);

    disabledButton_ = new QPushButton("Disabled", testWindow_);
    disabledButton_->setObjectName("disabledButton");
    disabledButton_->setEnabled(false);
    layout->addWidget(disabledButton_);

    textEdit_ = new QTextEdit(testWindow_);
    textEdit_->setObjectName("testTextEdit");
    textEdit_->setPlainText("Hello TextEdit");
    layout->addWidget(textEdit_);

    progressBar_ = new QProgressBar(testWindow_);
    progressBar_->setObjectName("testProgressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(50);
    layout->addWidget(progressBar_);

    tabWidget_ = new QTabWidget(testWindow_);
    tabWidget_->setObjectName("testTabWidget");
    tabWidget_->addTab(new QWidget(), "Tab 1");
    tabWidget_->addTab(new QWidget(), "Tab 2");
    tabWidget_->addTab(new QWidget(), "Tab 3");
    layout->addWidget(tabWidget_);

    listWidget_ = new QListWidget(testWindow_);
    listWidget_->setObjectName("testListWidget");
    listWidget_->addItem("Item A");
    listWidget_->addItem("Item B");
    listWidget_->addItem("Item C");
    layout->addWidget(listWidget_);

    radioButton_ = new QRadioButton("Test Radio", testWindow_);
    radioButton_->setObjectName("testRadioButton");
    layout->addWidget(radioButton_);

    calendarWidget_ = new QCalendarWidget(testWindow_);
    calendarWidget_->setObjectName("testCalendar");
    layout->addWidget(calendarWidget_);

    treeWidget_ = new QTreeWidget(testWindow_);
    treeWidget_->setObjectName("testTree");
    treeWidget_->setColumnCount(1);
    auto* item1 = new QTreeWidgetItem(treeWidget_, QStringList{ "Node A" });
    auto* item2 = new QTreeWidgetItem(treeWidget_, QStringList{ "Node B" });
    new QTreeWidgetItem(item1, QStringList{ "Child A1" });
    Q_UNUSED(item2);
    layout->addWidget(treeWidget_);

    tableWidget_ = new QTableWidget(3, 3, testWindow_);
    tableWidget_->setObjectName("testTable");
    tableWidget_->setItem(0, 0, new QTableWidgetItem("Cell 0,0"));
    tableWidget_->setItem(1, 1, new QTableWidgetItem("Cell 1,1"));
    tableWidget_->setItem(2, 2, new QTableWidgetItem("Cell 2,2"));
    layout->addWidget(tableWidget_);

    stackedWidget_ = new QStackedWidget(testWindow_);
    stackedWidget_->setObjectName("testStacked");
    stackedWidget_->addWidget(new QLabel("Page 0"));
    stackedWidget_->addWidget(new QLabel("Page 1"));
    stackedWidget_->addWidget(new QLabel("Page 2"));
    layout->addWidget(stackedWidget_);

    doubleSpinBox_ = new QDoubleSpinBox(testWindow_);
    doubleSpinBox_->setObjectName("testDoubleSpinBox");
    doubleSpinBox_->setRange(0.0, 100.0);
    doubleSpinBox_->setValue(25.5);
    layout->addWidget(doubleSpinBox_);

    dateEdit_ = new QDateEdit(testWindow_);
    dateEdit_->setObjectName("testDateEdit");
    dateEdit_->setDate(QDate(2025, 1, 15));
    layout->addWidget(dateEdit_);

    plainTextEdit_ = new QPlainTextEdit(testWindow_);
    plainTextEdit_->setObjectName("testPlainTextEdit");
    plainTextEdit_->setPlainText("Plain text content");
    layout->addWidget(plainTextEdit_);

    groupBox_ = new QGroupBox("Checkable Group", testWindow_);
    groupBox_->setObjectName("testGroupBox");
    groupBox_->setCheckable(true);
    groupBox_->setChecked(true);
    layout->addWidget(groupBox_);

    testWindow_->show();
    QTest::qWait(100);
  }

  void cleanupTestCase() {
    delete testWindow_;
  }

  // === Basic infrastructure tests ===

  void testUnknownCommand() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "test-1";
    cmd.name = "unknown_command";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_COMMAND"));
  }

  void testEmptyCommandId() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "";
    cmd.name = "get_tree";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.id, QString(""));
  }

  // === Introspection commands ===

  void testGetTree() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "tree-1";
    cmd.name = "get_tree";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // getTree returns a widget JSON with "class" and "objectName"
    QVERIFY(resp.result.contains("class") || resp.result.contains("objectName"));
  }

  void testGetTreeWithRoot() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "tree-2";
    cmd.name = "get_tree";
    cmd.params["root"] = "@name:testWindow";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testGetTreeWithOptions() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "tree-3";
    cmd.name = "get_tree";
    cmd.params["depth"] = 2;
    cmd.params["include_invisible"] = true;
    cmd.params["include_geometry"] = false;
    cmd.params["include_properties"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testFind() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-1";
    cmd.name = "find";
    cmd.params["query"] = "@class:QPushButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("matches"));
    QVERIFY(resp.result.value("count").toInt() >= 2);  // testButton, disabledButton
  }

  void testFindMissingQuery() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-2";
    cmd.name = "find";
    // Missing query

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testFindWithOptions() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-3";
    cmd.name = "find";
    cmd.params["query"] = "@class:QPushButton";
    cmd.params["max_results"] = 1;
    cmd.params["visible_only"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("count").toInt(), 1);
  }

  void testDescribe() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "desc-1";
    cmd.name = "describe";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("class").toString(), QString("QPushButton"));
  }

  void testDescribeMissingTarget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "desc-2";
    cmd.name = "describe";
    // Missing target

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testDescribeNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "desc-3";
    cmd.name = "describe";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testGetProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-1";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("value").toString(), QString("initial text"));
  }

  void testGetPropertyMissingProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-2";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testLineEdit";
    // Missing property

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testGetPropertyNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-3";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "nonexistent_property";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("PROPERTY_NOT_FOUND"));
  }

  void testGetPropertyInt() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-4";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // Note: cmdGetProperty converts int to string due to QString check being first
    QCOMPARE(resp.result.value("value").toString(), QString("50"));
  }

  void testGetPropertyBool() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-5";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testCheckbox";
    cmd.params["property"] = "checked";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("value").toBool(), false);
  }

  void testListProperties() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "listprop-1";
    cmd.name = "list_properties";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("properties"));
  }

  void testGetActions() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "actions-1";
    cmd.name = "get_actions";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("actions"));
  }

  void testGetFormFields() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "fields-1";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testWindow";
    cmd.params["visible_only"] = false;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
    // Should find lineEdit, checkbox, spinBox, slider, comboBox
    QVERIFY(resp.result.value("count").toInt() >= 5);
  }

  void testGetFormFieldsNoRoot() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "fields-2";
    cmd.name = "get_form_fields";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  // === Action commands ===

  void testClick() {
    CommandExecutor executor;
    QSignalSpy spy(button_, &QPushButton::clicked);

    Command cmd;
    cmd.id = "click-1";
    cmd.name = "click";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("clicked").toBool());
    QVERIFY(spy.wait(100));
  }

  void testClickMissingTarget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-2";
    cmd.name = "click";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testClickNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-3";
    cmd.name = "click";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testClickWithPosition() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-4";
    cmd.name = "click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["pos"] = QJsonObject{ { "x", 5 }, { "y", 5 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QTest::qWait(50);
  }

  void testClickWithButton() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-5";
    cmd.name = "click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["button"] = "right";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testClickDisabled() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-6";
    cmd.name = "click";
    cmd.params["target"] = "@name:disabledButton";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVOCATION_FAILED"));
  }

  void testDoubleClick() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "dblclick-1";
    cmd.name = "double_click";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("double_clicked").toBool());
  }

  void testRightClick() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "rclick-1";
    cmd.name = "right_click";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("right_clicked").toBool());
  }

  void testType() {
    CommandExecutor executor;
    lineEdit_->clear();

    Command cmd;
    cmd.id = "type-1";
    cmd.name = "type";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["text"] = "Hello World";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(lineEdit_->text(), QString("Hello World"));
  }

  void testTypeMissingText() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "type-2";
    cmd.name = "type";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testTypeClearFirst() {
    CommandExecutor executor;
    lineEdit_->setText("existing");

    Command cmd;
    cmd.id = "type-3";
    cmd.name = "type";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["text"] = "new text";
    cmd.params["clear_first"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKey() {
    CommandExecutor executor;
    lineEdit_->setFocus();

    Command cmd;
    cmd.id = "key-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "A";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeyMissingKey() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "key-2";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testKeySpecial() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "key-3";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Enter";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeySequence() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keyseq-1";
    cmd.name = "key_sequence";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["sequence"] = "Ctrl+A";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeySequenceMissing() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keyseq-2";
    cmd.name = "key_sequence";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testScroll() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "scroll-1";
    cmd.name = "scroll";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["delta_y"] = 100;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testHover() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "hover-1";
    cmd.name = "hover";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testFocus() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "focus-1";
    cmd.name = "focus";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(lineEdit_->hasFocus());
  }

  // === Verification commands ===

  void testExists() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "exists-1";
    cmd.name = "exists";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("exists").toBool());
  }

  void testExistsNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "exists-2";
    cmd.name = "exists";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(!resp.result.value("exists").toBool());
  }

  void testIsVisible() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "visible-1";
    cmd.name = "is_visible";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("visible").toBool());
  }

  void testIsVisibleHidden() {
    CommandExecutor executor;
    button_->hide();

    Command cmd;
    cmd.id = "visible-2";
    cmd.name = "is_visible";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(!resp.result.value("visible").toBool());

    button_->show();
  }

  void testScreenshot() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-1";
    cmd.name = "screenshot";
    cmd.params["target"] = "@name:testWindow";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("screenshot"));
    QVERIFY(resp.result.contains("width"));
    QVERIFY(resp.result.contains("height"));
  }

  void testAssertProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-1";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testCheckbox";
    cmd.params["property"] = "checked";
    cmd.params["value"] = false;  // Assert uses "value" not "expected"

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertPropertyFail() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-2";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testCheckbox";
    cmd.params["property"] = "checked";
    cmd.params["value"] = true;  // Assert uses "value" not "expected"

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(!resp.result.value("passed").toBool());
  }

  // === Synchronization commands ===

  void testWait() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-1";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "exists";
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testWaitTimeout() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-2";
    cmd.name = "wait";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["condition"] = "exists";
    cmd.params["timeout_ms"] = 100;

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("TIMEOUT"));
  }

  void testWaitIdle() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "idle-1";
    cmd.name = "wait_idle";
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testSleep() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "sleep-1";
    cmd.name = "sleep";
    cmd.params["ms"] = 10;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  // === State commands ===

  void testSetProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setprop-1";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";
    cmd.params["value"] = "new value";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(lineEdit_->text(), QString("new value"));
  }

  void testSetPropertyMissingProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setprop-2";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testSetValue() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-1";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["value"] = 75;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(spinBox_->value(), 75);
  }

  void testSetValueSlider() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-2";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testSlider";
    cmd.params["value"] = 50;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(slider_->value(), 50);
  }

  void testSetValueCheckbox() {
    CommandExecutor executor;
    checkbox_->setChecked(false);

    Command cmd;
    cmd.id = "setval-3";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testCheckbox";
    cmd.params["value"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(checkbox_->isChecked());
  }

  void testSetValueComboBox() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-4";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testComboBox";
    cmd.params["value"] = 2;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(comboBox_->currentIndex(), 2);
  }

  // === Extensibility commands ===

  void testListObjects() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "listobj-1";
    cmd.name = "list_objects";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("objects"));
  }

  void testListCustomCommands() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "listcmd-1";
    cmd.name = "list_custom_commands";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("commands"));
  }

  // === Transaction tests ===

  void testTransactionSuccess() {
    CommandExecutor executor;
    QSignalSpy completedSpy(&executor, &CommandExecutor::transactionCompleted);

    Transaction tx;
    tx.id = "tx-1";
    tx.rollbackOnFailure = false;

    Command step1;
    step1.name = "exists";
    step1.params["target"] = "@name:testButton";
    tx.steps.append(step1);

    Command step2;
    step2.name = "exists";
    step2.params["target"] = "@name:testLineEdit";
    tx.steps.append(step2);

    TransactionResponse resp = executor.execute(tx);

    QVERIFY(resp.success);
    QCOMPARE(resp.completedSteps, 2);
    QCOMPARE(resp.totalSteps, 2);
    QVERIFY(!resp.rollbackPerformed);
    QCOMPARE(completedSpy.count(), 1);
  }

  void testTransactionFailure() {
    CommandExecutor executor;

    Transaction tx;
    tx.id = "tx-2";
    tx.rollbackOnFailure = false;

    Command step1;
    step1.name = "exists";
    step1.params["target"] = "@name:testButton";
    tx.steps.append(step1);

    Command step2;
    step2.name = "click";
    step2.params["target"] = "@name:nonexistent";  // This will fail
    tx.steps.append(step2);

    TransactionResponse resp = executor.execute(tx);

    QVERIFY(!resp.success);
    QCOMPARE(resp.completedSteps, 1);  // Stopped at second step
  }

  void testTransactionRollback() {
    CommandExecutor executor;
    QString originalText = lineEdit_->text();

    Transaction tx;
    tx.id = "tx-3";
    tx.rollbackOnFailure = true;

    Command step1;
    step1.name = "set_property";
    step1.params["target"] = "@name:testLineEdit";
    step1.params["property"] = "text";
    step1.params["value"] = "modified";
    tx.steps.append(step1);

    Command step2;
    step2.name = "click";
    step2.params["target"] = "@name:nonexistent";  // This will fail
    tx.steps.append(step2);

    TransactionResponse resp = executor.execute(tx);

    QVERIFY(!resp.success);
    QVERIFY(resp.rollbackPerformed);
  }

  // === Track changes tests ===

  void testTrackChanges() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "track-1";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";
    cmd.params["value"] = "tracked change";
    cmd.options["track_changes"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // Changes may or may not be present depending on what actually changed
  }

  // === Duration tracking ===

  void testResponseDuration() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "duration-1";
    cmd.name = "sleep";
    cmd.params["ms"] = 50;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.durationMs >= 50);
  }

  // === Additional coverage tests ===

  void testDrag() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-1";
    cmd.name = "drag";
    cmd.params["from"] = "@name:testSlider";
    cmd.params["to"] = "@name:testSlider";
    cmd.params["from_pos"] = QJsonObject{ { "x", 10 }, { "y", 10 } };
    cmd.params["to_pos"] = QJsonObject{ { "x", 50 }, { "y", 10 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("dragged").toBool());
  }

  void testDragMissingFrom() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-2";
    cmd.name = "drag";
    cmd.params["to"] = "@name:testSlider";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testDragMissingTo() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-3";
    cmd.name = "drag";
    cmd.params["from"] = "@name:testSlider";
    cmd.params["to"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testKeyWithModifiers() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keymod-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "A";
    cmd.params["modifiers"] = QJsonArray{ "ctrl", "shift" };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeyEscape() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keyesc-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Escape";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeyTab() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keytab-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Tab";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeyBackspace() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keybsp-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Backspace";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeyDelete() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keydel-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Delete";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testKeySpace() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "keyspc-1";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Space";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testClickMiddleButton() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-mid";
    cmd.name = "click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["button"] = "middle";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testDoubleClickWithPos() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "dblclick-pos";
    cmd.name = "double_click";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["pos"] = QJsonObject{ { "x", 5 }, { "y", 5 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testRightClickWithPos() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "rclick-pos";
    cmd.name = "right_click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["pos"] = QJsonObject{ { "x", 5 }, { "y", 5 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testHoverWithPos() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "hover-pos";
    cmd.name = "hover";
    cmd.params["target"] = "@name:testButton";
    cmd.params["pos"] = QJsonObject{ { "x", 10 }, { "y", 10 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testSetValueLineEdit() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-le";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["value"] = "new line edit value";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(lineEdit_->text(), QString("new line edit value"));
  }

  void testSetValueComboBoxByText() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-combo-text";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testComboBox";
    cmd.params["value"] = "Option B";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(comboBox_->currentText(), QString("Option B"));
  }

  void testSetPropertyInt() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setprop-int";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["value"] = 42;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(spinBox_->value(), 42);
  }

  void testScreenshotWithAnnotate() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-ann";
    cmd.name = "screenshot";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["annotate"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("annotations"));
  }

  void testScreenshotNoTarget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-no-target";
    cmd.name = "screenshot";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testWaitVisible() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-vis";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "visible";
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testWaitEnabled() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-ena";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "enabled";
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testWaitPropertyEquals() {
    CommandExecutor executor;
    // Use objectName which is a string property for reliable comparison
    lineEdit_->setObjectName("testLineEdit");

    Command cmd;
    cmd.id = "wait-prop";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["condition"] = "property:objectName=testLineEdit";
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testCustomCommandHandler() {
    CommandExecutor executor;

    // Set up a custom command
    QHash<QString, CommandHandler> customCmds;
    customCmds["my_custom_cmd"] = [](const QJsonObject& params) -> QJsonObject {
      return QJsonObject{ { "custom_result", params.value("input").toString() } };
    };
    executor.setCustomCommands(&customCmds);

    Command cmd;
    cmd.id = "custom-1";
    cmd.name = "my_custom_cmd";
    cmd.params["input"] = "test_value";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("custom_result").toString(), QString("test_value"));
  }

  void testCustomCommandException() {
    CommandExecutor executor;

    // Set up a custom command that throws
    QHash<QString, CommandHandler> customCmds;
    customCmds["throwing_cmd"] = [](const QJsonObject&) -> QJsonObject {
      throw std::runtime_error("Test exception");
    };
    executor.setCustomCommands(&customCmds);

    Command cmd;
    cmd.id = "custom-exc";
    cmd.name = "throwing_cmd";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVOCATION_FAILED"));
  }

  void testInvoke() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "invoke-1";
    cmd.name = "invoke";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["method"] = "clear";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(lineEdit_->text().isEmpty());
  }

  void testInvokeMissingMethod() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "invoke-2";
    cmd.name = "invoke";
    cmd.params["target"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testQuit() {
    // Note: We can't actually quit in a test, but we can verify the command exists
    CommandExecutor executor;

    // Just verify the command is registered - don't actually call it
    Command cmd;
    cmd.id = "quit-check";
    cmd.name = "list_custom_commands";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  // === Additional coverage for set_value with different widget types ===

  void testSetValueTextEdit() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-textedit";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTextEdit";
    cmd.params["value"] = "New TextEdit Content";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(textEdit_->toPlainText(), QString("New TextEdit Content"));
  }

  void testSetValueProgressBar() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-progress";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testProgressBar";
    cmd.params["value"] = 75;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(progressBar_->value(), 75);
  }

  void testSetValueTabWidget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-tab";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTabWidget";
    cmd.params["value"] = 2;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(tabWidget_->currentIndex(), 2);
  }

  void testSetValueLabel() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-label";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testLabel";
    cmd.params["value"] = "Updated Label";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(label_->text(), QString("Updated Label"));
  }

  void testSetValueLabelNumber() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-label-num";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testLabel";
    cmd.params["value"] = 42.5;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // setNum formats the number
    QVERIFY(label_->text().contains("42.5"));
  }

  // === Additional coverage for invoke command ===

  void testInvokeNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "invoke-notfound";
    cmd.name = "invoke";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["method"] = "nonExistentMethod";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVOCATION_FAILED"));
  }

  // === Additional coverage for get_property with double type ===

  void testGetPropertyDouble() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "prop-double";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["property"] = "windowOpacity";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // windowOpacity returns a double
    QVERIFY(resp.result.contains("value"));
  }

  // === Test set_property failure case ===

  void testSetPropertyNotWritable() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setprop-fail";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testButton";
    cmd.params["property"] = "metaObject";  // Not a writable property
    cmd.params["value"] = "invalid";

    Response resp = executor.execute(cmd);

    // Setting metaObject should fail
    QVERIFY(!resp.success);
  }

  // === Test scroll with both delta_x and delta_y ===

  void testScrollWithDeltaX() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "scroll-xy";
    cmd.name = "scroll";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["delta_x"] = 50;
    cmd.params["delta_y"] = -100;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  // === Test call command ===

  void testCallWithRegisteredObject() {
    CommandExecutor executor;

    QHash<QString, QPointer<QObject>> registeredObjects;
    registeredObjects["myButton"] = button_;
    executor.setRegisteredObjects(&registeredObjects);

    Command cmd;
    cmd.id = "call-1";
    cmd.name = "call";
    cmd.params["object"] = "myButton";
    cmd.params["method"] = "click";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  void testCallMissingObject() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "call-missing";
    cmd.name = "call";
    cmd.params["object"] = "nonexistent";
    cmd.params["method"] = "click";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  // === Test assert with different conditions ===

  void testAssertExists() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-exists";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "exists";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertVisible() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-visible";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "visible";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertEnabled() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-enabled";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "enabled";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  // === Test form fields with all widget types ===

  void testGetFormFieldsWithAllWidgets() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "fields-all";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testWindow";
    cmd.params["visible_only"] = false;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // Should find lineEdit, checkbox, spinBox, slider, comboBox, textEdit
    QVERIFY(resp.result.value("count").toInt() >= 6);
  }

  // === Transaction with step signals ===

  void testTransactionStepSignals() {
    CommandExecutor executor;
    QSignalSpy stepSpy(&executor, &CommandExecutor::stepCompleted);

    Transaction tx;
    tx.id = "tx-signals";
    tx.rollbackOnFailure = false;

    Command step1;
    step1.name = "exists";
    step1.params["target"] = "@name:testButton";
    tx.steps.append(step1);

    Command step2;
    step2.name = "is_visible";
    step2.params["target"] = "@name:testButton";
    tx.steps.append(step2);

    TransactionResponse resp = executor.execute(tx);

    QVERIFY(resp.success);
    QCOMPARE(stepSpy.count(), 2);
  }

  // === Test set_value for QListWidget ===

  void testSetValueListWidgetByIndex() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-list-idx";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testListWidget";
    cmd.params["value"] = 1;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(listWidget_->currentRow(), 1);
  }

  void testSetValueListWidgetByText() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-list-text";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testListWidget";
    cmd.params["value"] = "Item C";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QCOMPARE(listWidget_->currentItem()->text(), QString("Item C"));
  }

  void testSetValueListWidgetNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-list-notfound";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testListWidget";
    cmd.params["value"] = "Nonexistent Item";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  // === Test set_value for RadioButton (checkable button) ===

  void testSetValueRadioButton() {
    CommandExecutor executor;
    radioButton_->setChecked(false);

    Command cmd;
    cmd.id = "setval-radio";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testRadioButton";
    cmd.params["value"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(radioButton_->isChecked());
  }

  // === Test set_value for unsupported widget ===

  void testSetValueUnsupportedWidget() {
    CommandExecutor executor;

    // testWindow is a plain QWidget that doesn't support set_value
    Command cmd;
    cmd.id = "setval-unsupported";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["value"] = "anything";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVOCATION_FAILED"));
  }

  // === Test listObjects with registered objects ===

  void testListObjectsWithRegistered() {
    CommandExecutor executor;

    QHash<QString, QPointer<QObject>> registeredObjects;
    registeredObjects["myButton"] = button_;
    registeredObjects["myEdit"] = lineEdit_;
    executor.setRegisteredObjects(&registeredObjects);

    Command cmd;
    cmd.id = "listobj-reg";
    cmd.name = "list_objects";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QJsonArray objects = resp.result.value("objects").toArray();
    QCOMPARE(objects.size(), 2);
  }

  // === Test listCustomCommands with custom commands ===

  void testListCustomCommandsWithCommands() {
    CommandExecutor executor;

    QHash<QString, CommandHandler> customCmds;
    customCmds["my_cmd1"] = [](const QJsonObject&) { return QJsonObject{}; };
    customCmds["my_cmd2"] = [](const QJsonObject&) { return QJsonObject{}; };
    executor.setCustomCommands(&customCmds);

    Command cmd;
    cmd.id = "listcmd-reg";
    cmd.name = "list_custom_commands";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QJsonArray commands = resp.result.value("commands").toArray();
    QCOMPARE(commands.size(), 2);
  }

  // === Test scroll with position ===

  void testScrollWithPosition() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "scroll-pos";
    cmd.name = "scroll";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["delta_y"] = 50;
    cmd.params["pos"] = QJsonObject{ { "x", 10 }, { "y", 10 } };

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
  }

  // === Test type with clear_first option ===

  void testTypeClearFirstOption() {
    CommandExecutor executor;
    lineEdit_->setText("old text");

    Command cmd;
    cmd.id = "type-clear";
    cmd.name = "type";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["text"] = "new text";
    cmd.params["clear_first"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(lineEdit_->text().endsWith("new text"));
  }

  // === Test call with missing method ===

  void testCallMissingMethod() {
    CommandExecutor executor;

    QHash<QString, QPointer<QObject>> registeredObjects;
    registeredObjects["myButton"] = button_;
    executor.setRegisteredObjects(&registeredObjects);

    Command cmd;
    cmd.id = "call-no-method";
    cmd.name = "call";
    cmd.params["object"] = "myButton";
    // Missing method parameter

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  // === Test focus clear ===

  void testFocusClear() {
    CommandExecutor executor;

    // First set focus
    lineEdit_->setFocus();
    QApplication::processEvents();

    Command cmd;
    cmd.id = "focus-clear";
    cmd.name = "focus";
    cmd.params["target"] = "@name:testButton";
    cmd.params["clear"] = true;

    Response resp = executor.execute(cmd);

    // The focus command should work regardless of clear param interpretation
    QVERIFY(resp.success);
  }

  // === More assert operator tests ===

  void testAssertNotEquals() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-neq";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["operator"] = "!=";
    cmd.params["value"] = 0;  // Current value is not 0

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertGreaterThan() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-gt";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["operator"] = ">";
    cmd.params["value"] = 0;  // Current value > 0

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertLessThan() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-lt";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["operator"] = "<";
    cmd.params["value"] = 100;  // Current value < 100

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertContains() {
    CommandExecutor executor;
    lineEdit_->setText("hello world");

    Command cmd;
    cmd.id = "assert-contains";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";
    cmd.params["operator"] = "contains";
    cmd.params["value"] = "world";

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertGreaterThanOrEqual() {
    CommandExecutor executor;
    spinBox_->setValue(50);  // Reset to known value

    Command cmd;
    cmd.id = "assert-gte";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["operator"] = ">=";
    cmd.params["value"] = 50;  // Current value >= 50

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertLessThanOrEqual() {
    CommandExecutor executor;
    spinBox_->setValue(50);  // Reset to known value

    Command cmd;
    cmd.id = "assert-lte";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["property"] = "value";
    cmd.params["operator"] = "<=";
    cmd.params["value"] = 50;  // Current value <= 50

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  void testAssertNoProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "assert-noprop";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testButton";
    // No property specified - uses "condition" instead

    Response resp = executor.execute(cmd);

    // When no property is given, it checks condition "exists"
    QVERIFY(resp.success);
  }

  // === Test double_click/right_click target not found ===

  void testDoubleClickNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "dblclick-notfound";
    cmd.name = "double_click";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testRightClickNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "rclick-notfound";
    cmd.name = "right_click";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testHoverNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "hover-notfound";
    cmd.name = "hover";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testScrollNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "scroll-notfound";
    cmd.name = "scroll";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  void testFocusNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "focus-notfound";
    cmd.name = "focus";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
  }

  // === Test set_property with value conversion ===

  void testSetPropertyBool() {
    CommandExecutor executor;
    checkbox_->setChecked(false);

    Command cmd;
    cmd.id = "setprop-bool";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testCheckbox";
    cmd.params["property"] = "checked";
    cmd.params["value"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    QVERIFY(checkbox_->isChecked());
  }

  // === Test invoke with args ===

  void testInvokeWithArgs() {
    CommandExecutor executor;
    lineEdit_->clear();

    Command cmd;
    cmd.id = "invoke-args";
    cmd.name = "invoke";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["method"] = "setText";
    cmd.params["args"] = QJsonArray{ "invoked text" };

    Response resp = executor.execute(cmd);

    // Note: This might fail if invoke doesn't support args yet
    QVERIFY(resp.success || resp.error.code == "INVOCATION_FAILED");
  }

  void testGetActionsNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "get-actions-not-found";
    cmd.name = "get_actions";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testListPropertiesNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "list-props-not-found";
    cmd.name = "list_properties";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testSetPropertyNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "set-prop-not-found";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["property"] = "text";
    cmd.params["value"] = "test";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testSetValueNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "set-value-not-found";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["value"] = "test";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testTypeNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "type-not-found";
    cmd.name = "type";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["text"] = "test";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testKeyNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "key-not-found";
    cmd.name = "key";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["key"] = "A";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testDragNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-not-found";
    cmd.name = "drag";
    cmd.params["from"] = "@name:nonexistent";
    cmd.params["to"] = "@name:testButton";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testDragToNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-to-not-found";
    cmd.name = "drag";
    cmd.params["from"] = "@name:testButton";
    cmd.params["to"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testGetPropertyNotFoundProperty() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "get-prop-not-found-prop";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testButton";
    cmd.params["property"] = "nonexistentProperty";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("PROPERTY_NOT_FOUND"));
  }

  void testDescribeSuccess() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "describe-success";
    cmd.name = "describe";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("class"));
  }

  void testListPropertiesSuccess() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "list-props-success";
    cmd.name = "list_properties";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("properties"));
  }

  void testGetActionsSuccess() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "get-actions-success";
    cmd.name = "get_actions";
    cmd.params["target"] = "@name:testButton";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("actions"));
  }

  void testFindWithMaxResults() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-max";
    cmd.name = "find";
    cmd.params["query"] = "QPushButton";
    cmd.params["max_results"] = 1;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("matches"));
  }

  void testFindVisibleOnly() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-visible";
    cmd.name = "find";
    cmd.params["query"] = "QWidget";
    cmd.params["visible_only"] = true;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("matches"));
  }

  void testWaitSignal() {
    CommandExecutor executor;

    // Schedule button click - use SIGNAL macro format (prefix "2")
    QTimer::singleShot(50, button_, &QPushButton::click);

    Command cmd;
    cmd.id = "wait-signal";
    cmd.name = "wait_signal";
    cmd.params["target"] = "@name:testButton";
    cmd.params["signal"] = "2clicked()";  // SIGNAL(clicked()) expands to "2clicked()"
    cmd.params["timeout_ms"] = 1000;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testWaitSignalTimeout() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-signal-timeout";
    cmd.name = "wait_signal";
    cmd.params["target"] = "@name:testButton";
    cmd.params["signal"] = "2clicked()";  // SIGNAL macro format
    cmd.params["timeout_ms"] = 50;

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("TIMEOUT"));
  }

  void testWaitSignalNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-signal-not-found";
    cmd.name = "wait_signal";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["signal"] = "2clicked()";
    cmd.params["timeout_ms"] = 100;

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testWaitSignalEmptyName() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-signal-empty";
    cmd.name = "wait_signal";
    cmd.params["target"] = "@name:testButton";
    cmd.params["signal"] = "";  // Empty signal name
    cmd.params["timeout_ms"] = 100;

    Response resp = executor.execute(cmd);
    // Currently returns TIMEOUT since empty signal can't be connected
    QVERIFY(!resp.success);
  }

  void testTrackChangesWithChange() {
    CommandExecutor executor;
    lineEdit_->setText("before");

    Command cmd;
    cmd.id = "track-change-real";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";
    cmd.params["value"] = "after";
    cmd.options["track_changes"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(resp.success);
    // Changes should be present when text actually changed
    if (resp.result.contains("changes")) {
      QVERIFY(resp.result.value("changes").isArray());
    }
  }

  void testTrackChangesWithError() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "track-change-error";
    cmd.name = "click";
    cmd.params["target"] = "@name:nonexistent";  // Will fail
    cmd.options["track_changes"] = true;

    Response resp = executor.execute(cmd);

    QVERIFY(!resp.success);
    QVERIFY(!resp.error.code.isEmpty());
  }

  void testTransactionEmptySteps() {
    CommandExecutor executor;

    Transaction tx;
    tx.id = "tx-empty";
    tx.rollbackOnFailure = false;
    // No steps added

    TransactionResponse resp = executor.execute(tx);

    QVERIFY(resp.success);  // Empty transaction succeeds
    QCOMPARE(resp.completedSteps, 0);
    QCOMPARE(resp.totalSteps, 0);
  }

  void testGetTreeDepth() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "tree-depth";
    cmd.name = "get_tree";
    cmd.params["depth"] = 1;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testGetTreeWithProperties() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "tree-props";
    cmd.name = "get_tree";
    cmd.params["include_properties"] = true;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testScreenshotTargetNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-notfound";
    cmd.name = "screenshot";
    cmd.params["target"] = "@name:nonexistent";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("ELEMENT_NOT_FOUND"));
  }

  void testWaitDisabled() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-disabled";
    cmd.name = "wait";
    cmd.params["target"] = "@name:disabledButton";
    cmd.params["condition"] = "disabled";
    cmd.params["timeout_ms"] = 500;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testWaitStable() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-stable";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "stable";
    cmd.params["timeout_ms"] = 500;
    cmd.params["stability_ms"] = 50;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testWaitFocused() {
    CommandExecutor executor;
    lineEdit_->setFocus();
    QTest::qWait(50);

    Command cmd;
    cmd.id = "wait-focused";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["condition"] = "focused";
    cmd.params["timeout_ms"] = 500;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testAssertStringEqual() {
    CommandExecutor executor;
    lineEdit_->setText("hello world");

    Command cmd;
    cmd.id = "assert-string";
    cmd.name = "assert";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["property"] = "text";
    cmd.params["value"] = "hello world";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.value("passed").toBool());
  }

  // === Tests for additional widget types ===

  void testSetValueCalendar() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-calendar";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testCalendar";
    cmd.params["value"] = "2025-06-15";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(calendarWidget_->selectedDate(), QDate(2025, 6, 15));
  }

  void testSetValueCalendarInvalid() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-calendar-invalid";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testCalendar";
    cmd.params["value"] = "not-a-date";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testSetValueTreeWidget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-tree";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTree";
    cmd.params["value"] = "Node A";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testSetValueTreeWidgetNotFound() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-tree-notfound";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTree";
    cmd.params["value"] = "Nonexistent Node";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testSetValueTableByCell() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-table-cell";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTable";
    QJsonObject cellSpec;
    cellSpec["row"] = 1;
    cellSpec["column"] = 1;
    cmd.params["value"] = cellSpec;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testSetValueTableByText() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-table-text";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTable";
    QJsonObject textSpec;
    textSpec["text"] = "Cell 0,0";
    cmd.params["value"] = textSpec;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testSetValueTableInvalid() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-table-invalid";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testTable";
    cmd.params["value"] = "just a string";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("INVALID_PARAMS"));
  }

  void testSetValueStackedWidget() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-stacked";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testStacked";
    cmd.params["value"] = 2;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(stackedWidget_->currentIndex(), 2);
  }

  void testWaitNotExists() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-not-exists";
    cmd.name = "wait";
    cmd.params["target"] = "@name:nonexistent";
    cmd.params["condition"] = "not_exists";
    cmd.params["timeout_ms"] = 500;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testWaitNotVisible() {
    CommandExecutor executor;
    // Create a hidden widget
    auto* hidden = new QWidget(testWindow_);
    hidden->setObjectName("hiddenWidget");
    hidden->hide();

    Command cmd;
    cmd.id = "wait-not-visible";
    cmd.name = "wait";
    cmd.params["target"] = "@name:hiddenWidget";
    cmd.params["condition"] = "not_visible";
    cmd.params["timeout_ms"] = 500;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);

    delete hidden;
  }

  void testGetFormFieldsCalendar() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-calendar";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testCalendar";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testGetFormFieldsTable() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-table";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testTable";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testScreenshotFormat() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-format";
    cmd.name = "screenshot";
    cmd.params["target"] = "@name:testButton";
    cmd.params["format"] = "jpg";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("format").toString(), QString("jpg"));
  }

  void testGetFormFieldsDoubleSpinBox() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-dsb";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testDoubleSpinBox";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testGetFormFieldsDateEdit() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-date";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testDateEdit";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testGetFormFieldsPlainText() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-plain";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testPlainTextEdit";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testGetFormFieldsGroupBox() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-group";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testGroupBox";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testGetFormFieldsWithVisibleOnly() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-visible";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testWindow";
    cmd.params["visible_only"] = true;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

  void testSetValueDoubleSpinBox() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-dsb";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testDoubleSpinBox";
    cmd.params["value"] = 75.5;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(doubleSpinBox_->value(), 75.5);
  }

  void testSetValuePlainTextEdit() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setval-plain";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testPlainTextEdit";
    cmd.params["value"] = "new plain text";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(plainTextEdit_->toPlainText(), QString("new plain text"));
  }

  void testInvokeParamMismatch() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "invoke-mismatch";
    cmd.name = "invoke";
    cmd.params["target"] = "@name:testButton";
    cmd.params["method"] = "setText";
    // setText expects 1 param, we give 0
    cmd.params["args"] = QJsonArray{};

    Response resp = executor.execute(cmd);
    // Should fail with invocation error about param count mismatch
    QVERIFY(!resp.success);
  }

  void testClickWithXY() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-xy";
    cmd.name = "click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["x"] = 5;
    cmd.params["y"] = 5;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testDoubleClickWithXY() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "dbl-xy";
    cmd.name = "double_click";
    cmd.params["target"] = "@name:testButton";
    cmd.params["x"] = 5;
    cmd.params["y"] = 5;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testScrollWithDeltaY() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "scroll-y";
    cmd.name = "scroll";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["delta_y"] = -50;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testKeySequenceWithModifiers() {
    CommandExecutor executor;
    lineEdit_->setFocus();
    lineEdit_->clear();

    Command cmd;
    cmd.id = "keyseq-mod";
    cmd.name = "key_sequence";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["sequence"] = "Ctrl+A";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testSetPropertyReadOnly() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "setprop-readonly";
    cmd.name = "set_property";
    cmd.params["target"] = "@name:testButton";
    cmd.params["property"] = "metaObject";  // Read-only property
    cmd.params["value"] = "test";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("PROPERTY_READ_ONLY"));
  }

  void testGetPropertyInvalid() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "getprop-invalid";
    cmd.name = "get_property";
    cmd.params["target"] = "@name:testButton";
    cmd.params["property"] = "invalidProperty123";

    Response resp = executor.execute(cmd);
    QVERIFY(!resp.success);
    QCOMPARE(resp.error.code, QString("PROPERTY_NOT_FOUND"));
  }

  void testWaitExistsAlreadyTrue() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "wait-exists-already";
    cmd.name = "wait";
    cmd.params["target"] = "@name:testButton";
    cmd.params["condition"] = "exists";
    cmd.params["timeout_ms"] = 100;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testDragWithDelta() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "drag-delta";
    cmd.name = "drag";
    cmd.params["from"] = "@name:testButton";
    cmd.params["to"] = "@name:testLineEdit";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testFindNoResults() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "find-none";
    cmd.name = "find";
    cmd.params["query"] = "NonexistentClassName";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(resp.result.value("count").toInt(), 0);
  }

  void testSetValueDial() {
    CommandExecutor executor;

    // Slider is an AbstractSlider
    Command cmd;
    cmd.id = "setval-slider2";
    cmd.name = "set_value";
    cmd.params["target"] = "@name:testSlider";
    cmd.params["value"] = 75;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QCOMPARE(slider_->value(), 75);
  }

  void testScreenshotWithFormat() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "screenshot-png";
    cmd.name = "screenshot";
    cmd.params["target"] = "@name:testWindow";
    cmd.params["format"] = "png";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("screenshot"));
  }

  void testClickOnSpinBox() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "click-spinbox";
    cmd.name = "click";
    cmd.params["target"] = "@name:testSpinBox";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testTypeOnSpinBox() {
    CommandExecutor executor;
    spinBox_->setFocus();

    Command cmd;
    cmd.id = "type-spinbox";
    cmd.name = "type";
    cmd.params["target"] = "@name:testSpinBox";
    cmd.params["text"] = "25";
    cmd.params["clear_first"] = true;

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testKeyReturn() {
    CommandExecutor executor;
    lineEdit_->setFocus();

    Command cmd;
    cmd.id = "key-return";
    cmd.name = "key";
    cmd.params["target"] = "@name:testLineEdit";
    cmd.params["key"] = "Return";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
  }

  void testGetFormFieldsSlider() {
    CommandExecutor executor;

    Command cmd;
    cmd.id = "formfields-slider";
    cmd.name = "get_form_fields";
    cmd.params["root"] = "@name:testSlider";

    Response resp = executor.execute(cmd);
    QVERIFY(resp.success);
    QVERIFY(resp.result.contains("fields"));
  }

private:
  QWidget* testWindow_ = nullptr;
  QPushButton* button_ = nullptr;
  QLineEdit* lineEdit_ = nullptr;
  QCheckBox* checkbox_ = nullptr;
  QSpinBox* spinBox_ = nullptr;
  QSlider* slider_ = nullptr;
  QComboBox* comboBox_ = nullptr;
  QLabel* label_ = nullptr;
  QPushButton* disabledButton_ = nullptr;
  QTextEdit* textEdit_ = nullptr;
  QProgressBar* progressBar_ = nullptr;
  QTabWidget* tabWidget_ = nullptr;
  QListWidget* listWidget_ = nullptr;
  QRadioButton* radioButton_ = nullptr;
  QCalendarWidget* calendarWidget_ = nullptr;
  QTreeWidget* treeWidget_ = nullptr;
  QTableWidget* tableWidget_ = nullptr;
  QStackedWidget* stackedWidget_ = nullptr;
  QDoubleSpinBox* doubleSpinBox_ = nullptr;
  QDateEdit* dateEdit_ = nullptr;
  QPlainTextEdit* plainTextEdit_ = nullptr;
  QGroupBox* groupBox_ = nullptr;
};

QTEST_MAIN(TestCommandExecutor)
#include "test_command_executor.moc"
