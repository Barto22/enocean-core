/**
 * @file ISemaphore.hpp
 * @brief Platform-agnostic semaphore interface for cross-platform
 * synchronization.
 *
 * This file defines the base semaphore interface (`ISemaphore`) and a concept
 * for validating platform-specific implementations. The interface provides a
 * modern C++ abstraction for counting semaphores using the CRTP (Curiously
 * Recurring Template Pattern) for compile-time polymorphism.
 *
 * - `ISemaphore` is a CRTP base class for compile-time polymorphic semaphore
 * services.
 * - `SemaphoreImplementation` concept validates conforming implementations at
 * compile time.
 *
 * Usage:
 *   - Inherit from `ISemaphore<YourClass, DurationType>` and implement
 * do_take() and do_give() for your platform (e.g., POSIX, Zephyr).
 *   - Use the interface for portable counting semaphore operations in your
 * application.
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

#include "SemaphoreError.hpp"

/**
 * @namespace PlatformCore
 * @brief Root namespace for platform abstraction interfaces and utilities.
 */
namespace PlatformCore {

/**
 * @brief CRTP-based interface for platform-agnostic counting semaphore
 * operations.
 *
 * Provides a compile-time polymorphic interface for semaphore synchronization
 * using the Curiously Recurring Template Pattern (CRTP). Derived classes must
 * implement do_take() and do_give() to define platform-specific semaphore
 * behavior.
 *
 * This design avoids runtime polymorphism overhead while providing a clean
 * abstraction for cross-platform semaphore functionality. The semaphore
 * supports named instances for debugging and tracing purposes.
 *
 * @tparam Derived The derived class implementing the actual semaphore
 * functionality
 * @tparam DurationType The type representing timeout duration (e.g.,
 * std::chrono::milliseconds, k_timeout_t, ULONG)
 */
template <typename Derived, typename DurationType>
class ISemaphore {
   public:
    /**
     * @brief Default destructor.
     * @note Non-virtual as CRTP doesn't require runtime polymorphism.
     */
    ~ISemaphore() noexcept = default;

    /**
     * @brief Default constructor is deleted - use named constructor.
     */
    ISemaphore() = delete;

    /**
     * @brief Copy constructor is deleted - semaphores are non-copyable.
     */
    ISemaphore(const ISemaphore&) = delete;

    /**
     * @brief Copy assignment is deleted - semaphores are non-copyable.
     */
    ISemaphore& operator=(const ISemaphore&) = delete;

    /**
     * @brief Constructs a named semaphore with specified initial count and
     * maximum limit.
     *
     * Initializes the semaphore with a human-readable name for debugging
     * purposes. The name is truncated to fit within the internal buffer (32
     * characters including null terminator). The initial_count and max_limit
     * parameters are passed to the derived class for platform-specific
     * initialization.
     *
     * @param name String identifier for the semaphore (used for
     * debugging/tracing)
     * @param initial_count The initial count value for the semaphore
     * @param max_limit The maximum count value the semaphore can reach
     * @note The name is stored in a fixed-size buffer and will be truncated if
     * too long
     */
    template <size_t N>
    explicit consteval ISemaphore(const char (&name)[N],
                                  unsigned int initial_count,
                                  unsigned int max_limit) noexcept
        : init_count{initial_count}, count_limit{max_limit} {
        static_assert(N <= sem_name_size,
                      "Semaphore name exceeds maximum length");

        std::copy_n(name, N - 1, sem_name.begin());

        sem_name[N - 1] = '\0';
    }

    /**
     * @brief Constructs a semaphore with a runtime-provided name.
     *
     * Initializes the semaphore with a name from a pointer parameter.
     * The name is truncated to fit within the internal buffer (31 characters
     * plus null terminator). This constructor is used when the name cannot be
     * determined at compile time.
     *
     * @param name Non-null pointer to the semaphore name string
     * @param initial_count The initial count value for the semaphore
     * @param max_limit The maximum count value the semaphore can reach
     */
    explicit constexpr ISemaphore(gsl::not_null<const char*> name,
                                  unsigned int initial_count,
                                  unsigned int max_limit) noexcept
        : init_count{initial_count}, count_limit{max_limit} {
        const size_t len = std::char_traits<char>::length(name.get());
        const size_t copy_len = std::min(len, sem_name_size - 1);
        std::copy_n(name.get(), copy_len, sem_name.begin());
        sem_name[copy_len] = '\0';
    }

