#pragma once

#include "unweaver.h"
#include "unweaver_barriers.h"
#include "rootwarden.h"
#include "game_layout.h"
#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace threadlight::unweaver_render {

using Point = foundation::Point;
using State = rootwarden::Stage;

inline Vector2 vec(Point p) { return {p.x, p.y}; }
inline Point mix(Point a, Point b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
inline Point offset(Point p, float angle, float distance) {
    return {p.x + std::cos(angle) * distance, p.y + std::sin(angle) * distance};
}
inline Point quadratic(Point a, Point control, Point b, float t) {
    const float oneMinus = 1.f - t;
    return {oneMinus * oneMinus * a.x + 2.f * oneMinus * t * control.x + t * t * b.x,
            oneMinus * oneMinus * a.y + 2.f * oneMinus * t * control.y + t * t * b.y};
}
inline void curvedLimb(Point a, Point control, Point b, float width, Color color, int segments = 7) {
    const int count = std::clamp(segments, 2, 10);
    Point previous = a;
    for (int i = 1; i <= count; ++i) {
        const Point next = quadratic(a, control, b, static_cast<float>(i) / count);
        DrawLineEx(vec(previous), vec(next), width, color);
        previous = next;
    }
}
inline Color ink(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return {r, g, b, a};
}
inline Color withAlpha(Color c, float alpha) {
    c.a = static_cast<unsigned char>(std::clamp(alpha, 0.f, 255.f));
    return c;
}
inline void dashed(Point a, Point b, float width, Color color, int dashes = 8) {
    const int count = std::clamp(dashes, 1, 16);
    for (int i = 0; i < count; ++i) {
        const float begin = static_cast<float>(i) / count;
        const float end = (static_cast<float>(i) + .58f) / count;
        DrawLineEx(vec(mix(a, b, begin)), vec(mix(a, b, end)), width, color);
    }
}
inline void crosshatch(Point a, Point b, Color color) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    const int bars = std::clamp(static_cast<int>(length / 24.f), 3, 14);
    const Point normal = length > .001f ? Point{-dy / length, dx / length} : Point{0, 1};
    for (int i = 0; i <= bars; ++i) {
        const Point p = mix(a, b, static_cast<float>(i) / bars);
        DrawLineEx(vec({p.x - normal.x * 5.f, p.y - normal.y * 5.f}),
                   vec({p.x + normal.x * 5.f, p.y + normal.y * 5.f}), 1.5f, color);
    }
}
inline void splitRing(Point center, float radius, float gapAngle, float rotation,
                      float halfGap, Color color) {
    const float gapDegrees = halfGap * 57.2957795f;
    const float centerDegrees = (gapAngle + rotation) * 57.2957795f;
    for (int side = 0; side < 2; ++side) {
        const float middle = centerDegrees + side * 180.f;
        const float start = middle + gapDegrees;
        const float end = middle + 180.f - gapDegrees;
        DrawRing(vec(center), radius - 1.2f, radius + 1.2f, start, end, 36, color);
        DrawRingLines(vec(center), radius - 1.2f, radius + 1.2f, start, end, 36,
                      withAlpha(color, static_cast<float>(color.a) * .8f));
    }
}

