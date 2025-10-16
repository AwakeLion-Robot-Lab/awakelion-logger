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
#include <exception>
#include <memory>
#include <string>

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
 * @brief format input log message
 * @tparam Args variadic template parameter
 * @param fmt format string
 * @param args variadic template parameter
 * @return formatted message
 */
template<typename... Args>
std::string format_message(std::string_view fmt, Args&&... args)
{
    return Formatter::format(fmt, std::forward<Args>(args)...);
}

} // namespace aw_logger

/***
 * @brief aw logger base macro definition
 * @param logger logger instance
 * @param level input log level
 * @param msg log message
 */
#define AW_LOG_BASE(logger, level, msg) \
    if (level >= logger->getThresLevel()) \
    { \
        try \
        { \
            aw_logger::LogEventWrap( \
                std::make_shared<aw_logger::LogEvent>( \
                    logger, \
                    level, \
                    aw_logger::LogEvent::LocalSourceLocation<std::string>( \
                        msg, \
                        std::source_location::current() \
                    ) \
                ) \
            ); \
        } catch (std::exception & ex) \
        { \
            std::cerr << ex.what() << "\n" << std::endl; \
        } \
    }

#define AW_LOG_DEBUG(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::DEBUG, msg)

#define AW_LOG_INFO(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::INFO, msg)

#define AW_LOG_NOTICE(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::NOTICE, msg)

#define AW_LOG_WARN(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::WARN, msg)

#define AW_LOG_ERROR(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::ERROR, msg)

#define AW_LOG_FATAL(logger, msg) AW_LOG_BASE(logger, aw_logger::LogLevel::level::FATAL, msg)

#endif //! LOG_MACRO_HPP
