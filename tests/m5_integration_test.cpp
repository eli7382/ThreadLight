#include "attack_coordinator.h"
#include "chapter_boss.h"
#include "chapter_one.h"
#include "crossweave.h"
#include "game_timing.h"
#include "signature_reward.h"
#include "reward_ledger.h"
#include "skitter.h"
#include <array>
#include <algorithm>
#include <iostream>
#include <vector>
#include <chrono>
#include <string_view>

namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';}}while(false)
namespace b=threadlight::chapter_boss;namespace f=threadlight::foundation;
namespace r=threadlight::ranged;namespace k=threadlight::skitter;
namespace q=threadlight::attacks;namespace c=threadlight::crossweave;
namespace sig=threadlight::signature_reward;

void targetBudgetAndResizeContact(){
    std::array<c::Target,10> targets{};
    for(int i=0;i<8;++i)targets[i]={static_cast<std::uint64_t>(8-i),{10,0},0,i,0};
    targets[8]={100,{200,0},1,8,0};targets[9]=targets[0];
    auto selected=c::selectTargets(targets,{0,0});CHECK(selected.count==6);
    for(int i=0;i<6;++i)CHECK(selected.items[i].id==static_cast<std::uint64_t>(i+1));
    std::reverse(targets.begin(),targets.end());auto reversed=c::selectTargets(targets,{0,0});
    CHECK(reversed.count==6);for(int i=0;i<6;++i)CHECK(selected.items[i].id==reversed.items[i].id);
    c::State state;state.travelDistance=160;
    std::array<c::Segment,1> silk{{{1,{190,200},{210,200},100}}};
    CHECK(!c::tick(state,{200,197.875f},{200,200},silk,true,true).triggered);
    CHECK(state.pendingCount==1);
    // The production resize block maps pending geometry with the silk/player.
    state.pendingCrossings[0].lastOnLine={400,400};
    silk[0].a={380,400};silk[0].b={420,400};
    auto pulse=c::tick(state,{400,400},{400,402.125f},silk,true,true);
    CHECK(pulse.triggered&&pulse.center.x==400&&pulse.center.y==400);
    state={};state.travelDistance=160;silk[0]={2,{30,100},{30,300},100};
    CHECK(!c::tick(state,{27.875f,200},{30,200},silk,true,true).triggered);
    state.pendingCrossings[0].lastOnLine={15,200};silk[0].a.x=silk[0].b.x=15;
    CHECK(!c::tick(state,{22,200},{22,200},silk,true,true).triggered);
    CHECK(state.pendingCount==0); // Clamp cannot manufacture an exit pulse.
}

void rewardContinuationAndCeiling(){
    f::RewardLedger ledger;sig::RewardState first,second;
    const auto boss1=ledger.registerEnemy(10,250);CHECK(ledger.defeatEnemy(boss1));
    CHECK(sig::queue(first,boss1,true));CHECK(sig::accept(first,f::Phase::PendingRewards,true,0));
    CHECK(ledger.remainder()==0); // Missing core cannot block a signature.
    const auto boss2=ledger.registerEnemy(10,250);CHECK(ledger.defeatEnemy(boss2));
    CHECK(sig::queue(second,boss2,true));CHECK(ledger.collectPickup(boss1));CHECK(ledger.collectPickup(boss2));
    CHECK(ledger.pendingCredits()==1&&!sig::accept(second,f::Phase::PendingRewards,true,ledger.pendingCredits()));
    CHECK(ledger.resolveCredit(ledger.nextCredit(),f::Phase::PendingRewards));
    CHECK(sig::accept(second,f::Phase::PendingRewards,true,0));
    CHECK(first.owned&&second.owned&&!sig::queue(second,boss2,true));
    f::RewardLedger ceiling;int value=0;
    for(int wave=0;wave<21;++wave){auto schedule=threadlight::chapter::schedule(wave,7382);
        for(int i=0;i<schedule.count;++i)value+=threadlight::combat::definition(schedule.items[i].kind,threadlight::chapter::encounter(wave).chapter).sparkValue;
    }
    CHECK(value==642);value+=4+4+8+30;CHECK(value==688);
    auto id=ceiling.registerEnemy(value,0);CHECK(ceiling.defeatEnemy(id));CHECK(ceiling.collectPickup(id));
    CHECK(ceiling.counters().creditsIssued==15&&ceiling.remainder()==22);
    CHECK(ceiling.pendingCredits()==15); // Final transition does not consume them.
    b::State dancer,conductor;dancer.kind=b::Kind::Dancer;conductor.kind=b::Kind::Conductor;
    for(int i=0;i<4;++i)CHECK(b::supportKind(dancer,i)==threadlight::combat::EnemyKind::Skitter);
    for(int i=0;i<6;++i)CHECK(b::supportKind(conductor,i)==(i<4?threadlight::combat::EnemyKind::Hunter:threadlight::combat::EnemyKind::Spitter));
}

