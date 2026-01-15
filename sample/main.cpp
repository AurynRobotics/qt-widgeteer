#include <widgeteer/Server.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>

// Sample service class to demonstrate QObject registration
class SampleService : public QObject
{
  Q_OBJECT

public:
  explicit SampleService(QObject* parent = nullptr) : QObject(parent)
  {
  }

  // Q_INVOKABLE methods can be called via the "call" command
  Q_INVOKABLE int add(int a, int b)
  {
    return a + b;
  }

  Q_INVOKABLE QString greet(const QString& name)
  {
    return QStringLiteral("Hello, %1!").arg(name);
  }

  Q_INVOKABLE QVariantMap getInfo()
  {
    return QVariantMap{ { "name", "SampleService" },
                        { "version", "1.0" },
                        { "counter", counter_ } };
  }

  Q_INVOKABLE void incrementCounter()
  {
    counter_++;
  }

  Q_INVOKABLE int getCounter() const
  {
    return counter_;
  }

  Q_INVOKABLE void setCounter(int value)
  {
    counter_ = value;
  }

private:
  int counter_ = 0;
};

class SampleMainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit SampleMainWindow(QWidget* parent = nullptr) : QMainWindow(parent)
  {
    setObjectName("mainWindow");
    setWindowTitle("Widgeteer Sample Application");
    resize(800, 600);

    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
  }

private slots:
  void onButtonClicked()
  {
    statusBar()->showMessage("Button clicked!", 2000);
    outputText_->append("Button was clicked");
  }

  void onShowDialogClicked()
  {
    // Use non-blocking dialog for Widgeteer compatibility.
    // Blocking dialogs (exec()) prevent Widgeteer from processing commands
    // until the dialog closes. With show(), the event loop continues running
    // and Widgeteer can interact with the dialog.
    QMessageBox* msgBox = new QMessageBox(this);
    msgBox->setObjectName("confirmDialog");
    msgBox->setWindowTitle("Confirm Action");
    msgBox->setText("Do you want to proceed with this action?");
    msgBox->setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

    connect(msgBox, &QMessageBox::finished, this, [this, msgBox](int result) {
      if (result == QMessageBox::Ok)
      {
        outputText_->append("Dialog: OK was clicked");
      }
      else
      {
        outputText_->append("Dialog: Cancel was clicked");
      }
      msgBox->deleteLater();
    });

    msgBox->show();  // Non-blocking
  }

  void onSubmitClicked()
  {
    QString name = nameEdit_->text();
    QString email = emailEdit_->text();
    outputText_->append(QStringLiteral("Form submitted: %1 <%2>").arg(name, email));
    statusBar()->showMessage("Form submitted!", 2000);
  }

  void onClearClicked()
  {
    nameEdit_->clear();
    emailEdit_->clear();
    outputText_->clear();
    statusBar()->showMessage("Cleared", 2000);
  }

  void onSliderChanged(int value)
  {
    progressBar_->setValue(value);
    sliderLabel_->setText(QStringLiteral("Value: %1").arg(value));
  }

  void onComboChanged(int index)
  {
    outputText_->append(QStringLiteral("Selected: %1").arg(comboBox_->currentText()));
  }

  void onCheckboxToggled(bool checked)
  {
    outputText_->append(QStringLiteral("Checkbox: %1").arg(checked ? "checked" : "unchecked"));
  }

  void onListItemClicked(QListWidgetItem* item)
  {
    outputText_->append(QStringLiteral("List item clicked: %1").arg(item->text()));
  }

  void onAbout()
  {
    QMessageBox::about(this, "About",
                       "Widgeteer Sample Application\n\n"
                       "A demonstration app for testing the Widgeteer framework.");
  }

