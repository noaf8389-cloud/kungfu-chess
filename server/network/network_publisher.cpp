#include "network_publisher.hpp"

#include <algorithm>

#include "bus/event_serialization.hpp"
#include "../game_session.hpp"

NetworkPublisher::NetworkPublisher(EventBus& bus, GameSession& gameSession, PlayerRegistry& players)
    : bus_(bus), gameSession_(gameSession), players_(players) {
    bus_.subscribe<MoveMadeEvent>([this](const MoveMadeEvent& e) {
        broadcast(toEnvelopeJson("MoveMadeEvent", e));
    });
    bus_.subscribe<PieceCapturedEvent>([this](const PieceCapturedEvent& e) {
        broadcast(toEnvelopeJson("PieceCapturedEvent", e));
    });
    bus_.subscribe<GameOverEvent>([this](const GameOverEvent& e) {
        broadcast(toEnvelopeJson("GameOverEvent", e));
    });
    bus_.subscribe<ScoreUpdatedEvent>([this](const ScoreUpdatedEvent& e) {
        broadcast(toEnvelopeJson("ScoreUpdatedEvent", e));
    });
    bus_.subscribe<MoveStartedEvent>([this](const MoveStartedEvent& e) {
        broadcast(toEnvelopeJson("MoveStartedEvent", e));
    });
    bus_.subscribe<JumpStartedEvent>([this](const JumpStartedEvent& e) {
        broadcast(toEnvelopeJson("JumpStartedEvent", e));
    });
    bus_.subscribe<JumpLandedEvent>([this](const JumpLandedEvent& e) {
        broadcast(toEnvelopeJson("JumpLandedEvent", e));
    });
    bus_.subscribe<RestEndedEvent>([this](const RestEndedEvent& e) {
        broadcast(toEnvelopeJson("RestEndedEvent", e));
    });
    bus_.subscribe<PlayerDisconnectedEvent>([this](const PlayerDisconnectedEvent& e) {
        broadcast(toEnvelopeJson("PlayerDisconnectedEvent", e));
    });
    bus_.subscribe<PlayerReconnectedEvent>([this](const PlayerReconnectedEvent& e) {
        broadcast(toEnvelopeJson("PlayerReconnectedEvent", e));
    });
}

void NetworkPublisher::addConnection(std::shared_ptr<ix::WebSocket> connection) {
    {
        std::lock_guard lock(connectionsMutex_);
        connections_.push_back(connection);
    }
    std::string whiteUsername = players_.usernameForColor('w').value_or("?");
    std::string blackUsername = players_.usernameForColor('b').value_or("?");
    connection->send(toEnvelopeJson("GameStartedEvent", gameSession_.buildGameStartedEvent(whiteUsername, blackUsername)));
}

void NetworkPublisher::removeConnection(ix::WebSocket* connection) {
    std::lock_guard lock(connectionsMutex_);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                        [connection](const std::shared_ptr<ix::WebSocket>& c) { return c.get() == connection; }),
        connections_.end());
}

void NetworkPublisher::broadcast(const std::string& message) {
    std::vector<std::shared_ptr<ix::WebSocket>> snapshot;
    {
        std::lock_guard lock(connectionsMutex_);
        snapshot = connections_;
    }
    for (auto& connection : snapshot) {
        connection->send(message);
    }
}
