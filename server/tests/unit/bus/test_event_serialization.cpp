#include "../../catch2/catch_amalgamated.hpp"
#include "bus/event_serialization.hpp"

TEST_CASE("MoveMadeEvent round-trips through JSON") {
    MoveMadeEvent original{Position{1, 2}, Position{3, 4}, "wP", 12345};
    json j = original;
    MoveMadeEvent restored = j.get<MoveMadeEvent>();

    REQUIRE(restored.from.row == original.from.row);
    REQUIRE(restored.from.col == original.from.col);
    REQUIRE(restored.to.row == original.to.row);
    REQUIRE(restored.to.col == original.to.col);
    REQUIRE(restored.piece_token == original.piece_token);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("PieceCapturedEvent round-trips through JSON") {
    PieceCapturedEvent original{Position{0, 0}, "wQ", "bK", 999};
    json j = original;
    PieceCapturedEvent restored = j.get<PieceCapturedEvent>();

    REQUIRE(restored.at.row == original.at.row);
    REQUIRE(restored.capturing_piece_token == original.capturing_piece_token);
    REQUIRE(restored.captured_piece_token == original.captured_piece_token);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("GameOverEvent serializes winner_color as a JSON string, not a number") {
    GameOverEvent original{'w', 42};
    json j = original;

    REQUIRE(j.at("winner_color").is_string());
    REQUIRE(j.at("winner_color").get<std::string>() == "w");

    GameOverEvent restored = j.get<GameOverEvent>();
    REQUIRE(restored.winner_color == 'w');
}

TEST_CASE("ScoreUpdatedEvent serializes color as a JSON string, not a number") {
    ScoreUpdatedEvent original{'b', 7, 100};
    json j = original;

    REQUIRE(j.at("color").is_string());
    REQUIRE(j.at("color").get<std::string>() == "b");

    ScoreUpdatedEvent restored = j.get<ScoreUpdatedEvent>();
    REQUIRE(restored.color == 'b');
    REQUIRE(restored.new_score == 7);
}

TEST_CASE("MoveStartedEvent round-trips through JSON") {
    MoveStartedEvent original{Position{0, 0}, Position{0, 3}, "wR", 3000, 1000};
    json j = original;
    MoveStartedEvent restored = j.get<MoveStartedEvent>();

    REQUIRE(restored.from.row == original.from.row);
    REQUIRE(restored.to.col == original.to.col);
    REQUIRE(restored.piece_token == original.piece_token);
    REQUIRE(restored.duration_ms == original.duration_ms);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("GameStartedEvent round-trips through JSON") {
    GameStartedEvent original{2, 2, {"wK", ".", ".", "bK"}, "alice", "bob", 2500};
    json j = original;
    GameStartedEvent restored = j.get<GameStartedEvent>();

    REQUIRE(restored.row_count == original.row_count);
    REQUIRE(restored.col_count == original.col_count);
    REQUIRE(restored.cells == original.cells);
    REQUIRE(restored.white_username == original.white_username);
    REQUIRE(restored.black_username == original.black_username);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("RestEndedEvent round-trips through JSON") {
    RestEndedEvent original{Position{0, 3}, "bQ", 4200};
    json j = original;
    RestEndedEvent restored = j.get<RestEndedEvent>();

    REQUIRE(restored.position.row == original.position.row);
    REQUIRE(restored.position.col == original.position.col);
    REQUIRE(restored.piece_token == original.piece_token);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("JumpStartedEvent round-trips through JSON") {
    JumpStartedEvent original{Position{1, 1}, "wK", 1000, 500};
    json j = original;
    JumpStartedEvent restored = j.get<JumpStartedEvent>();

    REQUIRE(restored.position.row == original.position.row);
    REQUIRE(restored.position.col == original.position.col);
    REQUIRE(restored.piece_token == original.piece_token);
    REQUIRE(restored.duration_ms == original.duration_ms);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}

TEST_CASE("PlayerDisconnectedEvent serializes color as a JSON string, not a number") {
    PlayerDisconnectedEvent original{'w', 20000, 500};
    json j = original;

    REQUIRE(j.at("color").is_string());
    REQUIRE(j.at("color").get<std::string>() == "w");

    PlayerDisconnectedEvent restored = j.get<PlayerDisconnectedEvent>();
    REQUIRE(restored.color == 'w');
    REQUIRE(restored.grace_duration_ms == 20000);
    REQUIRE(restored.timestamp_ms == 500);
}

TEST_CASE("PlayerReconnectedEvent serializes color as a JSON string, not a number") {
    PlayerReconnectedEvent original{'b', 700};
    json j = original;

    REQUIRE(j.at("color").is_string());
    REQUIRE(j.at("color").get<std::string>() == "b");

    PlayerReconnectedEvent restored = j.get<PlayerReconnectedEvent>();
    REQUIRE(restored.color == 'b');
    REQUIRE(restored.timestamp_ms == 700);
}

TEST_CASE("JumpLandedEvent round-trips through JSON") {
    JumpLandedEvent original{Position{1, 1}, "wK", 1500};
    json j = original;
    JumpLandedEvent restored = j.get<JumpLandedEvent>();

    REQUIRE(restored.position.row == original.position.row);
    REQUIRE(restored.position.col == original.position.col);
    REQUIRE(restored.piece_token == original.piece_token);
    REQUIRE(restored.timestamp_ms == original.timestamp_ms);
}
