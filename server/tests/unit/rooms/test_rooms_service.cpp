#include <unordered_map>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../lobby/in_memory_match_queue.hpp"
#include "../../../lobby/matchmaker.hpp"
#include "../../../rooms/rooms_service.hpp"

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

TEST_CASE("createRoom returns the creator's own match") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    CreateRoomResult result = rooms.createRoom("alice");

    REQUIRE(result.match != nullptr);
    REQUIRE(result.match == registry.matchFor("alice"));
}

TEST_CASE("createRoom returns a roomCode that a second player can join") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    CreateRoomResult created = rooms.createRoom("alice");
    JoinRoomResult joined = rooms.joinRoom("bob", created.roomCode);

    REQUIRE(joined.success);
    REQUIRE(joined.match == created.match);
}

TEST_CASE("createRoom cancels a pending matchmaking search for the same username") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    matchmaker.enqueue("alice", 1200);
    REQUIRE(matchmaker.waitingCount() == 1);

    rooms.createRoom("alice");

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("joinRoom cancels a pending matchmaking search for the joiner") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    CreateRoomResult created = rooms.createRoom("alice");
    matchmaker.enqueue("bob", 1200);
    REQUIRE(matchmaker.waitingCount() == 1);

    rooms.joinRoom("bob", created.roomCode);

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("joinRoom with an unknown roomCode fails without a match") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    InMemoryMatchQueue matchQueue(matchmaker);
    RoomsService rooms(registry, allocator, accounts, matchQueue, makeBoard());

    JoinRoomResult result = rooms.joinRoom("bob", "no-such-code");

    REQUIRE(result.success == false);
    REQUIRE(result.match == nullptr);
}
