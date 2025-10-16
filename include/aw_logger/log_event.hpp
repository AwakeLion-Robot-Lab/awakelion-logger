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

#ifndef LOG_EVENT_HPP
#define LOG_EVENT_HPP

// C++ standard library
#include <chrono>
#include <source_location>
#include <thread>

// aw_logger library
#include "aw_logger/fmt_base.hpp"
#include "aw_logger/logger.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief log event class for integrating required message
 */
class LogEvent {
public:
    /***
     * @brief local `std::source_location` struct for locate the log correctly
     * @tparam DataT the type of input data
     * @details the return of `std::source_location::current()` is the position of the current function which is `LogEvent`
     * constructor but not the call site function, that's why we need "local" `std::source_location`
     * @details copied from [minilog](https://github.com/archibate/minilog)
     */
    template<typename DataT>
    struct LocalSourceLocation {
        // tips: struct default is `public`, here is an explicit call
    public:
        /***
         * @brief `LocalSourceLocation` constructor
         * @tparam U universal reference type
         * @param u_ref universal reference of input data
         * @param loc local source location
         * @details
         * the core of real source location is default parameter
         * which means that the real source location is the call site of `LogEvent` constructor
         * you can refer to `log_macro.hpp`
         */
        template<typename U>
            requires std::constructible_from<
                         DataT,
                         U> // `require` ensure that `DataT` can be constructible from `U`

        // `std::constructible_from<DataT,U>` check whether `DataT` can be constructed from `U`
        constexpr LocalSourceLocation(
            U&& u_ref,
            const std::source_location loc = std::source_location::current()
        ):
            data_(
                std::forward<U>(u_ref)
            ), // `std::forward<U>` can judge whether `U` is lvalue or rvalue
            loc_(std::move(loc))
        {}

        /***
         * @brief get input data
         * @note if template `DataT` type is `std::string`, that means `data_` is the log message
         */
        constexpr DataT const& getData() const noexcept
        {
            return data_;
        }

        /***
         * @brief get local source location
         */
        constexpr std::source_location const& getLocation() const noexcept
        {
            return loc_;
        }

    private:
        /***
         * @brief input packed data
         */
        DataT data_;

        /***
         * @brief local source location
         */
        std::source_location loc_;
    };

    using Ptr = std::shared_ptr<LogEvent>;
    using ConstPtr = std::shared_ptr<const LogEvent>;

    /***
     * @brief constructor
     * @param logger logger
     * @param level log level
     * @param wrapped_msg wrapped message with local source location
     */
    explicit LogEvent(
        Logger::Ptr logger,
        LogLevel::level level,
        LocalSourceLocation<std::string> wrapped_msg
    );

    /***
     * @brief get log level
     * @return log level
     */
    inline constexpr LogLevel::level getLogLevel() const noexcept
    {
        return level_;
    }

    /***
     * @brief get log level in type of `std::string`
     * @return log level in type of `std::string`
     */
    inline constexpr std::string getLogLevelString() const noexcept
    {
        return LogLevel::to_string(level_);
    }

    /***
     * @brief get timestamp
     * @return timestamp
     */
    inline const auto getTimestamp() const noexcept
        -> std::chrono::local_time<std::chrono::system_clock::duration>
    {
        return timestamp_.get_local_time();
    }

    /***
     * @brief get source location
     * @return source location
     */
    inline const std::source_location& getSourceLocation() const noexcept
    {
        return wrapped_msg_.getLocation();
    }

    /***
     * @brief get input message
     */
    inline constexpr std::string getMsg() const noexcept
    {
        return wrapped_msg_.getData();
    }

    /***
     * @brief get thread id from thread local storage
     * @return thread id from thread local storage
     * @details copied from [spdlog](https://github.com/gabime/spdlog)
     */
    inline size_t getThreadId() const noexcept;

    /***
     * @brief get logger
     * @return logger
     */
    inline const Logger::Ptr getLogger() const noexcept
    {
        return logger_;
    }

private:
    /***
     * @brief logger
     */
    Logger::Ptr logger_;

    /***
     * @brief level
     */
    LogLevel::level level_;

    /***
     * @brief timestamp
     * @note this is timestamp from your time zone
     */
    std::chrono::zoned_time<std::chrono::system_clock::duration> timestamp_;

    /***
     * @brief wrapped message includes source location and input message
     */
    LocalSourceLocation<std::string> wrapped_msg_;

    /***
     * @brief thread id
     */
    size_t thread_id_;

    /***
     * @brief get thread id
     * @return thread id
     * @details copied from [spdlog](https://github.com/gabime/spdlog)
     */
    inline size_t _getThreadId() const noexcept;
};

/***
 * @brief wrapped class for `LogEvent` class
 * @details this class is used to pass the wrapped message to logger via destructor as a temporary object
 * @details inspired by [sylar logger](https://github.com/sylar-yin/sylar)
 */
class LogEventWrap {
public:
    /***
     * @brief constructor
     */
    LogEventWrap(const LogEvent::Ptr& event): event_(event) {}

    /***
     * @brief destructor to pass wrapped message to logger
     * @details thanks to RAII, when `LogEventWrap` is destructed, the wrapped message is passed to logger
     */
    ~LogEventWrap()
    {
        if (event_ != nullptr)
            event_->getLogger()->submit(event_);
    }

private:
    /***
    * @brief log event
    */
    LogEvent::Ptr event_;
};
} // namespace aw_logger

// aw_logger library
#include "impl/log_event_impl.hpp"

#endif //! LOG_EVENT_HPP
