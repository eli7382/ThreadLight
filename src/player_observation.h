#pragma once

#include "combat.h"
#include "game_geometry.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace threadlight::observation {

// Explicit world-space bounds make the signal independent of window/DPI layout.
struct Bounds { float left{}, top{}, right{1.f}, bottom{1.f}; };
enum class OffenseTarget : std::uint8_t { Pursuer, Heavy, Flanker, Ranged, Caller, Sower, Other, Count };

struct Summary {
    float movement = 0.f;       // Fraction of observed samples with meaningful displacement.
    float dashUse = 0.f;        // Fraction of samples spent dashing.
    float dashStartRate = 0.f;
    float dashHitRate = 0.f;    // Fraction of recorded incoming hits that happened during a dash.
    float incomingPressure = 0.f;
    float offense = 0.f;        // Decayed successful offense events per normalized unit.
    float counterSuccess = 0.f;
    float responseRate = 0.f;
    float confidence = 0.f;     // 0..1 from the bounded sample count.
    float horizontalBias = 0.f; // -1..1, recent normalized movement direction.
    float verticalBias = 0.f;
    float perimeter = 0.f;      // Fraction of recent positions near the arena edge.
    float orbitBias = 0.f;      // -1..1, directional tendency around arena center.
    float repetition = 0.f;     // Fraction of moving samples in the most repeated heading bin.
    std::array<float, static_cast<std::size_t>(OffenseTarget::Count)> offenseByTarget{};
};

// Campaign-local state. Every collection is fixed-size; evidence decays so an
// early play style cannot dominate a much later wave.
class State {
public:
    static constexpr std::size_t kMovementCapacity = 64;
    static constexpr std::size_t kTargetCount = static_cast<std::size_t>(OffenseTarget::Count);

    void reset() { *this = State{}; }

    void observe(foundation::Point previous, foundation::Point player, bool dashing,
                 bool dashStarted, Bounds bounds) {
        decay();
        const float width = std::max(1.f, bounds.right - bounds.left);
        const float height = std::max(1.f, bounds.bottom - bounds.top);
        const float dxWorld = player.x - previous.x, dyWorld = player.y - previous.y;
        const float dx = std::clamp(dxWorld / width, -1.f, 1.f);
        const float dy = std::clamp(dyWorld / height, -1.f, 1.f);
        const bool moving = std::sqrt(dxWorld * dxWorld + dyWorld * dyWorld) > .75f;
        const float px = std::clamp((previous.x - bounds.left) / width, 0.f, 1.f) - .5f;
        const float py = std::clamp((previous.y - bounds.top) / height, 0.f, 1.f) - .5f;
        const float orbitCross = px * dy - py * dx;
        const float nx = std::clamp((player.x - bounds.left) / width, 0.f, 1.f);
        const float ny = std::clamp((player.y - bounds.top) / height, 0.f, 1.f);
        const bool perimeter = nx < .15f || nx > .85f || ny < .15f || ny > .85f;
        const auto heading = static_cast<std::uint8_t>(moving ?
            (static_cast<int>(std::floor((std::atan2(dyWorld, dxWorld) + 3.14159265359f) * (4.f / 3.14159265359f))) & 7) : 0);
        movement_[movementHead_] = {previous, player, moving, dashing, dashStarted, perimeter,
                                    moving ? orbitCross : 0.f, heading};
        movementPoints_[movementHead_] = player;
        movementHead_ = (movementHead_ + 1) % kMovementCapacity;
        movementCount_ = std::min(kMovementCapacity, movementCount_ + 1);
        ++observations_;
        horizontal_ = horizontal_ * .985f + dx;
        vertical_ = vertical_ * .985f + dy;
    }

    void onOffense(OffenseTarget target, bool successful = true) {
        if (!successful) return;
        offense_ = std::min(64.f, offense_ + 1.f);
        const auto index = std::min(static_cast<std::size_t>(target), kTargetCount - 1);
        offenseByTarget_[index] = std::min(32.f, offenseByTarget_[index] + 1.f);
    }
    void onOffense(combat::EnemyKind target, bool successful = true) {
        onOffense(targetCategory(target), successful);
    }
    void onIncoming(int damage, bool dash) {
        if (damage <= 0) return;
        incoming_ = std::min(64.f, incoming_ + 1.f);
        if (dash) dashIncoming_ = std::min(64.f, dashIncoming_ + 1.f);
        damageTaken_ = std::min(256.f, damageTaken_ + std::clamp(static_cast<float>(damage) / 1000.f, .1f, 8.f));
    }
    void onCounterResult(bool successful) {
        counterAttempts_ = std::min(64.f, counterAttempts_ + 1.f);
        if (successful) counterSuccesses_ = std::min(64.f, counterSuccesses_ + 1.f);
    }
    // A response is an intentional action after a threat cue; it is supplied by
    // gameplay at the point that cue-response pairing is known.
    void onThreatResponse(bool responded) {
        responseOpportunities_ = std::min(64.f, responseOpportunities_ + 1.f);
        if (responded) responses_ = std::min(64.f, responses_ + 1.f);
    }

