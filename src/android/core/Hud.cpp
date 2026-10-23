#include "core/Hud.h"

#include <algorithm>

namespace do_a_barrel_roll::core {

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

}
