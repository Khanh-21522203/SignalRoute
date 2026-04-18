#pragma once

/**
 * SignalRoute — Result<T, E> error handling type
 *
 * A lightweight Result type for explicit error handling without exceptions.
 * Used on hot paths (ingestion, query) where exception overhead is unacceptable.
 *
 * Usage:
 *   Result<DeviceState, std::string> result = get_device("abc");
 *   if (result.ok()) {
 *       auto state = result.value();
 *   } else {
 *       log_error(result.error());
 *   }
 */

#include <variant>
#include <string>
#include <stdexcept>

namespace signalroute {

template<typename T, typename E = std::string>
class Result {
public:
    /// Construct a success result.
    static Result ok(T value) {
        return Result(std::move(value));
    }

    /// Construct an error result.
    static Result err(E error) {
        return Result(std::move(error));
    }

    /// Check if the result is successful.
    [[nodiscard]] bool is_ok() const {
        return std::holds_alternative<T>(data_);
    }

    /// Check if the result is an error.
    [[nodiscard]] bool is_err() const {
        return std::holds_alternative<E>(data_);
    }

    /// Get the success value. Throws if result is an error.
    [[nodiscard]] const T& value() const& {
        if (is_err()) {
            throw std::runtime_error("Result::value() called on error result");
        }
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        if (is_err()) {
            throw std::runtime_error("Result::value() called on error result");
        }
        return std::get<T>(std::move(data_));
    }

    /// Get the error. Throws if result is ok.
    [[nodiscard]] const E& error() const& {
        if (is_ok()) {
            throw std::runtime_error("Result::error() called on ok result");
        }
        return std::get<E>(data_);
    }

    /// Get value or a default.
    [[nodiscard]] T value_or(T default_value) const {
        if (is_ok()) return std::get<T>(data_);
        return default_value;
    }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(E error) : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

/**
 * Specialization for void success type.
 * Used for operations that either succeed (no value) or fail with an error.
 */
template<typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true); }
    static Result err(E error) { return Result(std::move(error)); }

    [[nodiscard]] bool is_ok() const { return success_; }
    [[nodiscard]] bool is_err() const { return !success_; }

    [[nodiscard]] const E& error() const {
        if (is_ok()) throw std::runtime_error("Result::error() called on ok result");
        return error_;
    }

private:
    explicit Result(bool) : success_(true) {}
    explicit Result(E error) : success_(false), error_(std::move(error)) {}

    bool success_ = false;
    E    error_;
};

} // namespace signalroute
