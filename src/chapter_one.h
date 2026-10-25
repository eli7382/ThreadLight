#pragma once

#include "combat.h"
#include "wave_foundation.h"
#include <array>
#include <algorithm>
#include <cstdint>

namespace threadlight::chapter {

constexpr int kEncounterCount = 9;
constexpr float kPi = 3.14159265359f;
struct Encounter { const char* name; int seconds; bool boss; int chapter=1; };
inline Encounter encounter(int index) {
    constexpr Encounter definitions[] = {{"FIRST THREAD",20,false},{"HEAVY ENDS",22,false},
        {"FOUR CORNERS",24,false},{"SIDE STITCH",24,false},{"CROSSGRAIN",26,false},
        {"THREE BEATS",28,false},{"THE ROOTWARDEN",0,true},
        {"TWO NEEDLES",26,false,2},{"TURNING WATCH",28,false,2}};
    return definitions[std::clamp(index,0,kEncounterCount-1)];
}
struct Spawn { int tick; combat::EnemyKind kind; float angle; };
struct Schedule { std::array<Spawn,32> items{}; int count=0; };

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
    default: break; // Boss supports are encounter events, never scheduled farming.
    }
    std::stable_sort(result.items.begin(),result.items.begin()+result.count,
                     [](const Spawn& a,const Spawn& b){return a.tick<b.tick;});
    return result;
}

} // namespace threadlight::chapter
