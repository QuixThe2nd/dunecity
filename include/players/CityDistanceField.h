#ifndef CITY_DISTANCE_FIELD_H
#define CITY_DISTANCE_FIELD_H
#include <algorithm>
#include <vector>

// Exact obstacle-free Chebyshev distances. Rebuilt for each placement search,
// including that search's reservations; nothing survives a planning decision.
class CityDistanceField {
public:
    static constexpr int missing = 1000000;
    CityDistanceField(int width, int height) : width_(width), height_(height), cells_(width*height, missing) {}
    void add(int x, int y, int w=1, int h=1) {
        for (int yy=std::max(0,y); yy<std::min(height_,y+h); ++yy)
            for (int xx=std::max(0,x); xx<std::min(width_,x+w); ++xx)
                cells_[yy*width_+xx]=0;
    }
    void build() {
        // The forward/backward chamfer passes are exact for the unit-cost
        // eight-neighbour metric used by city placement (no terrain costs).
        for (int y=0; y<height_; ++y) for (int x=0; x<width_; ++x) {
            int& d=cells_[y*width_+x];
            if (x>0) d=std::min(d,get(x-1,y)+1);
            if (y>0) for (int dx=-1; dx<=1; ++dx) d=std::min(d,get(x+dx,y-1)+1);
        }
        for (int y=height_-1; y>=0; --y) for (int x=width_-1; x>=0; --x) {
            int& d=cells_[y*width_+x];
            if (x+1<width_) d=std::min(d,get(x+1,y)+1);
            if (y+1<height_) for (int dx=-1; dx<=1; ++dx) d=std::min(d,get(x+dx,y+1)+1);
        }
    }
    int get(int x, int y) const {
        return x<0 || y<0 || x>=width_ || y>=height_ ? missing : cells_[y*width_+x];
    }
    int footprint(int x, int y, int w, int h) const {
        int distance=missing;
        for (int yy=y; yy<y+h; ++yy) for (int xx=x; xx<x+w; ++xx)
            distance=std::min(distance,get(xx,yy));
        return distance;
    }
private:
    int width_,height_;
    std::vector<int> cells_;
};
#endif
