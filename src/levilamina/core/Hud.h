#pragma once

// Geometry of the horizon line and momentum crosshair widgets, in GUI-scaled pixels.

#include "common/Math.h"

#include <vector>

namespace do_a_barrel_roll::core {

struct IntLine {
    int x0, y0, x1, y1;
};

/// Two line segments for the artificial horizon.
[[nodiscard]] std::vector<IntLine> horizonLines(int scaledWidth, int scaledHeight, double rollDeg, double pitchDeg);

struct MomentumWidget {
    bool    hasLine = false;
    IntLine line{}; // valid if hasLine
    int     crosshairOffsetX = 0;
    int     crosshairOffsetY = 0;
};

[[nodiscard]] MomentumWidget momentumWidget(int scaledWidth, int scaledHeight, Vec2 mouseTurnVec);

} // namespace do_a_barrel_roll::core
