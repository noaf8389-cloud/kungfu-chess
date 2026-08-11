#include "matchmaker_internal_server.hpp"

#include <httplib.h>

#include "matchmaker.hpp"
#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

MatchmakerInternalServer::MatchmakerInternalServer(int port, Matchmaker& matchmaker)
    : matchmaker_(matchmaker), port_(port), server_(std::make_unique<httplib::Server>()) {

    server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server_->Post("/internal/queue", [this](const httplib::Request& req, httplib::Response& res) {
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
        int rating = body.value("rating", 1200);

        matchmaker_.enqueue(username, rating);
        res.status = 204;
    });

    server_->Delete(R"(/internal/queue/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string username = req.matches[1];
        matchmaker_.dequeue(username);
        res.status = 204;
    });
}

MatchmakerInternalServer::~MatchmakerInternalServer() {
    stop();
}

bool MatchmakerInternalServer::start() {
    if (!server_->bind_to_port("0.0.0.0", port_)) {
        Logger::error("MatchmakerInternalServer failed to bind port " + std::to_string(port_));
        return false;
    }
    listenThread_ = std::thread([this]() { server_->listen_after_bind(); });
    return true;
}

void MatchmakerInternalServer::stop() {
    server_->stop();
    if (listenThread_.joinable()) listenThread_.join();
}
