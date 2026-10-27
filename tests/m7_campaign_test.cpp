#include "chapter_one.h"
#include "chapter_boss.h"
#include "combat.h"
#include "player_upgrades.h"
#include "reward_ledger.h"
#include "signature_catalog.h"
#include "signature_reward.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

namespace {
using namespace threadlight;
constexpr int kSeeds=2000;
constexpr std::array<int,3> kCreditBudgets{{16,22,26}};
constexpr std::array<int,5> kBossWaves{{6,13,20,27,34}};
constexpr std::array<int,5> kBossSupportCounts{{4,4,6,6,8}};
constexpr std::array<int,5> kCheckpointWaves{{6,13,20,27,34}};
constexpr int kTheoreticalSparkMaximum=1364;
constexpr int kMaximumCredits=29;
int failures=0;
void check(bool condition,const char* expression,int line) {
    if(condition)return;
    std::cerr << "M7 model check failed at line " << line << ": " << expression << '\n';
    ++failures;
}
#define CHECK(expression) check(static_cast<bool>(expression),#expression,__LINE__)

enum class Policy { Offense, Defense, Mobility, Hybrid };
struct Quality { const char* name; int killPercent,collectPercent; };
constexpr std::array<Quality,3> kQuality{{
    {"low",60,65},{"medium",78,80},{"high",92,93}
}};
constexpr std::array<const char*,4> kPolicyNames{{"offense","defense","mobility","hybrid"}};
constexpr std::array<upgrades::Id,3> kOffenseTargets{{upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad}};
constexpr std::array<upgrades::Id,3> kDefenseTargets{{upgrades::Id::Heartwood,upgrades::Id::Ironbark,upgrades::Id::Bloodstitch}};
constexpr std::array<upgrades::Id,3> kMobilityTargets{{upgrades::Id::Quick,upgrades::Id::Rhythm,upgrades::Id::Longstride}};
constexpr std::array<upgrades::Id,9> kHybridTargets{{
    upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Quick,
    upgrades::Id::Rhythm,upgrades::Id::Heartwood,upgrades::Id::Ironbark,
    upgrades::Id::Longstride,upgrades::Id::Bloodstitch}};
constexpr std::array<upgrades::Id,11> kOffenseFallback{{
    upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Quick,
    upgrades::Id::Rhythm,upgrades::Id::Longstride,upgrades::Id::Heartwood,
    upgrades::Id::Ironbark,upgrades::Id::Bloodstitch,upgrades::Id::Magnet,upgrades::Id::Needlecatch}};
constexpr std::array<upgrades::Id,11> kDefenseFallback{{
    upgrades::Id::Heartwood,upgrades::Id::Ironbark,upgrades::Id::Bloodstitch,
    upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Quick,
    upgrades::Id::Rhythm,upgrades::Id::Longstride,upgrades::Id::Magnet,upgrades::Id::Needlecatch}};
constexpr std::array<upgrades::Id,11> kMobilityFallback{{
    upgrades::Id::Quick,upgrades::Id::Rhythm,upgrades::Id::Longstride,
    upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Heartwood,
    upgrades::Id::Ironbark,upgrades::Id::Bloodstitch,upgrades::Id::Magnet,upgrades::Id::Needlecatch}};
constexpr std::array<upgrades::Id,11> kHybridFallback{{
    upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Quick,
    upgrades::Id::Rhythm,upgrades::Id::Heartwood,upgrades::Id::Ironbark,
    upgrades::Id::Longstride,upgrades::Id::Bloodstitch,upgrades::Id::Magnet,upgrades::Id::Needlecatch}};

std::uint32_t mix32(std::uint32_t x) {
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return x;
}
std::uint32_t eventRoll(std::uint32_t seed,int wave,int ordinal,std::uint32_t channel) {
    const std::uint32_t a=static_cast<std::uint32_t>(wave+1)*0x9e3779b9u;
    const std::uint32_t b=static_cast<std::uint32_t>(ordinal+1)*0x85ebca6bu;
    return mix32(seed^a^b^channel);
}
bool succeeds(std::uint32_t roll,int chancePercent) {
    return static_cast<int>(roll%10000u)<std::clamp(chancePercent,0,99)*100;
}
int chapterForWave(int wave) { return chapter::encounter(wave).chapter; }
int bossOrdinal(int wave) {
    for(int i=0;i<static_cast<int>(kBossWaves.size());++i)if(kBossWaves[i]==wave)return i;
    return -1;
}
chapter_boss::Kind bossKind(int wave) {
    switch(wave) {
    case 6:return chapter_boss::Kind::Rootwarden;
    case 13:return chapter_boss::Kind::Dancer;
    case 20:return chapter_boss::Kind::Conductor;
    case 27:return chapter_boss::Kind::Hollow;
    default:return chapter_boss::Kind::Heart;
    }
}
struct Economy {
    foundation::RewardLedger ledger;
    std::array<std::uint64_t,chapter::kEncounterCount> creditsAfterWave{};
    std::array<std::uint64_t,chapter::kEncounterCount> collectedAfterWave{};
    std::uint64_t theoreticalSparks=0;
};
void processEconomyEnemy(Economy& result,std::uint32_t seed,int wave,int ordinal,
                         combat::EnemyKind kind,const Quality& quality,
                         bool magnetSensitivity,int runQualityPercent,bool core=false) {
    const auto def=combat::definition(kind,chapterForWave(wave));
    const int sparkValue=core?10:def.sparkValue;
    const int killScore=core?0:def.killScore;
    result.theoreticalSparks+=static_cast<std::uint64_t>(sparkValue);
    const auto id=result.ledger.registerEnemy(sparkValue,killScore);
    CHECK(id!=0&&id==result.ledger.counters().spawned);
    int killChance=quality.killPercent+runQualityPercent;
    if(!core&&(kind==combat::EnemyKind::Brute||kind==combat::EnemyKind::BriarCaller))killChance-=3;
    const int collectionChance=std::min(99,quality.collectPercent+runQualityPercent+
        (magnetSensitivity&&wave>=9?8:0));
    if(!core&&!succeeds(eventRoll(seed,wave,ordinal,0x31415927u),killChance)) {
        CHECK(result.ledger.despawnEnemy(id));
        return;
    }
    CHECK(result.ledger.defeatEnemy(id).has_value());
    if(succeeds(eventRoll(seed,wave,ordinal,0x27182819u),collectionChance))
        CHECK(result.ledger.collectPickup(id));
    else CHECK(result.ledger.expirePickup(id));
}
Economy makeEconomy(std::uint32_t seed,const Quality& quality,bool magnetSensitivity) {
    Economy result;
    const int runQualityPercent=static_cast<int>(mix32(seed^0x6d2b79f5u)%13u)-6;
    std::uint64_t previousIssued=0,previousCollected=0;
    for(int wave=0;wave<chapter::kEncounterCount;++wave) {
        const auto schedule=chapter::schedule(wave,0x4d37434du);
        int ordinal=0;
        for(int i=0;i<schedule.count;++i)
            processEconomyEnemy(result,seed,wave,ordinal++,schedule.items[i].kind,quality,
                                magnetSensitivity,runQualityPercent);
        const int boss=bossOrdinal(wave);
        if(boss>=0) {
            chapter_boss::State state{};state.kind=bossKind(wave);
            for(int i=0;i<kBossSupportCounts[static_cast<std::size_t>(boss)];++i)
                processEconomyEnemy(result,seed,wave,ordinal++,chapter_boss::supportKind(state,i),quality,
                                    magnetSensitivity,runQualityPercent);
            if(boss<4)
                processEconomyEnemy(result,seed,wave,ordinal,combat::EnemyKind::Hunter,quality,
                                    magnetSensitivity,runQualityPercent,true);
        }
        const auto& counters=result.ledger.counters();
        result.creditsAfterWave[static_cast<std::size_t>(wave)]=counters.creditsIssued;
        result.collectedAfterWave[static_cast<std::size_t>(wave)]=counters.sparksCollected;
        CHECK(counters.creditsIssued>=previousIssued&&counters.creditsIssued<=kMaximumCredits);
        CHECK(counters.sparksCollected>=previousCollected&&counters.sparksCollected<=kTheoreticalSparkMaximum);
        CHECK(counters.sparksCreated<=result.theoreticalSparks);
        CHECK(counters.sparksCollected+counters.sparksExpired==counters.sparksCreated);
        previousIssued=counters.creditsIssued;previousCollected=counters.sparksCollected;
    }
    return result;
}
std::uint64_t theoreticalCampaignSparks() {
    std::uint64_t total=0;
    for(int wave=0;wave<chapter::kEncounterCount;++wave) {
        const auto schedule=chapter::schedule(wave,0x4d37434du);
        for(int i=0;i<schedule.count;++i)
            total+=static_cast<std::uint64_t>(combat::definition(schedule.items[i].kind,chapterForWave(wave)).sparkValue);
        const int boss=bossOrdinal(wave);
        if(boss>=0) {
            chapter_boss::State state{};state.kind=bossKind(wave);
            for(int i=0;i<kBossSupportCounts[static_cast<std::size_t>(boss)];++i)
                total+=static_cast<std::uint64_t>(combat::definition(chapter_boss::supportKind(state,i),chapterForWave(wave)).sparkValue);
            if(boss<4)total+=10;
        }
    }
    return total;
}

std::array<upgrades::Id,11> fallbackOrder(Policy policy) {
    switch(policy) {
    case Policy::Offense:return kOffenseFallback;
    case Policy::Defense:return kDefenseFallback;
    case Policy::Mobility:return kMobilityFallback;
    case Policy::Hybrid:return kHybridFallback;
    }
    return kHybridFallback;
}
int findChoice(const upgrades::Offer& offer,upgrades::Id id) {
    for(int i=0;i<offer.count;++i)if(offer.cards[i]==id)return i;
    return -1;
}
upgrades::Id firstTarget(Policy policy) {
    switch(policy) {
    case Policy::Offense:return upgrades::Id::Razor;
    case Policy::Defense:return upgrades::Id::Heartwood;
    case Policy::Mobility:return upgrades::Id::Quick;
    case Policy::Hybrid:return upgrades::Id::Razor;
    }
    return upgrades::Id::Razor;
}
int chooseUpgrade(const upgrades::Offer& offer,const upgrades::Ranks& ranks,
                  const combat::PlayerHealth& health,Policy policy,int creditNumber,
                  bool healWhenAvailable) {
    if(healWhenAvailable) {
        const int recovery=findChoice(offer,upgrades::Id::Restitch);
        if(recovery>=0)return recovery;
    }
    auto choose=[&](upgrades::Id id) {
        const int at=findChoice(offer,id);
        return at>=0&&upgrades::eligible(id,ranks,health,true,true)?at:-1;
    };
    if(policy==Policy::Offense)for(auto id:kOffenseTargets)if(const int at=choose(id);at>=0)return at;
    if(policy==Policy::Defense)for(auto id:kDefenseTargets)if(const int at=choose(id);at>=0)return at;
    if(policy==Policy::Mobility)for(auto id:kMobilityTargets)if(const int at=choose(id);at>=0)return at;
    if(policy==Policy::Hybrid)for(int offset=0;offset<static_cast<int>(kHybridTargets.size());++offset) {
        const auto id=kHybridTargets[static_cast<std::size_t>((creditNumber+offset)%kHybridTargets.size())];
        if(const int at=choose(id);at>=0)return at;
    }
    const auto fallback=fallbackOrder(policy);
    for(auto id:fallback)if(const int at=choose(id);at>=0)return at;
    return offer.count?0:-1;
}
struct OfferStats {
    int primaryMaxed=0,maxedUpgrades=0,permanentChoices=0,recoveryChoices=0;
};
OfferStats simulateOfferBudget(std::uint32_t seed,int budget,Policy policy,bool fixedDeficit) {
    foundation::RewardLedger ledger;
    upgrades::OfferState offerState{};
    offerState.rng=mix32(seed^static_cast<std::uint32_t>(0xa511e9b3u+static_cast<int>(policy)*0x9e3779b9u+
                                                            (fixedDeficit?0x6335u:0u)));
    upgrades::Ranks ranks{};
    combat::PlayerHealth health{};
    OfferStats result;
    for(int creditNumber=1;creditNumber<=budget;++creditNumber) {
        const int deposit=static_cast<int>(ledger.nextThreshold()-ledger.remainder());
        const auto id=ledger.registerEnemy(deposit,0);
        CHECK(id==ledger.counters().spawned);
        CHECK(ledger.defeatEnemy(id).has_value());
        CHECK(ledger.collectPickup(id));
        CHECK(ledger.counters().creditsIssued==static_cast<std::uint64_t>(creditNumber));
        if(fixedDeficit)health.hp=health.maxHp-3500;
        const auto creditId=ledger.nextCredit();
        CHECK(creditId==static_cast<foundation::CreditId>(creditNumber));
        const auto& offer=upgrades::prepareOffer(offerState,ranks,health,creditId,true,false,true);
        CHECK(offer.credit==creditId&&offer.count>0&&offer.count<=3);
        const int selectedIndex=chooseUpgrade(offer,ranks,health,policy,creditNumber-1,fixedDeficit);
        CHECK(selectedIndex>=0&&selectedIndex<offer.count);
        if(selectedIndex<0||selectedIndex>=offer.count)continue;
        const auto selected=offer.cards[static_cast<std::size_t>(selectedIndex)];
        CHECK(upgrades::resolveChoice(ledger,foundation::Phase::PendingRewards,offer,
                                      selectedIndex,ranks,health,true,true));
        CHECK(ledger.counters().creditsResolved==static_cast<std::uint64_t>(creditNumber));
        if(selected==upgrades::Id::Restitch)++result.recoveryChoices;
        else ++result.permanentChoices;
    }
    for(int i=0;i<upgrades::kPermanentCount;++i) {
        const auto id=static_cast<upgrades::Id>(i);
        const int rank=upgrades::rank(ranks,id);
        CHECK(rank<=upgrades::maximum(id));
        result.maxedUpgrades+=rank==upgrades::maximum(id)?1:0;
    }
    result.primaryMaxed=upgrades::rank(ranks,firstTarget(policy))==upgrades::maximum(firstTarget(policy))?1:0;
    CHECK(result.permanentChoices+result.recoveryChoices==budget);
    CHECK(result.maxedUpgrades<upgrades::kPermanentCount);
    return result;
}

double quantile(const std::vector<int>& values,double p) {
    if(values.empty())return 0;
    std::vector<int> sorted=values;std::sort(sorted.begin(),sorted.end());
    const auto index=static_cast<std::size_t>(p*static_cast<double>(sorted.size()-1));
    return static_cast<double>(sorted[index]);
}
double median(const std::vector<int>& values) {
    if(values.empty())return 0;
    std::vector<int> sorted=values;std::sort(sorted.begin(),sorted.end());
    const auto hi=sorted.size()/2;
    if(sorted.size()%2)return sorted[hi];
    return (static_cast<double>(sorted[hi-1])+sorted[hi])/2.0;
}
double mean(const std::vector<int>& values) {
    if(values.empty())return 0;
    const std::int64_t total=std::accumulate(values.begin(),values.end(),std::int64_t{0});
    return static_cast<double>(total)/static_cast<double>(values.size());
}
struct EconomyGroup {
    std::vector<int> credits;
    std::array<std::vector<int>,kCheckpointWaves.size()> checkpointCredits;
    void add(const Economy& result) {
        credits.push_back(static_cast<int>(result.ledger.counters().creditsIssued));
        for(std::size_t i=0;i<kCheckpointWaves.size();++i)
            checkpointCredits[i].push_back(static_cast<int>(result.creditsAfterWave[static_cast<std::size_t>(kCheckpointWaves[i])]));
    }
};
struct OfferGroup {
    std::vector<int> primaryMaxed,maxedUpgrades,permanentChoices,recoveryChoices;
    void add(const OfferStats& result) {
        primaryMaxed.push_back(result.primaryMaxed);maxedUpgrades.push_back(result.maxedUpgrades);
        permanentChoices.push_back(result.permanentChoices);recoveryChoices.push_back(result.recoveryChoices);
    }
};
std::size_t economyIndex(int quality,int magnet) { return static_cast<std::size_t>(quality*2+magnet); }
std::size_t offerIndex(int policy,int deficit,int budget) {
    return static_cast<std::size_t>((policy*2+deficit)*3+budget);
}
}

