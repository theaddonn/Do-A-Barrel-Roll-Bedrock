#pragma once

#include "common/Math.h"

namespace do_a_barrel_roll::core {

class Smoother {
public:
    double smooth(double original, double smoother) {
        mActualSum += original;
        double d = mActualSum - mSmoothedSum;
        double e = lerp(0.5, mMovementLatency, d);
        double f = sign(d);
        if (f * d > f * mMovementLatency) d = e;
        mMovementLatency = e;
        mSmoothedSum += d * smoother;
        return d * smoother;
    }

    void clear() { mActualSum = mSmoothedSum = mMovementLatency = 0.0; }

private:
    double mActualSum       = 0.0;
    double mSmoothedSum     = 0.0;
    double mMovementLatency = 0.0;
};

}
