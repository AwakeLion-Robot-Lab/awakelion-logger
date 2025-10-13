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
    rb_(1024),
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

inline void Logger::setFormatter(Formatter::ConstPtr& formatter)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    if (formatter_ == nullptr)
        formatter_ = formatter;
}

inline void Logger::setAppender(BaseAppender::ConstPtr& appender)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    appenders_.emplace_back(appender);
}

inline void Logger::removeAppender(BaseAppender::ConstPtr& appender)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    for (auto it = appenders_.begin(); it != appenders_.end(); it++)
    {
        if (*it == appender)
        {
            appenders_.erase(it);
            break;
        }
    }
}

inline void Logger::clearAppenders()
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    appenders_.remove_if([&]() { return appenders_.size() != 0; });
}

void Logger::submit(LogEvent::Ptr& event)
{
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event is nullptr!");

    /* log level filter */
    if (event->getLogLevel() < threshold_level_)
        return;

    /* push to ringbuffer */
    /* 'cause this is hot path, you SHOULD NOT block it */
    rb_.push(event);

    /* TODO(siyiya): finish worker thread */
    while (running_.load())
    {
        /* weak pointer to avoid hanging pointer */
        auto self = std::weak_ptr<Logger>(shared_from_this());
        auto lock = self.lock();

        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        LogEvent::Ptr out_event;
        if (rb_.pop(out_event))
        {
            auto fmt =
                Formatter::formatComponents(out_event, formatter_->getRegisteredComponents());
            for (auto& app: appenders_)
            {
                app->output(lock, fmt);
                std::unique_lock<std::mutex> cv_lk(cv_mtx_);
                cv_.wait(cv_lk, [&]() { return !running_.load() && rb_.getRestSize() != 0; });
            }
        }
    }
}
} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
