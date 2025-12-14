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

#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

// C++ standard library
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief base aw_logger exception class
 */
class aw_logger_exception: public std::exception {
public:
    explicit aw_logger_exception(std::string_view msg): msg_(msg) {}

    const char* what() const noexcept override
    {
        return msg_.c_str();
    }

protected:
    /***
     * @brief error message
     */
    std::string msg_;
};

/***
 * @brief invalid parameter exception
 */
class invalid_parameter final: public aw_logger_exception {
public:
    explicit invalid_parameter(std::string_view msg):
        aw_logger_exception("[aw_logger]: invalid parameter: " + std::string(msg))
    {}
};

/***
 * @brief ringbuffer exception
 */
class ringbuffer_exception final: public aw_logger_exception {
public:
    explicit ringbuffer_exception(std::string_view msg):
        aw_logger_exception("[aw_logger]: invalid parameter: " + std::string(msg))
    {}
};

/***
 * @brief bad json exception
 */
class bad_json final: public aw_logger_exception {
public:
    explicit bad_json(std::string_view msg):
        aw_logger_exception("[aw_logger]: invalid parameter: " + std::string(msg))
    {}
};

/***
 * @brief websocket exception
 */
class websocket_exception final: public aw_logger_exception {
public:
    explicit websocket_exception(std::string_view msg):
        aw_logger_exception("[aw_logger]: websocket error: " + std::string(msg))
    {}
};

} // namespace aw_logger

#endif //! EXCEPTION_HPP
