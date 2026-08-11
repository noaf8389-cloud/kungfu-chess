#include "in_memory_match_queue.hpp"

#include "matchmaker.hpp"

InMemoryMatchQueue::InMemoryMatchQueue(Matchmaker& matchmaker) : matchmaker_(matchmaker) {}

void InMemoryMatchQueue::enqueue(const std::string& username, int rating) {
    matchmaker_.enqueue(username, rating);
}

void InMemoryMatchQueue::dequeue(const std::string& username) {
    matchmaker_.dequeue(username);
}

void InMemoryMatchQueue::setOnNoMatchFound(NoMatchFoundHandler handler) {
    matchmaker_.setOnNoMatchFound(std::move(handler));
}
