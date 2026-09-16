// SPDX-License-Identifier: GPL-3.0-only
#include "tree.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
using namespace yard;
static double distance(geometry::Vec3 a,geometry::Vec3 b) {
    return std::hypot(double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z);
}
template<class Exception,class F> static void rejects(F f) {
    bool caught=false;try{f();}catch(const Exception&){caught=true;}assert(caught);
}
static void check_mesh(const geometry::Mesh& m) {
    assert(m.indices.size()%3==0);
    for(auto i:m.indices)assert(i<m.vertices.size());
    for(auto v:m.vertices) {
        assert(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z));
        assert(std::abs(distance(v.normal,{0,0,0})-1)<1e-4);
        assert(std::isfinite(v.uv.x)&&std::isfinite(v.uv.y));
    }
}
int main() {
    assert(tree::species().size()==20);
    std::ifstream reference("tests/data/tree-reference.txt");assert(reference);
    tree::Skeleton s;tree::Meshes meshes;std::vector<const tree::Branch*> ordered;std::string line;
    while(std::getline(reference,line)) {
        if(line.starts_with('#'))continue;
        std::istringstream fields(line);std::string kind;fields>>kind;
        if(kind=="tree") {
            std::string name;std::size_t branches,leaves;long tolerance;fields>>name>>branches>>leaves>>tolerance;
            s=tree::generate(tree::preset(name),123);
            meshes = name=="weeping_willow" ? tree::Meshes{} : tree::mesh(s);assert(s.branches.size()==branches);
            assert(std::abs(long(s.leaves.size())-long(leaves))<=tolerance);
            ordered.clear();for(auto& b:s.branches)ordered.push_back(&b);
            std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->depth<b->depth;});
        } else if(kind=="point") {
            std::size_t b,i;geometry::Vec3 pos;float radius;fields>>b>>i>>pos.x>>pos.y>>pos.z>>radius;
            auto p=ordered.at(b)->points.at(i);assert(distance(p.position,pos)<.002);assert(std::abs(p.radius-radius)<1e-5);
        } else if(kind=="leaf") {
            std::size_t i;geometry::Vec3 pos;fields>>i>>pos.x>>pos.y>>pos.z;
            assert(distance(s.leaves.at(i).position,pos)<.002);
        } else if(kind=="vertex") {
            std::string material;std::size_t i;geometry::Vec3 pos;fields>>material>>i>>pos.x>>pos.y>>pos.z;
            const auto& mesh=material=="Leaves"?meshes.leaves:meshes.blossoms;
            assert(distance(mesh.vertices.at(i).position,pos)<.002);
        } else assert(false);
        assert(fields);
    }
    // All presets, including both large willows: topology, ownership, finite data.
    for(const auto& preset:tree::species()) {
        auto tree=tree::generate(preset.parameters,123);
        assert(!tree.branches.empty());
        for(std::size_t i=0;i<tree.branches.size();++i) {
            const auto& b=tree.branches[i];assert(b.parent==tree::no_parent||b.parent<i);
            assert(b.depth<unsigned(preset.parameters.levels)&&b.points.size()>=1);
            for(auto p:b.points) {assert(std::isfinite(distance(p.position,{0,0,0})));assert(std::isfinite(p.radius)&&p.radius>=0);}
        }
        for(auto l:tree.leaves) {assert(l.branch<tree.branches.size());assert(std::isfinite(distance(l.position,{0,0,0})));}
    }
    auto p=tree::preset("fan_palm");s=tree::generate(p,123);auto again=tree::generate(p,123);
    assert(s.branches.size()==again.branches.size()&&s.leaves.size()==again.leaves.size());
    for(std::size_t i=0;i<s.leaves.size();++i)assert(distance(s.leaves[i].position,again.leaves[i].position)==0);
    assert(distance(s.branches[0].points.back().position,tree::generate(p,124).branches[0].points.back().position)>.01);
    auto fine=tree::mesh(s),coarse=tree::mesh(s,{{.05f,1,16},4,false});
    check_mesh(fine.wood);check_mesh(fine.leaves);check_mesh(coarse.wood);
    assert(coarse.wood.vertices.size()<fine.wood.vertices.size()&&coarse.leaves.vertices.empty());
    assert(s.leaves.size()==again.leaves.size());
    // All ten leaf and three blossom templates, including nonplanar petals.
    for(int shape=1;shape<=13;++shape) {
        tree::Skeleton one;one.parameters=p;one.foliage_scale=1;
        one.parameters.leaf_shape=shape;one.parameters.blossom_shape=shape-10;one.parameters.blossom_scale=.1;
        one.leaves.push_back({0,{0,0,0},{0,1,0},{1,0,0},shape>10});
        auto m=tree::mesh(one);check_mesh(m.leaves);check_mesh(m.blossoms);
        assert(!m.leaves.vertices.empty()||!m.blossoms.vertices.empty());
    }
    rejects<std::invalid_argument>([]{tree::preset("unknown");});
    rejects<std::length_error>([&]{tree::generate(p,123,{0,100,100});});
    rejects<std::length_error>([&]{tree::generate(p,123,{1000,0,10000});});
    rejects<std::length_error>([&]{tree::generate(p,123,{1000,10000,0});});
    rejects<std::length_error>([&]{tree::mesh(s,{{},8,true,5});});
    p.g_scale=std::numeric_limits<double>::quiet_NaN();rejects<std::invalid_argument>([&]{tree::generate(p);});
    p=tree::preset("palm");p.curve_res[0]=0;rejects<std::invalid_argument>([&]{tree::generate(p);});
    p=tree::preset("palm");p.levels=5;rejects<std::invalid_argument>([&]{tree::generate(p);});
    std::cout<<"tree tests passed (20 presets, Blender fixtures, LOD, foliage, budgets)\n";
}
