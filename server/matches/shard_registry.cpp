#include "shard_registry.hpp"

#include <stdexcept>

ShardRegistry::ShardRegistry(std::vector<GameRegistry*> shards) : shards_(std::move(shards)) {
    if (shards_.empty()) {
        throw std::invalid_argument("ShardRegistry: shards must not be empty");
    }
}

GameMatch* ShardRegistry::matchFor(const std::string& username) {
    for (GameRegistry* shard : shards_) {
        GameMatch* match = shard->matchFor(username);
        if (match != nullptr) return match;
    }
    return nullptr;
}

GameMatch* ShardRegistry::joinRoom(const RoomCode& roomCode, const std::string& joinerUsername) {
    for (GameRegistry* shard : shards_) {
        GameMatch* match = shard->joinRoom(roomCode, joinerUsername);
        if (match != nullptr) return match;
    }
    return nullptr;
}

void ShardRegistry::updateAll(long long deltaMs) {
    for (GameRegistry* shard : shards_) {
        shard->updateAll(deltaMs);
    }
}
