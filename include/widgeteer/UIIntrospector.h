#pragma once

#include <widgeteer/Export.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

namespace widgeteer
{

class WIDGETEER_EXPORT UIIntrospector
{
public:
  struct TreeOptions
  {
    int maxDepth;
    bool includeInvisible;
    bool includeGeometry;
    bool includeProperties;
    QStringList classFilter;

    TreeOptions()
      : maxDepth(-1), includeInvisible(false), includeGeometry(true), includeProperties(false)
    {
    }
  };

  UIIntrospector();

  // Get widget tree as JSON
  QJsonObject getTree(QWidget* root = nullptr, const TreeOptions& opts = TreeOptions());

  // Describe a single widget in detail
  QJsonObject describe(QWidget* widget);

  // List all properties of a widget
  QJsonArray listProperties(QWidget* widget);

  // List available actions/slots for a widget
  QJsonArray listActions(QWidget* widget);

private:
  // Convert widget to JSON recursively
  QJsonObject widgetToJson(QWidget* widget, int depth, const TreeOptions& opts);

  // Convert QVariant to JSON value
  QJsonValue propertyToJson(const QVariant& value);

  // Infer semantic role for widget ("button", "input", "container", etc.)
  QString inferWidgetRole(QWidget* widget);

  // Get widget-specific properties based on type
  QJsonObject getWidgetSpecificProps(QWidget* widget);
};

}  // namespace widgeteer
