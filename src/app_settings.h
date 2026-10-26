#pragma once

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace threadlight::settings {

struct AppSettings {
    int frameRateLimit = 60;
    bool showFps = true;
    int sfxVolumePercent = 78;
    bool tutorialCompleted = false;
    int codexSeen = 0;
    int bestWave = 0;
    std::uint64_t bestScore = 0, bestKills = 0;
    bool bestWon = false;
};

inline bool isBetterRun(const AppSettings& settings, bool won, int wave, std::uint64_t score) {
    if (settings.bestWave == 0) return true;
    if (won != settings.bestWon) return won;
    return wave > settings.bestWave || (wave == settings.bestWave && score > settings.bestScore);
}

// Live edits are cheap; disk writes happen once at an edit boundary. Failed
// saves remain pending so a later boundary (including shutdown) can retry.
struct PendingEdits {
    bool dragging = false;
    bool dirty = false;

    bool finishDrag(bool audioScreen, bool focused, bool mouseDown, bool keyboardUsed) {
        if (!dragging || (audioScreen && focused && mouseDown && !keyboardUsed)) return false;
        dragging = false;
        return true;
    }
    template<class Save>
    bool flush(Save save) {
        if (!dirty) return true;
        if (!save()) return false;
        dirty = false;
        return true;
    }
};

constexpr int kDefaultFrameRateLimit = 60;
constexpr int kDefaultSfxVolumePercent = 78;

inline std::filesystem::path userSettingsPath() {
#if defined(_WIN32)
    if (const wchar_t* appData = _wgetenv(L"APPDATA"); appData && *appData)
        return std::filesystem::path(appData) / "ThreadLight" / "settings.ini";
    if (const wchar_t* profile = _wgetenv(L"USERPROFILE"); profile && *profile)
        return std::filesystem::path(profile) / "AppData" / "Roaming" /
               "ThreadLight" / "settings.ini";
    return {};
#else
    if (const char* configHome = std::getenv("XDG_CONFIG_HOME"); configHome && *configHome)
        return std::filesystem::path(configHome) / "ThreadLight" / "settings.ini";
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".config" / "ThreadLight" / "settings.ini";
    return {};
#endif
}

inline bool validFrameRateLimit(int value) {
    return value == 30 || value == 60 || value == 120 || value == 0;
}

inline int nextFrameRateLimit(int current, int direction) {
    constexpr int choices[] = {30, 60, 120, 0};
    int index = 1;
    for (int i = 0; i < 4; ++i) {
        if (choices[i] == current) index = i;
    }
    index = (index + (direction < 0 ? 3 : 1)) % 4;
    return choices[index];
}

inline int adjustSfxVolume(int current, int direction) {
    return std::clamp(current + (direction < 0 ? -5 : (direction > 0 ? 5 : 0)), 0, 100);
}

inline int quantizeSfxVolume(int percent) {
    return std::clamp(static_cast<int>((std::clamp(percent, 0, 100) + 2) / 5) * 5,
                      0, 100);
}

inline bool parseInteger(std::string_view text, int& result) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

inline AppSettings load(const std::filesystem::path& path) {
    AppSettings settings;
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error) return settings;
    const auto bytes = std::filesystem::file_size(path, error);
    if (error || bytes > 4096) return settings;

    std::ifstream input(path, std::ios::binary);
    if (!input) return settings;
    std::string line;
    while (std::getline(input, line)) {
        if (line.size() > 128) continue;
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string_view key(line.data(), separator);
        const std::string_view value(line.data() + separator + 1, line.size() - separator - 1);
        int parsed = 0;
        if (key == "frame_rate_limit") {
            if (parseInteger(value, parsed) && validFrameRateLimit(parsed)) settings.frameRateLimit = parsed;
            else settings.frameRateLimit = kDefaultFrameRateLimit;
        } else if (key == "show_fps") {
            if (parseInteger(value, parsed) && (parsed == 0 || parsed == 1)) settings.showFps = parsed == 1;
            else settings.showFps = true;
        } else if (key == "sfx_volume_percent") {
            if (parseInteger(value, parsed) && parsed >= 0 && parsed <= 100) settings.sfxVolumePercent = parsed;
            else settings.sfxVolumePercent = kDefaultSfxVolumePercent;
        } else if (key == "tutorial_completed") {
            settings.tutorialCompleted = value == "1";
        } else if (key == "codex_seen") {
            if (parseInteger(value, parsed) && parsed >= 0 && parsed < (1 << 11)) settings.codexSeen = parsed;
        } else if (key == "best_wave") {
            if (parseInteger(value, parsed) && parsed >= 0 && parsed <= 35) settings.bestWave = parsed;
        } else if (key == "best_won") {
            settings.bestWon = value == "1";
        } else if (key == "best_score" || key == "best_kills") {
            std::uint64_t number = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
            if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) {
                if (key == "best_score") settings.bestScore = number; else settings.bestKills = number;
            }
        }
    }
    return settings;
}

inline bool save(const std::filesystem::path& path, const AppSettings& settings) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;

    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output << "version=1\n"
               << "frame_rate_limit=" << (validFrameRateLimit(settings.frameRateLimit)
                    ? settings.frameRateLimit : kDefaultFrameRateLimit) << '\n'
               << "show_fps=" << (settings.showFps ? 1 : 0) << '\n'
               << "sfx_volume_percent=" << std::clamp(settings.sfxVolumePercent, 0, 100) << '\n'
               << "tutorial_completed=" << (settings.tutorialCompleted ? 1 : 0) << '\n'
               << "codex_seen=" << (settings.codexSeen & 2047) << '\n'
               << "best_wave=" << std::clamp(settings.bestWave, 0, 35) << '\n'
               << "best_score=" << settings.bestScore << '\n'
               << "best_kills=" << settings.bestKills << '\n'
               << "best_won=" << (settings.bestWon ? 1 : 0) << '\n';
        output.flush();
        if (!output) return false;
    }

    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace threadlight::settings
