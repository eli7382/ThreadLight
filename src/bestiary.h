#pragma once
#include <array>

namespace threadlight::bestiary {
struct Entry { const char* name; const char* behavior; const char* tell; const char* counter; };
inline constexpr std::array<Entry, 11> entries{{
    {"HUNTER", "A basic pursuer follows your path.", "Watch its approach and leave yourself an exit.", "Move across its path so it crosses your silk."},
    {"BRUTE", "A slower, tougher pursuer with heavy contact damage.", "Its larger body approaches deliberately.", "Give it space and draw several silk passes."},
    {"SKITTER", "A fast flanker that commits to a burst of movement.", "Watch for its change in direction before a rush.", "Change lanes after it commits; avoid backing into it."},
    {"SPITTER", "A ranged enemy that fires projectiles.", "Watch its firing lane and incoming shots.", "Keep moving across the lane and circle toward it."},
    {"BRIAR CALLER", "Nearby enemies move faster inside its visible aura. Callers seek each other and fuse into stronger groups.", "Its clear circle shows the aura and fusion range; larger groups have a stronger aura.", "Keep callers apart or take one out before they merge. Silk damage remains effective."},
    {"SOWER", "A foe that leaves dangerous areas on the ground.", "Watch the ground as hazards form and expire.", "Keep an escape route clear and avoid active fields."},
    {"THE ROOTWARDEN", "A guardian that attacks with growing roots and lunges.", "Lines mark the direction of an upcoming attack.", "Move out of the marked path and strike between attacks."},
    {"THE NEEDLE DANCER", "A swift guardian that commits to strikes and steps.", "A marked lane warns of its next movement.", "Leave the lane early and approach during recovery."},
    {"THE THORN CONDUCTOR", "A guardian that combines firing patterns and buds.", "Firing lines and buds reveal incoming pressure.", "Move between firing lanes and cut buds when safe."},
    {"THE HOLLOW WEAVER", "A guardian that fills the arena with spatial patterns.", "Watch marked paths and spreading fields.", "Use the open lanes; attack during recovery."},
    {"THE BRIAR HEART", "The final guardian uses multi-stage arena patterns.", "Markers show where its next sequence will reach.", "Keep moving through safe gaps and watch its phase."},
}};
inline bool unlocked(int mask, int index) { return index >= 0 && index < static_cast<int>(entries.size()) && (mask & (1 << index)); }
} // namespace threadlight::bestiary
