#pragma once

#include <string>
#include <vector>

struct GameHistoryEntry {
    std::string opponent;
    std::string result;      // "win" or "loss", from the queried username's perspective
    long long playedAtMs;    // wall-clock, epoch milliseconds
};

// Persists completed-game results (who played whom, who won) and answers
// "recent games for this username" -- the "history" piece of the API
// Gateway's login/rooms/history responsibilities.
class GameHistoryStore {
public:
    virtual ~GameHistoryStore() = default;

    virtual void recordGame(const std::string& winnerUsername, const std::string& loserUsername) = 0;
    virtual std::vector<GameHistoryEntry> historyFor(const std::string& username, int limit) const = 0;
};
