#pragma once

#include <mutex>
#include <unordered_map>

#include "matchmaker_queue_store.hpp"

class InMemoryMatchmakerQueueStore : public MatchmakerQueueStore {
public:
    void enqueue(const std::string& username, int rating) override;
    void dequeue(const std::string& username) override;
    std::vector<WaitingPlayer> allWaiting() const override;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, WaitingPlayer> waiting_;
};
