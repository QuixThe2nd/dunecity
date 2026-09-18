#ifndef CURSORAPPEARANCE_H
#define CURSORAPPEARANCE_H
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace CursorAppearance {
enum class Action { Pointer, Move, Attack, Capture, Drop, Heal };
inline SDL_Point hotspot(Action action, float scale) {
    const float factor = scale / 1.5f;
    const int coordinate = static_cast<int>(std::lround((action == Action::Pointer ? 6 : 16) * factor));
    return {coordinate, coordinate};
}
inline float segmentDistance(SDL_FPoint p, SDL_FPoint a, SDL_FPoint b) {
    const float dx=b.x-a.x, dy=b.y-a.y;
    const float t=std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy),0.0f,1.0f);
    return std::hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);
}
inline float polygonDistance(SDL_FPoint p, std::initializer_list<SDL_FPoint> points) {
    bool inside=false;
    float distance=1000;
    auto a=*(points.end()-1);
    for(auto b:points) {
        if((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
        distance=std::min(distance,segmentDistance(p,a,b));
        a=b;
    }
    return inside ? -distance : distance;
}
inline float shapeDistance(Action action, SDL_FPoint p) {
    switch(action) {
        case Action::Pointer:
            // A compact, slightly rounded arrow, with its tip at the hotspot.
            return polygonDistance(p,{{4,4},{23,13},{14,15},{10,25}})-0.3f;
        case Action::Move:
            return polygonDistance(p,{{14,3},{19,8},{16,8},{16,12},{20,12},{20,9},{25,14},{20,19},{20,16},{16,16},{16,20},{19,20},{14,25},{9,20},{12,20},{12,16},{8,16},{8,19},{3,14},{8,9},{8,12},{12,12},{12,8},{9,8}});
        case Action::Attack:
            return std::min({std::abs(std::hypot(p.x-14,p.y-14)-7)-1.2f,
                segmentDistance(p,{14,2},{14,8})-1.1f,segmentDistance(p,{14,20},{14,26})-1.1f,
                segmentDistance(p,{2,14},{8,14})-1.1f,segmentDistance(p,{20,14},{26,14})-1.1f,
                std::hypot(p.x-14,p.y-14)-1.5f});
        case Action::Capture:
            return std::min({segmentDistance(p,{7,4},{7,24})-1.3f,
                polygonDistance(p,{{8,5},{23,5},{19,10},{23,15},{8,15}}),
                segmentDistance(p,{4,24},{14,24})-1.1f});
        case Action::Drop:
            return std::min({polygonDistance(p,{{12,3},{16,3},{16,12},{22,12},{14,20},{6,12},{12,12}}),
                segmentDistance(p,{5,21},{5,25})-1,segmentDistance(p,{5,25},{23,25})-1,
                segmentDistance(p,{23,25},{23,21})-1});
        case Action::Heal:
            return polygonDistance(p,{{11,4},{17,4},{17,11},{24,11},{24,17},{17,17},{17,24},{11,24},{11,17},{4,17},{4,11},{11,11}})-0.2f;
    }
    return 1000;
}
// The same vector geometry creates platform cursors for SDL desktop and web.
// Supersampling at the chosen size keeps the outline smooth at fractional scales.
inline SDL_Surface* create(Action action, float scale) {
    if(!std::isfinite(scale) || scale<1 || scale>4) return nullptr;
    const float factor=scale/1.5f;
    const int extent=static_cast<int>(std::ceil(33*factor));
    auto* surface=SDL_CreateRGBSurfaceWithFormat(0,extent,extent,32,SDL_PIXELFORMAT_RGBA32);
    if(!surface) return nullptr;
    constexpr int samples=4;
    for(int y=0;y<extent;++y) for(int x=0;x<extent;++x) {
        int coverage=0, brightness=0;
        for(int sy=0;sy<samples;++sy) for(int sx=0;sx<samples;++sx) {
            const SDL_FPoint p{(x+(sx+0.5f)/samples)/factor-2,(y+(sy+0.5f)/samples)/factor-2};
            const float distance=shapeDistance(action,p)*factor;
            if(distance<1.75f) {
                ++coverage;
                brightness+=distance<=0 ? 16 : 255;
            }
        }
        const Uint8 color=coverage ? brightness/coverage : 0;
        const Uint8 alpha=coverage*255/(samples*samples);
        auto* row=reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels)+y*surface->pitch);
        row[x]=SDL_MapRGBA(surface->format,color,color,color,alpha);
    }
    return surface;
}
}
#endif
