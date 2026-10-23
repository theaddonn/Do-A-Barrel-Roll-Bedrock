// Artificial horizon and momentum line, drawn after the UI as 1-GUI-pixel rectangles (there is no line primitive).
// The momentum crosshair is the vanilla one: HudCursorRenderer centres its sprite at (guiSize - owner.mSize) / 2, so
// shrinking owner.mSize around the original call moves it.

#include "levilamina/hooks/State.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/renderers/HudCursorRenderer.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/input/RectangleArea.h"
#include "levilamina/core/Hud.h"

#include <cstdint>
#include <vector>

namespace do_a_barrel_roll::hooks {

namespace {

constexpr std::int64_t kCursorMaxAgeMs = 250;

ll::event::ListenerPtr listener;
std::int64_t           lastCursorMs = -1000000;

bool momentumActive() {
    auto& cfg = state::config;
    return state::hudVisible && cfg.showMomentumWidget && cfg.momentumBasedMouse;
}

void rect(MinecraftUIRenderContext& ctx, float x0, float y0, float x1, float y1, mce::Color const& color, float alpha) {
    RectangleArea r{};
    r._x0 = x0;
    r._x1 = x1;
    r._y0 = y0;
    r._y1 = y1;
    ctx.fillRectangle(r, color, alpha);
}

// Each dot gets a dark halo (first pass) under the white dot, as there is no inverting blend to stay visible.
void line(MinecraftUIRenderContext& ctx, core::IntLine const& l, bool halo) {
    static mce::Color white{1.0f, 1.0f, 1.0f, 1.0f};
    static mce::Color black{0.0f, 0.0f, 0.0f, 1.0f};
    core::forBresenhamLine(l.x0, l.y0, l.x1, l.y1, [&](int x, int y) {
        auto fx = static_cast<float>(x);
        auto fy = static_cast<float>(y);
        if (halo) rect(ctx, fx - 1, fy - 1, fx + 2, fy + 2, black, 0.5f);
        else rect(ctx, fx, fy, fx + 1, fy + 1, white, 1.0f);
    });
}

void onRender(ll::event::AfterUIRenderEvent& e) {
    if (!state::hudVisible) return;
    auto& cfg = state::config;
    if (!cfg.showHorizon && !(cfg.showMomentumWidget && cfg.momentumBasedMouse)) return;
    if (e.screenView().getScreenName() != "hud_screen") return;
    auto client = ll::service::getClientInstance();
    if (!client) return;
    auto* player = client->getLocalPlayer();
    if (!player) return;

    auto& ctx = e.uiRenderContext();

    auto  clip = ctx.getFullClippingRectangle();
    float cw = clip._x1 - clip._x0, ch = clip._y1 - clip._y0;
    int   w, h;
    if (cw >= 16.0f && ch >= 16.0f) {
        w = static_cast<int>(cw);
        h = static_cast<int>(ch);
    } else {
        Vec2 guiSize = client->getGuiData()->mScreenSizeData->clientUIScreenSize;
        w            = static_cast<int>(guiSize.x);
        h            = static_cast<int>(guiSize.y);
    }
    if (w <= 0 || h <= 0) return;

    std::vector<core::IntLine> lines;

    if (cfg.showHorizon) {
        auto ps = state::snapshot(*player);
        for (auto const& l : core::horizonLines(w, h, state::controller.getRoll(), ps.pitch)) lines.push_back(l);
    }
    if (momentumActive()) {
        // Only while the crosshair renderer runs (it is hidden e.g. in third person). Java's line starts at h/2 - 1
        // for its 15px crosshair; shift by one for the 16px Bedrock sprite.
        auto m = core::momentumWidget(w, h, state::controller.mouseTurnVec());
        if (m.hasLine && state::nowMs() - lastCursorMs < kCursorMaxAgeMs) {
            m.line.y0 += 1;
            m.line.y1 += 1;
            lines.push_back(m.line);
        }
    }

    for (auto const& l : lines) line(ctx, l, true);
    for (auto const& l : lines) line(ctx, l, false);
}

LL_TYPE_INSTANCE_HOOK(
    HudCursorRenderHook,
    ll::memory::HookPriority::Normal,
    HudCursorRenderer,
    &HudCursorRenderer::$render,
    void,
    ::MinecraftUIRenderContext& renderContext,
    ::IClientInstance&          client,
    ::UIControl&                owner,
    int                         pass
) {
    if (!momentumActive()) return origin(renderContext, client, owner, pass);

    auto  m    = core::momentumWidget(0, 0, state::controller.mouseTurnVec());
    auto& size = owner.mSize.get();
    float sx = size.x, sy = size.y;
    size.x = sx - 2.0f * static_cast<float>(m.crosshairOffsetX);
    size.y = sy - 2.0f * static_cast<float>(m.crosshairOffsetY);
    origin(renderContext, client, owner, pass);
    size.x = sx;
    size.y = sy;

    lastCursorMs = state::nowMs();
}

} // namespace

void installHud() {
    HudCursorRenderHook::hook();
    listener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(onRender);
}

void uninstallHud() {
    HudCursorRenderHook::unhook();
    if (listener) ll::event::EventBus::getInstance().removeListener(listener);
    listener.reset();
}

} // namespace do_a_barrel_roll::hooks
