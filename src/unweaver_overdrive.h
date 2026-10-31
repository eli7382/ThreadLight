#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace threadlight::unweaver {
struct Overdrive {
    bool active=false;
    std::uint64_t age=0;
    std::array<float,9> reward{},attempts{};
    int damage=0,kills=0;
    void tick(int phase,std::uint64_t elapsed){
        if(phase>=4&&elapsed>=36000){active=true;age=elapsed-36000;}
    }
    void attempt(int action){
        if(!active||action<0||action>=9)return;
        for(int i=0;i<9;++i){reward[i]*=.94f;attempts[i]*=.94f;}
        attempts[action]+=1;
    }
    void outcome(int action,int amount,bool killed){
        if(!active||action<0||action>=9||amount<=0)return;
        damage+=amount;reward[action]+=amount/100.f;
        if(killed){++kills;reward[action]+=160.f;}
    }
    float utility(int action)const{return active?std::min(6.f,reward[action]/(attempts[action]+1.f)*.15f):0;}
    int warning(int normal)const{return active?std::max(48,normal*2/3):normal;}
    int recovery(int normal)const{return active?std::min(48,normal):normal;}
};
}
