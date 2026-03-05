
/**
 * @file IThread.hpp
 * @brief Platform-agnostic thread abstraction interfaces for cross-platform
 * projects.
 *
 * This file defines the core thread interface (`IThread`), thread runner
 * interface, and concepts for validating platform-specific implementations. The
 * interfaces provide a modern C++ abstraction for thread creation, management,
 * and execution without relying on the C++ Standard Library's threading
 * primitives.
 *
 * - `IThread` is an abstract base class for thread management.
 * - `IThreadRunner` is an interface for thread entry-point objects.
 * - `ThreadImplementation` and `ThreadRunnerImplementation` concepts check for
 * valid implementations.
 *
 * Usage:
 *   - Implement `IThread` for your platform (e.g., POSIX, Zephyr).
 *   - Use `IThreadRunner` for type-safe thread entry points.
 */
#pragma once

#include <array>
#include <cerrno>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <gsl/gsl>
#include <string_view>
#include <type_traits>
#include <variant>

#include "ThreadError.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief Structure to hold single thread parameter.
 */
struct ThreadParam1 {
    std::uintptr_t param{}; /**< Thread parameter storage (pointer-safe). */
};

/**
 * @brief Structure to hold two thread parameters.
 */
struct ThreadParam2 {
    std::uintptr_t param1{}; /**< First parameter storage (pointer-safe). */
    std::uintptr_t param2{}; /**< Second parameter storage (pointer-safe). */
};

/**
 * @brief Structure to hold three thread parameters.
 */
struct ThreadParam3 {
    std::uintptr_t param1{}; /**< First parameter storage (pointer-safe). */
    std::uintptr_t param2{}; /**< Second parameter storage (pointer-safe). */
    std::uintptr_t param3{}; /**< Third parameter storage (pointer-safe). */
};

/**
 * @brief Structure to hold an immediate value for platforms whose thread entry
 *        receives integral arguments directly.
 */
struct ThreadParamValue {
    std::uintptr_t value{}; /**< Value copied into the platform entry point. */
};

/**
 * @brief Converts a pointer to a portable thread parameter payload.
 * @param ptr Pointer to convert (may be nullptr).
 * @return Integral representation suitable for ThreadParam[1-3].
 */
inline auto ThreadParamFromPointer(const gsl::not_null<void*> ptr) noexcept
    -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(ptr.get());
}

/**
 * @brief Converts stored thread parameter payload back to a pointer.
 * @param value Integral representation produced by ThreadParamFromPointer.
 * @return Pointer value.
 */
inline auto ThreadParamToPointer(std::uintptr_t value) noexcept -> void* {
    return reinterpret_cast<void*>(value);
}

/**
 * @brief Variant type to hold different thread parameter structures.
 */
using ThreadParamVariant =
    std::variant<ThreadParam1, ThreadParam2, ThreadParam3, ThreadParamValue>;

/**
 * @brief Thread function type with one argument - POSIX type.
 */
using ThreadFunction1 = void* (*)(void*);

/**
 * @brief Thread function type with two arguments - ThreadX type.
 */
using ThreadFunction2 = void (*)(unsigned long);

/**
 * @brief Thread function type with three arguments - Zephyr type.
 */
using ThreadFunction3 = void (*)(void*, void*, void*);

/**
 * @brief Variant type to hold different thread function pointers.
 */
using ThreadFunctionVariant =
    std::variant<ThreadFunction1, ThreadFunction2, ThreadFunction3>;

/**
 * @class IThread
 * @brief Abstract base class for platform-agnostic thread management.
 *
 * Provides a modern C++ interface for thread creation, destruction, and
 * configuration. Does not depend on the C++ Standard Library's threading
 * primitives.
 *
 * @tparam Derived The derived class implementing the thread logic.
 * @tparam S stack pointer and size type.
 */
template <typename Derived, typename S>
class IThread {
   public:
    /**
     * @struct ThreadStackInfo
     * @brief Structure to hold thread stack information.
     */
    struct ThreadStackInfo {
        size_t stack_size; /**< Size of the thread stack in bytes. */
        S stack_pointer;   /**< Pointer to the thread stack. */
    };

    /**
     * @brief Destructor for safe polymorphic deletion.
     */
    ~IThread() noexcept = default;

    /**
     * @brief Deleted default constructor.
     */
    IThread() = delete;