private:
  void setupMenuBar()
  {
    QMenuBar* menuBar = this->menuBar();
    menuBar->setObjectName("menuBar");

    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->setObjectName("fileMenu");

    QAction* newAction = fileMenu->addAction("&New");
    newAction->setObjectName("newAction");
    newAction->setShortcut(QKeySequence::New);

    QAction* openAction = fileMenu->addAction("&Open...");
    openAction->setObjectName("openAction");
    openAction->setShortcut(QKeySequence::Open);

    QAction* saveAction = fileMenu->addAction("&Save");
    saveAction->setObjectName("saveAction");
    saveAction->setShortcut(QKeySequence::Save);

    fileMenu->addSeparator();

    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setObjectName("exitAction");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    // Edit menu
    QMenu* editMenu = menuBar->addMenu("&Edit");
    editMenu->setObjectName("editMenu");

    QAction* cutAction = editMenu->addAction("Cu&t");
    cutAction->setObjectName("cutAction");
    cutAction->setShortcut(QKeySequence::Cut);

    QAction* copyAction = editMenu->addAction("&Copy");
    copyAction->setObjectName("copyAction");
    copyAction->setShortcut(QKeySequence::Copy);

    QAction* pasteAction = editMenu->addAction("&Paste");
    pasteAction->setObjectName("pasteAction");
    pasteAction->setShortcut(QKeySequence::Paste);

    // Help menu
    QMenu* helpMenu = menuBar->addMenu("&Help");
    helpMenu->setObjectName("helpMenu");

    QAction* aboutAction = helpMenu->addAction("&About");
    aboutAction->setObjectName("aboutAction");
    connect(aboutAction, &QAction::triggered, this, &SampleMainWindow::onAbout);
  }

  void setupToolBar()
  {
    QToolBar* toolBar = addToolBar("Main Toolbar");
    toolBar->setObjectName("mainToolBar");

    QAction* newBtn = toolBar->addAction("New");
    newBtn->setObjectName("toolbarNew");

    QAction* openBtn = toolBar->addAction("Open");
    openBtn->setObjectName("toolbarOpen");

    QAction* saveBtn = toolBar->addAction("Save");
    saveBtn->setObjectName("toolbarSave");
  }

  void setupCentralWidget()
  {
    QWidget* central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    QHBoxLayout* mainLayout = new QHBoxLayout(central);

    // Left side - tabs
    QTabWidget* tabWidget = new QTabWidget(central);
    tabWidget->setObjectName("tabWidget");
    mainLayout->addWidget(tabWidget, 2);

    // Tab 1: Form
    QWidget* formTab = new QWidget();
    formTab->setObjectName("formTab");
    tabWidget->addTab(formTab, "Form");

    QVBoxLayout* formLayout = new QVBoxLayout(formTab);

    // Form group
    QGroupBox* formGroup = new QGroupBox("User Information", formTab);
    formGroup->setObjectName("formGroup");
    formLayout->addWidget(formGroup);

    QFormLayout* formFieldsLayout = new QFormLayout(formGroup);

    nameEdit_ = new QLineEdit(formGroup);
    nameEdit_->setObjectName("nameEdit");
    nameEdit_->setPlaceholderText("Enter your name");
    formFieldsLayout->addRow("Name:", nameEdit_);

    emailEdit_ = new QLineEdit(formGroup);
    emailEdit_->setObjectName("emailEdit");
    emailEdit_->setPlaceholderText("Enter your email");
    formFieldsLayout->addRow("Email:", emailEdit_);

    comboBox_ = new QComboBox(formGroup);
    comboBox_->setObjectName("roleComboBox");
    comboBox_->addItems({ "Developer", "Designer", "Manager", "Tester", "Other" });
    formFieldsLayout->addRow("Role:", comboBox_);
    connect(comboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SampleMainWindow::onComboChanged);

    ageSpinBox_ = new QSpinBox(formGroup);
    ageSpinBox_->setObjectName("ageSpinBox");
    ageSpinBox_->setRange(18, 100);
    ageSpinBox_->setValue(25);
    formFieldsLayout->addRow("Age:", ageSpinBox_);

    // Checkbox
    enableCheckBox_ = new QCheckBox("Enable notifications", formTab);
    enableCheckBox_->setObjectName("enableCheckBox");
    enableCheckBox_->setChecked(true);
    formLayout->addWidget(enableCheckBox_);
    connect(enableCheckBox_, &QCheckBox::toggled, this, &SampleMainWindow::onCheckboxToggled);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    formLayout->addLayout(buttonLayout);

    submitButton_ = new QPushButton("Submit", formTab);
    submitButton_->setObjectName("submitButton");
    buttonLayout->addWidget(submitButton_);
    connect(submitButton_, &QPushButton::clicked, this, &SampleMainWindow::onSubmitClicked);

    clearButton_ = new QPushButton("Clear", formTab);
    clearButton_->setObjectName("clearButton");
    buttonLayout->addWidget(clearButton_);
    connect(clearButton_, &QPushButton::clicked, this, &SampleMainWindow::onClearClicked);

    formLayout->addStretch();

    // Tab 2: Controls
    QWidget* controlsTab = new QWidget();
    controlsTab->setObjectName("controlsTab");
    tabWidget->addTab(controlsTab, "Controls");

    QVBoxLayout* controlsLayout = new QVBoxLayout(controlsTab);

    // Slider group
    QGroupBox* sliderGroup = new QGroupBox("Slider Control", controlsTab);
    sliderGroup->setObjectName("sliderGroup");
    controlsLayout->addWidget(sliderGroup);

    QVBoxLayout* sliderLayout = new QVBoxLayout(sliderGroup);

    slider_ = new QSlider(Qt::Horizontal, sliderGroup);
    slider_->setObjectName("slider");
    slider_->setRange(0, 100);
    slider_->setValue(50);
    sliderLayout->addWidget(slider_);
    connect(slider_, &QSlider::valueChanged, this, &SampleMainWindow::onSliderChanged);

    sliderLabel_ = new QLabel("Value: 50", sliderGroup);
    sliderLabel_->setObjectName("sliderLabel");
    sliderLayout->addWidget(sliderLabel_);

    progressBar_ = new QProgressBar(sliderGroup);
    progressBar_->setObjectName("progressBar");
    progressBar_->setRange(0, 100);
    progressBar_->setValue(50);
    sliderLayout->addWidget(progressBar_);

    // List group
    QGroupBox* listGroup = new QGroupBox("Items List", controlsTab);
    listGroup->setObjectName("listGroup");
    controlsLayout->addWidget(listGroup);

    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);

    listWidget_ = new QListWidget(listGroup);
    listWidget_->setObjectName("listWidget");
    listWidget_->addItems({ "Item 1", "Item 2", "Item 3", "Item 4", "Item 5" });
    listLayout->addWidget(listWidget_);
    connect(listWidget_, &QListWidget::itemClicked, this, &SampleMainWindow::onListItemClicked);

    // Button
    actionButton_ = new QPushButton("Action Button", controlsTab);
    actionButton_->setObjectName("actionButton");
    controlsLayout->addWidget(actionButton_);
    connect(actionButton_, &QPushButton::clicked, this, &SampleMainWindow::onButtonClicked);

    // Dialog button
    showDialogButton_ = new QPushButton("Show Dialog", controlsTab);
    showDialogButton_->setObjectName("showDialogButton");
    controlsLayout->addWidget(showDialogButton_);
    connect(showDialogButton_, &QPushButton::clicked, this, &SampleMainWindow::onShowDialogClicked);

    controlsLayout->addStretch();

    // Right side - output
    QGroupBox* outputGroup = new QGroupBox("Output", central);
    outputGroup->setObjectName("outputGroup");
    mainLayout->addWidget(outputGroup, 1);

    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    outputText_ = new QTextEdit(outputGroup);
    outputText_->setObjectName("outputText");
    outputText_->setReadOnly(true);
    outputText_->setPlaceholderText("Actions will be logged here...");
    outputLayout->addWidget(outputText_);
  }

  void setupStatusBar()
  {
    QStatusBar* status = statusBar();
    status->setObjectName("statusBar");
    status->showMessage("Ready");
  }

  // Form widgets
  QLineEdit* nameEdit_ = nullptr;
  QLineEdit* emailEdit_ = nullptr;
  QComboBox* comboBox_ = nullptr;
  QSpinBox* ageSpinBox_ = nullptr;
  QCheckBox* enableCheckBox_ = nullptr;
  QPushButton* submitButton_ = nullptr;
  QPushButton* clearButton_ = nullptr;

  // Control widgets
  QSlider* slider_ = nullptr;
  QLabel* sliderLabel_ = nullptr;
  QProgressBar* progressBar_ = nullptr;
  QListWidget* listWidget_ = nullptr;
  QPushButton* actionButton_ = nullptr;
  QPushButton* showDialogButton_ = nullptr;

  // Output
  QTextEdit* outputText_ = nullptr;
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("Widgeteer Sample");
  app.setApplicationVersion("1.0.0");

  // Start the Widgeteer server
  widgeteer::Server server;
  server.enableLogging(true);

  quint16 port = 9000;
  if (argc > 1)
  {
    port = static_cast<quint16>(QString::fromLocal8Bit(argv[1]).toUInt());
  }

  if (!server.start(port))
  {
    qCritical() << "Failed to start Widgeteer server";
    return 1;
  }

  qInfo() << "Widgeteer server running on port" << server.port();

  // Create and show main window
  SampleMainWindow window;
  window.show();

  // ========== Extensibility Demo ==========

  // Register a service object for QObject method invocation
  SampleService service;
  server.registerObject("myService", &service);

  // Register a custom lambda command
  server.registerCommand("echo", [](const QJsonObject& params) {
    QString message = params.value("message").toString();
    return QJsonObject{ { "echo", message }, { "length", message.length() } };
  });

  // Register a command that accesses application state
  server.registerCommand("get_app_info", [](const QJsonObject& params) {
    Q_UNUSED(params);
    return QJsonObject{ { "name", QApplication::applicationName() },
                        { "version", QApplication::applicationVersion() },
                        { "pid", QApplication::applicationPid() } };
  });

  return app.exec();
}

#include "main.moc"
