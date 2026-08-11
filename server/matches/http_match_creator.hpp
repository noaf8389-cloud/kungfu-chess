#pragma once

#include <memory>
#include <string>

#include "match_creator.hpp"

namespace httplib { class Client; }

// Calls out to a remote WS Gateway/shard process's internal HTTP API
// (ShardInternalServer) to actually create the match -- GameRegistry (and
// the live GameMatch) lives there, not in the Matchmaker process.
class HttpMatchCreator : public MatchCreator {
public:
    HttpMatchCreator(const std::string& host, int port);
    ~HttpMatchCreator();

    void createMatch(const std::string& player1Username, const std::string& player2Username) override;

private:
    std::unique_ptr<httplib::Client> client_;
};
