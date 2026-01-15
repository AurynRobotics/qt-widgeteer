#include <widgeteer/EventInjector.h>

#include <QAbstractButton>
#include <QApplication>
#include <QPointer>
#include <QTest>
#include <QTimer>

namespace widgeteer {

EventInjector::EventInjector() = default;

EventInjector::Result EventInjector::click(QWidget* target, Qt::MouseButton btn, QPoint pos,
                                           Qt::KeyboardModifiers mods) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint clickPos = resolvePosition(target, pos);

  // Schedule the click to execute AFTER the current command handler completes.
  // This is critical for supporting blocking dialogs (exec()):
  // 1. Command handler receives click command
  // 2. This function schedules click via QTimer::singleShot
  // 3. Command handler sends response
  // 4. Event loop continues, timer fires
  // 5. Click is executed (via QAbstractButton::click() or QTest::mouseClick)
  // 6. If button opens dialog.exec(), the dialog's nested event loop runs
  // 7. Subsequent commands can be processed by the nested event loop
  //
  // Using a small delay (10ms) ensures this timer fires AFTER the command
  // handler's timer (0ms) completes and sends the response.
  QPointer<QWidget> safeTarget = target;
  QTimer::singleShot(10, [safeTarget, clickPos, btn, mods]() {
    if (!safeTarget) {
      return;  // Widget was deleted
    }

    // For QAbstractButton (QPushButton, QCheckBox, QRadioButton, etc.),
    // use the native click() method which works reliably in all modes
    // including offscreen. For other widgets, use QTest::mouseClick.
    if (btn == Qt::LeftButton &&
        clickPos == QPoint(safeTarget->width() / 2, safeTarget->height() / 2)) {
      if (auto* abstractButton = qobject_cast<QAbstractButton*>(safeTarget.data())) {
        abstractButton->click();
        return;
      }
    }

    // For non-button widgets or specific click positions, use mouse events
    QTest::mouseClick(safeTarget, btn, mods, clickPos);
  });

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::doubleClick(QWidget* target, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint clickPos = resolvePosition(target, pos);
  QTest::mouseDClick(target, Qt::LeftButton, Qt::NoModifier, clickPos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::rightClick(QWidget* target, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint clickPos = resolvePosition(target, pos);
  QTest::mouseClick(target, Qt::RightButton, Qt::NoModifier, clickPos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::press(QWidget* target, Qt::MouseButton btn, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint pressPos = resolvePosition(target, pos);
  QTest::mousePress(target, btn, Qt::NoModifier, pressPos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::release(QWidget* target, Qt::MouseButton btn, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint releasePos = resolvePosition(target, pos);
  QTest::mouseRelease(target, btn, Qt::NoModifier, releasePos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::move(QWidget* target, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint movePos = resolvePosition(target, pos);
  QTest::mouseMove(target, movePos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::drag(QWidget* source, QPoint from, QWidget* dest, QPoint to) {
  Result result;

  if (!ensureVisible(source, result.error)) {
    return result;
  }
  if (!ensureVisible(dest, result.error)) {
    return result;
  }

  QPoint fromPos = resolvePosition(source, from);
  QPoint toPos = resolvePosition(dest, to);

  // Start drag
  QTest::mousePress(source, Qt::LeftButton, Qt::NoModifier, fromPos);

  // Move to destination (with intermediate steps for smooth drag)
  QPoint globalFrom = source->mapToGlobal(fromPos);
  QPoint globalTo = dest->mapToGlobal(toPos);

  // Interpolate
  int steps = 10;
  for (int i = 1; i <= steps; ++i) {
    double t = static_cast<double>(i) / steps;
    QPoint intermediate(static_cast<int>(globalFrom.x() + t * (globalTo.x() - globalFrom.x())),
                        static_cast<int>(globalFrom.y() + t * (globalTo.y() - globalFrom.y())));
    QWidget* widgetUnder = QApplication::widgetAt(intermediate);
    if (widgetUnder) {
      QPoint localPos = widgetUnder->mapFromGlobal(intermediate);
      QTest::mouseMove(widgetUnder, localPos);
    }
    QApplication::processEvents();
  }

  // Release at destination
  QTest::mouseRelease(dest, Qt::LeftButton, Qt::NoModifier, toPos);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::scroll(QWidget* target, int deltaX, int deltaY, QPoint pos) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QPoint scrollPos = resolvePosition(target, pos);

  // Create wheel event
  QWheelEvent event(scrollPos, target->mapToGlobal(scrollPos), QPoint(deltaX, deltaY),
                    QPoint(deltaX, deltaY), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);

  QApplication::sendEvent(target, &event);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::hover(QWidget* target, QPoint pos) {
  return move(target, pos);
}

EventInjector::Result EventInjector::type(QWidget* target, const QString& text) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  // Ensure focus
  if (!target->hasFocus()) {
    target->setFocus();
    QApplication::processEvents();
  }

  // Type each character
  QTest::keyClicks(target, text);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::keyPress(QWidget* target, Qt::Key key,
                                              Qt::KeyboardModifiers mods) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QTest::keyPress(target, key, mods);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::keyRelease(QWidget* target, Qt::Key key,
                                                Qt::KeyboardModifiers mods) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QTest::keyRelease(target, key, mods);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::keyClick(QWidget* target, Qt::Key key,
                                              Qt::KeyboardModifiers mods) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  QTest::keyClick(target, key, mods);

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::shortcut(QWidget* target, const QKeySequence& seq) {
  Result result;

  if (!ensureVisible(target, result.error)) {
    return result;
  }

  // Send key sequence
  for (int i = 0; i < seq.count(); ++i) {
    QKeyCombination combo = seq[i];
    QTest::keyClick(target, combo.key(), combo.keyboardModifiers());
  }

  result.success = true;
  return result;
}

EventInjector::Result EventInjector::setFocus(QWidget* target) {
  Result result;

  if (!target) {
    result.error = "Target widget is null";
    return result;
  }

  if (!target->isVisible()) {
    result.error = "Target widget is not visible";
    return result;
  }

  target->setFocus();
  QApplication::processEvents();

  if (!target->hasFocus()) {
    // Try activating the window first
    if (QWidget* window = target->window()) {
      window->activateWindow();
      QApplication::processEvents();
      target->setFocus();
      QApplication::processEvents();
    }
  }

  result.success = target->hasFocus();
  if (!result.success) {
    result.error = "Failed to set focus on widget";
  }

  return result;
}

EventInjector::Result EventInjector::clearFocus() {
  Result result;

  if (QWidget* focused = QApplication::focusWidget()) {
    focused->clearFocus();
    QApplication::processEvents();
  }

  result.success = true;
  return result;
}

bool EventInjector::ensureVisible(QWidget* target, QString& errorOut) {
  if (!target) {
    errorOut = "Target widget is null";
    return false;
  }

  if (!target->isVisible()) {
    errorOut = "Target widget is not visible";
    return false;
  }

  if (!target->isEnabled()) {
    errorOut = "Target widget is not enabled";
    return false;
  }

  return true;
}

QPoint EventInjector::resolvePosition(QWidget* target, QPoint pos) {
  if (pos.isNull()) {
    // Return center of widget
    return QPoint(target->width() / 2, target->height() / 2);
  }
  return pos;
}

}  // namespace widgeteer
