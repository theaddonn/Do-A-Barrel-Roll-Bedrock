// Replaces the glide transform of the local player in third person: the sideways tilt (y) becomes the roll and, while
// the mod owns the camera, the forward tilt (x) is derived from the mod's pitch instead of the clamped game pitch.
// applyGlobalAnims rotates about X by x, then about Y by y (skipped when y == 0).

#include "levilamina/hooks/Net.h"
#include "levilamina/hooks/State.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/actor/ActorRenderData.h"
#include "mc/world/actor/provider/MobGliding.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace do_a_barrel_roll::hooks {

namespace {

using GlideRotation = std::optional<std::pair<float, float>>;

LL_STATIC_HOOK(
    GlidingRotationHook,
    ll::memory::HookPriority::Normal,
    &ActorRenderData::getDamageOrGlidingXYRotation,
    GlideRotation,
    Mob const& mob,
    float      alpha,
    bool       isFirstPersonPlayer
) {
    auto result = origin(mob, alpha, isFirstPersonPlayer);
    if (!result || isFirstPersonPlayer) return result;
    auto client = ll::service::getClientInstance();
    if (!client || static_cast<Actor const*>(client->getLocalPlayer()) != static_cast<Actor const*>(&mob)) {
        if (auto const roll =
                otherPlayerRoll(static_cast<std::int64_t>(const_cast<Mob&>(mob).getOrCreateUniqueID().rawID))) {
            result->second = *roll;
        }
        return result;
    }

    if (state::cameraOverride) {
        float t       = static_cast<float>(MobGliding::getFallFlyTicks(mob.getEntityContext())) + alpha;
        float ramp    = std::clamp(t * t / 100.0f, 0.0f, 1.0f);
        result->first = (-90.0f - static_cast<float>(state::cameraPitch)) * ramp;
    }
    float roll = state::cameraRollDeg;
    if (roll != 0.0f && std::isfinite(roll)) result->second = roll;
    return result;
}

} // namespace

void installPlayerModel() { GlidingRotationHook::hook(); }

void uninstallPlayerModel() { GlidingRotationHook::unhook(); }

} // namespace do_a_barrel_roll::hooks
