#pragma once

#include <vector>

#include "game_registry.hpp"
#include "shard_lookup.hpp"

// Searches across multiple GameRegistry shards for lookups (matchFor,
// joinRoom) and advances every shard on updateAll -- the multi-shard-aware
// counterpart to GameAllocator. Deliberately a separate class from
// GameAllocator: Server_Design.md places Game Allocator specifically on the
// control plane, out of the hot path of per-move traffic ("only in match
// setup"), while lookup (matchFor is called on every reconnect) belongs with
// the hot path. GameAllocator answers "where should a NEW match go";
// ShardRegistry answers "where does an EXISTING match live."
class ShardRegistry : public ShardLookup {
public:
    /// Every pointer in shards must outlive this ShardRegistry. shards must not be empty.
    explicit ShardRegistry(std::vector<GameRegistry*> shards);

    GameMatch* matchFor(const std::string& username) override;
    GameMatch* joinRoom(const RoomCode& roomCode, const std::string& joinerUsername) override;
    void updateAll(long long deltaMs) override;

private:
    std::vector<GameRegistry*> shards_;
};
