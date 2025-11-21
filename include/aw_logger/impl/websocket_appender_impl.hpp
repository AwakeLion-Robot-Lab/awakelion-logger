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

#ifndef IMPL__WEBSOCKET_APPENDER_IMPL_HPP
#define IMPL__WEBSOCKET_APPENDER_IMPL_HPP

// C++ standard library
#include <functional>

// nlohmann JSON library
#include <nlohmann/json.hpp>

// aw_logger library
#include "aw_logger/appender.hpp"
#include "aw_logger/settings_path.h"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
WebsocketAppender::WebsocketAppender()
{
    /* parse and load settings from config file */
    const std::string setting_path = SETTINGS_FILE_PATH;
    loadWebsocketConfig(setting_path);

    /* initialize the websocket client */
    init();

    /* make connection to server */
    connect();
}

WebsocketAppender::WebsocketAppender(
    const std::string_view url,
    const bool message_deflate_en,
    const int ping_interval,
    const int handshake_timeout
):
    url_(url),
    message_deflate_en_(message_deflate_en),
    ping_interval_(ping_interval),
    handshake_timeout_(handshake_timeout)
{
    init();
    connect();
}

aw_logger::WebsocketAppender::~WebsocketAppender()
{
    if (!connected_.load())
        return;

    std::lock_guard<std::mutex> ws_lk(ws_mtx_);
    ws_.stop();
    connected_.store(false);
}

void aw_logger::WebsocketAppender::append(const LogEvent::Ptr& event)
{
    /* check status of log level */
    auto const curr_level = getThresholdLevel();
    if (!connected_.load() || event->getLogLevel() < curr_level)
        return;

    /* create json for message serialization */
    nlohmann::json log_msg_json;
    {
        std::lock_guard<std::mutex> app_lk(app_mtx_);
        const auto& components = formatter_->getRegisteredComponents();

        for (const auto& [key, _]: components)
        {
            if (key == "timestamp")
                log_msg_json["timestamp"] = event->getTimestamp().time_since_epoch().count();
            else if (key == "level")
                log_msg_json["level"] = event->getLogLevelString();
            else if (key == "tid")
                log_msg_json["tid"] = event->getThreadId();
            else if (key == "file_name")
                log_msg_json["file_name"] = event->getSourceLocation().file_name();
            else if (key == "function_name")
                log_msg_json["function_name"] = event->getSourceLocation().function_name();
            else if (key == "line")
                log_msg_json["line"] = event->getSourceLocation().line();
            else if (key == "msg")
                log_msg_json["msg"] = event->getMsg();
        }
    }

    /* send to server */
    std::string log_msg_str = log_msg_json.dump();
    {
        std::lock_guard<std::mutex> ws_lk(ws_mtx_);
        auto res = ws_.sendUtf8Text(log_msg_str);
        if (!res.success)
        {
            std::cerr << "websocket send log message failed, payload size: " << res.payloadSize
                      << ", wire size: " << res.wireSize << std::endl;
        }
    }
}

void aw_logger::WebsocketAppender::flush() {}

void WebsocketAppender::init()
{
    ws_.setUrl(url_);
    ix::WebSocketPerMessageDeflateOptions deflate_options(message_deflate_en_);
    ws_.setPerMessageDeflateOptions(deflate_options);
    ws_.setPingInterval(ping_interval_);
    ws_.setHandshakeTimeout(handshake_timeout_);
    ws_.enableAutomaticReconnection();
    ws_.setOnMessageCallback(
        std::bind(&WebsocketAppender::on_message, this, std::placeholders::_1)
    );
}

void aw_logger::WebsocketAppender::connect()
{
    if (connected_.load(std::memory_order_acquire))
        return;

    std::lock_guard<std::mutex> ws_lk(ws_mtx_);
    ws_.start();
}

void aw_logger::WebsocketAppender::on_message(const ix::WebSocketMessagePtr& msg)
{
    auto msg_type = msg->type;
    if (msg_type == ix::WebSocketMessageType::Open)
    {
        std::cout << "client connected to: " << url_ << std::endl;
        connected_.store(true);
    }
    else if (msg_type == ix::WebSocketMessageType::Close)
    {
        std::cout << "server closed: [code: " << msg->closeInfo.code << "] "
                  << msg->closeInfo.reason << std::endl;
        connected_.store(false);
    }
    else if (msg_type == ix::WebSocketMessageType::Error)
    {
        auto const error_info = msg->errorInfo;
        std::cerr << "websocket error: " << error_info.reason
                  << "\n retries: " << error_info.retries
                  << "\n wait_time(ms): " << error_info.wait_time
                  << "\n HTTP_status: " << error_info.http_status << std::endl;
        connected_.store(false);
    }
    else if (msg_type == ix::WebSocketMessageType::Ping
             || msg_type == ix::WebSocketMessageType::Pong)
    {
        std::cout << "client ping/pong from: " << msg->openInfo.uri << std::endl;
    }
    else if (msg_type == ix::WebSocketMessageType::Message)
    {
        auto const rx_msg = msg->str;
        std::cout << "client received message: " << rx_msg << std::endl;
        // this->setThresholdLevel(LogLevel::from_string(rx_msg));
    }
}

void WebsocketAppender::loadWebsocketConfig(std::string_view file_name)
{
    auto const file_name_s = std::string(file_name);
    std::ifstream config_file(file_name_s);

    if (!config_file.is_open())
        throw aw_logger::invalid_parameter(std::string("cannot open config file: ") + file_name_s);

    nlohmann::json config_json = nlohmann::json::parse(config_file);
    config_file.close();

    if (!config_json.contains("websocket") || !config_json["websocket"].is_array()
        || config_json["websocket"].empty())
        throw aw_logger::invalid_parameter("websocket config not found in JSON!");

    auto const ws_config = config_json["websocket"][0];

    if (ws_config.contains("url"))
        url_ = ws_config["url"].get<std::string>();

    if (ws_config.contains("message_deflate_en"))
        message_deflate_en_ = ws_config["message_deflate_en"].get<bool>();

    if (ws_config.contains("ping_interval"))
        ping_interval_ = ws_config["ping_interval"].get<int>();

    if (ws_config.contains("handshake_timeout"))
        handshake_timeout_ = ws_config["handshake_timeout"].get<int>();
}
} // namespace aw_logger

#endif //! IMPL__WEBSOCKET_APPENDER_IMPL_HPP
