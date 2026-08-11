#include <chrono>
#include <thread>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../rooms/http_rooms_client.hpp"
#include "../../../matches/shard_internal_server.hpp"
#include "../../../matches/in_memory_match_creator.hpp"
#include "../../../matches/game_allocator.hpp"
#include "../../../matches/game_registry.hpp"
#include "../../../lobby/in_memory_match_queue.hpp"
#include "../../../lobby/matchmaker.hpp"
#include "../../../rooms/rooms_service.hpp"

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

TEST_CASE("HttpRoomsClient: createRoom/joinRoom reach a real ShardInternalServer") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18104, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    HttpRoomsClient client("127.0.0.1", 18104);

    std::optional<std::string> roomCode = client.createRoom("alice");
    REQUIRE(roomCode.has_value());
    REQUIRE(registry.matchFor("alice") != nullptr);

    bool joined = client.joinRoom("bob", *roomCode);
    REQUIRE(joined);
    REQUIRE(registry.matchFor("bob") == registry.matchFor("alice"));
}

TEST_CASE("HttpRoomsClient: joinRoom with an unknown roomCode fails") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18105, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    HttpRoomsClient client("127.0.0.1", 18105);

    REQUIRE_FALSE(client.joinRoom("bob", "NOSUCH"));
}

TEST_CASE("HttpRoomsClient: createRoom against an unreachable server fails cleanly") {
    HttpRoomsClient client("127.0.0.1", 18106);

    REQUIRE_FALSE(client.createRoom("alice").has_value());
}

TEST_CASE("HttpRoomsClient: joinRoom against an unreachable server fails cleanly") {
    HttpRoomsClient client("127.0.0.1", 18107);

    REQUIRE_FALSE(client.joinRoom("alice", "ANYCODE"));
}

TEST_CASE("HttpRoomsClient: createRoom fails cleanly on a non-201 response from a real server") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator matchCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18108, matchCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    HttpRoomsClient client("127.0.0.1", 18108);

    // Empty username -> ShardInternalServer's own validation returns 400,
    // not 201 -- exercises the "reached the server but got an unexpected
    // status" branch, distinct from "server unreachable".
    REQUIRE_FALSE(client.createRoom("").has_value());
}
