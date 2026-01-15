#pragma once

#include <widgeteer/Export.h>
#include <widgeteer/Protocol.h>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

namespace widgeteer {

struct WIDGETEER_EXPORT RecordedAction {
  QString command;
  QJsonObject params;
  QDateTime timestamp;
  int durationMs = 0;

  QJsonObject toJson() const;
};

class WIDGETEER_EXPORT ActionRecorder : public QObject {
  Q_OBJECT

public:
  explicit ActionRecorder(QObject* parent = nullptr);

  // Recording control
  void start();
  void stop();
  bool isRecording() const;

  // Record an executed command
  void recordCommand(const Command& cmd, const Response& response);

  // Get recording in sample_tests.json format
  QJsonObject getRecording() const;

  // Clear recording
  void clear();

  // Stats
  int actionCount() const;
  QDateTime startTime() const;
  QDateTime endTime() const;

signals:
  void recordingStarted();
  void recordingStopped();
  void actionRecorded(const RecordedAction& action);

private:
  bool recording_ = false;
  QDateTime startTime_;
  QDateTime endTime_;
  QList<RecordedAction> actions_;
};

}  // namespace widgeteer
