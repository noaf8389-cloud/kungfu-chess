#pragma once

#include "rooms_client.hpp"

// Safe default: every call fails cleanly (no WsGateway to talk to). Matches
// NoOpGameHistoryStore/NatsPublisher's disconnected-default philosophy.
class NoOpRoomsClient : public RoomsClient {
public:
    std::optional<std::string> createRoom(const std::string& username) override;
    bool joinRoom(const std::string& username, const std::string& roomCode) override;
};
