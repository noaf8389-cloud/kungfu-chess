#pragma once

#include <string>
#include <vector>

#include "model/Position.hpp"

struct MoveMadeEvent {
    Position from;
    Position to;
    std::string piece_token;
    long long timestamp_ms;
};

struct PieceCapturedEvent {
    Position at;
    std::string capturing_piece_token;
    std::string captured_piece_token;
    long long timestamp_ms;
};

struct GameStartedEvent {
    int row_count;
    int col_count;
    std::vector<std::string> cells;
    std::string white_username;
    std::string black_username;
    long long timestamp_ms;
};

// Broadcast to every connection already in a match whenever a new connection
// joins it, so a player who connected before their opponent (and so received
// "?" in their own GameStartedEvent) finds out the opponent's name once it's
// known -- without resending the board itself, which would be unsafe once a
// lone player in a room has already made moves.
struct PlayersUpdatedEvent {
    std::string white_username;
    std::string black_username;
};

struct GameOverEvent {
    char winner_color;
    long long timestamp_ms;
};

struct ScoreUpdatedEvent {
    char color;
    int new_score;
    long long timestamp_ms;
};

struct MoveStartedEvent {
    Position from;
    Position to;
    std::string piece_token;
    long long duration_ms;
    long long timestamp_ms;
};

struct JumpStartedEvent {
    Position position;
    std::string piece_token;
    long long duration_ms;
    long long timestamp_ms;
};

struct JumpLandedEvent {
    Position position;
    std::string piece_token;
    long long timestamp_ms;
};

struct RestEndedEvent {
    Position position;
    std::string piece_token;
    long long timestamp_ms;
};

struct PlayerDisconnectedEvent {
    char color;
    long long grace_duration_ms;
    long long timestamp_ms;
};

struct PlayerReconnectedEvent {
    char color;
    long long timestamp_ms;
};
