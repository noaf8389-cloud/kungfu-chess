#include "rating_updater.hpp"
#include "elo_calculator.hpp"

#include "bus/event_bus.hpp"
#include "bus/game_events.hpp"
#include "../matches/no_op_game_history_store.hpp"
#include "../player_registry.hpp"
#include "../db/player_account_store.hpp"
#include "logging/logger.hpp"

RatingUpdater::RatingUpdater(EventBus& bus, PlayerRegistry& registry, PlayerAccountStore& accounts)
    : registry_(registry), accounts_(accounts),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_) {
    bus.subscribe<GameOverEvent>([this](const GameOverEvent& event) { onGameOver(event); });
}

RatingUpdater::RatingUpdater(EventBus& bus, PlayerRegistry& registry, PlayerAccountStore& accounts, GameHistoryStore& historyStore)
    : registry_(registry), accounts_(accounts), historyStore_(historyStore) {
    bus.subscribe<GameOverEvent>([this](const GameOverEvent& event) { onGameOver(event); });
}

void RatingUpdater::onGameOver(const GameOverEvent& event) {
    char loserColor = (event.winner_color == 'w') ? 'b' : 'w';

    std::optional<std::string> winnerName = registry_.usernameForColor(event.winner_color);
    std::optional<std::string> loserName = registry_.usernameForColor(loserColor);
    if (!winnerName.has_value() || !loserName.has_value()) {
        Logger::warn("GameOverEvent with no logged-in username for one or both colors; rating not updated");
        return;
    }

    std::optional<int> winnerRating = accounts_.ratingFor(*winnerName);
    std::optional<int> loserRating = accounts_.ratingFor(*loserName);
    if (!winnerRating.has_value() || !loserRating.has_value()) {
        Logger::warn("GameOverEvent but no stored rating for " + *winnerName + " or " + *loserName + "; rating not updated");
        return;
    }

    EloResult result = calculateElo(*winnerRating, *loserRating);
    accounts_.updateRating(*winnerName, result.newWinnerRating);
    accounts_.updateRating(*loserName, result.newLoserRating);
    Logger::info("Rating update: " + *winnerName + " " + std::to_string(*winnerRating) + "->" +
                 std::to_string(result.newWinnerRating) + ", " + *loserName + " " +
                 std::to_string(*loserRating) + "->" + std::to_string(result.newLoserRating));

    historyStore_.recordGame(*winnerName, *loserName);
}
