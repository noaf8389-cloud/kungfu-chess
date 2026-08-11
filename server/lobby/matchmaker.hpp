#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "matches/game_allocator.hpp"
#include "matches/match_creator.hpp"
#include "matchmaker_queue_store.hpp"
#include "db/player_account_store.hpp"
#include "logic/model/game_state.hpp"

// Holds a queue of players waiting for a match and pairs them up by rating proximity.
class Matchmaker {
public:
    using NoMatchFoundHandler = std::function<void(const std::string& username)>;

    static constexpr long long kDefaultTimeoutMs = 60000;

    // Default: owns its own InMemoryMatchCreator and InMemoryMatchmakerQueueStore
    // -- today's behavior unchanged; used throughout tests and by KungFuChessServer.
    Matchmaker(GameAllocator& gameAllocator, PlayerAccountStore& accounts, Board startingBoard,
               long long timeoutMs = kDefaultTimeoutMs);
    // Injected MatchCreator; still owns its own in-memory queue store.
    explicit Matchmaker(MatchCreator& matchCreator, long long timeoutMs = kDefaultTimeoutMs);
    // Both injected -- used by MatchmakerServer (Phase 5) with HttpMatchCreator +
    // RedisMatchmakerQueueStore.
    Matchmaker(MatchCreator& matchCreator, MatchmakerQueueStore& queueStore, long long timeoutMs = kDefaultTimeoutMs);

    // Adds a player to the waiting queue by rating; ignored if already waiting.
    void enqueue(const std::string& username, int rating);

    // Removes a waiting player from the queue; safe to call if they aren't in it.
    void dequeue(const std::string& username);

    // Pairs up waiting players by rating and evicts anyone past the timeout.
    void tick();

    // Registers the callback fired for each player evicted by a timeout.
    void setOnNoMatchFound(NoMatchFoundHandler handler);

    // Returns the number of players currently waiting.
    std::size_t waitingCount() const;

private:
    static constexpr int kRatingRange = 100;

    mutable std::mutex mutex_;
    std::unique_ptr<MatchCreator> ownedMatchCreator_;
    MatchCreator& matchCreator_;
    std::unique_ptr<MatchmakerQueueStore> ownedQueueStore_;
    MatchmakerQueueStore& queueStore_;
    long long timeoutMs_;
    NoMatchFoundHandler onNoMatchFound_;
};
