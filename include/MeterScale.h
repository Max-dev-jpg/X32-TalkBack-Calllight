#pragma once
// =============================================================================
// MeterScale.h  –  Convert raw X32/M32 signal values to dB (done in firmware so
// the trigger filters and the whole UI work in dB).
//
//   • Meter level (pre/post): measured level curve (linear 0..1 → dB), floored
//                             at -60 dB, capped at 0 dB.            [Level.csv]
//   • Meter gate / comp:      gain reduction in dB, INVERTED (positive = more
//                             reduction).  Measured curves.  [Gate.csv, Comp.csv]
//   • Fader position:         dB along the X32 fader taper (same curve as the UI)
//   • Mute:                   passed through as 0/1
//
// The gate/comp meters both follow -20·log10(v) very closely, but we use the
// measured tables verbatim so the extremes match the console exactly.
//
// All conversions are branch-only table lookups, a handful of calls per 50 ms
// meter frame — negligible cost.
// =============================================================================

#include <Arduino.h>
#include <math.h>
#include "config.h"

namespace MeterScale {

// dB used to represent silence for each domain (matches each curve's floor).
constexpr float DB_FLOOR          = -90.0f;  // fader-down
constexpr float METER_LEVEL_FLOOR = -60.0f;  // meter level (bottom of the curve)

// A calibration point: raw linear meter value `f` (0..1) ↔ decibels `db`.
struct MPoint { float f; float db; };

// Piecewise-linear interpolation over a table sorted by ASCENDING `f`.
// Below the first point returns `dbBelow`, above the last returns `dbAbove`.
inline float interpDb(const MPoint* P, int N, float v, float dbBelow, float dbAbove) {
    if (v <= P[0].f)      return dbBelow;
    if (v >= P[N - 1].f)  return dbAbove;
    for (int i = 0; i < N - 1; ++i) {
        if (v <= P[i + 1].f) {
            float r = (v - P[i].f) / (P[i + 1].f - P[i].f);
            return P[i].db + r * (P[i + 1].db - P[i].db);
        }
    }
    return dbAbove;
}

// ── Meter level (pre / post fade) ─────────────────────────────────────────────
// Measured level meter curve (Level.csv): linear meter value 0..1 → dB, floored
// at -60 dB, capped at 0 dB.
inline float levelToDb(float v) {
    static const MPoint P[] = {
        {0.0000f, -60}, {0.0018f, -55}, {0.0032f, -50}, {0.0056f, -45},
        {0.0100f, -40}, {0.0177f, -35}, {0.0315f, -30}, {0.0397f, -28},
        {0.0499f, -26}, {0.0629f, -24}, {0.0792f, -22}, {0.0996f, -20},
        {0.1255f, -18}, {0.1580f, -16}, {0.1988f, -14}, {0.2232f, -13},
        {0.2504f, -12}, {0.2808f, -11}, {0.3152f, -10}, {0.3537f,  -9},
        {0.3967f,  -8}, {0.4453f,  -7}, {0.4994f,  -6}, {0.5603f,  -5},
        {0.6290f,  -4}, {0.7057f,  -3}, {0.7915f,  -2}, {0.8884f,  -1},
        {0.9964f,   0}
    };
    return interpDb(P, (int)(sizeof(P) / sizeof(P[0])), v, METER_LEVEL_FLOOR, 0.0f);
}

// ── Gate gain reduction (INVERTED: positive dB = more reduction) ──────────────
// Measured gate meter curve (Gate.csv). Full-open (v≈1) = 0 dB reduction;
// smaller v = deeper reduction. Capped at 60 dB.
inline float gateToDb(float v) {
    static const MPoint P[] = {
        {0.0010f, 60}, {0.0032f, 50}, {0.0100f, 40}, {0.0316f, 30},
        {0.1000f, 20}, {0.3162f, 10}, {0.5012f,  6}, {0.7079f,  3},
        {1.0000f,  0}
    };
    return interpDb(P, (int)(sizeof(P) / sizeof(P[0])), v, 60.0f, 0.0f);
}

// ── Comp gain reduction (INVERTED: positive dB = more reduction) ──────────────
// Measured comp meter curve (Comp.csv). Capped at 30 dB.
inline float compToDb(float v) {
    static const MPoint P[] = {
        {0.0311f, 30}, {0.1246f, 18}, {0.1764f, 15}, {0.2497f, 12},
        {0.3526f,  9}, {0.4988f,  6}, {0.7055f,  3}, {0.9998f,  0}
    };
    return interpDb(P, (int)(sizeof(P) / sizeof(P[0])), v, 30.0f, 0.0f);
}

// ── Fader position → dB (X32 fader taper; mirrors the UI's normalizedToDb) ─────
inline float faderToDb(float v) {
    if (v <= 0.0f) return DB_FLOOR;
    if (v >= 1.0f) return 10.0f;
    if (v < 0.0625f) return (v / 0.0625f)            * 30.0f - 90.0f;
    if (v < 0.25f)   return ((v - 0.0625f) / 0.1875f) * 30.0f - 60.0f;
    if (v < 0.5f)    return ((v - 0.25f)   / 0.25f)   * 20.0f - 30.0f;
    return                  ((v - 0.5f)    / 0.5f)    * 20.0f - 10.0f;
}

// ── Dispatch by signal source + meter tap (meterSignalType 0=pre,1=gate,2=comp,3=post)
inline float toDb(uint8_t signalSource, uint8_t meterTap, float raw) {
    switch (signalSource) {
        case SIG_FADER: return faderToDb(raw);
        case SIG_METER:
            if (meterTap == 1) return gateToDb(raw);   // gate gain reduction
            if (meterTap == 2) return compToDb(raw);   // comp gain reduction
            return levelToDb(raw);                     // pre / post level
        default:        return raw;              // SIG_MUTE: 0/1 as-is
    }
}

// ── Signal domain bounds (in the converted dB / 0-1 units) ────────────────────
// Minimum ("quiet / no reduction / muted") and maximum ("loud / full reduction /
// unmuted") of each source, used to clamp the hysteresis release point and to
// pick a polarity-aware at-rest value.
inline float domainMin(uint8_t signalSource, uint8_t meterTap) {
    if (signalSource == SIG_MUTE)  return 0.0f;                        // muted
    if (signalSource == SIG_METER)
        return (meterTap == 1 || meterTap == 2) ? 0.0f                 // GR: no reduction
                                                : METER_LEVEL_FLOOR;   // level: -60 dB
    return DB_FLOOR;                                                   // fader: -90 dB
}
inline float domainMax(uint8_t signalSource, uint8_t meterTap) {
    if (signalSource == SIG_MUTE)  return 1.0f;                        // unmuted
    if (signalSource == SIG_METER) {
        if (meterTap == 1) return 60.0f;                              // gate GR (max)
        if (meterTap == 2) return 30.0f;                              // comp GR (max)
        return 0.0f;                                                   // level: 0 dB cap
    }
    return 10.0f;                                                      // fader: +10 dB
}

// ── "At-rest" value (no signal) used to init/reset the EMA so a fresh trigger
//    reads as INACTIVE for its polarity: a normal trigger fires above the
//    threshold, so it rests at the domain minimum; an inverted trigger fires
//    below the threshold, so it rests at the domain maximum. Without this an
//    inverted trigger would start active after a reboot.
inline float restLevel(uint8_t signalSource, uint8_t meterTap, bool invert) {
    return invert ? domainMax(signalSource, meterTap)
                  : domainMin(signalSource, meterTap);
}

} // namespace MeterScale
