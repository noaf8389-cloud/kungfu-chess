#include <chrono>
#include <thread>
#include <unordered_map>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../lobby/matchmaker_internal_server.hpp"
#include "../../../lobby/matchmaker.hpp"
#include "../../../matches/game_allocator.hpp"
#include "../../../matches/game_registry.hpp"

#include <httplib.h>
#include "../../../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
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

    // Each TEST_CASE uses its own port (like test_websocket_integration.cpp/
    // test_http_server.cpp) to avoid TIME_WAIT/port-reuse flakiness.
    void waitForServerReady() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

TEST_CASE("MatchmakerInternalServer: GET /health returns 200") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18100, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18100);
    auto res = client.Get("/health");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
}

TEST_CASE("POST /internal/queue enqueues a player into the real Matchmaker") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18090, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18090);
    json body = {{"username", "alice"}, {"rating", 1300}};
    auto res = client.Post("/internal/queue", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 204);
    REQUIRE(matchmaker.waitingCount() == 1);
}

TEST_CASE("DELETE /internal/queue/{username} dequeues a player from the real Matchmaker") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18091, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();
    matchmaker.enqueue("bob", 1200);

    httplib::Client client("127.0.0.1", 18091);
    auto res = client.Delete("/internal/queue/bob");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 204);
    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("POST /internal/queue with an empty username returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18092, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18092);
    json body = {{"username", ""}, {"rating", 1200}};
    auto res = client.Post("/internal/queue", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}
