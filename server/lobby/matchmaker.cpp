#include "matchmaker.hpp"

#include <algorithm>
#include <cmath>

#include "in_memory_matchmaker_queue_store.hpp"
#include "matches/in_memory_match_creator.hpp"

Matchmaker::Matchmaker(GameAllocator& gameAllocator, PlayerAccountStore& accounts, Board startingBoard,
                        long long timeoutMs)
    : ownedMatchCreator_(std::make_unique<InMemoryMatchCreator>(gameAllocator, accounts, std::move(startingBoard))),
      matchCreator_(*ownedMatchCreator_),
      ownedQueueStore_(std::make_unique<InMemoryMatchmakerQueueStore>()),
      queueStore_(*ownedQueueStore_),
      timeoutMs_(timeoutMs) {}

Matchmaker::Matchmaker(MatchCreator& matchCreator, long long timeoutMs)
    : matchCreator_(matchCreator),
      ownedQueueStore_(std::make_unique<InMemoryMatchmakerQueueStore>()),
      queueStore_(*ownedQueueStore_),
      timeoutMs_(timeoutMs) {}

Matchmaker::Matchmaker(MatchCreator& matchCreator, MatchmakerQueueStore& queueStore, long long timeoutMs)
    : matchCreator_(matchCreator), queueStore_(queueStore), timeoutMs_(timeoutMs) {}

void Matchmaker::enqueue(const std::string& username, int rating) {
    queueStore_.enqueue(username, rating);
}

void Matchmaker::dequeue(const std::string& username) {
    queueStore_.dequeue(username);
}

void Matchmaker::tick() {
    std::vector<MatchmakerQueueStore::WaitingPlayer> waiting = queueStore_.allWaiting();
    std::vector<std::string> timedOut;

    bool foundPair = true;
    while (foundPair) {
        foundPair = false;
        for (std::size_t i = 0; i < waiting.size() && !foundPair; ++i) {
            for (std::size_t j = i + 1; j < waiting.size(); ++j) {
                if (std::abs(waiting[i].rating - waiting[j].rating) <= kRatingRange) {
                    std::string player1 = waiting[i].username;
                    std::string player2 = waiting[j].username;

                    queueStore_.dequeue(player1);
                    queueStore_.dequeue(player2);
                    waiting.erase(waiting.begin() + j);
                    waiting.erase(waiting.begin() + i);

                    matchCreator_.createMatch(player1, player2);

                    foundPair = true;
                    break;
                }
            }
        }
    }

    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    for (const auto& player : waiting) {
        if (nowMs - player.joinedAtMs >= timeoutMs_) {
            queueStore_.dequeue(player.username);
            timedOut.push_back(player.username);
        }
    }

    // Fired outside mutex_: onNoMatchFound_ is caller-supplied code (e.g. it may
    // need to lock GameWebSocketServer's own connection state), and that caller also
    // calls back into enqueue()/dequeue() while holding its own lock. Calling the
    // handler while still holding mutex_ here would risk a lock-order inversion
    // between the two mutexes.
    NoMatchFoundHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler = onNoMatchFound_;
    }
    if (handler) {
        for (const std::string& username : timedOut) {
            handler(username);
        }
    }
}

void Matchmaker::setOnNoMatchFound(NoMatchFoundHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    onNoMatchFound_ = std::move(handler);
}

std::size_t Matchmaker::waitingCount() const {
    return queueStore_.allWaiting().size();
}
