// Server half of the handshake, roll sync and the marker command that advertises the mod to clients.

#include "levilamina/server/ServerState.h"

#include "levilamina/net/Packets.h"
#include "levilamina/net/Protocol.h"

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerDieEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/server/ServerPlayer.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/world/level/Level.h"

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <iterator>
#include <vector>

namespace do_a_barrel_roll::server {

namespace {

constexpr float        kSyncRange       = 128.0f;
constexpr std::int64_t kHeartbeatMs     = 250;
constexpr int          kHelloRetryTicks = 100;

std::atomic<bool> enabled{false};

struct Task {
    std::function<bool()> run; // returns false to retry on the next tick
    int                   retries = 0;
};

std::mutex        queueMutex;
std::vector<Task> queue;

void post(std::function<bool()> run, int retries = 0) {
    std::lock_guard lock(queueMutex);
    queue.push_back({std::move(run), retries});
}

void drainQueue() {
    std::vector<Task> tasks;
    {
        std::lock_guard lock(queueMutex);
        tasks.swap(queue);
    }
    std::vector<Task> retry;
    for (auto& task : tasks) {
        if (task.run() || task.retries <= 0) continue;
        --task.retries;
        retry.push_back(std::move(task));
    }
    if (retry.empty()) return;
    std::lock_guard lock(queueMutex);
    queue.insert(queue.end(), std::make_move_iterator(retry.begin()), std::make_move_iterator(retry.end()));
}

std::int64_t idOf(Player const& player) {
    return static_cast<std::int64_t>(const_cast<Player&>(player).getOrCreateUniqueID().rawID);
}

Player* playerFor(NetworkIdentifier const& id, SubClientId subId) {
    auto handler = ll::service::getServerNetworkHandler();
    return handler ? handler->_getServerPlayer(id, subId) : nullptr;
}

void sendConfig(Player& player, bool compatible) {
    net::ConfigSyncS2CPacket packet;
    packet.protocolVersion = net::PROTOCOL_VERSION;
    packet.compatible      = compatible;
    if (compatible) {
        // Operators ignore the server config, like the Java mod's ignore_config permission.
        packet.isLimited = player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors;
        packet.config    = packet.isLimited ? core::LimitedServerConfig{true, false} : config.limited();
    }
    {
        std::lock_guard lock(mutex);
        auto&           info = players[idOf(player)];
        info.helloMs         = nowMs();
        if (!compatible) info.handshake = HandshakeState::Failed;
        else if (info.handshake != HandshakeState::Accepted) info.handshake = HandshakeState::Sent;
        info.applied = packet.config;
    }
    packet.sendTo(player);
}

void broadcastRoll(Player& source, int dimension, bool rolling, float roll) {
    auto level = ll::service::getLevel();
    if (!level) return;

    net::RollSyncS2CPacket packet;
    packet.actorId = idOf(source);
    packet.rolling = rolling;
    packet.roll    = roll;

    std::vector<Player*> targets;
    {
        std::lock_guard lock(mutex);
        level->forEachPlayer([&](Player& other) {
            if (&other == &source || static_cast<int>(other.getDimensionId()) != dimension) return true;
            auto const it = players.find(idOf(other));
            if (it == players.end() || it->second.handshake != HandshakeState::Accepted) return true;
            if ((other.getPosition() - source.getPosition()).length() > kSyncRange) return true;
            targets.push_back(&other);
            return true;
        });
    }
    for (Player* target : targets) packet.sendTo(*target);
}

void broadcastRoll(Player& source, bool rolling, float roll) {
    broadcastRoll(source, static_cast<int>(source.getDimensionId()), rolling, roll);
}

void clearRoll(Player& player, int dimension) {
    bool wasRolling;
    {
        std::lock_guard lock(mutex);
        auto const      it = players.find(idOf(player));
        if (it == players.end()) return;
        wasRolling                     = it->second.rolling;
        it->second.rolling             = false;
        it->second.roll                = 0.0f;
        it->second.acceptClientUntilMs = 0;
    }
    if (wasRolling) broadcastRoll(player, dimension, false, 0.0f);
}

void handleHello(NetworkIdentifier const& id, SubClientId subId, int protocolVersion) {
    post(
        [=] {
            auto* player = playerFor(id, subId);
            if (!player) return false;
            sendConfig(*player, protocolVersion == net::PROTOCOL_VERSION);
            return true;
        },
        kHelloRetryTicks
    );
}

void handleResponse(NetworkIdentifier const& id, SubClientId subId, int protocolVersion, bool success) {
    post([=] {
        auto* player = playerFor(id, subId);
        if (!player) return true;
        std::lock_guard lock(mutex);
        auto const      it = players.find(idOf(*player));
        if (it == players.end() || it->second.handshake != HandshakeState::Sent) return true;
        bool const ok        = success && protocolVersion == net::PROTOCOL_VERSION;
        it->second.handshake = ok ? HandshakeState::Accepted : HandshakeState::Failed;
        return true;
    });
}

void handleRollSync(NetworkIdentifier const& id, SubClientId subId, net::RollSyncC2SPacket const& packet) {
    post([id,
          subId,
          rolling   = packet.rolling,
          rawRoll   = packet.roll,
          thrusting = packet.thrusting,
          v         = std::array<float, 3>{packet.velocity[0], packet.velocity[1], packet.velocity[2]}] {
        auto* player = playerFor(id, subId);
        if (!player || !std::isfinite(rawRoll)) return true;

        float const roll = rolling ? std::remainder(rawRoll, 360.0f) : 0.0f;
        bool        changed;
        {
            std::lock_guard lock(mutex);
            auto const      it = players.find(idOf(*player));
            if (it == players.end() || it->second.handshake != HandshakeState::Accepted) return true;
            auto& info   = it->second;
            changed      = info.rolling != rolling || std::abs(info.roll - roll) > 0.01f;
            info.rolling = rolling;
            info.roll    = roll;
            if (thrusting && info.applied.allowThrusting && player->isGliding() && std::isfinite(v[0])
                && std::isfinite(v[1]) && std::isfinite(v[2])) {
                float       scale  = 1.0f;
                float const length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (config.maxThrustSpeed > 0.0 && length > config.maxThrustSpeed) {
                    scale = static_cast<float>(config.maxThrustSpeed) / length;
                }
                for (int i = 0; i < 3; ++i) info.velocity[i] = v[i] * scale;
                info.acceptClientUntilMs = nowMs() + kThrustGraceMs;
            }
            if (changed) info.lastBroadcastMs = nowMs();
        }
        if (changed) broadcastRoll(*player, rolling, roll);
        return true;
    });
}

class HelloHandler : public ll::network::PacketHandlerBase<HelloHandler, net::HelloC2SPacket> {
public:
    void handlePacket(NetworkIdentifier const& id, NetEventCallback&, net::HelloC2SPacket const& packet) const {
        if (enabled) handleHello(id, packet.mSenderSubId, packet.protocolVersion);
    }
};

class ConfigResponseHandler
: public ll::network::PacketHandlerBase<ConfigResponseHandler, net::ConfigResponseC2SPacket> {
public:
    void
    handlePacket(NetworkIdentifier const& id, NetEventCallback&, net::ConfigResponseC2SPacket const& packet) const {
        if (enabled) handleResponse(id, packet.mSenderSubId, packet.protocolVersion, packet.success);
    }
};

class RollSyncHandler : public ll::network::PacketHandlerBase<RollSyncHandler, net::RollSyncC2SPacket> {
public:
    void handlePacket(NetworkIdentifier const& id, NetEventCallback&, net::RollSyncC2SPacket const& packet) const {
        if (enabled) handleRollSync(id, packet.mSenderSubId, packet);
    }
};

std::vector<ll::event::ListenerPtr> listeners;

void tickPlayers() {
    auto level = ll::service::getLevel();
    if (!level) return;
    auto const now = nowMs();
    level->forEachPlayer([&](Player& player) {
        int const dimension = static_cast<int>(player.getDimensionId());
        int       previous;
        bool      heartbeat = false;
        bool      rolling   = false;
        float     roll      = 0.0f;
        {
            std::lock_guard lock(mutex);
            auto const      it = players.find(idOf(player));
            if (it == players.end()) return true;
            auto& info     = it->second;
            previous       = info.dimension;
            info.dimension = dimension;
            if (info.rolling && now - info.lastBroadcastMs >= kHeartbeatMs) {
                info.lastBroadcastMs = now;
                heartbeat            = true;
                rolling              = info.rolling;
                roll                 = info.roll;
            }
        }
        if (previous >= 0 && previous != dimension) clearRoll(player, previous);
        else if (heartbeat) broadcastRoll(player, rolling, roll);
        return true;
    });
}

void kickUnmodded() {
    if (!config.forceInstalled) return;
    auto const                now = nowMs();
    std::vector<std::int64_t> late;
    {
        std::lock_guard lock(mutex);
        for (auto const& [id, info] : players) {
            auto const since = std::max(info.joinedMs, info.helloMs);
            if (info.handshake != HandshakeState::Accepted && now - since > config.installedTimeout * 50LL) {
                late.push_back(id);
            }
        }
    }
    auto level = ll::service::getLevel();
    if (!level) return;
    for (auto id : late) {
        if (auto* player = level->getPlayer(ActorUniqueID{id})) {
            player->disconnect("Please install the Do a Barrel Roll mod to play on this server.");
        }
    }
}

} // namespace

void installNet() {
    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        net::MARKER_COMMAND,
        "Shows whether Do a Barrel Roll is active on this server",
        CommandPermissionLevel::Any
    );
    command.overload().execute([](CommandOrigin const&, CommandOutput& output) {
        output.success("Do a Barrel Roll is installed on this server");
    });

