#include "visual_effects.h"
#include "game_layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
int failures = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) return;
    std::cerr << "FAIL line " << line << ": " << expression << '\n';
    ++failures;
}

#define CHECK(expression) check((expression), #expression, __LINE__)

void testSilkCoolsContinuouslyAndFadesWithAge() {
    using namespace threadlight::visual;
    const auto active = silkAppearance(0.7f, 0.5f);
    const auto justHarmless = silkAppearance(kActiveSilkSeconds + 0.01f, 0.5f);
    const auto transition = silkAppearance(kActiveSilkSeconds + 0.35f, 0.5f);
    const auto old = silkAppearance(3.0f, 0.5f);
    const auto expired = silkAppearance(kSilkLifetimeSeconds, 0.5f);
    CHECK(active.energy > transition.energy);
    CHECK(active.coreOpacity > transition.coreOpacity);
    CHECK(transition.energy > old.energy);
    CHECK(old.energy > expired.energy);
    CHECK(expired.fade == 0.0f);
    CHECK(active.ageBlend == 0.0f);
    CHECK(justHarmless.ageBlend < 0.01f);
    CHECK(std::abs(active.energy - justHarmless.energy) < 0.03f);
    CHECK(std::abs(active.coreOpacity - justHarmless.coreOpacity) < 0.03f);
    CHECK(active.energy <= 1.0f);
}

