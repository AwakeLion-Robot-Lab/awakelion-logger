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

// aw_logger library
#include "websocket_server_impl.hpp"

int main(int argc, char** argv)
{
    int port;
    std::string host;
    if (argc > 2)
    {
        port = std::stoi(argv[1]);
        host = argv[2];
    }
    else
    {
        port = 1234;
        host = "0.0.0.0";
    }

    aw_logger::WebSocketServer server(port, host);
    server.run();

    return 0;
}
