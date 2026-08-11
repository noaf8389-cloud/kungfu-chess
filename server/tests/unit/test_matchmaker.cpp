#include <chrono>
#include <thread>
#include <unordered_map>

#include "../catch2/catch_amalgamated.hpp"
#include "../../lobby/matchmaker.hpp"

namespace {
    class FakeAccountStore : public PlayerAccountStore {
    public:
        LoginResult loginOrRegister(const std::string&, const std::string&) override { return {}; }
        void updateRating(const std::string& username, int newRating) override { ratings[username] = newRating; }
        std::optional<int> ratingFor(const std::string& username) const override {
            auto it = ratings.find(username);
            return it != ratings.end() ? std::optional<int>(it->second) : std::nullopt;
        }
        std::unordered_map<std::string, int> ratings;
    };

    Board makeBoard() {
        Board board(4, 4);
        board.setCell(0, 0, "wR");
        return board;
    }
}

TEST_CASE("enqueue adds a player to the waiting queue") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());

    matchmaker.enqueue("alice", 1200);

    REQUIRE(matchmaker.waitingCount() == 1);
}

TEST_CASE("enqueue does nothing if the same username is already waiting") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());

    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("alice", 1500);

    REQUIRE(matchmaker.waitingCount() == 1);
}

TEST_CASE("dequeue removes a waiting player") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);

    matchmaker.dequeue("alice");

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("dequeuing a username that isn't waiting does nothing") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());

    REQUIRE_NOTHROW(matchmaker.dequeue("ghost"));
}

TEST_CASE("tick matches two players within the rating range and removes them from the queue") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1250);

    matchmaker.tick();

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("tick actually creates the match via GameRegistry") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1250);

    matchmaker.tick();

    REQUIRE(registry.matchFor("alice") != nullptr);
    REQUIRE(registry.matchFor("alice") == registry.matchFor("bob"));
}

TEST_CASE("tick does not match two players outside the rating range") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1400);

    matchmaker.tick();

    REQUIRE(matchmaker.waitingCount() == 2);
    REQUIRE(registry.matchCount() == 0);
}

TEST_CASE("tick matches a pair exactly at the edge of the rating range") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1300);   // exactly 100 apart

    matchmaker.tick();

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("tick can form multiple independent pairs in the same call") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1250);
    matchmaker.enqueue("carol", 1800);
    matchmaker.enqueue("dave", 1850);

    matchmaker.tick();

    REQUIRE(matchmaker.waitingCount() == 0);
    REQUIRE(registry.matchCount() == 2);
}

TEST_CASE("a player who has waited past the timeout without a match fires onNoMatchFound") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard(), /*timeoutMs=*/1);
    matchmaker.enqueue("alice", 1200);

    std::string notified;
    matchmaker.setOnNoMatchFound([&](const std::string& username) { notified = username; });

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    matchmaker.tick();

    REQUIRE(notified == "alice");
    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("a player matched before the timeout does not fire onNoMatchFound") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard(), /*timeoutMs=*/60000);
    matchmaker.enqueue("alice", 1200);
    matchmaker.enqueue("bob", 1250);

    bool called = false;
    matchmaker.setOnNoMatchFound([&](const std::string&) { called = true; });

    matchmaker.tick();

    REQUIRE(called == false);
}

TEST_CASE("onNoMatchFound does not fire for a player still within the timeout window") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard(), /*timeoutMs=*/60000);
    matchmaker.enqueue("alice", 1200);

    bool called = false;
    matchmaker.setOnNoMatchFound([&](const std::string&) { called = true; });

    matchmaker.tick();

    REQUIRE(called == false);
    REQUIRE(matchmaker.waitingCount() == 1);
}

TEST_CASE("a timeout eviction with no handler set does not crash") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard(), /*timeoutMs=*/1);
    matchmaker.enqueue("alice", 1200);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    REQUIRE_NOTHROW(matchmaker.tick());
}
