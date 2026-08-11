#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <ixwebsocket/IXWebSocketServer.h>

#include "../auth/auth_service.hpp"
#include "../lobby/lobby_registry.hpp"
#include "../lobby/match_queue.hpp"
#include "../matches/game_allocator.hpp"
#include "../matches/shard_lookup.hpp"
#include "../db/player_account_store.hpp"
#include "../player_registry.hpp"
#include "../rooms/rooms_service.hpp"

class GameMatch;
struct LoginRequest;
struct JoinRoomRequest;

class GameWebSocketServer {
public:
    /// Creates and configures the WebSocket server.
    GameWebSocketServer(int port, LobbyRegistry& lobbyRegistry, ShardLookup& gameRegistry, GameAllocator& gameAllocator,
                  MatchQueue& matchQueue, PlayerAccountStore& accounts, Board templateBoard);

    /// Starts listening for incoming WebSocket connections.
    bool start();
    /// Blocks until the WebSocket server stops.
    void wait();

    /// Attaches any player Matchmaker has just paired to their new match, so they start receiving that match's broadcasts.
    /// Call once per server tick, right. after Matchmaker::tick().
    void attachNewlyMatchedConnections();

private:
    struct ConnectionState {
        LobbyRegistry::ConnectionId lobbyId;
        std::shared_ptr<ix::WebSocket> socket;
        std::optional<std::string> username;
        GameMatch* match = nullptr;
        PlayerId matchPlayerId = 0;
    };
    
    void handleLogin(ix::WebSocket* rawConnection, ConnectionState& state, const LoginRequest& request);
    void handleCreateRoom(ConnectionState& state);
    void handleJoinRoom(ConnectionState& state, const JoinRoomRequest& request);
    void handleFindMatch(ix::WebSocket* rawConnection, ConnectionState& state);
    void handleGameMessage(ConnectionState& state, const std::string& rawMessage);
    void handleClose(ix::WebSocket* rawConnection, ConnectionState& state);
    void attachToMatch(ConnectionState& state, GameMatch* match, bool isResume);

    LobbyRegistry& lobbyRegistry_;
    ShardLookup& gameRegistry_;
    MatchQueue& matchQueue_;
    PlayerAccountStore& accounts_;
    Board templateBoard_;
    AuthService authService_;
    RoomsService roomsService_;

    std::mutex connectionsMutex_;
    std::unordered_map<ix::WebSocket*, std::shared_ptr<ConnectionState>> connections_;
    std::unordered_map<std::string, ix::WebSocket*> waitingByUsername_;

    // Declared last so its background per-connection threads (which touch the
    // members above via captured-by-reference callbacks) are stopped and
    // joined before those members are destroyed.
    ix::WebSocketServer server_;
};
