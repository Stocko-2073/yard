// Optional comparison tool; libtess2 is supplied externally, not a Yard dependency.
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numbers>
#include <tuple>
#include <vector>
#include <sys/resource.h>
#include <earcut.hpp>
#include <tesselator.h>
int main(int argc,char** argv) {
    if (argc!=2 || (std::strcmp(argv[1],"earcut")&&std::strcmp(argv[1],"libtess2"))) return 2;
    std::vector<std::vector<std::array<float,2>>> rings(2);
    for (int r=0;r<2;++r) {
        int count=r?32:128;
        for (int i=0;i<count;++i) {
            float a=2*std::numbers::pi_v<float>*i/count, radius=r?.3f:1;
            rings[r].push_back({radius*std::cos(a),radius*std::sin(a)});
        }
    }
    bool earcut=std::strcmp(argv[1],"earcut")==0;
    std::size_t triangles=0;
    auto start=std::chrono::steady_clock::now();
    for (int repeat=0;repeat<10000;++repeat) {
        if (earcut) {
            auto indices=mapbox::earcut<std::uint32_t>(rings);
            triangles=indices.size()/3;
        } else {
            auto* tess=tessNewTess(nullptr);
            if (!tess) return 1;
            for (const auto& ring:rings) tessAddContour(tess,2,ring.data(),sizeof(ring[0]),int(ring.size()));
            if (!tessTesselate(tess,TESS_WINDING_ODD,TESS_POLYGONS,3,2,nullptr)) return 1;
            triangles=tessGetElementCount(tess);
            tessDeleteTess(tess);
        }
        if (triangles!=160) return 1;
    }
    auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
    rusage usage{}; getrusage(RUSAGE_SELF,&usage);
    std::printf("%s: %.2f us/polygon, %zu triangles, %ld bytes process peak RSS (macOS)\n",argv[1],us,triangles,usage.ru_maxrss);
}
