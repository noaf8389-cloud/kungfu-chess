#pragma once

#include <memory>
#include <string>

// nats.h declares this as `typedef struct __natsConnection natsConnection;` — the
// forward declaration has to match that shape exactly, not `struct natsConnection;`.
struct __natsConnection;
typedef struct __natsConnection natsConnection;

// A best-effort, non-blocking publisher to NATS — Phase 4's proof that a real
// inter-service message channel works, before Phase 5 makes anything actually
// depend on it.
class NatsPublisher {
public:
    // Disconnected by default — publish() is then always a no-op, no connection
    // is attempted. Useful wherever a "no NATS" default is needed (e.g. GameRegistry's
    // default constructor).
    NatsPublisher();
    // natsUrl, e.g. "nats://localhost:4222". Never throws — connection failure
    // is logged and leaves this publisher permanently inert (isConnected() == false).
    explicit NatsPublisher(const std::string& natsUrl);
    ~NatsPublisher();

    bool isConnected() const;

    // Publishes payload (already-serialized, e.g. JSON) to subject. Does nothing if not connected.
    void publish(const std::string& subject, const std::string& payload);

private:
    struct ConnectionDeleter { void operator()(natsConnection* conn) const; };
    std::unique_ptr<natsConnection, ConnectionDeleter> connection_;
};
