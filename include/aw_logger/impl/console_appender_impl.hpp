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

#ifndef IMPL__CONSOLE_APPENDER_IMPL_HPP
#define IMPL__CONSOLE_APPENDER_IMPL_HPP

// aw_logger library
#include "aw_logger/appender.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
ConsoleAppender::ConsoleAppender(std::string_view stream_type):
    output_stream_(getStreamType(stream_type))
{}

ConsoleAppender::ConsoleAppender(Formatter::Ptr formatter, std::string_view stream_type):
    BaseAppender(std::move(formatter)),
    output_stream_(getStreamType(stream_type))
{}

void ConsoleAppender::append(const LogEvent::Ptr& event)
{
    auto log_msg = formatMsg(event);
    /* create temporary osyncstream - automatically emits on destruction for thread-safe output */
    std::osyncstream(output_stream_) << log_msg << std::endl;
}

inline std::ostream& aw_logger::ConsoleAppender::getStreamType(std::string_view stream_type)
{
    if (stream_type == "stdout")
        return std::cout;
    else if (stream_type == "stderr")
        return std::cerr;
    else
        throw aw_logger::invalid_parameter("invalid stream type, please use 'stdout' or 'stderr'.");
}

} // namespace aw_logger

#endif //! IMPL__CONSOLE_APPENDER_IMPL_HPP
