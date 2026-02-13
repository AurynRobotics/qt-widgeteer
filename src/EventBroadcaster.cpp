#include <widgeteer/EventBroadcaster.h>

#include <algorithm>

namespace widgeteer {

EventBroadcaster::EventBroadcaster(QObject* parent) : QObject(parent) {
}

void EventBroadcaster::setEnabled(bool enabled) {
  enabled_ = enabled;
}

bool EventBroadcaster::isEnabled() const {
  return enabled_;
}

void EventBroadcaster::subscribe(const QString& clientId, const QString& eventType,
                                 const QJsonObject& filter) {
  QList<SubscriptionEntry>& entries = clientSubscriptions_[clientId];
  for (const SubscriptionEntry& entry : entries) {
    if (entry.eventType == eventType && entry.filter == filter) {
      return;
    }
  }

  entries.append(SubscriptionEntry{ eventType, filter });
  eventSubscribers_[eventType].insert(clientId);
}

void EventBroadcaster::unsubscribe(const QString& clientId, const QString& eventType) {
  if (clientSubscriptions_.contains(clientId)) {
    QList<SubscriptionEntry>& entries = clientSubscriptions_[clientId];
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&eventType](const SubscriptionEntry& entry) {
                                   return entry.eventType == eventType;
                                 }),
                  entries.end());

    if (entries.isEmpty()) {
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

  const QList<SubscriptionEntry> entries = clientSubscriptions_.take(clientId);
  QSet<QString> events;
  for (const SubscriptionEntry& entry : entries) {
    events.insert(entry.eventType);
  }

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

  QStringList result;
  QSet<QString> seen;
  for (const SubscriptionEntry& entry : clientSubscriptions_[clientId]) {
    if (!seen.contains(entry.eventType)) {
      seen.insert(entry.eventType);
      result.append(entry.eventType);
    }
  }
  return result;
}

QList<QJsonObject> EventBroadcaster::filtersForEvent(const QString& eventType) const {
  QList<QJsonObject> filters;
  if (!eventSubscribers_.contains(eventType)) {
    return filters;
  }

  for (const QString& clientId : eventSubscribers_[eventType]) {
    if (!clientSubscriptions_.contains(clientId)) {
      continue;
    }
    for (const SubscriptionEntry& entry : clientSubscriptions_[clientId]) {
      if (entry.eventType == eventType) {
        filters.append(entry.filter);
      }
    }
  }

  return filters;
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

  QStringList recipients;
  for (const QString& clientId : subscribersFor(eventType)) {
    if (!clientSubscriptions_.contains(clientId)) {
      continue;
    }

    bool matched = false;
    for (const SubscriptionEntry& entry : clientSubscriptions_[clientId]) {
      if (entry.eventType == eventType && matchesFilter(eventType, data, entry.filter)) {
        matched = true;
        break;
      }
    }
    if (matched) {
      recipients.append(clientId);
    }
  }

  if (recipients.isEmpty()) {
    return;
  }

  emit eventReady(eventType, data, recipients);
}

bool EventBroadcaster::matchesFilter(const QString& eventType, const QJsonObject& data,
                                     const QJsonObject& filter) const {
  if (filter.isEmpty()) {
    return true;
  }

  const QString filterProperty = filter.value("property").toString();
  if (!filterProperty.isEmpty() && eventType == "property_changed") {
    if (data.value("property").toString() != filterProperty) {
      return false;
    }
  }

  const QString filterTarget = filter.value("target").toString();
  if (filterTarget.isEmpty()) {
    return true;
  }

  auto matchesValue = [&filterTarget](const QString& value) {
    if (value.isEmpty()) {
      return false;
    }

    if (filterTarget.startsWith("@name:")) {
      return value == filterTarget.mid(6);
    }
    if (filterTarget.startsWith("@class:")) {
      return value == filterTarget.mid(7);
    }

    return value == filterTarget || value.startsWith(filterTarget + "/");
  };

  if (matchesValue(data.value("path").toString()) ||
      matchesValue(data.value("oldPath").toString()) ||
      matchesValue(data.value("newPath").toString()) ||
      matchesValue(data.value("parentPath").toString()) ||
      matchesValue(data.value("objectName").toString()) ||
      matchesValue(data.value("oldObjectName").toString()) ||
      matchesValue(data.value("newObjectName").toString()) ||
      matchesValue(data.value("class").toString())) {
    return true;
  }

  return false;
}

}  // namespace widgeteer
