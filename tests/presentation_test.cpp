#include "game_presentation.h"
#include "game_timing.h"
#include "silk_collision.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
int failures=0;
void check(bool condition,const char* expression,int line) {
    if(!condition){std::cerr<<"FAIL line "<<line<<": "<<expression<<'\n';++failures;}
}
#define CHECK(expression) check((expression),#expression,__LINE__)
using namespace threadlight::presentation;
using threadlight::timing::FixedStepClock;
constexpr float dt=float(threadlight::timing::kFixedStepSeconds);
bool near(float a,float b){return std::abs(a-b)<.003f;}

void testFramePresentationCadence() {
    for(double fps : {30.,60.,120.,144.,165.,1000.,10000.}) {
        FixedStepClock clock;
        V player{},previous{};
        Foe foe{{0,0},{},14,{100,100},95,0,0,threadlight::combat::EnemyKind::Hunter};
        Particle<int> particle{{0,0},{},1,1,2,0};
        Spark spark{{0,0},0};
        DeathEffect effect{};
        float last=0;
        int ticks=0;
        for(int frame=1;frame<=int(fps*2);++frame) {
            const int steps=clock.advance(1./fps);
            for(int tick=0;tick<steps;++tick) {
                previous=player;foe.capture();particle.capture();spark.capture();effect.capture();
                player.x+=255*dt;foe.p.x+=95*dt;foe.phase+=dt;
                particle.p.x+=100*dt;particle.life-=dt;
                spark.p.x+=220*dt;spark.t+=dt;effect.age+=dt;
                ++ticks;
            }
            const float alpha=clock.presentationAlpha();
            const float shown=blend(previous,player,alpha).x;
            const float shownTime=std::max(0.,frame/fps-double(dt));
            CHECK(near(shown,255*shownTime));
            CHECK(near(foe.present(alpha).p.x,95*shownTime));
            CHECK(near(foe.present(alpha).phase,shownTime));
            CHECK(near(particle.present(alpha).p.x,100*shownTime));
            CHECK(near(spark.present(alpha).p.x,220*shownTime));
            CHECK(near(effect.present(alpha).age,shownTime));
            CHECK(shown>=last);
            if(frame>2&&ticks>1)CHECK(near(shown-last,float(255/fps)));
            last=shown;
        }
        CHECK(ticks==240);
        CHECK(near(player.x,510)); // Sampling did not mutate the simulation.
    }
}

void testSpawnRemovalEffectsAndSilk() {
    Foe first{{10,20},{},14,{100,100},95,3,0,threadlight::combat::EnemyKind::Hunter};
    Foe second{{500,600},{},20,{300,300},66,4,0,threadlight::combat::EnemyKind::Brute};
    CHECK(second.present(0).p.x==500); // New spawn never blends from origin.
    std::vector<Foe> foes{first,second};
    for(auto& foe:foes){foe.capture();foe.p.x+=10;foe.phase+=dt;}
    foes.erase(foes.begin());
    CHECK(foes[0].present(.5f).p.x==505); // Compaction keeps the correct history.
    CHECK(foes[0].p.x==510);
    foes[0].previousHitFlash=.1f;foes[0].hitFlash=.16f;
    const auto shown=foes[0].present(.5f);
    CHECK(shown.p.x==505 && near(shown.phase,4+dt*.5f));
    CHECK(shown.hitFlash==.16f && foes[0].p.x==510); // Immediate hit impulse; no simulation mutation.
    CHECK(sizeof(FoeView)<sizeof(Foe));
    Particle<int> particle{{100,200},{},.65f,.65f,3,0};
    particle.capture();particle.p.x=102;particle.life-=dt;
    CHECK(particle.present(.5f).p.x==101);
    CHECK(near(particle.present(.5f).life,.65f-dt*.5f));
    DeathEffect effect{};effect.p={300,200};effect.capture();effect.age=dt;
    CHECK(near(effect.present(.5f).age,dt*.5f));
    CHECK(decay(0,.18f,0)==.18f); // Camera/impact impulses remain immediate.
    CHECK(near(decay(.18f,.18f-dt,.5f),.18f-dt*.5f));
    Thread knot{{110,200},0};knot.age=dt;
    CHECK(silkPoint(knot,true,{105,200}).x==105); // New silk cannot lead its player.
    CHECK(knot.p.x==110); // Damage geometry stays authoritative.
    knot.capture();knot.age+=dt;
    CHECK(silkPoint(knot,true,{115,200}).x==110); // Historical knots stay fixed.
}

