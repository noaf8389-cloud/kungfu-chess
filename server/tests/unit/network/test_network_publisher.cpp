#include "../../catch2/catch_amalgamated.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "../../../matches/game_match.hpp"
#include "../../../db/sqlite_player_account_store.hpp"
#include "bus/nats_publisher.hpp"
#include "bus/game_events.hpp"

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

    class FakeAccountStore : public PlayerAccountStore {
    public:
        LoginResult loginOrRegister(const std::string&, const std::string&) override { return {}; }
        void updateRating(const std::string&, int) override {}
        std::optional<int> ratingFor(const std::string&) const override { return std::nullopt; }
    };

    Board makeBoard() {
        Board board(4, 4);
        board.setCell(0, 0, "wR");
        return board;
    }
}

// NetworkPublisher (one per GameMatch) subscribes to 10 event types and
// forwards each to every connected socket. MoveMadeEvent/MoveStartedEvent/
// GameOverEvent are already exercised end-to-end through real gameplay
// elsewhere (test_game_match.cpp, test_websocket_integration.cpp). The other 7
// are impractical to trigger organically through real moves (jump/rest timing,
// disconnect grace, score) -- publishing them directly on a live match's own
// bus still exercises the exact subscribe->broadcast wiring in
// NetworkPublisher that would otherwise go untested. Deliberately uses a bare
// GameMatch + raw ix::WebSocketServer instead of the full
// GameWebSocketServer/Matchmaker/GameRegistry stack (see
// test_websocket_integration.cpp) -- none of that machinery is relevant to
// what's under test here, and a real WS connection is only needed as a target
// for GameMatch::addConnection.
TEST_CASE("NetworkPublisher broadcasts every remaining event type to a connected client") {
    ix::initNetSystem();

    FakeAccountStore accounts;
    NatsPublisher natsPublisher;
    GameMatch match(makeBoard(), accounts, natsPublisher);

    const int port = 8911;
    std::mutex serverSocketMutex;
    std::shared_ptr<ix::WebSocket> serverSideSocket;

    ix::WebSocketServer rawServer(port, "0.0.0.0");
    rawServer.setOnConnectionCallback(
        [&serverSocketMutex, &serverSideSocket](std::weak_ptr<ix::WebSocket> weakWebSocket, std::shared_ptr<ix::ConnectionState>) {
            auto webSocket = weakWebSocket.lock();
            if (!webSocket) return;
            // Required by ixwebsocket even though this test never needs to
            // receive anything from the client -- without it, the library
            // logs "Server callback improperly registered" and never
            // completes the handshake (clientA never reaches Open).
            webSocket->setOnMessageCallback([](const ix::WebSocketMessagePtr&) {});

            // Deliberately NOT calling match.addConnection() here: the real
            // GameWebSocketServer never sends anything synchronously inside
            // setOnConnectionCallback either -- attaching a match always
            // happens later (a subsequent message, or the tick thread), by
            // which point the client's read path is actually ready. Sending
            // this early meant the client never received it.
            std::lock_guard<std::mutex> lock(serverSocketMutex);
            serverSideSocket = webSocket;
        });
    auto listenResult = rawServer.listen();
    REQUIRE(listenResult.first);
    rawServer.start();

    std::mutex mutexA;
    std::vector<std::string> receivedA;

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

    std::shared_ptr<ix::WebSocket> connectedSocket;
    REQUIRE(waitFor([&]() {
        std::lock_guard<std::mutex> lock(serverSocketMutex);
        connectedSocket = serverSideSocket;
        return connectedSocket != nullptr;
    }));

    // addConnection sends a GameStartedEvent snapshot immediately -- confirms
    // the connection actually reached the match before publishing anything else.
    match.addConnection(connectedSocket);
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "GameStartedEvent"); }));

    match.bus().publish(PieceCapturedEvent{Position{0, 0}, "wR", "bK", 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "PieceCapturedEvent"); }));

    match.bus().publish(ScoreUpdatedEvent{'w', 5, 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "ScoreUpdatedEvent"); }));

    match.bus().publish(JumpStartedEvent{Position{1, 1}, "wR", 100, 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "JumpStartedEvent"); }));

    match.bus().publish(JumpLandedEvent{Position{1, 1}, "wR", 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "JumpLandedEvent"); }));

    match.bus().publish(RestEndedEvent{Position{1, 1}, "wR", 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "RestEndedEvent"); }));

    match.bus().publish(PlayerDisconnectedEvent{'b', 20000, 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "PlayerDisconnectedEvent"); }));

    match.bus().publish(PlayerReconnectedEvent{'b', 1});
    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "PlayerReconnectedEvent"); }));

    clientA.stop();
    rawServer.stop();
}
