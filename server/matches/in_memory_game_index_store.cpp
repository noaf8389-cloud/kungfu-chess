#include "in_memory_game_index_store.hpp"

GameIndexStore::GameId InMemoryGameIndexStore::nextGameId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return nextId_++;
}

void InMemoryGameIndexStore::setUsernameGame(const std::string& username, GameId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    usernameToGame_[username] = id;
}

std::optional<GameIndexStore::GameId> InMemoryGameIndexStore::gameForUsername(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = usernameToGame_.find(username);
    return it != usernameToGame_.end() ? std::optional<GameId>(it->second) : std::nullopt;
}

void InMemoryGameIndexStore::removeUsernameGame(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    usernameToGame_.erase(username);
}

void InMemoryGameIndexStore::setRoomCodeGame(const std::string& roomCode, GameId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    roomCodeToGame_[roomCode] = id;
}

std::optional<GameIndexStore::GameId> InMemoryGameIndexStore::gameForRoomCode(const std::string& roomCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = roomCodeToGame_.find(roomCode);
    return it != roomCodeToGame_.end() ? std::optional<GameId>(it->second) : std::nullopt;
}

void InMemoryGameIndexStore::removeRoomCodeGame(const std::string& roomCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    roomCodeToGame_.erase(roomCode);
}
