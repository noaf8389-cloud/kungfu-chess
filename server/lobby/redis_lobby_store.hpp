#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "lobby_store.hpp"

struct redisContext;

// Redis-backed LobbyStore, via the raw hiredis C client. (redis-plus-plus was
// tried first for its higher-level API, but its CMake integration expects an
// installed hiredis — nested "hiredis/hiredis.h" layout — and doesn't resolve
// against a FetchContent source tree, which is flat. See Implementation_Roadmap.md
// Phase 3 Decision Log.) Makes connection/identity state visible across every
// server process sharing the same Redis instance. Only meaningful once more than
// one process exists (Phase 5); InMemoryLobbyStore stays the default until then.
class RedisLobbyStore : public LobbyStore {
public:
    // Throws std::runtime_error if the connection fails.
    RedisLobbyStore(const std::string& host, int port);
    ~RedisLobbyStore() override;

    ConnectionId nextConnectionId() override;
    void addConnection(ConnectionId id) override;
    void removeConnection(ConnectionId id) override;
    bool hasConnection(ConnectionId id) const override;
    void setIdentity(ConnectionId id, const std::string& username, int rating) override;
    std::optional<Identity> identityFor(ConnectionId id) const override;

private:
    struct ContextDeleter {
        void operator()(redisContext* ctx) const;
    };

    std::unique_ptr<redisContext, ContextDeleter> context_;
    // Guards context_: a single hiredis redisContext isn't safe for concurrent
    // use from multiple threads (same reasoning as RedisGameIndexStore /
    // RedisMatchmakerQueueStore) -- and unlike when this class was written,
    // LobbyRegistry no longer serializes callers on its own.
    mutable std::mutex mutex_;
};
