#include "postgres_game_history_store.hpp"

#include <pqxx/pqxx>
#include <stdexcept>

#include "logging/logger.hpp"

PostgresGameHistoryStore::PostgresGameHistoryStore(const std::string& connectionString) {
    try {
        connection_ = std::make_unique<pqxx::connection>(connectionString);
    } catch (const std::exception& e) {
        Logger::error(std::string("Cannot connect to PostgreSQL: ") + e.what());
        throw std::runtime_error(std::string("Cannot connect to PostgreSQL: ") + e.what());
    }
    createTableIfMissing();
}

PostgresGameHistoryStore::~PostgresGameHistoryStore() = default;

void PostgresGameHistoryStore::createTableIfMissing() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        pqxx::work txn(*connection_);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS games ("
            "  id SERIAL PRIMARY KEY,"
            "  winner TEXT NOT NULL,"
            "  loser TEXT NOT NULL,"
            "  played_at TIMESTAMPTZ NOT NULL DEFAULT now()"
            ");");
        txn.commit();
    } catch (const pqxx::unique_violation&) {
        // CREATE TABLE IF NOT EXISTS races against a concurrent session doing
        // the same thing (ApiGateway/WsGateway starting together) -- another
        // process already created this table, which is what we wanted anyway.
    }
}

void PostgresGameHistoryStore::recordGame(const std::string& winnerUsername, const std::string& loserUsername) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    txn.exec_params("INSERT INTO games (winner, loser) VALUES ($1, $2);", winnerUsername, loserUsername);
    txn.commit();
}

std::vector<GameHistoryEntry> PostgresGameHistoryStore::historyFor(const std::string& username, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::result rows = txn.exec_params(
        "SELECT winner, loser, "
        "  CAST(EXTRACT(EPOCH FROM played_at) * 1000 AS BIGINT) AS played_at_ms "
        "FROM games WHERE winner = $1 OR loser = $1 "
        "ORDER BY played_at DESC LIMIT $2;",
        username, limit);
    txn.commit();

    std::vector<GameHistoryEntry> entries;
    entries.reserve(rows.size());
    for (const auto& row : rows) {
        std::string winner = row["winner"].as<std::string>();
        std::string loser = row["loser"].as<std::string>();
        bool won = (winner == username);
        entries.push_back(GameHistoryEntry{
            won ? loser : winner,
            won ? "win" : "loss",
            row["played_at_ms"].as<long long>()
        });
    }
    return entries;
}
