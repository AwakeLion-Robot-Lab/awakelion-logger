// Copyright 2026 siyiovo
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
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
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
 * @brief asynchronous logger with a bounded normal ringbuffer and a critical ringbuffer
 * @details
 * `std::enabled_shared_from_this` allow to manage the ONLY ONE share pointer of this class object
 *  via `std::shared_from_this`, which is CRTP
 */
class Logger: public std::enable_shared_from_this<Logger> {
public:
    using Ptr = std::shared_ptr<Logger>;
    using ConstPtr = std::shared_ptr<const Logger>;

    /***
     * @brief overflow policy of ringbuffer
     * @details
     * drop_new: drop new log event when ringbuffer is full
     * block: block the producer thread until ringbuffer has space
     * overrun_oldest: overwrite the oldest log event when ringbuffer is full
     */
    enum class Policy { drop_new, block, overrun_oldest };

    /***
     * @brief queue capacity and policy options
     */
    struct Options {
        /***
         * @brief normal ringbuffer capacity
         */
        size_t normal_capacity = 256;

        /***
         * @brief critical ringbuffer capacity
         */
        size_t critical_capacity = 64;

        /***
         * @brief normal ringbuffer overflow policy
         */
        Policy normal_policy = Policy::drop_new;

        /***
         * @brief critical ringbuffer overflow policy
         */
        Policy critical_policy = Policy::drop_new;
    };

    /***
     * @brief constructor
     * @param name logger name
     * @param lvl log level threshold for logger
     */
    explicit Logger(
        const std::string& name = "root",
        const LogLevel::level lvl = LogLevel::level::DEBUG
    );

    /***
     * @brief constructor
     * @param name logger name
     * @param lvl log level threshold for logger
     * @param options queue capacity and overflow options
     */
    explicit Logger(const std::string& name, const LogLevel::level lvl, const Options& options);

    /***
     * @brief release final owner outside appender callbacks
     */
    ~Logger();

    /***
     * @brief initialize logger worker
     */
    void init();

    /***
     * @brief submit log event to selected queue
     * @param event log event to submit
     */
    void submit(const std::shared_ptr<LogEvent>& event);

    /***
     * @brief set log level threshold for logger
     * @param thres log level threshold for logger
     */
    void setThresholdLevel(LogLevel::level thres)
    {
        threshold_level_.store(thres, std::memory_order_release);
    }

    /***
     * @brief get log level threshold available for logger
     * @return log level threshold available for logger
     */
    inline LogLevel::level getThresholdLevel() const noexcept
    {
        return threshold_level_.load(std::memory_order_acquire);
    }

    /***
     * @brief set(bind) root logger
     * @param root_logger root logger
     */
    void setRootLogger(const Logger::Ptr& root_logger);

    /***
     * @brief set appender to appender list
     * @param appender appender to be added
     */
    void setAppender(const std::shared_ptr<BaseAppender>& appender);

    /***
     * @brief set multiple appenders to appender list
     * @tparam UArgs variadic template of appender types
     * @param appenders multiple appenders to be added
     * @details `std::convertible_to` check whether `UArgs` can be converted to `std::shared_ptr<BaseAppender>`
     */
    // clang-format off
    template<typename... UArgs>
        requires(std::convertible_to<UArgs, std::shared_ptr<BaseAppender>> && ...)
    void setAppenders(UArgs&&... appenders);
    // clang-format on

    /***
     * @brief remove specific appender from appender list
     * @param appender specific appender to be removed
     */
    void removeAppender(const std::shared_ptr<BaseAppender>& appender);

    /***
     * @brief clear all appenders inside appender list
     */
    void clearAppenders();

    /***
     * @brief wait for queue watermarks and flush appenders
     * @note returns immediately from this logger's worker callback
     */
    void flush();

    /***
     * @brief get logger name
     * @return current logger name
     */
    std::string getName() const noexcept
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        return name_;
    }

    /***
     * @brief start to run worker thread
     */
    void start();

    /***
     * @brief stop running worker thread
     */
    void stop();

private:
    /***
     * @brief current worker logger
     * @details prevents self-join and destruction inside appender callbacks
     */
    inline static thread_local const Logger* active_worker_logger_ = nullptr;

    /***
     * @brief asynchronous backend state
     */
    struct Backend {
        /***
         * @brief constructor
         * @param options queue capacity and overflow options
         */
        explicit Backend(const Options& options):
            normal(options.normal_capacity),
            critical(options.critical_capacity)
        {}

        /***
         * @brief normal and critical event queues
         */
        RingBuffer<std::shared_ptr<LogEvent>> normal;
        RingBuffer<std::shared_ptr<LogEvent>> critical;
        /***
         * @brief backend worker thread
         */
        std::thread worker;
        /***
         * @brief serialize worker lifecycle transitions
         */
        std::mutex worker_lifecycle_mtx;
        /***
         * @brief wake worker after a successful enqueue
         */
        std::atomic<uint64_t> work_seq { 0 };
        /***
         * @brief coordinate blocking producers with consumer progress
         */
        std::condition_variable space_cv;
        std::mutex space_mtx;
        /***
         * @brief wake flush waiters after event completion
         */
        std::condition_variable flush_cv;
        std::mutex flush_mtx;
        /***
         * @brief serialize appender calls from worker and flush
         */
        std::mutex appender_call_mtx;
        /***
         * @brief completed submission watermarks
         */
        std::atomic<uint64_t> normal_completed { 0 };
        std::atomic<uint64_t> critical_completed { 0 };
    };

    /***
     * @brief binded root logger
     */
    Logger::Ptr root_logger_;

    /***
     * @brief lazy asynchronous backend
     */
    std::unique_ptr<Backend> backend_;

    /***
     * @brief immutable queue options
     */
    Options options_;

    /***
     * @brief log level threshold
     */
    std::atomic<LogLevel::level> threshold_level_;

    /***
     * @brief flag to indicate whether the logger is running
     */
    std::atomic<bool> running_;

    /***
     * @brief stopped flag in submission state
     */
    inline static constexpr uint64_t stopped_mask_ = uint64_t { 1 } << 63;

    /***
     * @brief stopped flag and active submission count
     */
    std::atomic<uint64_t> submission_state_ { 0 };

    /***
     * @brief read and write logger mutex
     * @note
     * multi-read operation is `std::shared_lock`(share mode) in concurrency,
     * otherwise is `std::unique_lock`(unique mode) of unique write operation
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
     * @brief start backend worker
     * @param state backend state
     * @param reopen reopen producer admission when explicitly started
     */
    void startWorker(Backend* state, bool reopen);

    /***
     * @brief consume backend events in batches
     * @param state backend state
     */
    void workerLoop(Backend* state);

    /***
     * @brief wait for space and push event
     * @param state backend state
     * @param queue target queue
     * @param event log event to push
     * @return whether event was accepted
     */
    bool pushBlocking(
        Backend* state,
        RingBuffer<std::shared_ptr<LogEvent>>& queue,
        const std::shared_ptr<LogEvent>& event
    );

    /***
     * @brief register an active submission
     * @return whether submission is accepted
     */
    bool beginSubmission() noexcept;

    /***
     * @brief complete an active submission
     */
    void finishSubmission() noexcept;

    /***
     * @brief check producer admission
     * @return whether submissions are accepted
     */
    bool isAccepting() const noexcept;
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
    LoggerManager() = default;

    /***
     * @brief destructor
     */
    ~LoggerManager();

    /***
     * @brief get static instance of logger manager
     * @return static instance
     */
    static LoggerManager& getInstance()
    {
        static LoggerManager instance = LoggerManager();
        instance.init();
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
