// Client half of the server handshake: detects the server mod, applies its config, reports the own roll and thrust
// state and stores the roll of other players.
//
// The marker command is announced in the server's command list. Only then are LeviLamina runtime packets sent, so a
// vanilla server never receives one.

#include "levilamina/hooks/Net.h"

#include "levilamina/hooks/State.h"
#include "levilamina/net/Packets.h"
#include "levilamina/net/Protocol.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/command/ClientCommandRegisterEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/network/packet/Packet.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/player/LocalPlayer.h"

#include <atomic>
#include <cmath>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace do_a_barrel_roll::hooks {

namespace {

constexpr std::int64_t kFlightMemoryMs = 120000;
constexpr std::int64_t kFlyingFreshMs  = 150;
constexpr std::int64_t kRollExpiryMs   = 500;
constexpr long long    kHelloPollTicks = 100;

struct RemoteRoll {
    float        roll;
    std::int64_t updatedMs;
};

struct SentState {
    bool  rolling   = false;
    float roll      = 0.0f;
    bool  thrusting = false;
};

struct Inbound {
    bool                      isConfig = false;
    core::LimitedServerConfig config;
    int                       protocolVersion = 0;
    bool                      compatible      = true;
    std::int64_t              actorId         = 0;
    bool                      rolling         = false;
    float                     roll            = 0.0f;
};

std::atomic<bool>                            enabled{false};
std::mutex                                   mutex;
std::unordered_map<std::int64_t, RemoteRoll> rolls;
core::LimitedServerConfig                    applied;
std::vector<Inbound>                         inbox;
bool                                         connected      = false;
bool                                         helloSent      = false;
long long                                    helloPollUntil = 0;
std::optional<int>                           lastDimension;
SentState                                    lastSent;

ll::event::ListenerPtr commandListener;
ll::event::ListenerPtr exitListener;

void clearOverlay() {
    state::config.serverForceEnabled = false;
    state::config.serverBlockThrust  = false;
}

void clearRemote() {
    rolls.clear();
    lastSent = {};
}

void resetSession() {
    std::lock_guard lock(mutex);
    connected      = false;
    helloSent      = false;
    helloPollUntil = 0;
    lastDimension.reset();
    inbox.clear();
    clearRemote();
    clearOverlay();
}

void sendHello() {
    net::HelloC2SPacket packet;
    packet.protocolVersion = net::PROTOCOL_VERSION;
    packet.sendToServer();
}

void sendState() {
    bool const  rolling = state::cameraOverride;
    float const roll    = rolling ? state::cameraRollDeg : 0.0f;
    auto const  now     = state::nowMs();
    bool const  thrusting =
        now - state::lastThrustMs.load() <= kFlightMemoryMs && now - state::lastFlyingMs.load() <= kFlyingFreshMs;

    bool const idle = !rolling && !thrusting && !lastSent.rolling && !lastSent.thrusting;
    bool const same = rolling == lastSent.rolling && std::abs(roll - lastSent.roll) < 0.05f
                   && thrusting == lastSent.thrusting && !thrusting;
    if (idle || same) return;

    net::RollSyncC2SPacket packet;
    packet.rolling   = rolling;
    packet.roll      = roll;
    packet.thrusting = thrusting;
    if (thrusting) {
        packet.velocity[0] = state::clientVelX.load();
        packet.velocity[1] = state::clientVelY.load();
        packet.velocity[2] = state::clientVelZ.load();
    }
    packet.sendToServer();
    lastSent = {rolling, roll, thrusting};
}

void applyConfig(Inbound const& in) {
    net::ConfigResponseC2SPacket response;
    response.protocolVersion = net::PROTOCOL_VERSION;
    response.success         = in.compatible && in.protocolVersion == net::PROTOCOL_VERSION;
    if (response.success) {
        state::config.serverForceEnabled = in.config.forceEnabled;
        state::config.serverBlockThrust  = !in.config.allowThrusting;
        state::lastServerPolicyMs.store(state::nowMs());
        std::lock_guard lock(mutex);
        connected = true;
        applied   = in.config;
    }
    response.sendToServer();
}

void drainInbox() {
    std::vector<Inbound> pending;
    {
        std::lock_guard lock(mutex);
        pending.swap(inbox);
    }
    auto const now = state::nowMs();
    for (auto const& in : pending) {
        if (in.isConfig) {
            applyConfig(in);
            continue;
        }
        std::lock_guard lock(mutex);
        if (!connected) continue;
        if (in.rolling && std::isfinite(in.roll)) rolls[in.actorId] = {in.roll, now};
        else rolls.erase(in.actorId);
    }
}

void pollHello(long long tick) {
    if (helloSent || tick > helloPollUntil) return;
    auto registry = ll::service::getCommandRegistry(true);
    if (registry && registry->findCommand(net::MARKER_COMMAND)) {
        helloSent = true;
        sendHello();
    }
}

LL_TYPE_INSTANCE_HOOK(NetTickHook, ll::memory::HookPriority::Highest, LocalPlayer, &LocalPlayer::$normalTick, void) {
    if (!enabled) return origin();

    {
        std::lock_guard lock(mutex);
        if (!helloSent && !connected && helloPollUntil == 0) helloPollUntil = state::tickCount + kHelloPollTicks;
    }
    pollHello(state::tickCount);
    drainInbox();

    int const dimension = static_cast<int>(getDimensionId());
    bool      online;
    {
        std::lock_guard lock(mutex);
        online = connected;
        if (lastDimension && *lastDimension != dimension) clearRemote();
        lastDimension = dimension;
    }
    origin();
    if (online) sendState();
}

class ConfigSyncHandler : public ll::network::PacketHandlerBase<ConfigSyncHandler, net::ConfigSyncS2CPacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback&, net::ConfigSyncS2CPacket const& packet) const {
        if (!enabled) return;
        Inbound in;
        in.isConfig        = true;
        in.config          = packet.config;
        in.protocolVersion = packet.protocolVersion;
        in.compatible      = packet.compatible;
        std::lock_guard lock(mutex);
        inbox.push_back(in);
    }
};

