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
inline Logger::Logger(const std::string& name):
    name_(name),
    threshold_level_(LogLevel::level::DEBUG),
    rb_(1024),
    running_(false)
{}

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
                return !logger->running_.load() || logger->rb_.getSize() > 0;
            });

            /* check if logger is stopped and ringbuffer is empty */
            if (!logger->running_.load() && logger->rb_.getSize() == 0)
                break;

            /* pop out log event from ringbuffer */
            LogEvent::Ptr out_event;
            while (logger->rb_.pop(out_event))
            {
                try
                {
                    /* copy appenders in order to avoid data race and reduce lock time */
                    std::list<BaseAppender::Ptr> copy_appenders;
                    /* after this block, read lock will be released, and we get copied variables */
                    {
                        std::shared_lock<std::shared_mutex> read_lk(logger->rw_mtx_);
                        copy_appenders = logger->appenders_;
                    }

                    /* submit to each appender */
                    for (const auto& app: copy_appenders)
                    {
                        app->append(out_event);
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
    if (event == nullptr || event->getLogLevel() < threshold_level_)
        return;

    /* check whether have own appenders */
    bool has_appenders = false;
    Logger::Ptr curr_root_logger;
    /* copy for thread-safe */
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        has_appenders = !appenders_.empty();
        curr_root_logger = root_logger_;
    }

    /* if did not have appenders, use copy root logger to submit */
    if (!has_appenders && (curr_root_logger != nullptr))
    {
        curr_root_logger->submit(event);
        return;
    }

    /* push to ringbuffer */
    /* 'cause this is hot path, you SHOULD NOT block it */
    if (rb_.push(event))
    {
        std::unique_lock<std::mutex> cv_lk(cv_mtx_);
        cv_.notify_one();
    }
}

inline void Logger::setRootLogger(const Logger::Ptr& root_logger)
{
    if (root_logger == nullptr)
    {
        throw aw_logger::invalid_parameter("input root logger is nullptr!");
        return;
    }

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    root_logger_ = root_logger;
}

inline void Logger::setAppender(const BaseAppender::Ptr& appender)
{
    if (appender == nullptr)
    {
        throw aw_logger::invalid_parameter("input appender is nullptr!");
        return;
    }

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

LoggerManager::LoggerManager()
{
    /* initialize root logger */
    root_logger_ = std::make_shared<Logger>();
    root_logger_->setAppender(std::make_shared<ConsoleAppender>());
    loggers_map_.emplace(root_logger_->getName(), root_logger_);
}

inline Logger::Ptr LoggerManager::getLogger(const std::string& name)
{
    /* if just want to get root logger, return directly */
    if (name == "root")
    {
        std::unique_lock<std::mutex> lk(mgr_mtx_);
        return root_logger_;
    }

    std::unique_lock<std::mutex> lk(mgr_mtx_);
    auto it_1 = loggers_map_.find(name);
    if (it_1 != loggers_map_.end())
        return it_1->second;

    /* if can't find, create new logger */
    /* copy root logger to avoid lock */
    Logger::Ptr copy_root_logger = root_logger_;
    lk.unlock();

    Logger::Ptr logger = std::make_shared<Logger>(name);
    /* pass copy root logger instead of `this->root_logger`, here we can lock-free operation */
    logger->setRootLogger(copy_root_logger);

    /* lock again to check again, avoid another thread create it before */
    lk.lock();
    auto it_2 = loggers_map_.find(name);
    if (it_2 != loggers_map_.end())
        return it_2->second;

    loggers_map_.emplace(name, logger);
    return logger;
}

} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
