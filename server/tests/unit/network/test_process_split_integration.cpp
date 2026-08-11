#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../catch2/catch_amalgamated.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

namespace {
    // Real, separately-running WS Gateway + Matchmaker containers -- see
    // docker-compose.yml (Phase 5). Proves matchmaking actually works across
    // the real process/container boundary: WsGateway's HttpMatchQueue calling
    // MatchmakerServer's internal API to enqueue, MatchmakerServer's
    // HttpMatchCreator calling back into WsGateway's internal API to create
    // the match. 5.3b only proved the three processes start; this proves
    // they actually talk to each other correctly.
    const std::string kWsGatewayUrl = "ws://ws-gateway:8080";

    // 20s, not 10s: this now runs alongside Postgres/Redis/NATS/Loki/Promtail
    // in the same docker-compose stack (Phase 7+), so container startup under
    // load can eat into the window before ws-gateway's WS port is reachable
    // even though its healthcheck (a different port, see docker-compose.yml)
    // already passed.
    bool waitFor(const std::function<bool()>& predicate, int timeoutMs = 20000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

TEST_CASE("two real WS clients get matched through the actual WsGateway<->MatchmakerServer process split") {
    ix::initNetSystem();

    std::mutex mutexA, mutexB;
    std::vector<std::string> receivedA, receivedB;

    ix::WebSocket clientA;
    clientA.setUrl(kWsGatewayUrl);
    clientA.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexA);
            receivedA.push_back(msg->str);
        }
    });
    clientA.start();

    ix::WebSocket clientB;
    clientB.setUrl(kWsGatewayUrl);
    clientB.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            std::lock_guard<std::mutex> lock(mutexB);
            receivedB.push_back(msg->str);
        }
    });
    clientB.start();

    REQUIRE(waitFor([&]() { return clientA.getReadyState() == ix::ReadyState::Open; }));
    REQUIRE(waitFor([&]() { return clientB.getReadyState() == ix::ReadyState::Open; }));

    // HttpMatchQueue::enqueue is fire-and-forget/best-effort (logs and moves
    // on if MatchmakerServer isn't reachable yet, same contract as
    // NatsPublisher) -- a brief grace period here avoids a startup race
    // against docker-compose's own container-start ordering, which is not
    // itself a guarantee that MatchmakerServer's listener is already up.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Unique usernames per run so repeated runs against the same long-lived
    // containers don't collide with a still-resumable match from a previous run.
    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string usernameA = "processSplitAlice" + std::to_string(nowMs);
    std::string usernameB = "processSplitBob" + std::to_string(nowMs);

    clientA.send(R"({"command":"login","username":")" + usernameA + R"(","password":"pw123"})");
    clientB.send(R"({"command":"login","username":")" + usernameB + R"(","password":"pw123"})");
    clientA.send(R"({"command":"findMatch"})");
    clientB.send(R"({"command":"findMatch"})");

    REQUIRE(waitFor([&]() { return receivedContains(mutexA, receivedA, "GameStartedEvent"); }));
    REQUIRE(waitFor([&]() { return receivedContains(mutexB, receivedB, "GameStartedEvent"); }));

    clientA.stop();
    clientB.stop();
}
