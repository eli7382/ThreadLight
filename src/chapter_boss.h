#pragma once

#include "rootwarden.h"
#include "ranged_combat.h"
#include "area_hazards.h"
#include "unweaver.h"
#include "support_formations.h"
#include <array>
#include <cstdint>

namespace threadlight::chapter_boss {
namespace legacy=rootwarden;
using Point=foundation::Point;
using Arena=legacy::Arena;using Branch=legacy::Branch;using Stage=legacy::Stage;
using Events=legacy::Events;using DamageResult=legacy::DamageResult;
using legacy::add;using legacy::sub;using legacy::scale;using legacy::length;
using legacy::unit;using legacy::interpolate;using legacy::clamp;using legacy::rayEnd;
using legacy::stageName;using legacy::activeBranch;
constexpr int kCoreValue=10;
constexpr float kVulnerableRadius=legacy::kVulnerableRadius;
constexpr float kContactRadius=legacy::kContactRadius,kRootHalfWidth=legacy::kRootHalfWidth;
constexpr float kPi=3.14159265359f;
enum class Kind {Rootwarden, Dancer, Conductor, Hollow, Heart, Unweaver};
enum class Pattern {Fold, Ring, Spokes, Pulse, Lunge, Lane, Fork};
struct Bud {Point p{};combat::EnemyHealth health{200,200};std::uint64_t id=0;};
struct ScoredShot {Point p{},direction{};ranged::Pool::Ticket ticket=0;int bud=-1;};
// The small shared record preserves the proven Rootwarden implementation by
// delegation. Advanced fields belong only to the two new encounter machines.
struct State:legacy::State {
    Kind kind=Kind::Rootwarden;
    unweaver::State unweaver{};bool experimentalSupports=false;
    int chain=0,pattern=0,shotCursor=0,shotCount=0,budCount=0;
    int strikeTicks=34;bool strikeThisTick=false;
    bool counterstepping=false,counterThisTick=false;Point counterStart{},counterEnd{};
    std::array<Bud,3> buds{};
    std::array<ScoredShot,16> shots{};
    ranged::Pool bullets;
    std::uint64_t nextSource=0x8000000000000000ull,nextBud=0xC000000000000000ull;
    std::uint64_t shotsFired=0,warnings=0,budsCut=0;
    area_hazards::Pool fields{area_hazards::PoolKind::Boss};
    area_hazards::Corridor corridor{};
    std::uint64_t hazardOwner=0x7000000000000001ull;
    Pattern spatialPattern=Pattern::Fold;int sequenceBeat=0,spatialPass=0;
    int activePatternTicks=0;Point patternOrigin{},relocateEnd{},patternAxis{1,0};
};
inline bool spatial(const State& s){return s.kind==Kind::Hollow||s.kind==Kind::Heart;}
inline int maxHp(Kind kind){return kind==Kind::Unweaver?84000:kind==Kind::Heart?36000:kind==Kind::Hollow?28500:kind==Kind::Dancer?13500:kind==Kind::Conductor?21000:7000;}
inline float vulnerableRadius(const State& s){return s.kind==Kind::Unweaver?76.f:s.kind==Kind::Heart?78.f:s.kind==Kind::Hollow?74.f:s.kind==Kind::Conductor?66.f:s.kind==Kind::Dancer?52.f:54.f;}
inline float contactRadius(const State& s){return s.kind==Kind::Unweaver?32.f:s.kind==Kind::Heart?30.f:s.kind==Kind::Hollow?28.f:s.kind==Kind::Conductor?28.f:s.kind==Kind::Dancer?22.f:26.f;}
inline const char* name(const State& s){return s.kind==Kind::Unweaver?"THE UNWEAVER":s.kind==Kind::Heart?"THE BRIAR HEART":s.kind==Kind::Hollow?"THE HOLLOW WEAVER":s.kind==Kind::Dancer?"THE NEEDLE DANCER":s.kind==Kind::Conductor?"THE THORN CONDUCTOR":"THE ROOTWARDEN";}
inline combat::EnemyKind supportKind(const State& s,int ordinal){
    if(s.experimentalSupports){const auto config=support_formations::earlierBossWave(s.kind==Kind::Dancer?14:s.kind==Kind::Conductor?21:28);
        if(const auto* member=support_formations::memberForOrdinal(config,ordinal))return member->kind;}
    if(s.kind==Kind::Rootwarden)return ordinal>=2?combat::EnemyKind::Skitter:combat::EnemyKind::Hunter;
    if(s.kind==Kind::Hollow)return ordinal>=4?combat::EnemyKind::Sower:combat::EnemyKind::Hunter;
    if(s.kind==Kind::Heart)return ordinal>=6?combat::EnemyKind::Spitter:ordinal>=4?combat::EnemyKind::Skitter:combat::EnemyKind::Hunter;
    return s.kind==Kind::Dancer?combat::EnemyKind::Skitter:s.kind==Kind::Conductor&&ordinal>=4?combat::EnemyKind::Spitter:combat::EnemyKind::Hunter;
}
template <typename Visitor>
inline void visitSupportBatch(const State& s,int count,Visitor visitor){
    const int released=std::max(0,s.supportsReleased);
    const int batch=std::clamp(count,0,released);
    const int firstOrdinal=released-batch;
    for(int offset=0;offset<batch;++offset){
        const int ordinal=firstOrdinal+offset;
        visitor(supportKind(s,ordinal),offset,ordinal);
    }
}
inline int supportCapacity(const State& s){return s.kind==Kind::Unweaver||s.experimentalSupports?4:spatial(s)?3:s.kind==Kind::Dancer?2:6;}
inline int supportSlots(const State& s){
    if(s.kind==Kind::Unweaver)return s.stage==Stage::Recover?1:2;
    if(s.kind==Kind::Rootwarden)return 0;
    const bool priority=s.stage==Stage::Stalk&&s.remaining<=30;
    if(s.kind==Kind::Dancer)return(priority||s.counterstepping||s.stage==Stage::StepWarning||s.stage==Stage::StepActive)?1:0;
    return(priority||s.stage==Stage::RootWarning||s.stage==Stage::RootActive||s.stage==Stage::StepWarning||s.stage==Stage::StepActive||s.stage==Stage::Transition)?2:0;
}
inline void waitForSupports(State& s,int committed,std::size_t flying){
    if(s.kind==Kind::Unweaver)return;
    if(s.kind==Kind::Rootwarden||s.stage!=Stage::Stalk||s.counterstepping||s.remaining>1)return;
    const bool blocked=s.kind==Kind::Dancer?committed>1:committed>0||flying>0;
    if(blocked)s.remaining=2;
}
inline bool alive(const State& s){return s.kind==Kind::Unweaver?!s.unweaver.progress.defeated&&s.stage!=Stage::Defeated:legacy::alive(s);}
inline bool exposed(const State& s){return alive(s)&&s.stage==Stage::Recover;}
inline void enter(State& s,Stage stage,int ticks){legacy::enter(s,stage,ticks);}
inline void clearThreats(State& s){if(s.kind==Kind::Unweaver)unweaver::clear(s.unweaver);s.bullets.clear();s.fields.clear();s.corridor={};s.shotCount=s.shotCursor=s.budCount=0;s.branchCount=0;s.strikeThisTick=false;s.counterstepping=s.counterThisTick=false;}
inline void cancel(State& s){clearThreats(s);legacy::cancel(s);}
inline void start(State& s,Point position,Arena arena,Kind kind=Kind::Rootwarden){
    s=State{};s.kind=kind;
    if(kind==Kind::Rootwarden){legacy::start(s,position,arena);return;}
    s.health={maxHp(kind),maxHp(kind)};s.p=s.previousP=clamp(position,arena,vulnerableRadius(s));
    enter(s,Stage::Stalk,90);
}
inline DamageResult applyBossDamage(State& s,int damage,bool ordinary){
    if(s.kind==Kind::Rootwarden)return legacy::applyBossDamage(s,damage,ordinary);
    if(s.kind==Kind::Unweaver)return unweaver::damage(s,damage,ordinary);
    DamageResult result;if(!alive(s)||damage<=0)return result;
    result.exposed=ordinary&&exposed(s);const int before=s.health.hp;
    result.hit=ordinary?combat::applySilk(s.health,damage+(result.exposed?100:0)):combat::applyDamage(s.health,damage);
    if(result.hit==combat::SilkHit::None)return result;
    result.damage=before-s.health.hp;
    if(ordinary){++s.metrics.hits;s.metrics.exposedHits+=result.exposed?1:0;s.metrics.silkDamage+=result.damage;}
    else{++s.metrics.signatureHits;s.metrics.signatureDamage+=result.damage;}
    if(!s.health.alive()){cancel(s);result.events.defeated=true;return result;}
    const int percent=s.health.hp*100/s.health.maxHp;
    const int phase=s.kind==Kind::Heart?(percent<25?4:percent<50?3:percent<75?2:1):percent<(s.kind==Kind::Dancer?35:30)?3:percent<(s.kind==Kind::Dancer?70:65)?2:1;
    if(phase>s.phase){
        const int crossed=phase-s.phase;s.phase=phase;clearThreats(s);s.chain=0;
        enter(s,Stage::Transition,spatial(s)?120:108);result.events.phaseChanged=true;
        const int budget=s.experimentalSupports?8:s.kind==Kind::Heart?8:s.kind==Kind::Dancer?4:6;
        result.events.supports=std::min(crossed*2,budget-s.supportsReleased);
        s.supportsReleased+=result.events.supports;
    }
    return result;
}
inline DamageResult applySilk(State& s,int damage){return applyBossDamage(s,damage,true);}
inline DamageResult applySignature(State& s,int damage){return applyBossDamage(s,damage,false);}
inline bool silkContact(const State& s,Point a,Point b,float width){
    return alive(s)&&combat::silkContact(s.previousP,s.p,vulnerableRadius(s),a,b,width);
}
inline void prepareLine(State& s,Point player,Arena arena){
    s.strikeTicks=34;
    s.stepStart=s.p;
    s.stepEnd=rayEnd(s.p,unit(sub(player,s.p)),std::min(170.f,length(sub(player,s.p))+32.f),arena,vulnerableRadius(s));
    enter(s,Stage::StepWarning,48);++s.warnings;
}
inline bool prepareScore(State& s,Point player,Arena arena){
    const int pattern=s.phase==3&&s.metrics.cycles%2==1?3:static_cast<int>(s.metrics.cycles%3);
    const int needed=pattern==0?6:pattern==1?3:pattern==2?7:7;
    if(s.bullets.freeSlots()<static_cast<std::size_t>(needed))return false;
    s.pattern=pattern;s.shotCursor=s.shotCount=s.budCount=0;
    const auto aim=unit(sub(player,s.p));const float angle=std::atan2(aim.y,aim.x);
    auto shot=[&](Point origin,float direction,int bud=-1){
        const auto ticket=s.bullets.reserve(s.nextSource++);
        if(!ticket)return false;
        s.shots[s.shotCount++]={origin,{std::cos(direction),std::sin(direction)},ticket,bud};return true;
    };
    if(pattern==0||pattern==3)for(int i=0;i<6;++i){
        const float a=angle+(i-2.5f)*.26f;
        const Point d{std::cos(a),std::sin(a)};
        shot(add(s.p,scale(d,contactRadius(s)+8)),a);
    }
    if(pattern==2){
        // Omit five adjacent rays around the locked player bearing. The player
        // starts in the gap; no compulsory dash or last-second route search.
        for(int i=3;i<=9;++i){const float a=angle+i*kPi/6.f;
            shot(add(s.p,{std::cos(a)*36.f,std::sin(a)*36.f}),a);}
    }
    if(pattern==1||pattern==3){
        s.budCount=pattern==1?3:1;
        for(int i=0;i<s.budCount;++i){
            const float a=angle+kPi+(i-(s.budCount-1)*.5f)*1.1f;
            auto& bud=s.buds[i];bud={clamp(add(s.p,{std::cos(a)*132,std::sin(a)*132}),arena,22),{200,200},s.nextBud++};
            const auto d=unit(sub(player,bud.p));shot(bud.p,std::atan2(d.y,d.x),i);
        }
    }
    enter(s,Stage::RootWarning,54);++s.warnings;return true;
}
inline void fire(State& s,int index){
    auto& shot=s.shots[index];
    if(shot.bud>=0&&!s.buds[shot.bud].health.alive()){s.bullets.cancelReservation(shot.ticket);return;}
    if(s.bullets.launch(shot.ticket,shot.p,shot.direction,{180.f,5.f,shot.bud>=0?2400:2000,360}))++s.shotsFired;
}

inline bool prepareSpatial(State& s,Point player,Arena arena,float externalArea,Pattern pattern){
    namespace field=area_hazards;
    s.fields.clear();s.spatialPattern=pattern;s.patternOrigin=s.p;s.branchCount=0;
    const auto bounds=field::Bounds{arena.left,arena.right,arena.top,arena.bottom};
    auto axis=unit(sub(player,s.p));
    if(s.kind==Kind::Hollow&&s.metrics.cycles%3==2){const float a=std::atan2(axis.y,axis.x)+.65f;axis={std::cos(a),std::sin(a)};}
    s.patternAxis=axis;
    const Point side{-axis.y,axis.x};
    s.corridor=s.kind==Kind::Hollow?field::Corridor{true,rayEnd(s.p,scale(axis,-1),100000,arena),rayEnd(s.p,axis,100000,arena),68}:field::Corridor{};
    s.relocateEnd=rayEnd(s.p,axis,s.kind==Kind::Hollow?135.f:0.f,arena,vulnerableRadius(s));
    if(pattern==Pattern::Lunge){
        s.strikeTicks=42;s.stepStart=s.p;s.stepEnd=rayEnd(s.p,axis,std::min(210.f,length(sub(player,s.p))+48),arena,vulnerableRadius(s));
        enter(s,Stage::StepWarning,51);++s.warnings;return true;
    }
    std::array<field::PatchSpec,4> specs{};int count=0;
    const int warning=s.kind==Kind::Hollow?60:57;
    const int duration=s.kind==Kind::Hollow?360:pattern==Pattern::Pulse?120:90;
    auto capsule=[&](Point a,Point b,float width){
        auto& spec=specs[count++];spec.shape=field::Shape::Capsule;spec.ownerId=s.hazardOwner;
        spec.center=s.p;spec.a=clamp(a,arena,width);spec.b=clamp(b,arena,width);spec.halfWidth=width;
        spec.warningTicks=warning;spec.activeTicks=duration;spec.damage=s.kind==Kind::Hollow?2400:2800;
    };
    if(pattern==Pattern::Fold||pattern==Pattern::Lane){
        for(int sign:{-1,1}){const Point offset=scale(side,pattern==Pattern::Fold?120.f:90.f);
            capsule(add(add(s.p,scale(offset,(float)sign)),scale(axis,-110)),add(add(s.p,scale(offset,(float)sign)),scale(axis,110)),18);}
    }else if(pattern==Pattern::Spokes||pattern==Pattern::Fork){
        const int rays=pattern==Pattern::Spokes?4:2;
        const float base=std::atan2(axis.y,axis.x);
        for(int i=0;i<rays;++i){const float a=base+(i%2?1.f:-1.f)*(pattern==Pattern::Spokes?1.4f:.70f)+(i/2)*kPi;
            const Point d{std::cos(a),std::sin(a)};const auto startPoint=clamp(add(s.p,scale(d,pattern==Pattern::Spokes?110.f:40.f)),arena,18);
            capsule(startPoint,rayEnd(startPoint,d,pattern==Pattern::Spokes?190.f:210.f,arena,18),18);}
    }else{
        auto& spec=specs[count++];spec.shape=field::Shape::Annulus;spec.ownerId=s.hazardOwner;spec.center=s.p;
        spec.radiusStart=pattern==Pattern::Ring?119.f:50.f;spec.radiusEnd=pattern==Pattern::Ring?119.f:300.f;
        spec.ringHalfWidth=pattern==Pattern::Ring?29.f:14.f;
        spec.gapDirection=axis;spec.gapHalfAngleRadians=pattern==Pattern::Ring?.9f:.8f;
        spec.warningTicks=warning;spec.activeTicks=duration;spec.damage=s.kind==Kind::Hollow?2400:2800;
    }
    for(int i=0;i<count;++i){
        const auto protectedRoute=s.kind==Kind::Hollow&&specs[i].shape==field::Shape::Capsule?s.corridor:field::Corridor{};
        if(!s.fields.add(specs[i],bounds,protectedRoute,externalArea)){
            s.fields.clear();
            // Edge-clipped folds/spokes may lose their corridor. A ring with
            // the same locked gaps is the deterministic, readable fallback.
            if(s.kind==Kind::Hollow&&pattern!=Pattern::Ring)return prepareSpatial(s,player,arena,externalArea,Pattern::Ring);
            s.corridor={};return false;
        }
    }
    s.activePatternTicks=duration;enter(s,Stage::RootWarning,warning);++s.warnings;return true;
}
inline Pattern heartBeat(int beat){
    constexpr Pattern beats[]={Pattern::Lane,Pattern::Lunge,Pattern::Pulse,Pattern::Fork};return beats[std::clamp(beat,0,3)];
}
inline Events tickSpatial(State& s,Point player,Arena arena,float externalArea){
    Events events;if(!alive(s))return events;
    s.previousP=s.p;s.strikeThisTick=s.stage==Stage::StepActive;s.counterThisTick=false;
    ++s.metrics.ticks;combat::tick(s.health);
    if(s.stage==Stage::Stalk){const float d=length(sub(player,s.p));
        if(d>100)s.p=clamp(add(s.p,scale(unit(sub(player,s.p)),std::min(88.f/120.f,d-100))),arena,vulnerableRadius(s));
    }else if(s.stage==Stage::StepActive){
        s.p=interpolate(s.stepStart,s.stepEnd,(s.strikeTicks-s.remaining+1)/static_cast<float>(s.strikeTicks));
    }else if(s.stage==Stage::RootActive&&s.kind==Kind::Hollow){
        const float progress=std::clamp((s.activePatternTicks-s.remaining+1)/static_cast<float>(s.activePatternTicks),0.f,1.f);
        s.p=interpolate(s.patternOrigin,s.relocateEnd,progress);
    }
    if(s.stage==Stage::RootActive&&s.remaining<=1&&s.fields.liveCount()>0){s.remaining=1;return events;}
    if(--s.remaining>0)return events;
    auto begin=[&](Pattern pattern){
        if(prepareSpatial(s,player,arena,externalArea,pattern)){events.warning=true;return true;}
        enter(s,Stage::Stalk,30);return false;
    };
    switch(s.stage){
    case Stage::Stalk:
        s.sequenceBeat=s.spatialPass=0;
        if(s.kind==Kind::Hollow)begin(s.phase==3?(s.metrics.cycles%2?Pattern::Spokes:Pattern::Ring):s.metrics.cycles%2?Pattern::Ring:Pattern::Fold);
        else begin(s.phase==4?heartBeat(0):s.metrics.cycles%3==0?Pattern::Pulse:s.metrics.cycles%3==1?Pattern::Lunge:Pattern::Fork);
        break;
    case Stage::RootWarning:enter(s,Stage::RootActive,s.activePatternTicks);break;
    case Stage::StepWarning:enter(s,Stage::StepActive,s.strikeTicks);break;
    case Stage::RootActive:
    case Stage::StepActive:
        s.fields.clear();
        if(s.kind==Kind::Heart&&s.phase==4&&s.sequenceBeat<3){++s.sequenceBeat;begin(heartBeat(s.sequenceBeat));}
        else if(s.kind==Kind::Heart&&s.phase<4&&s.spatialPattern==Pattern::Fork&&s.spatialPass==0){s.spatialPass=1;begin(Pattern::Fork);}
        else{enter(s,Stage::Recover,s.kind==Kind::Heart&&s.phase==4?360:240);}
        break;
    case Stage::Recover:
        ++s.metrics.cycles;
        if(!s.firstCycleReleased){s.firstCycleReleased=true;events.supports=2;s.supportsReleased+=2;}
        s.corridor={};enter(s,Stage::Stalk,90);break;
    case Stage::Transition:enter(s,Stage::Stalk,90);break;
    case Stage::Defeated:break;
    }
    return events;
}
inline Events tick(State& s,Point player,Arena arena,float externalArea=0){
    if(s.kind==Kind::Unweaver)return unweaver::tick(s,player,arena);
    if(s.kind==Kind::Rootwarden)return legacy::tick(s,player,arena);
    if(spatial(s))return tickSpatial(s,player,arena,externalArea);
    Events events;if(!alive(s))return events;
    s.previousP=s.p;s.strikeThisTick=s.stage==Stage::StepActive;s.counterThisTick=s.counterstepping;++s.metrics.ticks;combat::tick(s.health);
    for(int i=0;i<s.budCount;++i)combat::tick(s.buds[i].health);
    if(s.stage==Stage::Stalk){
        if(s.counterstepping)s.p=interpolate(s.counterStart,s.counterEnd,(48-s.remaining+1)/48.f);
        else{
        const float d=length(sub(player,s.p));const float standOff=s.kind==Kind::Dancer?88.f:100.f;
        if(d>standOff)s.p=clamp(add(s.p,scale(unit(sub(player,s.p)),std::min((s.kind==Kind::Dancer?110.f:88.f)/120.f,d-standOff))),arena,vulnerableRadius(s));
        }
    }else if(s.stage==Stage::StepActive){
        s.p=interpolate(s.stepStart,s.stepEnd,(s.strikeTicks-s.remaining+1)/static_cast<float>(s.strikeTicks));
    }
    if(--s.remaining>0)return events;
    switch(s.stage){
    case Stage::Stalk:
        if(s.kind==Kind::Dancer){if(!s.counterstepping)s.chain=0;s.counterstepping=false;prepareLine(s,player,arena);events.warning=true;}
        else if(prepareScore(s,player,arena))events.warning=true;
        else enter(s,Stage::Stalk,30);
        break;
    case Stage::StepWarning:enter(s,Stage::StepActive,s.strikeTicks);break;
    case Stage::StepActive:
        ++s.chain;
        if(s.chain<s.phase){
            const auto d=unit(sub(player,s.p));const float side=s.chain%2?1.f:-1.f;
            s.counterStart=s.p;s.counterEnd=clamp(add(s.p,{-d.y*48.f*side,d.x*48.f*side}),arena,vulnerableRadius(s));
            s.counterstepping=true;enter(s,Stage::Stalk,48);
        }
        else enter(s,Stage::Recover,s.phase==3?240:168);
        break;
    case Stage::RootWarning:
        if(s.pattern==1){fire(s,0);s.shotCursor=1;enter(s,Stage::RootActive,42);}
        else{
            const int count=s.pattern==3?6:s.shotCount;
            for(int i=0;i<count;++i)fire(s,i);
            s.shotCursor=count;enter(s,Stage::RootActive,s.pattern==3?84:60);
        }
        break;
    case Stage::RootActive:
        if(s.shotCursor<s.shotCount){fire(s,s.shotCursor++);enter(s,Stage::RootActive,42);}
        else{s.budCount=0;enter(s,Stage::Recover,s.phase==3?240:216);}
        break;
    case Stage::Recover:
        ++s.metrics.cycles;
        if((s.kind==Kind::Conductor||s.experimentalSupports)&&!s.firstCycleReleased){
            s.firstCycleReleased=true;events.supports=2;s.supportsReleased+=2;
        }
        enter(s,Stage::Stalk,90);break;
    case Stage::Transition:enter(s,Stage::Stalk,90);break;
    case Stage::Defeated:break;
    }
    return events;
}
inline int incomingDamage(const State& s,Point previous,Point player,float radius){
    if(s.kind==Kind::Unweaver)return unweaver::hostileDamage(s,previous,player,radius);
    if(s.kind==Kind::Rootwarden)return legacy::incomingDamage(s,previous,player,radius);
    if(!alive(s)||s.stage==Stage::Transition)return 0;
    if(spatial(s))return combat::bodyContact(previous,player,radius,s.previousP,s.p,contactRadius(s))?(s.kind==Kind::Heart?(s.strikeThisTick?3200:2600):2400):0;
    if((s.counterstepping||s.counterThisTick)&&!s.strikeThisTick)return 0;
    // A short escape grace prevents a harmless flourish ending on the player
    // from becoming an immediate contact hit on the following warning tick.
    if(s.kind==Kind::Dancer&&s.chain>0&&s.stage==Stage::StepWarning&&s.remaining>36)return 0;
    if(combat::bodyContact(previous,player,radius,s.previousP,s.p,contactRadius(s)))
        return s.kind==Kind::Dancer?(s.strikeThisTick?2200:2000):2200;
    return 0;
}
inline ranged::Impacts projectileTick(State& s,Point previous,Point player,float radius,Arena arena){
    return s.bullets.tick(previous,player,radius,{arena.left,arena.right,arena.top,arena.bottom},alive(s));
}
inline combat::SilkHit damageBud(State& s,int index,int damage,bool ordinary){
    if(index<0||index>=s.budCount)return combat::SilkHit::None;
    auto& bud=s.buds[index];const auto hit=ordinary?combat::applySilk(bud.health,damage):combat::applyDamage(bud.health,damage);
    if(hit==combat::SilkHit::Killed){
        ++s.budsCut;for(int i=s.shotCursor;i<s.shotCount;++i)if(s.shots[i].bud==index)s.bullets.cancelReservation(s.shots[i].ticket);
    }
    return hit;
}
inline void remap(State& s,Arena from,Arena to,float externalArea=0){
    if(s.kind==Kind::Rootwarden){legacy::remap(s,from,to);return;}
    auto map=[&](Point p){return Point{to.left+(p.x-from.left)/(from.right-from.left)*(to.right-to.left),to.top+(p.y-from.top)/(from.bottom-from.top)*(to.bottom-to.top)};};
    const float progress=s.stage==Stage::StepActive?1.f-s.remaining/static_cast<float>(s.strikeTicks):0.f;
    s.p=s.previousP=clamp(map(s.p),to,vulnerableRadius(s));
    if(s.kind==Kind::Unweaver){clearThreats(s);s.stage=Stage::Transition;s.remaining=108;s.unweaver.historyCount=0;return;}
    s.stepStart=clamp(map(s.stepStart),to,vulnerableRadius(s));s.stepEnd=clamp(map(s.stepEnd),to,vulnerableRadius(s));
    s.counterStart=clamp(map(s.counterStart),to,vulnerableRadius(s));s.counterEnd=clamp(map(s.counterEnd),to,vulnerableRadius(s));
    if(s.counterstepping)s.p=s.previousP=interpolate(s.counterStart,s.counterEnd,(48-s.remaining)/48.f);
    if((s.kind==Kind::Dancer||s.kind==Kind::Heart)&&(s.stage==Stage::StepWarning||s.stage==Stage::StepActive)){
        s.strikeTicks=std::max(s.kind==Kind::Heart?42:34,static_cast<int>(std::ceil(length(sub(s.stepEnd,s.stepStart))*120.f/600.f)));
        if(s.stage==Stage::StepActive){
            s.remaining=std::max(1,static_cast<int>(std::ceil((1.f-progress)*s.strikeTicks)));
            s.p=s.previousP=interpolate(s.stepStart,s.stepEnd,1.f-s.remaining/static_cast<float>(s.strikeTicks));
        }
    }
    for(int i=0;i<s.budCount;++i)s.buds[i].p=map(s.buds[i].p);
    for(int i=s.shotCursor;i<s.shotCount;++i){auto& shot=s.shots[i];const auto end=map(add(shot.p,shot.direction));shot.p=map(shot.p);shot.direction=unit(sub(end,shot.p));}
    s.bullets.remap({from.left,from.right,from.top,from.bottom},{to.left,to.right,to.top,to.bottom});
    s.fields.remap({from.left,from.right,from.top,from.bottom},{to.left,to.right,to.top,to.bottom},externalArea);
    s.patternAxis=unit(sub(map(add(s.patternOrigin,s.patternAxis)),map(s.patternOrigin)));
    s.patternOrigin=clamp(map(s.patternOrigin),to,vulnerableRadius(s));s.relocateEnd=clamp(map(s.relocateEnd),to,vulnerableRadius(s));
    if(s.kind==Kind::Hollow&&s.stage==Stage::RootActive)s.p=s.previousP=interpolate(s.patternOrigin,s.relocateEnd,1.f-s.remaining/static_cast<float>(s.activePatternTicks));
    if(s.corridor.enabled){
        s.corridor.a=map(s.corridor.a);s.corridor.b=map(s.corridor.b);
        s.fields.cancelUnsafeCorridors(s.corridor);
    }
}
} // namespace threadlight::chapter_boss
