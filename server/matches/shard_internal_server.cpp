#include "shard_internal_server.hpp"

#include <httplib.h>

#include "match_creator.hpp"
#include "../rooms/rooms_service.hpp"
#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

ShardInternalServer::ShardInternalServer(int port, MatchCreator& matchCreator, RoomsService& roomsService, NoMatchFoundRelay onNoMatchFound)
    : matchCreator_(matchCreator), roomsService_(roomsService), onNoMatchFound_(std::move(onNoMatchFound)),
      port_(port), server_(std::make_unique<httplib::Server>()) {

    server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server_->Post("/internal/matches", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error&) {
            res.status = 400;
            return;
        }

        std::string player1 = body.value("player1Username", "");
        std::string player2 = body.value("player2Username", "");
        if (player1.empty() || player2.empty()) {
            res.status = 400;
            return;
        }

        matchCreator_.createMatch(player1, player2);
        res.status = 204;
    });

    server_->Post("/internal/rooms", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error&) {
            res.status = 400;
            return;
        }

        std::string username = body.value("username", "");
        if (username.empty()) {
            res.status = 400;
            return;
        }

        CreateRoomResult result = roomsService_.createRoom(username);
        json responseBody = {{"roomCode", result.roomCode}};
        res.status = 201;
        res.set_content(responseBody.dump(), "application/json");
    });

    server_->Post(R"(/internal/rooms/([^/]+)/join)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string roomCode = req.matches[1];
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error&) {
            res.status = 400;
            return;
        }

        std::string username = body.value("username", "");
        if (username.empty()) {
            res.status = 400;
            return;
        }

        JoinRoomResult result = roomsService_.joinRoom(username, roomCode);
        json responseBody = {{"success", result.success}};
        res.status = result.success ? 200 : 404;
        res.set_content(responseBody.dump(), "application/json");
    });

    server_->Post("/internal/no-match-found", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error&) {
            res.status = 400;
            return;
        }

        std::string username = body.value("username", "");
        if (username.empty()) {
            res.status = 400;
            return;
        }

        if (onNoMatchFound_) onNoMatchFound_(username);
        res.status = 204;
    });
}

ShardInternalServer::~ShardInternalServer() {
    stop();
}

bool ShardInternalServer::start() {
    if (!server_->bind_to_port("0.0.0.0", port_)) {
        Logger::error("ShardInternalServer failed to bind port " + std::to_string(port_));
        return false;
    }
    listenThread_ = std::thread([this]() { server_->listen_after_bind(); });
    return true;
}

void ShardInternalServer::stop() {
    server_->stop();
    if (listenThread_.joinable()) listenThread_.join();
}
