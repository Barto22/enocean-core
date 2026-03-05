/**
 * @file ZephyrAppender.hpp
 * @brief plog appender that routes log records to the Zephyr logging subsystem.
 *
 * The appender implementation lives in ZephyrAppender.cpp which owns the
 * LOG_MODULE_REGISTER declaration required by Zephyr.  All plog log records
 * produced anywhere in the application are funnelled through this single
 * appender and re-emitted via the appropriate Zephyr LOG_xxx macro.
 *
 * Level mapping:
 *   plog::fatal / plog::error  -> LOG_ERR
 *   plog::warning              -> LOG_WRN
 *   plog::info                 -> LOG_INF
 *   plog::debug / plog::verbose -> LOG_DBG
 */
#pragma once

#include <plog/Appenders/IAppender.h>

namespace Logging {

/// @brief plog IAppender implementation that forwards log records to Zephyr's
/// logging subsystem.
class ZephyrAppender : public plog::IAppender {
   public:
    /**
     * @brief Write a plog record to the Zephyr logging subsystem.
     *
     * Declared here, defined in ZephyrAppender.cpp which holds the Zephyr
     * LOG_MODULE_REGISTER so that all output is attributed to one module.
     *
     * @param record  The log record produced by plog.
     */
    void write(const plog::Record& record) noexcept override;
};

}  // namespace Logging
