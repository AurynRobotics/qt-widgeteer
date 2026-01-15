#include <widgeteer/EventBroadcaster.h>

#include <QSignalSpy>
#include <QTest>

using namespace widgeteer;

class TestEventBroadcaster : public QObject {
  Q_OBJECT

private slots:
  void testInitialState() {
    EventBroadcaster broadcaster;

    QVERIFY(!broadcaster.isEnabled());
    QVERIFY(!broadcaster.hasSubscribers("any_event"));
  }

  void testSetEnabled() {
    EventBroadcaster broadcaster;

    broadcaster.setEnabled(true);
    QVERIFY(broadcaster.isEnabled());

    broadcaster.setEnabled(false);
    QVERIFY(!broadcaster.isEnabled());
  }

  void testSubscribe() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "widget_created");

    QVERIFY(broadcaster.hasSubscribers("widget_created"));
    QCOMPARE(broadcaster.subscribersFor("widget_created"), QStringList{ "client1" });
    QCOMPARE(broadcaster.clientSubscriptions("client1"), QStringList{ "widget_created" });
  }

  void testMultipleSubscriptions() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "widget_created");
    broadcaster.subscribe("client1", "property_changed");
    broadcaster.subscribe("client2", "widget_created");

    // Check subscribers for widget_created
    QStringList subscribers = broadcaster.subscribersFor("widget_created");
    QCOMPARE(subscribers.size(), 2);
    QVERIFY(subscribers.contains("client1"));
    QVERIFY(subscribers.contains("client2"));

    // Check client1's subscriptions
    QStringList client1Subs = broadcaster.clientSubscriptions("client1");
    QCOMPARE(client1Subs.size(), 2);
    QVERIFY(client1Subs.contains("widget_created"));
    QVERIFY(client1Subs.contains("property_changed"));
  }

  void testUnsubscribe() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "widget_created");
    broadcaster.subscribe("client1", "property_changed");

    broadcaster.unsubscribe("client1", "widget_created");

    QVERIFY(!broadcaster.subscribersFor("widget_created").contains("client1"));
    QCOMPARE(broadcaster.clientSubscriptions("client1"), QStringList{ "property_changed" });
  }

  void testUnsubscribeAll() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "widget_created");
    broadcaster.subscribe("client1", "property_changed");
    broadcaster.subscribe("client1", "focus_changed");

    broadcaster.unsubscribeAll("client1");

    QCOMPARE(broadcaster.clientSubscriptions("client1").size(), 0);
    QVERIFY(!broadcaster.hasSubscribers("widget_created"));
    QVERIFY(!broadcaster.hasSubscribers("property_changed"));
    QVERIFY(!broadcaster.hasSubscribers("focus_changed"));
  }

  void testRemoveClient() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "widget_created");
    broadcaster.removeClient("client1");

    QVERIFY(!broadcaster.hasSubscribers("widget_created"));
    QCOMPARE(broadcaster.clientSubscriptions("client1").size(), 0);
  }

  void testUnsubscribeNonExistentClient() {
    EventBroadcaster broadcaster;

    // Should not crash
    broadcaster.unsubscribe("nonexistent", "widget_created");
    broadcaster.unsubscribeAll("nonexistent");
    broadcaster.removeClient("nonexistent");
  }

  void testEmitEventWhenDisabled() {
    EventBroadcaster broadcaster;
    QSignalSpy spy(&broadcaster, &EventBroadcaster::eventReady);

    broadcaster.subscribe("client1", "widget_created");
    // broadcaster is disabled by default

    broadcaster.emitEvent("widget_created", QJsonObject{ { "test", true } });

    QCOMPARE(spy.count(), 0);  // No event emitted when disabled
  }

  void testEmitEventWhenEnabled() {
    EventBroadcaster broadcaster;
    QSignalSpy spy(&broadcaster, &EventBroadcaster::eventReady);

    broadcaster.setEnabled(true);
    broadcaster.subscribe("client1", "widget_created");

    broadcaster.emitEvent("widget_created", QJsonObject{ { "test", true } });

    QCOMPARE(spy.count(), 1);

    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("widget_created"));
    QCOMPARE(args.at(1).toJsonObject().value("test").toBool(), true);
    QCOMPARE(args.at(2).toStringList(), QStringList{ "client1" });
  }

  void testEmitEventNoSubscribers() {
    EventBroadcaster broadcaster;
    QSignalSpy spy(&broadcaster, &EventBroadcaster::eventReady);

    broadcaster.setEnabled(true);

    broadcaster.emitEvent("widget_created", QJsonObject{});

    QCOMPARE(spy.count(), 0);  // No subscribers, no event
  }

  void testAvailableEventTypes() {
    QStringList types = EventBroadcaster::availableEventTypes();

    QVERIFY(types.contains("widget_created"));
    QVERIFY(types.contains("widget_destroyed"));
    QVERIFY(types.contains("property_changed"));
    QVERIFY(types.contains("focus_changed"));
    QVERIFY(types.contains("command_executed"));
  }

  void testCleanupOnUnsubscribe() {
    EventBroadcaster broadcaster;

    broadcaster.subscribe("client1", "event1");
    broadcaster.unsubscribe("client1", "event1");

    // After unsubscribing, client1 should be completely removed
    QCOMPARE(broadcaster.clientSubscriptions("client1").size(), 0);
    QVERIFY(!broadcaster.hasSubscribers("event1"));
  }
};

QTEST_MAIN(TestEventBroadcaster)
#include "test_event_broadcaster.moc"
