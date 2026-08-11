#include "../../catch2/catch_amalgamated.hpp"
#include "../../../lobby/redis_matchmaker_queue_store.hpp"

namespace {
    // Real, containerized Redis -- see docker-compose.yml at the repo root.
    RedisMatchmakerQueueStore makeStore() { return RedisMatchmakerQueueStore("redis", 6379); }

    bool containsUsername(const std::vector<MatchmakerQueueStore::WaitingPlayer>& waiting, const std::string& username) {
        for (const auto& player : waiting) {
            if (player.username == username) return true;
        }
        return false;
    }
}

TEST_CASE("RedisMatchmakerQueueStore: enqueue then allWaiting contains the player") {
    auto store = makeStore();
    store.dequeue("redis-mm-alice");   // clean slate in case a previous run left this behind

    store.enqueue("redis-mm-alice", 1300);
    REQUIRE(containsUsername(store.allWaiting(), "redis-mm-alice"));

    store.dequeue("redis-mm-alice");
}

TEST_CASE("RedisMatchmakerQueueStore: enqueue is ignored if already waiting") {
    auto store = makeStore();
    store.dequeue("redis-mm-bob");

    store.enqueue("redis-mm-bob", 1300);
    store.enqueue("redis-mm-bob", 1500);

    for (const auto& player : store.allWaiting()) {
        if (player.username == "redis-mm-bob") {
            REQUIRE(player.rating == 1300);
        }
    }

    store.dequeue("redis-mm-bob");
}

TEST_CASE("RedisMatchmakerQueueStore: dequeue removes the player") {
    auto store = makeStore();
    store.enqueue("redis-mm-carol", 1200);
    store.dequeue("redis-mm-carol");

    REQUIRE_FALSE(containsUsername(store.allWaiting(), "redis-mm-carol"));
}
