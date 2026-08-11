#include "../../catch2/catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include "../../../lobby/in_memory_match_queue.hpp"
#include "../../../lobby/lobby_registry.hpp"
#include "../../../lobby/matchmaker.hpp"
#include "../../../matches/game_registry.hpp"
#include "../../../network/websocket_server.hpp"
#include "../../../db/sqlite_player_account_store.hpp"

namespace {
    bool waitFor(const std::function<bool()>& predicate, int timeoutMs = 5000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    bool receivedContains(std::mutex& mutex, std::vector<std::string>& received, const std::string& needle) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const std::string& message : received) {
            if (message.find(needle) != std::string::npos) return true;
        }
        return false;
    }
}

TEST_CASE("two clients log in, get matched, and a click broadcasts to both over real WebSockets") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8901;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();

    ix::WebSocket clientB;
    clientB.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();

    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    clientA.send(R"({"command":"login","username":"alice","password":"pw123"})");
    clientB.send(R"({"command":"login","username":"bob","password":"pw123"})");
    clientA.send(R"({"command":"findMatch"})");
    clientB.send(R"({"command":"findMatch"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "GameStartedEvent"); }));
    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "GameStartedEvent"); }));

    // Color assignment order isn't deterministic across the two clients, so both
    // send the same clicks — only the one actually holding white will take effect.
    clientA.send(R"({"command":"click","row":0,"col":0})");
    clientB.send(R"({"command":"click","row":0,"col":0})");
    clientA.send(R"({"command":"click","row":0,"col":3})");
    clientB.send(R"({"command":"click","row":0,"col":3})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "MoveStartedEvent"); }));
    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "MoveStartedEvent"); }));

    clientA.stop();
    clientB.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("a client that reconnects with the same username is routed back into their existing match") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8903;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();

    ix::WebSocket clientB;
    clientB.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();

    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    clientA.send(R"({"command":"login","username":"alice2","password":"pw123"})");
    clientB.send(R"({"command":"login","username":"bob2","password":"pw123"})");
    clientA.send(R"({"command":"findMatch"})");
    clientB.send(R"({"command":"findMatch"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "GameStartedEvent"); }));
    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "GameStartedEvent"); }));

    // alice2 disconnects and reconnects with the same username, on a fresh socket.
    clientA.stop();

    std::mutex mutexA2;
    std::vector<std::string> receivedA2;
    ix::WebSocket clientA2;
    clientA2.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA2.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA2);
            receivedA2.push_back(msg->str);
        }
    });
    clientA2.start();
    REQUIRE(waitFor([&]() { return clientA2.getReadyState() == ix::ReadyState::Open; }));

    clientA2.send(R"({"command":"login","username":"alice2","password":"pw123"})");

    // Being routed back into the same match sends a fresh GameStartedEvent snapshot.
    REQUIRE(waitFor([&]() { return receivedContains(mutexA2, receivedA2, "GameStartedEvent"); }));

    clientA2.stop();
    clientB.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("a client with no matching rating partner receives NoMatchFoundEvent after the matchmaking timeout") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board, /*timeoutMs=*/50);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8904;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutex;
    std::vector<std::string> received;
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:" + std::to_string(port));
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutex);
            received.push_back(msg->str);
        }
    });
    client.start();
    REQUIRE(waitFor([&]() { return client.getReadyState() == ix::ReadyState::Open; }));

    client.send(R"({"command":"login","username":"lonely","password":"pw123"})");
    client.send(R"({"command":"findMatch"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutex, received, "NoMatchFoundEvent"); }));

    client.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("a client that creates a room receives a RoomCreatedEvent with a room code") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8905;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutex;
    std::vector<std::string> received;
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:" + std::to_string(port));
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutex);
            received.push_back(msg->str);
        }
    });
    client.start();
    REQUIRE(waitFor([&]() { return client.getReadyState() == ix::ReadyState::Open; }));

    client.send(R"({"command":"login","username":"roomCreator","password":"pw123"})");
    client.send(R"({"command":"createRoom"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutex, received, "RoomCreatedEvent"); }));

    client.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("a second client that joins an existing room by code gets matched with the creator") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8906;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();
    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));

    clientA.send(R"({"command":"login","username":"host","password":"pw123"})");
    clientA.send(R"({"command":"createRoom"})");
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "RoomCreatedEvent"); }));

    std::string roomCode;
    {
        std::lock_guard<std::mutex> lock(mutexA);
        for (const std::string& message : receivedA) {
            auto pos = message.find("roomCode\":\"");
            if (pos != std::string::npos) {
                std::size_t start = pos + 11;
                std::size_t end = message.find('"', start);
                roomCode = message.substr(start, end - start);
            }
        }
    }
    REQUIRE_FALSE(roomCode.empty());

    ix::WebSocket clientB;
    clientB.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    clientB.send(R"({"command":"login","username":"guest","password":"pw123"})");
    clientB.send(R"({"command":"joinRoom","roomCode":")" + roomCode + R"("})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "GameStartedEvent"); }));

    clientA.stop();
    clientB.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("joining with an unknown room code receives a JoinRoomFailedEvent") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8907;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutex;
    std::vector<std::string> received;
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:" + std::to_string(port));
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutex);
            received.push_back(msg->str);
        }
    });
    client.start();
    REQUIRE(waitFor([&]() { return client.getReadyState() == ix::ReadyState::Open; }));

    client.send(R"({"command":"login","username":"guest2","password":"pw123"})");
    client.send(R"({"command":"joinRoom","roomCode":"NOSUCH"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutex, received, "JoinRoomFailedEvent"); }));

    client.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("creating a room after findMatch removes the player from the matchmaking queue") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board, /*timeoutMs=*/50);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8908;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();
    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));

    // alice starts down the matchmaking path, then changes her mind and creates a room instead.
    // The two commands are spaced out so the server has processed findMatch (no ack of its own)
    // before createRoom arrives, avoiding any ambiguity about per-connection message ordering.
    clientA.send(R"({"command":"login","username":"alice3","password":"pw123"})");
    clientA.send(R"({"command":"findMatch"})");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    clientA.send(R"({"command":"createRoom"})");
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "RoomCreatedEvent"); }));

    // bob has a matching rating but alice must no longer be in the queue to pair with him —
    // if the bug were still present, bob would get matched with alice instead of timing out.
    ix::WebSocket clientB;
    clientB.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    clientB.send(R"({"command":"login","username":"bob3","password":"pw123"})");
    clientB.send(R"({"command":"findMatch"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "NoMatchFoundEvent"); }));
    REQUIRE(receivedContains(mutexB, receivedB, "GameStartedEvent") == false);

    clientA.stop();
    clientB.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("a client that reconnects into a room-created match is routed back into it") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8909;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();
    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));

    clientA.send(R"({"command":"login","username":"host2","password":"pw123"})");
    clientA.send(R"({"command":"createRoom"})");
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "RoomCreatedEvent"); }));

    std::string roomCode;
    {
        std::lock_guard<std::mutex> lock(mutexA);
        for (const std::string& message : receivedA) {
            auto pos = message.find("roomCode\":\"");
            if (pos != std::string::npos) {
                std::size_t start = pos + 11;
                std::size_t end = message.find('"', start);
                roomCode = message.substr(start, end - start);
            }
        }
    }
    REQUIRE_FALSE(roomCode.empty());

    ix::WebSocket clientB;
    clientB.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    clientB.send(R"({"command":"login","username":"guest3","password":"pw123"})");
    clientB.send(R"({"command":"joinRoom","roomCode":")" + roomCode + R"("})");
    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "GameStartedEvent"); }));

    // host2 disconnects and reconnects with the same username, on a fresh socket.
    clientA.stop();

    std::mutex mutexA2;
    std::vector<std::string> receivedA2;
    ix::WebSocket clientA2;
    clientA2.setUrl("ws://127.0.0.1:" + std::to_string(port));
    clientA2.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA2);
            receivedA2.push_back(msg->str);
        }
    });
    clientA2.start();
    REQUIRE(waitFor([&]() { return clientA2.getReadyState() == ix::ReadyState::Open; }));

    clientA2.send(R"({"command":"login","username":"host2","password":"pw123"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexA2, receivedA2, "GameStartedEvent"); }));

    clientA2.stop();
    clientB.stop();
    stopTicking = true;
    ticker.join();
}

