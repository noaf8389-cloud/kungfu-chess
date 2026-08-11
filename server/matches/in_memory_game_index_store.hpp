#pragma once

#include <mutex>
#include <unordered_map>

#include "game_index_store.hpp"

class InMemoryGameIndexStore : public GameIndexStore {
public:
    GameId nextGameId() override;

    void setUsernameGame(const std::string& username, GameId id) override;
    std::optional<GameId> gameForUsername(const std::string& username) const override;
    void removeUsernameGame(const std::string& username) override;

    void setRoomCodeGame(const std::string& roomCode, GameId id) override;
    std::optional<GameId> gameForRoomCode(const std::string& roomCode) const override;
    void removeRoomCodeGame(const std::string& roomCode) override;

private:
    mutable std::mutex mutex_;
    GameId nextId_ = 1;
    std::unordered_map<std::string, GameId> usernameToGame_;
    std::unordered_map<std::string, GameId> roomCodeToGame_;
};
