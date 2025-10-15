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

#ifndef APPENDER_HPP
#define APPENDER_HPP

// C++ standard library
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <syncstream>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/formatter.hpp"
#include "aw_logger/log_event.hpp"

//uWebSockets library

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief base appender class to output formatted log messages
 * @note inspired by [sinks in spdlog](https://github.com/gabime/spdlog/tree/v1.x/include/spdlog/sinks)
 */
class BaseAppender {
public:
    using Ptr = std::shared_ptr<BaseAppender>;
    using ConstPtr = std::shared_ptr<const BaseAppender>;

    /***
     * @brief constructor
     */
    explicit BaseAppender()
    {
        auto factory = std::make_shared<ComponentFactory>();
        formatter_ = std::make_shared<Formatter>(factory);
    }
    ~BaseAppender() = default;

    BaseAppender(const BaseAppender&) = delete;
    BaseAppender(BaseAppender&&) = delete;
    BaseAppender& operator=(const BaseAppender&) = delete;
    BaseAppender& operator=(BaseAppender&&) = delete;

    /***
     * @brief virtual append function
     * @param event log event
     */
    virtual void append(const LogEvent::Ptr& event) = 0;

    /***
     * @brief set formatter to appender
     * @param formatter formatter to be set
     */
    void setFormatter(const Formatter::Ptr& formatter)
    {
        std::lock_guard<std::mutex> lk(app_mtx_);
        formatter_ = formatter;
    }

protected:
    /***
     * @brief formatter
     */
    Formatter::Ptr formatter_;

    /***
     * @brief appender mutex
     */
    std::mutex app_mtx_;

    /***
     * @brief formatter mutex
     */
    std::mutex fmt_mtx_;

    /***
     * @brief format log message
     * @param event log event
     */
    std::string formatMsg(const LogEvent::Ptr& event)
    {
        std::lock_guard<std::mutex> lk(fmt_mtx_);
        if (formatter_ != nullptr && event != nullptr)
            return formatter_->formatComponents(event, formatter_->getRegisteredComponents());
        else
        {
            throw aw_logger::invalid_parameter("formatter or event is nullptr!");
        }
    }
};

/***
 * @brief console appender class which output to console directly
 */
class ConsoleAppender: public BaseAppender {
public:
    explicit ConsoleAppender(std::string_view stream_type = "stdout"): sync_stream_(std::cout)
    {
        if (stream_type == "stdout")
            sync_stream_ = std::osyncstream(std::cout);
        else if (stream_type == "stderr")
            sync_stream_ = std::osyncstream(std::cerr);
        else
            throw aw_logger::invalid_parameter(
                "invalid stream type, please use 'stdout' or 'stderr'."
            );
    }

    virtual void append(const LogEvent::Ptr& event) override;

private:
    /***
     * @brief synchronized output stream
     */
    std::osyncstream sync_stream_;
};

/***
 * @brief rolling file appender class which based on size-and-time policy
 */
class FileAppender: public BaseAppender {
private:
    size_t max_size_;
    size_t max_time_;
};

class WebsocketAppender: public BaseAppender {};
} // namespace aw_logger

#endif //! APPENDER_HPP
