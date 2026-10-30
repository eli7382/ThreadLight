#pragma once

#include "visual_effects.h"
#include "spark_pickup.h"
#include "combat.h"
#include "skitter.h"
#include "ranged_combat.h"
#include "briar_caller.h"
#include "sower.h"
#include "signature_effects.h"
#include <algorithm>
#include <array>

namespace threadlight::presentation {

struct V { float x{}, y{}; };
inline V operator+(V a,V b){return {a.x+b.x,a.y+b.y};}
inline V operator-(V a,V b){return {a.x-b.x,a.y-b.y};}
inline V operator*(V a,float s){return {a.x*s,a.y*s};}

template<class T> inline T blend(T previous,T current,float alpha) {
    return previous+(current-previous)*std::clamp(alpha,0.f,1.f);
}
// Event feedback starts immediately; only its decay needs interpolation.
inline float decay(float previous,float current,float alpha) {
    return current>previous?current:blend(previous,current,alpha);
}

// History lives on each entity, so erasing/compacting a vector cannot blend
// unrelated objects. New objects initialize history at their actual spawn.
// present() returns drawing data and never changes simulation state.
struct Thread {
    V p; float age;
    float previousAge=age;
    std::uint64_t id=0;
    bool severed=false;int corruptedTicks=0;
    void capture(){previousAge=age;}
};
inline V silkPoint(const Thread& knot,bool newest,V shownPlayer) {
    return newest&&knot.previousAge==0.f?shownPlayer:knot.p;
}
struct FoeView { V p; float phase, hitFlash; };
struct Foe {
    V p,vel; float r; combat::EnemyHealth health;
    float speed,phase,hitFlash;
    combat::EnemyKind kind= combat::EnemyKind::Hunter;
    skitter::State skitterState{};
    ranged::State rangedState{};
    V previousP=p;
    float previousPhase=phase,previousHitFlash=hitFlash;
    foundation::RewardId rewardId = 0;
    std::uint64_t projectileTicket = 0;
    bool tacticalSupport=false;
    briar_caller::State briarCaller{};
    briar_caller::SpeedBoost callerSpeedBoost{};
    sower::State sowerState{};
    signature_effects::SlowState slow{};
    void capture(){previousP=p;previousPhase=phase;previousHitFlash=hitFlash;}
    FoeView present(float alpha) const {
        return {blend(previousP,p,alpha), blend(previousPhase,phase,alpha),
                decay(previousHitFlash,hitFlash,alpha)};
    }
};
struct Spark {
    V p; float t;
    V previousP=p; float previousT=t;
    foundation::PickupState pickup{};
    bool bossCore=false;
    void capture(){previousP=p;previousT=t;}
    Spark present(float alpha) const {
        auto result=*this;result.p=blend(previousP,p,alpha);result.t=blend(previousT,t,alpha);return result;
    }
};
template<class Color> struct Particle {
    V p,v;float life,maxLife,size;Color color;
    V previousP=p;float previousLife=life;
    void capture(){previousP=p;previousLife=life;}
    Particle present(float alpha) const {
        auto result=*this;result.p=blend(previousP,p,alpha);result.life=blend(previousLife,life,alpha);return result;
    }
};
struct DeathEffect {
    V p;std::array<V,visual::kHeavyShardCount> velocity;float age{};int kind{};
    float previousAge=age;
    void capture(){previousAge=age;}
    DeathEffect present(float alpha) const {
        auto result=*this;result.age=blend(previousAge,age,alpha);return result;
    }
};

} // namespace threadlight::presentation
