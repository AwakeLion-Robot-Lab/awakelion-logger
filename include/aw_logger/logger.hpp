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

#ifndef LOGGER_HPP
#define LOGGER_HPP

// C++ standard library
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

// aw_logger library
#include "aw_logger/appender.hpp"
#include "aw_logger/log_event.hpp"
#include "aw_logger/ring_buffer.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @details
 **********************************************
 *  User Code(Frontend)       Logger(Backend) *
 *    write threads            read threads   *
 *      submit()                   pop()      *
 **********************************************
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
class LogEvent;
class BaseAppender;
class ConsoleAppender;

/***
 * @brief asynchronous logger class with a center ringbuffer
 * @details `std::enabled_shared_from_this` allow to manage the ONLY ONE share pointer of this class object
 * @details via `std::shared_from_this`, which is CRTP
 */
class Logger: public std::enable_shared_from_this<Logger> {
public:
    using Ptr = std::shared_ptr<Logger>;
    using ConstPtr = std::shared_ptr<const Logger>;

    /***
     * @brief constructor
     */
    explicit Logger(const std::string& name = "root");

    /***
     * @brief destructor
     */
    ~Logger();

    /***
     * @brief initialize logger
     */
    void init();

    /***
     * @brief submit log event pointer to ringbuffer
     * @param event enqueue event
     */
    void submit(const std::shared_ptr<LogEvent>& event);

    /***
     * @brief set log level threshold
     * @param thres log level threshold
     */
    void setThresholdLevel(LogLevel::level thres)
    {
        threshold_level_.store(thres, std::memory_order_release);
    }

    /***
     * @brief get log level threshold
     */
    inline LogLevel::level getThresLevel() const noexcept
    {
        return threshold_level_.load(std::memory_order_acquire);
    }

    /***
     * @brief set root logger
     * @param root_logger root logger
     */
    void setRootLogger(const Logger::Ptr& root_logger);

    /***
     * @brief set appender to appender list
     * @param appender appender to be added
     */
    void setAppender(const std::shared_ptr<BaseAppender>& appender);

    /***
     * @brief remove specific appender from appender list
     * @param appender specific appender to be removed
     */
    void removeAppender(const std::shared_ptr<BaseAppender>& appender);

    /***
     * @brief clear all appenders inside appender list
     */
    void clearAppenders();

    std::string getName() const noexcept
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        return name_;
    }

private:
    /***
     * @brief root logger
     */
    Logger::Ptr root_logger_;

    /***
     * @brief log event ringbuffer
     */
    RingBuffer<std::shared_ptr<LogEvent>> rb_;

    /***
     * @brief worker thread to pop out log message from ringbuffer to appenders
     */
    std::thread worker_;

    /***
     * @brief log level threshold
     */
    std::atomic<LogLevel::level> threshold_level_;

    /***
     * @brief flag to indicate whether the logger is running
     */
    std::atomic<bool> running_;

    /***
     * @brief start flag to ensure to start named logger ONLY ONCE
     */
    std::once_flag start_flag_;

    /***
     * @brief condition variable to notify the ringbuffer inside worker thread
     * @details
     * given to condition variable may be spurious wakeup or false wakeup, so we need to set predicate in `wait` function
     * and predication should validate the status of `running_` and whether ringbuffer has new messages
     */
    std::condition_variable cv_;

    /***
     * @brief mutex to manage condition variable
     * @details
     * this mutex is used to protect the condition variable, it MUST BE independent with `rw_mtx_`
     */
    mutable std::mutex cv_mtx_;

    /***
     * @brief read and write logger mutex
     * @note write operation is `std::shared_lock`(share mode), otherwise is `std::unique_lock`(unique mode) of read operation
     * @details
     * read operation of logger is attribute log messages to appenders list
     * write operation of logger includes add or remove appender and add log level threshold
     * push message to ringbuffer is a kind of hot path, it should be lock-free ought to be faster
     * so in fact, this shared mutex protect appender operation and it is not involved in ringbuffer operation
     */
    mutable std::shared_mutex rw_mtx_;

    /***
     * @brief list of appenders
     */
    std::list<std::shared_ptr<BaseAppender>> appenders_;

    /***
     * @brief logger name
     */
    std::string name_;

    /***
     * @brief start to run worker thread
     */
    void start();

    /***
     * @brief stop running worker thread
     */
    void stop();
};

/***
 * @brief singleton logger manager class to manage multi-loggers
 */
class LoggerManager {
public:
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager(LoggerManager&&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;
    LoggerManager& operator=(LoggerManager&&) = delete;

    /***
     * @brief constructor
     */
    LoggerManager() {}

    /***
     * @brief destructor
     */
    ~LoggerManager();

    /***
     * @brief get static `std::shared_ptr` instance
     * @return static `std::shared_ptr` instance
     */
    static std::shared_ptr<LoggerManager> getInstance()
    {
        static std::shared_ptr<LoggerManager> instance = std::make_shared<LoggerManager>();
        instance->init();
        return instance;
    }

    /***
     * @brief get logger
     * @param name logger name
     * @return current logger
     */
    Logger::Ptr getLogger(const std::string& name);

    /***
     * @brief initialize root logger for ONLY ONCE
     */
    void init();

private:
    /***
     * @brief root logger pointer
     */
    Logger::Ptr root_logger_;

    /***
     * @brief loggers map to storage and search specific logger
     * @details {logger name: pointer of logger}
     */
    std::unordered_map<std::string, Logger::Ptr> loggers_map_;

    /***
     * @brief read and write logger manager mutex
     */
    mutable std::shared_mutex rw_mtx_;

    /***
     * @brief start flag to ensure to start logger manager ONLY ONCE
     */
    std::once_flag start_flag_;

    /***
     * @brief destroy logger manager in RAII
     */
    void destroy();
};
} // namespace aw_logger

#endif //! LOGGER_HPP
