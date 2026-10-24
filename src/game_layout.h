#pragma once

#include <algorithm>

namespace threadlight::layout {

constexpr int kInitialScreenWidth = 1280;
constexpr int kInitialScreenHeight = 800;
constexpr float kPlayerRadius = 12.0f;
constexpr float kPlayerVisualRadius = kPlayerRadius + 4.0f;
constexpr float kHudBottom = 76.0f;
constexpr float kFooterHeight = 52.0f;
constexpr float kCameraShakeX = 6.0f;
constexpr float kCameraShakeY = 4.0f;

// All values use raylib's logical screen coordinates. High-DPI framebuffer
// dimensions must not enter world or input calculations.
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

struct CameraTransform {
    Point offset;
    Point target;
    float zoom;
    float scaleX;
    float scaleY;
};

// raylib's Camera2D translates world positions by this logical-pixel offset.
// Keep the transform explicit so layout tests can verify every shake extreme.
inline Point worldToScreen(Point world, Point cameraShift) {
    return {world.x + cameraShift.x, world.y + cameraShift.y};
}

inline CameraTransform cameraTransform(int logicalWidth, int logicalHeight,
                                       int framebufferWidth, int framebufferHeight,
                                       float cameraShiftX = 0.0f,
                                       float cameraShiftY = 0.0f) {
    const float width = static_cast<float>(std::max(1, logicalWidth));
    const float height = static_cast<float>(std::max(1, logicalHeight));
    const float renderWidth = static_cast<float>(std::max(1, framebufferWidth));
    const float renderHeight = static_cast<float>(std::max(1, framebufferHeight));
    const float scaleX = renderWidth / width;
    const float scaleY = renderHeight / height;
    // Camera2D resets raylib's normal 2D DPI matrix while active. Its zoom and
    // framebuffer-space offset must therefore restore the logical-to-pixel
    // scale for world rendering inside BeginMode2D().
    return {{renderWidth * 0.5f + cameraShiftX * scaleX,
             renderHeight * 0.5f + cameraShiftY * scaleX},
            {width * 0.5f, height * 0.5f}, scaleX, scaleX, scaleY};
}

inline Point cameraWorldToFramebuffer(Point world, const CameraTransform& camera) {
    return {camera.offset.x + (world.x - camera.target.x) * camera.zoom,
            camera.offset.y + (world.y - camera.target.y) * camera.zoom};
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
