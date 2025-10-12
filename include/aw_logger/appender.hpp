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

#ifndef BASE_APPENDER_HPP
#define BASE_APPENDER_HPP

// C++ standard library
#include <syncstream>

// aw_logger library
#include "aw_logger/log_event.hpp"
#include "aw_logger/logger.hpp"

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
 */
class BaseAppender {
public:
    using Ptr = std::shared_ptr<BaseAppender>;
    using ConstPtr = const std::shared_ptr<BaseAppender>;

    /***
     * @brief constructor
     * @param flushInterval flush interval in milliseconds
     */
    BaseAppender(int flushInterval);

    /***
     * @brief virtual ouput function
     * @param logger logger
     * @param msg formatted log message
     */
    virtual void output(Logger::ConstPtr& logger, std::string_view msg) = 0;

private:
    /***
     * @brief appender mutex
     */
    std::mutex app_mtx_;

    /***
     * @brief synchronized output stream
     */
    std::osyncstream sync_out_;

    /***
     * @brief flush interval
     * @note milliseconds, `0` means no flush
     */
    int flushInterval_;
};

class ConsoleAppender: public BaseAppender {};

class FileAppender: public BaseAppender {};

class WebsocketAppender: public BaseAppender {};
} // namespace aw_logger

#endif //! BASE_APPENDER_HPP
