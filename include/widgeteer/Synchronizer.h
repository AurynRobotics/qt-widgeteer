#pragma once

#include <widgeteer/Export.h>

#include <QObject>
#include <QString>
#include <QVariant>

namespace widgeteer {

class ElementFinder;

class WIDGETEER_EXPORT Synchronizer {
public:
  enum class Condition {
    Exists,
    NotExists,
    Visible,
    NotVisible,
    Enabled,
    Disabled,
    Focused,
    PropertyEquals,
    Stable,
    Idle
  };

  struct WaitParams {
    QString target;
    Condition condition = Condition::Exists;
    QString propertyName;
    QVariant propertyValue;
    int timeoutMs = 5000;
    int pollIntervalMs = 50;
    int stabilityMs = 200;
  };

  struct WaitResult {
    bool success = false;
    int elapsedMs = 0;
    QString error;
  };

  explicit Synchronizer(ElementFinder* finder);

  // Wait for a condition to be met
  WaitResult wait(const WaitParams& params);

  // Wait for Qt event queue to be empty (idle state)
  WaitResult waitForIdle(int timeoutMs = 5000);

  // Wait for a Qt signal to be emitted
  WaitResult waitForSignal(QObject* obj, const char* signal, int timeoutMs = 5000);

  // Parse condition string to enum (e.g., "visible", "enabled", "property:text=hello")
  static Condition parseCondition(const QString& condition, QString& propertyName,
                                  QVariant& propertyValue);

private:
  // Check if condition is currently met
  bool checkCondition(const WaitParams& params);

  ElementFinder* finder_;
};

}  // namespace widgeteer
