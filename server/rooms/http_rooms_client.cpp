#include "http_rooms_client.hpp"

#include <httplib.h>

#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

HttpRoomsClient::HttpRoomsClient(const std::string& host, int port)
    : client_(std::make_unique<httplib::Client>(host, port)) {}

HttpRoomsClient::~HttpRoomsClient() = default;

std::optional<std::string> HttpRoomsClient::createRoom(const std::string& username) {
    json body = {{"username", username}};
    auto result = client_->Post("/internal/rooms", body.dump(), "application/json");
    if (!result || result->status != 201) {
        Logger::warn("HttpRoomsClient::createRoom: request failed for " + username);
        return std::nullopt;
    }

    try {
        json responseBody = json::parse(result->body);
        return responseBody.value("roomCode", "");
    } catch (const json::parse_error&) {
        return std::nullopt;
    }
}

bool HttpRoomsClient::joinRoom(const std::string& username, const std::string& roomCode) {
    json body = {{"username", username}};
    auto result = client_->Post("/internal/rooms/" + roomCode + "/join", body.dump(), "application/json");
    if (!result) {
        Logger::warn("HttpRoomsClient::joinRoom: request failed for " + username);
        return false;
    }
    if (result->status != 200) return false;

    try {
        json responseBody = json::parse(result->body);
        return responseBody.value("success", false);
    } catch (const json::parse_error&) {
        return false;
    }
}
