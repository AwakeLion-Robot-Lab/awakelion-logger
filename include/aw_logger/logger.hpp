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
#include <mutex>

// aw_logger library
#include "aw_logger/formatter.hpp"
#include "aw_logger/ring_buffer.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief filter class apply to logger class in order to limit output log 
 */
class Filter {
public:
    using Ptr = std::shared_ptr<Filter>;
    using ConstPtr = const std::shared_ptr<Filter>;

    /***
     * @brief constructor
     */
    explicit Filter();

    /***
     * @brief set filter
     * @tparam Args changable parameter list
     * @param s input filter
     * @param args changable parameters
     */
    template<typename... Args>
    void setFilter(std::string_view s, Args&&... args);

private:
    /***
     * @brief log level
     */
    LogLevel::level level_;
};

/***
 * @brief asynchronous logger class with a center ringbuffer
 */
class Logger: public std::enable_shared_from_this<Logger> {
public:
    using Ptr = std::shared_ptr<Logger>;
    using ConstPtr = const std::shared_ptr<Logger>;

    /***
     * @brief constructor
     */
    explicit Logger();

private:
    /***
     * @brief formatter
     */
    Formatter::Ptr formatter_;

    /***
     * @brief filter
     */
    Filter::Ptr filter_;

    /***
     * @brief ringbuffer
     */
    RingBuffer<std::string> rb_;
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
     */
    std::unordered_map<std::string, Logger::Ptr> loggers_map_;

    /***
     * @brief mutex
     */
    std::mutex mutex_;
};
} // namespace aw_logger

#endif //! LOGGER_HPP