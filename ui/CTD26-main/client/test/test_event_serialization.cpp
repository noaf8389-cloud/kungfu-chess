#include "test_framework.hpp"
#include "bus/event_serialization.hpp"

TEST(position_round_trips_through_json) {
    Position p{3, 5};
    json j = p;
    Position back = j.get<Position>();
    EXPECT_EQ(back.row, 3);
    EXPECT_EQ(back.col, 5);
}

TEST(move_made_event_round_trips_through_json) {
    MoveMadeEvent e{Position{0, 0}, Position{0, 3}, "wR", 1000};
    json j = e;
    MoveMadeEvent back = j.get<MoveMadeEvent>();
    EXPECT_EQ(back.from.row, 0);
    EXPECT_EQ(back.to.col, 3);
    EXPECT_EQ(back.piece_token, std::string("wR"));
    EXPECT_EQ(back.timestamp_ms, 1000);
}

TEST(piece_captured_event_round_trips_through_json) {
    PieceCapturedEvent e{Position{2, 2}, "wQ", "bP", 2000};
    json j = e;
    PieceCapturedEvent back = j.get<PieceCapturedEvent>();
    EXPECT_EQ(back.at.row, 2);
    EXPECT_EQ(back.capturing_piece_token, std::string("wQ"));
    EXPECT_EQ(back.captured_piece_token, std::string("bP"));
    EXPECT_EQ(back.timestamp_ms, 2000);
}

TEST(game_started_event_round_trips_through_json) {
    GameStartedEvent e{2, 2, {"wR", "--", "--", "bR"}, "alice", "bob", 0};
    json j = e;
    GameStartedEvent back = j.get<GameStartedEvent>();
    EXPECT_EQ(back.row_count, 2);
    EXPECT_EQ(back.col_count, 2);
    EXPECT_EQ(back.cells.size(), size_t(4));
    EXPECT_EQ(back.cells[0], std::string("wR"));
    EXPECT_EQ(back.white_username, std::string("alice"));
    EXPECT_EQ(back.black_username, std::string("bob"));
    EXPECT_EQ(back.timestamp_ms, 0);
}

TEST(move_started_event_round_trips_through_json) {
    MoveStartedEvent e{Position{0, 0}, Position{0, 3}, "wR", 3000, 1000};
    json j = e;
    MoveStartedEvent back = j.get<MoveStartedEvent>();
    EXPECT_EQ(back.duration_ms, 3000);
    EXPECT_EQ(back.piece_token, std::string("wR"));
}

TEST(jump_started_event_round_trips_through_json) {
    JumpStartedEvent e{Position{1, 1}, "bN", 1000, 500};
    json j = e;
    JumpStartedEvent back = j.get<JumpStartedEvent>();
    EXPECT_EQ(back.position.row, 1);
    EXPECT_EQ(back.duration_ms, 1000);
}

TEST(jump_landed_event_round_trips_through_json) {
    JumpLandedEvent e{Position{1, 1}, "bN", 1500};
    json j = e;
    JumpLandedEvent back = j.get<JumpLandedEvent>();
    EXPECT_EQ(back.position.col, 1);
    EXPECT_EQ(back.timestamp_ms, 1500);
}

TEST(rest_ended_event_round_trips_through_json) {
    RestEndedEvent e{Position{4, 4}, "wP", 4000};
    json j = e;
    RestEndedEvent back = j.get<RestEndedEvent>();
    EXPECT_EQ(back.position.row, 4);
    EXPECT_EQ(back.piece_token, std::string("wP"));
}

TEST(game_over_event_round_trips_through_json) {
    GameOverEvent e{'w', 9999};
    json j = e;
    GameOverEvent back = j.get<GameOverEvent>();
    EXPECT_EQ(back.winner_color, 'w');
    EXPECT_EQ(back.timestamp_ms, 9999);
}

TEST(score_updated_event_round_trips_through_json) {
    ScoreUpdatedEvent e{'b', 15, 1234};
    json j = e;
    ScoreUpdatedEvent back = j.get<ScoreUpdatedEvent>();
    EXPECT_EQ(back.color, 'b');
    EXPECT_EQ(back.new_score, 15);
    EXPECT_EQ(back.timestamp_ms, 1234);
}