void testPlayerBoundsFollowLogicalClientAndCameraShake() {
    using namespace threadlight::layout;
    struct Viewport { int width; int height; };
    constexpr Viewport sizes[] = {
        {1280, 800}, {1600, 900}, {1920, 1080}, {960, 640}, {1280, 1024}
    };
    for (const auto size : sizes) {
        const auto bounds = playBounds(size.width, size.height);
        const auto hud = hudLayout(size.width, size.height);
        CHECK(bounds.left == 0.0f);
        CHECK(bounds.right == static_cast<float>(size.width));
        CHECK(bounds.top == kHudBottom);
        CHECK(bounds.bottom == static_cast<float>(size.height) - kFooterHeight);
        CHECK(hud.footerY + kFooterHeight == static_cast<float>(size.height));
        CHECK(hud.healthX + hud.healthWidth < hud.waveX);
        CHECK(hud.waveX + 145.0f < hud.sparksX);
        CHECK(hud.sparksX + 105.0f < hud.scoreX);
        CHECK(hud.scoreX + 150.0f < static_cast<float>(size.width));
        CHECK(hud.hintX + 140.0f < hud.settingsHintX);
        CHECK(hud.settingsHintX + 120.0f < hud.pauseHintX);
        CHECK(clampPlayerX(-1000.0f, bounds) == bounds.playerMinX);
        CHECK(clampPlayerX(10000.0f, bounds) == bounds.playerMaxX);
        CHECK(clampPlayerY(-1000.0f, bounds) == bounds.playerMinY);
        CHECK(clampPlayerY(10000.0f, bounds) == bounds.playerMaxY);

        // Exercise all combinations of maximum camera shake while the player
        // rests on each extreme of the reachable clamp rectangle.
        for (const float xShake : {-kCameraShakeX, kCameraShakeX}) {
            for (const float yShake : {-kCameraShakeY, kCameraShakeY}) {
                CHECK(playerVisible({bounds.playerMinX, bounds.playerMinY}, bounds,
                                    kPlayerVisualRadius, {xShake, yShake}));
                CHECK(playerVisible({bounds.playerMaxX, bounds.playerMaxY}, bounds,
                                    kPlayerVisualRadius, {xShake, yShake}));
                CHECK(playerVisible({bounds.playerMinX, bounds.playerMaxY}, bounds,
                                    kPlayerVisualRadius, {xShake, yShake}));
                CHECK(playerVisible({bounds.playerMaxX, bounds.playerMinY}, bounds,
                                    kPlayerVisualRadius, {xShake, yShake}));
            }
        }
    }

    const auto standard = playBounds(1280, 800);
    CHECK(standard.playerMinX - kCameraShakeX - kPlayerVisualRadius == standard.left);
    CHECK(standard.playerMaxX + kCameraShakeX + kPlayerVisualRadius == standard.right);
    CHECK(standard.playerMinY - kCameraShakeY - kPlayerVisualRadius == standard.top);
    CHECK(standard.playerMaxY + kCameraShakeY + kPlayerVisualRadius == standard.bottom);

    // World rendering is inside Camera2D, which clears raylib's regular
    // high-DPI matrix. Verify the camera restores the live framebuffer/client
    // scale and keeps world, player clamp, HUD, and footer on the same edges.
    struct DpiViewport { int width; int height; };
    constexpr DpiViewport dpiViewports[] = {
        {1280, 800}, {1600, 900}, {1920, 1080}, {960, 640},
        {1280, 1024}, {1365, 767}
    };
    constexpr float scales[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    const auto near = [](float a, float b, float tolerance = 1.0f) {
        return std::abs(a - b) <= tolerance;
    };
    for (const auto viewport : dpiViewports) {
        const auto bounds = playBounds(viewport.width, viewport.height);
        const auto hud = hudLayout(viewport.width, viewport.height);
        for (const float scale : scales) {
            const int framebufferWidth = static_cast<int>(std::lround(viewport.width * scale));
            const int framebufferHeight = static_cast<int>(std::lround(viewport.height * scale));
            const auto camera = cameraTransform(viewport.width, viewport.height,
                                                framebufferWidth, framebufferHeight);
            const Point topLeft = cameraWorldToFramebuffer({bounds.left, bounds.top}, camera);
            const Point bottomRight = cameraWorldToFramebuffer({bounds.right, bounds.bottom}, camera);
            CHECK(near(camera.zoom, scale, 0.001f));
            CHECK(near(camera.scaleY, scale, 0.001f));
            CHECK(near(camera.offset.x, framebufferWidth * 0.5f, 0.001f));
            CHECK(near(camera.offset.y, framebufferHeight * 0.5f, 0.001f));
            CHECK(near(cameraWorldToFramebuffer({0.0f, 0.0f}, camera).x, 0.0f));
            CHECK(near(cameraWorldToFramebuffer({static_cast<float>(viewport.width), 0.0f}, camera).x,
                       static_cast<float>(framebufferWidth)));
            CHECK(near(cameraWorldToFramebuffer({0.0f, static_cast<float>(viewport.height)}, camera).y,
                       static_cast<float>(framebufferHeight)));
            CHECK(near(cameraWorldToFramebuffer({0.0f, kHudBottom}, camera).y,
                       kHudBottom * scale));
            CHECK(near(cameraWorldToFramebuffer({0.0f, hud.footerY}, camera).y,
                       hud.footerY * scale));
            CHECK(near(topLeft.x, bounds.left * scale));
            CHECK(near(topLeft.y, bounds.top * scale));
            CHECK(near(bottomRight.x, static_cast<float>(framebufferWidth)));
            CHECK(near(bottomRight.y, bounds.bottom * scale));
            CHECK(near((hud.footerY + kFooterHeight) * scale,
                       static_cast<float>(framebufferHeight)));

            for (const float xShake : {-kCameraShakeX, kCameraShakeX}) {
                for (const float yShake : {-kCameraShakeY, kCameraShakeY}) {
                    const auto shaken = cameraTransform(viewport.width, viewport.height,
                        framebufferWidth, framebufferHeight, xShake, yShake);
                    const Point player = cameraWorldToFramebuffer(
                        {bounds.playerMaxX, bounds.playerMaxY}, shaken);
                    CHECK(player.x + kPlayerVisualRadius * shaken.zoom <= framebufferWidth + 0.01f);
                    CHECK(player.y + kPlayerVisualRadius * shaken.zoom <=
                          cameraWorldToFramebuffer({bounds.right, bounds.bottom}, shaken).y + 0.01f);
                    const Point opposite = cameraWorldToFramebuffer(
                        {bounds.playerMinX, bounds.playerMinY}, shaken);
                    CHECK(opposite.x - kPlayerVisualRadius * shaken.zoom >= -0.01f);
                    CHECK(opposite.y - kPlayerVisualRadius * shaken.zoom >=
                          cameraWorldToFramebuffer({bounds.left, bounds.top}, shaken).y - 0.01f);
                }
            }
        }
    }
}

void testRunGeometryRemapsOnResizeWithoutLeavingPlayableArea() {
    using namespace threadlight::layout;
    const auto original = playBounds(1280, 800);
    const auto resized = playBounds(1920, 1080);
    const Point originalCenter{(original.left + original.right) * 0.5f,
                               (original.top + original.bottom) * 0.5f};
    const Point resizedCenter = remapPoint(originalCenter, original, resized);
    CHECK(std::abs(resizedCenter.x - 960.0f) < 0.01f);
    CHECK(std::abs(resizedCenter.y - (resized.top + resized.bottom) * 0.5f) < 0.01f);
    const Point topLeft = remapPoint({original.left, original.top}, original, resized);
    const Point bottomRight = remapPoint({original.right, original.bottom}, original, resized);
    CHECK(topLeft.x == resized.left && topLeft.y == resized.top);
    CHECK(bottomRight.x == resized.right && bottomRight.y == resized.bottom);

    const auto smaller = playBounds(960, 640);
    const Point beyondRight = remapPoint({original.right + 200.0f, original.bottom}, original, smaller);
    CHECK(beyondRight.x == smaller.right);
    CHECK(beyondRight.y == smaller.bottom);
}

void testDeathTimingsAreDistinctAndBounded() {
    using namespace threadlight::visual;
    CHECK(kSmallDeathSeconds < kHeavyDeathSeconds);
    CHECK(deathProgress(0.0f, kSmallDeathSeconds) == 0.0f);
    CHECK(deathProgress(kSmallDeathSeconds, kSmallDeathSeconds) == 1.0f);
    CHECK(deathProgress(kHeavyDeathSeconds * 0.5f, kHeavyDeathSeconds) > 0.0f);
    CHECK(deathProgress(kHeavyDeathSeconds * 0.5f, kHeavyDeathSeconds) < 1.0f);
    CHECK(deathProgress(2.0f, kHeavyDeathSeconds) == 1.0f);
    CHECK(kSmallShardCount < kHeavyShardCount);
    CHECK(kHeavyShardCount <= kParticleLimit);
    CHECK(kDeathEffectLimit > 0);
}

void testHitFlashAndShakeDecaySmoothly() {
    using namespace threadlight::visual;
    CHECK(hitFlashStrength(0.0f) == 0.0f);
    CHECK(hitFlashStrength(kEnemyHitFlashSeconds) == 1.0f);
    CHECK(hitFlashStrength(kEnemyHitFlashSeconds * 0.5f) > 0.0f);
    CHECK(hitFlashStrength(kEnemyHitFlashSeconds * 0.5f) < 1.0f);
    CHECK(shakeEnvelope(0.0f, kDamageShakeSeconds) == 0.0f);
    CHECK(shakeEnvelope(kDamageShakeSeconds, kDamageShakeSeconds) == 1.0f);
    CHECK(shakeEnvelope(kDamageShakeSeconds * 0.5f, kDamageShakeSeconds) == 0.25f);
}
}

int main() {
    testSilkCoolsContinuouslyAndFadesWithAge();
    testPlayerBoundsFollowLogicalClientAndCameraShake();
    testRunGeometryRemapsOnResizeWithoutLeavingPlayableArea();
    testDeathTimingsAreDistinctAndBounded();
    testHitFlashAndShakeDecaySmoothly();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "All visual effect checks passed.\n";
    return EXIT_SUCCESS;
}
