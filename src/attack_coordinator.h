#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <span>

namespace threadlight::attacks {

enum class Kind { Skitter, Spitter };
struct Request { std::uint64_t id; Kind kind; };
struct Grant { std::uint64_t id=0; Kind kind=Kind::Skitter; };
struct Grants {
    std::array<Grant,2> items{};int count=0;
    bool contains(std::uint64_t id) const {
        for(int i=0;i<count;++i)if(items[i].id==id)return true;
        return false;
    }
};

// Caller supplies all current ready requests before ticking any enemy. Only
// Chapter II uses this coordinator; Chapter I's accepted commitment loop stays.
class Coordinator {
public:
    Grants choose(std::span<const Request> ready,int activeCommitments,
                  int projectileSlots,std::uint64_t tick) {
        // Removing dead/canceled/non-ready IDs also prevents stale future grants.
        int kept=0;
        for(int i=0;i<count_;++i){
            bool present=false;
            for(const auto& request:ready)if(request.id==queue_[i].request.id){present=true;break;}
            if(present)queue_[kept++]=queue_[i];
        }
        count_=kept;
        bool added=false;
        for(const auto& request:ready){
            if(!request.id)continue;
            bool found=false;
            for(int i=0;i<count_;++i)if(queue_[i].request.id==request.id){found=true;break;}
            if(!found&&count_<static_cast<int>(queue_.size())){queue_[count_++]={request,tick};added=true;}
        }
        // Pruning and granting preserve order. Only new arrivals can change it.
        if(added)std::sort(queue_.begin(),queue_.begin()+count_,[](const Waiting& a,const Waiting& b){
            return a.since<b.since||(a.since==b.since&&a.request.id<b.request.id);
        });
        Grants result;
        const int available=std::clamp(2-activeCommitments,0,2);
        while(result.count<available&&count_){
            const auto next=queue_[0].request;
            // Hold the head's place instead of repeatedly letting younger
            // Skitters take the opening and starve a queued ranged warning.
            if(next.kind==Kind::Spitter){
                if(projectileSlots<=0||(hadSpitter_&&tick-lastSpitterTick_<42))break;
                --projectileSlots;lastSpitterTick_=tick;hadSpitter_=true;
            }
            result.items[result.count++]={next.id,next.kind};
            for(int i=1;i<count_;++i)queue_[i-1]=queue_[i];
            --count_;
        }
        return result;
    }
    void clear(){count_=0;hadSpitter_=false;lastSpitterTick_=0;}
    int waiting() const{return count_;}
private:
    struct Waiting { Request request{};std::uint64_t since=0; };
    std::array<Waiting,64> queue_{};int count_=0;
    std::uint64_t lastSpitterTick_=0;bool hadSpitter_=false;
};

} // namespace threadlight::attacks
