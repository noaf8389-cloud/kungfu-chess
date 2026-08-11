#include "in_memory_matchmaker_queue_store.hpp"

#include <chrono>

void InMemoryMatchmakerQueueStore::enqueue(const std::string& username, int rating) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (waiting_.count(username)) return;

    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    waiting_[username] = WaitingPlayer{username, rating, nowMs};
}

void InMemoryMatchmakerQueueStore::dequeue(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    waiting_.erase(username);
}

std::vector<MatchmakerQueueStore::WaitingPlayer> InMemoryMatchmakerQueueStore::allWaiting() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WaitingPlayer> result;
    result.reserve(waiting_.size());
    for (const auto& [username, player] : waiting_) {
        result.push_back(player);
    }
    return result;
}
