#pragma once

#include <string>

class GameMatch;

// Abstraction over "find an existing match" -- satisfied by both a single
// GameRegistry (today, and every existing call site: it just IS-A
// ShardLookup, no code changes needed there) and a ShardRegistry that
// searches across multiple GameRegistry shards (Phase 6). Callers that only
// need lookup -- AuthService's resume check, RoomsService::joinRoom,
// GameWebSocketServer::attachNewlyMatchedConnections -- depend on this
// interface instead of a concrete GameRegistry, so they don't need to know
// or care how many shards exist behind it.
class ShardLookup {
public:
    using RoomCode = std::string;

    virtual ~ShardLookup() = default;

    virtual GameMatch* matchFor(const std::string& username) = 0;
    virtual GameMatch* joinRoom(const RoomCode& roomCode, const std::string& joinerUsername) = 0;
    virtual void updateAll(long long deltaMs) = 0;
};
