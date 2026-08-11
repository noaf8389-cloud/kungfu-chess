#pragma once

#include <string>

// Abstraction over "create a match for these two players." Matchmaker calls
// this once it pairs two waiting players -- it doesn't need to know whether
// the match is created in-process (today, via GameAllocator) or by asking a
// separate shard/WS-Gateway process over the network (once Matchmaker runs
// as its own process, Phase 5.2/5.3).
class MatchCreator {
public:
    virtual ~MatchCreator() = default;

    virtual void createMatch(const std::string& player1Username, const std::string& player2Username) = 0;
};
