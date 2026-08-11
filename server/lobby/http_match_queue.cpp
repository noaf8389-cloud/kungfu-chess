#include "http_match_queue.hpp"

#include <httplib.h>

#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"

using json = nlohmann::json;

HttpMatchQueue::HttpMatchQueue(const std::string& host, int port)
    : client_(std::make_unique<httplib::Client>(host, port)) {}

HttpMatchQueue::~HttpMatchQueue() = default;

void HttpMatchQueue::enqueue(const std::string& username, int rating) {
    json body = {{"username", username}, {"rating", rating}};
    auto result = client_->Post("/internal/queue", body.dump(), "application/json");
    if (!result) {
        Logger::warn("HttpMatchQueue::enqueue: request failed for username=" + username);
    }
}

void HttpMatchQueue::dequeue(const std::string& username) {
    auto result = client_->Delete("/internal/queue/" + username);
    if (!result) {
        Logger::warn("HttpMatchQueue::dequeue: request failed for username=" + username);
    }
}

void HttpMatchQueue::setOnNoMatchFound(NoMatchFoundHandler handler) {
    onNoMatchFound_ = std::move(handler);
}

void HttpMatchQueue::notifyNoMatchFound(const std::string& username) {
    if (onNoMatchFound_) onNoMatchFound_(username);
}
