#pragma once

#include <functional>
#include <string>

// Abstraction over "enqueue/dequeue a player for matchmaking." Lets
// GameWebSocketServer/RoomsService talk to whatever is running the
// matchmaking queue -- today, an in-process Matchmaker; once Matchmaker runs
// as its own process (Phase 5.2/5.3), an implementation that calls out to it
// over the network -- without knowing which.
class MatchQueue {
public:
    using NoMatchFoundHandler = std::function<void(const std::string& username)>;

    virtual ~MatchQueue() = default;

    virtual void enqueue(const std::string& username, int rating) = 0;
    virtual void dequeue(const std::string& username) = 0;
    virtual void setOnNoMatchFound(NoMatchFoundHandler handler) = 0;
};
