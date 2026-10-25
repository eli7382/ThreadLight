#pragma once

#include <algorithm>
#include <array>

namespace threadlight::upgrade_ui {

struct Rect {
    float x;
    float y;
    float width;
    float height;
};

struct Layout {
    Rect panel;
    std::array<Rect, 3> cards{};
    Rect confirm;
};

inline Layout layout(int logicalWidth, int logicalHeight, int cardCount = 3) {
    const float width = static_cast<float>(std::max(1, logicalWidth));
    const float height = static_cast<float>(std::max(1, logicalHeight));
    const Rect panel{(width - std::min(900.0f, std::max(1.0f, width - 40.0f))) * 0.5f,
                     (height - std::min(480.0f, std::max(1.0f, height - 40.0f))) * 0.5f,
                     std::min(900.0f, std::max(1.0f, width - 40.0f)),
                     std::min(480.0f, std::max(1.0f, height - 40.0f))};
    Layout result{panel, {}, {width * 0.5f - 90.0f, panel.y + panel.height - 58.0f, 180.0f, 40.0f}};
    const int count = std::clamp(cardCount, 0, 3);
    if (count == 0) return result;

    constexpr float gap = 16.0f;
    const float available = std::max(1.0f, panel.width - 56.0f);
    const float cardWidth = std::min(280.0f, (available - gap * (count - 1)) / count);
    const float rowWidth = cardWidth * count + gap * (count - 1);
    const float startX = panel.x + (panel.width - rowWidth) * 0.5f;
    const float cardY = panel.y + 118.0f;
    for (int i = 0; i < count; ++i)
        result.cards[i] = {startX + i * (cardWidth + gap), cardY, cardWidth, 240.0f};
    return result;
}

// Rectangles use half-open edges: left/top are owned; right/bottom are not.
inline bool contains(Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y &&
           x < rect.x + rect.width && y < rect.y + rect.height;
}

inline int cardAt(const Layout& value, float x, float y, int cardCount = 3) {
    const int count = std::clamp(cardCount, 0, 3);
    for (int i = 0; i < count; ++i)
        if (contains(value.cards[i], x, y)) return i;
    return -1;
}

} // namespace threadlight::upgrade_ui
