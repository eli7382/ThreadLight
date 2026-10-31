#include "unweaver_phase.h"
#include <iostream>
#include <limits>

int main(){
    using P=threadlight::unweaver::Progress;
    int failed=0;
    auto check=[&](bool value){if(!value)++failed;};
    auto advance=[&](P& progress,std::uint64_t ticks){
        for(std::uint64_t i=0;i<ticks;++i)progress.advanceTick();
    };

    // 59.99 seconds is 7199 simulation ticks; the first boundary is tick 7200.
    P clock;
    advance(clock,7199);
    check(clock.elapsedTicks==7199);
    check(clock.finishTick()==P::Boundary::None);
    check(clock.phase==1&&clock.vitality()==84000);
    clock.advanceTick();
    check(clock.elapsedTicks==7200);
    check(clock.finishTick()==P::Boundary::Transition);
    check(clock.phase==2&&clock.vitality()==84000);

    // Phase III starts at 120 seconds and Phase IV at 180 seconds.
    advance(clock,7199);
    check(clock.elapsedTicks==14399);
    check(clock.finishTick()==P::Boundary::None);
    clock.advanceTick();
    check(clock.finishTick()==P::Boundary::Transition);
    check(clock.phase==3&&clock.elapsedTicks==14400);
    advance(clock,7199);
    check(clock.elapsedTicks==21599);
    check(clock.finishTick()==P::Boundary::None);
    clock.advanceTick();
    check(clock.finishTick()==P::Boundary::Transition);
    check(clock.phase==4&&clock.elapsedTicks==21600);
    check(clock.finishTick()==P::Boundary::None);
    advance(clock,7200);
    check(clock.finishTick()==P::Boundary::None);
    check(clock.credit(std::numeric_limits<int>::max())==84000);
    check(clock.finishTick()==P::Boundary::Defeat);
    check(clock.defeated&&clock.finishTick()==P::Boundary::None);

    // The timer alone advances phases; experience bits remain diagnostic.
    P diagnostics;
    diagnostics.experience(0,3);
    check(diagnostics.experiences==0);
    diagnostics.experience(diagnostics.generation,1);
    check(diagnostics.experiences==1);
    advance(diagnostics,7200);
    check(diagnostics.finishTick()==P::Boundary::Transition);
    check(diagnostics.experiences==0&&diagnostics.generation==2);
    diagnostics.experience(1,3);
    check(diagnostics.experiences==0);
    diagnostics.experience(diagnostics.generation,2);
    check(diagnostics.experiences==2);

    // Lethal credited damage ends the encounter immediately in every phase,
    // even before that phase's timer boundary.
    for(int phase=1;phase<=4;++phase){
        P earlyLethal;
        earlyLethal.phase=phase;
        earlyLethal.elapsedTicks=static_cast<std::uint64_t>(phase-1)*7200u;
        check(earlyLethal.credit(std::numeric_limits<int>::max())==84000);
        check(earlyLethal.vitality()==0);
        check(earlyLethal.finishTick()==P::Boundary::Defeat);
        check(earlyLethal.defeated&&earlyLethal.phase==phase);
        check(earlyLethal.finishTick()==P::Boundary::None);
    }

    // If lethality and a timer boundary land on the same tick, defeat wins.
    P simultaneous;
    advance(simultaneous,7200);
    simultaneous.credit(std::numeric_limits<int>::max());
    check(simultaneous.finishTick()==P::Boundary::Defeat);
    check(simultaneous.phase==1&&simultaneous.generation==1);

    // Damage credit remains capped and rejects nonpositive or post-defeat hits.
    P damage;
    check(damage.credit(21000)==21000);
    check(damage.credit(std::numeric_limits<int>::max())==63000);
    check(damage.vitality()==0);
    check(damage.credit(900)==0&&damage.credit(-1)==0);
    check(damage.finishTick()==P::Boundary::Defeat);
    check(damage.credit(1)==0);

    // Large elapsed values still grant only one boundary per finishTick call.
    P overdue;
    overdue.elapsedTicks=21600;
    check(overdue.finishTick()==P::Boundary::Transition);
    check(overdue.phase==2);

    // The clock saturates instead of wrapping around.
    P saturated;
    saturated.elapsedTicks=std::numeric_limits<std::uint64_t>::max()-1;
    saturated.advanceTick();
    saturated.advanceTick();
    check(saturated.elapsedTicks==std::numeric_limits<std::uint64_t>::max());
    check(saturated.finishTick()==P::Boundary::Transition);

    P reset;
    check(reset.vitality()==84000&&reset.experiences==0&&reset.generation==1);
    check(reset.elapsedTicks==0&&reset.phase==1&&!reset.defeated);

    if(failed){std::cerr<<failed<<" phase checks failed\n";return 1;}
    std::cout<<"Unweaver simulation clock, immediate defeat, damage cap, stale epochs and idempotency passed\n";
}
