#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "game_index_store.hpp"

struct redisContext;

// Redis-backed GameIndexStore, via the raw hiredis C client -- same pattern
// as RedisLobbyStore (see Implementation_Roadmap.md Phase 3 Decision Log for
// why hiredis directly, not redis-plus-plus). Meant to be shared across
// multiple GameRegistry shards within one WsGateway process (Phase 6) -- each
// shard has its own independent mutex, so two shards can legitimately call
// into this same store from two different threads at once. Guards its own
// redisContext with a mutex accordingly: unlike RedisLobbyStore (used by
// exactly one LobbyRegistry, whose own single mutex already serializes every
// call), a raw hiredis connection is not safe for concurrent use on its own.
class RedisGameIndexStore : public GameIndexStore {
public:
    // Throws std::runtime_error if the connection fails.
    RedisGameIndexStore(const std::string& host, int port);
    ~RedisGameIndexStore() override;

    GameId nextGameId() override;

    void setUsernameGame(const std::string& username, GameId id) override;
    std::optional<GameId> gameForUsername(const std::string& username) const override;
    void removeUsernameGame(const std::string& username) override;

    void setRoomCodeGame(const std::string& roomCode, GameId id) override;
    std::optional<GameId> gameForRoomCode(const std::string& roomCode) const override;
    void removeRoomCodeGame(const std::string& roomCode) override;

private:
    struct ContextDeleter {
        void operator()(redisContext* ctx) const;
    };

    mutable std::mutex mutex_;
    std::unique_ptr<redisContext, ContextDeleter> context_;
};
