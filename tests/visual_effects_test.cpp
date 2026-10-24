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

}

// Model the actual Windows raylib 5.5 transitions, including the retained
// startup mouse scale and the independently updated screenScale. On Windows
// GLFW window/client dimensions and framebuffer dimensions are native pixels.
void testRaylibCallbackCoordinateTransitions() {
    using namespace threadlight::layout;
    const auto near=[](float a,float b){return std::abs(a-b)<1.1f;};
    for (const Point startupScale : {Point{1,1}, Point{1.25f,1.25f},
            Point{1.5f,1.5f}, Point{2,2}, Point{1.25f,1.5f}, Point{1.5f,1.3125f}}) {
        int screenWidth=1280,screenHeight=800;
        int renderWidth=int(screenWidth*startupScale.x);
        int renderHeight=int(screenHeight*startupScale.y);
        Point retainedDrawingScale=startupScale;
        const Point mouseScale{1/startupScale.x,1/startupScale.y};
        const auto coordinates=WindowCoordinates::atStartup(
            screenWidth,screenHeight,renderWidth,renderHeight);
        CHECK(coordinates.width(renderWidth)==1280);
        CHECK(coordinates.height(renderHeight)==800);
        CHECK(coordinates.width(coordinates.nativeWidth(960))>=960);
        CHECK(coordinates.height(coordinates.nativeHeight(640))>=640);
        for (int transition=0;transition<6;++transition) {
            if(transition==1||transition==3||transition==5) {
                // WindowSizeCallback -> SetupViewport + screen/currentFbo;
                // no screenScale or mouse-scale update. Resize/maximize/restore.
                renderWidth=screenWidth=(transition==3?2560:transition==5?1201:1537);
                renderHeight=screenHeight=(transition==3?1440:transition==5?901:977);
            } else if(transition==2||transition==4) {
                // WindowContentScaleCallback changes only screenScale. Test
                // both orders relative to size callbacks, plus unequal axes.
                retainedDrawingScale=transition==2?Point{2,1.5f}:Point{1.25f,1};
            }
            const int width=coordinates.width(renderWidth),height=coordinates.height(renderHeight);
            const auto bounds=playBounds(width,height);
            const auto hud=hudLayout(width,height);
            const Point nativeMouse{renderWidth*.6f,renderHeight*.65f};
            const Point mouse{nativeMouse.x*mouseScale.x,nativeMouse.y*mouseScale.y};
            const Point drawn=coordinates.toFramebuffer(mouse);
            CHECK(near(drawn.x,nativeMouse.x));
            CHECK(near(drawn.y,nativeMouse.y));
            CHECK(near(coordinates.toFramebuffer({float(width),float(height)}).x,float(renderWidth)));
            CHECK(near(coordinates.toFramebuffer({float(width),float(height)}).y,float(renderHeight)));
            CHECK(near(coordinates.toFramebuffer({0,hud.footerY+kFooterHeight}).y,float(renderHeight)));
            for(float sx : {-kCameraShakeX,kCameraShakeX})for(float sy : {-kCameraShakeY,kCameraShakeY}) {
                CHECK(playerVisible({bounds.playerMinX,bounds.playerMinY},bounds,kPlayerVisualRadius,{sx,sy}));
                CHECK(playerVisible({bounds.playerMaxX,bounds.playerMaxY},bounds,kPlayerVisualRadius,{sx,sy}));
                const auto player=coordinates.toFramebuffer({bounds.playerMaxX,bounds.playerMaxY},{sx,sy});
                CHECK(player.x+kPlayerVisualRadius*coordinates.scale.x<=renderWidth+1.f);
                CHECK(player.y+kPlayerVisualRadius*coordinates.scale.y<=
                      coordinates.toFramebuffer({0,bounds.bottom}).y+.01f);
            }
            if(transition==2) {
                // Demonstrate why framebuffer/screen ratios cannot recover
                // the retained drawing transform after these callbacks.
                CHECK(float(renderWidth)/screenWidth!=retainedDrawingScale.x);
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
    testRaylibCallbackCoordinateTransitions();
    testRunGeometryRemapsOnResizeWithoutLeavingPlayableArea();
    testDeathTimingsAreDistinctAndBounded();
    testHitFlashAndShakeDecaySmoothly();
    if (failures != 0) return EXIT_FAILURE;
    std::cout << "All visual effect checks passed.\n";
    return EXIT_SUCCESS;
}