struct ScheduleResult{std::vector<int> bossWarnings,supportWarnings;std::array<int,2> launches{};friend bool operator==(const ScheduleResult&,const ScheduleResult&)=default;};
ScheduleResult coordinated(b::Kind kind,double fps){
    b::State boss;b::start(boss,{600,400},{0,1280,0,800},kind);boss.phase=3;
    std::array<k::State,2> skitters{};std::array<r::State,2> spitters{};
    std::array<f::Point,2> p{{{100,200},{1100,600}}},v{};std::array<std::uint64_t,2> tickets{};
    for(auto& s:skitters)s.ticksRemaining=1;
    for(auto& s:spitters)s.remaining=1;
    r::Pool pool;q::Coordinator coordinator;threadlight::timing::FixedStepClock clock;
    ScheduleResult result;int tick=0;
    for(int frame=0;tick<6000;++frame){(void)frame;const int steps=clock.advance(1/fps);
        for(int sub=0;sub<steps&&tick<6000;++sub){++tick;int committed=0;std::array<q::Request,2> ready{};int count=0;
            for(int i=0;i<2;++i){
                if(kind==b::Kind::Dancer){committed+=k::committed(skitters[i]);if(skitters[i].phase==k::Phase::Flank&&skitters[i].ticksRemaining<=1)ready[count++]={static_cast<std::uint64_t>(i+1),q::Kind::Skitter};}
                else{committed+=r::committed(spitters[i]);if(r::wantsCommit(spitters[i]))ready[count++]={static_cast<std::uint64_t>(i+1),q::Kind::Spitter};}
            }
            b::waitForSupports(boss,committed,pool.flyingCount());
            const auto events=b::tick(boss,{850,400},{0,1280,0,800});if(events.warning)result.bossWarnings.push_back(tick);
            const auto grants=coordinator.choose(std::span<const q::Request>(ready.data(),count),committed+b::supportSlots(boss),static_cast<int>(pool.freeSlots()),tick);
            int after=0;
            for(int i=0;i<2;++i){
                if(kind==b::Kind::Dancer){auto event=k::tick(skitters[i],p[i],v[i],{850,400},{0,0},{11,1269,11,789},grants.contains(i+1),125);
                    if(event.telegraphStarted){result.supportWarnings.push_back(tick);++result.launches[i];}after+=k::committed(skitters[i]);
                }else{
                    if(grants.contains(i+1))tickets[i]=pool.reserve(i+1);
                    auto event=r::tick(spitters[i],p[i],v[i],{850,400},{15,1265,15,785},grants.contains(i+1)&&tickets[i]);
                    if(event.warningStarted)result.supportWarnings.push_back(tick);
                    if(event.shotReady){CHECK(pool.launch(tickets[i],r::muzzle(p[i],spitters[i].aimDirection),spitters[i].aimDirection));++result.launches[i];tickets[i]=0;}
                    after+=r::committed(spitters[i]);
                }
            }
            const bool bossAttack=kind==b::Kind::Dancer?(boss.stage==b::Stage::StepWarning||boss.stage==b::Stage::StepActive||boss.counterstepping):(boss.stage==b::Stage::RootWarning||boss.stage==b::Stage::RootActive);
            if(bossAttack)CHECK(after<=(kind==b::Kind::Dancer?1:0));
            pool.tick({850,400},{850,400},12,{0,1280,0,800});b::projectileTick(boss,{850,400},{850,400},12,{0,1280,0,800});
        }
    }
    CHECK(boss.metrics.cycles>=3);CHECK(result.launches[0]>0&&result.launches[1]>0);
    return result;
}
void boundedHelperStress(){
    std::array<c::Segment,520> silk{};c::State state;
    for(size_t i=0;i<silk.size();++i){silk[i]={i+1,{0,-30},{0,30},100};state.consumedIds[i]=i+1;}
    state.consumedCount=silk.size();std::uint64_t checksum=0;std::array<double,1000> micros{};
    for(int tick=0;tick<1000;++tick){const auto start=std::chrono::steady_clock::now();
        const float x=(tick%37-18)*.125f;
        c::tick(state,{x,-1},{x,1},silk,true,true);
        for(int enemy=0;enemy<37;++enemy)for(const auto& segment:silk)
            checksum+=threadlight::combat::silkContact({x+enemy*.1f,-2},{x+enemy*.1f,2},23,segment.a,segment.b,11);
        micros[tick]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    }
    double sum=0;for(double value:micros)sum+=value;std::sort(micros.begin(),micros.end());
    std::cout<<"synthetic_helper_stress steps=1000 silk=520 enemies=37 mean_us="<<sum/1000
             <<" p95_us="<<micros[950]<<" max_us="<<micros.back()<<" checksum="<<checksum<<'\n';
}
}
int main(int argc,char** argv){
    if(argc==2&&std::string_view(argv[1])=="--perf"){boundedHelperStress();return 0;}
    targetBudgetAndResizeContact();rewardContinuationAndCeiling();
    for(auto kind:{b::Kind::Dancer,b::Kind::Conductor})CHECK(coordinated(kind,30)==coordinated(kind,144));
    if(failures)return 1;
    std::cout<<"M5 integration checks passed\n";return 0;
}
