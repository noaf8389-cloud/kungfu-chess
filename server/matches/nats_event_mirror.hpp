#pragma once

class EventBus;
class NatsPublisher;
struct GameOverEvent;

// Mirrors GameOverEvent to NATS, alongside GameMatch's existing in-process
// subscribers (e.g. RatingUpdater) — the real proof for Phase 4, replacing the
// synthetic heartbeat. Safe even when NATS is unreachable (NatsPublisher never
// throws, publish() is then a no-op) — can never affect a running match.
class NatsEventMirror {
public:
    NatsEventMirror(EventBus& bus, NatsPublisher& natsPublisher);

private:
    void onGameOver(const GameOverEvent& event);

    NatsPublisher& natsPublisher_;
};
