#include <widgeteer/UIIntrospector.h>

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QProgressBar>
#include <QRadioButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>

namespace widgeteer
{

UIIntrospector::UIIntrospector() = default;

QJsonObject UIIntrospector::getTree(QWidget* root, const TreeOptions& opts)
{
  if (!root)
  {
    // Find the main window or first visible top-level widget
    QWidgetList topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget* w : topLevelWidgets)
    {
      if (w->isVisible())
      {
        root = w;
        break;
      }
    }
    if (!root && !topLevelWidgets.isEmpty())
    {
      root = topLevelWidgets.first();
    }
  }

  if (!root)
  {
    return QJsonObject();
  }

  return widgetToJson(root, 0, opts);
}

QJsonObject UIIntrospector::describe(QWidget* widget)
{
  if (!widget)
  {
    return QJsonObject();
  }

  QJsonObject json;
  json["objectName"] = widget->objectName();
  json["class"] = QString::fromLatin1(widget->metaObject()->className());
  json["role"] = inferWidgetRole(widget);
  json["visible"] = widget->isVisible();
  json["enabled"] = widget->isEnabled();
  json["focused"] = widget->hasFocus();

  // Geometry
  QRect geom = widget->geometry();
  QJsonObject geometry;
  geometry["x"] = geom.x();
  geometry["y"] = geom.y();
  geometry["width"] = geom.width();
  geometry["height"] = geom.height();
  json["geometry"] = geometry;

  // Global position
  QPoint globalPos = widget->mapToGlobal(QPoint(0, 0));
  QJsonObject global;
  global["x"] = globalPos.x();
  global["y"] = globalPos.y();
  json["globalPosition"] = global;

  // Widget-specific properties
  QJsonObject specific = getWidgetSpecificProps(widget);
  for (auto it = specific.begin(); it != specific.end(); ++it)
  {
    json[it.key()] = it.value();
  }

  // Window-specific info
  if (widget->isWindow())
  {
    json["windowTitle"] = widget->windowTitle();
  }

  return json;
}

QJsonArray UIIntrospector::listProperties(QWidget* widget)
{
  QJsonArray properties;

  if (!widget)
  {
    return properties;
  }

  const QMetaObject* meta = widget->metaObject();
  for (int i = 0; i < meta->propertyCount(); ++i)
  {
    QMetaProperty prop = meta->property(i);
    QJsonObject propObj;
    propObj["name"] = QString::fromLatin1(prop.name());
    propObj["type"] = QString::fromLatin1(prop.typeName());
    propObj["readable"] = prop.isReadable();
    propObj["writable"] = prop.isWritable();

    if (prop.isReadable())
    {
      propObj["value"] = propertyToJson(widget->property(prop.name()));
    }

    properties.append(propObj);
  }

  return properties;
}

QJsonArray UIIntrospector::listActions(QWidget* widget)
{
  QJsonArray actions;

  if (!widget)
  {
    return actions;
  }

  const QMetaObject* meta = widget->metaObject();

  // List slots
  for (int i = 0; i < meta->methodCount(); ++i)
  {
    QMetaMethod method = meta->method(i);
    if (method.methodType() == QMetaMethod::Slot || method.methodType() == QMetaMethod::Signal)
    {
      // Only include public slots and Q_INVOKABLE methods
      if (method.access() != QMetaMethod::Public)
      {
        continue;
      }

      QJsonObject action;
      action["name"] = QString::fromLatin1(method.name());
      action["type"] = method.methodType() == QMetaMethod::Slot ? "slot" : "signal";
      action["signature"] = QString::fromLatin1(method.methodSignature());

      QJsonArray params;
      for (int j = 0; j < method.parameterCount(); ++j)
      {
        QJsonObject param;
        param["name"] = QString::fromLatin1(method.parameterNames().at(j));
        param["type"] = QString::fromLatin1(method.parameterTypeName(j));
        params.append(param);
      }
      action["parameters"] = params;

      actions.append(action);
    }
  }

  // List widget-specific actions
  QList<QAction*> widgetActions = widget->actions();
  for (QAction* act : widgetActions)
  {
    QJsonObject action;
    action["name"] = act->objectName().isEmpty() ? act->text() : act->objectName();
    action["type"] = "qaction";
    action["text"] = act->text();
    action["enabled"] = act->isEnabled();
    action["checkable"] = act->isCheckable();
    if (act->isCheckable())
    {
      action["checked"] = act->isChecked();
    }
    actions.append(action);
  }

  return actions;
}

