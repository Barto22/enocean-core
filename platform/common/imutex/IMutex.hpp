/**
 * @file IMutex.hpp
 * @brief Platform-agnostic mutex interface for cross-platform synchronization.
 *
 * This file defines the base mutex interface (`IMutex`) using the CRTP pattern
 * and concepts for validating platform-specific implementations. The interface
 * provides a modern C++ abstraction for mutual exclusion locks with timeout
 * support across different platforms (POSIX, Zephyr).
 *
 * - `IMutex` is a CRTP base class for compile-time polymorphic mutex services.
 * - `MutexImplementation` concept validates conforming implementations at
 *   compile time.
 *
 * Usage:
 *   - Inherit from `IMutex<YourClass, DurationType>` and implement do_lock(),
 *     do_unlock() and do_init() for your platform.
 *   - Use the interface for portable mutex operations in your application.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <concepts>
#include <cstring>
#include <gsl/gsl>
#include <string_view>
#include <type_traits>
#include <variant>

#include "MutexError.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief CRTP-based mutex interface providing lock/unlock operations with
 * timeout support
 *
 * This class template uses the Curiously Recurring Template Pattern (CRTP) to
 * provide a compile-time polymorphic mutex interface. Derived classes must
 * implement do_lock() and do_unlock() methods to define platform-specific mutex
 * behavior.
 *
 * @tparam Derived The derived class implementing the actual mutex functionality
 * @tparam DurationType The type representing timeout duration (e.g.,
 * std::chrono::milliseconds)
 */
template <typename Derived, typename DurationType>
class IMutex {
   public:
    /**
     * @brief Destructor for safe polymorphic deletion.
     */
    ~IMutex() noexcept = default;

    /**
     * @brief Deleted default constructor.
     */
    IMutex() = delete;

