#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace httplib { class Server; }
class MatchCreator;
class RoomsService;

// WS Gateway/shard's internal HTTP listener -- exposes match creation to
// whatever process is running Matchmaker (HttpMatchCreator on that side),
// relays "no match found" pushes from Matchmaker back into this process's own
// MatchQueue implementation (see HttpMatchQueue::notifyNoMatchFound), and
// exposes room creation/joining to ApiGateway (HttpRoomsClient on that side).
// Distinct from any public-facing API; only meant to be reachable by other
// server processes on the internal network.
class ShardInternalServer {
public:
    using NoMatchFoundRelay = std::function<void(const std::string& username)>;

    ShardInternalServer(int port, MatchCreator& matchCreator, RoomsService& roomsService, NoMatchFoundRelay onNoMatchFound);
    ~ShardInternalServer();

    /// Non-blocking: binds the port and starts listening on a background thread.
    bool start();
    void stop();

private:
    MatchCreator& matchCreator_;
    RoomsService& roomsService_;
    NoMatchFoundRelay onNoMatchFound_;
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread listenThread_;
};
