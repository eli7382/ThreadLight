#pragma once

#include "combat.h"
#include "wave_foundation.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace threadlight::rootwarden {

using Point=foundation::Point;
struct Arena { float left,right,top,bottom; };
struct Branch { Point a,b; };
enum class Stage { Stalk,RootWarning,RootActive,StepWarning,StepActive,Recover,Transition,Defeated };
constexpr int kWarningTicks=54,kReachTicks=42,kStepTicks=108,kRecoveryTicks=192,kTransitionTicks=108,kStalkTicks=90;
constexpr int kMaxHp=7000,kDamage=1800,kExposureBonus=100,kCoreValue=10,kSupportBudget=4;
constexpr float kVulnerableRadius=54.f,kContactRadius=26.f,kRootHalfWidth=20.f;
struct Metrics {
    std::uint64_t ticks=0,hits=0,exposedHits=0,contactTicks=0,cycles=0;
    int silkDamage=0,playerDamage=0,signatureDamage=0;
    std::uint64_t signatureHits=0;
};
struct State {
    combat::EnemyHealth health{kMaxHp,kMaxHp};
    Point p{},previousP{},stepStart{},stepEnd{};
    std::array<Branch,3> branches{};
    int branchCount=0,phase=1,remaining=kStalkTicks,supportsReleased=0;
    Stage stage=Stage::Defeated;
    bool nextReach=true,firstCycleReleased=false;
    Metrics metrics{};
};
struct Events { int supports=0;bool warning=false,phaseChanged=false,defeated=false; };
struct DamageResult { combat::SilkHit hit=combat::SilkHit::None;Events events{};int damage=0;bool exposed=false; };
inline bool alive(const State& s){return s.stage!=Stage::Defeated&&s.health.alive();}
inline bool exposed(const State& s){return alive(s)&&s.stage==Stage::Recover;}
inline const char* stageName(Stage stage){
    switch(stage){
    case Stage::Stalk:return "stalk";case Stage::RootWarning:return "root_warning";
    case Stage::RootActive:return "root_active";case Stage::StepWarning:return "step_warning";
    case Stage::StepActive:return "step_active";case Stage::Recover:return "exposed";
    case Stage::Transition:return "phase_transition";case Stage::Defeated:return "defeated";
    }
    return "invalid";
}
inline Point add(Point a,Point b){return {a.x+b.x,a.y+b.y};}
inline Point sub(Point a,Point b){return {a.x-b.x,a.y-b.y};}
inline Point scale(Point a,float v){return {a.x*v,a.y*v};}
inline float length(Point p){return std::sqrt(p.x*p.x+p.y*p.y);}
inline Point unit(Point p){const float d=length(p);return d>.001f?scale(p,1.f/d):Point{1,0};}
inline Point interpolate(Point a,Point b,float t){return add(a,scale(sub(b,a),t));}
inline Point clamp(Point p,Arena a,float inset=0){
    return {std::clamp(p.x,a.left+inset,a.right-inset),std::clamp(p.y,a.top+inset,a.bottom-inset)};
}
inline Point rayEnd(Point p,Point direction,float distance,Arena a,float inset=0){
    if(direction.x>.00001f)distance=std::min(distance,(a.right-inset-p.x)/direction.x);
    if(direction.x<-.00001f)distance=std::min(distance,(a.left+inset-p.x)/direction.x);
    if(direction.y>.00001f)distance=std::min(distance,(a.bottom-inset-p.y)/direction.y);
    if(direction.y<-.00001f)distance=std::min(distance,(a.top+inset-p.y)/direction.y);
    return add(p,scale(direction,std::max(0.f,distance)));
}
inline void enter(State& s,Stage stage,int ticks){s.stage=stage;s.remaining=ticks;}
inline void start(State& s,Point position,Arena arena){
    s=State{};s.p=s.previousP=clamp(position,arena,kVulnerableRadius);enter(s,Stage::Stalk,kStalkTicks);
}
inline void cancel(State& s){s.branchCount=0;enter(s,Stage::Defeated,0);}
inline combat::EnemyKind supportKind(const State& s,int waveIndex){
    return waveIndex==6&&s.phase>=2?combat::EnemyKind::Skitter:combat::EnemyKind::Hunter;
}
inline int supports(State& s,int count){
    const int grant=std::min(count,kSupportBudget-s.supportsReleased);s.supportsReleased+=grant;return grant;
}

// Called only for an authoritative silk contact. Ordinary target gate remains
// the single owner of damage timing; exposure changes damage, never hit rate.
inline DamageResult applyBossDamage(State& s,int baseDamage,bool ordinary){
    DamageResult result;
    if(!alive(s)||baseDamage<=0)return result;
    result.exposed=ordinary&&exposed(s);const int before=s.health.hp;
    result.hit=ordinary?combat::applySilk(s.health,baseDamage+(result.exposed?kExposureBonus:0)):combat::applyDamage(s.health,baseDamage);
    if(result.hit==combat::SilkHit::None)return result;
    result.damage=before-s.health.hp;
    if(ordinary){++s.metrics.hits;s.metrics.exposedHits+=result.exposed?1:0;s.metrics.silkDamage+=result.damage;}
    else{++s.metrics.signatureHits;s.metrics.signatureDamage+=result.damage;}
    if(result.hit==combat::SilkHit::Killed){cancel(s);result.events.defeated=true;}
    else if(s.phase==1&&s.health.hp*100<kMaxHp*55){
        s.phase=2;s.branchCount=0;enter(s,Stage::Transition,kTransitionTicks);
        result.events.phaseChanged=true;result.events.supports=supports(s,2);
    }
    return result;
}