    /**
     * @brief Deleted copy constructor.
     */
    IThread(const IThread&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    IThread& operator=(const IThread&) = delete;

    /**
     * @brief Deleted move constructor.
     */
    IThread(IThread&&) = delete;

    /**
     * @brief Deleted move assignment operator.
     */
    IThread& operator=(IThread&&) = delete;

    /**
     * @brief Construct a thread object with a name.
     * @param name Thread name string.
     */
    template <size_t N>
    explicit consteval IThread(const char (&name)[N]) noexcept {
        static_assert(N <= thread_name_size,
                      "Thread name exceeds maximum length");

        std::copy_n(name, N - 1, thread_name.begin());

        thread_name[N - 1] = '\0';
    }

    /**
     * @brief Constructs a thread with a runtime-provided name.
     *
     * Initializes the thread with a name from a pointer parameter.
     * The name is truncated to fit within the internal buffer (31 characters
     * plus null terminator). This constructor is used when the name cannot be
     * determined at compile time.
     *
     * @param name Non-null pointer to the thread name string
     */
    explicit constexpr IThread(gsl::not_null<const char*> name) noexcept {
        const size_t len = std::char_traits<char>::length(name.get());
        const size_t copy_len = std::min(len, thread_name_size - 1);
        std::copy_n(name.get(), copy_len, thread_name.begin());
        thread_name[copy_len] = '\0';
    }

    /**
     * @brief Create and start the thread.
     * @param threadFunction Thread entry function.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority.
     * @param params Additional arguments for thread entry.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    [[nodiscard]] auto create(ThreadFunctionVariant threadFunction,
                              const ThreadStackInfo& stackInfo, int priority,
                              ThreadParamVariant& params)
        -> ErrorHandler<std::monostate, ThreadError> {
        if (active) {
            return ErrorHandler<std::monostate, ThreadError>(
                ThreadError::AlreadyCreated, "Thread already created");
        }

        set_thread_info(stackInfo, priority);

        const auto result{static_cast<Derived*>(this)->os_create(
            threadFunction, stackInfo, priority, params)};
        if (result) {
            active = true;
        }
        return result;
    }

    /**
     * @brief Create and start the thread without params.
     * @param threadFunction Thread entry function.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    [[nodiscard]] auto create(ThreadFunctionVariant threadFunction,
                              const ThreadStackInfo& stackInfo, int priority)
        -> ErrorHandler<std::monostate, ThreadError> {
        if (active) {
            return ErrorHandler<std::monostate, ThreadError>(
                ThreadError::AlreadyCreated, "Thread already created");
        }

        set_thread_info(stackInfo, priority);

        const auto result{static_cast<Derived*>(this)->os_create(
            threadFunction, stackInfo, priority)};
        if (result) {
            active = true;
        }
        return result;
    }

    /**
     * @brief Destroy the thread and release resources.
     * @return ErrorHandler with success (std::monostate) or ThreadError
     */
    [[nodiscard]] auto destroy() -> ErrorHandler<std::monostate, ThreadError> {
        if (!active) {
            return ErrorHandler<std::monostate, ThreadError>(
                std::monostate{});  // Already destroyed, success
        }
        const auto result{static_cast<Derived*>(this)->os_destroy()};
        if (result) {
            active = false;
        }
        return result;
    }

    /**
     * @brief Check if the thread is currently active.
     * @return True if the thread is active, false otherwise.
     */
    auto is_active() const -> bool { return active; }

   protected:
    /**
     * @brief Set the thread active state.
     * @param is_active True to mark thread as active, false otherwise.
     */
    void set_active(const bool is_active) { active = is_active; }

    /**
     * @brief Get the thread name.
     * @return Pointer to the thread name string.
     */
    auto get_thread_name() -> char* { return thread_name.data(); }

    /**
     * @brief Set the stack information.
     * @param stackInfo Stack pointer and size information.
     */
    void set_stack_info(const ThreadStackInfo& stackInfo) {
        stack_info = stackInfo;
    }

    /**
     * @brief Get the stack information.
     * @return Reference to the stack information.
     */
    auto get_stack_info() const -> const ThreadStackInfo& { return stack_info; }

    /**
     * @brief Set the thread priority.
     * @param priority Thread priority value.
     */
    void set_thread_priority(const int priority) { thread_priority = priority; }

    /**
     * @brief Get the thread priority.
     * @return Thread priority value.
     */
    auto get_thread_priority() const -> int { return thread_priority; }

    /**
     * @brief Set both stack information and thread priority.
     * @param stackInfo Stack pointer and size information.
     * @param priority Thread priority value.
     */
    void set_thread_info(const ThreadStackInfo& stackInfo, const int priority) {
        set_stack_info(stackInfo);
        set_thread_priority(priority);
    }

   private:
    bool active = false;
    static constexpr std::size_t thread_name_size = 32;
    std::array<char, thread_name_size> thread_name{};
    ThreadStackInfo stack_info{};
    int thread_priority{};
};

/**
 * @brief Concept for valid thread implementations.
 * @tparam T Runner type.
 * @tparam S stack pointer and size type.
 */
template <typename T, typename S>
concept ThreadImplementation =
    requires(T& thread, ThreadFunctionVariant func,
             const typename T::ThreadStackInfo& stack, int priority,
             ThreadParamVariant& params) {
        {
            thread.os_create(func, stack, priority, params)
        } -> std::same_as<ErrorHandler<std::monostate, ThreadError>>;
        {
            thread.os_destroy()
        } -> std::same_as<ErrorHandler<std::monostate, ThreadError>>;
        requires !std::is_abstract_v<T>;
    };

/**
 * @class IThreadRunner
 * @brief Interface for thread entry-point objects.
 *
 * Provides a type-safe way to define thread entry logic with varying
 * parameters.
 *
 * @tparam Derived The derived class implementing the thread logic.
 */
template <typename Derived>
class IThreadRunner {
   public:
    /**
     * @brief Destructor for safe polymorphic deletion.
     */
    ~IThreadRunner() noexcept = default;

    /**
     * @brief Run the thread entry-point logic with provided parameters.
     * @param params Arguments for thread execution.
     */
    void run(ThreadParamVariant& params) {
        static_cast<Derived*>(this)->run_impl(params);
    }

   protected:
    IThreadRunner() noexcept = default;

    /**
     * @brief Deleted copy constructor.
     */
    IThreadRunner(const IThreadRunner&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    IThreadRunner& operator=(const IThreadRunner&) = delete;

    /**
     * @brief Deleted move constructor.
     */
    IThreadRunner(IThreadRunner&&) = delete;

    /**
     * @brief Deleted move assignment operator.
     */
    IThreadRunner& operator=(IThreadRunner&&) = delete;
};

/**
 * @brief Concept for valid thread runner implementations.
 * @tparam T Runner type.
 */
template <typename T>
concept ThreadRunnerImplementation =
    requires(T runner, ThreadParamVariant& params) {
        { runner.run_impl(params) } -> std::same_as<void>;
        requires !std::is_abstract_v<T>;
    };
}  // namespace PlatformCore