void testDiscontinuitiesAndBoundedSampling() {
    // These boundaries all capture the new current state and clear the clock.
    for(int boundary=0;boundary<6;++boundary) { // pause/resume/evolve/restart/resize/end
        FixedStepClock clock;clock.advance(.003);
        Foe foe{{10,20},{},14,{100,100},95,0,0,threadlight::combat::EnemyKind::Hunter};
        foe.capture();foe.p={900,700};foe.phase=5;
        clock.clear();foe.capture();
        for(float alpha : {0.f,.3f,1.f}) {
            CHECK(foe.present(alpha).p.x==900);
            CHECK(foe.present(alpha).phase==5);
        }
        CHECK(clock.presentationAlpha()==0);
    }
    CHECK(blend(10.f,20.f,-1.f)==10);
    CHECK(blend(10.f,20.f,2.f)==20);
    FixedStepClock clock;
    CHECK(clock.advance(10)==threadlight::timing::kMaxStepsPerFrame);
    CHECK(clock.presentationAlpha()>=0&&clock.presentationAlpha()<=1);
}

void testActiveSilkAndCooldownEquivalence() {
    namespace combat=threadlight::combat;
    using threadlight::foundation::Point;
    auto first=[](const std::vector<Thread>& knots,float lifetime){
        return threadlight::silk::firstActiveSegment(std::span<const Thread>(knots),lifetime);
    };
    std::vector<Thread> empty;
    CHECK(first(empty,1.2f)==0);
    empty.push_back({{0,0},0});CHECK(first(empty,1.2f)==1);
    std::vector<Thread> knots{{{0,0},2.2f},{{10,0},1.3f},{{20,0},1.2f},{{30,0},.1f}};
    CHECK(first(knots,1.2f)==2); // Equality is active; segment still starts at knot 1.
    CHECK(first(knots,0.f)==knots.size());
    CHECK(first(knots,3.f)==1);
    std::vector<Thread> evolving;
    for(int tick=0;tick<1200;++tick){
        if(tick%3==0)evolving.push_back({{float(tick),0},0});
        for(auto& knot:evolving)knot.age+=dt;
        while(!evolving.empty()&&evolving.front().age>5.5f)evolving.erase(evolving.begin());
        for(float lifetime : {1.2f,1.5f,1.8f,2.1f}){
            std::size_t reference=evolving.size();
            for(std::size_t i=1;i<evolving.size();++i)if(evolving[i].age<=lifetime){reference=i;break;}
            CHECK(first(evolving,lifetime)==reference);
        }
    }
    combat::EnemyHealth oldHealth{10000,10000},newHealth=oldHealth;
    for(int tick=0;tick<300;++tick){
        combat::tick(oldHealth);combat::tick(newHealth);
        const Point from{float((tick%50)-10),-2},to{from.x+3,2};
        const float lifetime=tick%2?1.2f:2.1f;
        combat::SilkHit oldHit=combat::SilkHit::None,newHit=oldHit;
        std::size_t oldIndex=knots.size(),newIndex=knots.size();
        for(std::size_t i=1;i<knots.size();++i){
            if(knots[i].age>lifetime)continue;
            if(!combat::silkContact(from,to,2,{knots[i-1].p.x,knots[i-1].p.y},{knots[i].p.x,knots[i].p.y},1))continue;
            oldHit=combat::applySilk(oldHealth,100);oldIndex=i;break;
        }
        if(newHealth.silkCooldownTicks==0)for(std::size_t i=first(knots,lifetime);i<knots.size();++i){
            if(!combat::silkContact(from,to,2,{knots[i-1].p.x,knots[i-1].p.y},{knots[i].p.x,knots[i].p.y},1))continue;
            newHit=combat::applySilk(newHealth,100);newIndex=i;break;
        }
        CHECK(oldHit==newHit);
        if(oldHit!=combat::SilkHit::None)CHECK(oldIndex==newIndex);
        CHECK(oldHealth.hp==newHealth.hp && oldHealth.silkCooldownTicks==newHealth.silkCooldownTicks);
    }
}

void testKnotSamplingBoundary(){
    const V origin{0,0};
    const float infinity=std::numeric_limits<float>::infinity();
    const float nan=std::numeric_limits<float>::quiet_NaN();
    auto old=[](V a,V b){const auto d=a-b;return std::sqrt(d.x*d.x+d.y*d.y)>9.f;};
    for(float x : {0.f,8.99f,std::nextafter(9.f,0.f),9.f,std::nextafter(9.f,infinity),9.01f,infinity,nan})
        for(float y : {0.f,.001f,1.f,5.f})
            CHECK(threadlight::silk::shouldAppendKnot(origin,V{x,y})==old(origin,V{x,y}));
    for(int i=-1000;i<=1000;++i){
        V point{9.f+i*.000001f,.002f};
        CHECK(threadlight::silk::shouldAppendKnot(origin,point)==old(origin,point));
    }
}
}
int main() {
    testFramePresentationCadence();
    testSpawnRemovalEffectsAndSilk();
    testDiscontinuitiesAndBoundedSampling();
    testActiveSilkAndCooldownEquivalence();
    testKnotSamplingBoundary();
    if(failures)return EXIT_FAILURE;
    std::cout<<"Presentation cadence, entity history, and discontinuity checks passed.\n";
}
