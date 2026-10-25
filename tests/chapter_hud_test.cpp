#include "../src/chapter_hud.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

void layoutBounds() {
    using namespace threadlight::chapter_hud;
    const Layout minimum = layout(960, 640);
    check(near(minimum.bossBar.x, 230.0f), "minimum boss bar is centered");
    check(near(minimum.bossBar.y, 68.0f) && near(minimum.bossBar.width, 500.0f) &&
              near(minimum.bossBar.height, 4.0f), "minimum boss bar geometry");
    check(minimum.bossBar.x >= 0.0f && minimum.bossBar.x + minimum.bossBar.width <= 960.0f,
          "boss bar stays within logical width");
    check(near(minimum.dashTrack.x, 222.0f) && near(minimum.dashTrack.y, 613.0f) &&
              near(minimum.dashTrack.width, 80.0f) && near(minimum.dashTrack.height, 2.0f),
          "dash track follows the footer layout");
    check(minimum.dashTrack.x + minimum.dashTrack.width < 960.0f * 0.4f,
          "dash track ends before the hint");
    check(minimum.dashTrack.y + minimum.dashTrack.height < 640.0f,
          "dash track stays inside the logical height");

    const Layout wide = layout(1600, 900);
    check(near(wide.bossBar.x, 550.0f) && near(wide.bossBar.width, 500.0f),
          "wide boss bar remains centered and capped");
    const Layout narrow = layout(1000, 700);
    check(near(narrow.bossBar.width, 500.0f) && near(narrow.bossBar.x, 250.0f),
          "minimum supported width keeps the boss bar bounded");
}

void cooldownView() {
    using namespace threadlight::chapter_hud;
    const DashView mid = dashView(2.0f, 4.0f, false, true);
    check(near(mid.fill, 0.5f), "fill uses captured cooldown duration");
    check(mid.remainingTenths == 20 && mid.state == DashState::Recharging,
          "recharging time is rounded up to tenths");

    const DashView upgradedCaptured = dashView(2.0f, 2.0f, false, true);
    check(near(upgradedCaptured.fill, 0.0f), "captured shorter cooldown controls fill");
    check(upgradedCaptured.remainingTenths == mid.remainingTenths,
          "remaining time does not depend on a mutable stats denominator");

    const DashView lastFraction = dashView(0.00001f, 4.0f, false, true);
    check(lastFraction.state == DashState::Recharging && lastFraction.remainingTenths == 1,
          "positive last fraction remains recharging for at least one tenth");
    check(near(lastFraction.fill, 1.0f), "last fraction can show a full progress bar before readiness");

    const DashView readyMoving = dashView(0.0f, 4.0f, false, true);
    const DashView readyStationary = dashView(0.0f, 4.0f, false, false);
    check(readyMoving.state == DashState::Ready && readyMoving.remainingTenths == 0,
          "expired moving dash is ready");
    check(readyStationary.state == DashState::ReadyMove,
          "expired stationary dash prompts movement");

    const DashView active = dashView(3.0f, 4.0f, true, false);
    check(active.state == DashState::Active && active.remainingTenths == 30,
          "active state takes precedence over recharge and stationary hint");
    check(near(active.fill, 0.25f), "active view retains bounded cooldown fill");

    const DashView invalidDuration = dashView(1.0f, 0.0f, false, true);
    check(invalidDuration.state == DashState::Recharging && invalidDuration.remainingTenths == 10,
          "zero duration denominator does not grant readiness");
    check(near(invalidDuration.fill, 0.0f), "invalid duration produces bounded fill");

    const DashView hugeRemaining = dashView(std::numeric_limits<float>::max(), 1.0f, false, true);
    check(hugeRemaining.state == DashState::Recharging &&
              hugeRemaining.remainingTenths == std::numeric_limits<int>::max(),
          "very large finite remaining time is safely bounded");

    const DashView frozen = dashView(1.234f, 5.0f, false, true);
    const DashView frozenAgain = dashView(1.234f, 5.0f, false, true);
    check(near(frozen.fill, frozenAgain.fill) && frozen.remainingTenths == frozenAgain.remainingTenths &&
              frozen.state == frozenAgain.state,
          "same captured inputs produce deterministic view");
}

}  // namespace

int main() {
    layoutBounds();
    cooldownView();
    if (failures != 0) {
        std::cerr << failures << " chapter HUD check(s) failed\n";
        return 1;
    }
    std::cout << "chapter HUD checks passed\n";
    return 0;
}
