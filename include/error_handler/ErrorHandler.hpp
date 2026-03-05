/**
 * @file ErrorHandler.hpp
 * @brief A monadic error handling wrapper for safety-critical applications.
 *
 * This file provides a type-safe, monadic error handling mechanism built on
 * top of C++23's std::expected. The ErrorHandler class supports functional
 * composition through monadic operations (and_then, transform, or_else) and
 * includes automatic error logging at the point of error creation.
 *
 * Key Features:
 * - Type-safe error handling with enum-based error codes
 * - Automatic error logging with source location tracking
 * - Monadic operations for functional error handling pipelines
 * - Full move semantics support for zero-copy error propagation
 * - [[nodiscard]] attribute to prevent ignoring error conditions
 * - Safety-critical design with no exceptions in hot paths
 *
 * @note This implementation requires C++23 or later
 * @warning Error logging may introduce overhead in high-frequency paths
 *
 * Example usage:
 * @code
 * enum class FileError { NotFound, PermissionDenied, IOError };
 *
 * template <>
 * struct ErrorTraits<FileError> {
 *     static constexpr std::string_view name(FileError e) {
 *         switch (e) {
 *             case FileError::NotFound: return "File Not Found";
 *             case FileError::PermissionDenied: return "Permission Denied";
 *             case FileError::IOError: return "IO Error";
 *         }
 *     }
 * };
 *
 * ErrorHandler<std::string, FileError> read_file(const std::string& path) {
 *     if (!file_exists(path)) {
 *         return ErrorHandler<std::string, FileError>(
 *             FileError::NotFound, "File not found");
 *     }
 *     return ErrorHandler<std::string, FileError>(file_contents);
 * }
 *
 * auto result = read_file("config.txt")
 *     .transform([](const std::string& content) { return parse(content); })
 *     .and_then([](Config cfg) { return validate(cfg); })
 *     .or_else([](FileError e) { return use_default_config(); });
 * @endcode
 */
#pragma once

#include <expected>
#include <logging/Logger.hpp>
#include <source_location>
#include <string>
#include <string_view>
#include <variant>

/**
 * @brief Concept requiring the error type to be an enumeration.
 *
 * This concept ensures that only enum types are used as error types,
 * providing type safety and enabling compile-time validation.
 *
 * @tparam T The type to check
 */
template <typename T>
concept ErrorEnum = std::is_enum_v<T>;

/**
 * @brief Traits for converting error enum values to human-readable names.
 *
 * Users must specialize this struct for each error enum type to provide
 * meaningful error messages. The specialization should implement a static
 * name() function that returns a string_view describing the error.
 *
 * @tparam E The error enum type (must satisfy ErrorEnum concept)
 *
 * @note Specializations should be constexpr for compile-time evaluation
 * @warning Default implementation returns "Unknown Error" - always specialize!
 */
template <ErrorEnum E>
struct ErrorTraits {
    /**
     * @brief Returns a human-readable name for the given error value.
     * @param e The error enum value
     * @return String view containing the error description
     */
    static constexpr std::string_view name(E e) { return "Unknown Error"; }
};

/**
 * @brief A monadic error handling wrapper with automatic logging.
 *
 * ErrorHandler wraps a value of type T or an error of type E, providing
 * monadic operations for functional error handling. All error
 * constructors automatically log the error with source location information.
 *
 * The class provides four reference-qualified overloads for each monadic
 * operation (&, const&, &&, const&&) to support efficient move semantics
 * and enable method chaining on temporary objects.
 *
 * @tparam T The success value type
 * @tparam E The error enum type (must satisfy ErrorEnum concept)
 *
 * @note This class is marked [[nodiscard]] to prevent ignoring results
 * @note All monadic operations are noexcept-safe unless the user function
 * throws
 */
template <typename T, ErrorEnum E>
class [[nodiscard]] ErrorHandler {
   public:
    /// @brief Type alias for the success value type
    using value_type = T;

    /// @brief Type alias for the error enum type
    using error_type = E;

    /**
     * @brief Constructs an ErrorHandler with a success value.
     *
     * Creates an ErrorHandler containing the given value, indicating
     * successful operation. The value is moved into internal storage.
     *
     * @param value The success value to store
     *
     * @note No logging occurs for successful construction
     */
    explicit ErrorHandler(T value) : internal_result(std::move(value)) {}

