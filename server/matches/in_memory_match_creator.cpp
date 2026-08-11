#include "in_memory_match_creator.hpp"

InMemoryMatchCreator::InMemoryMatchCreator(GameAllocator& gameAllocator, PlayerAccountStore& accounts, Board startingBoard)
    : gameAllocator_(gameAllocator), accounts_(accounts), startingBoard_(std::move(startingBoard)) {}

void InMemoryMatchCreator::createMatch(const std::string& player1Username, const std::string& player2Username) {
    gameAllocator_.allocate().createMatch(startingBoard_, accounts_, player1Username, player2Username);
}
