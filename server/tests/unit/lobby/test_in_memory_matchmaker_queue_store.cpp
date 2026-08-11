#include "../../catch2/catch_amalgamated.hpp"
#include "../../../lobby/in_memory_matchmaker_queue_store.hpp"

TEST_CASE("InMemoryMatchmakerQueueStore: allWaiting is empty before any enqueue") {
    InMemoryMatchmakerQueueStore store;
    REQUIRE(store.allWaiting().empty());
}

TEST_CASE("InMemoryMatchmakerQueueStore: enqueue then allWaiting contains the player") {
    InMemoryMatchmakerQueueStore store;
    store.enqueue("alice", 1300);

    auto waiting = store.allWaiting();
    REQUIRE(waiting.size() == 1);
    REQUIRE(waiting[0].username == "alice");
    REQUIRE(waiting[0].rating == 1300);
}

TEST_CASE("InMemoryMatchmakerQueueStore: enqueue is ignored if already waiting") {
    InMemoryMatchmakerQueueStore store;
    store.enqueue("alice", 1300);
    store.enqueue("alice", 1500);

    auto waiting = store.allWaiting();
    REQUIRE(waiting.size() == 1);
    REQUIRE(waiting[0].rating == 1300);
}

TEST_CASE("InMemoryMatchmakerQueueStore: dequeue removes the player") {
    InMemoryMatchmakerQueueStore store;
    store.enqueue("alice", 1300);
    store.dequeue("alice");

    REQUIRE(store.allWaiting().empty());
}

TEST_CASE("InMemoryMatchmakerQueueStore: dequeuing a username that isn't waiting does nothing") {
    InMemoryMatchmakerQueueStore store;
    REQUIRE_NOTHROW(store.dequeue("ghost"));
}