    /**
     * @brief Deleted copy constructor.
     */
    IMutex(const IMutex&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    IMutex& operator=(const IMutex&) = delete;

    /**
     * @brief Deleted move constructor.
     */
    IMutex(IMutex&&) = delete;

    /**
     * @brief Deleted move assignment operator.
     */
    IMutex& operator=(IMutex&&) = delete;

    /**
     * @brief Construct a mutex object with a name.
     * @param name Mutex name string.
     */
    template <size_t N>
    explicit consteval IMutex(const char (&name)[N]) noexcept {
        static_assert(N <= mutex_name_size,
                      "Mutex name exceeds maximum length");

        std::copy_n(name, N - 1, mutex_name.begin());

        mutex_name[N - 1] = '\0';
    }

    /**
     * @brief Constructs a mutex with a runtime-provided name.
     *
     * Initializes the mutex with a name from a pointer parameter.
     * The name is truncated to fit within the internal buffer (31 characters
     * plus null terminator). This constructor is used when the name cannot be
     * determined at compile time.
     *
     * @param name Non-null pointer to the mutex name string
     */
    explicit constexpr IMutex(gsl::not_null<const char*> name) noexcept {
        const size_t len = std::char_traits<char>::length(name.get());
        const size_t copy_len = std::min(len, mutex_name_size - 1);
        std::copy_n(name.get(), copy_len, mutex_name.begin());
        mutex_name[copy_len] = '\0';
    }

    /**
     * @brief Initializes the platform-specific mutex resources.
     *
     * Delegates to the derived class's do_init() implementation to perform
     * the actual mutex initialization. This method should be called after
     * construction to create and configure the underlying platform mutex
     * object (e.g., pthread_mutex_init for POSIX, k_mutex_init for Zephyr).
     *
     * @return ErrorHandler with success (std::monostate) or MutexError
     * @note Must be called before using lock() or unlock() operations.
     * @note Some platforms may not require explicit initialization (e.g., POSIX
     *       with static initialization, Zephyr with k_mutex_init in
     * constructor).
     */
    [[nodiscard]] auto init() -> ErrorHandler<std::monostate, MutexError> {
        if (!initialized_) {
            const auto result{static_cast<Derived*>(this)->do_init()};
            if (result) {
                initialized_ = true;
            }
            return result;
        } else {
            return ErrorHandler<std::monostate, MutexError>(
                MutexError::AlreadyInitialized, "Mutex already initialized");
        }
    }

    /**
     * @brief Attempts to acquire the mutex with a specified timeout
     *
     * Delegates to the derived class's do_lock() implementation to perform the
     * actual locking operation. This allows for platform-specific timeout
     * handling.
     *
     * @param timeout Maximum duration to wait for mutex acquisition
     * @return ErrorHandler with success (std::monostate) or MutexError
     */
    [[nodiscard]] auto lock(DurationType timeout)
        -> ErrorHandler<std::monostate, MutexError> {
        if (!initialized_)
            return ErrorHandler<std::monostate, MutexError>(
                MutexError::NotInitialized, "Mutex not initialized");
        return static_cast<Derived*>(this)->do_lock(timeout);
    }

    /**
     * @brief Releases the previously acquired mutex
     *
     * Delegates to the derived class's do_unlock() implementation to perform
     * the actual unlock operation. Must only be called by the thread that
     * successfully locked the mutex.
     */
    void unlock() {
        if (!initialized_) return;  // Fail-safe: prevent use before init
        static_cast<Derived*>(this)->do_unlock();
    }

   protected:
    /**
     * @brief Get the mutex name.
     * @return Pointer to the mutex name string.
     */
    auto get_mutex_name() -> char* { return mutex_name.data(); }

    /**
     * @brief Check if mutex is initialized.
     * @return True if initialized, false otherwise.
     */
    auto is_initialized() const -> bool { return initialized_; }

    /**
     * @brief Set initialization state.
     */
    void set_initialized(bool state) { initialized_ = state; }

   private:
    static constexpr std::size_t mutex_name_size = 32;
    /** @brief Name of the mutex for identification purposes */
    std::array<char, mutex_name_size> mutex_name{};
    /** @brief Initialization state to prevent use before init */
    bool initialized_{false};
};

/**
 * @brief Concept defining the requirements for a type to be used as a mutex
 *
 * A type satisfies IsMutex if it provides:
 * - do_init() method returning ErrorHandler<std::monostate, MutexError>
 * - do_lock(Duration) method returning ErrorHandler<std::monostate, MutexError>
 * - do_unlock() method returning void
 *
 * @tparam T The type to check for mutex compliance
 * @tparam Duration The duration type for timeout specification
 */
template <typename T, typename Duration>
concept MutexImplementation = requires(T m, Duration d) {
    { m.do_init() } -> std::same_as<ErrorHandler<std::monostate, MutexError>>;
    { m.do_lock(d) } -> std::same_as<ErrorHandler<std::monostate, MutexError>>;
    { m.do_unlock() } -> std::same_as<void>;
    requires !std::is_abstract_v<T>;
};

/**
 * @brief RAII-style mutex lock guard for automatic mutex lifetime management
 *
 * IMutexLocker provides a scoped lock mechanism that automatically acquires
 * a mutex on construction and releases it on destruction, following the
 * Resource Acquisition Is Initialization (RAII) idiom. This ensures
 * exception-safe mutex handling and prevents common deadlock scenarios caused
 * by forgetting to unlock mutexes.
 *
 * The locker attempts to acquire the mutex with a specified timeout during
 * construction. If successful, the mutex is automatically unlocked when the
 * locker goes out of scope. The lock status can be queried using is_locked()
 * or the bool conversion operator.
 *
 * @tparam Duration The duration type for lock timeout specification (e.g.,
 *         std::chrono::milliseconds)
 * @tparam T The mutex implementation type that satisfies MutexImplementation
 *         concept
 *
 * @note This class is non-copyable and non-movable to prevent accidental
 *       multiple unlock operations
 * @note The mutex must outlive the IMutexLocker instance
 */
template <typename Duration, MutexImplementation<Duration> T>
class IMutexLocker {
   public:
    /**
     * @brief Constructs a mutex locker and attempts to acquire the mutex
     *
     * Attempts to lock the provided mutex with the specified timeout. If the
     * lock is successfully acquired, it will be automatically released when
     * this IMutexLocker object is destroyed. The success of the lock operation
     * can be checked using is_locked() or the bool conversion operator.
     *
     * @param mutex Reference to the mutex to lock (must outlive this locker)
     * @param timeout Timeout duration for the lock attempt (platform-specific
     *                type: timespec* for POSIX, k_timeout_t
     *                for Zephyr)
     * @note This constructor is marked explicit to prevent accidental implicit
     *       conversions
     */
    explicit IMutexLocker(T& mutex, Duration timeout)
        : mutex_(mutex), locked_(false) {
        auto lock_result{mutex_.lock(timeout)};
        locked_ = lock_result.has_value();
    }

    /**
     * @brief Destructor that automatically releases the mutex if locked
     *
     * If the mutex was successfully locked during construction, it will be
     * automatically unlocked here. This ensures exception-safe RAII behavior.
     */
    ~IMutexLocker() noexcept {
        if (locked_) {
            mutex_.unlock();
        }
    }

    /**
     * @brief Bool conversion operator to check if the mutex was successfully
     * locked
     *
     * Allows the locker to be used in boolean contexts to check lock status.
     * This is equivalent to calling is_locked().
     *
     * @return true if the mutex was successfully acquired, false otherwise
     * @note Marked explicit to prevent unintended implicit conversions
     *
     * Example:
     * @code
     * IMutexLocker locker(my_mutex, timeout);
     * if (locker) {
     *     // Mutex is locked
     * }
     * @endcode
     */
    explicit operator bool() const noexcept { return locked_; }

    /**
     * @brief Checks whether the mutex was successfully locked
     *
     * Query method to determine if the lock acquisition during construction
     * was successful.
     *
     * @return true if the mutex is currently held by this locker, false if
     *         the lock attempt failed or timed out
     * @note [[nodiscard]] attribute encourages checking the lock status
     */
    [[nodiscard]] bool is_locked() const noexcept { return locked_; }

    IMutexLocker() = delete;
    IMutexLocker(const IMutexLocker&) = delete;
    IMutexLocker& operator=(const IMutexLocker&) = delete;
    IMutexLocker(IMutexLocker&&) = delete;
    IMutexLocker& operator=(IMutexLocker&&) = delete;

   private:
    T& mutex_;
    bool locked_{false};
};

}  // namespace PlatformCore