    /**
     * @brief Constructs an ErrorHandler with an error and logs it.
     *
     * Creates an ErrorHandler containing the given error and automatically
     * logs the error with source location information. The error message
     * combines the error name from ErrorTraits and the custom message.
     *
     * @param error The error enum value
     * @param custom_msg Optional custom error message for additional context
     * @param loc Source location where the error occurred (auto-captured)
     *
     * @note Error is logged at LOGGER_ERROR level
     * @warning Ensure ErrorTraits is specialized for error type E
     */
    explicit ErrorHandler(
        E error, std::string_view custom_msg = "",
        std::source_location loc = std::source_location::current())
        : internal_result(std::unexpected(error)) {
        std::string_view error_name = ErrorTraits<E>::name(error);
        LOGGER_ERROR("Error at %s: %s - %s", loc.function_name(),
                     error_name.data(), custom_msg.data());
    }

    /**
     * @brief Constructs an ErrorHandler from std::unexpected and logs it.
     *
     * Creates an ErrorHandler from an std::unexpected wrapper and logs
     * the contained error with source location information.
     *
     * @param unex The std::unexpected wrapper containing the error
     * @param loc Source location where the error occurred (auto-captured)
     *
     * @note Error is logged at LOGGER_ERROR level
     */
    explicit ErrorHandler(
        std::unexpected<E> unex,
        std::source_location loc = std::source_location::current())
        : internal_result(std::move(unex)) {
        std::string_view error_name = ErrorTraits<E>::name(unex.error());
        LOGGER_ERROR("Error at %s: %s", loc.function_name(), error_name.data());
    }

    /**
     * @brief Checks whether the ErrorHandler contains a value.
     *
     * @return true if contains a value, false if contains an error
     * @note This function is marked [[nodiscard]] and noexcept
     */
    [[nodiscard]] bool has_value() const noexcept {
        return internal_result.has_value();
    }

    /**
     * @brief Checks whether the ErrorHandler contains a value (conversion
     * operator).
     *
     * Allows ErrorHandler to be used in boolean contexts (if statements, etc.).
     *
     * @return true if contains a value, false if contains an error
     * @note Explicit conversion prevents accidental implicit conversions
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return internal_result.has_value();
    }

    /**
     * @brief Accesses the contained value (lvalue reference).
     *
     * Returns a reference to the contained value. The value remains in
     * the ErrorHandler and can be modified through the reference.
     *
     * @return Reference to the contained value
     * @throws std::bad_expected_access if ErrorHandler contains an error
     */
    T& value() & { return internal_result.value(); }

    /**
     * @brief Accesses the contained value (const lvalue reference).
     *
     * Returns a const reference to the contained value.
     *
     * @return Const reference to the contained value
     * @throws std::bad_expected_access if ErrorHandler contains an error
     */
    const T& value() const& { return internal_result.value(); }

    /**
     * @brief Accesses the contained value (rvalue reference).
     *
     * Moves the value out of the ErrorHandler. After this call, the
     * ErrorHandler is in a valid but unspecified state.
     *
     * @return Rvalue reference to the contained value
     * @throws std::bad_expected_access if ErrorHandler contains an error
     * @note Enables efficient extraction from temporary ErrorHandler objects
     */
    T&& value() && { return std::move(internal_result.value()); }

    /**
     * @brief Accesses the contained value (const rvalue reference).
     *
     * Moves the value out of a const temporary ErrorHandler.
     *
     * @return Const rvalue reference to the contained value
     * @throws std::bad_expected_access if ErrorHandler contains an error
     */
    const T&& value() const&& { return std::move(internal_result.value()); }

    /**
     * @brief Accesses the contained error.
     *
     * Returns the error enum value. Behavior is undefined if
     * ErrorHandler contains a value instead of an error.
     *
     * @return The error enum value
     * @note Only call this after verifying has_value() == false
     */
    E error() const { return internal_result.error(); }

