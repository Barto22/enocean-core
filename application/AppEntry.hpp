
/**
 * @file AppEntry.hpp
 * @brief Application entry point thread runner for platform-agnostic logic.
 *
 * This file defines the main application entry class (`AppEntry`) implementing
 * the thread runner interface for platform-independent execution.
 *
 * - `AppEntry` is a final class derived from `IThreadRunner`.
 * - Implements the `run` method for application logic.
 * - Compile-time check ensures correct interface implementation.
 *
 * Usage:
 *   - Instantiate `AppEntry` and call `run()` with platform-specific
 * parameters.
 */
#pragma once

#include <cstdint>

#include "IThread.hpp"

namespace App {

/**
 * @class AppEntry
 * @brief Application thread runner for main logic execution.
 *
 * Implements the `IThreadRunner` interface for platform-agnostic application
 * entry.
 */
class AppEntry final : public PlatformCore::IThreadRunner<AppEntry> {
   public:
    /**
     * @brief Construct a new AppEntry object.
     */
    AppEntry() noexcept = default;

    /**
     * @brief Run the main application logic.
     * @param params Platform-specific thread parameters.
     */
    void run_impl(PlatformCore::ThreadParamVariant& params);
};

/**
 * @brief Compile-time check for valid thread runner implementation.
 */
static_assert(
    PlatformCore::ThreadRunnerImplementation<AppEntry>,
    "The Thread Runner must correctly implement the IThreadRunner interface.");

}  // namespace App