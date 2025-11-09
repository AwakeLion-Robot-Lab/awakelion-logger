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

// aw_logger library
#include "aw_logger/appender.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
WebsocketAppender::WebsocketAppender(
    std::string_view url,
    bool message_deflate_en,
    int ping_interval,
    int handshake_timeout
):
    url_(url),
    message_deflate_en_(message_deflate_en),
    ping_interval_(ping_interval),
    handshake_timeout_(handshake_timeout)
{
    /* websocket client configuration */
    ws_.setUrl(url_);
    ix::WebSocketPerMessageDeflateOptions deflate_options(message_deflate_en_);
    ws_.setPerMessageDeflateOptions(deflate_options);
    ws_.setPingInterval(ping_interval_);
    ws_.setHandshakeTimeout(handshake_timeout_);
    ws_.enableAutomaticReconnection();
    ws_.setOnMessageCallback(
        std::bind(&WebsocketAppender::on_message, this, std::placeholders::_1)
    );

    connect();
}

aw_logger::WebsocketAppender::~WebsocketAppender()
{
    if (connected_.load())
        return;

    std::lock_guard<std::mutex> ws_lk(ws_mtx_);
    ws_.stop();
    connected_.store(false);
}

void aw_logger::WebsocketAppender::on_message(const ix::WebSocketMessagePtr& msg)
{
    auto msg_type = msg->type;
    if (msg_type == ix::WebSocketMessageType::Open)
    {
        connected_.store(true);
    }
    else if (msg_type == ix::WebSocketMessageType::Close)
    {
        std::cout << "connection closed via: [code: " << msg->closeInfo.code << "] "
                  << msg->closeInfo.reason << std::endl;
        connected_.store(false);
    }
    else if (msg_type == ix::WebSocketMessageType::Error)
    {
        auto error_info = msg->errorInfo;
        std::cerr << "connection error: " << error_info.reason << " retries: " << error_info.retries
                  << " wait time(ms): " << error_info.wait_time
                  << "HTTP status: " << error_info.http_status << std::endl;

        connected_.store(false);
    }
    else if (msg_type == ix::WebSocketMessageType::Ping
             || msg_type == ix::WebSocketMessageType::Pong)
    {
        std::cout << "received ping from: " << msg->openInfo.uri << std::endl;
    }
    else if (msg_type == ix::WebSocketMessageType::Message)
    {
        // TODO(siyiya): handle threshold level applied to websocket appender
    }
}

void aw_logger::WebsocketAppender::connect()
{
    if (!connected_.load())
        return;

    std::lock_guard<std::mutex> ws_lk(ws_mtx_);
    ws_.start();
    if (ws_.getReadyState() == ix::ReadyState::Open)
    {
        std::cout << "connected to websocket server: " << url_ << std::endl;
        connected_.store(true);
    }
}
} // namespace aw_logger

#endif //! IMPL__WEBSOCKET_APPENDER_IMPL_HPP
