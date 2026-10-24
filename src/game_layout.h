#pragma once

#include <algorithm>
#include <cmath>

namespace threadlight::layout {

constexpr int kInitialScreenWidth = 1280;
constexpr int kInitialScreenHeight = 800;
constexpr float kPlayerRadius = 12.0f;
constexpr float kPlayerVisualRadius = kPlayerRadius + 4.0f;
constexpr float kHudBottom = 76.0f;
constexpr float kFooterHeight = 52.0f;
constexpr float kCameraShakeX = 6.0f;
constexpr float kCameraShakeY = 4.0f;

// World and UI values use the startup mouse coordinate space. Only the
// WindowCoordinates boundary converts native framebuffer dimensions into it.
struct PlayBounds {
    float left;
    float right;
    float top;
    float bottom;
    float playerMinX;
    float playerMaxX;
    float playerMinY;
    float playerMaxY;
};

struct Point {
    float x;
    float y;
};

struct HudLayout {
    float healthX;
    float healthWidth;
    float waveX;
    float sparksX;
    float scoreX;
    float footerY;
    float hintX;
    float settingsHintX;
    float pauseHintX;
};

// On Windows raylib 5.5 sets an inverse mouse scale at InitWindow(), but
// neither its resize nor content-scale callback updates it. Keep that startup
// coordinate space for the lifetime of the window. Do not infer it again from
// GetScreenWidth() after a callback, or from the current monitor's DPI.
struct WindowCoordinates {
    Point scale;

    static WindowCoordinates atStartup(int screenWidth, int screenHeight,
                                       int renderWidth, int renderHeight) {
        return {{static_cast<float>(std::max(1, renderWidth)) / std::max(1, screenWidth),
                 static_cast<float>(std::max(1, renderHeight)) / std::max(1, screenHeight)}};
    }
    int width(int renderWidth) const {
        return std::max(1, static_cast<int>(std::lround(renderWidth / scale.x)));
    }
    int height(int renderHeight) const {
        return std::max(1, static_cast<int>(std::lround(renderHeight / scale.y)));
    }
    int nativeWidth(int logicalWidth) const {
        return std::max(1, static_cast<int>(std::ceil(logicalWidth * scale.x)));
    }
    int nativeHeight(int logicalHeight) const {
        return std::max(1, static_cast<int>(std::ceil(logicalHeight * scale.y)));
    }
    Point toFramebuffer(Point point, Point shift = {}) const {
        return {(point.x + shift.x) * scale.x, (point.y + shift.y) * scale.y};
    }
};

// raylib's Camera2D translates world positions by this logical-pixel offset.
// Keep the transform explicit so layout tests can verify every shake extreme.
inline Point worldToScreen(Point world, Point cameraShift) {
    return {world.x + cameraShift.x, world.y + cameraShift.y};
}

inline Point remapPoint(Point point, const PlayBounds& from, const PlayBounds& to) {
    const float fromWidth = std::max(1.0f, from.right - from.left);
    const float fromHeight = std::max(1.0f, from.bottom - from.top);
    const float toWidth = std::max(0.0f, to.right - to.left);
    const float toHeight = std::max(0.0f, to.bottom - to.top);
    const float xFraction = std::clamp((point.x - from.left) / fromWidth, 0.0f, 1.0f);
    const float yFraction = std::clamp((point.y - from.top) / fromHeight, 0.0f, 1.0f);
    return {to.left + xFraction * toWidth, to.top + yFraction * toHeight};
}

inline HudLayout hudLayout(int logicalWidth, int logicalHeight) {
    const float width = static_cast<float>(std::max(1, logicalWidth));
    const float height = static_cast<float>(std::max(1, logicalHeight));
    const float healthX = width < 1120.0f ? 210.0f : width * 0.31f;
    const float healthWidth = std::min(190.0f, width * 0.145f);
    return {healthX, healthWidth, healthX + healthWidth + 38.0f,
            width * 0.66f, width * 0.82f, height - kFooterHeight,
            width * 0.40f, width * 0.68f, width - 105.0f};
}

inline PlayBounds playBounds(int logicalWidth, int logicalHeight,
                             float playerRadius = kPlayerVisualRadius,
                             float cameraShakeX = kCameraShakeX,
                             float cameraShakeY = kCameraShakeY) {
    const float width = static_cast<float>(std::max(1, logicalWidth));
    const float height = static_cast<float>(std::max(1, logicalHeight));
    const float top = std::min(kHudBottom, height);
    const float bottom = std::max(top, height - kFooterHeight);
    return {0.0f, width, top, bottom,
            std::min(width * 0.5f, playerRadius + std::max(0.0f, cameraShakeX)),
            std::max(width * 0.5f, width - playerRadius - std::max(0.0f, cameraShakeX)),
            std::min(height * 0.5f, top + playerRadius + std::max(0.0f, cameraShakeY)),
            std::max(height * 0.5f, bottom - playerRadius - std::max(0.0f, cameraShakeY))};
}

inline bool playerVisible(Point player, const PlayBounds& bounds,
                          float radius = kPlayerVisualRadius,
                          Point cameraShift = {}) {
    const Point screen = worldToScreen(player, cameraShift);
    return screen.x - radius >= bounds.left &&
           screen.x + radius <= bounds.right &&
           screen.y - radius >= bounds.top &&
           screen.y + radius <= bounds.bottom;
}

inline float clampPlayerX(float x, const PlayBounds& bounds) {
    return std::clamp(x, bounds.playerMinX, bounds.playerMaxX);
}

inline float clampPlayerY(float y, const PlayBounds& bounds) {
    return std::clamp(y, bounds.playerMinY, bounds.playerMaxY);
}

} // namespace threadlight::layout
