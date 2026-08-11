#include "../catch2/catch_amalgamated.hpp"
#include "../../matches/in_memory_game_index_store.hpp"

TEST_CASE("InMemoryGameIndexStore: nextGameId returns a distinct id each call") {
    InMemoryGameIndexStore store;
    auto first = store.nextGameId();
    auto second = store.nextGameId();
    REQUIRE(first != second);
}

TEST_CASE("InMemoryGameIndexStore: gameForUsername is nullopt before setUsernameGame") {
    InMemoryGameIndexStore store;
    REQUIRE(store.gameForUsername("noa").has_value() == false);
}

TEST_CASE("InMemoryGameIndexStore: setUsernameGame then gameForUsername round-trips") {
    InMemoryGameIndexStore store;
    store.setUsernameGame("noa", 7);
    REQUIRE(store.gameForUsername("noa").value() == 7);
}

TEST_CASE("InMemoryGameIndexStore: removeUsernameGame clears the mapping") {
    InMemoryGameIndexStore store;
    store.setUsernameGame("noa", 7);
    store.removeUsernameGame("noa");
    REQUIRE(store.gameForUsername("noa").has_value() == false);
}

TEST_CASE("InMemoryGameIndexStore: setRoomCodeGame then gameForRoomCode round-trips") {
    InMemoryGameIndexStore store;
    store.setRoomCodeGame("AB12CD", 3);
    REQUIRE(store.gameForRoomCode("AB12CD").value() == 3);
}

TEST_CASE("InMemoryGameIndexStore: removeRoomCodeGame clears the mapping") {
    InMemoryGameIndexStore store;
    store.setRoomCodeGame("AB12CD", 3);
    store.removeRoomCodeGame("AB12CD");
    REQUIRE(store.gameForRoomCode("AB12CD").has_value() == false);
}

TEST_CASE("InMemoryGameIndexStore: username and roomCode indices are independent") {
    InMemoryGameIndexStore store;
    store.setUsernameGame("noa", 1);
    store.setRoomCodeGame("AB12CD", 2);

    REQUIRE(store.gameForUsername("noa").value() == 1);
    REQUIRE(store.gameForRoomCode("AB12CD").value() == 2);
}
