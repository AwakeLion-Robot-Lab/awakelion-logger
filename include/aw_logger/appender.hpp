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
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <syncstream>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/formatter.hpp"
#include "aw_logger/log_event.hpp"

// IXWebSocket library
#include <ixwebsocket/IXWebSocket.h>

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
        auto factory = std::make_unique<ComponentFactory>();
        formatter_ = std::make_unique<Formatter>(std::move(factory));
    }

    /***
     * @brief constructor with formatter
     * @param formatter formatter
     */
    explicit BaseAppender(Formatter::Ptr formatter): formatter_(std::move(formatter)) {}

    /***
     * @brief destructor
     */
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
     * @brief flush output buffer
     */
    virtual void flush() = 0;

    /***
     * @brief set formatter to appender
     * @param formatter formatter to be set
     */
    void setFormatter(Formatter::Ptr formatter)
    {
        std::lock_guard<std::mutex> lk(app_mtx_);
        formatter_ = std::move(formatter);
    }

protected:
    /***
     * @brief formatter
     */
    Formatter::Ptr formatter_;

    /***
     * @brief appender mutex
     */
    mutable std::mutex app_mtx_;

    /***
     * @brief formatter mutex
     */
    mutable std::mutex fmt_mtx_;

    /***
     * @brief format log message
     * @param event log event
     */
    std::string formatMsg(const LogEvent::Ptr& event)
    {
        std::lock_guard<std::mutex> lk(fmt_mtx_);
        if (formatter_ != nullptr && event != nullptr)
            return formatter_->formatComponents(event, formatter_->getRegisteredComponents());
        else if (formatter_ == nullptr)
        {
            throw aw_logger::invalid_parameter("formatter is nullptr!");
        }
        else
        {
            throw aw_logger::invalid_parameter("event is nullptr!");
        }
    }
};

/***
 * @brief console appender class which output to console directly via `std::osyncstream`
 */
class ConsoleAppender final: public BaseAppender {
public:
    /***
     * @brief construtor
     * @param stream_type stream type, "stdout" - `std::cout` | "stderr" - `std::cerr`
     */
    explicit ConsoleAppender(std::string_view stream_type = "stdout");

    /***
     * @brief constructor with formatter
     * @param formatter formatter
     * @param stream_type stream type, "stdout" - `std::cout` | "stderr" - `std::cerr`
     */
    explicit ConsoleAppender(Formatter::Ptr formatter, std::string_view stream_type = "stdout");

    /***
     * @brief append to console
     * @param event log event
     */
    virtual void append(const LogEvent::Ptr& event) override;

    /***
     * @brief flush output stream
     */
    virtual void flush() override
    {
        /* use `std::osyncstream` to ensure thread-safe */
        std::osyncstream(output_stream_) << std::flush;
    }

private:
    /***
     * @brief reference to output stream，support `std::cout` and `std::cerr`
     */
    std::ostream& output_stream_;

    /***
     * @brief get output stream type
     * @param stream_type stream type
     * @return reference to std::cout or std::cerr
     */
    inline static std::ostream& getStreamType(std::string_view stream_type);
};

/***
 * @brief rolling file appender class which based on size policy
 */
class FileAppender final: public BaseAppender {
public:
    /***
     * @brief constructor
     * @param file_path path to log file
     * @param is_trunc flag for truncate file for its old logs
     * @param buffer_capacity buffer capacity of memory buffer
     */
    explicit FileAppender(
        std::string_view file_path,
        bool is_trunc = false,
        size_t buffer_capacity = 8192
    );

    /***
     * @brief constructor with formatter
     * @param formatter formatter
     * @param file_path path to log file
     * @param is_trunc flag for truncate file for its old logs
     * @param buffer_capacity buffer capacity of memory buffer
     */
    explicit FileAppender(
        Formatter::Ptr formatter,
        std::string_view file_path,
        bool is_trunc = false,
        size_t buffer_capacity = 8192
    );

