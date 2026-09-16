// C++ adaptation of tree-gen by Charlie Hewitt and Sami Pflibsen-Jones.
// SPDX-License-Identifier: GPL-3.0-only
#include "tree.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace yard::tree {
namespace {
constexpr double pi = std::numbers::pi;
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, double s) { return {float(a.x*s),float(a.y*s),float(a.z*s)}; }
double dot(Vec3 a, Vec3 b) { return double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double norm(Vec3 a) { return std::sqrt(dot(a,a)); }
Vec3 unit(Vec3 a) { double n=norm(a); return n>1e-12 ? a*(1/n) : Vec3{0,0,0}; }
Vec3 rotate(Vec3 v, Vec3 axis, double degrees) {
    if (norm(axis)<1e-12) return v;
    axis=unit(axis); double a=degrees*pi/180, c=std::cos(a), s=std::sin(a);
    return v*c+cross(axis,v)*s+axis*(dot(axis,v)*(1-c));
}
Vec3 world(Vec3 v) { return {v.x,v.z,-v.y}; }
Vec3 upstream(Vec3 v) { return {v.x,-v.z,v.y}; }
struct Turtle {
    Vec3 pos{0,0,0}, dir{0,0,1}, right{1,0,0};
    void move(double d) { pos=pos+dir*d; }
    void spin(Vec3 axis,double a) { dir=unit(rotate(dir,axis,a)); right=unit(rotate(right,axis,a)); }
    void turn(double a) { spin(cross(dir,right),a); }
    void pitch(double a) { dir=unit(rotate(dir,right,-a)); }
    void roll(double a) { right=unit(rotate(right,dir,a)); }
};
// MT19937 init_by_array and 53-bit draws matching Python's integer random.seed.
// Keep an owned generator: upstream saves/restores it around child recursion.
class Random {
    std::array<std::uint32_t,624> mt{};
    unsigned index=624;
    std::uint32_t next() {
        if (index==624) {
            for(unsigned i=0;i<624;++i) {
                auto y=(mt[i]&0x80000000u)|(mt[(i+1)%624]&0x7fffffffu);
                mt[i]=mt[(i+397)%624]^(y>>1)^((y&1)?0x9908b0dfu:0);
            }
            index=0;
        }
        auto y=mt[index++]; y^=y>>11; y^=(y<<7)&0x9d2c5680u;
        y^=(y<<15)&0xefc60000u; return y^(y>>18);
    }
public:
    explicit Random(std::uint32_t seed) {
        mt[0]=19650218;
        for(unsigned i=1;i<624;++i) mt[i]=1812433253u*(mt[i-1]^(mt[i-1]>>30))+i;
        unsigned i=1;
        for(unsigned k=0;k<624;++k) {
            mt[i]=(mt[i]^((mt[i-1]^(mt[i-1]>>30))*1664525u))+seed;
            if(++i>=624) {mt[0]=mt[623];i=1;}
        }
        for(unsigned k=0;k<623;++k) {
            mt[i]=(mt[i]^((mt[i-1]^(mt[i-1]>>30))*1566083941u))-i;
            if(++i>=624) {mt[0]=mt[623];i=1;}
        }
        mt[0]=0x80000000u;
    }
    double draw() { auto a=next()>>5,b=next()>>6; return (a*67108864.0+b)/9007199254740992.0; }
    double range(double lo,double hi) { return lo+draw()*(hi-lo); }
    double variation() { return range(-1,1); }
};
Vec3 position(const Point& a,const Point& b,double t) {
    double u=1-t;
    return a.position*(u*u*u)+a.handle_right*(3*u*u*t)+b.handle_left*(3*u*t*t)+b.position*(t*t*t);
}
Vec3 tangent(const Point& a,const Point& b,double t) {
    double u=1-t;
    return (a.handle_right-a.position)*(3*u*u)+(b.handle_left-a.handle_right)*(6*u*t)+(b.position-b.handle_left)*(3*t*t);
}
// Frame equivalent to Blender's direction.to_track_quat('Z', 'Y').
Vec3 track(Vec3 v,Vec3 direction) {
    Vec3 z=unit(direction), x=unit(cross(Vec3{0,0,1},z));
    if(norm(x)<1e-8) x={-1,0,0};
    Vec3 y=unit(cross(z,x));
    return x*v.x+y*v.y+z*v.z;
}
#include "tree_presets.inc"
struct Stem {
    unsigned depth=0;
    const Stem* parent=nullptr;
    std::size_t id=no_parent;
    double offset=0, limit=-1, length=0, radius=0, child_max=0;
};
class Generator {
    const Parameters& p;
    Random rng;
    Limits limits;
    Skeleton out;
    double scale=0,base_length=0;
    std::array<double,4> split_error{};
    std::size_t point_count=0;
    unsigned active_calls=0;
    double shape(int type,double r) const {
        switch(type) {
        case 1: return .2+.8*std::sin(pi*r);
        case 2: return .2+.8*std::sin(.5*pi*r);
        case 3: return 1;
        case 4: return .5+.5*r;
        case 5: return r<=.7?r/.7:(1-r)/.3;
        case 6: return 1-.8*r;
        case 7: return r<=.7?.5+.5*r/.7:.5+.5*(1-r)/.3;
        case 8:
            if(r<0||r>1) return 0;
            return r<1-p.prune_width_peak?std::pow(r/(1-p.prune_width_peak),p.prune_power_high):
                std::pow((1-r)/(1-p.prune_width_peak),p.prune_power_low);
        default:return .2+.8*r;
        }
    }
    double length(Stem& s) {
        if(s.depth==0) return std::max(0.0,scale*(p.length[0]+rng.variation()*p.length_v[0]));
        if(s.depth==1) return std::max(0.0,s.parent->length*s.parent->child_max*
            shape(p.shape,(s.parent->length-s.offset)/(s.parent->length-base_length)));
        return std::max(0.0,s.parent->child_max*(s.parent->length-.7*s.offset));
    }
    double radius(const Stem& s) const {
        if(s.depth==0) return s.length*p.ratio*p.radius_mod[0];
        return std::min(s.limit,std::max(.005,p.radius_mod[s.depth]*s.parent->radius*
                         std::pow(s.length/s.parent->length,p.ratio_power)));
    }
    double radius_at(const Stem& s,double z) const {
        double n=p.taper[s.depth],u=n<1?n:n<2?2-n:0;
        double taper=s.radius*(1-u*z),r=taper;
        if(n>=1 && taper>0) {
            double z2=(1-z)*s.length,depth=n<2||z2<taper?1:n-2;
            double z3=n<2?z2:std::abs(z2-2*taper*int(z2/(2*taper)+.5));
            if(!(n<2&&z3>=taper)) r=(1-depth)*taper+depth*std::sqrt(std::max(0.0,taper*taper-(z3-taper)*(z3-taper)));
        }
        if(s.depth==0) r*=p.flare*((std::pow(100,std::max(0.0,1-8*z))-1)/100)+1;
        return r;
    }
    double curve_angle(unsigned d,int seg) {
        double res=p.curve_res[d];
        double a=p.curve_back[d]==0?p.curve[d]/res:
                 (seg<res/2?p.curve[d]:p.curve_back[d])/(res/2);
        return a+rng.variation()*p.curve_v[d]/res;
    }
    void tropism(Turtle& t,unsigned d) const {
        Vec3 v{float(p.tropism[0]),float(p.tropism[1]),d>1?float(p.tropism[2]):0};
        auto axis=cross(t.dir,v); t.spin(axis,10*norm(axis));
    }
    struct Helix { Vec3 a,b,c,axis; };
    Helix helix(Turtle& t,const Stem& s) {
        double pitch=2*s.length/p.curve_res[s.depth]*rng.range(.8,1.2);
        double rad=3*pitch/(16*std::tan((90-std::abs(p.curve_v[s.depth]))*pi/180))*rng.range(.8,1.2);
        tropism(t,s.depth);
        double spin=rng.range(0,360);
        auto transform=[&](Vec3 v){return track(rotate(v,{0,0,1},spin),t.dir);};
        auto a=transform({0,float(-rad),float(-pitch/4)});
        return {transform({float(4*rad/3),float(-rad),0})-a,
                transform({float(4*rad/3),float(rad),0})-a,
                transform({0,float(rad),float(pitch/4)})-a,t.dir};
    }
    bool inside(Vec3 v) const {
        double r=(scale-v.z)/(scale*(1-p.base_size[0]));
        return std::hypot(v.x,v.y)/scale<p.prune_width*shape(8,r);
    }
    int splits(const Stem& s,int seg,double& prob,bool testing,bool* attempted=nullptr) {
        int base=int(std::ceil(p.base_size[0]*p.curve_res[0])),n=0;
        if(p.base_splits>0&&s.depth==0&&seg==base)
            n=testing?int(rng.draw()*(p.base_splits+.5)):p.base_splits;
        else if(p.seg_splits[s.depth]>0&&seg<p.curve_res[s.depth]&&(s.depth>0||seg>base)) {
            if(rng.draw()<=prob) {
                if(attempted)*attempted=true;
                n=int(p.seg_splits[s.depth]+split_error[s.depth]);
                split_error[s.depth]-=n-p.seg_splits[s.depth]; prob/=n+1;
            }
        }
        return n;
    }
    struct SplitAngles { double split,spread; bool base,direct; };
    SplitAngles angles(Turtle& t,const Stem& s,int seg,double& correction) {
        SplitAngles a{};
        a.base=p.base_splits>0&&s.depth==0&&seg==int(std::ceil(p.base_size[0]*p.curve_res[0]));
        a.direct=p.split_angle[s.depth]<0;
        if(a.direct) {a.spread=std::abs(p.split_angle[s.depth])+rng.variation()*p.split_angle_v[s.depth];correction=0;}
        else {
            double decl=std::atan2(std::hypot(t.dir.x,t.dir.y),t.dir.z)*180/pi;
            a.split=std::max(0.0,p.split_angle[s.depth]+rng.variation()*p.split_angle_v[s.depth]-decl);
            correction=a.split/(p.curve_res[s.depth]+1-seg);
            double r=rng.draw(); a.spread=-(20+.75*(30+std::abs(decl-90)*r*r));
        }
        return a;
    }
    void bend(Turtle& t,const Stem& s,int seg,double correction) {
        t.turn(-rng.variation()*p.bend_v[s.depth]/p.curve_res[s.depth]);
        t.pitch(curve_angle(s.depth,seg)-correction);
    }
    bool test_stem(Turtle t,const Stem& s,int start,double correction,double prob) {
        unsigned d=s.depth; int res=int(p.curve_res[d]);
        if(p.rotate[std::min(d+1,3u)]>=0) rng.range(0,360);
        Helix h{}; if(p.curve_v[d]<0) h=helix(t,s);
        Vec3 previous{};
        for(int seg=start;seg<=res;++seg) {
            if(p.curve_v[d]<0) {
                if(seg==1) t.pos=t.pos+h.c;
                else if(seg>1) {h.c=rotate(h.c,h.axis,(seg-1)*180);t.pos=previous+h.c;}
                previous=t.pos;
            } else if(seg!=start) {
                t.move(s.length/res);
                if(!(d==0&&start<int(std::ceil(p.base_size[0]*p.curve_res[0])))&&!inside(t.pos)) return false;
            }
            if(seg>start&&p.curve_v[d]>=0) {
                int n=splits(s,seg,prob,true);
                if(n>0) {
                    auto a=angles(t,s,seg,correction);t.pitch(a.split/2);
                    if(!a.base&&n==1) {if(a.direct)t.turn(-a.spread/2);else t.spin({0,0,1},-a.spread/2);}
                } else bend(t,s,seg,correction);
                tropism(t,d);
            }
        }
        return inside(t.pos);
    }
    double down_angle(const Stem& s,double offset) {
        unsigned d=std::min(s.depth+1,3u);
        if(p.down_angle_v[d]>=0) return p.down_angle[d]+rng.variation()*p.down_angle_v[d];
        double a=p.down_angle[d]+p.down_angle_v[d]*(1-2*shape(0,(s.length-offset)/(s.length*(1-p.base_size[s.depth]))));
        return a+rng.variation()*std::abs(a*.1);
    }
    struct Child { Turtle position,direction; double radius,offset; };
    Child setup(Turtle t,const Stem& s,int mode,double offset,const Point& a,const Point& b,
                double stem_offset,int index,double& previous,int group=0) {
        unsigned d=std::min(s.depth+1,3u);
        Turtle dir; dir.dir=unit(tangent(a,b,offset));
        dir.right=p.curve_v[s.depth]<0?cross(dir.dir,unit(tangent(a,b,offset+.0001))):
            cross(cross(t.dir,t.right),dir.dir);
        double limit=0;
        if(mode==2) dir.turn((group==1?0:p.rotate[d]*(double(index)/(group-1)-.5)+rng.variation()*p.rotate_v[d]));
        else {
            double angle;
            if(mode==1) angle=previous+360.0*index/group+rng.variation()*p.rotate_v[d];
            else if(p.rotate[d]>=0) {angle=std::fmod(previous+p.rotate[d]+rng.variation()*p.rotate_v[d],360);if(angle<0)angle+=360;previous=angle;}
            else {angle=previous*(180+p.rotate[d]+rng.variation()*p.rotate_v[d]);previous=-previous;}
            dir.roll(angle); limit=radius_at(s,stem_offset/s.length);
        }
        dir.pos=position(a,b,offset);Turtle pos=dir;pos.pitch(90);pos.move(limit);
        dir.pitch(down_angle(s,stem_offset));
        return {pos,dir,limit,stem_offset};
    }
    void children(Turtle t,const Stem& s,int seg,int count,double& previous,bool leaves,
                  const Point& a,const Point& b) {
        std::vector<Child> children;
        unsigned d=std::min(s.depth+1,3u);
        if(count<0) {
            for(int i=0;i<std::abs(count);++i) children.push_back(setup(t,s,2,1,a,b,1,i,previous,std::abs(count)));
        } else {
            double base=s.length*p.base_size[s.depth],dist=p.branch_dist[d];
            if(dist>1) {
                int whorls=int(count/(dist+1));double error=0;
                for(int w=0;w<whorls;++w) {
                    double offset=double(w)/whorls,so=(seg-1+offset)/p.curve_res[s.depth]*s.length;
                    if(so>base) {
                        int n=int(dist+1+error);error-=n-(dist+1);
                        for(int i=0;i<n;++i)children.push_back(setup(t,s,1,offset,a,b,so,i,previous,n));
                    }
                    previous+=p.rotate[d];
                }
            } else for(int i=0;i<count;++i) {
                double offset=std::clamp((i%2==0?double(i):i-dist)/count,0.0,1.0);
                double so=(seg-1+offset)/p.curve_res[s.depth]*s.length;
                if(so>base) children.push_back(setup(t,s,0,offset,a,b,so,i,previous));
            }
        }
        for(auto& child:children) {
            if(leaves) {
                if(out.leaves.size()>=limits.leaves)throw std::length_error("tree leaf budget");
                out.leaves.push_back({s.id,child.position.pos,child.direction.dir,child.direction.right,false});
            } else {
                Stem next;next.depth=d;next.parent=&s;next.offset=child.offset;next.limit=child.radius;
                make(child.direction,next,0,0,1,1,&child.position,nullptr,s.id,false);
            }
        }
    }
    void add_point(std::vector<Point>& points,Point v) {
        if(++point_count>limits.points)throw std::length_error("tree point budget");
        points.push_back(v);
    }
    void make(Turtle t,Stem s,int start=0,double correction=0,double factor=1,double prob=1,
              const Turtle* position_correction=nullptr,const Turtle* cloned=nullptr,
              std::size_t attachment=no_parent,bool is_split=false) {
        if(active_calls>=256)throw std::length_error("tree recursion budget");
        struct CallGuard {
            unsigned& count;
            explicit CallGuard(unsigned& n):count(n) {++count;}
            ~CallGuard() {--count;}
        } guard(active_calls);
        if(s.limit>=0&&s.limit<.0001)return;
        unsigned d=s.depth,next=std::min(d+1,3u);
        if(start==0) {
            s.child_max=p.length[next]+rng.variation()*p.length_v[next];
            s.length=length(s);s.radius=radius(s);if(d==0)base_length=s.length*p.base_size[0];
        }
        if(s.length<=0||s.radius<=0)return;
        if(position_correction) {auto pos=*position_correction;pos.move(-std::min(s.radius,s.limit));t.pos=pos.pos;}
        if(!cloned&&p.prune_ratio>0) {
            double original=s.length;auto saved=rng;auto errors=split_error;
            bool fits=test_stem(t,s,start,correction,prob);
            while(!fits) {
                s.length*=.9;
                if(s.length<.15*original) {if(p.prune_ratio<1){s.length=0;break;}else return;}
                rng=saved;split_error=errors;fits=test_stem(t,s,start,correction,prob);
                errors=split_error; // upstream aliases the saved error list after the first retry
            }
            s.length=original*(1-p.prune_ratio)+s.length*p.prune_ratio;s.radius=radius(s);
            rng=saved;split_error=errors;
        }
        if(s.length<=0||s.radius<=0)return;
        if(out.branches.size()>=limits.branches)throw std::length_error("tree branch budget");
        s.id=out.branches.size();out.branches.push_back({attachment,d,is_split,is_split?start*s.length/p.curve_res[d]:s.offset,s.length,s.radius,{}});
        // Keep working storage local: recursive vector growth cannot invalidate it.
        std::vector<Point> points;
        int res=int(p.curve_res[d]);double seg_length=s.length/res;
        double leaf_count=0,branch_count=0;
        if(int(d)==p.levels-1&&d>0&&p.leaf_blos_num!=0) {
            leaf_count=p.leaf_blos_num<0?p.leaf_blos_num:p.leaf_blos_num*scale/p.g_scale*
                (s.length/(s.parent->child_max*s.parent->length));
            leaf_count*=1-double(start)/res;
        } else {
            if(d==0)branch_count=p.branches[next]*(rng.draw()*.2+.9);
            else if(p.branches[next]<0)branch_count=p.branches[next];
            else if(d==1)branch_count=p.branches[next]*(.2+.8*((s.length/s.parent->length)/s.parent->child_max));
            else branch_count=p.branches[next]*(1-.5*s.offset/s.parent->length);
            branch_count=branch_count/(1-p.base_size[d])*(1-double(start)/res)*factor;
        }
        double branches_per=branch_count/res,leaves_per=leaf_count/res,branch_error=0,leaf_error=0;
        double previous=p.rotate[next]>=0?rng.range(0,360):1;
        Helix h{};if(p.curve_v[d]<0)h=helix(t,s);
        int max_points=int(std::ceil(std::max(1.0,100.0/res))),density=d==0||p.taper[d]>1?max_points:2;
        for(int seg=start;seg<=res;++seg) {
            Point pt{};
            if(p.curve_v[d]<0) {
                if(seg==0) {pt.position=t.pos;pt.handle_right=t.pos+h.a;pt.handle_left=t.pos;}
                else if(seg==1) {pt.position=t.pos+h.c;pt.handle_left=t.pos+h.b;pt.handle_right=pt.position*2-pt.handle_left;}
                else {pt.position=points.back().position+rotate(h.c,h.axis,(seg-1)*180);
                    pt.handle_left=pt.position-rotate(h.c-h.b,h.axis,(seg-1)*180);pt.handle_right=pt.position*2-pt.handle_left;}
                t.pos=pt.position;t.dir=unit(pt.handle_right); // upstream's absolute-handle convention
            } else {
                if(seg!=start)t.move(seg_length);
                pt.position=t.pos;Vec3 dir=cloned&&seg==start?cloned->dir:t.dir;
                pt.handle_left=t.pos-dir*(seg_length/3);pt.handle_right=t.pos+dir*(seg_length/3);
            }
            pt.radius=float(radius_at(s,double(seg)/res));add_point(points,pt);
            if(seg==start)continue;
            int n=0;
            if(p.curve_v[d]>=0) {
                bool attempted=false;n=splits(s,seg,prob,false,&attempted);
                // Upstream reduces branch propensity whenever a probabilistic split is attempted,
                // even if error diffusion selected zero splits (factor remains unchanged then).
                if(attempted) {factor=std::max(.8,factor/(n+1));branch_count*=factor;branches_per=branch_count/res;}
            }
            auto saved=rng;
            if(std::abs(branch_count)>0&&int(d)<p.levels-1) {
                int count=branch_count<0?(seg==res?int(branch_count):0):int(branches_per+branch_error);
                if(branch_count>=0)branch_error-=count-branches_per;
                if(count)children(t,s,seg,count,previous,false,points[points.size()-2],points.back());
            } else if(std::abs(leaf_count)>0&&d>0) {
                int count=leaf_count<0?(seg==res?int(leaf_count):0):int(leaves_per+leaf_error);
                if(leaf_count>=0)leaf_error-=count-leaves_per;
                if(count)children(t,s,seg,count,previous,true,points[points.size()-2],points.back());
            }
            rng=saved;
            if(p.curve_v[d]>=0) {
                if(n>0) {
                    auto a=angles(t,s,seg,correction);saved=rng;
                    if(!a.base&&n>2&&a.direct)throw std::invalid_argument("direct splits exceed three branches");
                    for(int i=0;i<n;++i) {
                        auto clone=t;clone.pitch(a.split/2);
                        double spread=a.base&&!a.direct?(i+1)*(360.0/(n+1))+rng.variation()*p.split_angle_v[d]:
                            (i==0?a.spread/2:-a.spread/2);
                        if(a.direct)clone.turn(-spread);else clone.spin({0,0,1},spread);
                        make(clone,s,seg,correction,factor,prob,nullptr,p.split_angle_v[d]>=0?&t:nullptr,s.id,true);
                    }
                    rng=saved;t.pitch(a.split/2);
                    if(!a.base&&n==1) {if(a.direct)t.turn(a.spread/2);else t.spin({0,0,1},-a.spread/2);}
                } else bend(t,s,seg,correction);
                tropism(t,d);
            }
            if(density>2) {
                auto a=points[points.size()-2],b=points.back();points.pop_back();--point_count;
                points.back().radius=float(radius_at(s,double(seg-1)/res));
                for(int k=1;k<density;++k) {
                    double offset=double(k)/(density-1);Point v=b;
                    if(k!=density-1) {v.position=position(a,b,offset);auto dir=unit(tangent(a,b,offset));
                        double mag=norm(b.handle_left-b.position);v.handle_left=v.position-dir*mag;v.handle_right=v.position+dir*mag;}
                    v.radius=float(radius_at(s,(offset+seg-1)/res));add_point(points,v);
                }
            }
        }
        if(density>2)for(auto& pt:points) {
            pt.handle_left=pt.position+(pt.handle_left-pt.position)*(1.0/max_points);
            pt.handle_right=pt.position+(pt.handle_right-pt.position)*(1.0/max_points);
        }
        out.branches[s.id].points=std::move(points);
    }
public:
    Generator(const Parameters& params,std::uint32_t seed,Limits lim):p(params),rng(seed),limits(lim) {out.parameters=p;}
    Skeleton run() {
        std::vector<std::pair<Vec3,double>> floor;
        if(p.branches[0]>0) {
            scale=p.g_scale+p.g_scale_v;Stem dummy;dummy.length=length(dummy);
            double spacing=2.5*radius(dummy);
            for(int i=0;i<int(p.branches[0]);++i) {
                bool found=false;
                for(int attempt=0;attempt<100000;++attempt) {
                    double dis=std::sqrt(rng.draw()*p.branches[0]/2.5*p.g_scale*p.ratio),a=rng.range(0,2*pi);
                    Vec3 pos{float(dis*std::cos(a)),float(dis*std::sin(a)),0};
                    if(std::all_of(floor.begin(),floor.end(),[&](auto v){return norm(v.first-pos)>=spacing;})) {
                        floor.emplace_back(pos,a);found=true;break;
                    }
                }
                if(!found)throw std::length_error("tree floor placement exhausted");
            }
        }
        for(int i=0;i<int(p.branches[0]);++i) {
            scale=p.g_scale+rng.variation()*p.g_scale_v;Turtle t;
            if(p.branches[0]>1) {t.roll((floor[i].second-90)*180/pi);t.pos=floor[i].first;}
            else t.roll(rng.range(0,360));
            make(t,Stem{});
        }
        out.foliage_scale=scale/p.g_scale;
        for(auto& b:out.branches)for(auto& v:b.points) {v.position=world(v.position);v.handle_left=world(v.handle_left);v.handle_right=world(v.handle_right);}
        for(auto& l:out.leaves) {l.position=world(l.position);l.direction=world(l.direction);l.right=world(l.right);
            l.blossom=p.blossom_rate!=0&&rng.draw()<p.blossom_rate;}
        return std::move(out);
    }
};
void validate(const Parameters& p) {
    auto check=[](bool ok){if(!ok)throw std::invalid_argument("invalid tree parameters");};
    auto finite=[&](double v){check(std::isfinite(v)&&std::abs(v)<=1000000);};
#include "tree_validate.inc"
    check(p.levels>=1&&p.levels<=4&&p.shape>=0&&p.shape<=8);
    check(p.g_scale>0&&p.g_scale_v>=0&&p.g_scale_v<p.g_scale&&p.ratio>0&&p.ratio_power>0);
    check(p.flare>=0&&p.base_splits>=-100&&p.base_splits<=100);
    check(p.prune_ratio>=0&&p.prune_ratio<=1&&p.prune_width>0&&p.prune_width_peak>0&&p.prune_width_peak<1);
    check(p.prune_power_low>=0&&p.prune_power_high>=0&&p.leaf_scale>0&&p.leaf_scale_x>0);
    check(p.blossom_scale>=0&&p.blossom_rate>=0&&p.blossom_rate<=1&&p.leaf_bend>=0&&p.leaf_bend<=1);
    check(p.branches[0]>=0&&p.branches[0]<=1000);
    for(unsigned d=0;d<4;++d) {
        check(p.base_size[d]>=0&&p.base_size[d]<1&&p.length[d]>=0&&p.length_v[d]>=0);
        check(p.taper[d]>=0&&p.taper[d]<=3&&p.radius_mod[d]>0&&p.seg_splits[d]>=0&&p.seg_splits[d]<=100);
        check(std::trunc(p.branches[d])==p.branches[d]&&std::abs(p.branches[d])<=10000);
        if(int(d)<p.levels) check(p.curve_res[d]>=1&&p.curve_res[d]<=1000&&std::trunc(p.curve_res[d])==p.curve_res[d]);
        if(int(d)<p.levels&&p.curve_v[d]<0) check(p.curve_v[d]>-90);
    }
}
struct LeafShape { std::vector<Vec3> vertices; std::vector<std::vector<unsigned>> faces; };
#include "tree_shapes.inc"
void append(geometry::Mesh& dst,const geometry::Mesh& src,std::size_t& remaining) {
    if(src.vertices.size()>remaining||dst.vertices.size()+src.vertices.size()>std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("tree mesh vertex budget");
    remaining-=src.vertices.size();auto offset=std::uint32_t(dst.vertices.size());
    dst.vertices.insert(dst.vertices.end(),src.vertices.begin(),src.vertices.end());
    for(auto i:src.indices)dst.indices.push_back(offset+i);
}
geometry::Mesh leaf_template(int shape,double scale,double scale_x) {
    const auto& source=leaf_shapes.at(shape);geometry::Mesh result;
    for(const auto& face:source.faces) {
        // Triangulate in a local face plane; restore original 3D positions after
        // tessellation so the nonplanar blossom petals retain their shape.
        Vec3 origin=source.vertices[face[0]],normal{};
        for(std::size_t i=1;i+1<face.size();++i) normal=normal+cross(source.vertices[face[i]]-origin,source.vertices[face[i+1]]-origin);
        normal=unit(normal);Vec3 u=unit(source.vertices[face[1]]-origin),v=unit(cross(normal,u));
        geometry::Ring ring;
        for(auto i:face) {auto delta=source.vertices[i]-origin;ring.push_back({float(dot(delta,u)),float(dot(delta,v))});}
        std::array<geometry::Ring,1> rings{ring};auto part=geometry::polygon(rings);
        for(std::size_t i=0;i<part.vertices.size();++i) {
            auto pos=source.vertices[face[i]];pos.x*=float(scale_x);pos=pos*scale;
            part.vertices[i].position=pos;part.vertices[i].normal={};
            part.vertices[i].uv={source.vertices[face[i]].x+.5f,source.vertices[face[i]].z};
        }
        for(std::size_t i=0;i<part.indices.size();i+=3) {
            auto a=part.indices[i],b=part.indices[i+1],c=part.indices[i+2];
            Vec3 n=cross(part.vertices[b].position-part.vertices[a].position,part.vertices[c].position-part.vertices[a].position);
            for(auto j:{a,b,c})part.vertices[j].normal=part.vertices[j].normal+n;
        }
        for(auto& vertex:part.vertices)vertex.normal=unit(vertex.normal);
        std::size_t unlimited=std::numeric_limits<std::size_t>::max();append(result,part,unlimited);
    }
    return result;
}
} // namespace
std::span<const Preset> species() {return presets;}
Parameters preset(std::string_view name) {
    for(const auto& s:presets)if(s.name==name)return s.parameters;
    throw std::invalid_argument("unknown tree species: "+std::string(name));
}
Skeleton generate(const Parameters& p,std::uint32_t seed,Limits limits) {
    validate(p);return Generator(p,seed,limits).run();
}
Meshes mesh(const Skeleton& tree,MeshDetail detail) {
    Meshes result;auto profile=geometry::circle_profile(detail.radial_segments);
    std::size_t remaining=detail.max_vertices;
    for(const auto& branch:tree.branches) {
        std::vector<geometry::Bezier> curves;
        for(std::size_t i=1;i<branch.points.size();++i) {
            auto a=branch.points[i-1],b=branch.points[i];
            // Geometry sweeps require positive radii. Retain exact zero tips in
            // the skeleton and use a 10 micrometre cap only in the render mesh.
            curves.push_back({{a.position,a.handle_right,b.handle_left,b.position},std::max(a.radius,1e-5f),std::max(b.radius,1e-5f)});
        }
        if(curves.empty())continue; // upstream permits zero-length terminal clones
        auto path=geometry::sample_curve(curves,detail.curve);
        if(path.size()*(profile.size()+1)+2*profile.size()>remaining)throw std::length_error("tree mesh vertex budget");
        append(result.wood,geometry::sweep(path,profile),remaining);
    }
    if(!detail.foliage||tree.leaves.empty())return result;
    const auto& p=tree.parameters;
    int leaf=p.leaf_shape<1||p.leaf_shape>10?7:p.leaf_shape-1;
    int blossom=p.blossom_shape<1||p.blossom_shape>3?10:9+p.blossom_shape;
    auto base=leaf_template(leaf,p.leaf_scale*tree.foliage_scale,p.leaf_scale_x);
    geometry::Mesh flower;if(p.blossom_scale>0)flower=leaf_template(blossom,p.blossom_scale*tree.foliage_scale,1);
    for(const auto& l:tree.leaves) {
        Vec3 dir=upstream(l.direction),right=upstream(l.right),pos=upstream(l.position);
        Vec3 x=track({1,0,0},dir);
        double spin=180-std::acos(std::clamp(dot(unit(right),x),-1.0,1.0))*180/pi;
        auto normal=cross(dir,right);
        double bend=(std::atan2(pos.y,pos.x)-std::atan2(normal.y,normal.x))*p.leaf_bend*180/pi;
        auto transform=[&](Vec3 v){return world(rotate(track(rotate(v,{0,0,1},spin),dir),{0,0,1},bend));};
        auto part=l.blossom?flower:base;
        for(auto& vertex:part.vertices) {vertex.position=transform(vertex.position)+l.position;vertex.normal=unit(transform(vertex.normal));}
        append(l.blossom?result.blossoms:result.leaves,part,remaining);
    }
    return result;
}
} // namespace yard::tree
