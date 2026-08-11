#include "no_op_game_history_store.hpp"

void NoOpGameHistoryStore::recordGame(const std::string&, const std::string&) {}

std::vector<GameHistoryEntry> NoOpGameHistoryStore::historyFor(const std::string&, int) const {
    return {};
}
