#include "chapter_one.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <set>
#include <string>
#include <tuple>

using threadlight::chapter::Encounter;
using threadlight::chapter::Schedule;
using threadlight::chapter::Spawn;
using threadlight::combat::EnemyKind;

namespace {
int failures=0;
void check(bool condition,const char* expression,int line) {
    if(condition)return;
    std::cerr << "m6 chapter check failed at line " << line << ": " << expression << '\n';
    ++failures;
}
#define CHECK(expression) check(static_cast<bool>(expression),#expression,__LINE__)

struct Expected {
    const char* name;
    int seconds;
    bool boss;
    int chapter;
    std::array<int,6> count; // hunter, brute, skitter, spitter, bulwark, sower
    int sparks;
};
constexpr std::array<Expected,14> expected{{
    {"FIRST THORNS",30,false,4,{12,4,6,4,2,3},46},
    {"EXPIRING PATHS",32,false,4,{10,2,6,2,2,8},46},
    {"PATIENT CUT",30,false,4,{8,10,4,2,6,2},58},
    {"CROSSFIRE GARDEN",34,false,4,{10,2,6,9,2,4},52},
    {"OPEN SPRINT",30,false,4,{14,4,16,2,2,2},52},
    {"CHANGING GAPS",34,false,4,{13,6,8,6,3,4},62},
    {"THE HOLLOW WEAVER",0,true,4,{0,0,0,0,0,0},0},
    {"NEW PATTERN",30,false,5,{20,4,12,2,0,2},48},
    {"TENSION",32,false,5,{6,12,4,2,6,2},60},
    {"ROTATING FIRE",34,false,5,{8,2,8,10,2,4},54},
    {"NEEDLE RUSH",30,false,5,{8,2,22,0,2,2},44},
    {"REVERSAL",32,false,5,{12,10,10,4,2,2},60},
    {"CIRCUIT REHEARSAL",34,false,5,{9,10,8,6,3,4},66},
    {"THE BRIAR HEART",0,true,5,{0,0,0,0,0,0},0}
}};
constexpr std::array<EnemyKind,6> kinds{{EnemyKind::Hunter,EnemyKind::Brute,
    EnemyKind::Skitter,EnemyKind::Spitter,EnemyKind::Bulwark,EnemyKind::Sower}};
int kindIndex(EnemyKind kind) {
    for(int i=0;i<static_cast<int>(kinds.size());++i)if(kinds[i]==kind)return i;
    CHECK(false && "unexpected scheduled enemy kind");
    return -1;
}
}

int main() {
    using namespace threadlight;
    CHECK(chapter::kEncounterCount==35);
    CHECK(chapter::encounter(20).chapter==3);
    CHECK(chapter::encounter(21).chapter==4);
    CHECK(chapter::encounter(27).chapter==4 && chapter::encounter(27).boss);
    CHECK(chapter::encounter(28).chapter==5);
    CHECK(chapter::encounter(34).chapter==5 && chapter::encounter(34).boss);

    const auto sower4=combat::definition(EnemyKind::Sower,4);
    const auto sower5=combat::definition(EnemyKind::Sower,5);
    CHECK(sower4.hp==500 && sower4.contactDamage==1500 && sower4.sparkValue==2);
    CHECK(sower4.killScore==30 && sower4.radius==16.f && sower4.speed==64.f);
    CHECK(sower5.hp==600 && sower5.contactDamage==1700 && sower5.sparkValue==2);
    CHECK(sower5.killScore==30 && sower5.radius==16.f && sower5.speed==76.f);

    int campaignSparks=0;
    int totalSowers=0;
    int maximumCount=0;
    for(int wave=0;wave<chapter::kEncounterCount;++wave) {
        const Encounter encounter=chapter::encounter(wave);
        const Schedule schedule=chapter::schedule(wave,0xdecafbadU);
        CHECK(schedule.count>=0 && schedule.count<=40);
        const int checkedCount=std::clamp(schedule.count,0,static_cast<int>(schedule.items.size()));
        maximumCount=std::max(maximumCount,checkedCount);
        for(int i=0;i<checkedCount;++i) {
            const Spawn& spawn=schedule.items[i];
            CHECK(spawn.tick>=0);
            if(wave>=21)CHECK(spawn.tick<=encounter.seconds*120-240);
            CHECK(i==0 || schedule.items[i-1].tick<=spawn.tick);
            const auto definition=combat::definition(spawn.kind,encounter.chapter);
            campaignSparks+=definition.sparkValue;
            if(spawn.kind==EnemyKind::Sower) {
                ++totalSowers;
                CHECK(wave>=21);
            }
        }
    }
    CHECK(maximumCount==40);
    CHECK(campaignSparks==1290);
    CHECK(totalSowers==39);

    for(int i=0;i<static_cast<int>(expected.size());++i) {
        const Expected& want=expected[i];
        const int wave=i+21;
        const Encounter got=chapter::encounter(wave);
        CHECK(std::string(got.name)==want.name);
        CHECK(got.seconds==want.seconds && got.boss==want.boss && got.chapter==want.chapter);

        const Schedule a=chapter::schedule(wave,0x12345678U);
        const Schedule b=chapter::schedule(wave,0x87654321U);
        CHECK(a.count==b.count);
        CHECK(a.count==std::accumulate(want.count.begin(),want.count.end(),0));
        const int checkedCount=std::clamp(a.count,0,static_cast<int>(a.items.size()));
        std::array<int,6> counts{};
        std::set<std::tuple<int,int>> timingIdentity;
        bool angleChanged=false;
        int sparks=0;
        for(int n=0;n<checkedCount;++n) {
            const Spawn& spawn=a.items[n];
            CHECK(spawn.tick==b.items[n].tick && spawn.kind==b.items[n].kind);
            CHECK(spawn.tick>=0 && spawn.tick<=want.seconds*120-240);
            CHECK(n==0 || a.items[n-1].tick<=spawn.tick);
            const int index=kindIndex(spawn.kind);
            CHECK(index>=0);
            if(index>=0)++counts[index];
            sparks+=combat::definition(spawn.kind,want.chapter).sparkValue;
            timingIdentity.emplace(spawn.tick,static_cast<int>(spawn.kind));
            angleChanged=angleChanged || spawn.angle!=b.items[n].angle;
        }
        CHECK(counts==want.count);
        CHECK(sparks==want.sparks);
        CHECK(timingIdentity.size()==static_cast<std::size_t>(checkedCount));
        CHECK(angleChanged || want.boss);
        if(want.boss)CHECK(a.count==0);
        else CHECK(a.count>0);
    }

    // Each regular new wave has its own ordered time-and-kind signature.
    for(int i=0;i<static_cast<int>(expected.size());++i) {
        if(expected[i].boss)continue;
        for(int j=i+1;j<static_cast<int>(expected.size());++j) {
            if(expected[j].boss)continue;
            const Schedule a=chapter::schedule(i+21,13U);
            const Schedule b=chapter::schedule(j+21,13U);
            bool same=a.count==b.count;
            const int checkedCount=std::clamp(a.count,0,static_cast<int>(a.items.size()));
            for(int n=0;same && n<checkedCount;++n)
                same=a.items[n].tick==b.items[n].tick && a.items[n].kind==b.items[n].kind;
            CHECK(!same);
        }
    }
    return failures==0?0:1;
}
