// Copyright 2025 siyiovo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOG_MACRO_HPP
#define LOG_MACRO_HPP

// C++ standard library
#include <memory>

// aw_logger library
#include "aw_logger/appender.hpp"
#include "aw_logger/fmt_base.hpp"
#include "aw_logger/log_event.hpp"
#include "aw_logger/logger.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief aw logger base macro definition
 * @param logger logger instance
 * @param level input log level
 * @param fmt log message
 * @param ... log message format arguments
 */
#define AW_LOG_BASE(logger, level, fmt, ...) \
    /* if the priority of input level higher than threshold level */ \
    if (level >= logger->getThresLevel()) \
    LogEventWrap( \
        std::make_shared<LogEvent>( \
            logger, \
            level, \
            LogEvent::LocalSourceLocation<std::string>(Formatter::vformat(fmt, __VA_ARGS__)) \
        ) \
    )

/***
 * @brief aw logger debug macro definition
 */
#define AW_LOG_DEBUG(logger, fmt, ...) AW_LOG_BASE(logger, LogLevel::level::DEBUG, fmt, __VA_ARGS__)

/***
 * @brief aw logger info macro definition
 */
#define AW_LOG_INFO(logger, fmt, ...) AW_LOG_BASE(logger, LogLevel::level::INFO, fmt, __VA_ARGS__)

/***
 * @brief aw logger notice macro definition
 */
#define AW_LOG_NOTICE(logger, fmt, ...) \
    AW_LOG_BASE(logger, LogLevel::level::NOTICE, fmt, __VA_ARGS__)

/***
 * @brief aw logger warn macro definition
 */
#define AW_LOG_WARN(logger, fmt, ...) AW_LOG_BASE(logger, LogLevel::level::WARN, fmt, __VA_ARGS__)

/***
 * @brief aw logger error macro definition
 */
#define AW_LOG_ERROR(logger, fmt, ...) AW_LOG_BASE(logger, LogLevel::level::ERROR, fmt, __VA_ARGS__)

/***
 * @brief aw logger fatal macro definition
 */
#define AW_LOG_FATAL(logger, fmt, ...) AW_LOG_BASE(logger, LogLevel::level::FATAL, fmt, __VA_ARGS__)

} // namespace aw_logger

#endif //! LOG_MACRO_HPP
