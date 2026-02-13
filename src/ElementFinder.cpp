#include <widgeteer/ElementFinder.h>

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QRegularExpression>

namespace widgeteer {

ElementFinder::ElementFinder() = default;

ElementFinder::FindResult ElementFinder::find(const QString& selector) {
  FindResult result;

  // Check cache first
  if (cache_.contains(selector)) {
    QWidget* cached = cache_.value(selector);
    if (cached) {
      result.widget = cached;
      result.resolvedPath = selector;
      return result;
    }
    // Widget was deleted, remove from cache
    cache_.remove(selector);
  }

  QString error;
  QWidget* widget = resolveSelector(selector, error);

  if (widget) {
    result.widget = widget;
    result.resolvedPath = pathFor(widget);
    cache_.insert(selector, widget);
  } else {
    result.error = error;
  }

  return result;
}

QList<ElementFinder::FindResult> ElementFinder::findAll(const QString& selector,
                                                        const FindOptions& opts) {
  QList<FindResult> results;

  // Handle different selector types
  if (selector.startsWith("@class:")) {
    QString className = selector.mid(7);
    for (QWidget* topLevel : topLevelWidgets()) {
      QList<QWidget*> matches = topLevel->findChildren<QWidget*>();
      for (QWidget* w : matches) {
        if (QString::fromLatin1(w->metaObject()->className()) == className) {
          if (opts.visibleOnly && !w->isVisible()) {
            continue;
          }
          FindResult r;
          r.widget = w;
          r.resolvedPath = pathFor(w);
          results.append(r);
          if (results.size() >= opts.maxResults) {
            return results;
          }
        }
      }
    }
  } else if (selector.startsWith("@text:")) {
    QString text = selector.mid(6);
    for (QWidget* topLevel : topLevelWidgets()) {
      QList<QWidget*> allWidgets = topLevel->findChildren<QWidget*>();
      allWidgets.prepend(topLevel);
      for (QWidget* w : allWidgets) {
        QString widgetText;
        if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
          widgetText = btn->text();
        } else if (auto* label = qobject_cast<QLabel*>(w)) {
          widgetText = label->text();
        } else if (auto* group = qobject_cast<QGroupBox*>(w)) {
          widgetText = group->title();
        }

        if (widgetText == text) {
          if (opts.visibleOnly && !w->isVisible()) {
            continue;
          }
          FindResult r;
          r.widget = w;
          r.resolvedPath = pathFor(w);
          results.append(r);
          if (results.size() >= opts.maxResults) {
            return results;
          }
        }
      }
    }
  } else {
    // For other selectors, just return the single match
    FindResult r = find(selector);
    if (r.widget) {
      results.append(r);
    }
  }

  return results;
}

QString ElementFinder::pathFor(QWidget* widget) {
  if (!widget) {
    return {};
  }

  QStringList path;
  QWidget* current = widget;

  while (current) {
    QString name = current->objectName();
    if (name.isEmpty()) {
      // Use class name with index if no object name
      QString className = QString::fromLatin1(current->metaObject()->className());
      QWidget* parent = current->parentWidget();
      if (parent) {
        QList<QWidget*> siblings =
            parent->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        int index = 0;
        int count = 0;
        for (int i = 0; i < siblings.size(); ++i) {
          if (QString::fromLatin1(siblings[i]->metaObject()->className()) == className) {
            if (siblings[i] == current) {
              index = count;
            }
            ++count;
          }
        }
        if (count > 1) {
          name = QStringLiteral("%1[%2]").arg(className).arg(index);
        } else {
          name = className;
        }
      } else {
        name = className;
      }
    }
    path.prepend(name);
    current = current->parentWidget();
  }

  return path.join('/');
}

void ElementFinder::invalidateCache() {
  cache_.clear();
}

QWidget* ElementFinder::byPath(const QString& path, QWidget* root) {
  if (path.isEmpty()) {
    return root;
  }

  QStringList parts = path.split('/', Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    return nullptr;
  }

  QWidget* current = root;

  // If no root provided, find it from top-level widgets
  if (!current) {
    QString firstPart = parts.takeFirst();
    for (QWidget* topLevel : topLevelWidgets()) {
      if (topLevel->objectName() == firstPart ||
          QString::fromLatin1(topLevel->metaObject()->className()) == firstPart) {
        current = topLevel;
        break;
      }
    }
    if (!current) {
      return nullptr;
    }
  }

  // Navigate through the path
  for (const QString& part : parts) {
    QWidget* child = findChild(current, part, false);
    if (!child) {
      // Try recursive search if direct child not found
      child = findChild(current, part, true);
    }
    if (!child) {
      return nullptr;
    }
    current = child;
  }

  return current;
}

