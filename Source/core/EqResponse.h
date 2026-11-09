// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "StudioSettings.h"
namespace fga {
struct EqCoefficients {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    void setCoeffs(double b0_, double b1_, double b2_, double a0_, double a1_, double a2_) {
        b0 = b0_ / a0_; b1 = b1_ / a0_; b2 = b2_ / a0_;
        a1 = a1_ / a0_; a2 = a2_ / a0_;
    }

    double magnitudeDb(double f, double rate) const {
        const double phi = 6.283185307179586 * std::min(f, rate * 0.499) / rate;
        const double c1 = std::cos(phi), s1 = std::sin(phi);
        const double c2 = std::cos(2.0 * phi), s2 = std::sin(2.0 * phi);
        const double reNum = b0 + b1 * c1 + b2 * c2;
        const double imNum = -b1 * s1 - b2 * s2;
        const double reDen = 1.0 + a1 * c1 + a2 * c2;
        const double imDen = -a1 * s1 - a2 * s2;
        const double numSq = reNum * reNum + imNum * imNum;
        const double denSq = reDen * reDen + imDen * imDen;
        if (denSq < 1e-12) return 0.0;
        return 10.0 * std::log10(std::max(1e-12, numSq / denSq));
    }
};

inline EqCoefficients eqCoefficients(const EqBand& b, double rate) {
    EqCoefficients f;
    if (!b.enabled) return f;
    const double hz = std::clamp(b.freq, 20.0, rate * 0.45);
    const double qVal = std::clamp(b.q, 0.1, 18.0);
    const double w = 6.283185307179586 * hz / rate;
    const double c = std::cos(w), s = std::sin(w), alpha = s / (2.0 * qVal);

    switch (b.type) {
        case (int)EqFilterType::LowCut:
            f.setCoeffs((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::HighCut:
            f.setCoeffs((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::Notch:
            f.setCoeffs(1.0, -2.0 * c, 1.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
            break;
        case (int)EqFilterType::LowShelf: {
            const double A = std::pow(10.0, b.gain / 40.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            f.setCoeffs(A * ((A + 1.0) - (A - 1.0) * c + beta),
                        2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                        A * ((A + 1.0) - (A - 1.0) * c - beta),
                        (A + 1.0) + (A - 1.0) * c + beta,
                        -2.0 * ((A - 1.0) + (A + 1.0) * c),
                        (A + 1.0) + (A - 1.0) * c - beta);
            break;
        }
        case (int)EqFilterType::HighShelf: {
            const double A = std::pow(10.0, b.gain / 40.0);
            const double beta = 2.0 * std::sqrt(A) * alpha;
            f.setCoeffs(A * ((A + 1.0) + (A - 1.0) * c + beta),
                        -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                        A * ((A + 1.0) + (A - 1.0) * c - beta),
                        (A + 1.0) - (A - 1.0) * c + beta,
                        2.0 * ((A - 1.0) - (A + 1.0) * c),
                        (A + 1.0) - (A - 1.0) * c - beta);
            break;
        }
        case (int)EqFilterType::Bell:
        default: {
            const double A = std::pow(10.0, b.gain / 40.0);
            f.setCoeffs(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
            break;
        }
    }
    return f;
}

}
