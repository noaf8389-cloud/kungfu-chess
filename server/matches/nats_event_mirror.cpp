#include "nats_event_mirror.hpp"

#include "bus/event_bus.hpp"
#include "bus/event_serialization.hpp"
#include "bus/game_events.hpp"
#include "bus/nats_publisher.hpp"

NatsEventMirror::NatsEventMirror(EventBus& bus, NatsPublisher& natsPublisher)
    : natsPublisher_(natsPublisher) {
    bus.subscribe<GameOverEvent>([this](const GameOverEvent& event) { onGameOver(event); });
}

void NatsEventMirror::onGameOver(const GameOverEvent& event) {
    natsPublisher_.publish("game.events.game_over", toEnvelopeJson("GameOverEvent", event));
}
