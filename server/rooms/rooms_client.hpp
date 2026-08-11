#pragma once

#include <optional>
#include <string>

// Abstraction over "create/join a room on whichever process actually owns
// GameRegistry/RoomsService" -- ApiGateway talks to WsGateway's internal API
// through this, the same shape as MatchCreator/MatchQueue for
// Matchmaker<->WsGateway. Never called from a WebSocket connection itself
// (GameWebSocketServer talks to its own in-process RoomsService directly).
class RoomsClient {
public:
    virtual ~RoomsClient() = default;

    // Returns the new room's code, or std::nullopt if the request failed
    // (e.g. WsGateway unreachable).
    virtual std::optional<std::string> createRoom(const std::string& username) = 0;

    // True only if roomCode is known and the join succeeded.
    virtual bool joinRoom(const std::string& username, const std::string& roomCode) = 0;
};
