#include "server.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>

#include "bus/nats_publisher.hpp"
#include "lobby/in_memory_match_queue.hpp"
#include "lobby/lobby_registry.hpp"
#include "lobby/matchmaker.hpp"
#include "matches/game_allocator.hpp"
#include "matches/game_registry.hpp"
#include "db/sqlite_player_account_store.hpp"
#include "logic/io/board_parser.hpp"
#include "network/http_server.hpp"
#include "network/websocket_server.hpp"
#include "logging/logger.hpp"

namespace {
    constexpr long long kTickMs = 50;

    Board loadStartingBoard(const std::string& layoutPath) {
        std::ifstream file(layoutPath);
        Board board(0, 0);
        std::vector<std::string> commands;
        BoardParser::parse(file, board, commands);
        return board;
    }
}

int Server::run() {
    ix::initNetSystem();

    Board templateBoard = loadStartingBoard(layoutPath_);
    SqlitePlayerAccountStore accounts("players.db");
    NatsPublisher natsPublisher("nats://localhost:4222");   // fixed for now, same as the "players.db" path above
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry(natsPublisher);
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, templateBoard);
    InMemoryMatchQueue matchQueue(matchmaker);

    GameWebSocketServer webSocketServer(wsPort_, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, std::move(templateBoard));

    if (!webSocketServer.start()) {
        return 1;
    }

    HttpServer httpServer(httpPort_, accounts);
    if (!httpServer.start()) {
        return 1;
    }

    std::cout << "Server listening on port " << wsPort_ << " (WebSocket) and " << httpPort_ << " (HTTP)" << std::endl;
    Logger::info("Server listening on port " + std::to_string(wsPort_) + " (WebSocket) and " + std::to_string(httpPort_) + " (HTTP)");

    int tickCount = 0;
    constexpr int kHeartbeatEveryNTicks = 100;   // ~5s at kTickMs=50

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
        matchmaker.tick();
        webSocketServer.attachNewlyMatchedConnections();
        gameRegistry.updateAll(kTickMs);

        if (++tickCount % kHeartbeatEveryNTicks == 0) {
            long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            natsPublisher.publish("server.heartbeat", "{\"timestampMs\":" + std::to_string(nowMs) + "}");
        }
    }
}
