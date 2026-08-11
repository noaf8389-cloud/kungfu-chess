#include <stdexcept>
#include <vector>

#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/game_allocator.hpp"

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
}

TEST_CASE("allocate returns the registry it was constructed with") {
    GameRegistry registry;
    GameAllocator allocator(registry);

    REQUIRE(&allocator.allocate() == &registry);
}

TEST_CASE("allocate returns the same registry on repeated calls") {
    GameRegistry registry;
    GameAllocator allocator(registry);

    REQUIRE(&allocator.allocate() == &allocator.allocate());
}

TEST_CASE("a match created via the allocated registry is visible through the original registry") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);

    allocator.allocate().createMatch(makeBoard(), accounts, "alice", "bob");

    REQUIRE(registry.matchFor("alice") != nullptr);
    REQUIRE(registry.matchFor("alice") == registry.matchFor("bob"));
}

TEST_CASE("multi-shard allocate picks the least-loaded shard") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    shardA.createMatch(makeBoard(), accounts, "alice", "bob");   // shardA now has 1 match, shardB has 0

    GameAllocator allocator(std::vector<GameRegistry*>{&shardA, &shardB});

    REQUIRE(&allocator.allocate() == &shardB);
}

TEST_CASE("multi-shard allocate rebalances as load shifts between calls") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    GameAllocator allocator(std::vector<GameRegistry*>{&shardA, &shardB});

    allocator.allocate().createMatch(makeBoard(), accounts, "alice", "bob");
    allocator.allocate().createMatch(makeBoard(), accounts, "carol", "dave");

    REQUIRE(shardA.matchCount() == 1);
    REQUIRE(shardB.matchCount() == 1);
}

TEST_CASE("multi-shard allocate favors an emptier shard over one with more matches") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    GameRegistry shardC;
    shardA.createMatch(makeBoard(), accounts, "alice", "bob");
    shardA.createMatch(makeBoard(), accounts, "carol", "dave");
    shardB.createMatch(makeBoard(), accounts, "eve", "frank");

    GameAllocator allocator(std::vector<GameRegistry*>{&shardA, &shardB, &shardC});

    REQUIRE(&allocator.allocate() == &shardC);
}

TEST_CASE("constructing a multi-shard GameAllocator with no shards throws") {
    REQUIRE_THROWS_AS(GameAllocator(std::vector<GameRegistry*>{}), std::invalid_argument);
}
