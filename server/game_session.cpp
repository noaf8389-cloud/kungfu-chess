#include "game_session.hpp"

#include <vector>

#include "bus/game_events.hpp"
#include "logic/real_time/real_time_arbiter.hpp"
#include "logging/logger.hpp"

namespace {
    // Standard chess piece values; the king isn't scored (capturing it already ends the game).
    int pieceValue(char pieceType) {
        switch (pieceType) {
            case 'P': return 1;
            case 'N': return 3;
            case 'B': return 3;
            case 'R': return 5;
            case 'Q': return 9;
            default: return 0;
        }
    }
}

void GameSession::handleClick(int row, int col, char actingColor) {
    std::lock_guard lock(mutex_);

    std::vector<Motion> started = Controller::handleClick(row, col, gameState_, actingColor);
    for (const Motion& motion : started) {
        std::string token = gameState_.getBoard().getCell(motion.from.row, motion.from.col);
        long long now = gameState_.getCurrentTime();
        long long duration = motion.completionTime - now;
        bus_.publish(MoveStartedEvent{motion.from, motion.to, token, duration, now});
    }
}

void GameSession::handleJump(int row, int col, char actingColor) {
    std::lock_guard lock(mutex_);

    std::optional<Position> started = Controller::handleJump(row, col, gameState_, actingColor);
    if (!started.has_value()) {
        return;
    }

    std::string token = gameState_.getBoard().getCell(started->row, started->col);
    long long now = gameState_.getCurrentTime();

    bus_.publish(JumpStartedEvent{*started, token, GameState::JUMP_DURATION_MS, now});
}

void GameSession::update(long long deltaMs) {
    std::lock_guard lock(mutex_);

    gameState_.advanceTime(deltaMs);

    for (const Position& position : RealTimeArbiter::applyExpiredJumps(gameState_)) {
        std::string token = gameState_.getBoard().getCell(position.row, position.col);
        long long now = gameState_.getCurrentTime();
        bus_.publish(JumpLandedEvent{position, token, now});
    }

    for (const CompletedMoveResult& result : RealTimeArbiter::applyCompletedMoves(gameState_)) {
        long long now = gameState_.getCurrentTime();

        bus_.publish(MoveMadeEvent{result.from, result.to, result.movedToken, now});

        if (result.captured) {
            bus_.publish(PieceCapturedEvent{result.to, result.survivingToken, result.capturedToken, now});

            char capturerColor = result.survivingToken[0];
            int gained = (result.capturedToken.size() == 2) ? pieceValue(result.capturedToken[1]) : 0;
            int& score = (capturerColor == 'w') ? whiteScore_ : blackScore_;
            score += gained;
            bus_.publish(ScoreUpdatedEvent{capturerColor, score, now});
        }

        if (result.gameEnded) {
            bus_.publish(GameOverEvent{result.survivingToken[0], now});
        }
    }

    for (const Position& position : gameState_.extractExpiredRests()) {
        std::string token = gameState_.getBoard().getCell(position.row, position.col);
        long long now = gameState_.getCurrentTime();
        bus_.publish(RestEndedEvent{position, token, now});
    }
}

GameStartedEvent GameSession::buildGameStartedEvent(const std::string& whiteUsername, const std::string& blackUsername) const {
    std::lock_guard lock(mutex_);

    const Board& board = gameState_.getBoard();
    std::vector<std::string> cells;
    cells.reserve(board.getRowCount() * board.getColCount());
    for (int row = 0; row < board.getRowCount(); ++row) {
        for (int col = 0; col < board.getColCount(); ++col) {
            cells.push_back(board.getCell(row, col));
        }
    }
    return GameStartedEvent{board.getRowCount(), board.getColCount(), cells, whiteUsername, blackUsername, gameState_.getCurrentTime()};
}

void GameSession::forfeit(char forfeitingColor) {
    std::lock_guard lock(mutex_);
    if (gameState_.isGameOver()) {
        return;
    }
    gameState_.endGame();
    char winner = (forfeitingColor == 'w') ? 'b' : 'w';
    Logger::info("Forfeit: color " + std::string(1, forfeitingColor) + " timed out, winner=" + std::string(1, winner));
    bus_.publish(GameOverEvent{winner, gameState_.getCurrentTime()});
}

void GameSession::playerDisconnected(char color, long long graceDurationMs) {
    std::lock_guard lock(mutex_);
    bus_.publish(PlayerDisconnectedEvent{color, graceDurationMs, gameState_.getCurrentTime()});
}

void GameSession::playerReconnected(char color) {
    std::lock_guard lock(mutex_);
    bus_.publish(PlayerReconnectedEvent{color, gameState_.getCurrentTime()});
}
