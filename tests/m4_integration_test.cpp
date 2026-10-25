#include "afterimage.h"
#include "attack_coordinator.h"
#include "chapter_one.h"
#include "combat.h"
#include "game_logic.h"
#include "game_timing.h"
#include "ranged_combat.h"
#include "reward_ledger.h"
#include "rootwarden.h"
#include "signature_reward.h"
#include "skitter.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures=0;
#define CHECK(x) do{if(!(x)){++failures;std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';}}while(false)
namespace c=threadlight::combat;namespace f=threadlight::foundation;
namespace a=threadlight::afterimage;namespace q=threadlight::attacks;
namespace r=threadlight::ranged;namespace k=threadlight::skitter;
namespace sig=threadlight::signature_reward;namespace boss=threadlight::rootwarden;

void chapterBudgetsAndStats(){
    using namespace threadlight;
    CHECK(chapter::kEncounterCount==21);
    int sparks=128+4+10;
    const int expected[2][4]={{18,2,4,2},{8,2,8,8}};
    for(int wave=7;wave<=8;++wave){
        const auto definition=chapter::encounter(wave),again=chapter::encounter(wave);
        CHECK(definition.chapter==2&&!definition.boss);
        CHECK(definition.seconds==(wave==7?26:28));
        const auto schedule=chapter::schedule(wave,7382),repeat=chapter::schedule(wave,7382);
        std::array<int,4> count{};int previous=-1;
        CHECK(schedule.count==26&&repeat.count==26);
        for(int i=0;i<schedule.count;++i){
            const auto& entry=schedule.items[i];++count[static_cast<int>(entry.kind)];
            CHECK(entry.tick>=previous&&entry.tick<definition.seconds*120);
            CHECK(entry.tick==repeat.items[i].tick&&entry.angle==repeat.items[i].angle);
            CHECK(std::isfinite(entry.angle));previous=entry.tick;
            sparks+=c::definition(entry.kind,again.chapter).sparkValue;
        }
        for(int i=0;i<4;++i)CHECK(count[i]==expected[wave-7][i]);
    }
    CHECK(sparks==208);
    CHECK(c::definition(c::EnemyKind::Hunter).hp==100&&c::definition(c::EnemyKind::Hunter,2).hp==200);
    CHECK(c::definition(c::EnemyKind::Brute).hp==300&&c::definition(c::EnemyKind::Brute,2).hp==450);
    CHECK(c::definition(c::EnemyKind::Skitter).speed==115&&c::definition(c::EnemyKind::Skitter,2).speed==125);
    CHECK(c::definition(c::EnemyKind::Spitter,2).contactDamage==1200);
    f::RewardLedger ledger;const auto id=ledger.registerEnemy(sparks,0);ledger.defeatEnemy(id);ledger.collectPickup(id);
    CHECK(ledger.counters().creditsIssued==5&&ledger.remainder()==22);
}

void echoDamageAndSharedOwners(){
    c::EnemyHealth target{200,200};
    CHECK(c::applySilk(target,100)==c::SilkHit::Damaged);
    const auto gate=target.silkCooldownTicks;
    CHECK(c::applyDamage(target,a::damage(100))==c::SilkHit::Damaged);
    CHECK(target.hp==25&&target.silkCooldownTicks==gate);
    CHECK(c::applySilk(target,100)==c::SilkHit::None);
    f::RewardLedger ledger;const auto id=ledger.registerEnemy(1,10);
    CHECK(c::applyDamage(target,a::damage(100))==c::SilkHit::Killed);
    CHECK(ledger.defeatEnemy(id).has_value());
    CHECK(c::applyDamage(target,100)==c::SilkHit::None&&!ledger.defeatEnemy(id));
    boss::State b;boss::start(b,{500,400},{0,1280,76,748});b.stage=boss::Stage::Recover;
    CHECK(boss::applySilk(b,100).damage==200);
    CHECK(boss::applySignature(b,a::damage(100,true)).damage==38);
    CHECK(b.health.hp==6762&&b.health.silkCooldownTicks==33);
    CHECK(b.metrics.hits==1&&b.metrics.signatureHits==1&&b.metrics.signatureDamage==38);
    r::Pool pool;const auto ticket=pool.reserve(1);CHECK(pool.launch(ticket,{0,0},{1,0}));
    const auto impacts=pool.tick({1.5f,0},{1.5f,0},12,{-100,100,-100,100});
    CHECK(impacts.count==1&&pool.flyingCount()==0);
    c::PlayerHealth player;player.immunityTicks=20;c::IncomingHit incoming;
    incoming.consider(impacts.items[0].damage,impacts.items[0].sourceId);
    CHECK(c::applyIncoming(player,incoming,false)==0&&player.hp==10000);
    CHECK(pool.tick({1.5f,0},{1.5f,0},12,{-100,100,-100,100}).count==0);
}

