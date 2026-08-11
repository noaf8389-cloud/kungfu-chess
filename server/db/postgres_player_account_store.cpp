#include "postgres_player_account_store.hpp"
#include "password_hasher.hpp"

#include <pqxx/pqxx>
#include <stdexcept>

#include "logging/logger.hpp"

PostgresPlayerAccountStore::PostgresPlayerAccountStore(const std::string& connectionString) {
    try {
        connection_ = std::make_unique<pqxx::connection>(connectionString);
    } catch (const std::exception& e) {
        Logger::error(std::string("Cannot connect to PostgreSQL: ") + e.what());
        throw std::runtime_error(std::string("Cannot connect to PostgreSQL: ") + e.what());
    }
    createTableIfMissing();
}

PostgresPlayerAccountStore::~PostgresPlayerAccountStore() = default;

void PostgresPlayerAccountStore::createTableIfMissing() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        pqxx::work txn(*connection_);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS players ("
            "  username TEXT PRIMARY KEY,"
            "  salt TEXT NOT NULL,"
            "  password_hash TEXT NOT NULL,"
            "  rating INTEGER NOT NULL DEFAULT 1200"
            ");");
        txn.commit();
    } catch (const pqxx::unique_violation&) {
        // Same benign race as PostgresGameHistoryStore::createTableIfMissing.
    }
}

LoginResult PostgresPlayerAccountStore::loginOrRegister(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);

    pqxx::result rows = txn.exec_params(
        "SELECT salt, password_hash, rating FROM players WHERE username = $1;", username);

    if (!rows.empty()) {
        std::string salt = rows[0]["salt"].as<std::string>();
        std::string storedHash = rows[0]["password_hash"].as<std::string>();
        int rating = rows[0]["rating"].as<int>();
        txn.commit();

        bool matches = PasswordHasher::hash(password, salt) == storedHash;
        return LoginResult{matches, matches ? rating : 0};
    }

    std::string salt = PasswordHasher::generateSalt();
    std::string hash = PasswordHasher::hash(password, salt);

    txn.exec_params(
        "INSERT INTO players (username, salt, password_hash, rating) VALUES ($1, $2, $3, $4);",
        username, salt, hash, kDefaultRating);
    txn.commit();

    return LoginResult{true, kDefaultRating};
}

void PostgresPlayerAccountStore::updateRating(const std::string& username, int newRating) {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    txn.exec_params("UPDATE players SET rating = $1 WHERE username = $2;", newRating, username);
    txn.commit();
}

std::optional<int> PostgresPlayerAccountStore::ratingFor(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    pqxx::work txn(*connection_);
    pqxx::result rows = txn.exec_params("SELECT rating FROM players WHERE username = $1;", username);
    txn.commit();
    if (rows.empty()) return std::nullopt;
    return rows[0]["rating"].as<int>();
}