    Summary summary() const {
        Summary out;
        const float denom = std::max(1.f, static_cast<float>(movementCount_));
        float moving = 0.f, dashing = 0.f, dashStarts = 0.f, perimeter = 0.f, orbit = 0.f, orbitMagnitude = 0.f;
        std::array<float, 8> headings{};
        for (std::size_t i = 0; i < movementCount_; ++i) {
            if (movement_[i].moving) moving += 1.f;
            if (movement_[i].dashing) dashing += 1.f;
            if (movement_[i].dashStarted) dashStarts += 1.f;
            if (movement_[i].perimeter) perimeter += 1.f;
            orbit += movement_[i].orbitCross;
            orbitMagnitude += std::abs(movement_[i].orbitCross);
            if (movement_[i].moving) headings[movement_[i].heading] += 1.f;
        }
        out.movement = moving / denom;
        out.dashUse = dashing / denom;
        out.dashStartRate = dashStarts / denom;
        out.perimeter = perimeter / denom;
        out.orbitBias = orbitMagnitude > 1.e-6f ? std::clamp(orbit / orbitMagnitude, -1.f, 1.f) : 0.f;
        out.repetition = moving > 0.f ? *std::max_element(headings.begin(), headings.end()) / moving : 0.f;
        out.dashHitRate = incoming_ > 0.f ? std::clamp(dashIncoming_ / incoming_, 0.f, 1.f) : 0.f;
        out.incomingPressure = damageTaken_ / 16.f;
        out.offense = offense_ / 16.f;
        out.counterSuccess = counterAttempts_ > 0.f ? counterSuccesses_ / counterAttempts_ : 0.f;
        out.responseRate = responseOpportunities_ > 0.f ? responses_ / responseOpportunities_ : 0.f;
        out.confidence = std::clamp(static_cast<float>(observations_ + eventSamples_) / 240.f, 0.f, 1.f);
        const float directionScale = static_cast<float>(std::max<std::size_t>(1, movementCount_));
        out.horizontalBias = std::clamp(horizontal_ / directionScale * 32.f, -1.f, 1.f);
        out.verticalBias = std::clamp(vertical_ / directionScale * 32.f, -1.f, 1.f);
        for (std::size_t i = 0; i < kTargetCount; ++i) out.offenseByTarget[i] = offenseByTarget_[i] / 8.f;
        return out;
    }

    std::size_t movementCount() const { return movementCount_; }
    const std::array<foundation::Point, kMovementCapacity>& movementHistory() const { return movementPoints_; }

private:
    struct Sample {
        foundation::Point previous{}, position{};
        bool moving{}, dashing{}, dashStarted{}, perimeter{};
        float orbitCross{};
        std::uint8_t heading{};
    };
    void decay() {
        constexpr float kDecay = .9995f;
        offense_ *= kDecay; incoming_ *= kDecay; dashIncoming_ *= kDecay; damageTaken_ *= kDecay;
        counterAttempts_ *= kDecay; counterSuccesses_ *= kDecay;
        responseOpportunities_ *= kDecay; responses_ *= kDecay;
        for (float& value : offenseByTarget_) value *= kDecay;
        ++eventSamples_;
    }
    static OffenseTarget targetCategory(combat::EnemyKind kind) {
        switch (kind) {
        case combat::EnemyKind::Hunter: return OffenseTarget::Pursuer;
        case combat::EnemyKind::Brute: return OffenseTarget::Heavy;
        case combat::EnemyKind::Skitter: return OffenseTarget::Flanker;
        case combat::EnemyKind::Spitter: return OffenseTarget::Ranged;
        case combat::EnemyKind::BriarCaller: return OffenseTarget::Caller;
        case combat::EnemyKind::Sower: return OffenseTarget::Sower;
        }
        return OffenseTarget::Other;
    }
    std::array<foundation::Point, kMovementCapacity> movementPoints_{};
    std::array<Sample, kMovementCapacity> movement_{};
    std::array<float, kTargetCount> offenseByTarget_{};
    std::size_t movementHead_{}, movementCount_{};
    std::uint64_t observations_{};
    float horizontal_{}, vertical_{};
    float offense_{}, incoming_{}, dashIncoming_{}, damageTaken_{};
    float counterAttempts_{}, counterSuccesses_{}, responseOpportunities_{}, responses_{};
    std::uint64_t eventSamples_{};
};

} // namespace threadlight::observation