TEST_CASE("sending createRoom again while already in a match is a no-op") {
    ix::initNetSystem();

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobbyRegistry;
    GameRegistry gameRegistry;
    GameAllocator gameAllocator(gameRegistry);
    Matchmaker matchmaker(gameAllocator, accounts, board);
    InMemoryMatchQueue matchQueue(matchmaker);

    const int port = 8910;
    GameWebSocketServer server(port, lobbyRegistry, gameRegistry, gameAllocator, matchQueue, accounts, board);
    REQUIRE(server.start());

    std::atomic<bool> stopTicking{false};
    std::thread ticker([&]() {
        while (!stopTicking.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            matchmaker.tick();
            server.attachNewlyMatchedConnections();
            gameRegistry.updateAll(20);
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::mutex mutex;
    std::vector<std::string> received;
    ix::WebSocket client;
    client.setUrl("ws://127.0.0.1:" + std::to_string(port));
    client.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutex);
            received.push_back(msg->str);
        }
    });
    client.start();
    REQUIRE(waitFor([&]() { return client.getReadyState() == ix::ReadyState::Open; }));

    client.send(R"({"command":"login","username":"solo","password":"pw123"})");
    client.send(R"({"command":"createRoom"})");
    REQUIRE(waitFor([&]() { return receivedContains(mutex, received, "RoomCreatedEvent"); }));

    client.send(R"({"command":"createRoom"})");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int roomCreatedCount = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (const std::string& message : received) {
            if (message.find("RoomCreatedEvent") != std::string::npos) roomCreatedCount++;
        }
    }
    REQUIRE(roomCreatedCount == 1);

    client.stop();
    stopTicking = true;
    ticker.join();
}
