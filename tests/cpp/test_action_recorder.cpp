#include <widgeteer/ActionRecorder.h>

#include <QSignalSpy>
#include <QTest>

using namespace widgeteer;

class TestActionRecorder : public QObject
{
  Q_OBJECT

private slots:
  void testInitialState()
  {
    ActionRecorder recorder;

    QVERIFY(!recorder.isRecording());
    QCOMPARE(recorder.actionCount(), 0);
  }

  void testStartRecording()
  {
    ActionRecorder recorder;
    QSignalSpy startedSpy(&recorder, &ActionRecorder::recordingStarted);

    recorder.start();

    QVERIFY(recorder.isRecording());
    QCOMPARE(startedSpy.count(), 1);
  }

  void testStopRecording()
  {
    ActionRecorder recorder;
    QSignalSpy stoppedSpy(&recorder, &ActionRecorder::recordingStopped);

    recorder.start();
    recorder.stop();

    QVERIFY(!recorder.isRecording());
    QCOMPARE(stoppedSpy.count(), 1);
  }

  void testStartWhileRecording()
  {
    ActionRecorder recorder;
    QSignalSpy startedSpy(&recorder, &ActionRecorder::recordingStarted);

    recorder.start();
    recorder.start();  // Should be ignored

    QCOMPARE(startedSpy.count(), 1);  // Only one start signal
  }

  void testStopWithoutStart()
  {
    ActionRecorder recorder;
    QSignalSpy stoppedSpy(&recorder, &ActionRecorder::recordingStopped);

    recorder.stop();  // Should be ignored

    QCOMPARE(stoppedSpy.count(), 0);
  }

  void testRecordCommand()
  {
    ActionRecorder recorder;
    QSignalSpy recordedSpy(&recorder, &ActionRecorder::actionRecorded);

    recorder.start();

    Command cmd;
    cmd.name = "click";
    cmd.params = QJsonObject{ { "target", "@name:button1" } };

    Response resp;
    resp.success = true;
    resp.durationMs = 10;

    recorder.recordCommand(cmd, resp);

    QCOMPARE(recorder.actionCount(), 1);
    QCOMPARE(recordedSpy.count(), 1);
  }

  void testRecordCommandSkipsIntrospection()
  {
    ActionRecorder recorder;
    recorder.start();

    // Introspection commands should be skipped
    QStringList skipCommands = {
      "get_tree",   "find",   "describe",   "get_property", "list_properties",     "get_actions",
      "screenshot", "exists", "is_visible", "list_objects", "list_custom_commands"
    };

    for (const QString& cmdName : skipCommands)
    {
      Command cmd;
      cmd.name = cmdName;
      Response resp;
      resp.success = true;
      recorder.recordCommand(cmd, resp);
    }

    QCOMPARE(recorder.actionCount(), 0);  // All skipped
  }

  void testRecordCommandWhenNotRecording()
  {
    ActionRecorder recorder;

    Command cmd;
    cmd.name = "click";
    Response resp;
    resp.success = true;

    recorder.recordCommand(cmd, resp);

    QCOMPARE(recorder.actionCount(), 0);  // Not recorded
  }

  void testGetRecordingFormat()
  {
    ActionRecorder recorder;
    recorder.start();

    Command cmd;
    cmd.name = "click";
    cmd.params = QJsonObject{ { "target", "@name:button1" } };
    Response resp;
    resp.success = true;
    resp.durationMs = 15;

    recorder.recordCommand(cmd, resp);
    recorder.stop();

    QJsonObject recording = recorder.getRecording();

    // Check top-level structure
    QVERIFY(recording.contains("name"));
    QVERIFY(recording.contains("description"));
    QVERIFY(recording.contains("tests"));
    QVERIFY(recording.contains("setup"));
    QVERIFY(recording.contains("teardown"));

    // Check tests array
    QJsonArray tests = recording.value("tests").toArray();
    QCOMPARE(tests.size(), 1);

    QJsonObject test = tests[0].toObject();
    QVERIFY(test.contains("name"));
    QVERIFY(test.contains("steps"));
    QVERIFY(test.contains("assertions"));

    // Check steps
    QJsonArray steps = test.value("steps").toArray();
    QCOMPARE(steps.size(), 1);

    QJsonObject step = steps[0].toObject();
    QCOMPARE(step.value("command").toString(), QString("click"));
    QCOMPARE(step.value("params").toObject().value("target").toString(), QString("@name:button1"));
  }

  void testClear()
  {
    ActionRecorder recorder;
    recorder.start();

    Command cmd;
    cmd.name = "click";
    Response resp;
    resp.success = true;

    recorder.recordCommand(cmd, resp);
    QCOMPARE(recorder.actionCount(), 1);

    recorder.clear();
    QCOMPARE(recorder.actionCount(), 0);
  }

  void testStartClearsRecording()
  {
    ActionRecorder recorder;
    recorder.start();

    Command cmd;
    cmd.name = "click";
    Response resp;
    resp.success = true;

    recorder.recordCommand(cmd, resp);
    recorder.stop();

    QCOMPARE(recorder.actionCount(), 1);

    // Start new recording should clear previous
    recorder.start();
    QCOMPARE(recorder.actionCount(), 0);
  }

  void testTimestamps()
  {
    ActionRecorder recorder;

    recorder.start();
    QDateTime startTime = recorder.startTime();
    QVERIFY(startTime.isValid());

    recorder.stop();
    QDateTime endTime = recorder.endTime();
    QVERIFY(endTime.isValid());
    QVERIFY(endTime >= startTime);
  }
};

QTEST_MAIN(TestActionRecorder)
#include "test_action_recorder.moc"
