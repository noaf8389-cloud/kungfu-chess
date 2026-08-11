#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/postgres_game_history_store.hpp"

namespace {
    // Real, containerized PostgreSQL — see docker-compose.yml at the repo root.
    const std::string kConnectionString = "host=postgres port=5432 dbname=kungfuchess user=kungfuchess password=devpassword";
}

TEST_CASE("PostgresGameHistoryStore: a recorded game appears in both players' history") {
    PostgresGameHistoryStore store(kConnectionString);
    store.recordGame("pg_hist_alice", "pg_hist_bob");

    auto winnerHistory = store.historyFor("pg_hist_alice", 20);
    REQUIRE_FALSE(winnerHistory.empty());
    REQUIRE(winnerHistory[0].opponent == "pg_hist_bob");
    REQUIRE(winnerHistory[0].result == "win");

    auto loserHistory = store.historyFor("pg_hist_bob", 20);
    REQUIRE_FALSE(loserHistory.empty());
    REQUIRE(loserHistory[0].opponent == "pg_hist_alice");
    REQUIRE(loserHistory[0].result == "loss");
}

TEST_CASE("PostgresGameHistoryStore: historyFor an unknown username is empty") {
    PostgresGameHistoryStore store(kConnectionString);
    REQUIRE(store.historyFor("pg_hist_no_such_player", 20).empty());
}

TEST_CASE("PostgresGameHistoryStore: historyFor respects the limit") {
    PostgresGameHistoryStore store(kConnectionString);
    for (int i = 0; i < 3; ++i) {
        store.recordGame("pg_hist_limit_player", "pg_hist_limit_opponent" + std::to_string(i));
    }

    auto history = store.historyFor("pg_hist_limit_player", 2);
    REQUIRE(history.size() == 2);
}