inline void drawWallState(const unweaver::barriers::State& state, float fade,
                          Color pale, Color mint, Color coral, Color gold) {
    using namespace unweaver::barriers;
    const int count = std::clamp(state.count, 0, kWallCount);
    for (int i = 0; i < count; ++i) {
        const Wall& wall = state.walls[static_cast<std::size_t>(i)];
        if (!unweaver::barriers::detail::live(wall)) continue;

        const float dx = wall.b.x - wall.a.x;
        const float dy = wall.b.y - wall.a.y;
        const float length = std::hypot(dx, dy);
        const Point normal = length > .001f ? Point{-dy / length, dx / length} : Point{0.f, 1.f};
        const float halfwidth = std::max(0.f, wall.halfwidth);

        if (wall.warning > 0) {
            const Color warning = withAlpha(gold, 225.f * fade);
            dashed(wall.a, wall.b, 2.f, warning, 12);
            const Point offsetA{normal.x * halfwidth, normal.y * halfwidth};
            const Point offsetB{-offsetA.x, -offsetA.y};
            dashed({wall.a.x + offsetA.x, wall.a.y + offsetA.y},
                   {wall.b.x + offsetA.x, wall.b.y + offsetA.y}, 1.2f,
                   withAlpha(pale, 165.f * fade), 12);
            dashed({wall.a.x + offsetB.x, wall.a.y + offsetB.y},
                   {wall.b.x + offsetB.x, wall.b.y + offsetB.y}, 1.2f,
                   withAlpha(pale, 165.f * fade), 12);
            crosshatch(wall.a, wall.b, withAlpha(gold, 190.f * fade));
            continue;
        }

        const Color edge = wall.spiked ? coral : mint;
        const bool broad = halfwidth > 24.f;
        const Color fill = withAlpha(wall.spiked ? coral : mint, (broad ? 6.f : 78.f) * fade);
        const Point offset{normal.x * halfwidth, normal.y * halfwidth};
        const Point aPlus{wall.a.x + offset.x, wall.a.y + offset.y};
        const Point bPlus{wall.b.x + offset.x, wall.b.y + offset.y};
        const Point aMinus{wall.a.x - offset.x, wall.a.y - offset.y};
        const Point bMinus{wall.b.x - offset.x, wall.b.y - offset.y};

        // The quad, centerline, and round caps match the collision capsule's
        // halfwidth. A bounded number of details keeps arena-wide walls cheap.
        DrawTriangle(vec(aPlus), vec(bPlus), vec(bMinus), fill);
        DrawTriangle(vec(aPlus), vec(bMinus), vec(aMinus), fill);
        DrawLineEx(vec(wall.a), vec(wall.b), std::max(1.f, halfwidth * 2.f),
                   withAlpha(edge, (broad ? 12.f : 210.f) * fade));
        DrawCircleV(vec(wall.a), halfwidth, fill);
        DrawCircleV(vec(wall.b), halfwidth, fill);
        DrawCircleLines(static_cast<int>(wall.a.x), static_cast<int>(wall.a.y), halfwidth,
                        withAlpha(pale, 190.f * fade));
        DrawCircleLines(static_cast<int>(wall.b.x), static_cast<int>(wall.b.y), halfwidth,
                        withAlpha(pale, 190.f * fade));

        const int details = std::clamp(static_cast<int>(length / 28.f), 1, 48);
        if (wall.spiked && length > .001f) {
            // Paired teeth signal traversable damage on both sides of the
            // capsule; their bases sit on its exact collision edge.
            for (int tooth = 0; tooth < details; ++tooth) {
                const Point center = mix(wall.a, wall.b,
                    (static_cast<float>(tooth) + .5f) / details);
                const float halfStep = length / details * .26f;
                for (float side : {-1.f, 1.f}) {
                    const Point baseA{center.x - dx / length * halfStep + normal.x * halfwidth * side,
                                      center.y - dy / length * halfStep + normal.y * halfwidth * side};
                    const Point baseB{center.x + dx / length * halfStep + normal.x * halfwidth * side,
                                      center.y + dy / length * halfStep + normal.y * halfwidth * side};
                    const float spikeReach = halfwidth + layout::kPlayerRadius;
                    const Point tip{center.x + normal.x * spikeReach * side,
                                    center.y + normal.y * spikeReach * side};
                    DrawTriangle(vec(baseA), vec(baseB), vec(tip), withAlpha(gold, 225.f * fade));
                }
            }
        } else if (!wall.spiked && length > .001f) {
            for (int mark = 0; mark < details; ++mark) {
                const Point center = mix(wall.a, wall.b,
                    (static_cast<float>(mark) + .5f) / details);
                DrawLineEx({center.x - normal.x * halfwidth * .55f,
                            center.y - normal.y * halfwidth * .55f},
                           {center.x + normal.x * halfwidth * .55f,
                            center.y + normal.y * halfwidth * .55f},
                           1.2f, withAlpha(pale, 145.f * fade));
            }
        }

        if (wall.fresh) {
            // Fresh activation is harmless for one update, shown as a bright
            // outline so the state change is readable without implying damage.
            DrawLineEx(vec(wall.a), vec(wall.b), std::max(2.f, halfwidth * 2.f + 2.f),
                       withAlpha(pale, 245.f * fade));
        }
    }
}

inline void drawBarriers(const unweaver::barriers::State& walls,
                         const unweaver::barriers::State& pressure, float fade,
                         Color pale, Color mint, Color coral, Color gold) {
    drawWallState(walls, fade, pale, mint, coral, gold);
    drawWallState(pressure, fade, pale, mint, coral, gold);
}

