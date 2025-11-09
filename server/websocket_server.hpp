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

#ifndef SERVER__WEBSOCKET_SERVER_HPP
#define SERVER__WEBSOCKET_SERVER_HPP

// aw_logger library

// IXWebSocket library
#include <ixwebsocket/IXWebSocketServer.h>

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
/***
 * @brief websocket server for remote logging powered by `IXWebSocket`
 */
class WebSocketServer {
public:
    /***
     * @brief constructor
     * @param port listening port
     * @param host listening host
     */
    WebSocketServer(int port = 9002, const std::string& host = "localhost");

private:
    /***
     * @brief websocket server
     */
    ix::WebSocketServer ws_server_;
};
} // namespace aw_logger

#endif //! SERVER__WEBSOCKET_SERVER_HPP
