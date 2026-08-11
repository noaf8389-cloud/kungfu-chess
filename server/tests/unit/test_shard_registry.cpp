#include <stdexcept>
#include <vector>

#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/shard_registry.hpp"

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

TEST_CASE("ShardRegistry::matchFor finds a match no matter which shard it's actually on") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    shardB.createMatch(makeBoard(), accounts, "alice", "bob");   // lives only on shardB

    ShardRegistry registry(std::vector<GameRegistry*>{&shardA, &shardB});

    REQUIRE(registry.matchFor("alice") != nullptr);
    REQUIRE(registry.matchFor("alice") == shardB.matchFor("alice"));
}

TEST_CASE("ShardRegistry::matchFor returns nullptr when no shard has the username") {
    GameRegistry shardA;
    GameRegistry shardB;
    ShardRegistry registry(std::vector<GameRegistry*>{&shardA, &shardB});

    REQUIRE(registry.matchFor("ghost") == nullptr);
}

TEST_CASE("ShardRegistry::joinRoom finds a room no matter which shard created it") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    GameRegistry::RoomCode code = shardA.createRoom(makeBoard(), accounts, "alice");   // lives only on shardA

    ShardRegistry registry(std::vector<GameRegistry*>{&shardA, &shardB});

    GameMatch* joined = registry.joinRoom(code, "bob");
    REQUIRE(joined != nullptr);
    REQUIRE(joined == shardA.matchFor("alice"));
}

TEST_CASE("ShardRegistry::joinRoom returns nullptr for an unknown roomCode") {
    GameRegistry shardA;
    GameRegistry shardB;
    ShardRegistry registry(std::vector<GameRegistry*>{&shardA, &shardB});

    REQUIRE(registry.joinRoom("NO-SUCH-CODE", "bob") == nullptr);
}

TEST_CASE("ShardRegistry::updateAll advances every shard, not just the first") {
    FakeAccountStore accounts;
    GameRegistry shardA;
    GameRegistry shardB;
    shardA.createMatch(makeBoard(), accounts, "alice", "bob");
    shardB.createMatch(makeBoard(), accounts, "carol", "dave");

    ShardRegistry registry(std::vector<GameRegistry*>{&shardA, &shardB});

    REQUIRE_NOTHROW(registry.updateAll(50));
    REQUIRE(shardA.matchFor("alice") != nullptr);
    REQUIRE(shardB.matchFor("carol") != nullptr);
}

TEST_CASE("constructing a ShardRegistry with no shards throws") {
    REQUIRE_THROWS_AS(ShardRegistry(std::vector<GameRegistry*>{}), std::invalid_argument);
}
