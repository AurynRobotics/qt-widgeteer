#pragma once

#include <widgeteer/Export.h>

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

namespace widgeteer {

class WIDGETEER_EXPORT EventBroadcaster : public QObject {
  Q_OBJECT

public:
  struct SubscriptionEntry {
    QString eventType;
    QJsonObject filter;
  };

  explicit EventBroadcaster(QObject* parent = nullptr);

  // Enable/disable broadcasting
  void setEnabled(bool enabled);
  bool isEnabled() const;

  // Subscription management
  void subscribe(const QString& clientId, const QString& eventType,
                 const QJsonObject& filter = QJsonObject());
  void unsubscribe(const QString& clientId, const QString& eventType);
  void unsubscribeAll(const QString& clientId);
  void removeClient(const QString& clientId);

  // Query subscriptions
  bool hasSubscribers(const QString& eventType) const;
  QStringList subscribersFor(const QString& eventType) const;
  QStringList clientSubscriptions(const QString& clientId) const;
  QList<QJsonObject> filtersForEvent(const QString& eventType) const;

  // Available event types
  static QStringList availableEventTypes();

signals:
  // Emitted when an event should be broadcast to clients
  void eventReady(const QString& eventType, const QJsonObject& data,
                  const QStringList& recipientClientIds);

public slots:
  // Emit a custom event
  void emitEvent(const QString& eventType, const QJsonObject& data);

private:
  bool matchesFilter(const QString& eventType, const QJsonObject& data,
                     const QJsonObject& filter) const;

  bool enabled_ = false;

  // clientId -> subscription entries
  QHash<QString, QList<SubscriptionEntry>> clientSubscriptions_;

  // eventType -> set of clientIds subscribed to it
  QHash<QString, QSet<QString>> eventSubscribers_;
};

}  // namespace widgeteer
