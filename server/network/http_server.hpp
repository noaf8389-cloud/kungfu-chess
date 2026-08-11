#pragma once

#include <memory>
#include <thread>

#include "../matches/game_history_store.hpp"
#include "../rooms/rooms_client.hpp"

namespace httplib { class Server; }
class PlayerAccountStore;

// The API Gateway's seed: a real HTTP/REST listener, separate from the WebSocket
// server, exposing PlayerAccountStore/GameHistoryStore operations that make sense
// without a live connection (credential check, rating lookup, match history),
// plus room creation/joining (proxied to WsGateway's internal API via
// RoomsClient — GameRegistry/RoomsService live there, not here). Deliberately
// does NOT expose AuthService's lobby-identify/resume-match behavior — that
// still only makes sense once a WebSocket connection exists to attach to;
// see Implementation_Roadmap.md Phase 1 Decision Log (rooms-over-REST itself
// is Phase 8's "closing the gap" addendum).
class HttpServer {
public:
    // Default: owns its own NoOpGameHistoryStore (GET .../history always empty)
    // and NoOpRoomsClient (POST /rooms* always fails with 503).
    HttpServer(int port, PlayerAccountStore& accounts);
    // Injected: GET .../history is served from a real GameHistoryStore; owns its own NoOpRoomsClient still.
    HttpServer(int port, PlayerAccountStore& accounts, GameHistoryStore& historyStore);
    // All injected: POST /rooms* is served from a real RoomsClient.
    HttpServer(int port, PlayerAccountStore& accounts, GameHistoryStore& historyStore, RoomsClient& roomsClient);
    ~HttpServer();

    /// Non-blocking: binds the port and starts listening on a background thread.
    bool start();
    /// Blocks until the HTTP server stops.
    void wait();
    void stop();

private:
    void registerRoutes();

    PlayerAccountStore& accounts_;
    std::unique_ptr<GameHistoryStore> ownedHistoryStore_;
    GameHistoryStore& historyStore_;
    std::unique_ptr<RoomsClient> ownedRoomsClient_;
    RoomsClient& roomsClient_;
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread listenThread_;
};
