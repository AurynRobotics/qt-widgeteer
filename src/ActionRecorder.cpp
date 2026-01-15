#include <widgeteer/ActionRecorder.h>

namespace widgeteer {

QJsonObject RecordedAction::toJson() const {
  QJsonObject json;
  json["command"] = command;
  json["params"] = params;
  return json;
}

ActionRecorder::ActionRecorder(QObject* parent) : QObject(parent) {
}

void ActionRecorder::start() {
  if (recording_) {
    return;
  }

  clear();
  recording_ = true;
  startTime_ = QDateTime::currentDateTime();
  emit recordingStarted();
}

void ActionRecorder::stop() {
  if (!recording_) {
    return;
  }

  recording_ = false;
  endTime_ = QDateTime::currentDateTime();
  emit recordingStopped();
}

bool ActionRecorder::isRecording() const {
  return recording_;
}

void ActionRecorder::recordCommand(const Command& cmd, const Response& response) {
  if (!recording_) {
    return;
  }

  // Skip introspection-only commands that don't modify state
  static const QSet<QString> skipCommands = {
    "get_tree",   "find",   "describe",   "get_property", "list_properties",     "get_actions",
    "screenshot", "exists", "is_visible", "list_objects", "list_custom_commands"
  };

  if (skipCommands.contains(cmd.name)) {
    return;
  }

  RecordedAction action;
  action.command = cmd.name;
  action.params = cmd.params;
  action.timestamp = QDateTime::currentDateTime();
  action.durationMs = response.durationMs;

  actions_.append(action);
  emit actionRecorded(action);
}

QJsonObject ActionRecorder::getRecording() const {
  QJsonObject recording;
  recording["name"] = QStringLiteral("Recorded Session");
  recording["description"] = QStringLiteral("Recorded on %1").arg(startTime_.toString(Qt::ISODate));

  QJsonArray tests;
  QJsonObject test;
  test["name"] = QStringLiteral("Recorded Test");

  QJsonArray steps;
  for (const RecordedAction& action : actions_) {
    steps.append(action.toJson());
  }
  test["steps"] = steps;
  test["assertions"] = QJsonArray();

  tests.append(test);
  recording["tests"] = tests;
  recording["setup"] = QJsonArray();
  recording["teardown"] = QJsonArray();

  return recording;
}

void ActionRecorder::clear() {
  actions_.clear();
  startTime_ = QDateTime();
  endTime_ = QDateTime();
}

int ActionRecorder::actionCount() const {
  return actions_.size();
}

QDateTime ActionRecorder::startTime() const {
  return startTime_;
}

QDateTime ActionRecorder::endTime() const {
  return endTime_;
}

}  // namespace widgeteer