    /**
     * @brief Initializes the platform-specific semaphore resources.
     *
     * Delegates to the derived class's do_init() implementation to perform
     * the actual semaphore initialization. This method should be called after
     * construction to create and configure the underlying platform semaphore
     * object (e.g., sem_init for POSIX, k_sem_init for Zephyr).
     *
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     * @note Must be called before using take() or give() operations.
     */
    [[nodiscard]] auto init() -> ErrorHandler<std::monostate, SemaphoreError> {
        if (!initialized_) {
            const auto result{static_cast<Derived*>(this)->do_init()};
            if (result) {
                initialized_ = true;
            }
            return result;
        } else {
            return ErrorHandler<std::monostate, SemaphoreError>(
                SemaphoreError::AlreadyInitialized,
                "Semaphore already initialized");
        }
    }

    /**
     * @brief Attempts to take (decrement) the semaphore with a specified
     * timeout.
     *
     * Delegates to the derived class's do_take() implementation to perform the
     * actual semaphore acquisition. This allows for platform-specific timeout
     * handling and semaphore semantics.
     *
     * If the semaphore count is greater than zero, it is decremented and the
     * operation succeeds immediately. If the count is zero, the calling thread
     * blocks until either the semaphore becomes available or the timeout
     * expires.
     *
     * @param timeout Maximum duration to wait for semaphore acquisition
     * @return ErrorHandler with success (std::monostate) or SemaphoreError
     */
    [[nodiscard]] auto take(DurationType timeout)
        -> ErrorHandler<std::monostate, SemaphoreError> {
        if (!initialized_)
            return ErrorHandler<std::monostate, SemaphoreError>(
                SemaphoreError::NotInitialized, "Semaphore not initialized");
        return static_cast<Derived*>(this)->do_take(timeout);
    }

    /**
     * @brief Gives (increments) the semaphore, potentially unblocking a waiting
     * thread.
     *
     * Delegates to the derived class's do_give() implementation to perform the
     * actual semaphore release. Increments the semaphore count, up to the
     * maximum limit. If threads are waiting, the highest priority thread is
     * unblocked.
     *
     * @note Giving a semaphore at its maximum limit may result in
     * platform-specific behavior (error, saturation, or no-op).
     */
    void give() {
        if (!initialized_) return;  // Fail-safe: prevent use before init
        static_cast<Derived*>(this)->do_give();
    }

   protected:
    /**
     * @brief Get the semaphore name.
     * @return Pointer to the semaphore name string.
     */
    auto get_semaphore_name() -> char* { return sem_name.data(); }

    /**
     * @brief Get the initial count value of the semaphore.
     * @return The initial count value.
     */
    auto get_initial_count() const -> unsigned int { return init_count; }

    /**
     * @brief Get the maximum count limit of the semaphore.
     * @return The maximum count limit.
     */
    auto get_count_limit() const -> unsigned int { return count_limit; }

    /**
     * @brief Check if semaphore is initialized.
     * @return True if initialized, false otherwise.
     */
    auto is_initialized() const -> bool { return initialized_; }

    /**
     * @brief Set initialization state.
     */
    void set_initialized(bool state) { initialized_ = state; }

   private:
    static constexpr std::size_t sem_name_size = 32;
    /**
     * @brief Fixed-size buffer storing the semaphore name for debugging.
     * @note Maximum name length is 31 characters plus null terminator.
     */
    std::array<char, sem_name_size> sem_name{};

    /**
     * @brief Initial count value for the semaphore.
     */
    unsigned int init_count{};

    /**
     * @brief Maximum count limit for the semaphore.
     */
    unsigned int count_limit{};

    /**
     * @brief Initialization state to prevent use before init.
     */
    bool initialized_{false};
};

/**
 * @brief Concept defining the requirements for a valid semaphore
 * implementation.
 *
 * A type satisfies SemaphoreImplementation if it provides:
 * - do_init() method returning ErrorHandler<std::monostate, SemaphoreError>
 * - do_take(Duration) method returning ErrorHandler<std::monostate,
 * SemaphoreError>
 * - do_give() method returning void
 * - Is a concrete (non-abstract) type
 *
 * @tparam T The type to check for semaphore implementation compliance
 * @tparam Duration The duration type for timeout specification
 */
template <typename T, typename Duration>
concept SemaphoreImplementation = requires(T s, Duration d) {
    {
        s.do_init()
    } -> std::same_as<ErrorHandler<std::monostate, SemaphoreError>>;
    {
        s.do_take(d)
    } -> std::same_as<ErrorHandler<std::monostate, SemaphoreError>>;
    { s.do_give() } -> std::same_as<void>;
    requires !std::is_abstract_v<T>;
};

}  // namespace PlatformCore