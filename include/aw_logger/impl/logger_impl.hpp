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

// C++ standard library
#include <typeinfo>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/logger.hpp"

namespace aw_logger {
inline Logger::Logger(const std::string& name):
    rb_(256),
    threshold_level_(LogLevel::level::DEBUG),
    running_(false),
    name_(name)
{}

inline Logger::~Logger()
{
    /* flush ringbuffer and stop */
    flush();
    stop();
}

void Logger::submit(const std::shared_ptr<LogEvent>& event)
{
    /* check status of event */
    if (event == nullptr || event->getLogLevel() < threshold_level_)
        return;

    /* get status of appenders list via thread-safe copy */
    bool has_appenders = false;
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        has_appenders = !appenders_.empty();
    }

    /* check whether it have own appenders */
    if (has_appenders)
    {
        /* if current logger has appenders, start it for once, after once, it will return via CAS operation */
        start();

        /* if get new event, notify worker thread via `std::condition_variable` */
        if (rb_.push(event))
        {
            std::unique_lock<std::mutex> cv_lk(cv_mtx_);
            cv_.notify_one();
        }
        return;
    }

    /* if it do not have own appenders, alter to root logger to append */
    Logger::Ptr curr_root_logger;
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        curr_root_logger = root_logger_;
    }

    if (curr_root_logger != nullptr)
    {
        curr_root_logger->submit(event);
    }
    else
    {
        throw aw_logger::invalid_parameter("root logger is nullptr!");
    }
}

inline void Logger::setRootLogger(const Logger::Ptr& root_logger)
{
    if (root_logger == nullptr)
        throw aw_logger::invalid_parameter("input root logger is nullptr!");

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    /* check existing and set root logger under write lock for thread-safe */
    if (root_logger_ != nullptr)
        throw aw_logger::invalid_parameter("root logger has been already set!");

    root_logger_ = root_logger;
}

inline void Logger::setAppender(const std::shared_ptr<BaseAppender>& appender)
{
    if (appender == nullptr)
        throw aw_logger::invalid_parameter("input appender is nullptr!");

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    /* check existing and set appender under write lock for thread-safe */
    for (const auto& ex_app: appenders_)
    {
        if (ex_app == appender)
        {
            throw aw_logger::invalid_parameter(
                std::string("an existing-type appender like: ") + typeid(*appender).name()
                + "has already setup!"
            );
        }
    }

    appenders_.emplace_back(appender);
}

inline void Logger::removeAppender(const std::shared_ptr<BaseAppender>& appender)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    for (auto it = appenders_.begin(); it != appenders_.end(); it++)
    {
        if (*it == appender)
        {
            appenders_.erase(it);
            return;
        }
    }

    /* if could not find, throw exception */
    throw aw_logger::invalid_parameter(
        std::string("appenders list did not set appender like: ") + typeid(*appender).name()
        + " before!"
    );
}

inline void Logger::clearAppenders()
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    appenders_.clear();
}

inline void Logger::flush()
{
    /* wait until ringbuffer is empty */
    while (rb_.getSize() > 0)
    {
        std::this_thread::yield();
    }

    /* flush all appenders, just for current thread */
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    for (const auto& app: appenders_)
    {
        app->flush();
    }
}

void Logger::init()
{
    std::call_once(start_flag_, [this]() { start(); });
}

void Logger::start()
{
    /* we gotta turn on running flag if worker thread is not running */
    /* CAS operation reference: https://blog.csdn.net/feikudai8460/article/details/107035480 */
    bool expected = false;
    /* already running */
    if (!running_.compare_exchange_strong(expected, true))
        return;

    /* weak pointer to avoid circular reference */
    auto self = std::weak_ptr<Logger>(shared_from_this());
    /* worker thread */
    worker_ = std::thread([self]() {
        /* keep running */
        while (true)
        {
            /* get instance of current logger */
            auto logger = self.lock();
            if (logger == nullptr)
                break;

            /**
             * wait for logger status(if not running, break the loop)
             * or new log event(size > 0, pop out to appender)
             */
            std::unique_lock<std::mutex> cv_lk(logger->cv_mtx_);
            logger->cv_.wait(cv_lk, [logger]() {
                return !logger->running_.load(std::memory_order_relaxed)
                    || logger->rb_.getSize() > 0;
            });

            /* check if logger is stopped and ringbuffer is empty */
            if (!logger->running_.load(std::memory_order_relaxed) && logger->rb_.getSize() == 0)
                break;

            /* pop out log event from ringbuffer */
            LogEvent::Ptr out_event;
            while (logger->rb_.pop(out_event))
            {
                try
                {
                    /* copy appenders in order to avoid data race which is for thread safe */
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
                    std::cerr << "unknown exception in logger worker thread.\n" << std::endl;
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

inline LoggerManager::~LoggerManager()
{
    destroy();
}

inline Logger::Ptr LoggerManager::getLogger(const std::string& name)
{
    /* if just want to get root logger, return directly */
    if (name == "root")
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        return root_logger_;
    }

    /* find in loggers map */
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        auto it = loggers_map_.find(name);
        if (it != loggers_map_.end())
            return it->second;
    }

    /* if can't find, create new logger */
    /* copy root logger for thread-safe */
    Logger::Ptr copy_root_logger;
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        copy_root_logger = root_logger_;
    }

    Logger::Ptr logger = std::make_shared<Logger>(name);
    /* pass copy root logger instead of `this->root_logger_`, here we can use lock-free operation */
    logger->setRootLogger(copy_root_logger);

    /* lock again and check again, avoid another thread create it before */
    {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        auto [it, inserted] = loggers_map_.try_emplace(name, logger);
        /* if this logger has create, `inserted` == false and it won't be construct */
        if (!inserted)
        {
            return it->second;
        }
        else
        {
            return logger;
        }
    }
}

inline void LoggerManager::init()
{
    std::call_once(start_flag_, [this]() {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        root_logger_ = std::make_shared<Logger>("root");
        root_logger_->setAppender(std::make_shared<ConsoleAppender>());
        loggers_map_.emplace("root", root_logger_);
        root_logger_->init();
    });
}

inline void LoggerManager::destroy()
{
    loggers_map_.clear();
}

} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
