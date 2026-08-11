#include "redis_game_index_store.hpp"

#include <stdexcept>

#include <hiredis.h>

namespace {
    std::string usernameKey(const std::string& username) { return "game:username:" + username; }
    std::string roomCodeKey(const std::string& roomCode) { return "game:roomcode:" + roomCode; }

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

void RedisGameIndexStore::ContextDeleter::operator()(redisContext* ctx) const {
    if (ctx) redisFree(ctx);
}

RedisGameIndexStore::RedisGameIndexStore(const std::string& host, int port) {
    redisContext* ctx = redisConnect(host.c_str(), port);
    if (ctx == nullptr) {
        throw std::runtime_error("RedisGameIndexStore: redisConnect returned null (out of memory)");
    }
    if (ctx->err) {
        std::string message = ctx->errstr;
        redisFree(ctx);
        throw std::runtime_error("RedisGameIndexStore: failed to connect to Redis at " + host + ":" +
                                  std::to_string(port) + ": " + message);
    }
    context_.reset(ctx);
}

RedisGameIndexStore::~RedisGameIndexStore() = default;

GameIndexStore::GameId RedisGameIndexStore::nextGameId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "INCR game:next_id"));
    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER) {
        throw std::runtime_error("RedisGameIndexStore::nextGameId: unexpected reply from INCR");
    }
    return static_cast<GameId>(reply->integer);
}

void RedisGameIndexStore::setUsernameGame(const std::string& username, GameId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string idStr = std::to_string(id);
    ReplyGuard reply(redisCommand(context_.get(), "SET %s %s", usernameKey(username).c_str(), idStr.c_str()));
}

std::optional<GameIndexStore::GameId> RedisGameIndexStore::gameForUsername(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "GET %s", usernameKey(username).c_str()));
    if (!reply.get() || reply->type != REDIS_REPLY_STRING) return std::nullopt;
    return static_cast<GameId>(std::stoull(reply->str));
}

void RedisGameIndexStore::removeUsernameGame(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "DEL %s", usernameKey(username).c_str()));
}

void RedisGameIndexStore::setRoomCodeGame(const std::string& roomCode, GameId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string idStr = std::to_string(id);
    ReplyGuard reply(redisCommand(context_.get(), "SET %s %s", roomCodeKey(roomCode).c_str(), idStr.c_str()));
}

std::optional<GameIndexStore::GameId> RedisGameIndexStore::gameForRoomCode(const std::string& roomCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "GET %s", roomCodeKey(roomCode).c_str()));
    if (!reply.get() || reply->type != REDIS_REPLY_STRING) return std::nullopt;
    return static_cast<GameId>(std::stoull(reply->str));
}

void RedisGameIndexStore::removeRoomCodeGame(const std::string& roomCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "DEL %s", roomCodeKey(roomCode).c_str()));
}
