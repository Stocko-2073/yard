// SPDX-License-Identifier: GPL-3.0-only
#include "tree.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <algorithm>
using namespace yard;
int main(int argc,char** argv) {
    try {
        bool found=false;
        for(const auto& preset:tree::species()) {
            if(argc>1&&preset.name!=argv[1])continue;
            found=true;
            auto begin=std::chrono::steady_clock::now();auto s=tree::generate(preset.parameters,123);
            auto generated=std::chrono::steady_clock::now();auto m=(argc>3&&std::string(argv[3])=="--skeleton-only")?tree::Meshes{}:tree::mesh(s);
            auto end=std::chrono::steady_clock::now();
            std::size_t vertices=m.wood.vertices.size()+m.leaves.vertices.size()+m.blossoms.vertices.size();
            std::size_t indices=m.wood.indices.size()+m.leaves.indices.size()+m.blossoms.indices.size();
            std::printf("%s branches=%zu leaves=%zu generation_ms=%.3f mesh_ms=%.3f vertices=%zu triangles=%zu mesh_MiB=%.3f\n",
                std::string(preset.name).c_str(),s.branches.size(),s.leaves.size(),
                std::chrono::duration<double,std::milli>(generated-begin).count(),
                std::chrono::duration<double,std::milli>(end-generated).count(),vertices,indices/3,
                double(vertices*sizeof(geometry::Vertex)+indices*sizeof(std::uint32_t))/(1024*1024));
            if(argc>2) {
                std::ofstream f(argv[2]);f<<std::setprecision(9);
                for(const auto& b:s.branches) {
                    f<<"b "<<b.depth<<' '<<b.points.size()<<'\n';
                    for(const auto& p:b.points)f<<p.position.x<<' '<<p.position.y<<' '<<p.position.z<<' '<<p.radius<<'\n';
                }
                for(const auto& l:s.leaves)f<<"l "<<l.position.x<<' '<<l.position.y<<' '<<l.position.z<<'\n';
                for(auto pair:{std::pair{"Leaves",&m.leaves},std::pair{"Blossom",&m.blossoms}})
                    for(std::size_t i=0;i<std::min(std::size_t(64),pair.second->vertices.size());++i) {
                        auto v=pair.second->vertices[i].position;
                        f<<"v "<<pair.first<<' '<<v.x<<' '<<v.y<<' '<<v.z<<'\n';
                    }
            }
        }
        if(!found)throw std::invalid_argument("unknown tree species");
    } catch(const std::exception& e) {std::fprintf(stderr,"tree: %s\n",e.what());return 1;}
}
