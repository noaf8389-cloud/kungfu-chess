#pragma once

#include <vector>

#include "game_registry.hpp"

// Decides which GameRegistry (Game Server Shard) a brand-new match should be
// created in. The single-shard constructor preserves the original trivial
// behavior used throughout Phases 0-5. The multi-shard constructor (Phase 6)
// picks the least-loaded shard by GameRegistry::matchCount() -- real
// allocation logic, not a placeholder. Deliberately does not make callers of
// GameRegistry lookups (matchFor, updateAll) shard-aware -- that's a separate,
// larger step (making GameWebSocketServer itself search across shards), not
// bundled into this one. See Implementation_Roadmap.md Phase 6 Decision Log.
class GameAllocator {
public:
    /// registry must outlive this GameAllocator; it only stores a reference to it.
    explicit GameAllocator(GameRegistry& registry);

    /// Every pointer in shards must outlive this GameAllocator. shards must not be empty.
    explicit GameAllocator(std::vector<GameRegistry*> shards);

    /// Returns the least-loaded shard (by matchCount()) a new match should be created in.
    GameRegistry& allocate();

private:
    std::vector<GameRegistry*> shards_;
};