QJsonObject UIIntrospector::widgetToJson(QWidget* widget, int depth, const TreeOptions& opts)
{
  if (!widget)
  {
    return QJsonObject();
  }

  // Check depth limit
  if (opts.maxDepth >= 0 && depth > opts.maxDepth)
  {
    return QJsonObject();
  }

  // Check visibility filter
  if (!opts.includeInvisible && !widget->isVisible())
  {
    return QJsonObject();
  }

  // Check class filter
  if (!opts.classFilter.isEmpty())
  {
    QString className = QString::fromLatin1(widget->metaObject()->className());
    if (!opts.classFilter.contains(className))
    {
      // Still need to check children
      QJsonArray childrenArray;
      QList<QWidget*> children =
          widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
      for (QWidget* child : children)
      {
        QJsonObject childJson = widgetToJson(child, depth + 1, opts);
        if (!childJson.isEmpty())
        {
          childrenArray.append(childJson);
        }
      }
      if (childrenArray.isEmpty())
      {
        return QJsonObject();
      }
      // Return just the children if this widget doesn't match filter
      if (childrenArray.size() == 1)
      {
        return childrenArray.first().toObject();
      }
      QJsonObject wrapper;
      wrapper["children"] = childrenArray;
      return wrapper;
    }
  }

  QJsonObject json;
  json["objectName"] = widget->objectName();
  json["class"] = QString::fromLatin1(widget->metaObject()->className());
  json["role"] = inferWidgetRole(widget);
  json["visible"] = widget->isVisible();
  json["enabled"] = widget->isEnabled();

  if (opts.includeGeometry)
  {
    QRect geom = widget->geometry();
    QJsonObject geometry;
    geometry["x"] = geom.x();
    geometry["y"] = geom.y();
    geometry["width"] = geom.width();
    geometry["height"] = geom.height();
    json["geometry"] = geometry;
  }

  // Add widget-specific properties
  QJsonObject specific = getWidgetSpecificProps(widget);
  for (auto it = specific.begin(); it != specific.end(); ++it)
  {
    json[it.key()] = it.value();
  }

  if (opts.includeProperties)
  {
    json["properties"] = listProperties(widget);
  }

  // Add children
  QList<QWidget*> children = widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  if (!children.isEmpty())
  {
    QJsonArray childrenArray;
    for (QWidget* child : children)
    {
      QJsonObject childJson = widgetToJson(child, depth + 1, opts);
      if (!childJson.isEmpty())
      {
        childrenArray.append(childJson);
      }
    }
    if (!childrenArray.isEmpty())
    {
      json["children"] = childrenArray;
    }
  }

  return json;
}

QJsonValue UIIntrospector::propertyToJson(const QVariant& value)
{
  switch (value.typeId())
  {
    case QMetaType::Bool:
      return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
      return value.toLongLong();
    case QMetaType::Double:
    case QMetaType::Float:
      return value.toDouble();
    case QMetaType::QString:
      return value.toString();
    case QMetaType::QStringList: {
      QJsonArray arr;
      for (const QString& s : value.toStringList())
      {
        arr.append(s);
      }
      return arr;
    }
    case QMetaType::QRect: {
      QRect r = value.toRect();
      QJsonObject obj;
      obj["x"] = r.x();
      obj["y"] = r.y();
      obj["width"] = r.width();
      obj["height"] = r.height();
      return obj;
    }
    case QMetaType::QSize: {
      QSize s = value.toSize();
      QJsonObject obj;
      obj["width"] = s.width();
      obj["height"] = s.height();
      return obj;
    }
    case QMetaType::QPoint: {
      QPoint p = value.toPoint();
      QJsonObject obj;
      obj["x"] = p.x();
      obj["y"] = p.y();
      return obj;
    }
    default:
      // Try to convert to string
      if (value.canConvert<QString>())
      {
        return value.toString();
      }
      return QJsonValue::Null;
  }
}

