#include <widgeteer/Synchronizer.h>

#include <widgeteer/ElementFinder.h>

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

namespace widgeteer {

Synchronizer::Synchronizer(ElementFinder* finder) : finder_(finder) {
}

Synchronizer::WaitResult Synchronizer::wait(const WaitParams& params) {
  WaitResult result;
  QElapsedTimer timer;
  timer.start();

  // For stability condition, track geometry changes
  QRect lastGeometry;
  qint64 stableStartTime = 0;

  while (timer.elapsed() < params.timeoutMs) {
    // Check condition FIRST, before processing any events.
    // This allows detecting dialogs that are already visible
    // without triggering new events that might block.
    if (params.condition == Condition::Stable) {
      // Special handling for stability - check if geometry hasn't changed
      auto findResult = finder_->find(params.target);
      if (findResult.widget) {
        QRect currentGeometry = findResult.widget->geometry();
        if (currentGeometry == lastGeometry) {
          if (stableStartTime == 0) {
            stableStartTime = timer.elapsed();
          } else if (timer.elapsed() - stableStartTime >= params.stabilityMs) {
            result.success = true;
            result.elapsedMs = static_cast<int>(timer.elapsed());
            return result;
          }
        } else {
          lastGeometry = currentGeometry;
          stableStartTime = 0;
        }
      }
    } else if (checkCondition(params)) {
      result.success = true;
      result.elapsedMs = static_cast<int>(timer.elapsed());
      return result;
    }

    // Process events to allow UI updates and network handling.
    // With async command handling in Server, this won't block even if
    // a previous click opens a blocking dialog - the dialog's event loop
    // will process subsequent commands.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(static_cast<unsigned long>(params.pollIntervalMs));
  }

  result.success = false;
  result.elapsedMs = static_cast<int>(timer.elapsed());
  result.error = QStringLiteral("Timeout waiting for condition on '%1'").arg(params.target);
  return result;
}

Synchronizer::WaitResult Synchronizer::waitForIdle(int timeoutMs) {
  WaitResult result;
  QElapsedTimer timer;
  timer.start();

  // Process pending events
  while (timer.elapsed() < timeoutMs) {
    // Process all events
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    // Wait a bit and try again
    QThread::msleep(50);

    // Process again to make sure
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    // If we've been processing for a bit and no more events, we're idle
    if (timer.elapsed() >= 100) {
      result.success = true;
      result.elapsedMs = static_cast<int>(timer.elapsed());
      return result;
    }
  }

  result.success = false;
  result.elapsedMs = static_cast<int>(timer.elapsed());
  result.error = "Timeout waiting for idle state";
  return result;
}

Synchronizer::WaitResult Synchronizer::waitForSignal(QObject* obj, const char* signal,
                                                     int timeoutMs) {
  WaitResult result;

  if (!obj) {
    result.error = "Object is null";
    return result;
  }

  QElapsedTimer timer;
  timer.start();

  QEventLoop loop;

  // Connect using old-style SIGNAL/SLOT
  auto connection = QObject::connect(obj, signal, &loop, SLOT(quit()));
  if (!connection) {
    result.error = "Failed to connect to signal";
    return result;
  }

  // We need to know if signal or timeout caused loop exit
  // Use a lambda slot to set the flag when timeout fires
  bool timedOut = false;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  QObject::connect(&timeoutTimer, &QTimer::timeout, [&loop, &timedOut]() {
    timedOut = true;
    loop.quit();
  });
  timeoutTimer.start(timeoutMs);

  loop.exec();

  // Disconnect to avoid double-quit scenarios
  QObject::disconnect(connection);

  result.elapsedMs = static_cast<int>(timer.elapsed());
  result.success = !timedOut;

  if (!result.success) {
    result.error = "Timeout waiting for signal";
  }

  return result;
}

Synchronizer::Condition Synchronizer::parseCondition(const QString& condition,
                                                     QString& propertyName,
                                                     QVariant& propertyValue) {
  if (condition == "exists") {
    return Condition::Exists;
  }
  if (condition == "not_exists") {
    return Condition::NotExists;
  }
  if (condition == "visible") {
    return Condition::Visible;
  }
  if (condition == "not_visible") {
    return Condition::NotVisible;
  }
  if (condition == "enabled") {
    return Condition::Enabled;
  }
  if (condition == "disabled") {
    return Condition::Disabled;
  }
  if (condition == "focused") {
    return Condition::Focused;
  }
  if (condition == "stable") {
    return Condition::Stable;
  }
  if (condition == "idle") {
    return Condition::Idle;
  }

  // Check for property condition: "property:name=value"
  if (condition.startsWith("property:")) {
    QString propPart = condition.mid(9);
    int eqIndex = propPart.indexOf('=');
    if (eqIndex > 0) {
      propertyName = propPart.left(eqIndex);
      propertyValue = propPart.mid(eqIndex + 1);
      return Condition::PropertyEquals;
    }
  }

  // Default to exists
  return Condition::Exists;
}

bool Synchronizer::checkCondition(const WaitParams& params) {
  auto findResult = finder_->find(params.target);
  QWidget* widget = findResult.widget;

  switch (params.condition) {
    case Condition::Exists:
      return widget != nullptr;

    case Condition::NotExists:
      return widget == nullptr;

    case Condition::Visible:
      return widget != nullptr && widget->isVisible();

    case Condition::NotVisible:
      return widget == nullptr || !widget->isVisible();

    case Condition::Enabled:
      return widget != nullptr && widget->isEnabled();

    case Condition::Disabled:
      return widget != nullptr && !widget->isEnabled();

    case Condition::Focused:
      return widget != nullptr && widget->hasFocus();

    case Condition::PropertyEquals:
      if (widget && !params.propertyName.isEmpty()) {
        QVariant currentValue = widget->property(params.propertyName.toUtf8().constData());
        return currentValue == params.propertyValue;
      }
      return false;

    case Condition::Stable:
      // Handled separately in wait()
      return false;

    case Condition::Idle:
      // Idle is better handled by waitForIdle(), return true here
      return true;
  }

  return false;
}

}  // namespace widgeteer
