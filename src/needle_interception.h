#pragma once

#include "ranged_combat.h"
#include <span>

namespace threadlight::needle_interception {
using Point=foundation::Point;
struct Segment { Point a{},b{}; };
struct Candidate {
    std::uint64_t id=0;
    int pool=0;
    double time=std::numeric_limits<double>::infinity();
    Point position{};
};
namespace detail {
inline double circleTime(Point start,Point end,Point center,double radius){
    const double x=static_cast<double>(start.x)-center.x,y=static_cast<double>(start.y)-center.y;
    const double dx=static_cast<double>(end.x)-start.x,dy=static_cast<double>(end.y)-start.y;
    const double c=x*x+y*y-radius*radius;
    if(c<=0)return 0;
    const double a=dx*dx+dy*dy,b=2*(x*dx+y*dy);
    if(a<=1e-16)return std::numeric_limits<double>::infinity();
    const double discriminant=b*b-4*a*c;
    if(discriminant<0)return std::numeric_limits<double>::infinity();
    const double t=(-b-std::sqrt(discriminant))/(2*a);
    return t>=0&&t<=1?t:std::numeric_limits<double>::infinity();
}
inline double capsuleTime(Point start,Point end,Segment segment,double radius){
    double result=std::min(circleTime(start,end,segment.a,radius),circleTime(start,end,segment.b,radius));
    const double ax=static_cast<double>(segment.b.x)-segment.a.x,ay=static_cast<double>(segment.b.y)-segment.a.y;
    const double length=std::hypot(ax,ay);
    if(length<=1e-8)return result;
    const double ux=ax/length,uy=ay/length;
    const double px=static_cast<double>(start.x)-segment.a.x,py=static_cast<double>(start.y)-segment.a.y;
    const double dx=static_cast<double>(end.x)-start.x,dy=static_cast<double>(end.y)-start.y;
    const double along=px*ux+py*uy,normal=-px*uy+py*ux;
    const double normalDelta=-dx*uy+dy*ux;
    if(along>=0&&along<=length&&std::abs(normal)<=radius)return 0;
    if(std::abs(normalDelta)>1e-12)for(double sign:{-1.,1.}){
        const double t=(sign*radius-normal)/normalDelta;
        const double u=along+t*(dx*ux+dy*uy);
        if(t>=0&&t<=1&&u>=0&&u<=length)result=std::min(result,t);
    }
    return result;
}
inline double exitTime(Point start,Point end,ranged::Bounds arena){
    double t=1;
    const double dx=static_cast<double>(end.x)-start.x,dy=static_cast<double>(end.y)-start.y;
    if(dx>0)t=std::min(t,(arena.right-start.x)/dx);
    if(dx<0)t=std::min(t,(arena.left-start.x)/dx);
    if(dy>0)t=std::min(t,(arena.bottom-start.y)/dy);
    if(dy<0)t=std::min(t,(arena.top-start.y)/dy);
    return std::clamp(t,0.,1.);
}
}

// One chronological selection across BOTH projectile pools. A player contact
// occurring before (or at the same instant as) silk wins; interception never
// retroactively protects a player already struck during this update.
inline Candidate find(const ranged::Pool& ordinary,const ranged::Pool& boss,
                      std::span<const Segment> activeSilk,float halfWidth,
                      Point previousPlayer,Point player,float playerRadius,
                      ranged::Bounds arena){
    Candidate best;
    if(activeSilk.empty()||!std::isfinite(halfWidth)||halfWidth<0)return best;
    const ranged::Pool* pools[]={&ordinary,&boss};
    for(int pool=0;pool<2;++pool)for(const auto& projectile:pools[pool]->items()){
        if(projectile.state!=ranged::SlotState::Flying)continue;
        const Point start=projectile.p;
        if(!ranged::detail::inside(start,arena))continue;
        const Point end{start.x+projectile.velocity.x/120.f,start.y+projectile.velocity.y/120.f};
        const Point relativeStart{start.x-previousPlayer.x,start.y-previousPlayer.y};
        const Point relativeEnd{end.x-player.x,end.y-player.y};
        const double playerTime=detail::circleTime(relativeStart,relativeEnd,{},playerRadius+projectile.spec.radius);
        double silkTime=std::numeric_limits<double>::infinity();
        for(const auto& segment:activeSilk){
            if(!ranged::detail::finite(segment.a)||!ranged::detail::finite(segment.b))continue;
            const float radius=halfWidth+projectile.spec.radius;
            if(std::max(start.x,end.x)+radius<std::min(segment.a.x,segment.b.x)||
               std::min(start.x,end.x)-radius>std::max(segment.a.x,segment.b.x)||
               std::max(start.y,end.y)+radius<std::min(segment.a.y,segment.b.y)||
               std::min(start.y,end.y)-radius>std::max(segment.a.y,segment.b.y))continue;
            if(foundation::segmentsDistanceSquared(start,end,segment.a,segment.b)>radius*radius)continue;
            silkTime=std::min(silkTime,detail::capsuleTime(start,end,segment,radius));
            if(silkTime==0)break; // No earlier interception exists for this shot.
        }
        if(!std::isfinite(silkTime)||silkTime>detail::exitTime(start,end,arena)||silkTime+1e-7>=playerTime)continue;
        if(silkTime<best.time-1e-7||(std::abs(silkTime-best.time)<=1e-7&&
           (pool<best.pool||(pool==best.pool&&projectile.id<best.id)))){
            best={projectile.id,pool,silkTime,{static_cast<float>(start.x+(end.x-start.x)*silkTime),static_cast<float>(start.y+(end.y-start.y)*silkTime)}};
        }
    }
    return best;
}
} // namespace threadlight::needle_interception
