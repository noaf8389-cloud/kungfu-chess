#pragma once

#include <string>

#include "third_party/nlohmann/json.hpp"
#include "game_events.hpp"

using json = nlohmann::json;

inline void to_json(json& j, const Position& p) {
    j = json{{"row", p.row}, {"col", p.col}};
}

inline void from_json(const json& j, Position& p) {
    j.at("row").get_to(p.row);
    j.at("col").get_to(p.col);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MoveMadeEvent, from, to, piece_token, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PieceCapturedEvent, at, capturing_piece_token, captured_piece_token, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameStartedEvent, row_count, col_count, cells, white_username, black_username, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MoveStartedEvent, from, to, piece_token, duration_ms, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JumpStartedEvent, position, piece_token, duration_ms, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JumpLandedEvent, position, piece_token, timestamp_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RestEndedEvent, position, piece_token, timestamp_ms)

inline void to_json(json& j, const GameOverEvent& e) {
    j = json{{"winner_color", std::string(1, e.winner_color)}, {"timestamp_ms", e.timestamp_ms}};
}

inline void from_json(const json& j, GameOverEvent& e) {
    e.winner_color = j.at("winner_color").get<std::string>().at(0);
    j.at("timestamp_ms").get_to(e.timestamp_ms);
}

inline void to_json(json& j, const ScoreUpdatedEvent& e) {
    j = json{{"color", std::string(1, e.color)}, {"new_score", e.new_score}, {"timestamp_ms", e.timestamp_ms}};
}

inline void from_json(const json& j, ScoreUpdatedEvent& e) {
    e.color = j.at("color").get<std::string>().at(0);
    j.at("new_score").get_to(e.new_score);
    j.at("timestamp_ms").get_to(e.timestamp_ms);
}

inline void to_json(json& j, const PlayerDisconnectedEvent& e) {
    j = json{{"color", std::string(1, e.color)}, {"grace_duration_ms", e.grace_duration_ms}, {"timestamp_ms", e.timestamp_ms}};
}
inline void from_json(const json& j, PlayerDisconnectedEvent& e) {
    e.color = j.at("color").get<std::string>().at(0);
    j.at("grace_duration_ms").get_to(e.grace_duration_ms);
    j.at("timestamp_ms").get_to(e.timestamp_ms);
}

inline void to_json(json& j, const PlayerReconnectedEvent& e) {
    j = json{{"color", std::string(1, e.color)}, {"timestamp_ms", e.timestamp_ms}};
}
inline void from_json(const json& j, PlayerReconnectedEvent& e) {
    e.color = j.at("color").get<std::string>().at(0);
    j.at("timestamp_ms").get_to(e.timestamp_ms);
}

// The single place every event-to-wire-format conversion goes through — whoever
// needs to serialize an event for sending (WebSocket broadcast, NATS mirror, ...)
// calls this instead of building its own {"type", "payload"} shape. If an event's
// structure changes, only its to_json above needs updating.
template <typename EventT>
std::string toEnvelopeJson(const char* typeName, const EventT& event) {
    json j;
    j["type"] = typeName;
    j["payload"] = event;
    return j.dump();
}
