#include "http_match_creator.hpp"

#include <httplib.h>

#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

HttpMatchCreator::HttpMatchCreator(const std::string& host, int port)
    : client_(std::make_unique<httplib::Client>(host, port)) {}

HttpMatchCreator::~HttpMatchCreator() = default;

void HttpMatchCreator::createMatch(const std::string& player1Username, const std::string& player2Username) {
    json body = {{"player1Username", player1Username}, {"player2Username", player2Username}};
    auto result = client_->Post("/internal/matches", body.dump(), "application/json");
    if (!result) {
        Logger::warn("HttpMatchCreator::createMatch: request failed for " + player1Username + " vs " + player2Username);
    }
}