class RollSyncHandler : public ll::network::PacketHandlerBase<RollSyncHandler, net::RollSyncS2CPacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback&, net::RollSyncS2CPacket const& packet) const {
        if (!enabled) return;
        Inbound in;
        in.actorId = packet.actorId;
        in.rolling = packet.rolling;
        in.roll    = packet.roll;
        std::lock_guard lock(mutex);
        inbox.push_back(in);
    }
};

} // namespace

std::optional<float> otherPlayerRoll(std::int64_t actorId) {
    std::lock_guard lock(mutex);
    auto const      it = rolls.find(actorId);
    if (it == rolls.end()) return std::nullopt;
    if (state::nowMs() - it->second.updatedMs > kRollExpiryMs) {
        rolls.erase(it);
        return std::nullopt;
    }
    return it->second.roll;
}

bool serverModConnected() {
    std::lock_guard lock(mutex);
    return connected;
}

void installNet() {
    enabled = true;
    NetTickHook::hook();
    auto& bus = ll::event::EventBus::getInstance();
    commandListener =
        bus.emplaceListener<ll::event::ClientCommandRegisterEvent>([](ll::event::ClientCommandRegisterEvent& event) {
            if (!enabled) return;
            resetSession();
            if (event.commandRegistry().findCommand(net::MARKER_COMMAND)) {
                std::lock_guard lock(mutex);
                helloSent = true;
                sendHello();
            }
        });
    exitListener = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](ll::event::ClientExitLevelEvent&) {
        if (enabled) resetSession();
    });
}

void uninstallNet() {
    enabled = false;
    NetTickHook::unhook();
    auto& bus = ll::event::EventBus::getInstance();
    if (commandListener) bus.removeListener(commandListener);
    if (exitListener) bus.removeListener(exitListener);
    commandListener = nullptr;
    exitListener    = nullptr;
    resetSession();
}

} // namespace do_a_barrel_roll::hooks
