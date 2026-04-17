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

// POSIX library
#include <limits.h>
#include <unistd.h>

// C++ standard library
#include <cerrno>
#include <mutex>

// aw_logger library
#include "aw_logger/appender.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
ConsoleAppender::ConsoleAppender(std::string_view stream_type): fd_(getStreamFd(stream_type)) {}

ConsoleAppender::ConsoleAppender(Formatter::Ptr formatter, std::string_view stream_type):
    BaseAppender(std::move(formatter)),
    fd_(getStreamFd(stream_type))
{}

void ConsoleAppender::append(const LogEvent::Ptr& event)
{
    /* check level */
    if (event->getLogLevel() < getThresholdLevel())
        return;

    /* thread-local buffer for no malloc */
    thread_local std::string log_msg;
    log_msg.clear();
    formatMsgTo(log_msg, event);
    log_msg.push_back('\n');

    const char* data = log_msg.data();
    size_t remaining = log_msg.size();

    if (remaining <= static_cast<size_t>(PIPE_BUF))
    {
        while (remaining > 0)
        {
            ssize_t n = ::write(fd_, data, remaining);
            if (n < 0)
            {
                if (errno == EINTR)
                    continue;
                return;
            }
            data += n;
            remaining -= static_cast<size_t>(n);
        }
        return;
    }

    std::lock_guard<std::mutex> lk(bigWriteMutex());
    while (remaining > 0)
    {
        ssize_t n = ::write(fd_, data, remaining);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return;
        }
        data += n;
        remaining -= static_cast<size_t>(n);
    }
}

inline int ConsoleAppender::getStreamFd(std::string_view stream_type)
{
    if (stream_type == "stdout")
        return STDOUT_FILENO;
    else if (stream_type == "stderr")
        return STDERR_FILENO;
    else
        throw aw_logger::invalid_parameter("invalid stream type, please use 'stdout' or 'stderr'.");
}

} // namespace aw_logger

#endif //! IMPL__CONSOLE_APPENDER_IMPL_HPP
