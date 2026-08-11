#pragma once

#include <memory>
#include <string>

#include "match_queue.hpp"

namespace httplib { class Client; }

// Calls out to a remote Matchmaker process's internal HTTP API
// (MatchmakerInternalServer) for enqueue/dequeue. setOnNoMatchFound stores
// the handler locally -- it's invoked via notifyNoMatchFound(), called by
// this process's own ShardInternalServer when Matchmaker pushes a timeout
// notification to /internal/no-match-found.
class HttpMatchQueue : public MatchQueue {
public:
    HttpMatchQueue(const std::string& host, int port);
    ~HttpMatchQueue();

    void enqueue(const std::string& username, int rating) override;
    void dequeue(const std::string& username) override;
    void setOnNoMatchFound(NoMatchFoundHandler handler) override;

    // Called by ShardInternalServer's /internal/no-match-found handler.
    void notifyNoMatchFound(const std::string& username);

private:
    std::unique_ptr<httplib::Client> client_;
    NoMatchFoundHandler onNoMatchFound_;
};
