#include "chapter_one.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using threadlight::chapter::Encounter;
using threadlight::chapter::Schedule;
using threadlight::chapter::Spawn;
using threadlight::combat::EnemyKind;

namespace {
int failures=0;
void check(bool condition,const char* expression,int line) {
    if(condition)return;
    std::cerr << "m5 chapter check failed at line " << line << ": " << expression << '\n';
    ++failures;
}
#define CHECK(expression) check(static_cast<bool>(expression),#expression,__LINE__)

struct Expected {
    const char* name;
    int seconds;
    bool boss;
    int chapter;
    std::array<int, 5> count; // hunter, brute, skitter, spitter, bulwark
    int sparks;
    int score;
};
constexpr std::array<Expected, 12> expected{{
    {"SLOW CARAVAN",26,false,2,{8,12,2,2,0},38,590},
    {"BURST WINDOWS",30,false,2,{6,2,20,2,0},34,490},
    {"RETURN ROUTE",28,false,2,{18,4,4,4,0},38,500},
    {"COMMITMENT TEST",32,false,2,{10,6,8,8,0},46,670},
    {"THE NEEDLE DANCER",0,true,2,{0,0,0,0,0},0,0},
    {"OPENING PLATES",28,false,3,{16,4,6,4,2},44,600},
    {"FIRING RELAY",30,false,3,{8,2,6,12,2},48,690},
    {"MOVING ANCHORS",30,false,3,{8,9,4,2,6},52,785},
    {"LOOSE WEFT",32,false,3,{6,2,18,4,2},42,610},
    {"GATHERING TIDE",30,false,3,{21,6,6,2,2},49,660},
    {"CALL AND RESPONSE",34,false,3,{10,6,8,6,5},57,835},
    {"THE THORN CONDUCTOR",0,true,3,{0,0,0,0,0},0,0}
}};
constexpr std::array<EnemyKind,5> kinds{{EnemyKind::Hunter,EnemyKind::Brute,
    EnemyKind::Skitter,EnemyKind::Spitter,EnemyKind::Bulwark}};
int kindIndex(EnemyKind kind) {
    for(int i=0;i<static_cast<int>(kinds.size());++i)if(kinds[i]==kind)return i;
    CHECK(false && "unexpected scheduled enemy kind");
    return -1;
}
}

int main() {
    using namespace threadlight;
    CHECK(chapter::kEncounterCount==35);
    CHECK(chapter::encounter(6).name && chapter::encounter(6).boss);
    CHECK(chapter::encounter(6).name==std::string("THE ROOTWARDEN"));
    for(int i=0;i<static_cast<int>(expected.size());++i) {
        const auto& want=expected[i];
        const Encounter got=chapter::encounter(i+9);
        CHECK(std::string(got.name)==want.name);
        CHECK(got.seconds==want.seconds && got.boss==want.boss && got.chapter==want.chapter);

        const Schedule a=chapter::schedule(i+9,0x12345678u);
        const Schedule b=chapter::schedule(i+9,0x87654321u);
        CHECK(a.count==b.count);
        CHECK(a.count>=0 && a.count<=37);
        const int checkedCount=std::clamp(a.count,0,static_cast<int>(a.items.size()));
        std::array<int,5> counts{};
        int sparks=0,score=0,latest=-1;
        bool angleChanged=false;
        std::set<std::tuple<int,int>> timingIdentity;
        for(int n=0;n<checkedCount;++n) {
            const Spawn& spawn=a.items[n];
            CHECK(spawn.tick>=0);
            CHECK(spawn.tick<want.seconds*120);
            CHECK(spawn.tick<want.seconds*120-240); // Keep two seconds clear at the end.
            CHECK(n==0 || a.items[n-1].tick<=spawn.tick);
            const int index=kindIndex(spawn.kind);
            CHECK(index>=0);
            if(index>=0)++counts[index];
            const auto definition=combat::definition(spawn.kind,want.chapter);
            sparks+=definition.sparkValue;
            score+=definition.killScore;
            latest=std::max(latest,spawn.tick);
            timingIdentity.emplace(spawn.tick,static_cast<int>(spawn.kind));
            CHECK(spawn.tick==b.items[n].tick && spawn.kind==b.items[n].kind);
            angleChanged=angleChanged || spawn.angle!=b.items[n].angle;
        }
        CHECK(counts==want.count);
        CHECK(sparks==want.sparks && score==want.score);
        CHECK(angleChanged || want.boss);
        CHECK(timingIdentity.size()==static_cast<std::size_t>(checkedCount));
        if(want.boss) CHECK(a.count==0);
        else CHECK(a.count>0 && latest>=0);
    }

    const auto bulwark=combat::definition(EnemyKind::Bulwark,3);
    CHECK(bulwark.hp==700 && bulwark.radius==23.f && bulwark.speed==52.f);
    CHECK(bulwark.contactDamage==2000 && bulwark.sparkValue==3 && bulwark.killScore==45);

    // Each encounter has a distinct ordered timing/composition identity.
    for(int i=0;i<12;++i)for(int j=i+1;j<12;++j) {
        if(expected[i].boss || expected[j].boss)continue;
        const Schedule a=chapter::schedule(i+9,17u);
        const Schedule b=chapter::schedule(j+9,17u);
        bool same=a.count==b.count;
        const int checkedCount=std::clamp(a.count,0,static_cast<int>(a.items.size()));
        for(int n=0;same && n<checkedCount;++n)
            same=a.items[n].tick==b.items[n].tick && a.items[n].kind==b.items[n].kind;
        CHECK(!same);
    }
    return failures==0?0:1;
}
