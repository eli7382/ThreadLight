#pragma once

#include <cstdint>

namespace threadlight::cosmetics {

enum class PaletteId : int { OriginalCyan = 0, VioletAscendant = 1 };

struct Rgba {
    std::uint8_t r, g, b, a = 255;
};

struct Palette {
    Rgba silkHaloFresh, silkHaloAged;
    Rgba silkBodyFresh, silkBodyAged;
    Rgba silkCoreFresh, silkCoreAged;
    Rgba afterimageHalo, afterimageCore, afterimageSpark;
    Rgba dashHalo, dashCore, dashSpark;
    Rgba playerAuraOuter, playerAuraInner, playerRim, playerCore, playerDashRim;
    std::uint8_t playerAuraOuterAlpha, playerAuraInnerAlpha;
    Rgba accent, highlight;
};

inline constexpr Palette kOriginalCyan{
    {0,222,211}, {42,129,165}, {14,247,220}, {45,151,184},
    {224,255,246}, {119,184,204}, {129,202,255}, {172,230,255}, {237,230,180},
    {0,183,229}, {190,248,255}, {67,220,255}, {45,230,214},
    {51,230,221}, {164,255,232}, {221,255,245}, {126,239,255},
    18, 34, {67,220,206}, {224,255,247}
};

// Deep indigo keeps the fine silk legible; violet light is layered beneath a
// cool lavender core so the palette stays luminous without broad bloom.
inline constexpr Palette kVioletAscendant{
    {132,73,242}, {76,69,151}, {178,113,255}, {104,91,190},
    {244,230,255}, {178,170,224}, {246,235,255}, {137,91,255}, {224,205,255},
    {112,61,239}, {233,211,255}, {194,157,255}, {130,83,238},
    {158,106,250}, {206,170,255}, {239,226,255}, {231,207,255},
    29, 48, {167,110,255}, {246,235,255}
};

inline constexpr bool validPaletteId(int value) {
    return value == static_cast<int>(PaletteId::OriginalCyan) ||
           value == static_cast<int>(PaletteId::VioletAscendant);
}

inline constexpr const Palette& palette(PaletteId id) {
    return id == PaletteId::VioletAscendant ? kVioletAscendant : kOriginalCyan;
}

inline constexpr PaletteId normalizePalette(int value, bool violetUnlocked) {
    return value == static_cast<int>(PaletteId::VioletAscendant) && violetUnlocked
        ? PaletteId::VioletAscendant : PaletteId::OriginalCyan;
}

inline constexpr Rgba withAlpha(Rgba color, std::uint8_t alpha) {
    color.a = alpha;
    return color;
}

inline constexpr Rgba mix(Rgba from, Rgba to, float amount) {
    const float t = amount < 0.f ? 0.f : (amount > 1.f ? 1.f : amount);
    return {
        static_cast<std::uint8_t>(from.r + (static_cast<float>(to.r) - from.r) * t),
        static_cast<std::uint8_t>(from.g + (static_cast<float>(to.g) - from.g) * t),
        static_cast<std::uint8_t>(from.b + (static_cast<float>(to.b) - from.b) * t),
        static_cast<std::uint8_t>(from.a + (static_cast<float>(to.a) - from.a) * t)
    };
}

} // namespace threadlight::cosmetics
