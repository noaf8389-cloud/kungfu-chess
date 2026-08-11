#pragma once

#include <string>

#include "game_allocator.hpp"
#include "match_creator.hpp"
#include "db/player_account_store.hpp"
#include "logic/model/game_state.hpp"

// Today's behavior: creates the match directly, in-process, via GameAllocator.
// Owns its own copy of the starting board, same reasoning as RoomsService and
// (previously) Matchmaker itself owning one.
class InMemoryMatchCreator : public MatchCreator {
public:
    InMemoryMatchCreator(GameAllocator& gameAllocator, PlayerAccountStore& accounts, Board startingBoard);

    void createMatch(const std::string& player1Username, const std::string& player2Username) override;

private:
    GameAllocator& gameAllocator_;
    PlayerAccountStore& accounts_;
    Board startingBoard_;
};
