#pragma once

#include <widgeteer/Export.h>
#include <widgeteer/Protocol.h>

#include <variant>

namespace widgeteer {

/**
 * @brief Result type for operations that can fail.
 *
 * A monadic result type that holds either a success value of type T
 * or an error of type E (defaults to ErrorDetails).
 *
 * @tparam T The success value type
 * @tparam E The error type (defaults to ErrorDetails)
 *
 * Example usage:
 * @code
 * Result<QString> getText(const QString& target);
 *
 * auto result = bot.getText("@name:label");
 * if (result) {
 *     qDebug() << "Text:" << result.value();
 * } else {
 *     qDebug() << "Error:" << result.error().message;
 * }
 * @endcode
 */
template <typename T, typename E = ErrorDetails>
class Result {
public:
  /**
   * @brief Create a successful result with the given value.
   */
  static Result ok(T value) {
    return Result(std::move(value));
  }

  /**
   * @brief Create a failed result with the given error.
   */
  static Result fail(E error) {
    return Result(std::move(error), false);
  }

  /**
   * @brief Check if the result represents success.
   */
  bool success() const {
    return std::holds_alternative<T>(data_);
  }

  /**
   * @brief Boolean conversion for use in if statements.
   */
  explicit operator bool() const {
    return success();
  }

  /**
   * @brief Get the success value. Only valid if success() is true.
   */
  const T& value() const {
    return std::get<T>(data_);
  }

  /**
   * @brief Get the success value (mutable). Only valid if success() is true.
   */
  T& value() {
    return std::get<T>(data_);
  }

  /**
   * @brief Get the value or a default if the result is an error.
   */
  T valueOr(T defaultValue) const {
    return success() ? value() : std::move(defaultValue);
  }

  /**
   * @brief Get the error. Only valid if success() is false.
   */
  const E& error() const {
    return std::get<E>(data_);
  }

private:
  explicit Result(T value) : data_(std::move(value)) {
  }
  Result(E error, bool /*tag*/) : data_(std::move(error)) {
  }

  std::variant<T, E> data_;
};

/**
 * @brief Specialization of Result for void operations.
 *
 * For operations that don't return a value but can still fail.
 *
 * Example usage:
 * @code
 * Result<void> click(const QString& target);
 *
 * if (bot.click("@name:button")) {
 *     qDebug() << "Click succeeded";
 * }
 * @endcode
 */
template <typename E>
class Result<void, E> {
public:
  /**
   * @brief Create a successful void result.
   */
  static Result ok() {
    return Result(true);
  }

  /**
   * @brief Create a failed result with the given error.
   */
  static Result fail(E error) {
    return Result(std::move(error));
  }

  /**
   * @brief Check if the result represents success.
   */
  bool success() const {
    return success_;
  }

  /**
   * @brief Boolean conversion for use in if statements.
   */
  explicit operator bool() const {
    return success_;
  }

  /**
   * @brief Get the error. Only valid if success() is false.
   */
  const E& error() const {
    return error_;
  }

private:
  explicit Result(bool /*success*/) : success_(true) {
  }
  explicit Result(E error) : success_(false), error_(std::move(error)) {
  }

  bool success_ = false;
  E error_;
};

}  // namespace widgeteer
