#include "../src/upgrade_layout.h"

#include <cmath>
#include <iostream>
#include <utility>

namespace {
int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool inside(const threadlight::upgrade_ui::Rect& r, float w, float h) {
    return r.x >= 0.0f && r.y >= 0.0f && r.width >= 0.0f && r.height >= 0.0f &&
           r.x + r.width <= w && r.y + r.height <= h;
}

bool overlaps(const threadlight::upgrade_ui::Rect& a,
              const threadlight::upgrade_ui::Rect& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}
}

int main() {
    using namespace threadlight::upgrade_ui;
    for (const auto& [w, h] : {std::pair{960, 640}, {1280, 800}, {1600, 900}, {1920, 1080}}) {
        const Layout value = layout(w, h);
        check(inside(value.panel, static_cast<float>(w), static_cast<float>(h)), "panel fits window");
        check(value.panel.width <= 900.0f && value.panel.height <= 480.0f, "panel respects caps");
        check(inside(value.confirm, static_cast<float>(w), static_cast<float>(h)), "confirm fits window");
        for (int i = 0; i < 3; ++i) {
            check(inside(value.cards[i], static_cast<float>(w), static_cast<float>(h)), "card fits window");
            check(contains(value.cards[i], value.cards[i].x + value.cards[i].width / 2.0f,
                           value.cards[i].y + value.cards[i].height / 2.0f), "card center is hittable");
            for (int j = i + 1; j < 3; ++j)
                check(!overlaps(value.cards[i], value.cards[j]), "cards are disjoint");
        }
    }

    const Layout three = layout(960, 640);
    const Rect first = three.cards[0];
    check(contains(first, first.x, first.y), "left/top edges belong to rectangle");
    check(!contains(first, first.x + first.width, first.y + 1.0f), "right edge is excluded");
    check(!contains(first, first.x + 1.0f, first.y + first.height), "bottom edge is excluded");
    check(cardAt(three, first.x, first.y) == 0, "card hit owns left/top edge");
    check(cardAt(three, first.x + first.width, first.y + 1.0f) == -1, "card hit excludes right edge");
    check(cardAt(three, first.x + first.width + 8.0f, first.y + 10.0f) == -1, "gap is not a card");
    check(cardAt(three, 0.0f, 0.0f) == -1, "outside point misses");

    const Layout one = layout(1280, 800, 1);
    const Layout two = layout(1280, 800, 2);
    check(std::abs((one.cards[0].x + one.cards[0].width / 2.0f) - 640.0f) < 0.01f,
          "single card is centered");
    check(std::abs(two.cards[0].x + two.cards[1].x + two.cards[1].width - 1280.0f) < 0.01f,
          "two-card row is centered");
    check(cardAt(two, two.cards[1].x + 1.0f, two.cards[1].y + 1.0f, 1) == -1,
          "hit testing honors actual count");
    check(layout(1280, 800, 0).cards[0].width == 0.0f, "zero cards produces no card geometry");
    check(cardAt(three, first.x + 1.0f, first.y + 1.0f, 0) == -1, "zero count has no hits");

    if (failures != 0) return 1;
    std::cout << "upgrade_layout_test: all checks passed\n";
    return 0;
}
