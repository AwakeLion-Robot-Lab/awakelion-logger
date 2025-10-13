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
#include <list>
#include <memory>
#include <shared_mutex>
#include <thread>

// aw_logger library
#include "aw_logger/appender.hpp"
#include "aw_logger/formatter.hpp"
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
/***
 * @brief asynchronous logger class with a center ringbuffer
 * @details `std::enabled_shared_from_this` allow to manage the ONLY ONE share pointer of this class object
 * @details via `std::shared_from_this`, which is CRTP
 */
class Logger: public std::enable_shared_from_this<Logger> {
public:
    using Ptr = std::shared_ptr<Logger>;
    using ConstPtr = const std::shared_ptr<Logger>;

    /***
     * @brief constructor
     */
    explicit Logger(std::string_view name = "root");

    /***
     * @brief destructor
     */
    ~Logger();

    /***
     * @brief submit formatted log messages to ringbuffer
     */
    void submit(LogEvent::Ptr& event);

    /***
     * @brief set log level threshold
     * @param thres log level threshold
     */
    void setThresholdLevel(LogLevel::level thres)
    {
        threshold_level_ = thres;
    }

    /***
     * @brief set formatter to logger
     * @param formatter formatter to be set
     */
    void setFormatter(Formatter::ConstPtr& formatter);

    /***
     * @brief set appender to appender list
     * @param appender appender to be added
     */
    void setAppender(BaseAppender::ConstPtr& appender);

    /***
     * @brief remove specific appender from appender list
     * @param appender specific appender to be removed
     */
    void removeAppender(BaseAppender::ConstPtr& appender);

    /***
     * @brief clear all appenders inside appender list
     */
    void clearAppenders();

private:
    /***
     * @brief formatter
     */
    Formatter::Ptr formatter_;

    /***
     * @brief log event ringbuffer
     */
    RingBuffer<LogEvent::Ptr> rb_;

    /***
     * @brief log level threshold
     */
    LogLevel::level threshold_level_;

    /***
     * @brief worker thread to pop out log message from ringbuffer to appenders
     */
    std::thread worker_;

    /***
     * @brief flag to indicate whether the logger is running
     */
    std::atomic<bool> running_;

    /***
     * @brief condition variable to notify the ringbuffer inside worker thread
     * @details
     * given to condition variable may be spurious wakeup or false wakeup, so we need to set predicate in `wait` function
     * and predication should validate the status of `running_` and whether ringbuffer has rest size
     */
    std::condition_variable cv_;

    /***
     * @brief mutex to manage condition variable
     * @details
     * this mutex is used to protect the condition variable, it MUST BE independent with `rw_mtx_`
     */
    std::mutex cv_mtx_;

    /***
     * @brief read and write logger mutex
     * @note write operation is `std::shared_lock`(share mode), otherwise is `std::unique_lock`(unique mode) of read operation
     * @details
     * read operation of logger is attribute log messages to appenders list
     * write operation of logger includes add or remove appender, log level threshold and formatter
     * push message to ringbuffer is a kind of hot path, it should be lock-free ought to be faster
     * so in fact, this shared mutex protect appender operation and it is not involved in ringbuffer operation
     */
    std::shared_mutex rw_mtx_;

    /***
     * @brief list of appenders
     */
    std::list<BaseAppender::Ptr> appenders_;

    /***
     * @brief logger name
     */
    std::string name_;
};

/***
 * @brief singleton logger manager class to manage multi-loggers
 */
class LoggerManager {
public:
    /***
     * @brief constructor
     */
    explicit LoggerManager();

    LoggerManager(const LoggerManager&) = delete;
    LoggerManager(const LoggerManager&&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    /***
     * @brief get static instance
     * @return static instance
     */
    std::shared_ptr<LoggerManager> getInstance();

    /***
     * @brief get logger
     * @param name logger name
     * @return current logger
     */
    Logger::Ptr& getLogger(std::string_view name);

    /***
     * @brief get root logger
     * @return root logger
     */
    Logger::Ptr& getRootLogger()
    {
        return root_logger_;
    }

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
     * @brief logger manager mutex
     */
    std::mutex mgr_mtx_;
};
} // namespace aw_logger

#endif //! LOGGER_HPP
