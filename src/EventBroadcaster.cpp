#include <widgeteer/EventBroadcaster.h>

namespace widgeteer {

EventBroadcaster::EventBroadcaster(QObject* parent) : QObject(parent) {
}

void EventBroadcaster::setEnabled(bool enabled) {
  enabled_ = enabled;
}

bool EventBroadcaster::isEnabled() const {
  return enabled_;
}

void EventBroadcaster::subscribe(const QString& clientId, const QString& eventType) {
  clientSubscriptions_[clientId].insert(eventType);
  eventSubscribers_[eventType].insert(clientId);
}

void EventBroadcaster::unsubscribe(const QString& clientId, const QString& eventType) {
  if (clientSubscriptions_.contains(clientId)) {
    clientSubscriptions_[clientId].remove(eventType);
    if (clientSubscriptions_[clientId].isEmpty()) {
      clientSubscriptions_.remove(clientId);
    }
  }

  if (eventSubscribers_.contains(eventType)) {
    eventSubscribers_[eventType].remove(clientId);
    if (eventSubscribers_[eventType].isEmpty()) {
      eventSubscribers_.remove(eventType);
    }
  }
}

void EventBroadcaster::unsubscribeAll(const QString& clientId) {
  if (!clientSubscriptions_.contains(clientId)) {
    return;
  }

  const QSet<QString> events = clientSubscriptions_.take(clientId);
  for (const QString& eventType : events) {
    if (eventSubscribers_.contains(eventType)) {
      eventSubscribers_[eventType].remove(clientId);
      if (eventSubscribers_[eventType].isEmpty()) {
        eventSubscribers_.remove(eventType);
      }
    }
  }
}

void EventBroadcaster::removeClient(const QString& clientId) {
  unsubscribeAll(clientId);
}

bool EventBroadcaster::hasSubscribers(const QString& eventType) const {
  return eventSubscribers_.contains(eventType) && !eventSubscribers_[eventType].isEmpty();
}

QStringList EventBroadcaster::subscribersFor(const QString& eventType) const {
  if (!eventSubscribers_.contains(eventType)) {
    return {};
  }

  return eventSubscribers_[eventType].values();
}

QStringList EventBroadcaster::clientSubscriptions(const QString& clientId) const {
  if (!clientSubscriptions_.contains(clientId)) {
    return {};
  }

  return clientSubscriptions_[clientId].values();
}

QStringList EventBroadcaster::availableEventTypes() {
  return { "widget_created", "widget_destroyed", "property_changed", "focus_changed",
           "command_executed" };
}

void EventBroadcaster::emitEvent(const QString& eventType, const QJsonObject& data) {
  if (!enabled_) {
    return;
  }

  if (!hasSubscribers(eventType)) {
    return;
  }

  const QStringList recipients = subscribersFor(eventType);
  emit eventReady(eventType, data, recipients);
}

}  // namespace widgeteer
