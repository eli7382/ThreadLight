#include "encounter_cleanup.h"
#include "game_presentation.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace cleanup=threadlight::encounter_cleanup;
namespace root=threadlight::chapter_boss;
namespace ranged=threadlight::ranged;
namespace hazards=threadlight::area_hazards;
namespace attacks=threadlight::attacks;
namespace presentation=threadlight::presentation;
using threadlight::foundation::RewardLedger;
#define CHECK(x) do { if(!(x)){std::cerr<<"CHECK failed: " #x " at "<<__FILE__<<':'<<__LINE__<<'\n';std::abort();} } while(false)

namespace {
constexpr hazards::Bounds kFieldBounds{0,1000,0,600};

presentation::Foe foe(threadlight::foundation::RewardId id,bool support,ranged::Pool::Ticket ticket,int hp=100){
    presentation::Foe out{};out.p={100.f+static_cast<float>(id),120};out.previousP=out.p;
    out.health={hp,100,0};out.rewardId=id;out.tacticalSupport=support;out.projectileTicket=ticket;return out;
}
ranged::Pool::Ticket shot(ranged::Pool& pool,std::uint64_t owner,bool flying){
    const auto ticket=pool.reserve(owner);CHECK(ticket);
    if(flying)CHECK(pool.launch(ticket,{200,200},{1,0},{120,3,1400,240}));
    return ticket;
}
hazards::Ticket field(hazards::Pool& pool,std::uint64_t owner,bool activate){
    hazards::PatchSpec spec;spec.shape=hazards::Shape::Disk;spec.ownerId=owner;
    spec.center={300.f+static_cast<float>(owner%20),300};spec.radius=18;spec.warningTicks=1;spec.activeTicks=60;spec.damage=900;
    const auto ticket=pool.add(spec,kFieldBounds);CHECK(ticket);
    if(activate){(void)pool.tick(spec.center,spec.center,0);(void)pool.tick(spec.center,spec.center,0);CHECK(pool.count(hazards::SlotState::Active)==1);}
    return ticket;
}

void cleanupMixedThreatState(){
    RewardLedger ledger;
    const auto hunter=ledger.registerEnemy(1,10),earnedSupport=ledger.registerEnemy(2,35),spitter=ledger.registerEnemy(2,30);
    CHECK(hunter&&earnedSupport&&spitter);
    const auto earnedPickup=ledger.defeatEnemy(earnedSupport);CHECK(earnedPickup&&earnedPickup->sparkValue==2);

    ranged::Pool ordinary;
    const auto liveReserved=shot(ordinary,101,false),deadSupportFlying=shot(ordinary,102,true);
    std::vector<presentation::Foe> foes{foe(hunter,false,liveReserved),foe(earnedSupport,true,deadSupportFlying,0),foe(spitter,true,0)};

    root::State boss;root::start(boss,{500,300},{0,1000,0,600},root::Kind::Unweaver);
    const auto bossReserved=shot(boss.bullets,201,false),bossFlying=shot(boss.bullets,202,true);
    boss.unweaver.rayCount=2;
    boss.unweaver.rays[0]={{500,300},{1,0},120,bossReserved};
    boss.unweaver.rays[1]={{500,300},{0,1},120,bossFlying};
    boss.unweaver.planCommitted=true;boss.unweaver.echo.count=2;boss.unweaver.echo.warning=5;boss.unweaver.echo.active=40;
    boss.unweaver.strands[0]={77,{120,150},{220,150},0,80,false,false};boss.unweaver.sealTicks=30;
    CHECK(field(boss.fields,301,false));

    hazards::Pool ground;CHECK(field(ground,402,true));CHECK(field(ground,401,false));
    attacks::Coordinator coordinator;
    const std::array<attacks::Request,1> first{{{501,attacks::Kind::Spitter}}};
    CHECK(coordinator.choose(std::span<const attacks::Request>(first),0,1,10).contains(501));
    const std::array<attacks::Request,2> queued{{{502,attacks::Kind::Spitter},{503,attacks::Kind::Skitter}}};
    CHECK(coordinator.choose(std::span<const attacks::Request>(queued),2,1,11).count==0&&coordinator.waiting()==2);

    std::vector<presentation::Thread> silk(3);
    silk[0].p={20,30};silk[0].age=1.25f;silk[0].previousAge=1;silk[0].id=601;silk[0].corruptedTicks=90;
    silk[1].p={40,50};silk[1].age=2.5f;silk[1].previousAge=2;silk[1].id=602;silk[1].severed=true;silk[1].corruptedTicks=12;
    silk[2].p={60,70};silk[2].age=3.5f;silk[2].previousAge=3;silk[2].id=603;
    int supportQueued=4;

    const auto before=ledger.counters();const auto score=ledger.score();const auto remainder=ledger.remainder();
    const int hp=boss.health.hp,phase=boss.unweaver.progress.phase;const auto credit=boss.unweaver.progress.credited;
    cleanup::cancelHostiles(boss,foes,ordinary,ground,coordinator,ledger,supportQueued,std::span<presentation::Thread>(silk));

    CHECK(foes.empty());CHECK(ordinary.reservedCount()==0&&ordinary.flyingCount()==0);
    CHECK(boss.bullets.reservedCount()==0&&boss.bullets.flyingCount()==0);
    CHECK(boss.fields.occupiedSlotCount()==0&&ground.occupiedSlotCount()==0);
    CHECK(coordinator.waiting()==0&&supportQueued==0);
    CHECK(boss.unweaver.rayCount==0&&!boss.unweaver.planCommitted&&boss.unweaver.echo.count==0);
    CHECK(boss.unweaver.strands[0].id==0&&boss.unweaver.sealTicks==0);
    CHECK(boss.health.hp==hp&&boss.unweaver.progress.phase==phase&&boss.unweaver.progress.credited==credit);
    CHECK(silk[0].corruptedTicks==0&&silk[1].corruptedTicks==0&&silk[2].corruptedTicks==0);
    CHECK(!silk[0].severed&&silk[1].severed&&!silk[2].severed);
    CHECK(silk[0].id==601&&silk[0].p.x==20&&silk[0].p.y==30&&silk[0].age==1.25f&&silk[0].previousAge==1);
    CHECK(silk[1].id==602&&silk[1].p.x==40&&silk[1].age==2.5f&&silk[1].previousAge==2);
    CHECK(silk[2].id==603&&silk[2].p.x==60&&silk[2].age==3.5f&&silk[2].previousAge==3);

    const auto after=ledger.counters();
    CHECK(after.spawned==before.spawned&&after.killed==before.killed&&after.despawned==before.despawned+2);
    CHECK(after.pickupsCreated==before.pickupsCreated&&after.pickupsCollected==before.pickupsCollected&&after.pickupsExpired==before.pickupsExpired);
    CHECK(after.sparksCreated==before.sparksCreated&&after.sparksCollected==before.sparksCollected&&after.sparksExpired==before.sparksExpired);
    CHECK(after.creditsIssued==before.creditsIssued&&after.creditsResolved==before.creditsResolved);
    CHECK(ledger.score()==score&&ledger.remainder()==remainder);
    CHECK(!ledger.despawnEnemy(hunter)&&!ledger.despawnEnemy(spitter));

    // The killed support's already-earned pickup survives both passes.
    CHECK(ledger.collectPickup(earnedSupport));
    CHECK(ledger.counters().pickupsCollected==before.pickupsCollected+1&&ledger.score()==score+10);
    const auto despawns=ledger.counters().despawned;
    cleanup::cancelHostiles(boss,foes,ordinary,ground,coordinator,ledger,supportQueued,std::span<presentation::Thread>(silk));
    CHECK(ledger.counters().despawned==despawns);
    CHECK(boss.bullets.flyingCount()==0&&ground.occupiedSlotCount()==0&&supportQueued==0);
    const std::array<attacks::Request,1> retry{{{504,attacks::Kind::Spitter}}};
    CHECK(coordinator.choose(std::span<const attacks::Request>(retry),0,1,12).contains(504));
}
}

int main(){cleanupMixedThreatState();std::cout<<"encounter cleanup invariants passed\n";}
