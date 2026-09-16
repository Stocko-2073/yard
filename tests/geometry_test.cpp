#include "geometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace yard::geometry;
static Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static void valid(const Mesh& m) {
    assert(!m.vertices.empty()&&!m.indices.empty()&&m.indices.size()%3==0);
    for (auto v:m.vertices) {
        assert(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z));
        assert(std::abs(dot(v.normal,v.normal)-1)<1e-4);
        assert(std::isfinite(v.uv.x)&&std::isfinite(v.uv.y));
    }
    for (std::size_t i=0;i<m.indices.size();i+=3) {
        for (int j=0;j<3;++j) assert(m.indices[i+j]<m.vertices.size());
        auto a=m.vertices[m.indices[i]],b=m.vertices[m.indices[i+1]],c=m.vertices[m.indices[i+2]];
        Vec3 n=cross(sub(b.position,a.position),sub(c.position,a.position));
        assert(dot(n,n)>0);
        assert(dot(n,a.normal)>0&&dot(n,b.normal)>0&&dot(n,c.normal)>0);
    }
}
template<class F> static void rejects(F f) {
    bool caught=false;
    try { f(); } catch (const std::invalid_argument&) { caught=true; }
    catch (const std::length_error&) { caught=true; }
    assert(caught);
}
int main() {
    std::array<Ring,2> rings={Ring{{0,0},{4,0},{4,4},{0,4}},Ring{{1,1},{1,3},{3,3},{3,1}}};
    auto m=polygon(rings); valid(m);
    float area=0;
    for (std::size_t i=0;i<m.indices.size();i+=3) {
        auto a=m.vertices[m.indices[i]].position,b=m.vertices[m.indices[i+1]].position,c=m.vertices[m.indices[i+2]].position;
        area+=cross(sub(b,a),sub(c,a)).z*.5f;
        float x=(a.x+b.x+c.x)/3,y=(a.y+b.y+c.y)/3;
        assert(!(x>1&&x<3&&y>1&&y<3));
    }
    assert(std::abs(area-12)<1e-5);
    for (auto& r:rings) std::reverse(r.begin(),r.end());
    valid(polygon(rings,{3,4,5},{1,0,0},{0,0,1}));
    std::array<Ring,1> concave={Ring{{0,0},{2,0},{2,1},{1,1},{1,2},{0,2}}};
    valid(polygon(concave));
    std::array<Sample,2> line={Sample{{0,0,0},1},Sample{{0,2,0},.5f}};
    auto profile=circle_profile(16);
    auto tube=sweep(line,profile); valid(tube);
    assert(tube.vertices.size()==66&&tube.indices.size()==180);
    // Seam duplicates agree in position/normal while U spans one repeat.
    for (std::size_t start:{0u,17u}) {
        auto a=tube.vertices[start],b=tube.vertices[start+16];
        assert(dot(sub(a.position,b.position),sub(a.position,b.position))==0);
        assert(dot(sub(a.normal,b.normal),sub(a.normal,b.normal))==0);
        assert(a.uv.x==0&&b.uv.x==1);
        assert(a.normal.y>0); // taper contributes to the side normal
    }
    valid(sweep(line,concave[0]));
    std::array<Bezier,1> curve={Bezier{{Vec3{0,0,0},Vec3{1,1,0},Vec3{-1,2,0},Vec3{0,3,0}},.4f,.1f}};
    auto coarse=sample_curve(curve,{.1f,1,16});
    auto fine=sample_curve(curve,{.001f,.1f,16});
    assert(fine.size()>coarse.size());
    assert(fine.front().position.y==0&&fine.back().position.y==3&&fine.back().radius==.1f);
    for (std::size_t i=1;i<fine.size();++i) {
        auto d=sub(fine[i].position,fine[i-1].position);
        assert(std::sqrt(dot(d,d))<=.10001f);
    }
    // Dense analytic samples independently check the requested centerline error.
    for (int i=0;i<=1000;++i) {
        float t=float(i)/1000, u=1-t;
        Vec3 point={3*u*u*t-3*u*t*t,3*u*u*t+6*u*t*t+3*t*t*t,0};
        float nearest=std::numeric_limits<float>::infinity();
        for (std::size_t j=1;j<fine.size();++j) {
            auto a=fine[j-1].position, d=sub(fine[j].position,a);
            float k=std::clamp(dot(sub(point,a),d)/dot(d,d),0.0f,1.0f);
            auto delta=sub(point,{a.x+k*d.x,a.y+k*d.y,a.z+k*d.z});
            nearest=std::min(nearest,std::sqrt(dot(delta,delta)));
        }
        assert(nearest<=.00101f);
    }
    valid(sweep(fine,profile));
    std::array<Bezier,2> chain={curve[0],Bezier{{Vec3{0,3,0},Vec3{1,4,0},
                                                        Vec3{1,5,1},Vec3{1,6,2}},.1f,.05f}};
    auto joined=sample_curve(chain);
    assert(std::count_if(joined.begin(),joined.end(),[](Sample p){return p.position.y==3;})==1);
    valid(sweep(joined,circle_profile(8)));
    // A constant-radius capped prism has its analytic polygonal volume.
    auto cylinder=sweep(std::array<Sample,2>{line[0],Sample{{0,2,0},1}},profile);
    double volume=0;
    for (std::size_t i=0;i<cylinder.indices.size();i+=3) {
        auto a=cylinder.vertices[cylinder.indices[i]].position;
        auto b=cylinder.vertices[cylinder.indices[i+1]].position;
        auto c=cylinder.vertices[cylinder.indices[i+2]].position;
        volume+=dot(a,cross(b,c))/6.0;
    }
    assert(std::abs(volume-16*std::sin(2*3.141592653589793/16))<1e-5);
    auto again=sweep(fine,profile);
    assert(again.indices==sweep(fine,profile).indices);
    // More than 16-bit addressable vertices must retain valid indices.
    std::vector<Sample> long_path;
    for (int i=0;i<4000;++i) long_path.push_back({{0,float(i)*.1f,0},1});
    auto large=sweep(long_path,profile,false); valid(large);
    assert(*std::max_element(large.indices.begin(),large.indices.end())>65535);
    rejects([&]{sample_curve(curve,{0,.1f,16});});
    rejects([&]{sample_curve(curve,{1e-6f,.01f,0});});
    rejects([&]{circle_profile(2);});
    rejects([&]{sweep(std::array<Sample,2>{line[0],line[0]},profile);});
    rejects([&]{sweep(std::array<Sample,3>{line[0],line[1],line[0]},profile);});
    rejects([&]{auto p=profile; std::reverse(p.begin(),p.end()); sweep(line,p);});
    rejects([&]{auto c=curve; c[0].points[1].x=std::numeric_limits<float>::quiet_NaN(); sample_curve(c);});
    rejects([&]{auto c=curve; c[0].end_radius=0; sample_curve(c);});
    rejects([&]{auto c=std::array<Bezier,2>{curve[0],curve[0]}; sample_curve(c);});
    rejects([&]{polygon(rings,{0,0,0},{2,0,0},{0,1,0});});
    rejects([&]{polygon(std::array<Ring,1>{Ring{{0,0},{1,0},{2,0}}});});
    rejects([&]{sweep(long_path,circle_profile(256));});
    std::puts("geometry tests passed");
}
