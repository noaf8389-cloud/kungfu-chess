#pragma once

#include <string>

class Server {
public:
    // Stores the board layout path and the WebSocket/HTTP ports to use once run() is called.
    Server(const std::string& layoutPath, int wsPort, int httpPort)
        : layoutPath_(layoutPath), wsPort_(wsPort), httpPort_(httpPort) {}

    /// Starts the server and runs the main update loop.
    int run();

private:
    std::string layoutPath_;
    int wsPort_;
    int httpPort_;
};
