#include <cstdlib>
#include <iostream>
#include <string>

#ifdef HAS_POSTGRES
#include "db/postgres_player_account_store.hpp"
#include "matches/postgres_game_history_store.hpp"
#else
#include "db/sqlite_player_account_store.hpp"
#include "matches/no_op_game_history_store.hpp"
#endif
#include "network/http_server.hpp"
#include "logging/logger.hpp"
#include "rooms/http_rooms_client.hpp"

namespace {
    std::string getEnvOr(const char* name, const std::string& fallback) {
        const char* value = std::getenv(name);
        return value ? std::string(value) : fallback;
    }

#ifdef HAS_POSTGRES
    std::string postgresConnectionString() {
        return "host=" + getEnvOr("POSTGRES_HOST", "localhost") +
               " port=" + getEnvOr("POSTGRES_PORT", "5432") +
               " dbname=" + getEnvOr("POSTGRES_DB", "kungfuchess") +
               " user=" + getEnvOr("POSTGRES_USER", "kungfuchess") +
               " password=" + getEnvOr("POSTGRES_PASSWORD", "devpassword");
    }
#endif
}

// API Gateway process: HttpServer + PlayerAccountStore (+ GameHistoryStore,
// read-only from here -- WsGateway is what records games). Genuinely
// stateless -- see Implementation_Roadmap.md Phase 5 for why this is the
// simplest of the three processes to split out.
int main() {
    // LOG_DIR defaults to the current directory (native, unchanged); Compose
    // points it at a shared volume so Promtail can ship every process's log
    // to Loki -- see Implementation_Roadmap.md Phase 7.
    Logger::init(getEnvOr("LOG_DIR", ".") + "/api_gateway.log");

    constexpr int kHttpPort = 8081;

    // WsGateway owns GameRegistry/RoomsService -- rooms created/joined here
    // are proxied to its internal API (ShardInternalServer). Best-effort,
    // same contract as HttpMatchCreator/HttpMatchQueue: a request made while
    // WsGateway isn't reachable just fails (503), it doesn't crash ApiGateway.
    HttpRoomsClient roomsClient(getEnvOr("WS_GATEWAY_HOST", "127.0.0.1"), std::stoi(getEnvOr("WS_GATEWAY_INTERNAL_PORT", "8082")));

#ifdef HAS_POSTGRES
    PostgresPlayerAccountStore accounts(postgresConnectionString());
    PostgresGameHistoryStore historyStore(postgresConnectionString());
    HttpServer httpServer(kHttpPort, accounts, historyStore, roomsClient);
#else
    SqlitePlayerAccountStore accounts("players.db");
    NoOpGameHistoryStore historyStore;
    HttpServer httpServer(kHttpPort, accounts, historyStore, roomsClient);
#endif

    if (!httpServer.start()) {
        return 1;
    }

    std::cout << "API Gateway listening on port " << kHttpPort << " (HTTP)" << std::endl;
    Logger::info("API Gateway listening on port " + std::to_string(kHttpPort) + " (HTTP)");

    httpServer.wait();
    return 0;
}