    /**
     * @brief Applies a function if value exists, propagates error otherwise
     * (lvalue).
     *
     * Monadic bind operation (also known as flatMap). If the ErrorHandler
     * contains a value, applies function f to it and returns the result.
     * If it contains an error, propagates the error without calling f.
     *
     * @tparam F Callable type that takes T& and returns ErrorHandler<U, E>
     * @param f Function to apply to the value
     * @return Result of f(value) if has_value(), otherwise ErrorHandler with
     * error
     *
     * @note Function f must return an ErrorHandler (or compatible type)
     * @note Called on lvalue ErrorHandler objects
     */
    template <typename F>
    auto and_then(F&& f) & {
        using Result = std::invoke_result_t<F, T&>;
        if (internal_result.has_value()) {
            return std::forward<F>(f)(internal_result.value());
        }
        return Result(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Applies a function if value exists, propagates error otherwise
     * (const lvalue).
     *
     * Const lvalue overload of and_then for const ErrorHandler objects.
     *
     * @tparam F Callable type that takes const T& and returns ErrorHandler<U,
     * E>
     * @param f Function to apply to the value
     * @return Result of f(value) if has_value(), otherwise ErrorHandler with
     * error
     *
     * @note Called on const lvalue ErrorHandler objects
     */
    template <typename F>
    auto and_then(F&& f) const& {
        using Result = std::invoke_result_t<F, const T&>;
        if (internal_result.has_value()) {
            return std::forward<F>(f)(internal_result.value());
        }
        return Result(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Applies a function if value exists, propagates error otherwise
     * (rvalue).
     *
     * Rvalue overload of and_then for temporary ErrorHandler objects.
     * Moves the value to function f for efficient chaining.
     *
     * @tparam F Callable type that takes T&& and returns ErrorHandler<U, E>
     * @param f Function to apply to the value
     * @return Result of f(value) if has_value(), otherwise ErrorHandler with
     * error
     *
     * @note Critical for method chaining: result().and_then(...).and_then(...)
     * @note Called on rvalue/temporary ErrorHandler objects
     */
    template <typename F>
    auto and_then(F&& f) && {
        using Result = std::invoke_result_t<F, T&&>;
        if (internal_result.has_value()) {
            return std::forward<F>(f)(std::move(internal_result.value()));
        }
        return Result(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Applies a function if value exists, propagates error otherwise
     * (const rvalue).
     *
     * Const rvalue overload of and_then for const temporary ErrorHandler
     * objects.
     *
     * @tparam F Callable type that takes const T&& and returns ErrorHandler<U,
     * E>
     * @param f Function to apply to the value
     * @return Result of f(value) if has_value(), otherwise ErrorHandler with
     * error
     *
     * @note Called on const rvalue ErrorHandler objects
     */
    template <typename F>
    auto and_then(F&& f) const&& {
        using Result = std::invoke_result_t<F, const T&&>;
        if (internal_result.has_value()) {
            return std::forward<F>(f)(std::move(internal_result.value()));
        }
        return Result(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Transforms the value if it exists, propagates error otherwise
     * (lvalue).
     *
     * Monadic map operation. If the ErrorHandler contains a value, applies
     * function f to transform it to a new value of type U. If it contains
     * an error, propagates the error without calling f.
     *
     * @tparam F Callable type that takes T& and returns U
     * @param f Function to transform the value
     * @return ErrorHandler<U, E> containing transformed value or original error
     *
     * @note Unlike and_then, f returns a plain value (not ErrorHandler)
     * @note Called on lvalue ErrorHandler objects
     */
    template <typename F>
    auto transform(F&& f) & {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<U, E>(
                std::forward<F>(f)(internal_result.value()));
        }
        return ErrorHandler<U, E>(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Transforms the value if it exists, propagates error otherwise
     * (const lvalue).
     *
     * Const lvalue overload of transform for const ErrorHandler objects.
     *
     * @tparam F Callable type that takes const T& and returns U
     * @param f Function to transform the value
     * @return ErrorHandler<U, E> containing transformed value or original error
     *
     * @note Called on const lvalue ErrorHandler objects
     */
    template <typename F>
    auto transform(F&& f) const& {
        using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<U, E>(
                std::forward<F>(f)(internal_result.value()));
        }
        return ErrorHandler<U, E>(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Transforms the value if it exists, propagates error otherwise
     * (rvalue).
     *
     * Rvalue overload of transform for temporary ErrorHandler objects.
     * Moves the value to function f for efficient transformation chains.
     *
     * @tparam F Callable type that takes T&& and returns U
     * @param f Function to transform the value
     * @return ErrorHandler<U, E> containing transformed value or original error
     *
     * @note Critical for method chaining with move semantics
     * @note Called on rvalue/temporary ErrorHandler objects
     */
    template <typename F>
    auto transform(F&& f) && {
        using U = std::remove_cv_t<std::invoke_result_t<F, T&&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<U, E>(
                std::forward<F>(f)(std::move(internal_result.value())));
        }
        return ErrorHandler<U, E>(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Transforms the value if it exists, propagates error otherwise
     * (const rvalue).
     *
     * Const rvalue overload of transform for const temporary ErrorHandler
     * objects.
     *
     * @tparam F Callable type that takes const T&& and returns U
     * @param f Function to transform the value
     * @return ErrorHandler<U, E> containing transformed value or original error
     *
     * @note Called on const rvalue ErrorHandler objects
     */
    template <typename F>
    auto transform(F&& f) const&& {
        using U = std::remove_cv_t<std::invoke_result_t<F, const T&&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<U, E>(
                std::forward<F>(f)(std::move(internal_result.value())));
        }
        return ErrorHandler<U, E>(std::unexpected<E>(internal_result.error()));
    }

    /**
     * @brief Applies error recovery function if error exists, propagates value
     * otherwise (lvalue).
     *
     * Monadic error handling operation. If the ErrorHandler contains an error,
     * applies function f to attempt recovery. If it contains a value, returns
     * the value unchanged.
     *
     * @tparam F Callable type that takes E& and returns ErrorHandler<T, E>
     * @param f Function to recover from error
     * @return Result of f(error) if has error, otherwise ErrorHandler with
     * value
     *
     * @note Enables error recovery and fallback mechanisms
     * @note Called on lvalue ErrorHandler objects
     */
    template <typename F>
    [[nodiscard]] auto or_else(F&& f) & {
        using Result = std::invoke_result_t<F, E&>;
        if (!internal_result.has_value()) {
            return std::forward<F>(f)(internal_result.error());
        }
        return Result(internal_result.value());
    }

    /**
     * @brief Applies error recovery function if error exists, propagates value
     * otherwise (const lvalue).
     *
     * Const lvalue overload of or_else for const ErrorHandler objects.
     *
     * @tparam F Callable type that takes const E& and returns ErrorHandler<T,
     * E>
     * @param f Function to recover from error
     * @return Result of f(error) if has error, otherwise ErrorHandler with
     * value
     *
     * @note Called on const lvalue ErrorHandler objects
     */
    template <typename F>
    [[nodiscard]] auto or_else(F&& f) const& {
        using Result = std::invoke_result_t<F, const E&>;
        if (!internal_result.has_value()) {
            return std::forward<F>(f)(internal_result.error());
        }
        return Result(internal_result.value());
    }

    /**
     * @brief Applies error recovery function if error exists, propagates value
     * otherwise (rvalue).
     *
     * Rvalue overload of or_else for temporary ErrorHandler objects.
     * Moves both value and error for efficient recovery chains.
     *
     * @tparam F Callable type that takes E&& and returns ErrorHandler<T, E>
     * @param f Function to recover from error
     * @return Result of f(error) if has error, otherwise ErrorHandler with
     * value
     *
     * @note Called on rvalue/temporary ErrorHandler objects
     */
    template <typename F>
    [[nodiscard]] auto or_else(F&& f) && {
        using Result = std::invoke_result_t<F, E&&>;
        if (!internal_result.has_value()) {
            return std::forward<F>(f)(std::move(internal_result.error()));
        }
        return Result(std::move(internal_result.value()));
    }

    /**
     * @brief Applies error recovery function if error exists, propagates value
     * otherwise (const rvalue).
     *
     * Const rvalue overload of or_else for const temporary ErrorHandler
     * objects.
     *
     * @tparam F Callable type that takes const E&& and returns ErrorHandler<T,
     * E>
     * @param f Function to recover from error
     * @return Result of f(error) if has error, otherwise ErrorHandler with
     * value
     *
     * @note Called on const rvalue ErrorHandler objects
     */
    template <typename F>
    [[nodiscard]] auto or_else(F&& f) const&& {
        using Result = std::invoke_result_t<F, const E&&>;
        if (!internal_result.has_value()) {
            return std::forward<F>(f)(std::move(internal_result.error()));
        }
        return Result(std::move(internal_result.value()));
    }

    /**
     * @brief Transforms the error type while preserving the value (lvalue).
     *
     * Converts an ErrorHandler<T, E> to ErrorHandler<T, G> by transforming
     * the error enum type. If the ErrorHandler contains a value, the value
     * is preserved unchanged. If it contains an error, function f transforms
     * the error from type E to type G.
     *
     * @tparam F Callable type that takes E& and returns G
     * @param f Function to transform the error type
     * @return ErrorHandler<T, G> with same value or transformed error
     *
     * @note Useful for error domain conversions (e.g., FileError to
     * NetworkError)
     * @note Called on lvalue ErrorHandler objects
     */
    template <typename F>
    auto transform_error(F&& f) & {
        using G = std::remove_cv_t<std::invoke_result_t<F, E&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<T, G>(internal_result.value());
        }
        return ErrorHandler<T, G>(std::forward<F>(f)(internal_result.error()),
                                  "Error transformed");
    }

    /**
     * @brief Transforms the error type while preserving the value (const
     * lvalue).
     *
     * Const lvalue overload of transform_error for const ErrorHandler objects.
     *
     * @tparam F Callable type that takes const E& and returns G
     * @param f Function to transform the error type
     * @return ErrorHandler<T, G> with same value or transformed error
     *
     * @note Called on const lvalue ErrorHandler objects
     */
    template <typename F>
    auto transform_error(F&& f) const& {
        using G = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<T, G>(internal_result.value());
        }
        return ErrorHandler<T, G>(std::forward<F>(f)(internal_result.error()),
                                  "Error transformed");
    }

    /**
     * @brief Transforms the error type while preserving the value (rvalue).
     *
     * Rvalue overload of transform_error for temporary ErrorHandler objects.
     * Moves both value and error during transformation.
     *
     * @tparam F Callable type that takes E&& and returns G
     * @param f Function to transform the error type
     * @return ErrorHandler<T, G> with same value or transformed error
     *
     * @note Called on rvalue/temporary ErrorHandler objects
     */
    template <typename F>
    auto transform_error(F&& f) && {
        using G = std::remove_cv_t<std::invoke_result_t<F, E&&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<T, G>(std::move(internal_result.value()));
        }
        return ErrorHandler<T, G>(
            std::forward<F>(f)(std::move(internal_result.error())),
            "Error transformed");
    }

    /**
     * @brief Transforms the error type while preserving the value (const
     * rvalue).
     *
     * Const rvalue overload of transform_error for const temporary ErrorHandler
     * objects.
     *
     * @tparam F Callable type that takes const E&& and returns G
     * @param f Function to transform the error type
     * @return ErrorHandler<T, G> with same value or transformed error
     *
     * @note Called on const rvalue ErrorHandler objects
     */
    template <typename F>
    auto transform_error(F&& f) const&& {
        using G = std::remove_cv_t<std::invoke_result_t<F, const E&&>>;
        if (internal_result.has_value()) {
            return ErrorHandler<T, G>(std::move(internal_result.value()));
        }
        return ErrorHandler<T, G>(
            std::forward<F>(f)(std::move(internal_result.error())),
            "Error transformed");
    }

    /**
     * @brief Returns the value if present, otherwise returns default value
     * (const lvalue).
     *
     * Extracts the value from the ErrorHandler if it exists, otherwise
     * returns the provided default value. This is a safe way to extract
     * values without throwing exceptions.
     *
     * @tparam U Type of the default value (usually deduced)
     * @param default_value Value to return if ErrorHandler contains an error
     * @return The contained value or the default value
     *
     * @note Returns by value, so a copy/move of T is made
     * @note Called on const lvalue ErrorHandler objects
     */
    template <class U>
    T value_or(U&& default_value) const& {
        return internal_result.value_or(std::forward<U>(default_value));
    }

    /**
     * @brief Returns the value if present, otherwise returns default value
     * (rvalue).
     *
     * Rvalue overload that moves the value out of temporary ErrorHandler
     * objects. This avoids unnecessary copies when extracting from temporaries.
     *
     * @tparam U Type of the default value (usually deduced)
     * @param default_value Value to return if ErrorHandler contains an error
     * @return The contained value (moved) or the default value
     *
     * @note Critical for efficient extraction: get_value().value_or(default)
     * @note Called on rvalue/temporary ErrorHandler objects
     */
    template <class U>
    T value_or(U&& default_value) && {
        return std::move(internal_result)
            .value_or(std::forward<U>(default_value));
    }

   private:
    /// @brief Internal storage using std::expected from C++23
    std::expected<T, E> internal_result{};
};