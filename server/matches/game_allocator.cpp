#include "game_allocator.hpp"

#include <algorithm>
#include <stdexcept>

GameAllocator::GameAllocator(GameRegistry& registry) : shards_{&registry} {}

GameAllocator::GameAllocator(std::vector<GameRegistry*> shards) : shards_(std::move(shards)) {
    if (shards_.empty()) {
        throw std::invalid_argument("GameAllocator: shards must not be empty");
    }
}

GameRegistry& GameAllocator::allocate() {
    auto leastLoaded = std::min_element(shards_.begin(), shards_.end(),
        [](GameRegistry* a, GameRegistry* b) { return a->matchCount() < b->matchCount(); });
    return **leastLoaded;
}
