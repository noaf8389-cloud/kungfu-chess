#pragma once

#include <mutex>

#include "bus/event_bus.hpp"
#include "bus/game_events.hpp"
#include "logic/model/game_state.hpp"
#include "logic/input/controller.hpp"

class GameSession {
public:
    GameSession(GameState& gameState, EventBus& bus)
        : gameState_(gameState), bus_(bus) {}

    void handleClick(int row, int col, char actingColor);
    void handleJump(int row, int col, char actingColor);

    /// Updates the game state and publishes events for completed actions.
    void update(long long deltaMs);

    /// Builds a snapshot of the current board as a GameStartedEvent, for a newly connected client.
    GameStartedEvent buildGameStartedEvent(const std::string& whiteUsername, const std::string& blackUsername) const;

    /// Ends the game as a forfeit; the other color is declared the winner.
    void forfeit(char forfeitingColor);
    
    /// Publishes notice that a logged-in player disconnected, with their reconnect window.
    void playerDisconnected(char color, long long graceDurationMs);
    
    /// Publishes notice that a disconnected player reconnected in time.
    void playerReconnected(char color);

private:
    GameState& gameState_;
    EventBus& bus_;
    mutable std::recursive_mutex mutex_;
    int whiteScore_ = 0;
    int blackScore_ = 0;
};
