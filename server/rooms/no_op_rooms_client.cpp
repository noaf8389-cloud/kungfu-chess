#include "no_op_rooms_client.hpp"

std::optional<std::string> NoOpRoomsClient::createRoom(const std::string&) {
    return std::nullopt;
}

bool NoOpRoomsClient::joinRoom(const std::string&, const std::string&) {
    return false;
}
