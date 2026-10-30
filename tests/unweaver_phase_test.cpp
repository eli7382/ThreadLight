#include "unweaver_phase.h"
#include <iostream>
#include <limits>
int main(){
    using P=threadlight::unweaver::Progress;int failed=0;
    auto check=[&](bool v){if(!v)++failed;};
    P p;check(p.credit(21000)==21000);check(p.finishTick()==P::Boundary::None);
    p.experience(1,1);check(p.finishTick()==P::Boundary::None);
    p.experience(1,2);check(p.finishTick()==P::Boundary::Transition);
    p.experience(1,3);check(p.experiences==0);check(p.vitality()==63000);
    check(p.credit(std::numeric_limits<int>::max())==63000);check(p.vitality()==0&&!p.defeated);
    check(p.credit(900)==0);check(p.credit(-1)==0);
    for(int phase=2;phase<=4;++phase){
        check(p.phase==phase);check(p.finishTick()==P::Boundary::None);
        p.experience(p.generation,3);p.experience(p.generation,3);
        check(p.finishTick()==(phase==4?P::Boundary::Defeat:P::Boundary::Transition));
    }
    check(p.defeated&&p.finishTick()==P::Boundary::None&&p.credited==84000);
    P reset;check(reset.vitality()==84000&&reset.experiences==0&&reset.generation==1);
    if(failed){std::cerr<<failed<<" phase checks failed\n";return 1;}
    std::cout<<"Unweaver damage ledger, thresholds, overflow, stale events and terminal idempotency passed\n";
}
