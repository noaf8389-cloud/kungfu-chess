#pragma once

#include "game_history_store.hpp"

// Safe default: discards every recorded game and always answers "no history."
// Matches NatsPublisher's disconnected-default philosophy -- a match/registry
// that doesn't care about history shouldn't have to know that.
class NoOpGameHistoryStore : public GameHistoryStore {
public:
    void recordGame(const std::string& winnerUsername, const std::string& loserUsername) override;
    std::vector<GameHistoryEntry> historyFor(const std::string& username, int limit) const override;
};