QWidget* ElementFinder::byName(const QString& name, QWidget* root) {
  if (root) {
    return root->findChild<QWidget*>(name);
  }

  for (QWidget* topLevel : topLevelWidgets()) {
    if (topLevel->objectName() == name) {
      return topLevel;
    }
    QWidget* found = topLevel->findChild<QWidget*>(name);
    if (found) {
      return found;
    }
  }
  return nullptr;
}

QWidget* ElementFinder::byClass(const QString& className, QWidget* root) {
  auto matchesClass = [&className](QWidget* w) {
    return QString::fromLatin1(w->metaObject()->className()) == className;
  };

  if (root) {
    if (matchesClass(root)) {
      return root;
    }
    QList<QWidget*> children = root->findChildren<QWidget*>();
    for (QWidget* child : children) {
      if (matchesClass(child)) {
        return child;
      }
    }
    return nullptr;
  }

  for (QWidget* topLevel : topLevelWidgets()) {
    if (matchesClass(topLevel)) {
      return topLevel;
    }
    QList<QWidget*> children = topLevel->findChildren<QWidget*>();
    for (QWidget* child : children) {
      if (matchesClass(child)) {
        return child;
      }
    }
  }
  return nullptr;
}

QWidget* ElementFinder::byText(const QString& text, QWidget* root) {
  auto hasText = [&text](QWidget* w) -> bool {
    if (auto* btn = qobject_cast<QAbstractButton*>(w)) {
      return btn->text() == text;
    }
    if (auto* label = qobject_cast<QLabel*>(w)) {
      return label->text() == text;
    }
    if (auto* group = qobject_cast<QGroupBox*>(w)) {
      return group->title() == text;
    }
    return false;
  };

  QList<QWidget*> searchList;
  if (root) {
    searchList.append(root);
    searchList.append(root->findChildren<QWidget*>());
  } else {
    for (QWidget* topLevel : topLevelWidgets()) {
      searchList.append(topLevel);
      searchList.append(topLevel->findChildren<QWidget*>());
    }
  }

  for (QWidget* w : searchList) {
    if (hasText(w)) {
      return w;
    }
  }
  return nullptr;
}

QWidget* ElementFinder::byAccessible(const QString& name, QWidget* root) {
  auto hasAccessibleName = [&name](QWidget* w) -> bool {
    QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(w);
    if (iface) {
      return iface->text(QAccessible::Name) == name;
    }
    return false;
  };

  QList<QWidget*> searchList;
  if (root) {
    searchList.append(root);
    searchList.append(root->findChildren<QWidget*>());
  } else {
    for (QWidget* topLevel : topLevelWidgets()) {
      searchList.append(topLevel);
      searchList.append(topLevel->findChildren<QWidget*>());
    }
  }

  for (QWidget* w : searchList) {
    if (hasAccessibleName(w)) {
      return w;
    }
  }
  return nullptr;
}

QWidget* ElementFinder::byDialogRole(const QString& role) {
  // Get the active window (which might be a dialog)
  QWidget* activeWindow = QApplication::activeWindow();
  if (!activeWindow) {
    return nullptr;
  }

  // Map role names to QDialogButtonBox standard buttons
  QDialogButtonBox::StandardButton standardButton = QDialogButtonBox::NoButton;
  if (role == "accept" || role == "ok") {
    standardButton = QDialogButtonBox::Ok;
  } else if (role == "reject" || role == "cancel") {
    standardButton = QDialogButtonBox::Cancel;
  } else if (role == "apply") {
    standardButton = QDialogButtonBox::Apply;
  } else if (role == "help") {
    standardButton = QDialogButtonBox::Help;
  } else if (role == "yes") {
    standardButton = QDialogButtonBox::Yes;
  } else if (role == "no") {
    standardButton = QDialogButtonBox::No;
  } else if (role == "save") {
    standardButton = QDialogButtonBox::Save;
  } else if (role == "discard") {
    standardButton = QDialogButtonBox::Discard;
  } else if (role == "close") {
    standardButton = QDialogButtonBox::Close;
  } else if (role == "reset") {
    standardButton = QDialogButtonBox::Reset;
  }

  // Find QDialogButtonBox in the active window
  QDialogButtonBox* buttonBox = activeWindow->findChild<QDialogButtonBox*>();
  if (buttonBox && standardButton != QDialogButtonBox::NoButton) {
    QPushButton* button = buttonBox->button(standardButton);
    if (button) {
      return button;
    }
  }

  // If no standard button found, try to find by role in button box
  if (buttonBox) {
    // Try to match by button role
    QDialogButtonBox::ButtonRole targetRole = QDialogButtonBox::InvalidRole;
    if (role == "accept" || role == "ok" || role == "yes" || role == "save") {
      targetRole = QDialogButtonBox::AcceptRole;
    } else if (role == "reject" || role == "cancel" || role == "no" || role == "close") {
      targetRole = QDialogButtonBox::RejectRole;
    } else if (role == "apply") {
      targetRole = QDialogButtonBox::ApplyRole;
    } else if (role == "help") {
      targetRole = QDialogButtonBox::HelpRole;
    } else if (role == "reset") {
      targetRole = QDialogButtonBox::ResetRole;
    } else if (role == "discard") {
      targetRole = QDialogButtonBox::DestructiveRole;
    }

    if (targetRole != QDialogButtonBox::InvalidRole) {
      QList<QAbstractButton*> buttons = buttonBox->buttons();
      for (QAbstractButton* btn : buttons) {
        if (buttonBox->buttonRole(btn) == targetRole) {
          return btn;
        }
      }
    }
  }

  return nullptr;
}

