#include <chrono>
#include <thread>
#include <unordered_map>

#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/shard_internal_server.hpp"
#include "../../matches/in_memory_match_creator.hpp"
#include "../../matches/game_allocator.hpp"
#include "../../matches/game_registry.hpp"
#include "../../lobby/in_memory_match_queue.hpp"
#include "../../lobby/matchmaker.hpp"
#include "../../rooms/rooms_service.hpp"

#include <httplib.h>
#include "../../third_party/nlohmann/json.hpp"

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

    void waitForServerReady() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

TEST_CASE("ShardInternalServer: GET /health returns 200") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18099, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18099);
    auto res = client.Get("/health");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
}

TEST_CASE("POST /internal/matches creates a real match via the injected MatchCreator") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18095, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18095);
    json body = {{"player1Username", "alice"}, {"player2Username", "bob"}};
    auto res = client.Post("/internal/matches", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 204);
    REQUIRE(registry.matchFor("alice") != nullptr);
    REQUIRE(registry.matchFor("alice") == registry.matchFor("bob"));
}

TEST_CASE("POST /internal/no-match-found invokes the relay callback") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    std::string notified;
    ShardInternalServer server(18096, matchCreator, rooms, [&](const std::string& username) { notified = username; });
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18096);
    json body = {{"username", "carol"}};
    auto res = client.Post("/internal/no-match-found", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 204);
    REQUIRE(notified == "carol");
}

TEST_CASE("POST /internal/matches with a missing player returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18097, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18097);
    json body = {{"player1Username", "alice"}};
    auto res = client.Post("/internal/matches", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /internal/rooms creates a room via the injected RoomsService") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18110, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18110);
    json body = {{"username", "alice"}};
    auto res = client.Post("/internal/rooms", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 201);
    json responseBody = json::parse(res->body);
    std::string roomCode = responseBody["roomCode"];
    REQUIRE_FALSE(roomCode.empty());
    REQUIRE(registry.matchFor("alice") != nullptr);
}

TEST_CASE("POST /internal/rooms with a missing username returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18111, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18111);
    json body = json::object();
    auto res = client.Post("/internal/rooms", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /internal/rooms/{roomCode}/join joins an existing room via the injected RoomsService") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18112, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    std::string roomCode = rooms.createRoom("alice").roomCode;

    httplib::Client client("127.0.0.1", 18112);
    json body = {{"username", "bob"}};
    auto res = client.Post("/internal/rooms/" + roomCode + "/join", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json responseBody = json::parse(res->body);
    REQUIRE(responseBody["success"] == true);
    REQUIRE(registry.matchFor("bob") == registry.matchFor("alice"));
}

TEST_CASE("POST /internal/rooms/{roomCode}/join with an unknown roomCode returns 404") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18113, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18113);
    json body = {{"username", "bob"}};
    auto res = client.Post("/internal/rooms/NOSUCH/join", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 404);
}

TEST_CASE("POST /internal/rooms/{roomCode}/join with a missing username returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18114, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    std::string roomCode = rooms.createRoom("alice").roomCode;

    httplib::Client client("127.0.0.1", 18114);
    json body = json::object();
    auto res = client.Post("/internal/rooms/" + roomCode + "/join", body.dump(), "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /internal/rooms with a malformed body returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18115, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18115);
    auto res = client.Post("/internal/rooms", "not json", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /internal/rooms/{roomCode}/join with a malformed body returns 400") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18116, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    std::string roomCode = rooms.createRoom("alice").roomCode;

    httplib::Client client("127.0.0.1", 18116);
    auto res = client.Post("/internal/rooms/" + roomCode + "/join", "not json", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}
