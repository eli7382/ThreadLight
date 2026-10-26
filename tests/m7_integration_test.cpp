#include "needle_interception.h"
#include "silk_utilities.h"
#include "signature_effects.h"
#include "signature_catalog.h"
#include "player_upgrades.h"
#include "game_timing.h"
#include "reward_ledger.h"
#include <array>
#include <chrono>
#include <iostream>
#include <string_view>

namespace n=threadlight::needle_interception;
namespace r=threadlight::ranged;
namespace c=threadlight::combat;
namespace s=threadlight::silk_utilities;
namespace e=threadlight::signature_effects;
namespace u=threadlight::upgrades;
namespace f=threadlight::foundation;
int failures=0,checks=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::cerr<<"M7 integration line "<<__LINE__<<": "<<#x<<'\n';}}while(false)
constexpr r::Bounds arena{-500,500,-500,500};
r::Pool::Ticket shot(r::Pool& pool,f::Point origin,f::Point direction,float speed=1200,float radius=1){
    const auto id=pool.reserve(1);CHECK(id!=0);CHECK(pool.launch(id,origin,direction,{speed,radius,1400,360}));return id;
}
void needleChronologyAndSharedBudget(){
    std::array<n::Segment,1> silk{{{{0,-50},{0,50}}}};
    r::Pool ordinary,boss;
    const auto ordinaryId=shot(ordinary,{-10,0},{1,0});
    const auto bossId=shot(boss,{-5,20},{1,0});
    auto hit=n::find(ordinary,boss,silk,1,{100,0},{100,0},12,arena);
    CHECK(hit.id==bossId&&hit.pool==1); // Two pools reuse ticket 1; chronology wins.
    CHECK(boss.intercept(hit.id));CHECK(!boss.intercept(hit.id));
    s::NeedlecatchState state;CHECK(s::trigger(state,1));
    CHECK(!s::ready(state,1));CHECK(ordinary.flyingCount()==1&&ordinary.items()[0].id==ordinaryId);
    CHECK(boss.flyingCount()==0);CHECK(!s::trigger(state,1));
    ordinary.clear();boss.clear();
    shot(ordinary,{-10,0},{1,0});
    CHECK(n::find(ordinary,boss,silk,1,{-8,0},{-8,0},1,arena).id==0); // Player first.
    CHECK(ordinary.tick({-8,0},{-8,0},1,arena).count==1);
    shot(ordinary,{-10,0},{1,0});
    CHECK(n::find(ordinary,boss,silk,1,{0,0},{0,0},1,arena).id==0); // Exact tie: physical impact wins.
    CHECK(ordinary.tick({0,0},{0,0},1,arena).count==1);
    const auto reserved=ordinary.reserve(9);CHECK(reserved!=0);
    CHECK(n::find(ordinary,boss,silk,1,{100,0},{100,0},12,arena).id==0);
    CHECK(!ordinary.intercept(reserved));CHECK(ordinary.reservedCount()==1);
}
void oldSilkOnlyAndGrazing(){
    r::Pool ordinary,boss;
    shot(ordinary,{0,21},{0,-1},180,5);
    std::array<n::Segment,1> past{{{{-20,0},{0,0}}}};
    CHECK(n::find(ordinary,boss,past,5,{0,0},{0,12},12,arena).id==0);
    const std::array<n::Segment,1> future{{{{0,0},{0,12}}}};
    CHECK(n::find(ordinary,boss,future,5,{0,0},{0,12},12,arena).id!=0); // Control: future silk WOULD wrongly shield this hit.
    CHECK(ordinary.tick({0,0},{0,12},12,arena).count==1);
    // The new (0,0)->(0,12) strand is deliberately NOT in the historical span.
    ordinary.clear();
    const auto id=shot(ordinary,{-10,2},{1,0});
    std::array<n::Segment,1> point{{{{0,0},{0,0}}}};
    const auto tangent=n::find(ordinary,boss,point,1,{100,100},{100,100},12,arena);
    CHECK(tangent.id==id&&std::abs(tangent.time-1.)<1e-8);
    CHECK(ordinary.intercept(tangent.id));
    CHECK(ordinary.tick({100,100},{100,100},12,arena).count==0);
    CHECK(n::find(ordinary,boss,std::span<const n::Segment>{},5,{0,0},{0,0},12,arena).id==0);
}
void sharedDamageAndOrdinaryKillOwnership(){
    c::PlayerHealth health{10000,10000,0};c::IncomingHit hit;hit.consider(1400,1);int barrier=1200;
    CHECK(c::applyIncoming(health,hit,false,&barrier,24)==0);CHECK(barrier==136&&health.hp==10000&&health.immunityTicks==0);
    CHECK(c::applyIncoming(health,hit,false,&barrier,24)==928);CHECK(barrier==0&&health.hp==9072);
    barrier=1200;const int immunity=health.immunityTicks;
    CHECK(c::applyIncoming(health,hit,true,&barrier,24)==0);CHECK(barrier==1200&&health.immunityTicks==immunity);
    s::BloodstitchState blood;f::RewardLedger ledger;health={5000,10000,0};
    for(int i=0;i<8;++i){
        c::EnemyHealth enemy{100,100};const auto id=ledger.registerEnemy(1,10);
        const bool ordinary=i<5;
        const auto result=ordinary?c::applySilk(enemy,100):c::applyDamage(enemy,100);
        CHECK(result==c::SilkHit::Killed);
        if(ledger.defeatEnemy(id))s::onOrdinaryKill(blood,1,health,ordinary);
        CHECK(!ledger.defeatEnemy(id)); // No second heal call from rejected death.
    }
    const auto timeout=ledger.registerEnemy(1,10);CHECK(ledger.despawnEnemy(timeout));
    CHECK(!ledger.defeatEnemy(timeout));CHECK(health.hp==5300&&blood.healedCenti==300&&blood.kills==0);
    e::SlowState slow;e::onOrdinaryHit(slow,true);int slowMoves=0;
    for(int i=0;i<100;++i){const float factor=e::factor(slow,false);e::tick(slow,true);slowMoves+=factor<1;}
    CHECK(slowMoves==90); // Main snapshots factor before decrement, then moves.
}
struct Outcome {int hp=0,catches=0,healed=0;std::uint64_t killed=0;bool operator==(const Outcome&)const=default;};
Outcome scenario(int fps){
    threadlight::timing::FixedStepClock clock;r::Pool ordinary,boss;s::NeedlecatchState needle;s::BloodstitchState blood;
    c::PlayerHealth health{10000,10000,0};f::RewardLedger ledger;Outcome out;int tick=0;
    const std::array<n::Segment,1> silk{{{{0,-100},{0,100}}}};
    while(tick<2400){
        const int steps=clock.advance(1./fps);
        for(int step=0;step<steps&&tick<2400;++step,++tick){
            c::tick(health);s::tick(needle,true);
            if(tick%240==0){shot(ordinary,{-20,0},{1,0},600);shot(boss,{-15,35},{1,0},600);}
            if(s::ready(needle,3)){
                const auto hit=n::find(ordinary,boss,silk,5,{100,0},{100,0},12,arena);
                if(hit.id&&(hit.pool?boss:ordinary).intercept(hit.id)){CHECK(s::trigger(needle,3));++out.catches;}
            }
            c::IncomingHit incoming;
            for(auto* pool:{&ordinary,&boss}){const auto impacts=pool->tick({100,0},{100,0},12,arena);for(int i=0;i<impacts.count;++i)incoming.consider(impacts.items[i].damage,impacts.items[i].sourceId);}
            c::applyIncoming(health,incoming,false,nullptr,24);
            if(tick%70==0){const auto id=ledger.registerEnemy(1,10);if(ledger.defeatEnemy(id))out.healed+=s::onOrdinaryKill(blood,3,health);}
        }
    }
    out.hp=health.hp;out.killed=ledger.counters().killed;return out;
}
void stress(){
    r::Pool ordinary,boss;
    for(int i=0;i<64;++i){const auto a=ordinary.reserve(i+1),b=boss.reserve(i+101);ordinary.launch(a,{-12.f,i*2.f},{1,0});boss.launch(b,{-10.f,i*2.f},{1,0});}
    std::array<n::Segment,520> silk{};for(size_t i=0;i<silk.size();++i)silk[i]={{0.f,-250.f+i},{0.f,-249.f+i}};
    std::uint64_t checksum=0;std::array<double,1000> micros{};
    for(int i=0;i<1000;++i){const auto start=std::chrono::steady_clock::now();const auto hit=n::find(ordinary,boss,silk,11,{300,300},{300,300},12,arena);checksum+=hit.id;micros[i]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();}
    std::sort(micros.begin(),micros.end());double sum=0;for(double t:micros)sum+=t;
    std::cout<<"synthetic_needle_stress projectiles=128 strands=520 mean_us="<<sum/1000<<" p95_us="<<micros[950]<<" max_us="<<micros.back()<<" checksum="<<checksum<<'\n';
}
int main(int argc,char** argv){
    if(argc==2&&std::string_view(argv[1])=="--perf"){stress();return 0;}
    needleChronologyAndSharedBudget();oldSilkOnlyAndGrazing();sharedDamageAndOrdinaryKillOwnership();
    const auto reference=scenario(30);for(int fps:{60,120,144,240})CHECK(scenario(fps)==reference);
    CHECK(reference.catches>0&&reference.healed<=1500&&reference.killed>0);
    if(failures)return 1;
    std::cout<<"M7 integration checks passed: "<<checks<<" checks\n";
}
