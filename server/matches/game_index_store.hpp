#pragma once

#include <cstddef>
#include <optional>
#include <string>

// Storage for GameRegistry's two lookup indices (username -> GameId, roomCode
// -> GameId) plus GameId allocation. Deliberately excludes the actual
// GameMatch objects themselves (owned only in-process by GameRegistry) --
// this interface only ever describes routing/lookup metadata, following the
// same "storage behind the interface, orchestration stays in the owning
// class" split already used for LobbyStore/LobbyRegistry.
class GameIndexStore {
public:
    using GameId = std::size_t;

    virtual ~GameIndexStore() = default;

    virtual GameId nextGameId() = 0;

    virtual void setUsernameGame(const std::string& username, GameId id) = 0;
    virtual std::optional<GameId> gameForUsername(const std::string& username) const = 0;
    virtual void removeUsernameGame(const std::string& username) = 0;

    virtual void setRoomCodeGame(const std::string& roomCode, GameId id) = 0;
    virtual std::optional<GameId> gameForRoomCode(const std::string& roomCode) const = 0;
    virtual void removeRoomCodeGame(const std::string& roomCode) = 0;
};