    enabled   = true;
    auto& bus = ll::event::EventBus::getInstance();
    listeners.push_back(bus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) {
        std::lock_guard lock(mutex);
        players[idOf(event.self())].joinedMs = nowMs();
    }));
    listeners.push_back(
        bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& event) {
            auto const id = idOf(event.self());
            {
                std::lock_guard lock(mutex);
                players.erase(id);
            }
            broadcastRoll(event.self(), false, 0.0f);
        })
    );
    listeners.push_back(bus.emplaceListener<ll::event::PlayerDieEvent>([](ll::event::PlayerDieEvent& event) {
        if (enabled) clearRoll(event.self(), static_cast<int>(event.self().getDimensionId()));
    }));
    listeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](ll::event::ServerLevelTickEvent&) {
        if (!enabled) return;
        drainQueue();
        tickPlayers();
        static std::int64_t lastCheck = 0;
        auto const          now       = nowMs();
        if (now - lastCheck < 500) return;
        lastCheck = now;
        kickUnmodded();
    }));
}

void uninstallNet() {
    enabled = false;
    for (auto const& listener : listeners) ll::event::EventBus::getInstance().removeListener(listener);
    listeners.clear();
    std::lock_guard lock(mutex);
    players.clear();
    std::lock_guard queueLock(queueMutex);
    queue.clear();
}

} // namespace do_a_barrel_roll::server
