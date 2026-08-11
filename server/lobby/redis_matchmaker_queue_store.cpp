#include "redis_matchmaker_queue_store.hpp"

#include <chrono>
#include <stdexcept>

#include <hiredis.h>

#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
    const char* kWaitingKey = "matchmaker:waiting";

    // Owns a redisReply so it's freed on every exit path, including exceptions.
    class ReplyGuard {
    public:
        explicit ReplyGuard(void* reply) : reply_(static_cast<redisReply*>(reply)) {}
        ReplyGuard(const ReplyGuard&) = delete;
        ReplyGuard& operator=(const ReplyGuard&) = delete;
        ~ReplyGuard() {
            if (reply_) freeReplyObject(reply_);
        }

        redisReply* operator->() const { return reply_; }
        redisReply* get() const { return reply_; }

    private:
        redisReply* reply_;
    };
}

void RedisMatchmakerQueueStore::ContextDeleter::operator()(redisContext* ctx) const {
    if (ctx) redisFree(ctx);
}

RedisMatchmakerQueueStore::RedisMatchmakerQueueStore(const std::string& host, int port) {
    redisContext* ctx = redisConnect(host.c_str(), port);
    if (ctx == nullptr) {
        throw std::runtime_error("RedisMatchmakerQueueStore: redisConnect returned null (out of memory)");
    }
    if (ctx->err) {
        std::string message = ctx->errstr;
        redisFree(ctx);
        throw std::runtime_error("RedisMatchmakerQueueStore: failed to connect to Redis at " + host + ":" +
                                  std::to_string(port) + ": " + message);
    }
    context_.reset(ctx);
}

RedisMatchmakerQueueStore::~RedisMatchmakerQueueStore() = default;

void RedisMatchmakerQueueStore::enqueue(const std::string& username, int rating) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check-then-set, not atomic -- safe today only because MatchmakerServer
    // runs as a single process (same caveat already documented for
    // GameRegistry::createRoom's roomCode generation). Would need HSETNX or
    // a Lua script if a second Matchmaker instance ever shared this store.
    ReplyGuard existing(redisCommand(context_.get(), "HEXISTS %s %s", kWaitingKey, username.c_str()));
    if (existing.get() && existing->type == REDIS_REPLY_INTEGER && existing->integer != 0) {
        return;
    }

    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    json value = {{"rating", rating}, {"joinedAtMs", nowMs}};
    ReplyGuard reply(redisCommand(context_.get(), "HSET %s %s %s", kWaitingKey, username.c_str(), value.dump().c_str()));
}

void RedisMatchmakerQueueStore::dequeue(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "HDEL %s %s", kWaitingKey, username.c_str()));
}

std::vector<MatchmakerQueueStore::WaitingPlayer> RedisMatchmakerQueueStore::allWaiting() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WaitingPlayer> result;
    ReplyGuard reply(redisCommand(context_.get(), "HGETALL %s", kWaitingKey));
    if (!reply.get() || reply->type != REDIS_REPLY_ARRAY) return result;

    for (std::size_t i = 0; i + 1 < reply->elements; i += 2) {
        std::string username = reply->element[i]->str;
        json value = json::parse(reply->element[i + 1]->str, nullptr, false);
        if (value.is_discarded()) continue;
        result.push_back(WaitingPlayer{username, value.value("rating", 1200), value.value("joinedAtMs", 0LL)});
    }
    return result;
}
