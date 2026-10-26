#pragma once

#include "player_upgrades.h"
#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace threadlight::upgrade_ui {

struct WrappedCardText {
    std::vector<std::string> name;
    std::vector<std::string> effect;
};

// The game is paused during offers. Rebuild only when text inputs or the
// available card width change, including health-dependent recovery text.
class OfferTextCache {
public:
    template<class Measure>
    bool update(const upgrades::Offer& offer, const upgrades::Ranks& ranks,
                const combat::PlayerHealth& health, float width, Measure measure) {
        const Key next{offer.cards,ranks.value,offer.count,health.hp,health.maxHp,width};
        if (valid_ && next == key_) return false;
        key_=next;valid_=true;
        const int count=std::clamp(offer.count,0,3);
        for(int i=0;i<3;++i){
            cards_[i]={};
            if(i>=count)continue;
            const auto id=offer.cards[i];
            cards_[i].name=wrap(upgrades::name(id),width,17,measure);
            cards_[i].effect=wrap(upgrades::effectText(id,ranks,health),width,15,measure);
        }
        return true;
    }

    const WrappedCardText& card(std::size_t index) const { return cards_[index]; }

private:
    struct Key {
        std::array<upgrades::Id,3> cards{};
        std::array<int,upgrades::kPermanentCount> ranks{};
        int count=0,hp=0,maxHp=0;
        float width=0;
        bool operator==(const Key&) const = default;
    };

    template<class Measure>
    static std::vector<std::string> wrap(const std::string& text,float width,int size,Measure& measure){
        std::vector<std::string> lines;
        std::string line;std::size_t start=0;
        while(start<text.size()){
            const auto end=text.find(' ',start);
            const auto word=text.substr(start,end==std::string::npos?end:end-start);
            const auto candidate=line.empty()?word:line+" "+word;
            if(!line.empty()&&measure(candidate.c_str(),size)>width){lines.push_back(std::move(line));line=word;}
            else line=candidate;
            if(end==std::string::npos)break;
            start=end+1;
        }
        if(!line.empty())lines.push_back(std::move(line));
        return lines;
    }

    Key key_{};
    bool valid_=false;
    std::array<WrappedCardText,3> cards_{};
};

} // namespace threadlight::upgrade_ui
