#pragma once

#include <widgeteer/Export.h>

#include <QKeySequence>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <Qt>

namespace widgeteer
{

class WIDGETEER_EXPORT EventInjector
{
public:
  struct Result
  {
    bool success = false;
    QString error;
  };

  EventInjector();

  // Mouse operations
  Result click(QWidget* target, Qt::MouseButton btn = Qt::LeftButton, QPoint pos = {},
               Qt::KeyboardModifiers mods = {});
  Result doubleClick(QWidget* target, QPoint pos = {});
  Result rightClick(QWidget* target, QPoint pos = {});
  Result press(QWidget* target, Qt::MouseButton btn, QPoint pos = {});
  Result release(QWidget* target, Qt::MouseButton btn, QPoint pos = {});
  Result move(QWidget* target, QPoint pos);
  Result drag(QWidget* source, QPoint from, QWidget* dest, QPoint to);
  Result scroll(QWidget* target, int deltaX, int deltaY, QPoint pos = {});
  Result hover(QWidget* target, QPoint pos = {});

  // Keyboard operations
  Result type(QWidget* target, const QString& text);
  Result keyPress(QWidget* target, Qt::Key key, Qt::KeyboardModifiers mods = {});
  Result keyRelease(QWidget* target, Qt::Key key, Qt::KeyboardModifiers mods = {});
  Result keyClick(QWidget* target, Qt::Key key, Qt::KeyboardModifiers mods = {});
  Result shortcut(QWidget* target, const QKeySequence& seq);

  // Focus operations
  Result setFocus(QWidget* target);
  Result clearFocus();

private:
  // Ensure widget is visible and can receive events
  bool ensureVisible(QWidget* target, QString& errorOut);

  // Resolve position relative to widget (default = center)
  QPoint resolvePosition(QWidget* target, QPoint pos);
};

}  // namespace widgeteer
