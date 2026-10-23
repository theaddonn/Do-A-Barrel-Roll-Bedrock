#pragma once

#include "levilamina/core/ServerConfig.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>

class Player;

namespace do_a_barrel_roll::server {

constexpr std::int64_t kThrustGraceMs = 2000;

enum class HandshakeState { NotSent, Sent, Accepted, Failed };

struct PlayerNet {
    HandshakeState            handshake       = HandshakeState::NotSent;
    std::int64_t              joinedMs        = 0;
    std::int64_t              helloMs         = 0;
    int                       dimension       = -1;
    std::int64_t              lastBroadcastMs = 0;
    core::LimitedServerConfig applied;
    bool                      rolling             = false;
    float                     roll                = 0.0f;
    std::int64_t              acceptClientUntilMs = 0; // thrust: accept the client position until then
    float                     velocity[3]{};
};

inline core::ServerConfig                          config;
inline std::filesystem::path                       configPath;
inline std::mutex                                  mutex;
inline std::unordered_map<std::int64_t, PlayerNet> players; // by actor unique id

[[nodiscard]] inline std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void installNet();
void uninstallNet();
void installThrust();
void uninstallThrust();

} // namespace do_a_barrel_roll::server