inline void drawEventLoom(const unweaver::State& u, float clock, float fade,
                          Color pale, Color mint, Color violet, Color gold) {
    constexpr int segments = 32;
    DrawPolyLinesEx(vec(u.anchor),8,112.f+4.f*std::sin(clock*2.f),clock*12.f,1.f,withAlpha(violet,60.f*fade));
    for (int lane = 0; lane < 8; ++lane) {
        const int rayIndex = lane;
        if (rayIndex >= u.rayCount) break;
        const auto& ray = u.rays[rayIndex];
        if (ray.curveTicks <= 0 || !ray.committed || ray.removed) continue;

        // Render the exact committed route. The four lanes are each stored
        // twice for their delayed pulses, so laneIndex is always 2 * lane.
        Point previous = ray.origin;
        const float dashPhase = std::fmod(clock * .21f + lane * .23f + 1.f, 1.f);
        for (int segment = 1; segment <= segments; ++segment) {
            const Point next = ranged::quadraticBezier(ray.origin, ray.control, ray.end,
                                                       static_cast<float>(segment) / segments);
            const float along = static_cast<float>(segment - 1) / segments;
            const auto tangent=unweaver::unit(unweaver::sub(next,previous));const Point normal{-tangent.y,tangent.x};
            const float braidA=std::sin(along*18.8495559f-clock*3.5f+lane)*11.f;
            const float braidB=std::sin(segment/float(segments)*18.8495559f-clock*3.5f+lane)*11.f;
            DrawLineEx(vec(previous),vec(next),15.f,withAlpha(violet,14.f*fade));
            for(int side:{-1,1})DrawLineEx(vec(unweaver::add(previous,unweaver::scale(normal,braidA*side))),vec(unweaver::add(next,unweaver::scale(normal,braidB*side))),1.6f,withAlpha(side<0?mint:violet,72.f*fade));
            if(segment%4==0)DrawLineEx(vec(unweaver::add(next,unweaver::scale(normal,braidB))),vec(unweaver::add(next,unweaver::scale(normal,-braidB))),1.f,withAlpha(gold,45.f*fade));
            if(segment%8==0)DrawPolyLinesEx(vec(next),4,5.f,clock*20.f+lane*45.f,1.f,withAlpha(mint,65.f*fade));
            const float moving = std::fmod(along - dashPhase + 1.f, 1.f);
            const bool warningGlow = moving < .19f || moving > .92f;
            const Color thread = warningGlow
                ? withAlpha(gold, (105.f + 85.f * (.5f + .5f * std::sin(clock * 5.f + lane))) * fade)
                : withAlpha((segment / 4 + lane) % 2 ? mint : violet, 52.f * fade);
            // One bounded stroke per segment: low-opacity lattice gaps stay
            // readable beneath a short, moving warning dash.
            DrawLineEx(vec(previous), vec(next), warningGlow ? 2.6f : 1.35f, thread);
            previous = next;
        }

        const Point eye = ray.origin;
        const float aperture = 8.f + 1.8f * std::sin(clock * 3.f + lane * 1.3f);
        DrawPolyLinesEx(vec(eye),6,23.f,clock*12.f+lane*60.f,1.5f,withAlpha(gold,150.f*fade));
        DrawCircleV(vec(eye), 18.f, withAlpha(ink(8, 15, 31, 220), 205.f * fade));
        DrawCircleLines(static_cast<int>(eye.x), static_cast<int>(eye.y), 14.f,
                        withAlpha(violet, 205.f * fade));
        DrawCircleLines(static_cast<int>(eye.x), static_cast<int>(eye.y), aperture,
                        withAlpha(gold, 205.f * fade));
        DrawEllipse(static_cast<int>(eye.x), static_cast<int>(eye.y), aperture * .62f,
                    aperture * .3f, withAlpha(mint, 215.f * fade));
        DrawCircleV(vec(eye), 2.3f, withAlpha(pale, 245.f * fade));
    }
}

