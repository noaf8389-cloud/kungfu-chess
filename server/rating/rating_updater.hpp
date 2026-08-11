#pragma once

#include <memory>

#include "../matches/game_history_store.hpp"

class EventBus;
class PlayerRegistry;
class PlayerAccountStore;
struct GameOverEvent;

class RatingUpdater {
public:
    // Default: owns its own NoOpGameHistoryStore (recorded games are discarded) --
    // today's behavior for callers that don't care about history.
    // Subscribes to GameOverEvent; on each game end, looks up both players' usernames and ratings and persists the updated ELO scores.
    RatingUpdater(EventBus& bus, PlayerRegistry& registry, PlayerAccountStore& accounts);
    // Injected: also records the result (winner/loser) through historyStore.
    RatingUpdater(EventBus& bus, PlayerRegistry& registry, PlayerAccountStore& accounts, GameHistoryStore& historyStore);

private:
    void onGameOver(const GameOverEvent& event);

    PlayerRegistry& registry_;
    PlayerAccountStore& accounts_;
    std::unique_ptr<GameHistoryStore> ownedHistoryStore_;
    GameHistoryStore& historyStore_;
};