QWidget* ElementFinder::resolveSelector(const QString& selector, QString& errorOut) {
  if (selector.isEmpty()) {
    errorOut = "Empty selector";
    return nullptr;
  }

  QWidget* result = nullptr;

  // @name:objectName
  if (selector.startsWith("@name:")) {
    QString name = selector.mid(6);
    result = byName(name);
    if (!result) {
      errorOut = QStringLiteral("No widget with objectName '%1'").arg(name);
    }
  }
  // @class:ClassName
  else if (selector.startsWith("@class:")) {
    QString className = selector.mid(7);
    result = byClass(className);
    if (!result) {
      errorOut = QStringLiteral("No widget with class '%1'").arg(className);
    }
  }
  // @text:Text Content
  else if (selector.startsWith("@text:")) {
    QString text = selector.mid(6);
    result = byText(text);
    if (!result) {
      errorOut = QStringLiteral("No widget with text '%1'").arg(text);
    }
  }
  // @accessible:Accessible Name
  else if (selector.startsWith("@accessible:")) {
    QString name = selector.mid(12);
    result = byAccessible(name);
    if (!result) {
      errorOut = QStringLiteral("No widget with accessible name '%1'").arg(name);
    }
  }
  // Semantic role selectors: @accept, @reject, @apply, @help, @yes, @no, @save, @discard, @close,
  // @reset
  else if (selector.startsWith("@")) {
    QString role = selector.mid(1).toLower();
    result = byDialogRole(role);
    if (!result) {
      errorOut = QStringLiteral("No dialog button with role '%1' found in active window").arg(role);
    }
  }
  // Path-based: parent/child/grandchild
  else {
    result = byPath(selector);
    if (!result) {
      errorOut = QStringLiteral("No widget matching path '%1'").arg(selector);
    }
  }

  return result;
}

QList<QWidget*> ElementFinder::topLevelWidgets() {
  return QApplication::topLevelWidgets();
}

QWidget* ElementFinder::findChild(QWidget* parent, const QString& name, bool recursive) {
  if (!parent) {
    return nullptr;
  }

  // Check for index notation: ClassName[index]
  static QRegularExpression indexRegex(R"(^(.+)\[(\d+)\]$)");
  QRegularExpressionMatch match = indexRegex.match(name);

  if (match.hasMatch()) {
    QString baseName = match.captured(1);
    int index = match.captured(2).toInt();

    Qt::FindChildOptions opts =
        recursive ? Qt::FindChildrenRecursively : Qt::FindDirectChildrenOnly;
    QList<QWidget*> children = parent->findChildren<QWidget*>(QString(), opts);

    int count = 0;
    for (QWidget* child : children) {
      bool matches = (child->objectName() == baseName) ||
                     (QString::fromLatin1(child->metaObject()->className()) == baseName);
      if (matches) {
        if (count == index) {
          return child;
        }
        ++count;
      }
    }
    return nullptr;
  }

  // Handle wildcard: *
  if (name == "*") {
    // Return first child
    Qt::FindChildOptions opts =
        recursive ? Qt::FindChildrenRecursively : Qt::FindDirectChildrenOnly;
    QList<QWidget*> children = parent->findChildren<QWidget*>(QString(), opts);
    return children.isEmpty() ? nullptr : children.first();
  }

  // Normal name lookup
  Qt::FindChildOptions opts = recursive ? Qt::FindChildrenRecursively : Qt::FindDirectChildrenOnly;

  // Try by objectName first
  QWidget* result = parent->findChild<QWidget*>(name, opts);
  if (result) {
    return result;
  }

  // Try by class name
  QList<QWidget*> children = parent->findChildren<QWidget*>(QString(), opts);
  for (QWidget* child : children) {
    if (QString::fromLatin1(child->metaObject()->className()) == name) {
      return child;
    }
  }

  return nullptr;
}

}  // namespace widgeteer
