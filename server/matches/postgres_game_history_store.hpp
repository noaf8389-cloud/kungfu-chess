#pragma once

#include <memory>
#include <mutex>

#include <pqxx/pqxx>

#include "game_history_store.hpp"

// PostgreSQL-backed GameHistoryStore. Same availability rule as
// PostgresPlayerAccountStore: only compiled where libpqxx is available
// (HAS_POSTGRES, see CMakeLists.txt) -- Docker/Linux today, not native
// Windows. ApiGateway and WsGateway each construct their own instance
// against the same connectionString/table; Postgres itself is what makes
// "recorded by WsGateway, read by ApiGateway" work.
class PostgresGameHistoryStore : public GameHistoryStore {
public:
    /// Throws std::runtime_error if the connection fails or the games table can't be created.
    explicit PostgresGameHistoryStore(const std::string& connectionString);
    ~PostgresGameHistoryStore() override;

    void recordGame(const std::string& winnerUsername, const std::string& loserUsername) override;
    std::vector<GameHistoryEntry> historyFor(const std::string& username, int limit) const override;

private:
    void createTableIfMissing();

    std::unique_ptr<pqxx::connection> connection_;
    // Guards connection_: a single pqxx::connection is not safe for concurrent
    // use from multiple threads, same reasoning as PostgresPlayerAccountStore.
    mutable std::mutex mutex_;
};
