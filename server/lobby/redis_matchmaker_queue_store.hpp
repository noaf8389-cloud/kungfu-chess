#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "matchmaker_queue_store.hpp"

struct redisContext;

// Redis-backed MatchmakerQueueStore, via raw hiredis (same pattern as
// RedisLobbyStore/RedisGameIndexStore). Stores each waiting player as one
// field in a Redis hash (matchmaker:waiting), so allWaiting() is a single
// HGETALL. Unlike the earlier stores, this isn't primarily about
// multi-instance coordination -- MatchmakerServer (Phase 5) already runs as
// a genuine single process, so that race is already resolved. The value
// here is durability (the waiting queue survives a Matchmaker restart) and
// staying consistent with the rest of the codebase's storage pattern.
// Guards its own redisContext with a mutex: MatchmakerInternalServer's HTTP
// handlers (their own thread(s), one per request) and the main tick() loop
// both call into this store, and a raw hiredis connection isn't safe for
// concurrent use across threads on its own.
class RedisMatchmakerQueueStore : public MatchmakerQueueStore {
public:
    // Throws std::runtime_error if the connection fails.
    RedisMatchmakerQueueStore(const std::string& host, int port);
    ~RedisMatchmakerQueueStore() override;

    void enqueue(const std::string& username, int rating) override;
    void dequeue(const std::string& username) override;
    std::vector<WaitingPlayer> allWaiting() const override;

private:
    struct ContextDeleter {
        void operator()(redisContext* ctx) const;
    };

    mutable std::mutex mutex_;
    std::unique_ptr<redisContext, ContextDeleter> context_;
};
