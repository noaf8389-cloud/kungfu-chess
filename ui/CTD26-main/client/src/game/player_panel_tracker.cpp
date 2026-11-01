#include "player_panel_tracker.hpp"
#include "bus/event_bus.hpp"
#include "bus/game_events.hpp"

namespace {
    std::string squareName(const Position& pos, int rowCount) {
        char file = static_cast<char>('a' + pos.col);
        int rank = rowCount - pos.row;
        return std::string(1, file) + std::to_string(rank);
    }

    char pieceLetter(const std::string& token) {
        if (token.size() != 2) return '\0';
        switch (token[1]) {
            case 'N': case 'B': case 'R': case 'Q': case 'K': return token[1];
            default: return '\0';   // pawn: no letter
        }
    }
}

PlayerPanelTracker::PlayerPanelTracker(EventBus& bus) {
    bus.subscribe<GameStartedEvent>([this](const GameStartedEvent& e) {
        rowCount_ = e.row_count;
        white_.username = e.white_username;
        black_.username = e.black_username;
    });

    bus.subscribe<PlayersUpdatedEvent>([this](const PlayersUpdatedEvent& e) {
        white_.username = e.white_username;
        black_.username = e.black_username;
    });

    bus.subscribe<ScoreUpdatedEvent>([this](const ScoreUpdatedEvent& e) {
        if (e.color == 'w') white_.score = e.new_score;
        else black_.score = e.new_score;
    });

    bus.subscribe<MoveMadeEvent>([this](const MoveMadeEvent& e) {
        char color = e.piece_token.empty() ? 'w' : e.piece_token[0];
        char letter = pieceLetter(e.piece_token);
        std::string dest = squareName(e.to, rowCount_);
        std::string line = letter ? std::string(1, letter) + dest : dest;

        std::vector<std::string>& moves = (color == 'w') ? white_.moves : black_.moves;
        moves.push_back(line);

        pendingMove_ = PendingMove{e.to, color, letter == '\0', e.from.col};
    });

    bus.subscribe<PieceCapturedEvent>([this](const PieceCapturedEvent& e) {
        if (!pendingMove_.has_value() || !(pendingMove_->to == e.at)) return;

        std::vector<std::string>& moves = (pendingMove_->color == 'w') ? white_.moves : black_.moves;
        if (moves.empty()) return;
        std::string& last = moves.back();

        if (pendingMove_->isPawn) {
            char fromFile = static_cast<char>('a' + pendingMove_->fromCol);
            last = std::string(1, fromFile) + "x" + last;
        } else {
            last.insert(1, "x");   // "Nf3" -> "Nxf3"
        }
    });
}
