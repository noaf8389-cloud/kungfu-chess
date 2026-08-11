#include <chrono>
#include <thread>
#include <unordered_map>

#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/http_match_creator.hpp"
#include "../../matches/shard_internal_server.hpp"
#include "../../matches/in_memory_match_creator.hpp"
#include "../../matches/game_allocator.hpp"
#include "../../matches/game_registry.hpp"
#include "../../lobby/in_memory_match_queue.hpp"
#include "../../lobby/matchmaker.hpp"
#include "../../rooms/rooms_service.hpp"

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

TEST_CASE("HttpMatchCreator::createMatch reaches a real ShardInternalServer") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    InMemoryMatchCreator innerCreator(allocator, accounts, makeBoard());
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());
    ShardInternalServer server(18098, innerCreator, rooms, [](const std::string&) {});
    REQUIRE(server.start());
    waitForServerReady();

    HttpMatchCreator matchCreator("127.0.0.1", 18098);
    matchCreator.createMatch("alice", "bob");

    REQUIRE(registry.matchFor("alice") != nullptr);
    REQUIRE(registry.matchFor("alice") == registry.matchFor("bob"));
}
