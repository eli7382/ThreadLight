#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace threadlight::unweaver {
// Vitality and encounter liveness are intentionally separate. This ledger is
// the sole owner of carried damage; entering a phase never reapplies a hit.
struct Progress {
    int maximum=84000;
    std::int64_t credited=0;
    int phase=1;
    unsigned experiences=0;
    std::uint64_t generation=1;
    std::uint64_t elapsedTicks=0;
    bool defeated=false;
    int vitality() const {return static_cast<int>(std::max<std::int64_t>(0,maximum-credited));}
    int credit(int effectiveDamage) {
        if(defeated||effectiveDamage<=0)return 0;
        const int amount=std::min(vitality(),effectiveDamage);
        credited+=amount;return amount;
    }
    void experience(std::uint64_t epoch,unsigned bits) {
        if(!defeated&&epoch==generation)experiences|=bits&3u;
    }
    void advanceTick() {
        if(elapsedTicks<std::numeric_limits<std::uint64_t>::max())++elapsedTicks;
    }
    enum class Boundary {None,Transition,Defeat};
    Boundary finishTick() {
        if(defeated)return Boundary::None;
        if(vitality()==0){defeated=true;return Boundary::Defeat;}
        if(elapsedTicks<static_cast<std::uint64_t>(phase)*7200u)return Boundary::None;
        if(phase>=4)return Boundary::None;
        ++phase;++generation;experiences=0;return Boundary::Transition;
    }
};
}
