#pragma once

#include "combat.h"
#include "wave_foundation.h"
#include <array>
#include <algorithm>
#include <cstdint>

namespace threadlight::chapter {

constexpr int kEncounterCount = 21;
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
        {"OPENING PLATES",28,false,3},{"FIRING RELAY",30,false,3},
        {"MOVING ANCHORS",30,false,3},{"LOOSE WEFT",32,false,3},
        {"GATHERING TIDE",30,false,3},{"CALL AND RESPONSE",34,false,3},
        {"THE THORN CONDUCTOR",0,true,3}};
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
    case 14: // Opening Plates: plated anchors mark the first chapter-three lanes.
        for(int i=0;i<16;++i)add(i*192,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(240+i*840,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(480+i*420,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<4;++i)add(360+i*840,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(960+i*1200,K::Bulwark,i?kPi:0.f);
        break;
    case 15: // Firing Relay: ranged positions pass the firing lane across sectors.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi:0.f);
        for(int i=0;i<6;++i)add(240+i*540,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int relay=0;relay<3;++relay)for(int i=0;i<4;++i)
            add(240+relay*900+i*120,K::Spitter,relay*kPi*2.f/3.f+(i%2?.12f:-.12f));
        for(int i=0;i<2;++i)add(1080+i*1320,K::Bulwark,i?kPi*.5f:-kPi*.5f);
        break;
    case 16: // Moving Anchors: slow plates advance between separated brute beats.
        for(int i=0;i<8;++i)add(i*360,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<9;++i)add(180+i*360,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(840+i*1440,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(360+i*480,K::Bulwark,i%2?kPi:0.f);
        break;
    case 17: // Loose Weft: a long skitter weave leaves gaps between its crossings.
        for(int i=0;i<6;++i)add(i*480,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(720+i*1680,K::Brute,i?kPi:0.f);
        for(int packet=0;packet<3;++packet)for(int i=0;i<6;++i)
            add(180+packet*660+i*72,K::Skitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<4;++i)add(600+i*720,K::Spitter,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(1080+i*1320,K::Bulwark,i?kPi:0.f);
        break;
    case 18: // Gathering Tide: pursuit lanes thicken, then settle into a final gap.
        for(int i=0;i<21;++i)add(i*96,K::Hunter,i%2?kPi:0.f);
        for(int i=0;i<6;++i)add(300+i*540,K::Brute,i%2?kPi*.5f:-kPi*.5f);
        for(int i=0;i<6;++i)add(600+i*480,K::Skitter,i%2?kPi:0.f);
        for(int i=0;i<2;++i)add(960+i*1320,K::Spitter,i?kPi*.5f:-kPi*.5f);
        for(int i=0;i<2;++i)add(1440+i*1080,K::Bulwark,i?kPi:0.f);
        break;
    case 19: // Call and Response: spaced heavy calls answer paired moving/ranged lines.
        for(int act=0;act<3;++act) {
            const int base=act*1200;
            const float direction=act%2?kPi:0.f;
            const float flank=act%2?-kPi*.5f:kPi*.5f;
            const int hunters=act==0?4:3;
            const int skitters=act<2?3:2;
            const int bulwarks=act<2?2:1;
            for(int i=0;i<hunters;++i)add(base+i*150,K::Hunter,direction);
            add(base+180,K::Brute,flank);add(base+780,K::Brute,-flank);
            for(int i=0;i<skitters;++i)add(base+360+i*180,K::Skitter,direction+kPi);
            add(base+600,K::Spitter,flank);add(base+960,K::Spitter,-flank);
            for(int i=0;i<bulwarks;++i)add(base+900+i*180,K::Bulwark,direction);
        }
        break;
    default: break; // Boss supports are encounter events, never scheduled farming.
    }
    std::stable_sort(result.items.begin(),result.items.begin()+result.count,
                     [](const Spawn& a,const Spawn& b){return a.tick<b.tick;});
    return result;
}

} // namespace threadlight::chapter