template<class Boss>
void draw(const Boss& boss, float alpha, float time) {
    const auto& u = boss.unweaver;
    const float interpolation = std::clamp(std::isfinite(alpha) ? alpha : 1.f, 0.f, 1.f);
    const float clock = std::isfinite(time) ? time : 0.f;
    if (boss.stage == State::Defeated) return;

    const Point p = mix(boss.previousP, boss.p, interpolation);
    const bool transition = boss.stage == State::Transition;
    const float transitionFade = transition
        ? std::clamp(boss.remaining / static_cast<float>(std::max(1, boss.phase == 4 ? 180 : 108)), 0.f, 1.f)
        : 1.f;
    const float fade = transitionFade;
    const Color pale = ink(181, 244, 229, static_cast<unsigned char>(220 * fade));
    const Color mint = ink(82, 222, 198, static_cast<unsigned char>(205 * fade));
    const Color violet = ink(169, 130, 239, static_cast<unsigned char>(210 * fade));
    const Color coral = ink(255, 126, 151, static_cast<unsigned char>(220 * fade));
    const Color gold = ink(255, 217, 139, static_cast<unsigned char>(220 * fade));
    const Color dark = ink(12, 23, 35, static_cast<unsigned char>(245 * fade));

    // Barrier geometry shares the world-space collision capsules and stays
    // outside the boss-only scale transform below.
    if constexpr (requires { u.walls; u.pressure; }) {
        drawBarriers(u.walls, u.pressure, fade, pale, mint, coral, gold);
    }

    // Draw vulnerable geometry and the pending rays before the body.
    if (!transition && boss.stage != State::Recover) {
        const int rayCount = std::clamp(u.rayCount, 0, static_cast<int>(u.rays.size()));
        if (u.loom) drawEventLoom(u, clock, fade, pale, mint, violet, gold);
        const float arenaWidth = u.arenaBounds.right - u.arenaBounds.left;
        const float arenaHeight = u.arenaBounds.bottom - u.arenaBounds.top;
        const float fullArenaRayLength = std::hypot(arenaWidth, arenaHeight) + 80.f;
        for (int i = 0; i < rayCount; ++i) {
            const auto& ray = u.rays[i];
            if (!ray.ticket || ray.fireTick < u.clock || ray.fireTick > u.clock + u.warningTicks) continue;
            if (ray.curveTicks > 0) continue;
            const Point start = ray.origin;
            const Point end = rootwarden::rayEnd(start, ray.direction, fullArenaRayLength,
                                                 u.arenaBounds);
            const Color cue=u.focused?coral:u.attack==unweaver::Attack::RotatingEclipse?violet:u.attack==unweaver::Attack::FracturedRhythm?mint:gold;
            const float pulse=.6f+.4f*std::sin(clock*4.f-ray.fireTick*.025f);
            dashed(start, end, 1.8f, withAlpha(cue, static_cast<float>(cue.a) * (.6f+.22f*pulse)), u.attack==unweaver::Attack::FracturedRhythm?5:10);
            const Point marker = {start.x + ray.direction.x * 58.f,
                                  start.y + ray.direction.y * 58.f};
            DrawPoly(vec(marker), 3, 5.f, std::atan2(ray.direction.y, ray.direction.x) * 57.2957795f,
                     withAlpha(gold, static_cast<float>(gold.a) * .85f));
        }
        if(u.planCommitted&&boss.stage==State::RootWarning){
            const char* label=u.loom?"THE EVENT LOOM":u.focused?"ESCAPE FAN":u.unique?"WEAVE TRAP":unweaver::attackName(u.attack);
            DrawText(label,(int)boss.p.x-65,(int)boss.p.y+104,10,withAlpha(gold,220.f*fade));
            if(u.focused){dashed(u.anchor,u.aimTarget,2.f,withAlpha(coral,180.f*fade),8);DrawCircleLines((int)u.aimTarget.x,(int)u.aimTarget.y,12,coral);}
        }
        // Complete outlines accompany the full-circle committed rays.
        if (boss.phase >= 4 && u.planCommitted&&!u.focused&&!u.unique&&!u.loom) {
            const float rotation = clock * .72f;
            const Point ringCenter = u.anchor;
            for (int i = 0; i < (u.attack==unweaver::Attack::FinalConvergence?3:2); ++i) {
                const float pulse = .5f + .5f * std::sin(clock * 1.65f - i * .9f);
                const float radius = 116.f + i * 55.f + pulse * 15.f;
                const float gap = 0.f;
                float attackRotation = 0.f;
                if (u.attack == unweaver::Attack::RotatingEclipse) attackRotation = i * .52f;
                else if (u.attack == unweaver::Attack::PredictedEscape) attackRotation = -i * .27f;
                const float lockedGap = unweaver::angleOf(u.gap) + attackRotation;
                splitRing(ringCenter, radius, lockedGap, 0.f, gap,
                          withAlpha(i == 1 ? violet : mint, 95.f * fade));
                const float ornament = rotation * (i % 2 ? -1.f : 1.f) + i * .8f;
                for (int shard = 0; shard < 2; ++shard) {
                    const float a = ornament + shard * 3.14159265f;
                    const Point node = offset(ringCenter, a, radius);
                    DrawPoly(vec(node), 3, 4.f, a * 57.2957795f,
                             withAlpha(i == 1 ? gold : pale, 140.f * fade));
                }
            }

        }
    }

    // The recorded echo is dashed during warning and becomes a continuous path
    // only while its exact stored segments are active.
    if (!transition && u.echo.count > 1 && (u.echo.warning > 0 || u.echo.active > 0)) {
        const int count = std::clamp(u.echo.count, 0, static_cast<int>(u.echo.path.size()));
        for (int i = 1; i < count; ++i) {
            const Point a = u.echo.path[i - 1], b = u.echo.path[i];
            if (u.echo.warning > 0) dashed(a, b, 2.f, withAlpha(gold, 185.f * fade), 5);
            else {
                DrawLineEx(vec(a), vec(b), 13.f, withAlpha(violet, 34.f * fade));
                DrawLineEx(vec(a), vec(b), 2.6f, withAlpha(pale, 215.f * fade));
                DrawCircleV(vec(b), 3.f, withAlpha(gold, 205.f * fade));
            }
        }
    }

    // Strand collision remains a straight capsule of half-width seven. The
    // corruption teeth are an added visual warning over that exact centerline.
    for (const auto& strand : u.strands) {
        if (!strand.id || transition) continue;
        if (strand.warning > 0) {
            DrawLineEx(vec(strand.a), vec(strand.b), 2.f, withAlpha(gold, 205.f * fade));
            crosshatch(strand.a, strand.b, withAlpha(gold, 200.f * fade));
        } else if (strand.sever) {
            const Point middle = mix(strand.a, strand.b, .5f);
            dashed(strand.a, middle, 2.2f, withAlpha(mint, 230.f * fade), 3);
            dashed(middle, strand.b, 2.2f, withAlpha(mint, 230.f * fade), 3);
            DrawLineEx({middle.x - 6.f, middle.y - 6.f}, {middle.x + 6.f, middle.y + 6.f}, 2.f,
                       withAlpha(pale, 225.f * fade));
        } else if (strand.active > 0) {
            DrawLineEx(vec(strand.a), vec(strand.b), 14.f, withAlpha(coral, 30.f * fade));
            DrawLineEx(vec(strand.a), vec(strand.b), 7.f, withAlpha(coral, 105.f * fade));
            constexpr int teeth = 12;
            const float dx = strand.b.x - strand.a.x, dy = strand.b.y - strand.a.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            const Point normal = len > .001f ? Point{-dy / len, dx / len} : Point{0, 1};
            Point previous = strand.a;
            for (int i = 1; i <= teeth; ++i) {
                const float t = static_cast<float>(i) / teeth;
                const Point base = mix(strand.a, strand.b, t);
                const float sign = i % 2 ? 1.f : -1.f;
                const Point next{base.x + normal.x * sign * 4.f, base.y + normal.y * sign * 4.f};
                DrawLineEx(vec(previous), vec(next), 1.7f, withAlpha(gold, 210.f * fade));
                previous = next;
            }
        }
    }

    // Seal tethers and their sixteen-pixel contact radius follow the live buds.
    const int budCount = std::clamp(boss.budCount, 0, static_cast<int>(boss.buds.size()));
    for (int i = 0; i < budCount; ++i) {
        const auto& bud = boss.buds[i];
        if (!bud.health.alive()) continue;
        const bool active = boss.stage == State::RootActive && u.sealTicks > 0;
        const Color tether = active ? coral : gold;
        if (active) {
            DrawLineEx(vec(p), vec(bud.p), 32.f, withAlpha(coral, 22.f * fade));
            DrawLineEx(vec(p), vec(bud.p), 2.f, withAlpha(coral, 200.f * fade));
        } else {
            dashed(p, bud.p, 2.f, withAlpha(tether, 205.f * fade), 9);
            crosshatch(p, bud.p, withAlpha(tether, 125.f * fade));
        }
        DrawCircleV(vec(bud.p), 19.f, withAlpha(dark, fade * 240.f));
        DrawCircleLines(static_cast<int>(bud.p.x), static_cast<int>(bud.p.y), 16.f,
                        withAlpha(active ? coral : mint, 235.f * fade));
        DrawCircleV(vec(bud.p), 6.f, withAlpha(active ? gold : pale, 230.f * fade));
        DrawLineEx({bud.p.x - 5.f, bud.p.y}, {bud.p.x + 5.f, bud.p.y}, 1.4f,
                   withAlpha(tether, 220.f * fade));
    }

    // The Hunt keeps its locked lane readable: dashed while announced, thick
    // and continuous during the actual step.
    if (!transition && boss.stage == State::StepWarning) {
        dashed(boss.stepStart, boss.stepEnd, 3.f, withAlpha(gold, 225.f * fade), 12);
        DrawCircleLines(static_cast<int>(boss.stepEnd.x), static_cast<int>(boss.stepEnd.y),
                        18.f, withAlpha(gold, 225.f * fade));
        DrawPoly(vec(boss.stepEnd), 3, 8.f, std::atan2(boss.stepEnd.y - boss.stepStart.y,
                 boss.stepEnd.x - boss.stepStart.x) * 57.2957795f, withAlpha(gold, 230.f * fade));
    } else if (!transition && boss.stage == State::StepActive) {
        DrawLineEx(vec(boss.stepStart), vec(boss.stepEnd), 2*unweaver::kContactRadius, withAlpha(coral, 30.f * fade));
        DrawLineEx(vec(boss.stepStart), vec(boss.stepEnd), 3.f, withAlpha(coral, 225.f * fade));
        DrawCircleLines(static_cast<int>(boss.stepEnd.x), static_cast<int>(boss.stepEnd.y),
                        unweaver::kContactRadius, withAlpha(coral, 220.f * fade));
    }

    // Collision radii are quiet guides behind the body art; neither ring is
    // used as the creature's silhouette.
    rlPushMatrix();
    rlTranslatef(p.x, p.y, 0.f);
    rlScalef(.88f, .88f, 1.f);
    rlTranslatef(-p.x, -p.y, 0.f);
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 76.f,
                    withAlpha(gold, 48.f * fade));
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 32.f,
                    withAlpha(coral, 76.f * fade));

    // Distinct silhouette language for each learned style.
    const float breathe = 1.f + .025f * std::sin(clock * 2.3f);
    const bool opened = boss.phase >= 4 && !transition && u.planCommitted;
    const float radius = (opened ? 45.f : 49.f) * breathe;
    Color edge = u.style == unweaver::Style::Predator ? coral
               : u.style == unweaver::Style::Architect ? mint : violet;
    const int limbs = u.style == unweaver::Style::Predator ? 6
                    : u.style == unweaver::Style::Architect ? 4 : 5;
    for (int i = 0; i < limbs; ++i) {
        const float orbit = clock * (u.style == unweaver::Style::Puppeteer ? -.11f : .055f);
        const float angle = orbit + i * 6.28318530718f / limbs;
        const float reach = (opened ? 111.f : u.style == unweaver::Style::Architect ? 72.f
                          : u.style == unweaver::Style::Predator ? 82.f : 91.f);
        const float sweep = (i % 2 ? .30f : -.30f) + (opened ? (i % 2 ? .25f : -.25f) : 0.f);
        const Point shoulder = offset(p, angle, radius * .67f);
        const Point elbow = offset(p, angle + sweep, reach * .61f);
        const Point tip = offset(p, angle + sweep * 1.5f, reach);
        const Point firstControl = offset(p, angle + sweep * .18f, radius * 1.29f);
        const Point secondControl = offset(p, angle + sweep * 1.26f, reach * .87f);
        const Color flesh = u.style == unweaver::Style::Puppeteer ? violet
                          : u.style == unweaver::Style::Predator ? ink(121, 55, 75, 245)
                          : ink(46, 126, 124, 245);
        curvedLimb(shoulder, firstControl, elbow, 11.f, withAlpha(dark, 245.f * fade));
        curvedLimb(shoulder, firstControl, elbow, 5.f, withAlpha(flesh, 245.f * fade));
        curvedLimb(elbow, secondControl, tip, 6.f, withAlpha(dark, 235.f * fade));
        curvedLimb(elbow, secondControl, tip, 2.1f, withAlpha(edge, 235.f * fade));
        DrawCircleV(vec(elbow), 6.f, withAlpha(ink(24, 31, 42, 250), fade * 250.f));
        DrawCircleLines(static_cast<int>(elbow.x), static_cast<int>(elbow.y), 5.f,
                        withAlpha(gold, 215.f * fade));

        // A paired inner filament gives each limb a layered, tendon-like fork.
        const Point pairRoot = offset(p, angle + .13f, radius * .46f);
        const Point pairTip = offset(p, angle + sweep * 1.35f + (i % 2 ? .16f : -.16f), reach * .82f);
        const Point pairControl = offset(p, angle + sweep * .78f + .10f, reach * .57f);
        curvedLimb(pairRoot, pairControl, pairTip, 2.5f, withAlpha(pale, 190.f * fade), 6);
        const Point plate = offset(shoulder, angle + sweep, 15.f);
        DrawPoly(vec(plate), 4, 8.f, angle * 57.2957795f,
                 withAlpha(u.style == unweaver::Style::Architect ? mint : flesh, 230.f * fade));
        DrawLineEx(vec(plate), vec(shoulder), 1.5f, withAlpha(pale, 160.f * fade));
        if (u.style == unweaver::Style::Predator) {
            const Point clawA = offset(tip, angle + .72f, 15.f);
            const Point clawB = offset(tip, angle - .54f, 12.f);
            curvedLimb(elbow, secondControl, clawA, 2.f, withAlpha(gold, 225.f * fade), 5);
            DrawLineEx(vec(tip), vec(clawB), 1.8f, withAlpha(gold, 220.f * fade));
        } else if (u.style == unweaver::Style::Architect) {
            DrawPoly(vec(tip), 3, 7.f, angle * 57.2957795f, withAlpha(mint, 220.f * fade));
        } else {
            DrawCircleLines(static_cast<int>(tip.x), static_cast<int>(tip.y), 5.f,
                            withAlpha(violet, 225.f * fade));
        }
    }

    DrawCircleV(vec(p), radius + 13.f, withAlpha(edge, 16.f * fade));
    DrawCircleV(vec(p), radius, withAlpha(ink(10, 21, 32, 250), fade * 250.f));
    // Overlapping chitin plates form an armored mantle. Phase four parts those
    // plates into petals, exposing the central eye and luminous inner chamber.
    const int plates = u.style == unweaver::Style::Architect ? 8 : 9;
    for (int i = 0; i < plates; ++i) {
        const float angle = i * 6.28318530718f / plates + clock * .025f;
        const float plateRadius = opened ? radius * .73f : radius * .61f;
        const Point plateCenter = offset(p, angle, plateRadius);
        const Color plateColor = i % 3 == 0 ? ink(36, 74, 79, 255)
                              : i % 3 == 1 ? ink(29, 46, 61, 255) : ink(55, 39, 61, 255);
        DrawPoly(vec(plateCenter), 4, opened ? 15.f : 17.f, angle * 57.2957795f + 45.f,
                 withAlpha(plateColor, fade * 250.f));
        DrawLineEx(vec(p), vec(plateCenter), 1.3f, withAlpha(edge, 130.f * fade));
        const Point barbA = offset(plateCenter, angle + 2.55f, 7.f);
        const Point barbB = offset(plateCenter, angle - 2.55f, 7.f);
        DrawLineEx(vec(barbA), vec(plateCenter), 2.f, withAlpha(gold, 175.f * fade));
        DrawLineEx(vec(plateCenter), vec(barbB), 2.f, withAlpha(gold, 175.f * fade));
    }
    DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius * .91f,
                    withAlpha(edge, 175.f * fade));
    if (u.style == unweaver::Style::Predator) {
        DrawPoly(vec(p), 8, radius * .48f, clock * 17.f,
                 withAlpha(ink(43, 24, 43, 250), fade * 250.f));
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 2.f), opened ? 16.f : 21.f,
                    opened ? 18.f : 11.f, withAlpha(opened ? gold : coral, 245.f * fade));
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 2.f), 7.f, 9.f,
                    withAlpha(ink(8, 15, 25, 255), fade * 255.f));
        DrawCircleV({p.x - 2.f, p.y - 6.f}, 2.5f, withAlpha(pale, 255.f * fade));
        DrawLineEx({p.x - 29.f, p.y - 16.f}, {p.x - 17.f, p.y - 10.f}, 4.f,
                   withAlpha(ink(85, 32, 58, 255), fade * 250.f));
        DrawLineEx({p.x + 29.f, p.y - 16.f}, {p.x + 17.f, p.y - 10.f}, 4.f,
                   withAlpha(ink(85, 32, 58, 255), fade * 250.f));
        if (opened) {
            for (int i = 0; i < 4; ++i) {
                const float a = i * 1.5707963f + clock * .2f;
                DrawLineEx(vec(offset(p, a, 15.f)), vec(offset(p, a, 28.f)), 1.7f,
                           withAlpha(pale, 225.f * fade));
            }
        }
    } else if (u.style == unweaver::Style::Architect) {
        DrawPoly(vec(p), opened ? 8 : 6, radius * .48f, clock * -8.f,
                 withAlpha(ink(18, 48, 55, 250), fade * 250.f));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius * .39f,
                        withAlpha(mint, 210.f * fade));
        for (int i = 0; i < (opened ? 8 : 4); ++i) {
            const Point node = offset(p, i * 6.2831853f / (opened ? 8.f : 4.f) + .785f,
                                      radius * (opened ? .56f : .34f));
            DrawPoly(vec(node), 4, 5.f, 45.f, withAlpha(gold, 235.f * fade));
        }
        DrawCircleV(vec(p), opened ? 11.f : 7.f, withAlpha(pale, 245.f * fade));
        DrawCircleV(vec(p), 3.f, withAlpha(ink(8, 18, 28, 255), fade * 255.f));
    } else {
        DrawPoly(vec(p), opened ? 7 : 5, radius * .48f, clock * 10.f,
                 withAlpha(ink(37, 25, 60, 250), fade * 250.f));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius * .36f,
                        withAlpha(violet, 215.f * fade));
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 3.f), opened ? 14.f : 8.f,
                    opened ? 17.f : 10.f, withAlpha(opened ? gold : violet, 245.f * fade));
        DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y - 3.f), 5.f, 8.f,
                    withAlpha(ink(10, 12, 28, 255), fade * 255.f));
        DrawCircleV({p.x - 2.f, p.y - 7.f}, 2.f, withAlpha(pale, 255.f * fade));
        DrawLineEx({p.x - 16.f, p.y + 12.f}, {p.x + 16.f, p.y + 12.f}, 1.5f,
                   withAlpha(violet, 235.f * fade));
    }

    // Fragments remain attached to the mantle by short silk hinges; only three
    // are allowed to float beyond the body, avoiding a noisy particle halo.
    for (int i = 0; i < 4; ++i) {
        const float angle = i * 1.5707963f + clock * .035f;
        const Point plate = offset(p, angle + .785f, radius * .74f);
        const Point chip = offset(plate, angle + .22f, 12.f + (i % 2) * 3.f);
        DrawLineEx(vec(plate), vec(chip), 2.f, withAlpha(pale, 190.f * fade));
        DrawPoly(vec(chip), 3, 5.5f, angle * 57.2957795f,
                 withAlpha(i % 2 ? gold : edge, 235.f * fade));
    }
    for (int i = 0; i < 3; ++i) {
        const float angle = clock * (.16f + i * .025f) + i * 2.0943951f;
        const float orbit = 96.f + 5.f * std::sin(clock * .8f + i);
        const Point shard = offset(p, angle, orbit);
        const Point tether = offset(p, angle, orbit - 7.f);
        DrawLineEx(vec(tether), vec(shard), 1.f, withAlpha(edge, 43.f * fade));
        DrawPoly(vec(shard), 3, 4.5f, angle * 57.2957795f,
                 withAlpha(i == 1 ? gold : pale, 175.f * fade));
    }

    rlPopMatrix();

    for (const auto& projectile : boss.bullets.items()) {
        if (projectile.state != ranged::SlotState::Flying) continue;
        const Point shot = mix(projectile.previousP, projectile.p, interpolation);
        const float shotRadius = std::clamp(projectile.spec.radius, 1.f, 12.f);
        if (projectile.curved && projectile.curveTicks > 0) {
            const float t = std::clamp((projectile.curveAge - 1.f + interpolation) /
                                       static_cast<float>(projectile.curveTicks), 0.f, 1.f);
            Point tail = ranged::quadraticBezier(projectile.curveOrigin, projectile.curveControl,
                                                  projectile.curveEnd,
                                                  std::max(0.f, t - 8.f / projectile.curveTicks));
            for (int segment = 1; segment <= 4; ++segment) {
                const float trailT = std::max(0.f, t - 8.f / projectile.curveTicks +
                                                   (8.f / projectile.curveTicks) * segment / 4.f);
                const Point next = ranged::quadraticBezier(projectile.curveOrigin,
                    projectile.curveControl, projectile.curveEnd, trailT);
                const Color lattice = segment % 2 ? mint : violet;
                DrawLineEx(vec(tail), vec(next), segment == 4 ? 2.3f : 1.2f,
                           withAlpha(lattice, (segment == 4 ? 150.f : 64.f) * fade));
                tail = next;
            }
            // The ribbon is a quiet route cue; only this bright 9 px knot is
            // the actual swept collision head of an Event Loom pulse.
            DrawCircleV(vec(shot), 14.f, withAlpha(violet, 34.f * fade));
            DrawCircleV(vec(shot), 9.f, withAlpha(ink(16, 23, 43, 225), 230.f * fade));
            DrawCircleLines(static_cast<int>(shot.x), static_cast<int>(shot.y), 9.f,
                            withAlpha(gold, 230.f * fade));
            DrawCircleLines(static_cast<int>(shot.x), static_cast<int>(shot.y), 5.4f,
                            withAlpha(mint, 240.f * fade));
            DrawPoly(vec(shot), 4, 4.4f, clock * 42.f,
                     withAlpha(pale, 250.f * fade));
            DrawCircleV(vec(shot), 1.8f, withAlpha(gold, 255.f * fade));
        } else {
            DrawLineEx(vec(projectile.previousP), vec(shot), shotRadius * 1.5f,
                       withAlpha(coral, 72.f * fade));
            DrawPoly(vec(shot), 4, shotRadius, clock * 18.f, withAlpha(coral, 245.f * fade));
            DrawCircleV(vec(shot), std::max(1.5f, shotRadius * .35f), withAlpha(pale, 235.f * fade));
        }
    }

    if (transition) {
        const float ring = 54.f + (1.f - transitionFade) * 104.f;
        splitRing(p, ring, unweaver::angleOf(u.gap), clock * .3f, 0.f,
                  withAlpha(pale, 125.f * fade));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius + 18.f,
                        withAlpha(gold, 150.f * fade));
    }
}

} // namespace threadlight::unweaver_render
