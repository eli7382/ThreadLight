#include "game_presentation.h"
#include "game_timing.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
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
        Foe foe{{0,0},{},14,1,1,95,0,0,0,0};
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
    Foe first{{10,20},{},14,1,1,95,0,3,0,0};
    Foe second{{500,600},{},20,3,3,66,0,4,0,1};
    CHECK(second.present(0).p.x==500); // New spawn never blends from origin.
    std::vector<Foe> foes{first,second};
    for(auto& foe:foes){foe.capture();foe.p.x+=10;foe.phase+=dt;}
    foes.erase(foes.begin());
    CHECK(foes[0].present(.5f).p.x==505); // Compaction keeps the correct history.
    CHECK(foes[0].p.x==510);
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
        Foe foe{{10,20},{},14,1,1,95,0,0,0,0};
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
}
int main() {
    testFramePresentationCadence();
    testSpawnRemovalEffectsAndSilk();
    testDiscontinuitiesAndBoundedSampling();
    if(failures)return EXIT_FAILURE;
    std::cout<<"Presentation cadence, entity history, and discontinuity checks passed.\n";
}
