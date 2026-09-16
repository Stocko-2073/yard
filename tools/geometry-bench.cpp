#include "geometry.h"
#include <chrono>
#include <cstdio>
#include <sys/resource.h>
using namespace yard::geometry;
template<class F> void measure(const char* name,int iterations,F generate) {
    std::size_t vertices=0,indices=0,capacity=0;
    auto start=std::chrono::steady_clock::now();
    for (int i=0;i<iterations;++i) {
        auto m=generate();
        vertices=m.vertices.size(); indices=m.indices.size();
        capacity=m.vertices.capacity()*sizeof(Vertex)+m.indices.capacity()*sizeof(std::uint32_t);
    }
    double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/iterations;
    std::printf("%s: %.2f us/mesh, %zu vertices, %zu triangles, %zu payload bytes, %zu capacity bytes\n",
                name,us,vertices,indices/3,vertices*sizeof(Vertex)+indices*sizeof(std::uint32_t),capacity);
}
int main() {
    std::array<Bezier,1> curve={Bezier{{Vec3{0,0,0},Vec3{1,2,0},Vec3{-1,4,1},Vec3{0,6,1}},.3f,.02f}};
    auto low=circle_profile(6),high=circle_profile(16);
    measure("sweep low",2000,[&]{return sweep(sample_curve(curve,{.05f,.5f,16}),low);});
    measure("sweep high",2000,[&]{return sweep(sample_curve(curve,{.002f,.1f,16}),high);});
    std::array<Ring,2> rings={circle_profile(128),circle_profile(32)};
    for (auto& p:rings[1]) { p.x*=.3f; p.y*=.3f; }
    measure("polygon with hole",10000,[&]{return polygon(rings);});
    rusage usage{}; getrusage(RUSAGE_SELF,&usage);
    std::printf("process peak RSS: %ld bytes (macOS; includes runtime and all workloads)\n",usage.ru_maxrss);
}
