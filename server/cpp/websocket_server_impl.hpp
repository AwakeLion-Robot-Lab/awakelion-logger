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

#ifndef SERVER__WEBSOCKET_SERVER_IMPL_HPP
#define SERVER__WEBSOCKET_SERVER_IMPL_HPP

// C++ standard library
#include <iostream>
#include <memory>

// nlohmann JSON library
#include <nlohmann/json.hpp>

// aw_logger library
#include "websocket_server.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
WebSocketServer::WebSocketServer(const int port, const std::string_view host):
    wss_(port, std::string(host))
{}

WebSocketServer::~WebSocketServer()
{
    wss_.stop();
}

void WebSocketServer::run()
{
    // clang-format off
    wss_.setOnClientMessageCallback(
        std::bind(
            &WebSocketServer::on_client_message,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        )
    );
    // clang-format on

    auto [res, err_msg] = wss_.listen();
    if (!res)
    {
        std::cerr << "webSocket server listen failed: " << err_msg << std::endl;
    }

    wss_.start();
    std::cout << "\033[34m websocket server listening on " << wss_.getHost() << ":"
              << wss_.getPort() << "\033[0m" << std::endl;
    wss_.wait();
}

void WebSocketServer::on_client_message(
    std::shared_ptr<ix::ConnectionState> cs,
    ix::WebSocket& ws,
    const ix::WebSocketMessagePtr& msg
)
{
    auto const msg_type = msg->type;
    if (msg->type == ix::WebSocketMessageType::Open)
    {
        std::cout << "\033[32m received new connection from: " << cs->getRemoteIp()
                  << " (ID: " << cs->getId() << ")\033[0m" << std::endl;
    }
    else if (msg->type == ix::WebSocketMessageType::Close)
    {
        std::cout << "\033[33m connection closed (ID: " << cs->getId() << ")\033[0m" << std::endl;
    }
    else if (msg->type == ix::WebSocketMessageType::Error)
    {
        std::cerr << "\033[31m websocket error: " << msg->errorInfo.reason << "\033[0m"
                  << std::endl;
    }
    else if (msg->type == ix::WebSocketMessageType::Message)
    {
        for (auto&& client: wss_.getClients())
        {
            if (client.get() != &ws)
            {
                if (msg->binary)
                    client->sendBinary(msg->str);
                else
                    client->sendUtf8Text(msg->str);
            }
        }
    }
}
} // namespace aw_logger

#endif //! SERVER__WEBSOCKET_SERVER_IMPL_HPP
