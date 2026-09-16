#include "geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <tuple>
#include <earcut.hpp>

namespace yard::geometry {
namespace {
constexpr std::size_t vertex_limit = 1'000'000;
constexpr float epsilon = 1e-6f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 mul(Vec3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float length(Vec3 a) { return std::sqrt(dot(a,a)); }
bool finite(float x) { return std::isfinite(x); }
bool finite(Vec3 a) { return finite(a.x)&&finite(a.y)&&finite(a.z); }
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
Vec3 unit(Vec3 a) {
    float n = length(a);
    require(finite(n)&&n>epsilon, "degenerate direction or triangle");
    return mul(a,1/n);
}
void budget(std::size_t n) {
    if (n>vertex_limit) throw std::length_error("geometry vertex/sample budget exceeded");
}
float segment_distance(Vec3 p, Vec3 a, Vec3 b) {
    Vec3 d = sub(b,a);
    float n = dot(d,d);
    return length(sub(p,add(a,mul(d,n>0 ? std::clamp(dot(sub(p,a),d)/n,0.0f,1.0f) : 0))));
}
void subdivide(const Bezier& b, Detail d, unsigned depth, std::vector<Sample>& out) {
    auto p = b.points;
    float error = std::max(segment_distance(p[1],p[0],p[3]),segment_distance(p[2],p[0],p[3]));
    if (error<=d.chord_error && length(sub(p[3],p[0]))<=d.max_segment_length) {
        require(length(sub(p[3],out.back().position))>epsilon,"coincident curve samples");
        budget(out.size()+1);
        out.push_back({p[3],b.end_radius});
        return;
    }
    if (depth==d.max_depth) throw std::length_error("curve detail exceeds subdivision depth");
    Vec3 a=mul(add(p[0],p[1]),.5f), c=mul(add(p[1],p[2]),.5f), e=mul(add(p[2],p[3]),.5f);
    Vec3 f=mul(add(a,c),.5f), g=mul(add(c,e),.5f), h=mul(add(f,g),.5f);
    float r=(b.start_radius+b.end_radius)*.5f;
    subdivide({{p[0],a,f,h},b.start_radius,r},d,depth+1,out);
    subdivide({{h,g,e,p[3]},r,b.end_radius},d,depth+1,out);
}
float ring_area(std::span<const Vec2> ring) {
    double a=0;
    for (std::size_t i=0;i<ring.size();++i) {
        auto p=ring[i], q=ring[(i+1)%ring.size()];
        require(finite(p.x)&&finite(p.y),"nonfinite polygon coordinate");
        require(std::hypot(p.x-q.x,p.y-q.y)>epsilon,"duplicate ring vertex");
        a+=double(p.x)*q.y-double(p.y)*q.x;
    }
    require(std::isfinite(a)&&std::abs(a)>epsilon,"zero area ring");
    return float(a*.5);
}
void triangle(Mesh& m, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    m.indices.insert(m.indices.end(),{a,b,c});
}
}

std::vector<Sample> sample_curve(std::span<const Bezier> curve, Detail d) {
    require(!curve.empty(),"empty curve");
    require(finite(d.chord_error)&&d.chord_error>0 && finite(d.max_segment_length)&&d.max_segment_length>epsilon
            && d.max_depth<=24,"invalid curve detail");
    std::vector<Sample> out;
    for (const auto& b:curve) {
        for (auto p:b.points) require(finite(p),"nonfinite curve point");
        require(finite(b.start_radius)&&finite(b.end_radius)&&b.start_radius>epsilon&&b.end_radius>epsilon,"invalid radius");
        if (out.empty()) out.push_back({b.points[0],b.start_radius});
        else require(length(sub(out.back().position,b.points[0]))<=epsilon &&
                     std::abs(out.back().radius-b.start_radius)<=epsilon,"disconnected curve");
        subdivide(b,d,0,out);
    }
    return out;
}
Ring circle_profile(unsigned segments) {
    require(segments>=3,"circle needs at least three segments");
    budget(segments);
    Ring ring;
    ring.reserve(segments);
    for (unsigned i=0;i<segments;++i) {
        float a=2*std::numbers::pi_v<float>*i/segments;
        ring.push_back({std::cos(a),std::sin(a)});
    }
    return ring;
}
Mesh polygon(std::span<const Ring> rings, Vec3 origin, Vec3 u, Vec3 v) {
    require(!rings.empty(),"empty polygon");
    require(finite(origin)&&finite(u)&&finite(v)&&std::abs(dot(u,u)-1)<epsilon*10 &&
            std::abs(dot(v,v)-1)<epsilon*10&&std::abs(dot(u,v))<epsilon*10,"invalid polygon plane");
    Mesh m;
    std::vector<std::vector<std::array<double,2>>> input;
    Vec3 n=cross(u,v);
    double expected_area=0;
    for (const auto& ring:rings) {
        require(ring.size()>=3,"ring needs three vertices");
        budget(m.vertices.size()+ring.size());
        float area=std::abs(ring_area(ring));
        expected_area+=input.empty()?area:-area;
        input.emplace_back();
        for (auto p:ring) {
            input.back().push_back({p.x,p.y});
            Vec3 position=add(origin,add(mul(u,p.x),mul(v,p.y)));
            require(finite(position),"polygon coordinate overflow");
            m.vertices.push_back({position,n,p});
        }
    }
    require(expected_area>epsilon,"invalid polygon area");
    m.indices=mapbox::earcut<std::uint32_t>(input);
    double actual_area=0;
    for (std::size_t i=0;i<m.indices.size();i+=3) {
        auto a=m.vertices[m.indices[i]].uv,b=m.vertices[m.indices[i+1]].uv,c=m.vertices[m.indices[i+2]].uv;
        double area=(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
        require(std::abs(area)>0,"degenerate polygon triangle");
        if (area<0) std::swap(m.indices[i+1],m.indices[i+2]);
        actual_area+=std::abs(area)*.5;
    }
    require(std::abs(actual_area-expected_area)<=std::max(1e-6,expected_area*1e-5),"polygon triangulation area mismatch");
    return m;
}
Mesh sweep(std::span<const Sample> path, std::span<const Vec2> profile, bool caps) {
    require(path.size()>=2&&profile.size()>=3,"sweep needs a path and closed profile");
    budget(path.size()); budget(profile.size());
    budget(path.size()*(profile.size()+1)+(caps?2*profile.size():0));
    require(ring_area(profile)>0,"sweep profile must be counterclockwise");
    for (auto s:path) require(finite(s.position)&&finite(s.radius)&&s.radius>epsilon,"invalid sweep sample");
    const auto count=profile.size(), stride=count+1;
    std::vector<Vec3> tangents(path.size()), frames(path.size());
    std::vector<float> distance(path.size()), perimeter(stride);
    for (std::size_t i=1;i<path.size();++i) {
        Vec3 delta=sub(path[i].position,path[i-1].position);
        unit(delta);
        distance[i]=distance[i-1]+length(delta);
        require(finite(distance[i]),"sweep length overflow");
    }
    for (std::size_t i=0;i<path.size();++i) {
        if (i==0) tangents[i]=unit(sub(path[1].position,path[0].position));
        else if (i+1==path.size()) tangents[i]=unit(sub(path[i].position,path[i-1].position));
        else tangents[i]=unit(add(unit(sub(path[i].position,path[i-1].position)),unit(sub(path[i+1].position,path[i].position))));
    }
    Vec3 t=tangents[0];
    frames[0]=unit(cross(std::abs(t.y)<.9f?Vec3{0,1,0}:Vec3{1,0,0},t));
    for (std::size_t i=1;i<path.size();++i) {
        Vec3 a=tangents[i-1], b=tangents[i], k=cross(a,b);
        float c=dot(a,b);
        require(c>-.9999f,"sweep tangent reverses");
        // Minimal rotation: no Frenet-frame flips at straight spans/inflections.
        Vec3 x=frames[i-1];
        x=add(x,add(cross(k,x),mul(cross(k,cross(k,x)),1/(1+c))));
        frames[i]=unit(sub(x,mul(b,dot(x,b))));
    }
    for (std::size_t j=1;j<=count;++j) {
        auto a=profile[j-1],b=profile[j%count];
        perimeter[j]=perimeter[j-1]+std::hypot(b.x-a.x,b.y-a.y);
    }
    require(finite(perimeter.back()),"profile perimeter overflow");
    Mesh m;
    m.vertices.reserve(path.size()*stride+(caps?2*count:0));
    m.indices.reserve(6*(path.size()-1)*count+(caps?6*(count-2):0));
    for (std::size_t i=0;i<path.size();++i) {
        Vec3 u=frames[i],v=cross(tangents[i],u);
        for (std::size_t j=0;j<=count;++j) {
            auto p=profile[j%count];
            Vec3 position=add(path[i].position,mul(add(mul(u,p.x),mul(v,p.y)),path[i].radius));
            require(finite(position),"sweep coordinate overflow");
            m.vertices.push_back({position,{0,0,0},{perimeter[j]/perimeter.back(),distance[i]}});
        }
    }
    for (std::size_t i=0;i+1<path.size();++i) for (std::size_t j=0;j<count;++j) {
        auto a=std::uint32_t(i*stride+j),b=a+1,c=std::uint32_t(a+stride),d=c+1;
        triangle(m,a,b,c); triangle(m,b,d,c);
    }
    for (std::size_t i=0;i<m.indices.size();i+=3) {
        auto a=m.indices[i],b=m.indices[i+1],c=m.indices[i+2];
        Vec3 n=cross(sub(m.vertices[b].position,m.vertices[a].position),sub(m.vertices[c].position,m.vertices[a].position));
        require(finite(n)&&length(n)>0,"degenerate sweep triangle");
        for (auto index:{a,b,c}) m.vertices[index].normal=add(m.vertices[index].normal,n);
    }
    for (std::size_t i=0;i<path.size();++i) {
        auto a=i*stride,b=a+count;
        Vec3 n=add(m.vertices[a].normal,m.vertices[b].normal);
        m.vertices[a].normal=m.vertices[b].normal=n;
    }
    for (auto& vertex:m.vertices) {
        float size=length(vertex.normal);
        require(finite(size)&&size>0,"degenerate sweep normal");
        vertex.normal=mul(vertex.normal,1/size);
    }
    if (caps) {
        std::array<Ring,1> rings={Ring(profile.begin(),profile.end())};
        auto cap=polygon(rings);
        for (auto i:{std::size_t(0),path.size()-1}) {
            auto offset=std::uint32_t(m.vertices.size());
            Vec3 n=mul(tangents[i],i==0?-1.0f:1.0f);
            for (std::size_t j=0;j<count;++j) m.vertices.push_back({m.vertices[i*stride+j].position,n,profile[j]});
            for (std::size_t k=0;k<cap.indices.size();k+=3) {
                auto a=cap.indices[k],b=cap.indices[k+1],c=cap.indices[k+2];
                if (i==0) std::swap(b,c);
                triangle(m,offset+a,offset+b,offset+c);
            }
        }
    }
    return m;
}
} // namespace yard::geometry
