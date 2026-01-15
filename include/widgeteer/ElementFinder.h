#pragma once

#include <widgeteer/Export.h>

#include <QHash>
#include <QPointer>
#include <QString>
#include <QWidget>

namespace widgeteer {

class WIDGETEER_EXPORT ElementFinder {
public:
  struct FindResult {
    QWidget* widget = nullptr;
    QString resolvedPath;
    QString error;
  };

  struct FindOptions {
    bool recursive;
    int maxResults;
    bool visibleOnly;

    FindOptions() : recursive(true), maxResults(100), visibleOnly(false) {
    }
  };

  ElementFinder();

  // Find single element by selector
  FindResult find(const QString& selector);

  // Find all matching elements
  QList<FindResult> findAll(const QString& selector, const FindOptions& opts = FindOptions());

  // Get path for a widget (reverse lookup)
  QString pathFor(QWidget* widget);

  // Invalidate cached lookups
  void invalidateCache();

private:
  // By path: "mainWindow/sidebar/loadButton"
  QWidget* byPath(const QString& path, QWidget* root = nullptr);

  // By objectName: "@name:plotWidget"
  QWidget* byName(const QString& name, QWidget* root = nullptr);

  // By class: "@class:QLineEdit"
  QWidget* byClass(const QString& className, QWidget* root = nullptr);

  // By text content: "@text:Open File"
  QWidget* byText(const QString& text, QWidget* root = nullptr);

  // By accessible name: "@accessible:Load Data Button"
  QWidget* byAccessible(const QString& name, QWidget* root = nullptr);

  // Parse selector and dispatch to appropriate method
  QWidget* resolveSelector(const QString& selector, QString& errorOut);

  // Get all top-level widgets as potential roots
  QList<QWidget*> topLevelWidgets();

  // Find child by name in parent (one level or recursive)
  QWidget* findChild(QWidget* parent, const QString& name, bool recursive);

  // Cache for frequently accessed widgets
  QHash<QString, QPointer<QWidget>> cache_;
};

}  // namespace widgeteer