    /***
     * @brief destructor - ensures buffer is flushed
     */
    ~FileAppender();

    /***
     * @brief append to file with buffering
     * @param event log event
     */
    virtual void append(const LogEvent::Ptr& event) override;

    /***
     * @brief flush buffer to file
     */
    virtual void flush() override;

    /***
     * @brief set max file size for rolling
     * @param max_size max file size in bytes
     */
    void setMaxFileSize(size_t max_size) noexcept
    {
        max_file_size_ = max_size;
    }

    /***
     * @brief set max backup file number
     * @param max_num max number of backup files
     */
    void setMaxBackupNum(size_t max_num) noexcept
    {
        max_backup_num_ = max_num;
    }

    /***
     * @brief get current file size
     * @return current file size in bytes
     */
    inline size_t getFileSize() const noexcept
    {
        return file_size_;
    }

    /***
     * @brief reopen file
     * @param is_trunc truncate mode
     */
    void reopen(bool is_trunc = false);

private:
    /***
     * @brief file stream for log output
     */
    std::ofstream file_stream_;

    /***
     * @brief log file path
     */
    std::filesystem::path file_path_;

    /***
     * @brief string buffer for log message
     */
    std::string buffer_;

    /***
     * @brief current file size
     */
    size_t file_size_;

    /***
     * @brief max file size for rotation
     * @details 0 means no size limit
     */
    size_t max_file_size_;

    /***
     * @brief max number of backup files
     * @details 0 means no size limit
     */
    size_t max_backup_num_;

    /***
     * @brief flag for truncate file for its old logs
     */
    bool is_trunc_;

    /***
     * @brief open file
     * @param is_trunc truncate mode
     */
    void open(bool is_trunc);

    /***
     * @brief flush log messages to buffer
     */
    void flushToBuffer();

    /***
     * @brief rotate log file while the current size is greater than max file size
     */
    void rotateFile();

    /***
     * @brief create backup file path
     * @param index backup index
     * @return backup file path
     */
    std::filesystem::path createBackupPath(size_t index) const noexcept;
};

/***
 * @brief websocket appender class which output to websocket server via `IXWebSocket`
 * @details API reference: https://machinezone.github.io/IXWebSocket/usage/
 */
class WebsocketAppender final: public BaseAppender {
public:
    /***
     * @brief constructor
     * @param url websocket server url
     * @param message_deflate_en enable message deflate compression
     * @param ping_interval ping interval in seconds
     * @param connect_timeout connection timeout in seconds
     */
    explicit WebsocketAppender(
        std::string_view url,
        bool message_deflate_en = false,
        int ping_interval = 30,
        int connect_timeout = 5
    );

    /***
     * @brief destructor
     */
    ~WebsocketAppender();

    /***
     * @brief append log event to websocket server
     * @param event log event
     */
    virtual void append(const LogEvent::Ptr& event) override;

    /***
     * @brief flush buffer
     */
    virtual void flush() override;

private:
    /***
     * @brief websocket client
     */
    ix::WebSocket ws_;

    /***
     * @brief flag for connection status
     */
    std::atomic<bool> connected_ { false };

    /***
     * @brief mutex for websocket client
     */
    mutable std::mutex ws_mtx_;

    /***
     * @brief url of websocket server
     */
    std::string url_;

    /***
     * @brief enable message deflate compression
     */
    bool message_deflate_en_;

    /***
     * @brief ping interval in seconds
     */
    int ping_interval_;

    /***
     * @brief handshake timeout in seconds
     */
    int handshake_timeout_;

    /***
     * @brief callback function for websocket message
     * @param msg websocket message
     */
    void on_message(const ix::WebSocketMessagePtr& msg);

    /***
     * @brief connect to websocket server
     */
    void connect();
};
} // namespace aw_logger

#endif //! APPENDER_HPP
