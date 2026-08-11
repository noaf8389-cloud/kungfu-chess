#include <chrono>
#include <thread>
#include <unordered_map>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../lobby/http_match_queue.hpp"
#include "../../../lobby/matchmaker_internal_server.hpp"
#include "../../../lobby/matchmaker.hpp"
#include "../../../matches/game_allocator.hpp"
#include "../../../matches/game_registry.hpp"

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

TEST_CASE("HttpMatchQueue::enqueue reaches a real MatchmakerInternalServer") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18093, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();

    HttpMatchQueue matchQueue("127.0.0.1", 18093);
    matchQueue.enqueue("alice", 1300);

    REQUIRE(matchmaker.waitingCount() == 1);
}

TEST_CASE("HttpMatchQueue::dequeue reaches a real MatchmakerInternalServer") {
    FakeAccountStore accounts;
    GameRegistry registry;
    GameAllocator allocator(registry);
    Matchmaker matchmaker(allocator, accounts, makeBoard());
    MatchmakerInternalServer server(18094, matchmaker);
    REQUIRE(server.start());
    waitForServerReady();
    matchmaker.enqueue("bob", 1200);

    HttpMatchQueue matchQueue("127.0.0.1", 18094);
    matchQueue.dequeue("bob");

    REQUIRE(matchmaker.waitingCount() == 0);
}

TEST_CASE("HttpMatchQueue::notifyNoMatchFound invokes the locally registered handler") {
    HttpMatchQueue matchQueue("127.0.0.1", 1);   // never actually connects for this test

    std::string notified;
    matchQueue.setOnNoMatchFound([&](const std::string& username) { notified = username; });

    matchQueue.notifyNoMatchFound("carol");

    REQUIRE(notified == "carol");
}

TEST_CASE("HttpMatchQueue::notifyNoMatchFound does nothing when no handler is registered") {
    HttpMatchQueue matchQueue("127.0.0.1", 1);

    REQUIRE_NOTHROW(matchQueue.notifyNoMatchFound("dave"));
}
