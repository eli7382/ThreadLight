#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace threadlight::chapter_hud {

struct Rect {
    float x;
    float y;
    float width;
    float height;
};

struct Layout {
    Rect bossBar;
    Rect dashTrack;
};

inline Layout layout(int width, int height) {
    const float logicalWidth = static_cast<float>(width);
    const float logicalHeight = static_cast<float>(height);
    const float bossWidth = std::min(500.0f, logicalWidth - 400.0f);

    return {
        {(logicalWidth - bossWidth) * 0.5f, 68.0f, bossWidth, 4.0f},
        {222.0f, logicalHeight - 52.0f + 25.0f, 80.0f, 2.0f},
    };
}

enum class DashState {
    Active,
    Recharging,
    Ready,
    ReadyMove,
};

struct DashView {
    float fill;
    int remainingTenths;
    DashState state;
};

inline DashView dashView(float remaining, float capturedCooldown, bool active, bool moving) {
    // Treat non-finite clock inputs as expired. A positive remaining value is
    // still authoritative for readiness, even when the captured duration is
    // invalid and cannot be used as a progress denominator.
    if (!std::isfinite(remaining)) {
        remaining = 0.0f;
    }

    const bool recharging = remaining > 0.0f;
    int remainingTenths = 0;
    if (recharging) {
        const double roundedTenths = std::ceil(static_cast<double>(remaining) * 10.0 - 1e-6);
        remainingTenths = static_cast<int>(std::clamp(
            roundedTenths, 1.0, static_cast<double>(std::numeric_limits<int>::max())));
    }

    float fill = 1.0f;
    if (std::isfinite(capturedCooldown) && capturedCooldown > 0.0f) {
        fill = std::clamp(1.0f - remaining / capturedCooldown, 0.0f, 1.0f);
    } else if (recharging) {
        fill = 0.0f;
    }

    DashState state = DashState::Ready;
    if (active) {
        state = DashState::Active;
    } else if (recharging) {
        state = DashState::Recharging;
    } else if (!moving) {
        state = DashState::ReadyMove;
    }

    return {fill, remainingTenths, state};
}

}  // namespace threadlight::chapter_hud