inline DamageResult applySilk(State& s,int baseDamage){return applyBossDamage(s,baseDamage,true);}
// Caller supplies already rounded/resisted signature damage; no exposure bonus.
inline DamageResult applySignature(State& s,int damage){return applyBossDamage(s,damage,false);}

inline Branch activeBranch(const State& s,int index){
    auto result=s.branches[index];
    if(s.stage==Stage::RootActive){
        const float progress=std::clamp(static_cast<float>(kReachTicks-s.remaining+1)/kReachTicks,0.f,1.f);
        result.b=interpolate(result.a,result.b,progress);
    }
    return result;
}

inline Events tick(State& s,Point player,Arena arena){
    Events events;
    if(!alive(s))return events;
    s.previousP=s.p;++s.metrics.ticks;combat::tick(s.health);
    if(s.stage==Stage::Stalk){
        const auto delta=sub(player,s.p);const float d=length(delta);
        if(d>82.f)s.p=clamp(add(s.p,scale(unit(delta),std::min(85.f/120.f,d-82.f))),arena,kVulnerableRadius);
    }else if(s.stage==Stage::StepActive){
        const float progress=static_cast<float>(kStepTicks-s.remaining+1)/kStepTicks;
        s.p=interpolate(s.stepStart,s.stepEnd,progress);
    }
    if(--s.remaining>0)return events;
    switch(s.stage){
    case Stage::Stalk:{
        const auto direction=unit(sub(player,s.p));
        if(s.nextReach){
            const float angle=std::atan2(direction.y,direction.x);s.branchCount=s.phase==1?2:3;
            for(int i=0;i<s.branchCount;++i){
                const float offset=s.phase==1?(i? .60f:-.60f):(i-1)*.75f;
                const float a=angle+offset;const Point aim{std::cos(a),std::sin(a)};
                const auto startPoint=add(s.p,scale(aim,kContactRadius));
                s.branches[i]={startPoint,rayEnd(startPoint,aim,180.f,arena,kRootHalfWidth)};
            }
            enter(s,Stage::RootWarning,kWarningTicks);
        }else{
            s.branchCount=0;s.stepStart=s.p;
            s.stepEnd=rayEnd(s.p,direction,std::min(140.f,length(sub(player,s.p))+32.f),arena,kVulnerableRadius);
            enter(s,Stage::StepWarning,kWarningTicks);
        }
        events.warning=true;break;
    }
    case Stage::RootWarning:enter(s,Stage::RootActive,kReachTicks);break;
    case Stage::StepWarning:enter(s,Stage::StepActive,kStepTicks);break;
    case Stage::RootActive:
    case Stage::StepActive:s.branchCount=0;enter(s,Stage::Recover,kRecoveryTicks);break;
    case Stage::Recover:
        ++s.metrics.cycles;s.nextReach=!s.nextReach;
        if(!s.firstCycleReleased){s.firstCycleReleased=true;events.supports=supports(s,2);}
        enter(s,Stage::Stalk,kStalkTicks);break;
    case Stage::Transition:enter(s,Stage::Stalk,kStalkTicks);break;
    case Stage::Defeated:break;
    }
    return events;
}

inline int incomingDamage(const State& s,Point previousPlayer,Point player,float radius){
    if(!alive(s)||s.stage==Stage::Transition)return 0;
    if(combat::bodyContact(previousPlayer,player,radius,s.previousP,s.p,kContactRadius))return kDamage;
    if(s.stage==Stage::RootActive)for(int i=0;i<s.branchCount;++i){
        const auto branch=activeBranch(s,i);
        const float contact=radius+kRootHalfWidth;
        if(foundation::segmentsDistanceSquared(previousPlayer,player,branch.a,branch.b)<=contact*contact)return kDamage;
    }
    return 0;
}
inline bool silkContact(const State& s,Point a,Point b,float halfWidth){
    return alive(s)&&combat::silkContact(s.previousP,s.p,kVulnerableRadius,a,b,halfWidth);
}

// All committed geometry is remapped together; phase clocks remain frozen across
// resize. No lock is recomputed from the player's new position.
inline void remap(State& s,Arena from,Arena to){
    auto map=[&](Point p){return Point{to.left+(p.x-from.left)/(from.right-from.left)*(to.right-to.left),
                                      to.top+(p.y-from.top)/(from.bottom-from.top)*(to.bottom-to.top)};};
    const auto mapped=map(s.p);const auto bounded=clamp(mapped,to,kVulnerableRadius);
    const auto correction=sub(bounded,mapped);s.p=s.previousP=bounded;
    s.stepStart=clamp(add(map(s.stepStart),correction),to,kVulnerableRadius);
    s.stepEnd=clamp(add(map(s.stepEnd),correction),to,kVulnerableRadius);
    for(int i=0;i<s.branchCount;++i){s.branches[i].a=clamp(add(map(s.branches[i].a),correction),to,kRootHalfWidth);s.branches[i].b=clamp(add(map(s.branches[i].b),correction),to,kRootHalfWidth);}
    if(s.stage==Stage::StepActive){
        const float progress=static_cast<float>(kStepTicks-s.remaining)/kStepTicks;
        s.p=s.previousP=interpolate(s.stepStart,s.stepEnd,progress);
    }
}

} // namespace threadlight::rootwarden
