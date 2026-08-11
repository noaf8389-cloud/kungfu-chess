#pragma once

#include <memory>
#include <thread>

namespace httplib { class Server; }
class Matchmaker;

// Matchmaker's internal HTTP listener -- exposes enqueue/dequeue to whatever
// process is running the WS Gateway/shard (HttpMatchQueue on that side).
// Distinct from any public-facing API; only meant to be reachable by other
// server processes on the internal network.
class MatchmakerInternalServer {
public:
    MatchmakerInternalServer(int port, Matchmaker& matchmaker);
    ~MatchmakerInternalServer();

    /// Non-blocking: binds the port and starts listening on a background thread.
    bool start();
    void stop();

private:
    Matchmaker& matchmaker_;
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread listenThread_;
};
