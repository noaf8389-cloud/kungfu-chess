#pragma once

#include "match_queue.hpp"

class Matchmaker;

// Today's behavior: forwards directly to an in-process Matchmaker.
class InMemoryMatchQueue : public MatchQueue {
public:
    explicit InMemoryMatchQueue(Matchmaker& matchmaker);

    void enqueue(const std::string& username, int rating) override;
    void dequeue(const std::string& username) override;
    void setOnNoMatchFound(NoMatchFoundHandler handler) override;

private:
    Matchmaker& matchmaker_;
};
