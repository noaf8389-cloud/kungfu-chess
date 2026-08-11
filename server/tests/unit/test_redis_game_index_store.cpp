#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/redis_game_index_store.hpp"

namespace {
    // Real, containerized Redis -- see docker-compose.yml at the repo root.
    RedisGameIndexStore makeStore() { return RedisGameIndexStore("redis", 6379); }
}

TEST_CASE("RedisGameIndexStore: nextGameId returns a distinct id each call") {
    auto store = makeStore();
    auto first = store.nextGameId();
    auto second = store.nextGameId();
    REQUIRE(first != second);
}

TEST_CASE("RedisGameIndexStore: gameForUsername is nullopt before setUsernameGame") {
    auto store = makeStore();
    REQUIRE(store.gameForUsername("no-such-user-xyz").has_value() == false);
}

TEST_CASE("RedisGameIndexStore: setUsernameGame then gameForUsername round-trips") {
    auto store = makeStore();
    auto id = store.nextGameId();
    store.setUsernameGame("redis-test-user", id);
    REQUIRE(store.gameForUsername("redis-test-user").value() == id);
}

TEST_CASE("RedisGameIndexStore: removeUsernameGame clears the mapping") {
    auto store = makeStore();
    auto id = store.nextGameId();
    store.setUsernameGame("redis-test-user-2", id);
    store.removeUsernameGame("redis-test-user-2");
    REQUIRE(store.gameForUsername("redis-test-user-2").has_value() == false);
}

TEST_CASE("RedisGameIndexStore: setRoomCodeGame then gameForRoomCode round-trips") {
    auto store = makeStore();
    auto id = store.nextGameId();
    store.setRoomCodeGame("ZZ99YY", id);
    REQUIRE(store.gameForRoomCode("ZZ99YY").value() == id);
}

TEST_CASE("RedisGameIndexStore: removeRoomCodeGame clears the mapping") {
    auto store = makeStore();
    auto id = store.nextGameId();
    store.setRoomCodeGame("ZZ99YY2", id);
    store.removeRoomCodeGame("ZZ99YY2");
    REQUIRE(store.gameForRoomCode("ZZ99YY2").has_value() == false);
}
