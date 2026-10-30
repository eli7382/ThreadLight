#pragma once

#include "combat.h"
#include "wave_foundation.h"
#include <array>
#include <algorithm>
#include <cstdint>

namespace threadlight::chapter {

constexpr int kEncounterCount = 35;
constexpr float kPi = 3.14159265359f;
struct Encounter { const char* name; int seconds; bool boss; int chapter=1; };
inline Encounter encounter(int index) {
    constexpr Encounter definitions[] = {{"FIRST THREAD",20,false},{"HEAVY ENDS",22,false},
        {"FOUR CORNERS",24,false},{"SIDE STITCH",24,false},{"CROSSGRAIN",26,false},
        {"THREE BEATS",28,false},{"THE ROOTWARDEN",0,true},
        {"TWO NEEDLES",26,false,2},{"TURNING WATCH",28,false,2},
        {"SLOW CARAVAN",26,false,2},{"BURST WINDOWS",30,false,2},
        {"RETURN ROUTE",28,false,2},{"COMMITMENT TEST",32,false,2},
        {"THE NEEDLE DANCER",0,true,2},
        {"FIRST CALL",28,false,3},{"FIRING RELAY",30,false,3},
        {"MOVING ANCHORS",30,false,3},{"LOOSE WEFT",32,false,3},
        {"GATHERING TIDE",30,false,3},{"CALL AND RESPONSE",34,false,3},
        {"THE THORN CONDUCTOR",0,true,3},
        {"FIRST THORNS",30,false,4},{"EXPIRING PATHS",32,false,4},
        {"PATIENT CUT",30,false,4},{"CROSSFIRE GARDEN",34,false,4},
        {"OPEN SPRINT",30,false,4},{"CHANGING GAPS",34,false,4},
        {"THE HOLLOW WEAVER",0,true,4},
        {"NEW PATTERN",30,false,5},{"TENSION",32,false,5},
        {"ROTATING FIRE",34,false,5},{"NEEDLE RUSH",30,false,5},
        {"REVERSAL",32,false,5},{"CIRCUIT REHEARSAL",34,false,5},
        {"THE UNWEAVER",0,true,5}};
    return definitions[std::clamp(index,0,kEncounterCount-1)];
}
struct Spawn { int tick; combat::EnemyKind kind; float angle; };
struct Schedule { std::array<Spawn,64> items{}; int count=0; };

// Fixed budgets and deliberate entry packets. Seed changes a small entry-angle
// offset, never composition, deadlines, or warnings. No cosmetic RNG dependency.
inline Schedule schedule(int wave, std::uint32_t seed) {
    Schedule result;
    std::uint32_t random=seed^(0x85ebca6bu*static_cast<std::uint32_t>(wave+1));
    auto add=[&](int tick,combat::EnemyKind kind,float angle){
        random=random*1664525u+1013904223u;
        const float jitter=(static_cast<int>((random>>16)%101)-50)*.001f;
        result.items[result.count++]={tick,kind,angle+jitter};
    };
    using K=combat::EnemyKind;
    switch(wave) {
    case 0:
        for(int i=0;i<12;++i)add(60+i*180,K::Hunter,i%2?kPi:0.f);
        break;
    case 1:
        for(int stream=0;stream<2;++stream){
            const int base=stream*1320;const float angle=stream?kPi:0.f;
            add(base,K::Brute,angle);
            for(int i=0;i<7;++i)add(base+120+i*168,K::Hunter,angle+(i%2?.12f:-.12f));
        }
        break;
    case 2:
        for(int packet=0;packet<4;++packet){
            const float angle=kPi*.25f+packet*kPi*.5f;
            add(packet*720,K::Brute,angle);
            for(int i=0;i<3;++i)add(packet*720+42+i*42,K::Hunter,angle+(i-1)*.10f);
        }
        break;
    case 3:
        for(int i=0;i<12;++i)add(i*240,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<3;++i)add(480+i*840,K::Brute,i%2?kPi:0.f);
        for(int i=0;i<5;++i)add(360+i*480,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 4:
        for(int packet=0;packet<3;++packet)for(int side=0;side<2;++side)
            for(int i=0;i<2;++i)add(packet*960+i*54,K::Hunter,(side?kPi:0.f)+(i?-.12f:.12f));
        for(int i=0;i<4;++i)add(120+i*720,K::Brute,i%2?kPi:0.f);
        for(int i=0;i<6;++i)add(360+i*468,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 5:
        for(int i=0;i<12;++i)add(i*90,K::Hunter,(i%3-1)*kPi*.5f);
        for(int i=0;i<7;++i)add(1080+i*240,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<5;++i)add(2400+i*180,K::Brute,i%2?kPi:0.f);
        break;
    case 7:
        // A simple pursuit tide with two widely separated stationary firing origins.
        for(int i=0;i<18;++i)add(i*156,K::Hunter,i%2?kPi:0.f);
        add(240,K::Brute,0.f);add(1680,K::Brute,kPi);
        for(int i=0;i<4;++i)add(600+i*600,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        add(480,K::Spitter,-kPi*.5f);add(1440,K::Spitter,kPi*.5f);
        break;
    case 8:
        // Four sector relays. Each packet has two ranged positions, two flankers,
        // and two pursuers; stagger entries instead of a simultaneous crossfire.
        for(int sector=0;sector<4;++sector){
            const int base=sector*780;const float direction=sector*kPi*.5f;
            add(base,K::Spitter,direction-.20f);add(base+240,K::Spitter,direction+.20f);
            add(base+60,K::Hunter,direction+kPi);add(base+180,K::Hunter,direction+kPi);
            add(base+300,K::Skitter,direction+kPi*.5f);add(base+420,K::Skitter,direction-kPi*.5f);
            if(sector%2==0)add(base+480,K::Brute,direction);
        }
        break;
    case 9: // Slow Caravan: steady pursuers shelter a slow-moving brute wall.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?0.f:kPi);
        for(int i=0;i<12;++i)add(i*240,K::Brute,0.f);
        for(int i=0;i<2;++i)add(720+i*1440,K::Skitter,i?kPi:0.f);
        for(int i=0;i<2;++i)add(1080+i*960,K::Spitter,i?kPi*.5f:-kPi*.5f);
        break;
    case 10: // Burst Windows: narrow skitter rushes punctuate ranged crossfire.
        for(int i=0;i<6;++i)add(120+i*600,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(600+i*1800,K::Brute,i?kPi:0.f);
        for(int packet=0;packet<4;++packet)for(int i=0;i<5;++i)
            add(packet*840+i*48,K::Skitter,(packet%2?kPi:0.f)+(i%2?.16f:-.16f));
        for(int i=0;i<2;++i)add(360+i*2040,K::Spitter,i?kPi*.5f:-kPi*.5f);
        break;
    case 11: // Return Route: alternating outer lanes fold back toward the arena.
        for(int i=0;i<18;++i)add(i*180,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(360+i*840,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(240+i*900,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 12: // Commitment Test: measured mixed packets with room to reset lanes.
        for(int i=0;i<8;++i)add(i*96,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<8;++i)add(900+i*120,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<10;++i)add(1980+i*132,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<6;++i)add(2520+i*144,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 14: // First Call: paired Briar Callers enter the chapter-three schedule.
        for(int i=0;i<16;++i)add(i*192,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(240+i*840,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(480+i*420,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(360+i*840,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(960+i*1200,K::BriarCaller,i?kPi:0.f);
        break;
    case 15: // Firing Relay: ranged positions pass the firing lane across sectors.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi:0.f);
        for(int i=0;i<6;++i)add(240+i*540,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int relay=0;relay<3;++relay)for(int i=0;i<4;++i)
            add(240+relay*900+i*120,K::Spitter,relay*kPi*2.f/3.f+(i%2?.12f:-.12f));
        for(int i=0;i<2;++i)add(1080+i*1320,K::BriarCaller,i?kPi*.5f:-kPi*.5f);
        break;
    case 16: // Moving Anchors: slow plates advance between separated brute beats.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<9;++i)add(180+i*360,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(840+i*1440,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(360+i*480,K::BriarCaller,i%2?kPi:0.f);
        break;
    case 17: // Loose Weft: a long skitter weave leaves gaps between its crossings.
        for(int i=0;i<6;++i)add(i*480,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi:0.f);
        for(int packet=0;packet<3;++packet)for(int i=0;i<6;++i)
            add(180+packet*660+i*72,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(1080+i*1320,K::BriarCaller,i?kPi:0.f);
        break;
    case 18: // Gathering Tide: pursuit lanes thicken, then settle into a final gap.
        for(int i=0;i<21;++i)add(i*96,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<6;++i)add(300+i*540,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(600+i*480,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(960+i*1320,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(1440+i*1080,K::BriarCaller,i?kPi:0.f);
        break;
    case 19: // Call and Response: spaced heavy calls answer paired moving/ranged lines.
        for(int act=0;act<3;++act) {
            const int base=act*1200;
            const float direction=act%2?kPi:0.f;
            const float flank=act%2?-kPi*.5f:kPi*.5f;
            const int hunters=act==0?4:3;
            const int skitters=act<2?3:2;
            const int callers=act<2?2:1;
            for(int i=0;i<hunters;++i)add(base+i*150,K::Hunter,direction);
            add(base+180,K::Brute,flank);add(base+780,K::Brute,-flank);
            for(int i=0;i<skitters;++i)add(base+360+i*180,K::Skitter,direction+kPi);
            add(base+600,K::Spitter,flank);add(base+960,K::Spitter,-flank);
            for(int i=0;i<callers;++i)add(base+900+i*180,K::BriarCaller,direction);
        }
        break;
    case 21: // First Thorns: paced sowers and moving threats introduce the new lane.
        for(int i=0;i<12;++i)add(i*240,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(360+i*720,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(600+i*420,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(480+i*840,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(840+i*1440,K::BriarCaller,i?kPi:0.f);
        for(int i=0;i<3;++i)add(120+i*1200,K::Sower,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 22: // Expiring Paths: spread sowers so every early route can clear.
        for(int i=0;i<10;++i)add(120+i*300,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1800,K::Brute,i?kPi:0.f);
        for(int i=0;i<6;++i)add(240+i*420,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(960+i*1680,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(1320+i*1200,K::BriarCaller,i?kPi:0.f);
        for(int i=0;i<8;++i)add(i*480,K::Sower,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 23: // Patient Cut: a measured brute train advances through wide gaps.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<10;++i)add(240+i*240,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(960+i*1440,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(480+i*480,K::BriarCaller,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1440,K::Sower,i?kPi*.5f:-kPi*.5f);
        break;
    case 24: // Crossfire Garden: three ranged relays answer from separated sectors.
        for(int i=0;i<10;++i)add(i*300,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(600+i*1800,K::Brute,i?kPi:0.f);
        for(int i=0;i<6;++i)add(240+i*480,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int relay=0;relay<3;++relay)for(int i=0;i<3;++i)
            add(180+relay*900+i*120,K::Spitter,relay*kPi*2.f/3.f+(i%2?.14f:-.14f));
        for(int i=0;i<2;++i)add(960+i*1320,K::BriarCaller,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(360+i*720,K::Sower,i%2?kPi*.5f:-kPi*.5f);
        break;
    case 25: // Open Sprint: short pursuer bursts leave the center lane moving.
        for(int i=0;i<14;++i)add(i*96,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(300+i*600,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int packet=0;packet<4;++packet)for(int i=0;i<4;++i)
            add(240+packet*540+i*72,K::Skitter,(packet%2?kPi:0.f)+(i%2?.12f:-.12f));
        for(int i=0;i<2;++i)add(600+i*1200,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(960+i*1200,K::BriarCaller,i?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1200,K::Sower,i?kPi*.5f:-kPi*.5f);
        break;
    case 26: // Changing Gaps: three acts reverse the safe approach lanes.
        for(int act=0;act<3;++act) {
            const int base=act*1320;
            const float direction=act%2?kPi:0.f;
            const float flank=act%2?-kPi*.5f:kPi*.5f;
            const int hunters=act==0?5:4,skitters=act<2?3:2,sowers=act==0?2:1;
            for(int i=0;i<hunters;++i)add(base+i*150,K::Hunter,direction);
            for(int i=0;i<2;++i)add(base+180+i*600,K::Brute,flank);
            for(int i=0;i<skitters;++i)add(base+360+i*180,K::Skitter,-direction);
            for(int i=0;i<2;++i)add(base+510+i*360,K::Spitter,flank);
            if(act<2)add(base+960,K::BriarCaller,direction);
            for(int i=0;i<sowers;++i)add(base+720+i*360,K::Sower,-flank);
        }
        // The third act closes with a delayed Briar Caller reinforcement.
        add(3480,K::BriarCaller,kPi);
        break;
    case 28: // New Pattern: a diagonal pulse replaces the former chapter rhythm.
        for(int i=0;i<20;++i)add(i*132,K::Hunter,(i%2?kPi:0.f)+kPi*.25f);
        for(int i=0;i<4;++i)add(360+i*720,K::Brute,i%2?kPi*.75f:kPi*.25f);
        for(int packet=0;packet<3;++packet)for(int i=0;i<4;++i)
            add(240+packet*840+i*96,K::Skitter,(packet%2?kPi*.75f:kPi*.25f)+(i%2?.1f:-.1f));
        for(int i=0;i<2;++i)add(1080+i*1440,K::Spitter,i?kPi*.75f:kPi*.25f);
        for(int i=0;i<2;++i)add(840+i*1440,K::Sower,i?kPi*.75f:kPi*.25f);
        break;
    case 29: // Tension: long heavy beats break into brief pursuit releases.
        for(int i=0;i<6;++i)add(i*420,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<12;++i)add(240+i*240,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*600,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(960+i*1320,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(360+i*480,K::BriarCaller,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(1320+i*1200,K::Sower,i?kPi*.5f:-kPi*.5f);
        break;
    case 30: // Rotating Fire: five ranged stations hand off their firing sector.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi:0.f);
        for(int i=0;i<8;++i)add(300+i*360,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int sector=0;sector<5;++sector)for(int i=0;i<2;++i)
            add(240+sector*600+i*144,K::Spitter,sector*kPi*2.f/5.f+(i?-.12f:.12f));
        for(int i=0;i<2;++i)add(960+i*1440,K::BriarCaller,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(480+i*840,K::Sower,i%2?kPi:0.f);
        break;
    case 31: // Needle Rush: four skitter packets arrive without ranged fire.
        for(int i=0;i<8;++i)add(i*300,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi*.5f:-kPi*.5f);
        for(int packet=0;packet<4;++packet) {
            const int packetSize=packet<2?6:5;
            for(int i=0;i<packetSize;++i)
                add(120+packet*720+i*72,K::Skitter,packet%2?kPi*.5f:-kPi*.5f);
        }
        for(int i=0;i<2;++i)add(960+i*1440,K::BriarCaller,i?kPi:0.f);
        for(int i=0;i<2;++i)add(600+i*1560,K::Sower,i?kPi*.5f:-kPi*.5f);
        break;
    case 32: // Reversal: each later act approaches from the opposite side.
        for(int act=0;act<3;++act) {
            const int base=act*1200;
            const float direction=act%2?kPi:0.f;
            const float flank=act%2?-kPi*.5f:kPi*.5f;
            const int brutes=act==0?4:3,skitters=act==0?4:3;
            for(int i=0;i<4;++i)add(base+i*120,K::Hunter,direction);
            for(int i=0;i<brutes;++i)add(base+180+i*180,K::Brute,flank);
            for(int i=0;i<skitters;++i)add(base+420+i*144,K::Skitter,-direction);
            if(act==0)add(base+780,K::Spitter,flank);
            else if(act==1)add(base+780,K::Spitter,flank);
            else {add(base+660,K::Spitter,flank);add(base+900,K::Spitter,-flank);}
            if(act<2)add(base+960,K::BriarCaller,direction);
            if(act!=1)add(base+840,K::Sower,-flank);
        }
        break;
    case 33: // Circuit Rehearsal: three mixed acts cycle the chapter's patterns.
        for(int act=0;act<3;++act) {
            const int base=act*1320;
            const float direction=act%2?kPi:0.f;
            const float flank=act%2?-kPi*.5f:kPi*.5f;
            const int brutes=act==0?4:3,skitters=act<2?3:2,sowers=act==0?2:1;
            for(int i=0;i<3;++i)add(base+i*144,K::Hunter,direction);
            for(int i=0;i<brutes;++i)add(base+180+i*180,K::Brute,flank);
            for(int i=0;i<skitters;++i)add(base+420+i*180,K::Skitter,-direction);
            for(int i=0;i<2;++i)add(base+360+i*600,K::Spitter,flank);
            if(act<2)add(base+960,K::BriarCaller,direction);
            for(int i=0;i<sowers;++i)add(base+720+i*360,K::Sower,-flank);
        }
        // The final act closes with a delayed Briar Caller reinforcement.
        add(3480,K::BriarCaller,kPi);
        break;
    default: break; // Boss supports are encounter events, never scheduled farming.
    }
    std::stable_sort(result.items.begin(),result.items.begin()+result.count,
                     [](const Spawn& a,const Spawn& b){return a.tick<b.tick;});
    return result;
}

} // namespace threadlight::chapter
