#pragma once

#include <string>
#include <vector>

// Storage for Matchmaker's waiting queue. joinedAtMs is wall-clock (epoch
// milliseconds via system_clock), not steady_clock -- unlike an in-process
// time_point, it needs to survive being written to and read back from an
// external store.
class MatchmakerQueueStore {
public:
    struct WaitingPlayer {
        std::string username;
        int rating;
        long long joinedAtMs;
    };

    virtual ~MatchmakerQueueStore() = default;

    // Ignored if username is already waiting.
    virtual void enqueue(const std::string& username, int rating) = 0;
    virtual void dequeue(const std::string& username) = 0;
    virtual std::vector<WaitingPlayer> allWaiting() const = 0;
};
