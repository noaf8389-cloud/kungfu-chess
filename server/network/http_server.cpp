#include "http_server.hpp"

#include <httplib.h>

#include "../db/player_account_store.hpp"
#include "../matches/no_op_game_history_store.hpp"
#include "../rooms/no_op_rooms_client.hpp"
#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

HttpServer::HttpServer(int port, PlayerAccountStore& accounts)
    : accounts_(accounts),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_),
      ownedRoomsClient_(std::make_unique<NoOpRoomsClient>()), roomsClient_(*ownedRoomsClient_),
      port_(port), server_(std::make_unique<httplib::Server>()) {
    registerRoutes();
}

HttpServer::HttpServer(int port, PlayerAccountStore& accounts, GameHistoryStore& historyStore)
    : accounts_(accounts), historyStore_(historyStore),
      ownedRoomsClient_(std::make_unique<NoOpRoomsClient>()), roomsClient_(*ownedRoomsClient_),
      port_(port), server_(std::make_unique<httplib::Server>()) {
    registerRoutes();
}

HttpServer::HttpServer(int port, PlayerAccountStore& accounts, GameHistoryStore& historyStore, RoomsClient& roomsClient)
    : accounts_(accounts), historyStore_(historyStore), roomsClient_(roomsClient),
      port_(port), server_(std::make_unique<httplib::Server>()) {
    registerRoutes();
}

void HttpServer::registerRoutes() {
    server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server_->Post("/login", [this](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (const json::parse_error&) {
            res.status = 400;
            return;
        }

        std::string username = body.value("username", "");
        std::string password = body.value("password", "");
        if (username.empty()) {
            res.status = 400;
            return;
        }

        LoginResult result = accounts_.loginOrRegister(username, password);

        json responseBody = {{"success", result.success}, {"rating", result.rating}};
        res.set_content(responseBody.dump(), "application/json");
        Logger::info("REST login attempt: username=" + username + " success=" + (result.success ? "true" : "false"));
    });

    server_->Get(R"(/players/([^/]+)/rating)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.matches[1];
        std::optional<int> rating = accounts_.ratingFor(username);
        if (!rating.has_value()) {
            res.status = 404;
            return;
        }
        json responseBody = {{"rating", *rating}};
        res.set_content(responseBody.dump(), "application/json");
    });

    server_->Get(R"(/players/([^/]+)/history)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.matches[1];
        constexpr int kDefaultLimit = 20;
        std::vector<GameHistoryEntry> entries = historyStore_.historyFor(username, kDefaultLimit);

        json responseBody = json::array();
        for (const auto& entry : entries) {
            responseBody.push_back({{"opponent", entry.opponent}, {"result", entry.result}, {"playedAtMs", entry.playedAtMs}});
        }
        res.set_content(responseBody.dump(), "application/json");
    });

    server_->Post("/rooms", [this](const httplib::Request& req, httplib::Response& res) {
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

        std::optional<std::string> roomCode = roomsClient_.createRoom(username);
        if (!roomCode.has_value()) {
            res.status = 503;
            return;
        }

        json responseBody = {{"roomCode", *roomCode}};
        res.status = 201;
        res.set_content(responseBody.dump(), "application/json");
        Logger::info("REST room created: username=" + username + " roomCode=" + *roomCode);
    });

    server_->Post(R"(/rooms/([^/]+)/join)", [this](const httplib::Request& req, httplib::Response& res) {
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

        bool success = roomsClient_.joinRoom(username, roomCode);
        json responseBody = {{"success", success}};
        res.status = success ? 200 : 404;
        res.set_content(responseBody.dump(), "application/json");
        Logger::info("REST join room: username=" + username + " roomCode=" + roomCode + " success=" + (success ? "true" : "false"));
    });
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (!server_->bind_to_port("0.0.0.0", port_)) {
        Logger::error("HTTP server failed to bind port " + std::to_string(port_));
        return false;
    }
    listenThread_ = std::thread([this]() { server_->listen_after_bind(); });
    return true;
}

void HttpServer::wait() {
    if (listenThread_.joinable()) listenThread_.join();
}

void HttpServer::stop() {
    server_->stop();
    if (listenThread_.joinable()) listenThread_.join();
}
