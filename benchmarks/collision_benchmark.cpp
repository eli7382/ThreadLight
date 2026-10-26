#include "afterimage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
using Point = threadlight::foundation::Point;
using State = threadlight::afterimage::State;
namespace ai = threadlight::afterimage;
constexpr std::size_t kQueryCount = 2048;
constexpr std::size_t kBatchSize = 64;
constexpr float kTargetRadius = 2.f;
constexpr std::uint64_t kFirstId = 1;
using Clock = std::chrono::steady_clock;
constexpr std::size_t kRepetitions = 128;
volatile std::uint64_t benchmarkSink = 0;

struct Query { Point previous, current; };
struct Workload { std::string_view name; State state; std::array<Query,kQueryCount> queries; };

Workload makeWorkload(std::string_view name, int mode) {
    Workload work{};
    work.name = name;
    Point previous{-150.f,0.f};
    ai::beginDash(work.state,previous,true,true);
    for (std::size_t i=1;i<=31;++i) {
        const float x=-150.f+10.f*static_cast<float>(i);
        const float y=18.f*std::sin(static_cast<float>(i)*.21f);
        const Point current{x,y};
        ai::appendDash(work.state,previous,current,true);
        previous=current;
    }
    ai::finishDash(work.state,true,true);
    for (std::size_t i=0;i<kQueryCount;++i) {
        float x=0.f,y=0.f;
        if (mode==0) { x=260.f+static_cast<float>(i%79); y=220.f+static_cast<float>(i%31); }
        else if (mode==1) {
            if (i%4==0) { x=250.f+static_cast<float>(i%37); y=-220.f; }
            else { x=-140.f+static_cast<float>((i*17)%280); y=24.f+static_cast<float>(i%13); }
        } else { x=-145.f+static_cast<float>((i*17)%280); y=static_cast<float>(i%7)-3.f; }
        work.queries[i]={{x,y},{x+1.25f,y+.5f}};
    }
    return work;
}

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define BENCH_NOINLINE __attribute__((noinline))
#else
#define BENCH_NOINLINE
#endif

BENCH_NOINLINE bool referenceClaim(State& state, std::uint64_t id, const Query& q) {
    if(!state.active()||state.recording||state.count<2||state.count>ai::kPathPointCapacity||id==0||
       !ai::finite(q.previous)||!ai::finite(q.current)||!std::isfinite(kTargetRadius))return false;
    for(std::size_t i=0;i<state.hitTargetCount&&i<ai::kHitTargetCapacity;++i)
        if(state.hitTargetIds[i]==id)return false;
    if(state.hitTargetOverflow||state.hitTargetCount>=ai::kHitTargetCapacity){state.hitTargetOverflow=true;return false;}
    const float radius=ai::kHalfWidth+std::max(0.f,kTargetRadius),radiusSquared=radius*radius;
    for(std::size_t i=1;i<state.count;++i){
        if(threadlight::foundation::segmentsDistanceSquared(q.previous,q.current,state.path[i-1],state.path[i])<=radiusSquared){
            state.hitTargetIds[state.hitTargetCount++]=id;
            return true;
        }
    }
    return false;
}
BENCH_NOINLINE bool candidateClaim(State& state, std::uint64_t id, const Query& q) {
    return ai::claimContact(state,id,q.previous,q.current,kTargetRadius);
}

using Predicate = bool(*)(State&,std::uint64_t,const Query&);
std::uint64_t run(State& state,const Workload& work,std::size_t repetitions,Predicate predicate) {
    std::uint64_t hits=0;
    for (std::size_t r=0;r<repetitions;++r) for (std::size_t i=0;i<work.queries.size();++i){
        if(i%kBatchSize==0)state.hitTargetCount=0;
        hits+=predicate(state,kFirstId+(i%kBatchSize),work.queries[i]);
    }
    benchmarkSink = hits;
    return hits;
}

double measure(const Workload& work, Predicate predicate, std::uint64_t& checksum) {
    State state=work.state;
    const auto start=Clock::now();
    checksum=run(state,work,kRepetitions,predicate);
    const auto elapsed=std::chrono::duration<double,std::nano>(Clock::now()-start).count();
    return elapsed/static_cast<double>(kRepetitions*kQueryCount);
}

void benchmark(const Workload& work) {
    State referenceState=work.state,candidateState=work.state;
    for(std::size_t i=0;i<work.queries.size();++i){
        if(i%kBatchSize==0)referenceState.hitTargetCount=candidateState.hitTargetCount=0;
        const auto id=kFirstId+(i%kBatchSize);
        if(referenceClaim(referenceState,id,work.queries[i])!=candidateClaim(candidateState,id,work.queries[i])){
            std::cerr<<"outcome mismatch in "<<work.name<<"\n";std::exit(2);
        }
    }
    std::array<double,5> oldTimes{},newTimes{};
    std::uint64_t oldHits{},newHits{};
    for (std::size_t trial=0;trial<oldTimes.size();++trial) {
        if ((trial&1)==0) {
            oldTimes[trial]=measure(work,referenceClaim,oldHits);
            newTimes[trial]=measure(work,candidateClaim,newHits);
        } else {
            newTimes[trial]=measure(work,candidateClaim,newHits);
            oldTimes[trial]=measure(work,referenceClaim,oldHits);
        }
        if (oldHits!=newHits) { std::cerr<<"checksum mismatch in "<<work.name<<"\n"; std::exit(3); }
    }
    auto median=[](std::array<double,5> values){std::sort(values.begin(),values.end());return values[2];};
    const double oldNs=median(oldTimes),newNs=median(newTimes);
    std::cout<<work.name<<" trials=5 repetitions="<<kRepetitions<<" claims="<<kRepetitions*kQueryCount
             <<" hits="<<oldHits<<" reference_ns/query="<<oldNs<<" candidate_ns/query="<<newNs
             <<" speedup="<<(oldNs/newNs)<<"x sink="<<benchmarkSink<<"\n";
}
}

int main() {
    benchmark(makeWorkload("far",0));
    benchmark(makeWorkload("mixed",1));
    benchmark(makeWorkload("overlapping",2));
}