void rewardBoundary(){
    f::RewardLedger ledger;sig::RewardState reward;
    const auto normal=ledger.registerEnemy(128,0);ledger.defeatEnemy(normal);ledger.collectPickup(normal);
    CHECK(ledger.pendingCredits()==3&&ledger.remainder()==38);
    for(int i=0;i<3;++i)CHECK(ledger.resolveCredit(ledger.nextCredit(),f::Phase::PendingRewards));
    const auto bossId=ledger.registerEnemy(10,250);CHECK(ledger.defeatEnemy(bossId));
    CHECK(sig::queue(reward,bossId,true));
    f::WaveLifecycle phase;phase.start({1,true,0});phase.tick(0,true);
    for(int i=0;i<600;++i)phase.tick(0,false,false,true);
    CHECK(phase.phase()==f::Phase::PendingRewards&&ledger.remainder()==38);
    // Core contact is optional for the signature. Credit spending, if a pickup
    // issued one, is mandatory before accepting the separate signature reward.
    CHECK(ledger.collectPickup(bossId));
    CHECK(ledger.pendingCredits()==1&&ledger.remainder()==0);
    CHECK(!sig::accept(reward,phase.phase(),true,ledger.pendingCredits()));
    CHECK(ledger.resolveCredit(ledger.nextCredit(),phase.phase()));
    CHECK(sig::accept(reward,phase.phase(),true,ledger.pendingCredits()));
    CHECK(!sig::accept(reward,phase.phase(),true,0)&&reward.owned);
    sig::RewardState missedCore;CHECK(sig::queue(missedCore,bossId,true));
    CHECK(sig::accept(missedCore,f::Phase::PendingRewards,true,0));
    sig::RewardState dead;CHECK(!sig::queue(dead,bossId,false));
    CHECK(!sig::accept(dead,f::Phase::PendingRewards,false,0)&&!dead.owned);
    threadlight::Mode mode=threadlight::Mode::Signature;CHECK(!threadlight::beginPlayFrame(mode,false));
    CHECK(phase.finishRewards(false));phase.start({26*120,false});
    CHECK(phase.phase()==f::Phase::Countdown&&phase.elapsedTicks()==0);
}

struct MixedResult { std::vector<int> warnings,shots;std::array<int,5> first{}; };
MixedResult coordinated(double fps){
    q::Coordinator coordinator;r::Pool pool;
    std::array<k::State,2> skitters{};std::array<r::State,3> spitters{};
    std::array<f::Point,5> p{{{300,300},{500,300},{300,500},{500,500},{700,500}}},v{};
    std::array<std::uint64_t,3> tickets{};
    for(auto& s:skitters)s.ticksRemaining=1;
    for(auto& s:spitters)s.remaining=1;
    std::array<int,3> warningTick{};
    threadlight::timing::FixedStepClock clock;MixedResult result;int tick=0;
    for(int frame=0;frame<(int)(fps*20);++frame)for(int sub=clock.advance(1/fps);sub>0;--sub){
        ++tick;int committed=0;std::array<q::Request,5> ready{};int count=0;
        for(int i=0;i<2;++i){
            committed+=k::committed(skitters[i])?1:0;
            if(skitters[i].phase==k::Phase::Flank&&skitters[i].ticksRemaining<=1)ready[count++]={(std::uint64_t)i+1,q::Kind::Skitter};
        }
        for(int i=0;i<3;++i){
            committed+=r::committed(spitters[i])?1:0;
            if(r::wantsCommit(spitters[i]))ready[count++]={(std::uint64_t)i+3,q::Kind::Spitter};
        }
        const auto grants=coordinator.choose(std::span<const q::Request>(ready.data(),count),committed,(int)pool.freeSlots(),tick);
        for(int i=0;i<2;++i){
            const auto event=k::tick(skitters[i],p[i],v[i],{500,400},{1,0},{0,1000,0,800},grants.contains(i+1),125);
            if(event.telegraphStarted&&!result.first[i])result.first[i]=tick;
        }
        for(int i=0;i<3;++i){
            if(grants.contains(i+3))tickets[i]=pool.reserve(i+3);
            const auto event=r::tick(spitters[i],p[i+2],v[i+2],{500,400},{0,1000,0,800},grants.contains(i+3)&&tickets[i]);
            if(event.warningStarted){warningTick[i]=tick;result.warnings.push_back(tick);if(!result.first[i+2])result.first[i+2]=tick;}
            if(event.shotReady){
                CHECK(tick-warningTick[i]==108);CHECK(pool.launch(tickets[i],r::muzzle(p[i+2],spitters[i].aimDirection),spitters[i].aimDirection));
                tickets[i]=0;result.shots.push_back(tick);
            }
        }
        committed=0;for(const auto& s:skitters)committed+=k::committed(s)?1:0;
        for(const auto& s:spitters)committed+=r::committed(s)?1:0;
        CHECK(committed<=2);
        pool.tick({10000,10000},{10000,10000},12,{-20000,20000,-20000,20000});
    }
    for(int t:result.first)CHECK(t>0&&t<700);
    for(size_t i=1;i<result.shots.size();++i)CHECK(result.shots[i]-result.shots[i-1]>=42);
    pool.clear();coordinator.clear();CHECK(pool.flyingCount()==0&&pool.reservedCount()==0&&coordinator.waiting()==0);
    return result;
}
void coordinatedSchedules(){
    const auto baseline=coordinated(30);
    for(double fps:{60.,120.,144.,240.,1000.}){
        const auto result=coordinated(fps);CHECK(result.shots==baseline.shots&&result.warnings==baseline.warnings&&result.first==baseline.first);
    }
}
}

int main(){chapterBudgetsAndStats();echoDamageAndSharedOwners();rewardBoundary();coordinatedSchedules();
    if(failures)return 1;
    std::cout<<"M4 integration checks passed\n";return 0;}
