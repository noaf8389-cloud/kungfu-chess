#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/no_op_game_history_store.hpp"

TEST_CASE("NoOpGameHistoryStore: recordGame is a no-op, historyFor is always empty") {
    NoOpGameHistoryStore store;
    REQUIRE_NOTHROW(store.recordGame("alice", "bob"));

    REQUIRE(store.historyFor("alice", 20).empty());
}