QString UIIntrospector::inferWidgetRole(QWidget* widget)
{
  if (!widget)
  {
    return "unknown";
  }

  // Check specific widget types
  if (qobject_cast<QPushButton*>(widget))
    return "button";
  if (qobject_cast<QRadioButton*>(widget))
    return "radio";
  if (qobject_cast<QCheckBox*>(widget))
    return "checkbox";
  if (qobject_cast<QAbstractButton*>(widget))
    return "button";
  if (qobject_cast<QLineEdit*>(widget))
    return "textfield";
  if (qobject_cast<QTextEdit*>(widget))
    return "textarea";
  if (qobject_cast<QComboBox*>(widget))
    return "combobox";
  if (qobject_cast<QAbstractSpinBox*>(widget))
    return "spinbox";
  if (qobject_cast<QAbstractSlider*>(widget))
    return "slider";
  if (qobject_cast<QProgressBar*>(widget))
    return "progressbar";
  if (qobject_cast<QLabel*>(widget))
    return "label";
  if (qobject_cast<QListWidget*>(widget))
    return "list";
  if (qobject_cast<QTreeWidget*>(widget))
    return "tree";
  if (qobject_cast<QTableWidget*>(widget))
    return "table";
  if (qobject_cast<QTabWidget*>(widget))
    return "tabwidget";
  if (qobject_cast<QMenuBar*>(widget))
    return "menubar";
  if (qobject_cast<QMenu*>(widget))
    return "menu";
  if (qobject_cast<QToolBar*>(widget))
    return "toolbar";
  if (qobject_cast<QStatusBar*>(widget))
    return "statusbar";
  if (qobject_cast<QGroupBox*>(widget))
    return "group";

  // Window types
  if (widget->isWindow())
  {
    return "window";
  }

  // Default to container for widgets with children
  if (!widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).isEmpty())
  {
    return "container";
  }

  return "widget";
}

QJsonObject UIIntrospector::getWidgetSpecificProps(QWidget* widget)
{
  QJsonObject props;

  if (auto* btn = qobject_cast<QAbstractButton*>(widget))
  {
    props["text"] = btn->text();
    if (btn->isCheckable())
    {
      props["checkable"] = true;
      props["checked"] = btn->isChecked();
    }
  }
  else if (auto* label = qobject_cast<QLabel*>(widget))
  {
    props["text"] = label->text();
  }
  else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
  {
    props["text"] = lineEdit->text();
    props["placeholderText"] = lineEdit->placeholderText();
    props["readOnly"] = lineEdit->isReadOnly();
  }
  else if (auto* textEdit = qobject_cast<QTextEdit*>(widget))
  {
    props["plainText"] = textEdit->toPlainText();
    props["readOnly"] = textEdit->isReadOnly();
  }
  else if (auto* combo = qobject_cast<QComboBox*>(widget))
  {
    props["currentIndex"] = combo->currentIndex();
    props["currentText"] = combo->currentText();
    props["count"] = combo->count();
    QJsonArray items;
    for (int i = 0; i < combo->count(); ++i)
    {
      items.append(combo->itemText(i));
    }
    props["items"] = items;
  }
  else if (auto* slider = qobject_cast<QAbstractSlider*>(widget))
  {
    props["value"] = slider->value();
    props["minimum"] = slider->minimum();
    props["maximum"] = slider->maximum();
  }
  else if (auto* progress = qobject_cast<QProgressBar*>(widget))
  {
    props["value"] = progress->value();
    props["minimum"] = progress->minimum();
    props["maximum"] = progress->maximum();
  }
  else if (auto* group = qobject_cast<QGroupBox*>(widget))
  {
    props["title"] = group->title();
    if (group->isCheckable())
    {
      props["checkable"] = true;
      props["checked"] = group->isChecked();
    }
  }
  else if (auto* tab = qobject_cast<QTabWidget*>(widget))
  {
    props["currentIndex"] = tab->currentIndex();
    props["count"] = tab->count();
    QJsonArray tabs;
    for (int i = 0; i < tab->count(); ++i)
    {
      tabs.append(tab->tabText(i));
    }
    props["tabs"] = tabs;
  }

  return props;
}

}  // namespace widgeteer
