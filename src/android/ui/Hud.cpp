#include "Hud.hpp"
#include "core/Hud.h"
#include "game/Hooks.hpp"
#include "game/State.hpp"
#include "runtime/Runtime.hpp"
#include <cmath>

namespace dabr {
namespace {
UiRenderFn renderNext{};
CursorRenderFn cursorNext{};
std::int64_t lastCursor{-1000000};
bool momentum() {
    const auto& state = flight();
    return Runtime::get().active() && Runtime::get().hudTop() && state.hudVisible && state.config.showMomentumWidget && state.config.momentumBasedMouse;
}
void pixel(void* context, float x, float y, bool halo) {
    const auto fill = method<void (*)(void*, const Rect&, const Color&, float)>(context, build::slot::UiFillRectangle);
    if (!fill) return;
    const Rect rect = halo ? Rect{x - 1, x + 2, y - 1, y + 2} : Rect{x, x + 1, y, y + 1};
    fill(context, rect, halo ? Color{0, 0, 0, 1} : Color{}, halo ? 0.5f : 1.0f);
}
void renderHook(void* screen, void* context) {
    renderNext(screen, context);
    auto& runtime = Runtime::get();
    const auto& state = flight();
    if (!runtime.active() || !state.hudVisible || !runtime.hudTop() || !context ||
        read<void**>(context) != runtime.native().uiVtable) return;
    if (!momentum() || nowMs() - lastCursor >= 250) return;
    const auto getClip = method<Rect (*)(const void*)>(context, build::slot::UiFullClip);
    if (!getClip) return;
    const auto clip = getClip(context);
    const float width = clip.right - clip.left, height = clip.bottom - clip.top;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 16 || height < 16 || width > 16384 || height > 16384) return;
    const auto w = static_cast<int>(width), h = static_cast<int>(height);
    auto widget = core::momentumWidget(w, h, state.controller.mouseTurnVec());
    if (!widget.hasLine) return;
    ++widget.line.y0;
    ++widget.line.y1;
    const auto& line = widget.line;
    for (const bool halo : {true, false})
        core::forBresenhamLine(line.x0, line.y0, line.x1, line.y1, [&](int x, int y) { pixel(context, static_cast<float>(x) + clip.left, static_cast<float>(y) + clip.top, halo); });
}
void cursorHook(void* self, void* context, void* client, void* owner, int pass) {
    if (!momentum() || !owner) { cursorNext(self, context, client, owner, pass); return; }
    const auto original = read<Vec2>(owner, build::field::ControlSize);
    const auto widget = core::momentumWidget(0, 0, flight().controller.mouseTurnVec());
    const Vec2 shifted{original.x - 2.0f * widget.crosshairOffsetX, original.y - 2.0f * widget.crosshairOffsetY};
    write(owner, build::field::ControlSize, shifted);
    cursorNext(self, context, client, owner, pass);
    write(owner, build::field::ControlSize, original);
    lastCursor = nowMs();
}
}
void clearHud() { lastCursor = -1000000; }
bool installHudHooks(Host& host, const Targets& target) {
    return host.install(reinterpret_cast<void*>(target.uiRender), reinterpret_cast<void*>(renderHook), reinterpret_cast<void**>(&renderNext)) &&
        host.install(reinterpret_cast<void*>(target.cursor), reinterpret_cast<void*>(cursorHook), reinterpret_cast<void**>(&cursorNext));
}
}
