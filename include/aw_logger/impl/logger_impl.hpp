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
#ifndef IMPL__LOGGER_IMPL_HPP
#define IMPL__LOGGER_IMPL_HPP

// aw_logger library
#include "aw_logger/logger.hpp"

namespace aw_logger {
inline Logger::Logger(std::string_view name):
    name_(name),
    threshold_level_(LogLevel::level::DEBUG),
    rb_(64 * 1024),
    running_(false)
{
    formatter_ = std::make_shared<Formatter>(new ComponentFactory());
}

inline Logger::~Logger()
{
    running_.store(false);
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void Logger::submit(LogEvent::ConstPtr& event)
{
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event is nullptr!");

    /* log level filter */
    if (event->getLogLevel() < threshold_level_)
        return;

    /* push to ringbuffer */
    rb_.push(event);
}
} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
