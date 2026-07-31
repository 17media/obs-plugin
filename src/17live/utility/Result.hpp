#pragma once

#include <optional>
#include <string>
#include <utility>

struct ResultError {
    std::string code;
    std::string message;
    bool retryable = false;
    std::string detail;
};

template<typename T>
class Result {
   public:
    static Result Ok(T value) {
        return Result(true, std::move(value), ResultError{});
    }

    static Result Err(ResultError error) {
        return Result(false, std::nullopt, std::move(error));
    }

    bool ok() const {
        return ok_;
    }

    explicit operator bool() const {
        return ok_;
    }

    const T& value() const {
        return *value_;
    }

    T& value() {
        return *value_;
    }

    T takeValue() {
        return std::move(*value_);
    }

    const ResultError& error() const {
        return error_;
    }

   private:
    Result(bool ok, std::optional<T> value, ResultError error)
        : ok_(ok), value_(std::move(value)), error_(std::move(error)) {}

    bool ok_ = false;
    std::optional<T> value_;
    ResultError error_;
};

template<>
class Result<void> {
   public:
    static Result Ok() {
        return Result(true, ResultError{});
    }

    static Result Err(ResultError error) {
        return Result(false, std::move(error));
    }

    bool ok() const {
        return ok_;
    }

    explicit operator bool() const {
        return ok_;
    }

    const ResultError& error() const {
        return error_;
    }

   private:
    Result(bool ok, ResultError error) : ok_(ok), error_(std::move(error)) {}

    bool ok_ = false;
    ResultError error_;
};

