#pragma once

#include "combat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace threadlight::support_formations {

using Point = foundation::Point;

enum class Kind { RoyalGuard, PredatorsNet, ThornDominion, ShrapnelCage,
                  EarlierWave14, EarlierWave21, EarlierWave28 };
enum class Role { Caller, Guard, Interceptor, Ranged, Sower };

constexpr int kMaxMembers = 5;
constexpr int kMaxBudget = 8;
constexpr int kMaxCapacity = 5;
constexpr int kRepeatDelayTicks = 600;

struct Bounds { float left{}, top{}, right{}, bottom{}; };
struct Member {
    combat::EnemyKind kind{combat::EnemyKind::Hunter};
    Role role{Role::Interceptor};
    // Local x points toward the player; local y points to the left of that axis.
    Point offset{};
    int deployTick{};
};
struct Config {
    Kind kind{Kind::RoyalGuard};
    std::array<Member, kMaxMembers> members{};
    int memberCount{};
    int budget{};
    int capacity{};
};
struct Placement { Point point{}; bool valid{}; };

inline Config configure(Kind kind, int budget, int capacity) {
    Config result;
    result.kind = kind;
    result.budget = std::clamp(budget, 0, kMaxBudget);
    result.capacity = std::clamp(capacity, 0, kMaxCapacity);
    auto add = [&](combat::EnemyKind enemy, Role role, Point offset, int tick) {
        result.members[result.memberCount++] = {enemy, role, offset, tick};
    };
    switch (kind) {
    case Kind::RoyalGuard:
        // Brute occupies the player-facing lane while Caller stays behind it.
        add(combat::EnemyKind::Brute, Role::Guard, {76.f, 0.f}, 0);
        add(combat::EnemyKind::BriarCaller, Role::Caller, {-62.f, 0.f}, 150);
        break;
    case Kind::PredatorsNet:
        add(combat::EnemyKind::Hunter, Role::Interceptor, {0.f, -76.f}, 0);
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 76.f}, 105);
        break;
    case Kind::ThornDominion:
        add(combat::EnemyKind::Brute, Role::Guard, {76.f, -54.f}, 0);
        add(combat::EnemyKind::BriarCaller, Role::Caller, {-62.f, 0.f}, 120);
        add(combat::EnemyKind::Sower, Role::Sower, {-8.f, 90.f}, 240);
        break;
    case Kind::ShrapnelCage:
        add(combat::EnemyKind::Spitter, Role::Ranged, {0.f, -104.f}, 0);
        add(combat::EnemyKind::Spitter, Role::Ranged, {0.f, 104.f}, 210);
        break;
    case Kind::EarlierWave14:
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 0.f}, 0);
        add(combat::EnemyKind::Hunter, Role::Interceptor, {-92.f, 0.f}, 120);
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 0.f}, 240);
        add(combat::EnemyKind::Brute, Role::Guard, {72.f, -64.f}, 360);
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 0.f}, 480);
        break;
    case Kind::EarlierWave21:
        add(combat::EnemyKind::Brute, Role::Guard, {76.f, 0.f}, 0);
        add(combat::EnemyKind::BriarCaller, Role::Caller, {-62.f, 0.f}, 150);
        add(combat::EnemyKind::Hunter, Role::Interceptor, {0.f, -76.f}, 300);
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 76.f}, 450);
        break;
    case Kind::EarlierWave28:
        add(combat::EnemyKind::Sower, Role::Sower, {-76.f, 0.f}, 0);
        add(combat::EnemyKind::Spitter, Role::Ranged, {0.f, -104.f}, 150);
        add(combat::EnemyKind::Brute, Role::Guard, {76.f, 0.f}, 300);
        add(combat::EnemyKind::Skitter, Role::Interceptor, {0.f, 76.f}, 450);
        add(combat::EnemyKind::BriarCaller, Role::Caller, {-62.f, -50.f}, 600);
        break;
    }
    return result;
}

// Displayed wave numbers are one-based. Unlisted encounters have no override.
inline Config earlierBossWave(int displayedWave) {
    switch (displayedWave) {
    case 14: return configure(Kind::EarlierWave14, 8, 4);
    case 21: return configure(Kind::EarlierWave21, 8, 4);
    case 28: return configure(Kind::EarlierWave28, 8, 4);
    default: return {};
    }
}

inline const Member* memberForOrdinal(const Config& config, int ordinal) {
    if (ordinal < 0 || ordinal >= config.budget || config.memberCount <= 0) return nullptr;
    return &config.members[ordinal % config.memberCount];
}

inline bool canRelease(const Config& config, int ordinal, int activeCount) {
    return memberForOrdinal(config, ordinal) != nullptr && config.capacity > 0 &&
           activeCount >= 0 && activeCount < config.capacity;
}

inline int deploymentTick(const Config& config, int ordinal) {
    const Member* member = memberForOrdinal(config, ordinal);
    if (!member) return -1;
    return member->deployTick + (ordinal / config.memberCount) * kRepeatDelayTicks;
}

inline Point desiredPosition(const Member& member, Point anchor, Point player) {
    const float dx = player.x - anchor.x;
    const float dy = player.y - anchor.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    const Point forward = length > 0.0001f ? Point{dx / length, dy / length} : Point{1.f, 0.f};
    const Point side{-forward.y, forward.x};
    return {anchor.x + forward.x * member.offset.x + side.x * member.offset.y,
            anchor.y + forward.y * member.offset.x + side.y * member.offset.y};
}

inline Placement safePlacement(Point desired, Bounds bounds, Point player,
                                float padding, float minimumSeparation) {
    Placement result{};
    if (!std::isfinite(desired.x) || !std::isfinite(desired.y) ||
        !std::isfinite(player.x) || !std::isfinite(player.y) ||
        !std::isfinite(bounds.left) || !std::isfinite(bounds.top) ||
        !std::isfinite(bounds.right) || !std::isfinite(bounds.bottom) ||
        !std::isfinite(padding) || !std::isfinite(minimumSeparation) ||
        padding < 0.f || minimumSeparation < 0.f ||
        bounds.right - bounds.left < 2.f * padding ||
        bounds.bottom - bounds.top < 2.f * padding) return result;

    const float left = bounds.left + padding, right = bounds.right - padding;
    const float top = bounds.top + padding, bottom = bounds.bottom - padding;
    const float minSquared = minimumSeparation * minimumSeparation;
    auto accept = [&](Point candidate) {
        if (candidate.x < left || candidate.x > right || candidate.y < top || candidate.y > bottom)
            return false;
        const float dx = candidate.x - player.x, dy = candidate.y - player.y;
        if (dx * dx + dy * dy < minSquared) return false;
        result = {candidate, true};
        return true;
    };

    const Point clamped{std::clamp(desired.x, left, right), std::clamp(desired.y, top, bottom)};
    if (accept(clamped)) return result;

    // Search around the player at the requested radius. Return invalid when
    // the arena cannot satisfy the separation instead of silently violating it.
    constexpr int kCandidates = 72;
    constexpr float kPi = 3.14159265358979323846f;
    const float radius = minimumSeparation + 0.01f;
    const float start = std::atan2(desired.y - player.y, desired.x - player.x);
    for (int i = 0; i < kCandidates; ++i) {
        const float angle = start + 2.f * kPi * static_cast<float>(i) / kCandidates;
        const Point candidate{player.x + std::cos(angle) * radius,
                              player.y + std::sin(angle) * radius};
        if (accept(candidate)) return result;
    }
    return result;
}

} // namespace threadlight::support_formations
