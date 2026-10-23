#pragma once

#include "common/Math.h"

namespace do_a_barrel_roll::core {

struct IntLine {
    int x0, y0, x1, y1;
};

struct MomentumWidget {
    bool    hasLine = false;
    IntLine line{};
    int     crosshairOffsetX = 0;
    int     crosshairOffsetY = 0;
};

[[nodiscard]] MomentumWidget momentumWidget(int scaledWidth, int scaledHeight, Vec2 mouseTurnVec);

}
