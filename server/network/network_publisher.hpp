#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include <ixwebsocket/IXWebSocket.h>

#include "bus/event_bus.hpp"
#include "../player_registry.hpp"

class GameSession;

class NetworkPublisher {
public:
    /// Creates a publisher; subscribes to broadcast events and asks gameSession for a snapshot on new connections.
    NetworkPublisher(EventBus& bus, GameSession& gameSession, PlayerRegistry& players);

    /// Adds a client connection, sends it the current board as a GameStartedEvent, and registers it for future broadcasts.
    void addConnection(std::shared_ptr<ix::WebSocket> connection);

    /// Removes a disconnected client connection.
    void removeConnection(ix::WebSocket* connection);

private:
    void broadcast(const std::string& message);

    EventBus& bus_;
    GameSession& gameSession_;
    PlayerRegistry& players_;
    std::recursive_mutex connectionsMutex_;
    std::vector<std::shared_ptr<ix::WebSocket>> connections_;
};
