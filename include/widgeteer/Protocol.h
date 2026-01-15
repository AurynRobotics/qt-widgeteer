#pragma once

#include <widgeteer/Export.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace widgeteer
{

// WebSocket message types
enum class MessageType
{
  Command,      // Client -> Server: Execute a command
  Response,     // Server -> Client: Command result
  Event,        // Server -> Client: Real-time event
  Subscribe,    // Client -> Server: Subscribe to events
  Unsubscribe,  // Client -> Server: Unsubscribe from events
  RecordStart,  // Client -> Server: Start recording
  RecordStop    // Client -> Server: Stop recording
};

// Convert MessageType to string
WIDGETEER_EXPORT QString messageTypeToString(MessageType type);

// Parse MessageType from string (returns std::nullopt on failure)
WIDGETEER_EXPORT std::optional<MessageType> stringToMessageType(const QString& str);

// Error codes for the JSON protocol
namespace ErrorCode
{
constexpr const char* ElementNotFound = "ELEMENT_NOT_FOUND";
constexpr const char* ElementNotVisible = "ELEMENT_NOT_VISIBLE";
constexpr const char* ElementNotEnabled = "ELEMENT_NOT_ENABLED";
constexpr const char* PropertyNotFound = "PROPERTY_NOT_FOUND";
constexpr const char* PropertyReadOnly = "PROPERTY_READ_ONLY";
constexpr const char* InvalidSelector = "INVALID_SELECTOR";
constexpr const char* InvalidCommand = "INVALID_COMMAND";
constexpr const char* InvalidParams = "INVALID_PARAMS";
constexpr const char* Timeout = "TIMEOUT";
constexpr const char* InvocationFailed = "INVOCATION_FAILED";
constexpr const char* ScreenshotFailed = "SCREENSHOT_FAILED";
constexpr const char* TransactionFailed = "TRANSACTION_FAILED";
constexpr const char* InternalError = "INTERNAL_ERROR";
}  // namespace ErrorCode

// Command structure
struct WIDGETEER_EXPORT Command
{
  QString id;
  QString name;
  QJsonObject params;
  QJsonObject options;

  static Command fromJson(const QJsonObject& json);
  QJsonObject toJson() const;
};

// Transaction structure
struct WIDGETEER_EXPORT Transaction
{
  QString id;
  QList<Command> steps;
  bool rollbackOnFailure = true;

  static Transaction fromJson(const QJsonObject& json);
  QJsonObject toJson() const;
};

// Error details for response
struct WIDGETEER_EXPORT ErrorDetails
{
  QString code;
  QString message;
  QJsonObject details;

  QJsonObject toJson() const;
};

// Response structure
struct WIDGETEER_EXPORT Response
{
  QString id;
  bool success = false;
  QJsonObject result;
  ErrorDetails error;
  int durationMs = 0;

  QJsonObject toJson() const;
  static Response ok(const QString& id, const QJsonObject& result = {});
  static Response fail(const QString& id, const QString& code, const QString& message,
                       const QJsonObject& details = {});
};

// Transaction response structure
struct WIDGETEER_EXPORT TransactionResponse
{
  QString id;
  bool success = false;
  int completedSteps = 0;
  int totalSteps = 0;
  QJsonArray stepsResults;
  bool rollbackPerformed = false;

  QJsonObject toJson() const;
};

// Helper to build error responses with context for LLM recovery
WIDGETEER_EXPORT QJsonObject buildElementNotFoundError(const QString& searchedPath,
                                                       const QString& partialMatch,
                                                       const QStringList& availableChildren);

}  // namespace widgeteer
