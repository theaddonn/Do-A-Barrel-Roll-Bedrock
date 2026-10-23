#include "levilamina/core/Hud.h"

#include <algorithm>

namespace do_a_barrel_roll::core {

std::vector<IntLine> horizonLines(int scaledWidth, int scaledHeight, double rollDeg, double pitchDeg) {
    int    centerX = scaledWidth / 2 - 1;
    int    centerY = scaledHeight / 2 - 1;
    double roll    = rollDeg * -kToRad;

    Vec2 v{std::cos(roll), std::sin(roll)};
    Vec2 offset = Vec2{v.y, -v.x} * (pitchDeg * scaledHeight * 0.007);
    centerX += static_cast<int>(std::llround(offset.x));
    centerY += static_cast<int>(std::llround(offset.y));

    std::vector<IntLine> lines;
    for (int i = 0; i < 2; i++) {
        v = -v;
        Vec2 s = v * 10.0, e = v * 50.0;
        lines.push_back({centerX + (int)s.x, centerY + (int)s.y, centerX + (int)e.x, centerY + (int)e.y});
    }
    return lines;
}

MomentumWidget momentumWidget(int scaledWidth, int scaledHeight, Vec2 mouseTurnVec) {
    int  centerX = scaledWidth / 2;
    int  centerY = scaledHeight / 2 - 1;
    Vec2 m       = mouseTurnVec * 50.0;
    Vec2 neg     = -m;
    neg.normalize();
    Vec2 lineVec = m + neg * std::min(m.length(), 10.0);

    MomentumWidget w;
    if ((lineVec.x != 0.0 || lineVec.y != 0.0) && m.lengthSquared() > 10.0 * 10.0) {
        w.hasLine = true;
        w.line    = {centerX, centerY, centerX + (int)lineVec.x, centerY + (int)lineVec.y};
    }
    w.crosshairOffsetX = (int)m.x;
    w.crosshairOffsetY = (int)m.y;
    return w;
}

} // namespace do_a_barrel_roll::core
