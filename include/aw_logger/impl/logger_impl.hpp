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
    stop();
}

void Logger::start()
{
    /* we gotta turn on running flag if worker thread is not running */
    /* CAS operation reference: https://blog.csdn.net/feikudai8460/article/details/107035480 */
    bool expected = false;
    /* already running */
    if (!running_.compare_exchange_strong(expected, true))
        return;

    /* weak pointer to avoid hanging pointer */
    auto self = std::weak_ptr<Logger>(shared_from_this());
    /* worker thread */
    worker_ = std::thread([self]() {
        /* keep running */
        while (true)
        {
            /* get logger instance */
            auto logger = self.lock();
            if (logger == nullptr)
                break;

            /* wait for logger status and new log event */
            std::unique_lock<std::mutex> cv_lk(logger->cv_mtx_);
            logger->cv_.wait(cv_lk, [&]() {
                return !logger->running_.load() || logger->rb_.getRestSize() != 0;
            });

            /* check if logger is stopped and ringbuffer is empty */
            if (!logger->running_.load() && logger->rb_.getRestSize() == 0)
                break;

            /* pop out log event from ringbuffer */
            LogEvent::Ptr out_event;
            while (logger->rb_.pop(out_event))
            {
                try
                {
                    /* copy formatter and appenders in order to avoid data race and reduce lock time */
                    Formatter::Ptr copy_formatter = logger->formatter_;
                    std::list<BaseAppender::Ptr> copy_appenders = logger->appenders_;
                    /* after this block, read lock will be released, and we get copied variables */
                    {
                        std::shared_lock<std::shared_mutex> read_lk(logger->rw_mtx_);
                        copy_formatter = logger->formatter_;
                        copy_appenders = logger->appenders_;
                    }

                    /* check if formatter is nullptr */
                    if (copy_formatter == nullptr)
                        continue;

                    /* format */
                    std::string fmt_msg = copy_formatter->formatComponents(
                        out_event,
                        copy_formatter->getRegisteredComponents()
                    );

                    /* submit to each appender */
                    for (const auto& app: copy_appenders)
                    {
                        app->append(logger, fmt_msg);
                    }
                } catch (const std::exception& ex)
                {
                    std::cerr << ex.what() << '\n' << std::endl;
                } catch (...)
                {
                    std::cerr << "unknown exception in logger worker thread!\n" << std::endl;
                }
            }
        }
    });
}

inline void Logger::stop()
{
    /* if `running_` is true, we gotta turn it off */
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false))
        cv_.notify_all();

    /* wait for the worker thread to finish */
    if (worker_.joinable())
        worker_.join();
}

void Logger::submit(const LogEvent::Ptr& event)
{
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event is nullptr!");

    /* log level filter */
    if (event->getLogLevel() < threshold_level_)
        return;

    /* push to ringbuffer */
    /* 'cause this is hot path, you SHOULD NOT block it */
    rb_.push(event);
    cv_.notify_one();
}

inline void Logger::setFormatter(const Formatter::Ptr& formatter)
{
    if (formatter_ == nullptr)
    {
        throw aw_logger::invalid_parameter("formatter is nullptr!");
        return;
    }

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    formatter_ = formatter;
}

inline void Logger::setAppender(const BaseAppender::Ptr& appender)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    appenders_.emplace_back(appender);
}

inline void Logger::removeAppender(const BaseAppender::Ptr& appender)
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
    appenders_.clear();
}
} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
