#include "test_framework.hpp"
#include "../src/game/client_board_state.hpp"
#include "bus/event_bus.hpp"
#include "model/board.hpp"

namespace {
GameStartedEvent make_started_event_3x3() {
    return GameStartedEvent{3, 3, {
        "wR", ".", ".",
        ".", ".", ".",
        ".", ".", "bK"
    }, "white", "black", 0};
}
}

TEST(client_board_state_starts_with_the_initial_board) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(make_started_event_3x3());

    Board board = state.board();
    EXPECT_EQ(board.getCell(0, 0), std::string("wR"));
    EXPECT_EQ(board.getCell(2, 2), std::string("bK"));
    EXPECT_EQ(board.getCell(1, 1), Board::EMPTY_CELL);
}

TEST(client_board_state_applies_move_made_event) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(make_started_event_3x3());

    bus.publish(MoveMadeEvent{Position{0, 0}, Position{1, 1}, "wR", 1000});

    Board board = state.board();
    EXPECT_EQ(board.getCell(0, 0), Board::EMPTY_CELL);
    EXPECT_EQ(board.getCell(1, 1), std::string("wR"));
}

TEST(client_board_state_applies_normal_capture) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(GameStartedEvent{3, 3, {
        "wR", ".", ".",
        ".", "bP", ".",
        ".", ".", "bK"
    }, "white", "black", 0});

    // מהלך רגיל שמסתיים באכילה: MoveMadeEvent ואז PieceCapturedEvent, שני ה-Events מסכימים מי שרד.
    bus.publish(MoveMadeEvent{Position{0, 0}, Position{1, 1}, "wR", 1000});
    bus.publish(PieceCapturedEvent{Position{1, 1}, "wR", "bP", 1000});

    Board result = state.board();
    EXPECT_EQ(result.getCell(0, 0), Board::EMPTY_CELL);
    EXPECT_EQ(result.getCell(1, 1), std::string("wR"));
}

TEST(client_board_state_piece_captured_event_corrects_jump_defeat_case) {
    // מקרה קצה: כלי מזנק (bK) יושב באוויר ב-(1,1). wR "זז" לשם - אבל בפועל נתפס.
    // MoveMadeEvent כותב wR ב-(1,1) (שגוי!), PieceCapturedEvent מתקן: השורד האמיתי הוא bK.
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(make_started_event_3x3());

    bus.publish(MoveMadeEvent{Position{0, 0}, Position{1, 1}, "wR", 1000});
    bus.publish(PieceCapturedEvent{Position{1, 1}, "bK", "wR", 1000});

    Board result = state.board();
    EXPECT_EQ(result.getCell(1, 1), std::string("bK"));
}

TEST(client_board_state_reports_game_over_after_event) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(make_started_event_3x3());

    EXPECT_TRUE(!state.is_game_over());
    bus.publish(GameOverEvent{'w', 5000});
    EXPECT_TRUE(state.is_game_over());
}

TEST(client_board_state_board_snapshot_is_independent_copy) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(make_started_event_3x3());

    Board snapshot = state.board();
    bus.publish(MoveMadeEvent{Position{0, 0}, Position{1, 1}, "wR", 1000});

    EXPECT_EQ(snapshot.getCell(0, 0), std::string("wR"));
    EXPECT_EQ(snapshot.getCell(1, 1), Board::EMPTY_CELL);

    Board updated = state.board();
    EXPECT_EQ(updated.getCell(0, 0), Board::EMPTY_CELL);
    EXPECT_EQ(updated.getCell(1, 1), std::string("wR"));
}

TEST(client_board_state_tracks_multiple_independent_moves) {
    EventBus bus;
    ClientBoardState state(bus);
    bus.publish(GameStartedEvent{3, 3, {
        "wR", ".", ".",
        ".", ".", ".",
        "bR", ".", "."
    }, "white", "black", 0});

    bus.publish(MoveMadeEvent{Position{0, 0}, Position{0, 1}, "wR", 1000});
    bus.publish(MoveMadeEvent{Position{2, 0}, Position{2, 1}, "bR", 1000});

    Board result = state.board();
    EXPECT_EQ(result.getCell(0, 0), Board::EMPTY_CELL);
    EXPECT_EQ(result.getCell(0, 1), std::string("wR"));
    EXPECT_EQ(result.getCell(2, 0), Board::EMPTY_CELL);
    EXPECT_EQ(result.getCell(2, 1), std::string("bR"));
}
