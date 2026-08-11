#include "../../catch2/catch_amalgamated.hpp"
#include "../../../rooms/no_op_rooms_client.hpp"

TEST_CASE("NoOpRoomsClient: createRoom fails, joinRoom fails") {
    NoOpRoomsClient client;

    REQUIRE_FALSE(client.createRoom("alice").has_value());
    REQUIRE_FALSE(client.joinRoom("bob", "ANYCODE"));
}
