#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>

#include "bus/nats_publisher.hpp"
#ifdef HAS_POSTGRES
#include "db/postgres_player_account_store.hpp"
#include "matches/postgres_game_history_store.hpp"
#else
#include "db/sqlite_player_account_store.hpp"
#endif
#include "lobby/http_match_queue.hpp"
#include "lobby/lobby_registry.hpp"
#include "lobby/redis_lobby_store.hpp"
#include "logic/io/board_parser.hpp"
#include "logging/logger.hpp"
#include "matches/game_allocator.hpp"
#include "matches/game_registry.hpp"
#include "matches/in_memory_match_creator.hpp"
#include "matches/redis_game_index_store.hpp"
#include "matches/shard_internal_server.hpp"
#include "matches/shard_registry.hpp"
#include "network/websocket_server.hpp"
#include "rooms/rooms_service.hpp"

namespace {
    constexpr long long kTickMs = 50;
    constexpr int kDefaultShardCount = 3;

    Board loadStartingBoard(const std::string& layoutPath) {
        std::ifstream file(layoutPath);
        Board board(0, 0);
        std::vector<std::string> commands;
        BoardParser::parse(file, board, commands);
        return board;
    }

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

// WS Gateway + Game Shards process (still co-located in one OS process --
// splitting shards out into their own processes/nodes, with real network
// routing, is future work beyond Phase 6; see Implementation_Roadmap.md).
// Runs SHARD_COUNT independent GameRegistry shards, all sharing one
// RedisGameIndexStore (so a username/roomCode is findable regardless of
// which shard actually holds the match -- see ShardRegistry). LobbyRegistry
// is backed by RedisLobbyStore. Accounts come from PostgresPlayerAccountStore
// when built with libpqxx (Docker), SqlitePlayerAccountStore otherwise
// (native Windows, where libpqxx isn't installable -- see
// Implementation_Roadmap.md Phase 2 Decision Log).
int main(int argc, char** argv) {
    std::string layoutPath = argc > 1 ? argv[1] : "../../ui/CTD26-main/layout_standard.txt";
    // LOG_DIR defaults to the current directory (native, unchanged); Compose
    // points it at a shared volume so Promtail can ship every process's log
    // to Loki -- see Implementation_Roadmap.md Phase 7.
    Logger::init(getEnvOr("LOG_DIR", ".") + "/ws_gateway.log");
    ix::initNetSystem();

    constexpr int kWsPort = 8080;
    constexpr int kInternalPort = 8082;

    std::string matchmakerHost = getEnvOr("MATCHMAKER_HOST", "127.0.0.1");
    int matchmakerPort = std::stoi(getEnvOr("MATCHMAKER_PORT", "8083"));
    int shardCount = std::stoi(getEnvOr("SHARD_COUNT", std::to_string(kDefaultShardCount)));
    std::string redisHost = getEnvOr("REDIS_HOST", "127.0.0.1");
    int redisPort = std::stoi(getEnvOr("REDIS_PORT", "6379"));

    Board templateBoard = loadStartingBoard(layoutPath);

    NatsPublisher natsPublisher(getEnvOr("NATS_URL", "nats://localhost:4222"));
    RedisLobbyStore lobbyStore(redisHost, redisPort);
    LobbyRegistry lobbyRegistry(lobbyStore);
    RedisGameIndexStore gameIndexStore(redisHost, redisPort);

    std::vector<std::unique_ptr<GameRegistry>> shards;
    std::vector<GameRegistry*> shardPointers;

#ifdef HAS_POSTGRES
    PostgresPlayerAccountStore accounts(postgresConnectionString());
    PostgresGameHistoryStore historyStore(postgresConnectionString());
    for (int i = 0; i < shardCount; ++i) {
        shards.push_back(std::make_unique<GameRegistry>(natsPublisher, gameIndexStore, historyStore));
        shardPointers.push_back(shards.back().get());
    }
#else
    SqlitePlayerAccountStore accounts("players.db");
    for (int i = 0; i < shardCount; ++i) {
        shards.push_back(std::make_unique<GameRegistry>(natsPublisher, gameIndexStore));
        shardPointers.push_back(shards.back().get());
    }
#endif

    GameAllocator gameAllocator(shardPointers);
    ShardRegistry shardRegistry(shardPointers);
    InMemoryMatchCreator matchCreator(gameAllocator, accounts, templateBoard);
    HttpMatchQueue matchQueue(matchmakerHost, matchmakerPort);

    // Its own RoomsService, independent from the one GameWebSocketServer
    // builds for itself below -- both are stateless facades over the same
    // shardRegistry/gameAllocator/accounts/matchQueue, so having two
    // instances is equivalent to sharing one; simpler than threading a
    // reference through GameWebSocketServer's constructor for this alone.
    RoomsService internalRoomsService(shardRegistry, gameAllocator, accounts, matchQueue, templateBoard);

    // Matchmaker (a separate process) calls this to create matches and to
    // push timeout notifications -- see Implementation_Roadmap.md Phase 5.2.
    // ApiGateway (via HttpRoomsClient) calls this to create/join rooms over
    // REST -- see Implementation_Roadmap.md Phase 8 addendum.
    ShardInternalServer internalServer(kInternalPort, matchCreator, internalRoomsService,
        [&matchQueue](const std::string& username) { matchQueue.notifyNoMatchFound(username); });
    if (!internalServer.start()) {
        return 1;
    }

    GameWebSocketServer webSocketServer(kWsPort, lobbyRegistry, shardRegistry, gameAllocator, matchQueue, accounts, std::move(templateBoard));
    if (!webSocketServer.start()) {
        return 1;
    }

    std::cout << "WS Gateway listening on port " << kWsPort << " (WebSocket), internal API on port " << kInternalPort
              << ", " << shardCount << " shard(s)" << std::endl;
    Logger::info("WS Gateway listening on port " + std::to_string(kWsPort) + " (WebSocket), internal API on port " +
                 std::to_string(kInternalPort) + ", " + std::to_string(shardCount) + " shard(s)");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
        webSocketServer.attachNewlyMatchedConnections();
        shardRegistry.updateAll(kTickMs);
    }
}
