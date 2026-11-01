#include "test_framework.hpp"
#include "../src/game/player_panel_tracker.hpp"
#include "bus/event_bus.hpp"
#include "bus/game_events.hpp"

TEST(player_panel_tracker_formats_a_pawn_move_without_piece_letter) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    // e2-e4 על לוח 8x8 רגיל: row 6 -> row 4, col 4 בשני המקרים.
    bus.publish(MoveMadeEvent{Position{6, 4}, Position{4, 4}, "wP", 1000});

    EXPECT_EQ(tracker.white().moves.size(), size_t(1));
    EXPECT_EQ(tracker.white().moves[0], std::string("e4"));
}

TEST(player_panel_tracker_formats_a_knight_move_with_piece_letter) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    // Nb1-c3
    bus.publish(MoveMadeEvent{Position{7, 1}, Position{5, 2}, "wN", 1000});

    EXPECT_EQ(tracker.white().moves[0], std::string("Nc3"));
}

TEST(player_panel_tracker_routes_moves_to_the_correct_color) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(MoveMadeEvent{Position{6, 4}, Position{4, 4}, "wP", 1000});
    bus.publish(MoveMadeEvent{Position{1, 4}, Position{3, 4}, "bP", 1000});

    EXPECT_EQ(tracker.white().moves.size(), size_t(1));
    EXPECT_EQ(tracker.black().moves.size(), size_t(1));
    EXPECT_EQ(tracker.black().moves[0], std::string("e5"));
}

TEST(player_panel_tracker_marks_a_non_pawn_capture_with_x_after_the_letter) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(MoveMadeEvent{Position{7, 1}, Position{5, 2}, "wN", 1000});
    bus.publish(PieceCapturedEvent{Position{5, 2}, "wN", "bP", 1000});

    EXPECT_EQ(tracker.white().moves[0], std::string("Nxc3"));
}

TEST(player_panel_tracker_marks_a_pawn_capture_with_the_origin_file) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(MoveMadeEvent{Position{6, 4}, Position{5, 3}, "wP", 1000});
    bus.publish(PieceCapturedEvent{Position{5, 3}, "wP", "bP", 1000});

    EXPECT_EQ(tracker.white().moves[0], std::string("exd3"));
}

TEST(player_panel_tracker_ignores_a_capture_that_does_not_match_the_last_move) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(MoveMadeEvent{Position{7, 1}, Position{5, 2}, "wN", 1000});
    bus.publish(PieceCapturedEvent{Position{0, 0}, "bK", "bQ", 1000});   // מיקום לא קשור

    EXPECT_EQ(tracker.white().moves[0], std::string("Nc3"));   // לא השתנה
}

TEST(player_panel_tracker_uses_the_board_size_from_game_started_event_for_ranks) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(GameStartedEvent{4, 4, std::vector<std::string>(16, "."), "white", "black", 0});
    bus.publish(MoveMadeEvent{Position{0, 0}, Position{0, 3}, "wR", 1000});

    EXPECT_EQ(tracker.white().moves[0], std::string("Rd4"));   // rank = 4 - 0, לא 8 - 0
}

TEST(player_panel_tracker_reads_usernames_from_game_started_event) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    EXPECT_EQ(tracker.white().username, std::string("?"));
    EXPECT_EQ(tracker.black().username, std::string("?"));

    bus.publish(GameStartedEvent{4, 4, std::vector<std::string>(16, "."), "alice", "bob", 0});

    EXPECT_EQ(tracker.white().username, std::string("alice"));
    EXPECT_EQ(tracker.black().username, std::string("bob"));
}

TEST(player_panel_tracker_updates_usernames_from_players_updated_event) {
    EventBus bus;
    PlayerPanelTracker tracker(bus);

    bus.publish(GameStartedEvent{4, 4, std::vector<std::string>(16, "."), "alice", "?", 0});
    EXPECT_EQ(tracker.black().username, std::string("?"));

    bus.publish(PlayersUpdatedEvent{"alice", "bob"});

    EXPECT_EQ(tracker.white().username, std::string("alice"));
    EXPECT_EQ(tracker.black().username, std::string("bob"));
}
