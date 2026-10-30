#pragma once
#include "unweaver_phase.h"
#include "rootwarden.h"
#include "ranged_combat.h"
#include "player_observation.h"
#include <array>
#include <span>

namespace threadlight::unweaver {
using Point=foundation::Point;using Arena=rootwarden::Arena;
using rootwarden::add;using rootwarden::sub;using rootwarden::scale;
using rootwarden::length;using rootwarden::unit;using rootwarden::clamp;
constexpr float kPi=3.14159265359f;
enum class Style {Predator,Architect,Puppeteer};
enum class Attack {ShatteredHalo,RotatingEclipse,FracturedRhythm,PredictedEscape,FinalConvergence};
enum class Action {Radial,Charge,Hunt,Flank,Focused,Bait,Unique};
inline const char* actionName(Action a){constexpr const char* n[]={"RADIAL","INTERCEPT","CONTACT HUNT","FLANK","FOCUSED FAN","BAIT/COUNTER","WEAVE TRAP"};return n[(int)a];}
enum class Mechanic {Hunt,Echo,Sever,Corrupt,Seal};
enum class Objective {Intercept,CounterDash,Displace,Protect,Engage};
inline const char* styleName(Style x){return x==Style::Predator?"PREDATOR":x==Style::Architect?"ARCHITECT":"PUPPETEER";}
inline const char* attackName(Attack x){constexpr const char* names[]={"SHATTERED HALO","ROTATING ECLIPSE","FRACTURED RHYTHM","PREDICTED ESCAPE","FINAL CONVERGENCE"};return names[(int)x];}
struct Ray {Point origin{},direction{};int fireTick=0;ranged::Pool::Ticket ticket=0;};
struct Echo {std::array<Point,24> path{};int count=0,warning=0,active=0;bool fresh=false;};
struct Strand {std::uint64_t id=0;Point a{},b{};int warning=0,active=0;bool sever=false,fresh=false;};
struct MovingThreat {Point p{},velocity{};float radius=0;int warning=0,lifetime=0;};
struct CapsuleThreat {Point a{},b{};float width=0;int warning=0,lifetime=0;};
struct State {
    Action action=Action::Radial;bool focused=false,unique=false,chargePreview=false,validCharge=false;
    bool chargeIsRetreat=false;std::uint64_t chargePredictionHits=0,chargePredictionMisses=0;Point chargeForecast{},aimTarget{},chargeStart{},chargeEnd{},forecast{},linearForecast{};int forecastTicks=0,tacticTicks=0;float predictionError=0;
    std::array<float,5> recentFamilies{};std::array<float,7> recentActions{};std::array<std::uint64_t,7> actionChoices{};std::array<std::uint64_t,5> familyChoices{};
    std::uint64_t activeTicks=0,radialTicks=0,chargeAttempts=0,contactTicks=0,predictions=0;double forecastError=0,linearError=0;
    std::array<std::uint64_t,8> stageTicks{};
    Progress progress{};Style style=Style::Predator;Attack attack=Attack::ShatteredHalo;Mechanic mechanic=Mechanic::Hunt;
    observation::Summary profile{};Objective objective=Objective::Intercept;
    std::array<Ray,54> rays{};int rayCount=0,clock=0,warningTicks=102,sequenceTicks=0;
    std::array<Point,360> history{};int historyHead=0,historyCount=0;
    Echo echo{};std::array<Strand,4> strands{};
    std::array<MovingThreat,72> externalShots{};int externalShotCount=0;
    std::array<CapsuleThreat,12> externalFields{};int externalFieldCount=0;
    std::array<MovingThreat,8> bodies{};int bodyCount=0;
    std::array<std::array<Point,2>,32> silkObstacles{};int silkObstacleCount=0;
    Point playerVelocity{},commitVelocity{},goal{},gap{1,0},anchor{};int safeRoute=0;bool safeUsesDash=false;
    float playerSpeed=255,playerDashMultiplier=3.7f,dashCooldown=0,dashRemaining=0;
    Arena playerBounds{};
    std::array<float,3> success{},attempts{},styleScores{};
    int cycles=0,styleCycles=0,think=0,sealTicks=0,sealBroken=0,silkActiveTicks=144;
    bool interceptUsed=false,followup=false,resultPending=false,formationBreakPending=false,lastDamageOrdinary=false;int consecutiveRejects=0,responseTicks=0;Point responseDirection{};
    int recoveryBonus=0,formationEvents=0,formationRequest=-1,formationKind=0,formationReleased=0,hitDuringSequence=0,opening=0;
    std::uint64_t plans=0,rejected=0,shots=0,echoes=0,cuts=0,corruptions=0;
    bool planCommitted=false,mechanicExecuted=false,radialExecuted=false,needsStrands=false,huntExecuted=false;
    int forcedAttack=-1,forcedMechanic=-1;
};
inline float angleOf(Point p){return std::atan2(p.y,p.x);}
inline Point direction(float angle){return {std::cos(angle),std::sin(angle)};}
inline float angleDistance(float a,float b){return std::abs(std::remainder(a-b,2*kPi));}
inline Point predict(const State& u,Point player,float horizon,bool curved=true){
 Point velocity=u.playerVelocity;float turn=0;
 if(u.historyCount>=25){const auto now=u.history[(u.historyHead-1+360)%360],mid=u.history[(u.historyHead-13+360)%360],old=u.history[(u.historyHead-25+360)%360];
  const Point a=sub(mid,old),b=sub(now,mid);
  if(length(a)>1&&length(b)>1){turn=std::clamp(std::atan2(a.x*b.y-a.y*b.x,a.x*b.x+a.y*b.y)*10.f,-3.f,3.f);
   if(a.x*b.x+a.y*b.y<0)horizon=std::min(horizon,.18f);}
 }
 const float trust=1.f/(1.f+u.predictionError/150.f);horizon*=.5f+.5f*trust;
 if(curved&&std::abs(turn)>.05f)velocity=scale(direction(angleOf(velocity)+turn*horizon*.5f),length(velocity));
 return clamp(add(player,scale(velocity,horizon)),u.playerBounds);
}
inline void record(State& u,Point p){u.history[u.historyHead]=p;u.historyHead=(u.historyHead+1)%360;u.historyCount=std::min(360,u.historyCount+1);}
inline void captureEcho(State& u){
    u.echo={};for(int age=std::min(300,u.historyCount-1);age>=12&&u.echo.count<24;age-=13)
        u.echo.path[u.echo.count++]=u.history[(u.historyHead-1-age+720)%360];
    if(u.echo.count>=2){u.echo.warning=u.warningTicks;u.echo.active=84;++u.echoes;}
}
inline bool friendly(const State& u,std::uint64_t id){
    for(const auto& s:u.strands)if(s.id==id&&s.warning==0&&(s.sever||s.active>0))return false;
    return true;
}
template<class Segment> void selectStrands(State& u,std::span<const Segment> segments,Point player){
    u.needsStrands=false;std::array<float,4> scores{};int count=0;
    const auto& profile=u.profile;
    const Point prediction=add(player,scale(u.playerVelocity,.2f+.4f*profile.confidence+.7f*profile.dashUse));
    for(const auto& seg:segments){
        if((u.mechanic==Mechanic::Sever&&seg.ageTicks+102>=u.silkActiveTicks)||seg.ageTicks>420||!seg.id||length(sub(seg.b,seg.a))<2)continue;
        const Point middle=scale(add(seg.a,seg.b),.5f);
        const float edge=std::max(0.f,std::min({middle.x-u.playerBounds.left,u.playerBounds.right-middle.x,middle.y-u.playerBounds.top,u.playerBounds.bottom-middle.y}));
        const float score=1.f/(30.f+std::sqrt(foundation::segmentDistanceSquared(prediction,seg.a,seg.b)))+
            std::min(2.f,profile.offense)*.25f/(50.f+std::sqrt(foundation::segmentDistanceSquared(u.anchor,seg.a,seg.b)))+
            profile.perimeter*profile.repetition*.25f/(50.f+edge);
        if(count==4&&score<=scores[3])continue;
        const int slot=count<4?count++:3;
        u.strands[slot]={seg.id,seg.a,seg.b,102,180,u.mechanic==Mechanic::Sever};scores[slot]=score;
        for(int i=slot;i>0&&scores[i]>scores[i-1];--i){std::swap(scores[i],scores[i-1]);std::swap(u.strands[i],u.strands[i-1]);}
    }
}
inline void clear(State& u){u.tacticTicks=0;u.validCharge=false;u.focused=u.unique=u.chargePreview=false;u.rayCount=0;u.echo={};u.strands={};u.sealTicks=0;u.planCommitted=false;u.needsStrands=false;u.formationRequest=-1;u.followup=false;u.consecutiveRejects=0;u.resultPending=false;u.formationBreakPending=false;}
inline void chooseStyle(State& u,int phase){
    const auto& p=u.profile;
    u.objective=p.confidence<.4f?Objective::Engage:p.dashUse>.12f?Objective::CounterDash:
        p.perimeter>.6f?Objective::Intercept:p.offense>1.f?Objective::Displace:phase>=3?Objective::Protect:Objective::Engage;
    u.styleScores={1.f+p.repetition*.5f+p.dashUse,1.f+p.offense*.7f+p.perimeter*.7f+p.responseRate,
                   phase>=3?1.4f+p.offense*.5f+(1-p.movement)*.5f:.2f};
    for(int i=0;i<3;++i)u.styleScores[i]+= .7f*(2*u.success[i]-u.attempts[i])/(u.attempts[i]+1.f);
    const int best=static_cast<int>(std::max_element(u.styleScores.begin(),u.styleScores.end())-u.styleScores.begin());
    if(u.styleCycles>=2&&u.styleScores[best]>u.styleScores[(int)u.style]+.15f){u.style=(Style)best;u.styleCycles=0;}
    ++u.styleCycles;
}
inline void makeRays(State& u,Point anchor,Attack attack,float bearing){
    u.rayCount=0;u.anchor=anchor;u.attack=attack;u.gap=direction(bearing);
    if(u.unique){u.sequenceTicks=u.warningTicks+190;return;}
    if(u.focused){for(int group=0;group<2;++group)for(int i=0;i<7;++i)u.rays[u.rayCount++]={anchor,direction(bearing+(i-3)*.12f+group*.06f),u.warningTicks+group*40,0};u.sequenceTicks=u.warningTicks+220;return;}
    const int rings=attack==Attack::ShatteredHalo||attack==Attack::FracturedRhythm?1:attack==Attack::PredictedEscape?2:3;
    int last=u.warningTicks;
    for(int ring=0;ring<rings;++ring){
        const float rotation=attack==Attack::RotatingEclipse?ring*kPi/18:attack==Attack::PredictedEscape?ring*(u.profile.orbitBias>=0?.14f:-.14f):attack==Attack::FinalConvergence?ring*kPi/27:0;
        for(int ray=0;ray<18;++ray){
            const int delay=attack==Attack::FracturedRhythm?(ray%3)*42:attack==Attack::RotatingEclipse?ring*80:attack==Attack::PredictedEscape?ring*90:ring*66+(attack==Attack::FinalConvergence?(ray%2)*18:0);
            auto& shot=u.rays[u.rayCount++];shot={anchor,direction(bearing+ray*2*kPi/18.f+rotation),u.warningTicks+delay,0};last=std::max(last,shot.fireTick);
        }
    }
    u.sequenceTicks=last+320;

}
// Bounded response witnesses use keyboard directions, reaction delay, dash
// protection and the same swept primitives as execution. They cannot establish
// human fairness or predict arbitrary future decisions by moving supports.
inline Point responseDirection(const State& u,Point p,int tick,int route,Point held={}){
    if(tick%4!=0)return held; // A control change is executable even at 30 FPS.
    Point wanted;
    if(tick<24)wanted=length(u.commitVelocity)>.01f?unit(u.commitVelocity):Point{};
    else if(route<4){
        const float bearing=angleOf(u.gap)+kPi/18.f+(route%2?kPi:0)+(u.attack==Attack::RotatingEclipse?std::clamp((tick-u.warningTicks-30)/80,0,2)*kPi/18.f:0);
        wanted=sub(add(u.anchor,scale(direction(bearing),std::clamp(length(sub(p,u.anchor)),130.f,260.f))),p);
    }else wanted=direction((route-4)*kPi*.5f);
    return length(wanted)>.1f?direction(std::round(angleOf(wanted)/(kPi*.25f))*kPi*.25f):Point{};
}
inline bool witness(const State& u,Point player,float radius,int route,bool useDash){
    radius+=3.f; // Clearance margin for presentation/input sampling uncertainty.
    Point p=player,held{};float cd=u.dashCooldown;int dashTicks=static_cast<int>(std::ceil(u.dashRemaining*120));
    for(int tick=0;tick<u.sequenceTicks;++tick){
        const Point wanted=responseDirection(u,p,tick,route,held);held=wanted;
        if(useDash&&tick%4==0&&dashTicks==0&&cd<=0&&tick>=u.warningTicks-12&&tick<u.warningTicks+120){dashTicks=23;cd=2.f;}
        const bool protectedNow=dashTicks>0;const Point previous=p;
        p=clamp(add(p,scale(wanted,u.playerSpeed*(protectedNow?u.playerDashMultiplier:1.f)/120.f)),u.playerBounds);
        if(protectedNow)--dashTicks;
        cd-=1.f/120;
        if(protectedNow)continue;
        Point bodyA=u.anchor,bodyB=u.anchor;
        if(u.chargePreview){const float a=std::clamp((tick-66)/42.f,0.f,1.f),b=std::clamp((tick+1-66)/42.f,0.f,1.f);bodyA=rootwarden::interpolate(u.chargeStart,u.chargeEnd,a);bodyB=rootwarden::interpolate(u.chargeStart,u.chargeEnd,b);}
        if(combat::bodyContact(previous,p,radius,bodyA,bodyB,32))return false;
        for(const auto& strand:u.strands)if(strand.id&&!strand.sever&&tick>=strand.warning&&tick<strand.warning+strand.active&&
            combat::silkContact(previous,p,radius,strand.a,strand.b,7))return false;
        for(int i=0;i<u.rayCount;++i){const auto& ray=u.rays[i];const int age=tick+1-ray.fireTick;if(age<0||age>=(u.focused?160:300))continue;
            const Point a=add(ray.origin,scale(ray.direction,40.f+age*210.f/120));const Point b=add(a,scale(ray.direction,210.f/120));
            if(combat::bodyContact(previous,p,radius,a,b,5))return false;
        }
        if(u.echo.count>1&&tick>=u.echo.warning&&tick<u.echo.warning+u.echo.active)
            for(int i=1;i<u.echo.count;++i)if(combat::silkContact(previous,p,radius,u.echo.path[i-1],u.echo.path[i],8))return false;
        for(int i=0;i<u.externalShotCount;++i){const auto& x=u.externalShots[i];if(tick<x.warning||tick>x.lifetime)continue;
            const Point a=add(x.p,scale(x.velocity,tick/120.f)),b=add(a,scale(x.velocity,1.f/120));
            if(combat::bodyContact(previous,p,radius,a,b,x.radius))return false;
        }
        for(int i=0;i<u.externalFieldCount;++i){const auto& x=u.externalFields[i];
            if(tick>=x.warning&&tick<x.lifetime&&combat::silkContact(previous,p,radius,x.a,x.b,x.width))return false;
        }
        for(int i=0;i<u.bodyCount;++i){const auto& x=u.bodies[i];const Point a=add(x.p,scale(x.velocity,std::min(tick,60)/120.f));
            if(combat::bodyContact(previous,p,radius,a,a,x.radius+4))return false;
        }
    }
    return true;
}
template<class Boss> bool commit(Boss& s,Point player,Arena arena){
    auto& u=s.unweaver;chooseStyle(u,s.phase);++u.plans;
    u.warningTicks=u.focused?72:102;u.commitVelocity=u.playerVelocity;u.mechanicExecuted=false;u.radialExecuted=false;u.hitDuringSequence=0;
    u.mechanic=s.phase==1?Mechanic::Hunt:s.phase==2?Mechanic::Echo:s.phase==3?Mechanic::Seal:Mechanic::Echo;
    if(s.phase>=3&&u.progress.experiences==3){u.mechanic=u.profile.offense>1.f?Mechanic::Corrupt:u.profile.repetition>.35f?Mechanic::Echo:u.style==Style::Puppeteer?Mechanic::Seal:Mechanic::Sever;}
    if(u.unique)u.mechanic=s.phase<3?Mechanic::Echo:u.style==Style::Puppeteer?Mechanic::Seal:u.profile.offense>1?Mechanic::Corrupt:Mechanic::Sever;
    if(u.forcedMechanic>=0)u.mechanic=(Mechanic)u.forcedMechanic;
    // Once a seal has executed, the next mandatory action is its radial exchange.
    if(u.mechanic==Mechanic::Seal&&(u.progress.experiences&2)&&!(u.progress.experiences&1))u.mechanic=Mechanic::Corrupt;
    if(u.mechanic==Mechanic::Seal&&s.bullets.flyingCount()==0){
        s.budCount=s.phase==4?3:2;u.sealTicks=420;u.sealBroken=0;
        const float axis=angleOf(sub(player,s.p));
        for(int i=0;i<s.budCount;++i)s.buds[i]={clamp(add(s.p,scale(direction(axis+(i?1.f:-1.f)*1.2f),150)),arena,22),{450,450},s.nextBud++};
        s.stage=rootwarden::Stage::RootWarning;s.remaining=102;u.clock=0;u.planCommitted=true;return true;
    }
    if(u.mechanic==Mechanic::Seal||s.bullets.freeSlots()<54)return false;
    const float distance=length(sub(player,s.p));
    std::array<float,5> familyScore{2.4f+(distance<180?1.f:0.f),2.f+u.profile.movement+u.profile.repetition,2.f+u.profile.dashUse*3.f,2.f+u.profile.confidence*u.profile.perimeter+u.profile.repetition-u.predictionError/150.f,s.phase==4?3.f:-100.f};
    for(int i=0;i<5;++i)familyScore[i]-=u.recentFamilies[i]*3.f;
    Attack chosen=(Attack)(std::max_element(familyScore.begin(),familyScore.end())-familyScore.begin());
    if(s.phase==4&&u.progress.experiences!=3)chosen=Attack::FinalConvergence;
    if(u.forcedAttack>=0)chosen=(Attack)u.forcedAttack;
    float bearing=angleOf(sub(player,s.p))+.48f;
    u.aimTarget=predict(u,player,std::clamp(.6f+length(sub(player,s.p))/210.f,.8f,1.8f));
    if(u.focused)bearing=angleOf(sub(u.aimTarget,s.p));
    if(chosen==Attack::PredictedEscape&&!u.focused)bearing=angleOf(sub(predict(u,player,.6f),s.p))+(u.profile.orbitBias>=0?-.62f:.62f);
    for(int candidate=0;candidate<4;++candidate){
        makeRays(u,s.p,chosen,bearing+candidate*kPi*.5f);
        u.echo={};if(u.mechanic==Mechanic::Echo)captureEcho(u);
        bool safe=false;for(int route=0;route<8&&!safe;++route){
            if(witness(u,player,12,route,false)){safe=true;u.safeRoute=route;u.safeUsesDash=false;}
            else if(witness(u,player,12,route,true)){safe=true;u.safeRoute=route;u.safeUsesDash=true;}}
        if(!safe){++u.rejected;continue;}
        int reserved=0;for(int i=0;i<u.rayCount;++i){u.rays[i].ticket=s.bullets.reserve(s.nextSource++);if(!u.rays[i].ticket)break;++reserved;}
        if(reserved!=u.rayCount){for(int i=0;i<reserved;++i)s.bullets.cancelReservation(u.rays[i].ticket);return false;}
        u.consecutiveRejects=0;u.clock=0;u.planCommitted=true;u.needsStrands=u.mechanic==Mechanic::Sever||u.mechanic==Mechanic::Corrupt;
        u.mechanicExecuted=u.mechanic==Mechanic::Hunt&&u.huntExecuted;
        s.stage=rootwarden::Stage::RootWarning;s.remaining=u.warningTicks;++s.warnings;if(!u.focused&&!u.unique){++u.familyChoices[(int)chosen];for(auto& v:u.recentFamilies)v*=.6f;u.recentFamilies[(int)chosen]+=1;}return true;
    }
    u.echo={};u.rayCount=0;return false;
}
template<class Boss> rootwarden::DamageResult damage(Boss& s,int amount,bool ordinary){
    rootwarden::DamageResult result;auto& u=s.unweaver;
    if(u.progress.defeated||amount<=0||(ordinary&&s.health.silkCooldownTicks>0))return result;
    result.exposed=ordinary&&s.stage==rootwarden::Stage::Recover;
    std::int64_t value=static_cast<std::int64_t>(amount)+(result.exposed?u.recoveryBonus:0);
    if(u.sealTicks>0)value=(value*55+50)/100;
    result.damage=u.progress.credit(static_cast<int>(std::min<std::int64_t>(value,2147483647)));
    s.health.hp=u.progress.vitality();if(!result.damage)return result;
    u.lastDamageOrdinary=ordinary;
    if(ordinary)s.health.silkCooldownTicks=combat::kSilkHitIntervalTicks;
    result.hit=combat::SilkHit::Damaged;
    if(ordinary){++s.metrics.hits;s.metrics.silkDamage+=result.damage;s.metrics.exposedHits+=result.exposed;}
    else{++s.metrics.signatureHits;s.metrics.signatureDamage+=result.damage;}
    return result;
}
template<class Boss> void opening(Boss& s,int ticks,int bonus,int reason){
    if(s.unweaver.formationBreakPending&&reason!=3){ticks=std::max(ticks,300);bonus=std::max(bonus,160);reason=1;}
    s.unweaver.validCharge=false;s.unweaver.formationBreakPending=false;s.unweaver.opening=reason;s.unweaver.recoveryBonus=bonus;s.stage=rootwarden::Stage::Recover;s.remaining=ticks;
}
// Eight reachable local goals, scored at 5 Hz. No hidden player inputs or future retargeting.
template<class Boss> void position(Boss& s,Point player,Arena arena,bool recovering){
 auto& u=s.unweaver;
 if(--u.think<=0){u.think=24;if(s.metrics.ticks%240<24)chooseStyle(u,s.phase);
  const Point toward=unit(sub(player,s.p));const float distance=length(sub(player,s.p));
  const float closing=-(u.playerVelocity.x*toward.x+u.playerVelocity.y*toward.y);
  const Point prediction=predict(u,player,u.tacticTicks>0?.9f:.55f);
  float best=-1e20f;
  for(int i=0;i<8;++i){const Point candidate=clamp(add(s.p,scale(direction(i*kPi/4),110)),arena,82);
   const float desired=!(u.progress.experiences&1)?190.f:u.tacticTicks>0?(u.action==Action::Hunt?0.f:u.action==Action::Bait&&u.tacticTicks>180?200.f:55.f):recovering?40.f:closing>50?185.f:u.style==Style::Predator?85.f:150.f;
   float score=-std::abs(length(sub(candidate,prediction))-desired);
   // Favor cutting across an orbit rather than trailing the player's current location.
   score+=u.profile.perimeter*.18f*(length(sub(s.p,prediction))-length(sub(candidate,prediction)));
   for(int n=0;n<u.silkObstacleCount;++n){const auto& edge=u.silkObstacles[n];
    const float clearance=std::sqrt(foundation::segmentDistanceSquared(candidate,edge[0],edge[1]));
    score-=std::max(0.f,90-clearance)*2.f;
    if(combat::silkContact(s.p,candidate,76,edge[0],edge[1],4))score-=100;
   }
   if(!recovering&&u.tacticTicks==0&&distance<80&&length(sub(candidate,player))<distance)score-=80;
   if(u.action==Action::Flank&&u.tacticTicks>0){const Point side{-toward.y,toward.x};score+=35*(candidate.x*side.x+candidate.y*side.y)*(u.profile.orbitBias>=0?1.f:-1.f)/110.f;}
   if(score>best){best=score;u.goal=candidate;}
  }
 }
 const Point delta=sub(u.goal,s.p);s.p=clamp(add(s.p,scale(unit(delta),std::min(length(delta),155.f/120))),arena,82);
 if(combat::bodyContact(sub(player,scale(u.playerVelocity,1.f/120)),player,12,s.previousP,s.p,32))++u.contactTicks;
}
template<class Boss> Action selectAction(Boss& s,Point player){
 auto& u=s.unweaver;const float d=length(sub(player,s.p));
 const float share=u.activeTicks?float(u.radialTicks)/u.activeTicks:0;
 std::array<float,7> scores{2.f+(share<.2f?3.f:share>.4f?-5.f:0.f),2.8f+u.profile.repetition+(length(sub(predict(u,player,.85f),s.p))<260?1.5f:-3.f),2.f+(d>180?2.f:1.f),2.f+u.profile.perimeter,2.5f+u.profile.dashUse,1.5f+u.profile.offense*.3f,s.phase>=2?2.4f:-100.f};
 for(int i=0;i<7;++i){scores[i]-=u.recentActions[i]*2.8f;u.recentActions[i]*=.65f;}
 const auto a=(Action)(std::max_element(scores.begin(),scores.end())-scores.begin());u.recentActions[(int)a]+=1;++u.actionChoices[(int)a];return a;
}
template<class Boss> bool charge(Boss& s,Point player,Arena arena,bool retreat=false){
 auto& u=s.unweaver;if(s.bullets.flyingCount()||s.bullets.reservedCount())return false;
 u.chargeStart=s.p;const Point target=retreat?add(s.p,scale(unit(sub(s.p,player)),180)):predict(u,player,.85f);const Point delta=sub(target,s.p);
 if(!retreat&&length(delta)>300)return false;
 u.chargeEnd=clamp(add(s.p,scale(unit(delta),std::min(260.f,length(delta)))),arena,82);
 if(length(sub(u.chargeEnd,s.p))<35)return false;
 u.chargePreview=true;u.anchor=s.p;u.rayCount=0;u.commitVelocity=u.playerVelocity;u.sequenceTicks=108;u.warningTicks=66;bool safe=false;
 for(int r=0;r<8&&!safe;++r){if(witness(u,player,12,r,false)){safe=true;u.safeRoute=r;u.safeUsesDash=false;}else if(witness(u,player,12,r,true)){safe=true;u.safeRoute=r;u.safeUsesDash=true;}}
 u.chargePreview=false;if(!safe)return false;
 u.chargeForecast=target;u.chargeIsRetreat=retreat;u.validCharge=true;u.clock=0;u.action=Action::Charge;s.stepStart=s.p;s.stepEnd=u.chargeEnd;s.stage=rootwarden::Stage::StepWarning;s.remaining=66;u.planCommitted=false;u.followup=true;++u.chargeAttempts;return true;
}
template<class Boss> rootwarden::Events tick(Boss& s,Point player,Arena arena){
    rootwarden::Events events;auto& u=s.unweaver;if(u.progress.defeated)return events;
    s.previousP=s.p;++u.stageTicks[(int)s.stage];++s.metrics.ticks;combat::tick(s.health);record(u,player);
    if(s.stage!=rootwarden::Stage::Transition){++u.activeTicks;if(u.planCommitted&&u.sealTicks==0&&!u.focused&&!u.unique&&s.stage!=rootwarden::Stage::Recover)++u.radialTicks;}
    if(u.forecastTicks>0&&--u.forecastTicks==0){const float error=length(sub(player,u.forecast));u.predictionError=.7f*u.predictionError+.3f*error;u.forecastError+=error;u.linearError+=length(sub(player,u.linearForecast));++u.predictions;}
    if(u.forecastTicks==0&&u.historyCount>=25){u.forecast=predict(u,player,.5f);u.linearForecast=clamp(add(player,scale(u.playerVelocity,.5f)),u.playerBounds);u.forecastTicks=60;}
    for(int i=0;i<s.budCount;++i)combat::tick(s.buds[i].health);
    if(s.stage==rootwarden::Stage::Transition){if(--s.remaining<=0){s.stage=rootwarden::Stage::Stalk;s.remaining=72;}return events;}
    u.echo.fresh=false;
    if(u.echo.warning>0){if(--u.echo.warning==0)u.echo.fresh=true;}else if(u.echo.active>0){--u.echo.active;if(u.echo.active==0)u.mechanicExecuted=true;}
    for(auto& strand:u.strands)if(strand.id){
        strand.fresh=false;
        if(strand.warning>0){if(--strand.warning==0){strand.fresh=true;u.mechanicExecuted=true;if(strand.sever)++u.cuts;else ++u.corruptions;}}
        else if(!strand.sever&&strand.active>0)--strand.active;
    }
    if(u.sealTicks>0){
        if(s.stage==rootwarden::Stage::RootWarning&&--s.remaining<=0){s.stage=rootwarden::Stage::RootActive;s.remaining=420;}
        if(s.stage==rootwarden::Stage::RootActive){bool any=false;for(int i=0;i<s.budCount;++i)any|=s.buds[i].health.alive();
            if(!any||--u.sealTicks==0){u.sealBroken=!any;u.sealTicks=0;s.budCount=0;u.mechanicExecuted=true;
                u.progress.experience(u.progress.generation,2);opening(s,!any?300:108,!any?180:50,!any?3:0);}}
        return events;
    }
    if(s.stage==rootwarden::Stage::Recover){
        position(s,player,arena,true);
        if(--s.remaining<=0){s.stage=rootwarden::Stage::Stalk;s.remaining=72;u.planCommitted=false;u.interceptUsed=false;++u.cycles;++s.metrics.cycles;
            if(s.phase>=3&&u.formationEvents<(s.phase-2)*2){const auto& profile=u.profile;
                const auto callerIndex=static_cast<std::size_t>(observation::OffenseTarget::Caller);
                const auto rangedIndex=static_cast<std::size_t>(observation::OffenseTarget::Ranged);
                u.formationRequest=profile.offenseByTarget[callerIndex]>.5f?0:profile.perimeter>.55f||profile.movement>.75f?1:
                    profile.offenseByTarget[rangedIndex]>.5f?2:u.style==Style::Architect?3:u.formationEvents%4;
                ++u.formationEvents;}}
        return events;
    }
    if(s.stage==rootwarden::Stage::StepWarning){++u.clock;if(--s.remaining<=0){s.stage=rootwarden::Stage::StepActive;s.remaining=42;}return events;}
    if(s.stage==rootwarden::Stage::StepActive){
        ++u.clock;
        s.p=rootwarden::interpolate(s.stepStart,s.stepEnd,(43-s.remaining)/42.f);
        if(combat::bodyContact(sub(player,scale(u.playerVelocity,1.f/120)),player,12,s.previousP,s.p,32))++u.contactTicks;
        if(--s.remaining<=0){if(u.validCharge&&!u.chargeIsRetreat){const float error=length(sub(player,u.chargeForecast));if(error<=64)++u.chargePredictionHits;else{++u.chargePredictionMisses;u.recentActions[(int)Action::Charge]+=.35f;}u.predictionError=.7f*u.predictionError+.3f*error;}
            u.validCharge=false;u.huntExecuted=true;if(u.followup){u.followup=false;opening(s,144,170,4);}
            else{s.stage=rootwarden::Stage::Stalk;s.remaining=1;}}
        return events;
    }
    if(s.stage==rootwarden::Stage::Stalk){
        position(s,player,arena,false);
        if(--s.remaining<=0){
            if(u.forcedAttack>=0||u.forcedMechanic>=0||!(u.progress.experiences&1)||(s.phase>=3&&!(u.progress.experiences&2))){u.focused=u.unique=false;u.action=Action::Radial;
                if(s.phase==1&&!u.huntExecuted&&charge(s,player,arena)){u.followup=false;events.warning=true;}
                else if(commit(s,player,arena))events.warning=true;else {s.remaining=48;if(++u.consecutiveRejects>=3&&charge(s,player,arena,true)){u.consecutiveRejects=0;u.followup=false;events.warning=true;}}
            }else if(u.tacticTicks>0){
                if(--u.tacticTicks==0){if(charge(s,player,arena))events.warning=true;else opening(s,108,80,2);}else s.remaining=1;
            }else {
                u.action=selectAction(s,player);u.focused=u.action==Action::Focused;u.unique=u.action==Action::Unique;
                if(u.action==Action::Hunt||u.action==Action::Flank||u.action==Action::Bait){u.tacticTicks=u.action==Action::Hunt?360:240;u.think=0;s.remaining=1;}
                else if(u.action==Action::Charge){if(charge(s,player,arena))events.warning=true;else {u.tacticTicks=180;u.action=Action::Hunt;s.remaining=1;}}
                else if(commit(s,player,arena))events.warning=true;
                else {u.think=0;s.remaining=48;if(++u.consecutiveRejects>=3&&charge(s,player,arena,true)){u.consecutiveRejects=0;u.followup=false;events.warning=true;}}
            }

        }
        return events;
    }
    if(u.planCommitted){
        ++u.clock;--s.remaining;if(u.clock>=u.warningTicks)s.stage=rootwarden::Stage::RootActive;
        for(int i=0;i<u.rayCount;++i){auto& ray=u.rays[i];if(ray.ticket&&u.clock>=ray.fireTick){
            if(s.bullets.launch(ray.ticket,add(ray.origin,scale(ray.direction,40)),ray.direction,{210,5,2200,u.focused?160:300})){++u.shots;++s.shotsFired;u.radialExecuted=!u.focused&&!u.unique;}ray.ticket=0;}}
        if(u.clock>=u.sequenceTicks){
            if(u.radialExecuted)u.progress.experience(u.progress.generation,1);
            if(u.mechanicExecuted)u.progress.experience(u.progress.generation,2);
            u.resultPending=true;++u.attempts[(int)u.style];u.success[(int)u.style]+=u.hitDuringSequence?1.f:0.f;
            for(int i=0;i<3;++i){u.attempts[i]*=.92f;u.success[i]*=.92f;}
            if(u.hitDuringSequence&&charge(s,player,arena))events.warning=true;
            else opening(s,216,130,2);
        }
    }
    return events;
}
template<class Boss> rootwarden::Events finish(Boss& s){
    rootwarden::Events e;auto& u=s.unweaver;if(s.stage==rootwarden::Stage::Transition)return e;
    const auto boundary=u.progress.finishTick();if(boundary==Progress::Boundary::None)return e;
    clear(u);s.bullets.clear();s.fields.clear();s.budCount=0;s.phase=u.progress.phase;
    if(boundary==Progress::Boundary::Defeat){s.stage=rootwarden::Stage::Defeated;e.defeated=true;}
    else{s.stage=rootwarden::Stage::Transition;s.remaining=s.phase==4?180:108;e.phaseChanged=true;}
    return e;
}
template<class Boss> int hostileDamage(const Boss& s,Point previous,Point player,float radius){
    if(s.stage==rootwarden::Stage::Transition||s.unweaver.progress.defeated)return 0;
    const auto& u=s.unweaver;int result=0;
    if(combat::bodyContact(previous,player,radius,s.previousP,s.p,32))result=2400;
    if(u.echo.warning==0&&u.echo.active>0&&!u.echo.fresh)for(int i=1;i<u.echo.count;++i)
        if(combat::silkContact(previous,player,radius,u.echo.path[i-1],u.echo.path[i],8))result=std::max(result,2200);
    for(const auto& strand:u.strands)if(strand.id&&!strand.sever&&strand.warning==0&&strand.active>0&&!strand.fresh&&combat::silkContact(previous,player,radius,strand.a,strand.b,7))result=std::max(result,1800);
    return result;
}
}