int runFullModel() {
    using namespace threadlight;
    CHECK(chapter::kEncounterCount==35);
    CHECK(theoreticalCampaignSparks()==kTheoreticalSparkMaximum);
    std::cout<<"MODEL ONLY: economy probabilities, policy choices and fixed-deficit recovery are explicit assumptions, not measured gameplay or skill estimates.\n";
    std::cout<<"Economy: 2000 paired seeds per low/medium/high quality and base/+8 Magnet sensitivity arm. Kill/collect bases are 60/65, 78/80, 92/93 percent; shared run quality is uniform integer -6..+6 points. Brute/Briar Caller kill chance -3 points. Caller fusion is not simulated in this assumed economy model. Failed kills despawn; failed manual pickups expire. Sower/support patterns use production data. First four boss cores are 10 sparks with the normal collect roll. No pickup lifetime, survival combat, signature effects, hidden rewards or autobank.\n";
    std::cout<<"Offer budget model is independent of those earned-credit distributions: each requested 16/22/26-credit fixture deposits only the RewardLedger threshold needed for one more credit. Offers use beforeBoss=false. Injured stress sets HP=maxHP-3500 before every offer (a repeated fixed deficit, not cumulative damage), so Restitch availability follows the production recovery-missed rule; choose it when present, then policy priorities. No signature choices are included here.\n";
    std::cout<<"Theoretical collectible ceiling="<<kTheoreticalSparkMaximum<<" sparks; maximum ordinary credits="<<kMaximumCredits<<".\n";

    std::array<EconomyGroup,6> economies{};
    std::array<std::array<std::vector<int>,kCheckpointWaves.size()>,2> lowQualityCheckpoints{};
    std::array<OfferGroup,4*2*3> offers{};
    const auto start=std::chrono::steady_clock::now();
    for(int seedIndex=1;seedIndex<=kSeeds;++seedIndex) {
        const auto seed=static_cast<std::uint32_t>(seedIndex);
        for(int quality=0;quality<3;++quality) {
            const Economy base=makeEconomy(seed,kQuality[static_cast<std::size_t>(quality)],false);
            const Economy magnet=makeEconomy(seed,kQuality[static_cast<std::size_t>(quality)],true);
            CHECK(base.ledger.counters().killed==magnet.ledger.counters().killed);
            CHECK(base.ledger.counters().sparksCollected<=magnet.ledger.counters().sparksCollected);
            CHECK(base.ledger.counters().creditsIssued<=magnet.ledger.counters().creditsIssued);
            economies[economyIndex(quality,0)].add(base);
            economies[economyIndex(quality,1)].add(magnet);
            if(quality==0)for(int checkpoint=0;checkpoint<static_cast<int>(kCheckpointWaves.size());++checkpoint) {
                lowQualityCheckpoints[0][static_cast<std::size_t>(checkpoint)].push_back(
                    static_cast<int>(base.creditsAfterWave[static_cast<std::size_t>(kCheckpointWaves[static_cast<std::size_t>(checkpoint)])]));
                lowQualityCheckpoints[1][static_cast<std::size_t>(checkpoint)].push_back(
                    static_cast<int>(magnet.creditsAfterWave[static_cast<std::size_t>(kCheckpointWaves[static_cast<std::size_t>(checkpoint)])]));
            }
        }
        for(int policy=0;policy<4;++policy)for(int deficit=0;deficit<2;++deficit)
            for(int budget=0;budget<3;++budget) {
                const auto stats=simulateOfferBudget(seed,kCreditBudgets[static_cast<std::size_t>(budget)],
                    static_cast<Policy>(policy),deficit!=0);
                offers[offerIndex(policy,deficit,budget)].add(stats);
            }
    }
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now()-start).count();
    std::cout<<std::fixed<<std::setprecision(2);
    std::cout<<"Economy results: quality magnet collected-credit-total p10/median/p90 over 2000 runs.\n";
    for(int quality=0;quality<3;++quality)for(int magnet=0;magnet<2;++magnet) {
        const auto& group=economies[economyIndex(quality,magnet)];
        std::cout<<kQuality[static_cast<std::size_t>(quality)].name<<' '
            <<(magnet?"magnet_plus8_from_wave10":"base")<<' '
            <<quantile(group.credits,.10)<<'/'<<median(group.credits)<<'/'<<quantile(group.credits,.90)<<'\n';
    }
    std::cout<<"Low-quality cumulative earned credits at encounter checkpoints (median; not a game mode):\n";
    for(int magnet=0;magnet<2;++magnet) {
        std::cout<<(magnet?"magnet_plus8_from_wave10":"base");
        for(std::size_t i=0;i<kCheckpointWaves.size();++i)
            std::cout<<" wave"<<(kCheckpointWaves[i]+1)<<'='<<median(lowQualityCheckpoints[static_cast<std::size_t>(magnet)][i]);
        std::cout<<'\n';
    }
    std::cout<<"Offer results: 2000 seeded synthetic offer runs per cell; percent reaches policy's single primary target cap, other values are mean counts/run.\n";
    std::cout<<"policy health credit_budget primary_target_maxed_percent maxed_permanent_upgrades permanent_choices recovery_choices\n";
    for(int policy=0;policy<4;++policy)for(int deficit=0;deficit<2;++deficit)for(int budget=0;budget<3;++budget) {
        const auto& group=offers[offerIndex(policy,deficit,budget)];
        CHECK(group.primaryMaxed.size()==kSeeds);
        std::cout<<kPolicyNames[static_cast<std::size_t>(policy)]<<' '
            <<(deficit?"fixed_deficit_35hp_each_offer":"healthy")<<' '
            <<kCreditBudgets[static_cast<std::size_t>(budget)]<<' '
            <<(100.0*mean(group.primaryMaxed))<<' '
            <<mean(group.maxedUpgrades)<<' '
            <<mean(group.permanentChoices)<<' '
            <<mean(group.recoveryChoices)<<'\n';
    }
    std::cout<<"elapsed_ms="<<elapsed<<" checks="<<(failures==0?"PASS":"FAIL")<<'\n';
    return failures==0?0:1;
}

int runInvariantChecks() {
    using namespace threadlight;
    CHECK(chapter::kEncounterCount==35);
    CHECK(kBossSupportCounts[0]==4);
    chapter_boss::State rootwarden{};
    rootwarden.kind=chapter_boss::Kind::Rootwarden;
    CHECK(chapter_boss::supportKind(rootwarden,0)==combat::EnemyKind::Hunter);
    CHECK(chapter_boss::supportKind(rootwarden,1)==combat::EnemyKind::Hunter);
    CHECK(chapter_boss::supportKind(rootwarden,2)==combat::EnemyKind::Skitter);
    CHECK(chapter_boss::supportKind(rootwarden,3)==combat::EnemyKind::Skitter);
    CHECK(theoreticalCampaignSparks()==kTheoreticalSparkMaximum);
    std::uint64_t scheduledSparks=0;
    for(int wave=0;wave<chapter::kEncounterCount;++wave) {
        const auto enc=chapter::encounter(wave);
        const auto schedule=chapter::schedule(wave,0x4d37434du);
        CHECK(schedule.count>=0&&schedule.count<=40);
        CHECK(!enc.boss||schedule.count==0);
        for(int i=0;i<schedule.count;++i) {
            const auto& spawn=schedule.items[i];
            CHECK(i==0||schedule.items[i-1].tick<=spawn.tick);
            if(!enc.boss)scheduledSparks+=static_cast<std::uint64_t>(combat::definition(spawn.kind,enc.chapter).sparkValue);
            CHECK(wave>=21||spawn.kind!=combat::EnemyKind::Sower);
            if(wave>=21&&!enc.boss)CHECK(spawn.tick<=enc.seconds*120-240);
        }
    }
    CHECK(scheduledSparks==1290);
    CHECK(chapter::encounter(21).chapter==4&&chapter::encounter(28).chapter==5);

    foundation::RewardLedger thresholdFixture;
    const auto pickup=thresholdFixture.registerEnemy(1364,0);
    CHECK(pickup==1);
    CHECK(thresholdFixture.defeatEnemy(pickup).has_value());
    CHECK(thresholdFixture.collectPickup(pickup));
    CHECK(thresholdFixture.counters().creditsIssued==29&&thresholdFixture.remainder()==26);

    upgrades::OfferState offerState{};
    upgrades::Ranks ranks{};
    combat::PlayerHealth health{};
    const auto offer=upgrades::prepareOffer(offerState,ranks,health,1);
    const auto stableOffer=offer;
    const auto stableRng=offerState.rng;
    CHECK(stableOffer.count==3);
    CHECK(upgrades::prepareOffer(offerState,ranks,health,1).cards==stableOffer.cards);
    CHECK(offerState.rng==stableRng);
    signature_catalog::State signatures{};
    const auto sigOffer=signature_catalog::prepare(signatures,7);
    const auto sigRng=signatures.rng;
    CHECK(sigOffer.count==3&&sigOffer.bossSource==7);
    CHECK(signature_catalog::prepare(signatures,7).choices==sigOffer.choices);
    CHECK(signatures.rng==sigRng);
    std::cout<<"M7 campaign invariants "<<(failures==0?"PASS":"FAIL")
        <<": 35 encounters, 1290 scheduled sparks, 1364 theoretical pickup ceiling, 29 maximum ordinary credits, Sower begins at wave 22. Full separated 2000-seed model: pass --model.\n";
    return failures==0?0:1;
}

int main(int argc,char** argv) {
    if(argc==1)return runInvariantChecks();
    if(argc==2&&std::string_view(argv[1])=="--model")return runFullModel();
    std::cerr<<"usage: m7_campaign_test [--model]\n";
    return 2;
}
